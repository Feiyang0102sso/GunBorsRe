/** Exercise real BIG maps through the production loader and CProp ownership. */
#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "gun_bros_re/gameplay/map/CMapEffects.h"
#include "gun_bros_re/gameplay/map/CRenderQueue.h"
#include "engine/resources/CResTOCManager.h"
#include "engine/platform/ZWindow.h"
#include "engine/graphics/ZShaderProgram.h"
#include "engine/graphics/ZQuadBatch.h"
#include "engine/core/ZPaths.h"
#include <cstdio>
#include <utility>

int RunMapResourceChecks(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    ZWindow window;
    if (!window.Open("Map resource ownership check", 640, 480)) { return 1; }
    ZShaderProgram program;
    if (!program.Load(Paths::Shaders().c_str(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    ZQuadBatch batch;
    if (!batch.Create(program)) { return 1; }

    unsigned failures = 0, maps = 0, props = 0, independentPairs = 0;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC &pack = *toc.GetPack(packIndex);
        CGameObjectPack table;
        if (!table.Init(pack)) { continue; }
        const unsigned count = table.GetObjectCount(ZGameSection::TileLayer);
        for (unsigned ordinal = 0; ordinal < count; ++ordinal) {
            CMap loaded;
            if (!loaded.Load(toc, packIndex, ordinal)) { ++failures; continue; }
            ++maps;
            // A raw map load must not run any of the levels that reference it.
            for (unsigned layer = 0; layer < loaded.GetTileLayerCount(); ++layer) {
                if (loaded.GetTileLayer(layer).IsScrolling()) { ++failures; }
            }
            loaded.LoadProps(toc);
            props += static_cast<unsigned>(loaded.GetResources().props.size());
            loaded.BuildCollisionScene();
            loaded.DrawBackground(batch, true, false);

            // CMap moves its owner, preserving every CProp, Flow self pointer,
            // template, frame cache and atlas address borrowed by live instances.
            auto *resources = &loaded.GetResources();
            CProp *first = nullptr;
            if (!resources->props.empty()) { first = &resources->props.front(); }
            CMap moved(std::move(loaded));
            CMap assigned;
            assigned = std::move(moved);
            if (&assigned.GetResources() != resources) { ++failures; }
            if (first != nullptr && first != &assigned.GetResources().props.front()) { ++failures; }
            for (CProp &prop : assigned.GetResources().props) { prop.Update(16, false); }
            assigned.DrawBackground(batch, true, false);

            bool checkedPair = false;
            for (CProp &firstProp : assigned.GetResources().props) {
                if (!firstProp.HasScript()) { continue; }
                for (const CProp &secondProp : assigned.GetResources().props) {
                    if (&firstProp == &secondProp || firstProp.resources != secondProp.resources) { continue; }
                    const auto state = secondProp.GetStateId();
                    const auto step = secondProp.GetPlayer(0).GetStep();
                    const auto health = secondProp.GetHealth();
                    firstProp.HandleMessage(0);
                    firstProp.Update(80, false);
                    if (secondProp.GetStateId() != state || secondProp.GetPlayer(0).GetStep() != step ||
                        secondProp.GetHealth() != health) { ++failures; }
                    ++independentPairs;
                    checkedPair = true;
                    break;
                }
                if (checkedPair) { break; }
            }
            std::printf("[map-resource-check] %s:%u props=%zu atlas-packs=%zu failures=%u\n",
                pack.GetShortName().c_str(), ordinal, resources->props.size(), resources->packs.size(), failures);
        }
    }
    if (maps == 0 || props == 0 || independentPairs == 0) { ++failures; }
    std::printf("[map-resource-check] maps=%u props=%u independent-pairs=%u failures=%u\n",
        maps, props, independentPairs, failures);
    return failures != 0;
}
