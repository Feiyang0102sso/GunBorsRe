#include "gun_bros_re/gameplay/map/ZMapViewer.h"
/** Exercise Haven's placed turret models and authored indicator scripts. */
#include "TestOutput.h"
#include "gun_bros_re/gameplay/map/CMapInternal.h"
#include "gun_bros_re/gameplay/map/ZMapViewer.h"
#include "gun_bros_re/debug/Capture.h"
#include "tests/TestOutput.h"
using namespace MapDetail;

int RunMapTurretChecks(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.InitAuto(bigDirectory) || !toc.Bind()) { return 1; }
    ZWindow window;
    if (!window.Open("Map turret check", 1000, 800)) { return 1; }
    ZShaderProgram program;
    if (!program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    CMap loaded;
    for (const ZCatalogMap &entry : BuildCatalog(toc)) {
        if (entry.packName != "pack9" || entry.mapIndex != 0) { continue; }
        if (!LoadPreviewMap(toc, entry.packIndex, entry.mapIndex, loaded)) { return 1; }
        loaded.LoadProps(toc);
        LoadPlacedEnemies(toc, program, loaded);
        break;
    }
    unsigned wrongParts = 0;
    for (const auto &placed : loaded.GetResources().enemies) {
        const CEnemy &enemy = *placed;
        const unsigned move = placed->data->script.GetStates()[enemy.GetStateId()].GetOwnSequence()[0];
        std::printf("[map-turret] sequence-move=%u selected-part=%d actual=%u\n", move,
            enemy.combat.variables[14], enemy.GetPart(1).controller.GetMoveIndex());
        if (enemy.GetPart(1).controller.GetMoveIndex() != move) { ++wrongParts; }
    }
    ZQuadBatch batch;
    if (!batch.Create(program)) { return 1; }
    ZMapTurretPreview preview;
    preview.Bind(loaded);
    unsigned failures = wrongParts;
    // Expected authored states, including the script-driven opening/closing completion.
    const unsigned indicatorStates[] = {2, 3, 0, 1, 2};
    const unsigned enemyStates[] = {2, 4, 8, 8, 2};
    for (unsigned state = 0; state < 5; ++state) {
        unsigned changedFrames = 0;
        for (unsigned elapsed = 0; elapsed < 1024; elapsed += 16) {
            int previousStep = -1;
            for (const CProp &prop : loaded.GetResources().props) {
                if (preview.Contains(prop)) { previousStep = prop.GetPlayer(0).GetStep(); break; }
            }
            AdvanceEnemies(loaded, 16);
            preview.Update(16);
            for (const CProp &prop : loaded.GetResources().props) {
                if (preview.Contains(prop)) {
                    if (previousStep != prop.GetPlayer(0).GetStep()) { ++changedFrames; }
                    break;
                }
            }
        }
        unsigned indicators = 0;
        for (const CProp &prop : loaded.GetResources().props) {
            if (!preview.Contains(prop)) { continue; }
            ++indicators;
            if (prop.GetStateId() != indicatorStates[state] || prop.GetUnsupportedCount() != 0) { ++failures; }
            const int pausedStep = prop.GetPlayer(0).GetStep();
            preview.Update(0);
            if (pausedStep != prop.GetPlayer(0).GetStep()) { ++failures; }
        }
        if (indicators != 2 || loaded.GetResources().enemies.size() != 2) { ++failures; }
        if (state == 2) {
            if (changedFrames != 0) { ++failures; }
        } else if (changedFrames == 0) { ++failures; }
        for (const auto &placed : loaded.GetResources().enemies) {
            const CEnemy &enemy = *placed;
            if (enemy.GetStateId() != enemyStates[state]) { ++failures; }
            const unsigned move = placed->data->script.GetStates()[enemy.GetStateId()].GetOwnSequence()[0];
            if (enemy.GetPart(1).controller.GetMoveIndex() != move || enemy.GetPart(0).controller.GetMoveIndex() != 0) { ++failures; }
            std::printf("[map-turret-check] %s enemy-state=%u part1-move=%u light-frame-changes=%u failures=%u\n",
                preview.StateName(), enemy.GetStateId(), enemy.GetPart(1).controller.GetMoveIndex(), changedFrames, failures);
        }
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(500, 400, kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -292, -1112);
        glViewport(0, 0, 1000, 800);
        glClearColor(0.08f, 0.08f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        loaded.DrawBackground(batch, true, true, false);
        batch.Draw(program, mvp);
        CRenderQueue::Draw(loaded, batch, program, mvp, true);
        if (!Capture::SaveFrame(window, TestOutput::Path("turret-state-" + std::to_string(state) + ".png"))) { return 1; }
        preview.Cycle();
    }
    // The gameplay CProp entry point must retain the authored off/charge/idle/active cycle.
    for (const CProp &prop : loaded.GetResources().props) {
        if (!preview.Contains(prop)) { continue; }
        CProp runtime;
        runtime.Bind(prop.resources->data, &prop.resources->durations);
        for (unsigned state = 0; state < 4; ++state) {
            if (runtime.GetStateId() != state) { ++failures; }
            runtime.HandleMessage(0);
        }
        if (runtime.GetStateId() != 0 || runtime.GetUnsupportedCount() != 0) { ++failures; }
    }
    CMap empty;
    preview.Bind(empty);
    preview.Cycle();
    preview.Update(16);
    if (!preview.Empty()) { ++failures; }
    // Run the real game prop host: message 0 advances both original indicator scripts.
    ZPackTables tables(toc);
    CBrother player;
    ZPlayerVitals vitals;
    std::vector<CEnemy::Template> catalog;
    CLevel scene(toc, tables, program);
    scene.BindCombat(catalog, player, vitals, 1.0f);
    CLevel level;
    CLevel::Props props(loaded, scene, level);
    props.Reset();
    for (unsigned layer = 0; layer < loaded.GetObjectLayerCount(); ++layer) {
        props.StartLayer(loaded.GetObjectLayer(layer).GetLayerIndex());
    }
    for (unsigned state = 0; state < 4; ++state) {
        unsigned checked = 0;
        for (const CProp &prop : loaded.GetResources().props) {
            if (prop.resources->resource.packHash != CStringToKey("pack9") || prop.resources->resource.localIndex != 47) { continue; }
            ++checked;
            if (!prop.active || !prop.HasScript() || prop.GetStateId() != state) { ++failures; }
            props.SendMessage(prop.objectId, 0);
        }
        if (checked != 2) { ++failures; }
    }
    // Real combat animation follows the same selected part and original activation messages.
    const CEnemy::Template &entry = *loaded.GetResources().enemies[0]->data;
    CEnemy gameModel;
    gameModel.combat.enabled = true;
    if (!gameModel.Bind(tables, entry, false, nullptr)) { return 1; }
    gameModel.Spawn();
    gameModel.HandleMessage(2);
    if (gameModel.GetStateId() != 2 || gameModel.GetPart(1).controller.GetMoveIndex() != 2) { ++failures; }
    gameModel.HandleMessage(1);
    for (unsigned elapsed = 0; elapsed < 1024; elapsed += 16) { gameModel.Update(16); }
    if (gameModel.GetStateId() != 4 || gameModel.GetPart(1).controller.GetMoveIndex() != 1) { ++failures; }
    std::printf("[map-turret-check] gameplay prop messages and enemy activation failures=%u\n", failures);
    std::printf("[map-turret-check] failures=%u\n", failures);
    return failures == 0 ? 0 : 1;
}
