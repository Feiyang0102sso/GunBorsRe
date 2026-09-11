#include "gun_bros_viewer/milestones/ArenaInternal.h"
namespace ArenaDetail {
constexpr float kRadians = 3.14159265f / 180;
const char *const kShaders = Paths::Shaders().c_str();

void Circle(CMarkerBatch &batch, float x, float y, float radius) {
    for (int i = 0; i < 32; ++i) {
        const float a = i * 360 / 32.0f * kRadians;
        const float b = (i + 1) * 360 / 32.0f * kRadians;
        batch.AddSegment(x + std::cos(a) * radius, y + std::sin(a) * radius,
            x + std::cos(b) * radius, y + std::sin(b) * radius, 1.5f);
    }
}

bool Equip(PackTables &tables, const PlayerTemplateData &data, const WeaponEntry &entry,
    PlayerModel &player, const CShaderProgram &program) {
    return EquipPlayerWeapon(tables, data.script, entry.data, entry.owner, player) &&
        CreatePlayerBuffers(player, program);
}

}

using namespace ArenaDetail;

int RunArena(const std::string &bigDirectory, std::uint32_t enemyIndex,
    std::uint32_t weaponIndex, const std::string &screenshot, std::uint32_t advanceMs,
    bool fire, bool check, bool showCollisions, int armorIndex) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<EnemyTemplateData> catalog;
    std::vector<WeaponEntry> weapons;
    PlayerTemplateData playerData;
    PlayerVitals vitals;
    if (!LoadEnemyCatalog(toc, tables, catalog) || catalog.empty() ||
        !LoadWeaponCatalog(toc, tables, weapons) || !FindPlayerTemplate(toc, tables, playerData) ||
        !LoadInitialPlayerHealth(toc, tables, vitals.maximum)) { return 1; }
    CWindow window;
    if (!window.Open("Gun Bros - Arena", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    CShaderProgram program, markerProgram;
    if (!program.Load(kShaders, "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
        !markerProgram.Load(kShaders, "ogles_vs_mvp_constcolor", "ogles_ps_constcolor")) { return 1; }
    CMarkerBatch markers;
    if (!markers.Create(markerProgram)) { return 1; }
    PlayerModel player;
    player.vitals = &vitals;
    std::size_t weapon = weaponIndex % weapons.size();
    std::size_t entry = enemyIndex % catalog.size();
    if (!BuildPlayerBody(tables, playerData.moveSet, player) ||
        !Equip(tables, playerData, weapons[weapon], player, program)) { return 1; }
    WeaponEffects effects(toc, tables, program);
    CombatScene scene(tables, program, catalog, player, vitals, effects,
        playerData.gameScale);
    
#if GB_ENABLE_TESTS
if (check) { return CheckArena(window, toc, tables, program, catalog, weapons, playerData, player, vitals, effects, scene); }
#endif

    if (armorIndex >= 0) {
        std::vector<ArmorEntry> armor;
        if (!LoadArmorCatalog(toc, tables, armor) || armorIndex >= static_cast<int>(armor.size()) ||
            !EquipPlayerArmor(tables, armor[armorIndex].data, program, player)) {
            return 1;
        }
    }
    scene.Reset();
    scene.Spawn(entry, 600, 330);
    for (std::uint32_t time = 0; time < advanceMs; time += kStepMs) {
        scene.Update(kStepMs, 0, 0, fire);
    }
    std::printf("[arena] arrows: template, X: spawn, R: reset, G: invincible, 1-7/N/M: weapons\n"
        "[arena] WASD: move, mouse/left click: aim/fire, space: pause, period: 16 ms step, C: collision\n");
    bool paused = false;
    bool collisions = showCollisions;
    std::uint64_t previous = window.GetTicksMs();
    int accumulator = 0;
    std::uint64_t noticeUntil = 0;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    while (window.PumpEvents()) {
        bool step = false;
        bool reset = false;
        std::size_t nextEntry = entry;
        std::size_t nextWeapon = weapon;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None; key = window.TakeKeyPress()) {
            if (key == KeyCode::Left) { nextEntry = (nextEntry + catalog.size() - 1) % catalog.size(); }
            else if (key == KeyCode::Right) { nextEntry = (nextEntry + 1) % catalog.size(); }
            else if (key == KeyCode::X) {
                if (scene.SpawnNearby(entry) == nullptr) { noticeUntil = window.GetTicksMs() + 2500; }
            }
            else if (key == KeyCode::R) { reset = true; }
            else if (key == KeyCode::G) { vitals.invincible = !vitals.invincible; }
            else if (key == KeyCode::Space) { paused = !paused; accumulator = 0; effects.SetPaused(paused); }
            else if (key == KeyCode::Period) { paused = true; step = true; effects.SetPaused(true); }
            else if (key == KeyCode::C) { collisions = !collisions; }
            else { nextWeapon = SelectWeaponKey(weapons, nextWeapon, key); }
        }
        if (nextEntry != entry || reset) {
            entry = nextEntry;
            scene.Reset();
            scene.Spawn(entry, 600, 330);
            accumulator = 0;
        }
        if (nextWeapon != weapon && !vitals.dead) {
            // Release the old gun's continuous effects; enemy attacks and
            // already launched projectiles keep their independent lifetimes.
            effects.RetireOwner(kPlayerCombatId);
            if (Equip(tables, playerData, weapons[nextWeapon], player, program)) { weapon = nextWeapon; }
        }
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        float mouseX = 0, mouseY = 0;
        if (screenshot.empty() && window.GetMousePosition(mouseX, mouseY) && !vitals.dead) {
            scene.facing = std::atan2(mouseX * kArenaWidth / width - scene.playerX,
                scene.playerY - mouseY * kArenaHeight / height) / kRadians;
        }
        float moveX = 0, moveY = 0;
        if (window.IsKeyDown(KeyCode::A)) { moveX -= 1; }
        if (window.IsKeyDown(KeyCode::D)) { moveX += 1; }
        if (window.IsKeyDown(KeyCode::W)) { moveY -= 1; }
        if (window.IsKeyDown(KeyCode::S)) { moveY += 1; }
        const std::uint64_t now = window.GetTicksMs();
        if (!paused && screenshot.empty()) { accumulator += static_cast<int>(std::min<std::uint64_t>(now - previous, 100)); }
        previous = now;
        if (step) { accumulator = kStepMs; }
        while (accumulator >= kStepMs) {
            scene.Update(kStepMs, moveX, moveY, fire || window.IsLeftMouseDown());
            accumulator -= kStepMs;
        }
        float projection[16];
        Matrix4dOrthoTopLeft(kArenaWidth, kArenaHeight, 4000, projection);
        glViewport(0, 0, width, height);
        glClearColor(0.055f, 0.075f, 0.09f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        markers.Begin();
        for (float x = 0; x <= kArenaWidth; x += 100) { markers.AddSegment(x, 140, x, kArenaHeight, 1); }
        for (float y = 200; y <= kArenaHeight; y += 100) { markers.AddSegment(0, y, kArenaWidth, y, 1); }
        markers.Draw(markerProgram, projection, 0.1f, 0.15f, 0.18f, 1);
        float playerMatrix[16], model[16], mvp[16];
        scene.PlayerMatrix(playerMatrix);
        effects.Draw(projection, nullptr, 1, WeaponDrawPass::BehindPlayer);
        // Actor meshes share a depth buffer; UI and billboards are layered after.
        glEnable(GL_DEPTH_TEST);
        for (auto &actor : scene.enemies) {
            if (actor->model.enemy.combat.removed) { continue; }
            scene.EnemyMatrix(*actor, model);
            Matrix4dMultiply(projection, model, mvp);
            mvp[3] += 2.0f * actor->model.enemy.stun.GetOffset() / width;
            DrawEnemyModel(actor->model, program, mvp);
        }
        Matrix4dMultiply(projection, playerMatrix, mvp);
        DrawPlayer(player, program, mvp);
        effects.Draw(projection, nullptr, 1, WeaponDrawPass::InFrontOfPlayer);
        glDisable(GL_DEPTH_TEST);
        // Every bar uses the same predicate as projectile damage filtering.
        for (auto &actor : scene.enemies) {
            CEnemy &enemy = actor->model.enemy;
            const EnemyCombat &state = enemy.combat;
            if (state.removed || state.dead) { continue; }
            float barY = state.y - std::max(60.0f, actor->data->gameScale * 0.55f);
            markers.Begin();
            markers.AddRect(state.x - 34, barY, 68, 8);
            markers.Draw(markerProgram, projection, 0.01f, 0.015f, 0.02f, 1);
            markers.Begin();
            float fraction = 0;
            if (state.maxHealth > 0) { fraction = std::clamp(state.health / state.maxHealth, 0.0f, 1.0f); }
            markers.AddRect(state.x - 32, barY + 2, 64 * fraction, 4);
            if (!state.enabled) { markers.Draw(markerProgram, projection, 0.5f, 0.5f, 0.5f, 1); }
            else if (enemy.CanReceiveProjectile(0, kPlayerCombatId)) { markers.Draw(markerProgram, projection, 0.95f, 0.23f, 0.2f, 1); }
            else { markers.Draw(markerProgram, projection, 0.22f, 0.95f, 0.5f, 1); }
            markers.Begin();
            char health[64];
            std::snprintf(health, sizeof(health), "%.0f/%.0f", state.health, state.maxHealth);
            DrawHudText(markers, state.x - 32, barY - 12, health, 1.2f);
            markers.Draw(markerProgram, projection, 0.82f, 0.88f, 0.9f, 1);
        }
        if (collisions) {
            markers.Begin();
            Circle(markers, scene.playerX, scene.playerY, scene.GetPlayerRadius());
            for (const auto &actor : scene.enemies) {
                const CEnemy &enemy = actor->model.enemy;
                const EnemyCombat &state = enemy.combat;
                if (!state.enabled || state.dead || state.removed) { continue; }
                if (state.collision.GetEdges().empty()) {
                    for (std::uint32_t part = 0; part < enemy.GetPartCount(); ++part) {
                        float x = 0, y = 0, radius = 0;
                        scene.EnemyCircle(*actor, part, x, y, radius);
                        if (radius > 0 && enemy.GetPart(part).visible) { Circle(markers, x, y, radius); }
                    }
                }
                const auto &vertices = state.collision.GetVertices();
                for (const auto &edge : state.collision.GetEdges()) {
                    if (!edge.enabled || edge.firstVertex >= vertices.size() || edge.secondVertex >= vertices.size()) { continue; }
                    const auto &a = vertices[edge.firstVertex], &b = vertices[edge.secondVertex];
                    const float c = std::cos(state.facing * kRadians), s = std::sin(state.facing * kRadians);
                    markers.AddSegment(state.x + (a.x * c - a.y * s) * state.scaleFactor,
                        state.y + (a.x * s + a.y * c) * state.scaleFactor,
                        state.x + (b.x * c - b.y * s) * state.scaleFactor,
                        state.y + (b.x * s + b.y * c) * state.scaleFactor, 2);
                }
            }
            markers.Draw(markerProgram, projection, 0.3f, 0.8f, 1, 0.8f);
        }
        markers.Begin();
        markers.AddRect(0, 0, kArenaWidth, 132);
        markers.Draw(markerProgram, projection, 0.025f, 0.04f, 0.05f, 0.97f);
        unsigned kills = scene.kills, hits = scene.hits;
        float damage = scene.damageDealt;
        unsigned deferred = 0;
        for (const auto &actor : scene.enemies) {
            const EnemyCombat &state = actor->model.enemy.combat;
            kills += state.deathCount; hits += state.hitCount;
            damage += state.totalDamage;
            deferred |= state.deferredMechanisms;
        }
        char line[256];
        std::snprintf(line, sizeof(line), "ARENA %zu/%zu - %s", entry, catalog.size() - 1, catalog[entry].owner.c_str());
        markers.Begin();
        DrawHudText(markers, 20, 16, line);
        const char *godLabel = "GOD OFF";
        if (vitals.invincible) { godLabel = "GOD ON"; }
        std::snprintf(line, sizeof(line), "HP %.1f/%.0f   INCOMING %.1f   LAST %.1f   G: %s", vitals.health, vitals.maximum,
            vitals.incomingDamage, vitals.lastDamage, godLabel);
        DrawHudText(markers, 20, 42, line, 1.6f);
        std::snprintf(line, sizeof(line), "ALIVE %zu   HITS %u   KILLS %u   DAMAGE %.1f   LAST %.1f", scene.AliveCount(), hits, kills, damage, scene.lastDamage);
        DrawHudText(markers, 20, 62, line, 1.6f);
        DrawHudText(markers, 20, 83, WeaponSelectionLabel(weapons, weapon).substr(0, 95), 1.4f);
        DrawHudText(markers, 20, 106, "ARROWS ENEMY  X SPAWN  R RESET  1-7/N/M WEAPON  WASD MOVE  SPACE PAUSE  . STEP  C COLLISION", 1.3f);
        if (paused) { DrawHudText(markers, 985, 18, "PAUSED", 2); }
        if (!scene.enemies.empty()) {
            const EnemyCombat &state = scene.enemies.front()->model.enemy.combat;
            std::snprintf(line, sizeof(line), "FILTER %d  TARGET TYPE %d", state.variables[16], state.targetType);
            DrawHudText(markers, 20, 145, line, 1.4f);
        }
        if ((deferred & 3) != 0) { DrawHudText(markers, 20, 167, "BOSS / LEVEL MECHANISMS DEFERRED", 1.4f); }
        if (now < noticeUntil) { DrawHudText(markers, 380, 220, "NO FREE SPAWN POSITION", 2); }
        if (!catalog[entry].script.IsPresent()) { DrawHudText(markers, 380, 180, "UNUSED - NO SCRIPT", 3); }
        else if (catalog[entry].gameScale == 0) { DrawHudText(markers, 380, 180, "NO VISIBLE MODEL", 3); }
        if (vitals.dead) { DrawHudText(markers, 400, 450, "PLAYER DEAD - R RESET", 3); }
        markers.Draw(markerProgram, projection, 0.84f, 0.91f, 0.94f, 1);
        markers.Begin();
        markers.AddRect(20, 124, 260 * std::clamp(vitals.health / vitals.maximum, 0.0f, 1.0f), 4);
        markers.Draw(markerProgram, projection, 0.2f, 0.7f, 1, 1);
        window.SetTitle("Arena | " + catalog[entry].owner + " | " + weapons[weapon].name);
        if (!screenshot.empty()) {
            if (GB_SAVE_FRAME(window, screenshot)) { return 0; }
            return 1;
        }
        window.Present();
    }
    return 0;
}
