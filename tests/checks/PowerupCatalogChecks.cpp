/** @file ZPowerupCatalog.cpp
 * @brief Read authoritative records and enumerate original use actions.
 */
#include "TestOutput.h"
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "engine/core/CStringToKey.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/gameplay/enemy/CTargetingController.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include "Checks.h"

int RunPowerupCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    std::vector<ZPowerupEntry> catalog;
    if (!LoadPowerupCatalog(toc, tables, catalog)) { return 1; }
    std::filesystem::create_directories(TestOutput::Path(""));
    std::ofstream report(TestOutput::Path("powerup-check.txt"));
    unsigned failures = CheckTargetingController(), references = 0;
    for (const ZPowerupEntry &entry : catalog) {
        report << entry.owner << ' ' << std::quoted(entry.name) << " fields=" << unsigned(entry.data.field28) << ','
            << unsigned(entry.data.field29) << ',' << unsigned(entry.data.field30) << ','
            << unsigned(entry.data.field112) << ',' << unsigned(entry.data.field124) << " query=";
        for (unsigned function = 0; function < 5; ++function) {
            CPowerup powerup;
            powerup.Bind(entry.data);
            report << powerup.Query(static_cast<std::uint8_t>(function), 0);
            failures += powerup.GetUnsupportedCount();
        }
        for (const ZScriptResourceRef &resource : entry.data.script.GetResources()) {
            std::vector<std::uint8_t> payload;
            ++references;
            bool present = false;
            if (resource.sectionOrType == 253) {
                // CMovie::Init (:109521): movies use a named base outside the
                // 33 game sections, then add the local movie ordinal.
                CResPackTOC *pack = toc.GetPack(toc.GetPackIndexFromHash(resource.packHash));
                const unsigned base = pack->GetResValue("GLU_MOVIE_MOVIE");
                if (base != 0) { present = pack->GetResource(base + resource.resourceId, payload); }
            } else {
                present = tables.ReadSectionResource(resource.packHash, static_cast<ZGameSection>(resource.sectionOrType + 1), resource.resourceId, payload);
            }
            if (!present) {
                ++failures;
                report << " missing=" << tables.GetPackName(resource.packHash) << ':' << unsigned(resource.sectionOrType) << ':' << resource.resourceId;
            }
        }
        CPowerup powerup;
        powerup.Bind(entry.data);
        powerup.Equip();
        powerup.Use();
        // Research only: explicitly deliver host completion callbacks while
        // retaining actual script timers; this does not claim movie playback.
        for (int tick = 0; tick < 600; ++tick) {
            const auto actions = powerup.TakeActions();
            for (const ZPowerupAction &action : actions) {
                report << " [native=" << unsigned(action.function) << " args=";
                for (unsigned index = 0; index < action.count; ++index) { report << action.arguments[index] << ','; }
                if (!action.resource.IsNull()) { report << " ref=" << tables.GetPackName(action.resource.packHash) << ':' << unsigned(action.resource.localIndex); }
                report << ']';
                if (action.function == 24) {
                    std::vector<std::uint8_t> payload;
                    if (!tables.ReadSectionResource(action.resource.packHash, ZGameSection::Bullet, action.resource.localIndex, payload)) { ++failures; }
                    else {
                        CArrayInputStream stream(payload);
                        CBullet::Template bullet;
                        if (!bullet.Init(stream) || stream.Available() != 0) { ++failures; }
                        report << " [bullet flags=" << bullet.GetFlags() << " damage=" << bullet.GetBaseDamage()
                            << " acceleration=" << bullet.GetAcceleration() << " trajectory=" << bullet.GetTrajectoryHeight()
                            << ',' << bullet.GetTrajectoryDurationMs() << ',' << bullet.GetTrajectoryType() << ']';
                        CBullet projectile;
                        projectile.Bind(bullet, false);
                        unsigned splashCount = 0;
                        int firstSplashMs = 0;
                        float maximumHeight = 0;
                        for (int time = 0; time < 5000 && !projectile.removed; time += 16) {
                            projectile.Update(16, 1000);
                            maximumHeight = std::max(maximumHeight, projectile.GetTrajectoryHeight());
                            for (const ZGunCue &cue : projectile.TakeCues()) {
                                if (cue.kind == ZGunCue::Kind::Splash) {
                                    ++splashCount;
                                    if (firstSplashMs == 0) { firstSplashMs = projectile.ageMs; }
                                    report << " [splash time=" << projectile.ageMs << " damage=" << cue.damage << " radius=" << cue.radius << ']';
                                } else if (cue.kind == ZGunCue::Kind::SpawnEnemy) {
                                    report << " [spawn time=" << projectile.ageMs << " enemy=" << tables.GetPackName(cue.resource.packHash)
                                        << ':' << unsigned(cue.resource.localIndex) << " object=" << cue.spawnObjectId << " force=" << cue.forceSpawn << ']';
                                }
                            }
                        }
                        report << " [trajectory-events=" << projectile.GetTrajectoryEvents() << " peak=" << maximumHeight
                            << " splashes=" << splashCount << " removed=" << projectile.removed << ']';
                        if (action.resource.localIndex == 90 || action.resource.localIndex == 94) {
                            if (splashCount != 1 || firstSplashMs != 608 || maximumHeight < 0.99f || !projectile.removed) { ++failures; }
                        }
                        if (action.resource.localIndex == 93) {
                            // pack5 BULLET 93, 0097_0x61ab.bin: @0x97 emits three
                            // timed pulses, then function 2 @0xB4 splashes once
                            // more before native 5 removes the projectile.
                            if (splashCount != 4 || firstSplashMs != 320 || projectile.GetTrajectoryEvents() != 1 || !projectile.removed) { ++failures; }
                            CBullet wallProbe;
                            wallProbe.Bind(bullet, false);
                            wallProbe.OnWallCollision();
                            if (wallProbe.removed) { ++failures; }
                        }
                    }
                }
                if (action.function == 4) { powerup.HandleEvent(1); }
                if (action.function == 5 || action.function == 2) { powerup.HandleEvent(2); }
                if (action.function == 13) { powerup.HandleEvent(4); }
                if (action.function == 1 || action.function == 15) { powerup.HandleEvent(0); }
            }
            powerup.Update(100);
            if (powerup.IsDone() && actions.empty()) { break; }
        }
        failures += powerup.GetUnsupportedCount();
        report << " state=" << powerup.GetStateId() << " done=" << powerup.IsDone() << " unsupported=" << powerup.GetUnsupportedCount() << '\n';
    }
    // Original native 23 has the same payload as native 0 and an additional
    // percentage flag. Both the two- and four-argument forms must preserve it.
    for (std::uint8_t function : {std::uint8_t(0), std::uint8_t(23)}) {
        for (std::uint8_t count : {std::uint8_t(2), std::uint8_t(4)}) {
            CBullet nativeProbe;
            const std::int16_t args[4] = {10, 150, 256, 100};
            nativeProbe.FunctionResolver(function, args, count);
            const auto cues = nativeProbe.TakeCues();
            if (cues.size() != 1 || cues[0].kind != ZGunCue::Kind::Splash ||
                cues[0].damage != 10 || cues[0].radius != 150 || cues[0].percentDamage != (function == 23)) { ++failures; }
        }
    }
    std::printf("[powerup-check] templates=%zu references=%u failures=%u\n", catalog.size(), references, failures);
    return failures != 0;
}
