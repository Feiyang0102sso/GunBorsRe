/** Real BIG + GL verification of template ownership and image-pool lifetime. */
#include "Checks.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "engine/platform/ZWindow.h"
#include <SDL3/SDL_video.h>
#include <cstdio>
int RunResourceLoadingCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CGunBros game(toc);
    std::vector<CGun::Entry> guns;
    if (!CGun::LoadEntries(toc, game, guns) || guns.empty()) { return 1; }
    CResourceLoader &loader = game.GetResourceLoader();
    unsigned reads = 0;
    loader.SetReadObserver([&reads]() { ++reads; });
    const CGun::Template &source = guns.front().data;
    CGun::Template copy = source;
    if (!source.LoadMesh(loader)) { return 1; }
    const unsigned afterModel = reads;
    if (!copy.LoadMesh(loader) || reads != afterModel || source.GetMesh() != copy.GetMesh()) { return 1; }
    source.GetMoveSet().Load(loader);
    if (!loader.LoadImmediate()) { return 1; }
    const unsigned afterMoves = reads;
    copy.GetMoveSet().Load(loader);
    if (!loader.LoadImmediate() || reads != afterMoves) { return 1; }
    if (copy.GetMoveSet().LoadMesh(loader, UINT32_MAX)) { return 1; }
    // Template Init replaces its model bank without invalidating previous copies.
    const auto retained = copy.GetMesh();
    const auto ref = source.GetMeshRef();
    const auto atlas = source.GetImageRef();
    if (retained->GetVertexCount() == 0 || ref.IsNull() || atlas.IsNull()) { return 1; }
    std::vector<std::uint8_t> templateBytes;
    if (!game.ReadSectionResource(guns.front().packHash, ZGameSection::Gun, guns.front().ordinal, templateBytes)) { return 1; }
    CArrayInputStream input(templateBytes);
    if (!copy.Init(input) || copy.GetMesh() == retained || retained->GetVertexCount() == 0) { return 1; }

    ZWindow window;
    if (!window.Open("Resource loading checks", 320, 240)) { return 1; }
    // All shared GPU owners are declared after the window and released first.
    std::shared_ptr<ZTexture> first, second, cancelled;
    unsigned order = 0;
    loader.AddFunction([&order]() { order = 1; return true; });
    loader.AddImage(atlas.packHash, atlas.assetId, first);
    loader.AddImage(atlas.packHash, atlas.assetId, second);
    loader.AddFunction([&order, &first, &second]() {
        if (order != 1 || !first || first != second) { return false; }
        order = 2;
        return true;
    });
    const unsigned beforeImages = reads;
    if (!loader.LoadImmediate() || order != 2 || reads != beforeImages + 1) { return 1; }
    const GLuint handle = first->GetHandle();
    std::weak_ptr<ZTexture> lifetime = first;
    loader.RemoveImage(first);
    if (lifetime.expired() || glIsTexture(handle) != GL_TRUE) { return 1; }
    loader.RemoveImage(second);
    if (!lifetime.expired() || glIsTexture(handle) != GL_FALSE) { return 1; }

    const unsigned beforeCancel = reads;
    loader.AddImage(atlas.packHash, atlas.assetId, cancelled);
    loader.RemoveImage(cancelled);
    if (!loader.LoadImmediate() || cancelled || reads != beforeCancel) { return 1; }
    loader.AddImage(atlas.packHash, UINT32_MAX, cancelled);
    loader.AddFunction([&order]() { order = 99; return true; });
    if (loader.LoadImmediate() || loader.IsLoading() || cancelled || order == 99) { return 1; }
    loader.AddImage(atlas.packHash, atlas.assetId, first);
    if (!loader.LoadImmediate() || !first) { return 1; }

    // Keep one consumer alive while a distinct GL context requests the same BIG.
    SDL_Window *surface = SDL_GL_GetCurrentWindow();
    SDL_GLContext originalContext = SDL_GL_GetCurrentContext();
    SDL_GLContext otherContext = SDL_GL_CreateContext(surface);
    if (otherContext == nullptr) { return 1; }
    bool isolated = SDL_GL_MakeCurrent(surface, otherContext);
    if (isolated) {
        loader.AddImage(atlas.packHash, atlas.assetId, second);
        isolated = loader.LoadImmediate() && second && second != first;
        loader.RemoveImage(second);
    }
    SDL_GL_MakeCurrent(surface, originalContext);
    SDL_GL_DestroyContext(otherContext);
    if (!isolated || glIsTexture(first->GetHandle()) != GL_TRUE) { return 1; }
    lifetime = first;
    loader.RemoveImage(first);
    if (!lifetime.expired() || retained->GetVertexCount() == 0) { return 1; }
    std::printf("[resource-loading-check] model-sharing=1 image-sharing=1 last-owner-release=1 cancel=1 failure-clear=1 context-isolation=1 failures=0\n");
    return 0;
}
