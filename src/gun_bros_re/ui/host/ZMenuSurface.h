#pragma once
#include "engine/platform/ZWindow.h"
#include "gun_bros_re/gameplay/enemy/CEnemyCasualty.h"
#include "gun_bros_re/ui/menus/CMenuGameResourcesEffects.h"
#include "gun_bros_re/ui/menus/CMenuPostGameOptionEffects.h"
#include "gun_bros_re/ui/menus/CMenuMovieMultiplayerOverlayEffects.h"
#include "gun_bros_re/ui/menus/CMenuMissionPresentation.h"
#include "engine/core/ZPaths.h"
#include "gun_bros_re/ui/controls/CMenuMeshPlayer.h"
#include "gun_bros_re/ui/system/CMenuNavigationBar.h"
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/ui/host/ZMenuFrame.h"
#include "gun_bros_re/ui/controls/ZMenuScrollMotion.h"
#include "gun_bros_re/ui/controls/CMenuMesh.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/gameplay/enemy/CMenuMeshEnemy.h"
#include "gun_bros_re/effects/CParticleEffectPlayer.h"
#include "gun_bros_re/effects/ZParticleResources.h"
#include "gun_bros_re/effects/CParticlePool.h"
#include "engine/glu/sprite/ZSpriteRenderer.h"
#include "engine/graphics/ZQuadBatch.h"
#include "engine/core/CMatrix4d.h"
#include "gun_bros_re/gameplay/audio/CBGM.h"

namespace MenuDetail {
/** All GL owners are destroyed before the menu window's context. */
class ZMenuSurface {
public:
    explicit ZMenuSurface(ZWindow *sharedWindow = nullptr) : window(sharedWindow ? *sharedWindow : ownedWindow) {}
    /** Integration harness input; it still goes through rendered button hit tests. */
    void InjectTap(const ZMenuInputFrame &click) { mouseX = click.x; mouseY = click.y; clicked = true; injectedClick = true; }
    bool HasInjectedClick() const { return injectedClick; }
    /** Temporarily route this frame's click exclusively to a modal panel. */
    bool ExchangeClick(bool enabled) { const bool previous = clicked; clicked = enabled; return previous; }
    std::pair<float, float> Cursor() const { return {mouseX, mouseY}; }
    bool Open(CResTOCManager &toc, CGunBros &tables, const CProfileManager *profile = nullptr, bool startup = false, CBGM *music = nullptr);

    void Begin();

    void Text(float x, float y, const std::string &text, float size = 2,
        float r = 0.88f, float g = 0.93f, float b = 0.95f) {
        unsigned font = 1;
        float scale = size * 7 / 18.0f;
        if (r > 0.9f && g < 0.8f) { font = 5; scale = size * 7 / 27.0f; }
        movies.Text(text, x, y, font, scale);
    }

    bool Hit(float x, float y, float width, float height) {
        if (inputEnabled && clicked && mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height) {
            clicked = false;
            return true;
        }
        return false;
    }

    void CenterText(const std::string &text, float center, float y, unsigned font = 0, float scale = 1) {
        movies.Text(text, center - movies.TextWidth(text, font, scale) * 0.5f, y, font, scale);
    }

    void Clip(float x, float y, float width, float height) {
        int screenWidth = 0, screenHeight = 0;
        window.GetDrawableSize(screenWidth, screenHeight);
        glEnable(GL_SCISSOR_TEST);
        glScissor(static_cast<int>(x * screenWidth / 1024), static_cast<int>((768 - y - height) * screenHeight / 768),
            static_cast<int>(width * screenWidth / 1024), static_cast<int>(height * screenHeight / 768));
    }

    void EndClip() { glDisable(GL_SCISSOR_TEST); }
    void UpdateMeshRotation(CMenuMesh &mesh, unsigned deltaMs, const ZMovieRegion &region, bool enabled) const {
        mesh.UpdateRotation(deltaMs, window.IsLeftMouseDown(), mouseX, mouseY,
            region.x, region.y, region.width, region.height, enabled);
    }

    bool MouseIn(float x, float y, float width, float height) const {
        return inputEnabled && mouseX >= x && mouseX < x + width && mouseY >= y && mouseY < y + height;
    }

    bool TitleImage();

    CMenuNavigationBar navigation;

    // Historical explicit .dat research UI; native profiles use Header below.

    bool Icon(CResTOCManager &toc, CGunBros &tables, const CStoreItem::Entry &entry, float x, float y, float width,
        float height, float alpha = 1, bool originalSize = false, bool fitHeight = false,
        bool alignRight = false, float *renderedWidth = nullptr);

    /** CEnemy::SpawnForUI assembles the original result-card model. */
    bool DrawCasualty(CGunBros &tables, CResTOCManager &toc, const CEnemyCasualty &casualty, float x,
        const ZMovieRegion *originalRegion = nullptr);

    ZWindow ownedWindow;
    ZWindow &window;
    // Menu time; the integration harness advances it instead of real ticks.
    std::uint64_t clock = 0;
    // Under the harness the real pointer must not scroll anything, or a stray
    // drag over the window moves a list out from under a scripted click.
    bool scripted = false;
    // The plate movies own the press burst; remember the last press so the
    // following frames can play it where the button was.
    unsigned pressMovie = 0;
    float pressX = 0, pressY = 0, pressWidth = 0, pressHeight = 0;
    std::uint64_t pressStart = 0;

    void NotePress(unsigned movie, float x, float y, float width, float height) {
        pressMovie = movie;
        pressX = x;
        pressY = y;
        pressWidth = width;
        pressHeight = height;
        pressStart = clock;
    }

    /** Chapter 1 of a button movie is its release burst; it runs 300 ms. */
    void DrawPress() {
        if (pressMovie == 0 || clock < pressStart) { return; }
        const std::uint64_t elapsed = clock - pressStart;
        const auto *movie = movies.GetMovie(pressMovie);
        unsigned start = 0, end = 0;
        if (!movie || !movie->GetChapterRange(1, start, end) || elapsed > end - start) { return; }
        movies.DrawFitted(pressMovie, start + static_cast<unsigned>(elapsed), pressX, pressY, pressWidth, pressHeight, 1);
    }
    ZMovieRenderer movies;
    unsigned storeRestTime = 0;
    float dragX = 0, dragY = 0;
    bool pointerHeld = false, pointerPressed = false, pointerReleased = false;
    void Scroll(ZMenuScrollMotion &motion, float &position, const ZMovieRegion &viewport,
        bool enabled, float maximum, float stride, unsigned duration);
    bool animateNavigation = true;
    bool inputEnabled = true;
private:
    ZShaderProgram textProgram;
    ZShaderProgram imageProgram;
    ZMarkerBatch markers;
    ZQuadBatch images;
    ZTexture titleImage;
    float projection[16]{};
    float mouseX = 0, mouseY = 0;
    bool clicked = false, previousDown = false, injectedClick = false;
    float dragDistance = 0;
    std::map<std::uint64_t, std::unique_ptr<ZTexture>> icons;
    std::map<std::uint64_t, std::unique_ptr<CMenuMeshEnemy>> enemyPreviews;
public:
    ZShaderProgram &ImageProgram() { return imageProgram; }
    CMenuMeshPlayer playerPreview;
    CMenuMission::Presentation planets{movies};
    // Runtime resources are tied to the window; each native menu owns its players.
    ZMenuParticleContext particles;
    CMenuMovieMultiplayerOverlay::Effects modeEffects{particles, movies};
    CMenuPostGameOption::Effects postGameEffects{particles, movies};
    CMenuGameResources::Effects refineryEffects{particles, movies};
};
}
