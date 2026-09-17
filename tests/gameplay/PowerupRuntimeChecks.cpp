/** Synthetic Flow exercises execution independently of retail powerup IDs.
 * Byte layout follows entries/common.bt and flow_bytecode.bt; never game data.
 */
#include "gameplay/PowerupRuntimeChecks.h"
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <algorithm>
#include <cstdio>

namespace {
using Bytes = std::vector<std::uint8_t>;

void Block(Bytes &bytes, const Bytes &code) {
    bytes.push_back(static_cast<std::uint8_t>(code.size()));
    bytes.insert(bytes.end(), code.begin(), code.end());
}

bool MakeTimedPowerup(ZPowerupEntry &entry, bool unsupported = false) {
    entry.owner = "synthetic-timed-powerup";
    entry.resource.packHash = 0x53594E54;
    entry.resource.localIndex = 42;
    Bytes bytes = {1, 0, 0, 0, 0, 0, 0, 8, 0, 1, 2, 3, 4, 5, 6, 7,
        0, 0, 0, 0, 1}; // No secondary table, resources, blocks or variables; one state.
    bytes.insert(bytes.end(), {255, 0, 0}); // No parent, animation or local exports.
    Bytes state = {2, kScriptOpFunction, 8, 15, 1, 0, 129,
        kScriptOpEvent, 3, 15}; // Timer = Q8 256 (one second); OnTimer event 0x0F03.
    Block(state, {2, kScriptOpFunction, 10, 15, 1, 2, 128,
        kScriptOpFunction, 0, 15, 0}); // AddHealth(2), Exit().
    Block(bytes, state);
    Block(bytes, {0}); // State exit.
    bytes.push_back(8);
    for (unsigned query = 0; query < 5; ++query) { Block(bytes, {1, kScriptOpReturn, 1, 128}); }
    Block(bytes, {0}); // Equip.
    if (unsupported) { Block(bytes, {1, kScriptOpFunction, 255, 15, 0}); }
    else { Block(bytes, {1, kScriptOpResult, 0}); } // Ordinary Use enters the timed state.
    Block(bytes, {2, kScriptOpFunction, 10, 15, 1, 4, 128,
        kScriptOpFunction, 0, 15, 0}); // Selector Use heals immediately, a different export.
    CArrayInputStream stream(bytes);
    entry.data.script.Load(stream);
    return !stream.Overran() && stream.Available() == 0;
}
}

unsigned CheckPowerupRuntime(CResTOCManager &toc, ZPackTables &tables, CLevel &scene,
    ZPlayerModel &player, ZPlayerVitals &vitals, ZWeaponEffects &effects) {
    ZPowerupEntry timed;
    ZPowerupEntry unsupported;
    if (!MakeTimedPowerup(timed) || !MakeTimedPowerup(unsupported, true)) { return 1; }
    CPowerup powerup(toc, tables, scene);
    powerup.BindActor(player, vitals, effects);
    const float savedHealth = vitals.health;
    const bool savedDead = vitals.dead;
    const bool savedPaused = scene.IsPaused();
    unsigned failures = 0;
    vitals.dead = false;
    vitals.health = 1;
    if (!powerup.Start(timed) || !powerup.IsActive() || powerup.IsPresentationActive()) { ++failures; }
    powerup.Update(0);
    powerup.Update(999);
    if (vitals.health != 1 || !powerup.IsActive()) { ++failures; }
    powerup.Update(1);
    if (vitals.health != std::min(vitals.maximum, 3.0f) || !powerup.IsDone() || powerup.IsActive()) { ++failures; }
    powerup.Update(1000);
    if (vitals.health != std::min(vitals.maximum, 3.0f)) { ++failures; }
    // Native queries must observe mutations within the same actor lifetime.
    powerup.FunctionResolver(11, nullptr, 0);
    if (powerup.FunctionResolver(12, nullptr, 0) != 1) { ++failures; }

    vitals.health = 1;
    if (!powerup.Start(timed, true) || vitals.health != std::min(vitals.maximum, 5.0f) ||
        powerup.IsActive() || powerup.IsPresentationActive()) { ++failures; }

    vitals.health = 1;
    scene.FunctionResolver(64, nullptr, 0);
    if (!powerup.Start(timed)) { ++failures; }
    powerup.Update(500);
    powerup.Reset();
    powerup.Update(1000);
    if (vitals.health != 1 || !powerup.IsDone() || powerup.IsActive() || !scene.IsPaused()) { ++failures; }
    if (powerup.Start(unsupported) || powerup.IsActive() || powerup.GetUnsupportedCount() != 1) { ++failures; }
    vitals.health = savedHealth;
    vitals.dead = savedDead;
    if (!savedPaused) { scene.FunctionResolver(65, nullptr, 0); }
    std::printf("[powerup-runtime] timer, live queries, selector export, cancel and native rejection failures=%u\n", failures);
    return failures;
}
