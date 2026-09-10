/** @file SurvivalHud.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "runtime/SurvivalHud.h"
#include "runtime/OriginalMenuData.h"
#include "runtime/OriginalTextLayout.h"
#include "runtime/HostSettings.h"
#include "runtime/PowerupCatalog.h"
#include "engine/platform/CWindow.h"
#include "gun_bros/CResTOCManager.h"
#include "gun_bros/CLevel.h"
#include "engine/CPNG.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

bool SurvivalHud::Init(CResTOCManager &toc, PackTables &tables) {
    m_tables = &tables;
    CResPackTOC &core = *toc.GetPack(toc.GetCorePackIndex());
    if (!m_movies.Init(core, core) || !LoadStoreCatalog(toc, tables, m_store)) { return false; }
    if (!LoadPowerupCatalog(toc, tables, m_powerups)) { return false; }
    for (const PowerupEntry &entry : m_powerups) {
        const unsigned hash = entry.data.sprite.packHash;
        if (m_powerupRenderers.count(hash) != 0) { continue; }
        auto renderer = std::make_unique<MovieRenderer>();
        CResPackTOC *pack = toc.GetPack(toc.GetPackIndexFromHash(hash));
        if (pack == nullptr || !renderer->Init(*pack, core)) { return false; }
        m_powerupRenderers[hash] = std::move(renderer);
    }
    // First-use font and atlas uploads belong to loading, not the first shot
    // CStoreAggregator::InitFilteredList :159210 and SortFilteredList :155433.
    std::vector<std::pair<int, unsigned>> selectorOrder;
    for (unsigned index = 0; index < m_store.size(); ++index) {
        const auto &item = m_store[index].data;
        if (item.type < 10 || item.type > 13 || item.displayOrder < 0 || item.value242 == 1 ||
            item.objects.empty() || item.objects.front().type != 17) { continue; }
        selectorOrder.push_back({item.displayOrder, index});
    }
    std::sort(selectorOrder.begin(), selectorOrder.end());
    for (const auto &item : selectorOrder) { m_selectorEntries.push_back(item.second); }
    // First-use font and atlas uploads belong to loading, not the first shot
    // or a wave-clear frame. No gameplay or notification state is advanced.
    constexpr unsigned movies[] = {1, 3, 7, 34, 87, 116, 130, 131, 134, 135};
    for (unsigned movie : movies) {
        if (!m_movies.Draw(movie, 600)) { return false; }
    }
    constexpr unsigned sprites[] = {6, 7, 8, 27, 28, 35, 36, 39, 87};
    for (unsigned sprite : sprites) { m_movies.SpriteDuration(1, sprite); }
    for (unsigned font : {0u, 1u, 5u, 6u, 7u}) { m_movies.TextWidth("0123456789", font); }
    // Composite sticks and badges have their own lazy-expanded frame caches.
    // Draw a neutral snapshot while the loading screen still owns presentation.
    if (!DrawOriginalControls(SurvivalHudState{})) { return false; }
    return true;
}

/** Original pack12 LEVEL string references exercise the radio popup end to end. */
int RunOriginalDialogCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CWindow window;
    if (!window.Open("Gun Bros", 1024, 768)) { return 1; }
    SurvivalHud hud;
    if (!hud.Init(toc, tables)) { return 1; }
    const int packIndex = toc.GetPackIndexFromName("pack12");
    if (packIndex < 0) { return 1; }
    const unsigned hash = toc.GetPack(packIndex)->GetPackHash();
    unsigned tested = 0, failures = 0;
    const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Level);
    for (unsigned ordinal = 0; ordinal < count; ++ordinal) {
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(hash, GameSection::Level, ordinal, bytes)) { return 1; }
        CArrayInputStream input(bytes);
        CLevel::Template data;
        if (!data.Init(input) || input.Available() != 0) { return 1; }
        for (const auto &resource : data.script.GetResources()) {
            if (resource.sectionOrType != 254) { continue; }
            CGameAssetRef ref;
            ref.packHash = resource.packHash;
            ref.assetId = resource.resourceId;
            const auto text = ReadGameString(toc, ref);
            if (text.empty()) {
                // LEVEL resource 6 is not consumed by its ShowDialog call.
                // Keep the unresolved reference visible; do not create text.
                std::printf("[dialog-check] unreadable string=%08x:%u length=%zu\n", ref.packHash, ref.assetId, text.size());
                continue;
            }
            if (!hud.ShowDialog(text, true, 0)) { ++failures; continue; }
            SurvivalHudState state;
            state.dialog = text;
            if (hud.Pointer(state, 800, 600, true) == SurvivalHudAction::Continue ||
                hud.CapturesPointer(state, 800, 600)) { std::printf("[dialog-check] invented input capture\n"); ++failures; }
            unsigned elapsed = 0;
            while (!hud.IsDialogDone() && elapsed < 60000) {
                hud.UpdateDialog(16);
                elapsed += 16;
                if (elapsed == 3008) {
                    glViewport(0, 0, 1024, 768);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    if (!hud.m_dialog.Draw() || !window.SaveFrame("out/original-dialog-" + std::to_string(tested) + ".png")) { ++failures; }
                }
            }
            if (!hud.IsDialogDone() || elapsed <= 1000) { std::printf("[dialog-check] auto close failed\n"); ++failures; }
            // Script-owned dialogs remain until native70 requests the outro.
            if (!hud.ShowDialog(text, false, 0)) { ++failures; }
            for (unsigned tick = 0; tick < 4000; ++tick) { hud.UpdateDialog(16); }
            if (hud.IsDialogDone()) { std::printf("[dialog-check] manual closed early\n"); ++failures; }
            hud.ClearDialog(false);
            if (hud.IsDialogDone()) { ++failures; }
            for (unsigned tick = 0; tick < 1000; ++tick) { hud.UpdateDialog(16); }
            if (!hud.IsDialogDone()) { std::printf("[dialog-check] outro not completed\n"); ++failures; }
            std::printf("[dialog-check] LEVEL=%u string=%08x:%u pages=%u elapsed=%u text=%s\n",
                ordinal, ref.packHash, ref.assetId, hud.m_dialog.PageCount(), elapsed, text.c_str());
            ++tested;
        }
    }
    if (tested == 0) { ++failures; }
    std::printf("[dialog-check] original-strings=%u failures=%u\n", tested, failures);
    return failures != 0;
}

void SurvivalHud::Icon(unsigned type, const GameObjectRef &object, const MovieRegion &region) {
    if (object.IsNull()) { return; }
    if (type == 17 && region.width < 60) {
        for (const PowerupEntry &entry : m_powerups) {
            if (entry.resource.packHash != object.packHash || entry.resource.localIndex != object.localIndex) { continue; }
            const CGameSpriteGluRef &sprite = entry.data.sprite;
            m_powerupRenderers[sprite.packHash]->DrawSpriteFitted(sprite.archetype, sprite.animation, 0,
                region.x, region.y, region.width, region.height);
            return;
        }
    }
    for (const StoreEntry &entry : m_store) {
        // Bundle thumbnails depict several products; a single equipped icon
        // must resolve the matching single-product offer instead.
        bool singleProduct = true;
        for (const GameObjectTypeRef &ref : entry.data.objects) {
            if (ref.type != type || ref.object.packHash != object.packHash || ref.object.localIndex != object.localIndex) { singleProduct = false; break; }
        }
        if (!singleProduct) { continue; }
        for (const GameObjectTypeRef &reference : entry.data.objects) {
            if (reference.type != type || reference.object.packHash != object.packHash || reference.object.localIndex != object.localIndex) { continue; }
            const CGameAssetRef &image = entry.data.assets[1];
            if (image.IsNull() || image.assetId < 0) { continue; }
            const std::uint64_t key = (static_cast<std::uint64_t>(image.packHash) << 32) | image.assetId;
            if (m_icons.count(key) == 0) {
                std::vector<std::uint8_t> bytes;
                PNGImage decoded;
                auto texture = std::make_unique<CTexture>();
                if (!m_tables->ReadSectionResource(image.packHash, GameSection::Png, image.assetId, bytes) ||
                    !PNGDecode(bytes, decoded) || !texture->Create(decoded)) { return; }
                m_icons[key] = std::move(texture);
            }
            m_movies.Image(*m_icons[key], region.x, region.y, region.width, region.height);
            return;
        }
    }
}

std::vector<SurvivalHud::Button> SurvivalHud::Buttons(const SurvivalHudState &state) const {
    // Every entry, including isolated map studies, uses original input geometry.
    if (state.shopOpen || state.paused || state.dead || state.cleared) { return {}; }
    return OriginalControlButtons(state);
}

bool SurvivalHud::FindActionRegion(const SurvivalHudState &state, SurvivalHudAction action, MovieRegion &region) const {
    if (state.shopOpen) {
        for (const auto &hit : m_selectorHits) {
            if (hit.action == action) { region = hit.area; return true; }
        }
        return false;
    }
    if (state.paused && !m_pauseHelp && action == SurvivalHudAction::Resume) {
        for (const auto &hit : m_pauseHits) {
            if (hit.second >= m_pauseItems.size()) { continue; }
            const auto *entry = OriginalMenuData("MDS_PAUSE_ROOT", m_pauseItems[hit.second]);
            if (entry->action == 31) { region = hit.first; return true; }
        }
        return false;
    }
    for (const auto &button : Buttons(state)) {
        if (button.action == action) { region = button.rect; return true; }
    }
    return false;
}

SurvivalHudAction SurvivalHud::Pointer(const SurvivalHudState &state, float x, float y, bool down) {
    m_mouseX = x;
    m_mouseY = y;
    const bool clicked = down && !m_previousDown;
    m_previousDown = down;
    if (!clicked) { return SurvivalHudAction::None; }
    if (state.shopOpen) {
        if (m_selectorPromptRequested || m_selectorPrompt.IsActive()) {
            if (m_selectorPrompt.IsReady()) {
                for (const auto &hit : m_selectorPromptHits) {
                    if (!hit.first.Contains(x, y)) { continue; }
                    if (hit.second == 45) { m_selectorPrompt.Hide(); }
                    if (hit.second == 71) {
                        // The Windows selector has no live NGS checkout. Preserve
                        // the original offline response rather than invent a payment.
                        m_selectorPrompt = CMenuPopupPrompt{};
                        m_selectorPromptTable = "MDS_STORE_PROMPT_OFFLINE";
                        m_selectorPromptBody.clear();
                        m_selectorPromptRequested = true;
                        m_selectorPromptFunds = false;
                    }
                    break;
                }
            }
            return SurvivalHudAction::None;
        }
        for (const auto &hit : m_selectorHits) {
            if (!hit.area.Contains(x, y)) { continue; }
            if (hit.storeIndex >= 0) { m_selectedItem = hit.storeIndex; }
            return hit.action;
        }
        return SurvivalHudAction::None;
    }
    if (state.paused && !state.shopOpen && !state.dead && !state.cleared) {
        return OriginalPausePointer(state, x, y);
    }
    for (const Button &button : Buttons(state)) {
        if (button.rect.Contains(x, y)) { return button.action; }
    }
    return SurvivalHudAction::None;
}

bool SurvivalHud::CapturesPointer(const SurvivalHudState &state, float x, float y) const {
    if (state.shopOpen || state.paused || state.dead || state.cleared) { return true; }
    for (const Button &button : Buttons(state)) {
        if (button.rect.Contains(x, y)) { return true; }
    }
    return false;
}

void SurvivalHud::Centre(const std::string &text, float y, unsigned font, float scale) {
    const float width = m_movies.TextWidth(text, font, scale);
    m_movies.Text(text, 512 - width * 0.5f, y, font, scale);
}

bool SurvivalHud::Draw(const SurvivalHudState &state) {
    for (const auto &bar : state.enemyHealthBars) {
        // Original Utility::DrawRect border 0xFF7F8C98 and red fill.
        m_movies.Rectangle(bar.x, bar.y, bar.width, 1, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_movies.Rectangle(bar.x, bar.y + bar.height - 1, bar.width, 1, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_movies.Rectangle(bar.x, bar.y, 1, bar.height, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_movies.Rectangle(bar.x + bar.width - 1, bar.y, 1, bar.height, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_movies.Rectangle(bar.x + bar.border, bar.y + bar.border,
            (bar.width - 2 * bar.border) * bar.fraction, bar.height - 2 * bar.border,
            bar.red, std::max(0.0f, bar.red - 200 / 255.0f), std::max(0.0f, bar.red - 200 / 255.0f), 1);
    }
    if (!state.shopOpen) { m_selectorBound = false; m_selectorHits.clear(); }
    if (!state.paused && m_pauseBound) { m_pauseHelp = false; ResetPauseList(); }
    ObserveProgress(state);
    if (state.withBrother && state.brotherHealth > 0 && state.brotherLabelAlpha > 0) {
        m_movies.Text(state.brotherName, state.brotherLabelX - m_movies.TextWidth(state.brotherName, 0) * 0.5f,
            state.brotherLabelY - m_movies.TextHeight(0) / 2, 0, 1, 0, state.brotherLabelAlpha);
    }
    // Preserve the authored iPad bottom rail and original top-left pause control.
    // Exact ARMv7 CLevelIndicator::INDICATOR_ANIMS bytes at 0x3C4AB0.
    constexpr unsigned indicatorAnimations[7][3] = {{31,31,255}, {67,67,68}, {53,54,255},
        {69,69,70}, {64,64,65}, {64,64,66}, {69,69,70}};
    // CLevelIndicator::Init :191653 uses 25/25/100 camera viewport units.
    // Convert those physical-screen margins into the host's logical HUD canvas.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float viewportScale = std::min(viewport[2] / 480.0f, viewport[3] / 320.0f);
    const float horizontalMargin = int(25 * viewportScale) * 1024.0f / viewport[2];
    const float topMargin = int(25 * viewportScale) * 768.0f / viewport[3];
    const float bottomMargin = int(100 * viewportScale) * 768.0f / viewport[3];
    for (const CLevelIndicator &indicator : state.indicators) {
        if (indicator.type >= 7 || indicator.IsDone()) { continue; }
        const float x = std::clamp(indicator.x, horizontalMargin, 1024.0f - horizontalMargin);
        const float y = std::clamp(indicator.y, topMargin, 768.0f - bottomMargin);
        const float dx = indicator.x - x, dy = indicator.y - y;
        float angle = 0;
        if (std::hypot(dx, dy) >= 1) { angle = std::atan2(dx, -dy) * 180 / 3.14159265f; }
        unsigned animation = indicatorAnimations[indicator.type][0];
        unsigned elapsed = indicator.elapsedMs;
        // Type 2 has a distinct appearing animation before its idle loop.
        if (indicator.type == 2) {
            const unsigned introDuration = m_movies.SpriteDuration(1, animation);
            if (introDuration > 0 && elapsed >= introDuration) { animation = indicatorAnimations[2][1]; elapsed -= introDuration; }
        }
        m_movies.DrawSprite(1, animation, elapsed, x, y, 1, indicator.Alpha(), angle);
        const unsigned icon = indicatorAnimations[indicator.type][2];
        if (icon != 255) { m_movies.DrawSprite(1, icon, elapsed, x, y, 1, indicator.Alpha()); }
    }
    if (!DrawOriginalControls(state)) { return false; }
    const unsigned visibleWave = std::min(state.wave, 499u);
    std::string waveTitle = "WAVE " + std::to_string(visibleWave % 50 + 1);
    std::string waveSubtitle = "REVOLUTION " + std::to_string(visibleWave / 50 + 1) + " / 10";
    if (state.horde) {
        waveTitle = "HORDE " + std::to_string(state.wave + 1);
        char stopwatch[32];
        std::snprintf(stopwatch, sizeof(stopwatch), "%02u:%02u.%u", state.stopwatchMs / 60000,
            state.stopwatchMs / 1000 % 60, state.stopwatchMs / 100 % 10);
        waveSubtitle = std::string("BOKOR  ") + stopwatch;
    }
    std::string reward = "XPLODIUM " + std::to_string(state.xplodium);
    if (state.horde) { reward = "POINTS " + std::to_string(state.score); }
    if (GameHostSettings().debugMode) {
        m_movies.Rectangle(80, 0, 784, 71, 0, 0, 0, 0.65f);
        Centre(waveTitle + "  " + waveSubtitle, 8, 0, 0.68f);
        Centre("ENEMIES " + std::to_string(state.enemies) + "  KILLS " + std::to_string(state.kills) + "  " + reward, 35, 0, 0.68f);
        m_movies.Text(state.buffs, 16, 126, 0, 0.62f, 950);
        char debug[240];
        std::snprintf(debug, sizeof(debug), "FPS %.1f / %.2f MS / XY %.1f %.1f / DAMAGE %.1f / BONUS %llu / XP %llu",
            1000.0f / std::max(0.1f, state.frameMs), state.frameMs, state.playerX, state.playerY, state.damageDealt,
            state.perfectBonus, state.experience);
        m_movies.Rectangle(8, 93, 1008, 25, 0, 0, 0, 0.75f);
        m_movies.Text(debug, 12, 98, 0, 0.62f);
    }
    DrawNotice();
    if (state.shopOpen) { return DrawOriginalSelector(state); }
    if (state.paused && !state.dead && !state.cleared) { return DrawOriginalPause(state); }
    // End-of-run navigation is owned by the original postgame menu in the shell.
    // CDialogPopup draws the original radio portrait beside its text region.
    if (!m_dialog.Draw()) { return false; }
    return m_movies.Failures() == 0;
}

const StoreEntry *SurvivalHud::SelectedItem() const {
    if (m_selectedItem < 0 || m_selectedItem >= static_cast<int>(m_store.size())) { return nullptr; }
    return &m_store[m_selectedItem];
}

void SurvivalHud::Scroll(const SurvivalHudState &state, float amount) {
    if (amount == 0) { return; }
    if (state.shopOpen) {
        if (!state.itemChoice) {
            const float maximum = std::max(0.0f, float(m_selectorEntries.size()) - 3);
            m_selectorTarget = std::clamp(m_selectorTarget - amount, std::min(2.0f, maximum), maximum);
        }
        return;
    }
    if (state.paused && !state.shopOpen) {
        MovieRegion content;
        const unsigned movie = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_PAUSE");
        unsigned start = 0, end = 0;
        if (!m_movies.GetMovie(movie)->GetChapterRange(1, start, end)) { return; }
        if (m_movies.Region(movie, 8, start, content) && content.Contains(m_mouseX, m_mouseY)) {
            m_pauseBodyPosition = std::max(0.0f, m_pauseBodyPosition - amount);
        } else {
            m_pauseTarget = std::clamp(m_pauseTarget - amount, 0.0f, std::max(0.0f, float(m_pauseItems.size()) - 3));
        }
        return;
    }

}

void SurvivalHud::ResetNotices() {
    m_notices.clear();
    m_observedProgress = false;
    m_interstitialCompleted = false;
}

std::string SurvivalHud::OriginalNoticeNumber(const char *name, unsigned number) {
    // OnLevelUp/OnLevelStart/OnWaveClear pass one signed integer to SWPrintF_S.
    // ARM: 0x62A40, 0x63588, 0x62714 and 0x627B0 (LEVEL reward percent).
    std::string text = m_movies.NamedString(name);
    std::size_t position = text.find("%i");
    if (position == std::string::npos) { position = text.find("%d"); }
    if (position != std::string::npos) { text.replace(position, 2, std::to_string(number)); }
    std::size_t percent = text.find("%%");
    while (percent != std::string::npos) {
        text.replace(percent, 2, "%");
        percent = text.find("%%", percent + 1);
    }
    return text;
}

void SurvivalHud::QueueOriginalNotice(const char *movie, const std::string &title, const std::string &footer, bool releaseLevel) {
    // SetUpOverlay :87596 uses a six-slot ring with one empty slot.
    if (m_notices.size() >= 5) { return; }
    m_notices.push_back({m_movies.Ordinal(movie), 0, title, footer, releaseLevel});
}

bool SurvivalHud::HasInterstitial() const {
    for (const auto &notice : m_notices) { if (notice.releaseLevel) { return true; } }
    return false;
}

bool SurvivalHud::TakeInterstitialCompletion() {
    const bool completed = m_interstitialCompleted;
    m_interstitialCompleted = false;
    return completed;
}

void SurvivalHud::BeginOriginalLevel(unsigned wave, bool horde, bool boss) {
    ResetNotices();
    const char *name = "IDS_HUD_WAVE_START";
    if (horde) { name = "IDS_HUD_HORDE_START"; }
    std::string text = OriginalNoticeNumber(name, wave);
    if (boss) { text = m_movies.NamedString("IDS_HUD_BOSS_WAVE_START"); }
    QueueOriginalNotice("GLU_MOVIE_WAVE_CLEARED", text, "", true);
    std::printf("[hud-overlay] begin wave=%u horde=%d boss=%d text=%s\n", wave, horde, boss, text.c_str());
}

void SurvivalHud::OnOriginalWaveClear(unsigned wave, bool perfect, unsigned rewardPercent, bool boss) {
    // OnWaveClear :89760 discards previous notices before its new sequence.
    m_notices.clear();
    m_interstitialCompleted = false;
    std::string text = OriginalNoticeNumber("IDS_HUD_WAVE_CLEAR", wave);
    // Original ARM 0x626A0..0x6271C loads the boss string but never copies it
    // to the zeroed output buffer. Preserve this observed original defect.
    if (boss) { text.clear(); }
    QueueOriginalNotice("GLU_MOVIE_WAVE_CLEARED", text, "", !perfect);
    if (perfect) {
        QueueOriginalNotice("GLU_MOVIE_PERFECT_WAVE", m_movies.NamedString("IDS_HUD_WAVE_PERFECT"),
            OriginalNoticeNumber("IDS_HUD_WAVE_PERFECT_SUMMARY", rewardPercent), true);
    }
    std::printf("[hud-overlay] clear wave=%u perfect=%d reward=%u boss=%d\n", wave, perfect, rewardPercent, boss);
}

void SurvivalHud::ObserveProgress(const SurvivalHudState &state) {
    // Native wave events come directly from the LEVEL consumer, not from
    // comparing two rendered snapshots. Reloading an account is no level-up.
    if (m_observedProgress && state.level > m_previousLevel) {
        QueueOriginalNotice("GLU_MOVIE_LEVEL_UP", OriginalNoticeNumber("IDS_HUD_LEVEL_REACHED", state.level));
        QueueOriginalNotice("GLU_MOVIE_LEVEL_UP", m_movies.NamedString("IDS_HUD_HEALTH_UP"));
    }
    m_previousLevel = state.level;
    m_observedProgress = true;
}

void SurvivalHud::Advance(int deltaMs) {
    if (deltaMs > 0) {
        m_controlTime += deltaMs;
        for (auto &meter : m_meters) { meter.Update(deltaMs); }
    }
    if (deltaMs <= 0 || m_notices.empty()) { return; }
    Notice &notice = m_notices.front();
    notice.elapsed += static_cast<unsigned>(deltaMs);
    CMovie *movie = m_movies.GetMovie(notice.movie);
    if (movie != nullptr && notice.elapsed >= movie->duration) {
        if (notice.releaseLevel) { m_interstitialCompleted = true; }
        m_notices.erase(m_notices.begin());
    }
}

void SurvivalHud::DrawNotice() {
    if (m_notices.empty()) { return; }
    const Notice &notice = m_notices.front();
    {
        // OverlayDraw :86532 uses font 11 at its original size and centers in
        // the Movie region. Drawing as a callback preserves authored layering.
        class OverlayCallback : public IMovieRegionCallback {
        public:
            OverlayCallback(MovieRenderer &renderer, const Notice &current) : movies(renderer), notice(current) {}
            bool DrawMovieRegion(const MovieRegion &area) override {
                if (area.index > 1) { return true; }
                const std::string *text = &notice.title;
                if (area.index == 1) { text = &notice.footer; }
                if (text->empty()) { return true; }
                const float x = area.x + int(area.width) / 2 - int(movies.TextWidth(*text, 11)) / 2;
                const float y = area.y + int(area.height) / 2 - int(movies.TextHeight(11)) / 2;
                return movies.Text(*text, x, y, 11, 1, 0, area.alpha);
            }
            MovieRenderer &movies;
            const Notice &notice;
        } callback(m_movies, notice);
        m_movies.Draw(notice.movie, notice.elapsed, 512, 384, 1024, 768, 0, 1, &callback);
        return;
    }
}

/** Retained HUD milestone now checks original controls, selector and pause tree. */
int RunSurvivalHudCheck(const std::string &bigDirectory) {
    if (RunOriginalHudCheck(bigDirectory) != 0 || RunOriginalPowerupSelectorCheck(bigDirectory) != 0 ||
        RunOriginalPauseCheck(bigDirectory) != 0) { return 1; }
    return 0;
}

void SurvivalHud::ResetPauseList() {
    m_pauseBound = false;
    m_pauseTime = 0;
    m_pauseBodyTime = 0;
    m_pauseFocus = 0;
    m_pausePosition = 0;
    m_pauseTarget = 0;
    m_pauseBodyPosition = 0;
    m_pauseHits.clear();
}

void SurvivalHud::AdvanceMenu(unsigned deltaMs) {
    m_pauseDelta = deltaMs;
    if (!m_selectorBound) { return; }
    m_selectorTime += deltaMs;
    m_selectorChoiceTime += deltaMs;
    m_selectorPrompt.Update(deltaMs);
    unsigned start = 0, end = 0;
    if (m_movies.GetMovie(m_movies.Ordinal("GLU_MOVIE_POWER_UP_LAYOUT"))->GetChapterRange(1, start, end)) {
        const float step = float(deltaMs) / (end - start + 1);
        if (m_selectorPosition < m_selectorTarget) { m_selectorPosition = std::min(m_selectorPosition + step, m_selectorTarget); }
        else { m_selectorPosition = std::max(m_selectorPosition - step, m_selectorTarget); }
    }
}

bool SurvivalHud::BackFromHelp() {
    if (!m_pauseHelp) { return false; }
    m_pauseHelp = false;
    ResetPauseList();
    return true;
}

std::string SurvivalHud::PauseText(const SurvivalHudState &state, unsigned index, unsigned slot) {
    const char *table = "MDS_PAUSE_ROOT";
    if (m_pauseHelp) { table = "MDS_HELP"; }
    const auto *entry = OriginalMenuData(table, index);
    if (entry == nullptr || slot > 1) { return {}; }
    if (entry->strings[slot][0] != 0) { return m_movies.NamedString(entry->strings[slot]); }
    // CMenuAction::ResolveActionString :95837..95911.
    if (entry->action == 9) {
        if (state.soundEnabled) { return m_movies.NamedString("IDS_SOUND_ON_GAME"); }
        return m_movies.NamedString("IDS_SOUND_OFF_GAME");
    }
    if (entry->action == 10) {
        if (state.musicEnabled) { return m_movies.NamedString("IDS_MUSIC_ON_GAME"); }
        return m_movies.NamedString("IDS_MUSIC_OFF_GAME");
    }
    if (entry->action == 16) {
        if (state.dockedSticks) { return m_movies.NamedString("IDS_DOCKED_STICKS_ON_GAME"); }
        return m_movies.NamedString("IDS_DOCKED_STICKS_OFF_GAME");
    }
    return {};
}

SurvivalHudAction SurvivalHud::OriginalPausePointer(const SurvivalHudState &state, float x, float y) {
    for (const auto &hit : m_pauseHits) {
        if (!hit.first.Contains(x, y)) { continue; }
        if (hit.second == UINT32_MAX) { BackFromHelp(); return SurvivalHudAction::None; }
        const unsigned item = hit.second;
        if (item != m_pauseFocus) {
            m_pauseButtonTimes[m_pauseFocus] = 0;
            m_pauseFocus = item;
            m_pauseBodyTime = 0;
            m_pauseBodyPosition = 0;
            unsigned start = 0, end = 0;
            m_movies.GetMovie(m_movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON"))->GetChapterRange(2, start, end);
            m_pauseButtonTimes[item] = start;
            m_pauseTarget = std::clamp(float(item) - 1, 0.0f, float(m_pauseItems.size()) - 3);
        }
        const char *table = "MDS_PAUSE_ROOT";
        if (m_pauseHelp) { table = "MDS_HELP"; }
        const auto *entry = OriginalMenuData(table, m_pauseItems[item]);
        switch (entry->action) {
        case 31: return SurvivalHudAction::Resume;
        case 40: return SurvivalHudAction::Exit;
        case 9: return SurvivalHudAction::Sound;
        case 10: return SurvivalHudAction::Music;
        case 16: return SurvivalHudAction::DockedSticks;
        case 1: m_pauseHelp = true; ResetPauseList(); break;
        }
        return SurvivalHudAction::None;
    }
    return SurvivalHudAction::None;
}

bool SurvivalHud::DrawOriginalPause(const SurvivalHudState &state) {
    const unsigned list = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_PAUSE");
    const unsigned button = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON");
    const unsigned body = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_TEXT");
    unsigned start = 0, end = 0, focusStart = 0, focusEnd = 0, restStart = 0, restEnd = 0;
    unsigned bodyStart = 0, bodyEnd = 0, textStart = 0, textEnd = 0;
    if (!m_movies.GetMovie(list)->GetChapterRange(1, start, end) ||
        !m_movies.GetMovie(button)->GetChapterRange(2, focusStart, focusEnd) ||
        !m_movies.GetMovie(button)->GetChapterRange(0, restStart, restEnd) ||
        !m_movies.GetMovie(body)->GetChapterRange(0, bodyStart, bodyEnd) ||
        !m_movies.GetMovie(body)->GetChapterRange(1, textStart, textEnd)) { return false; }
    if (!m_pauseBound) {
        m_pauseBound = true;
        m_pauseItems.clear();
        const char *table = "MDS_PAUSE_ROOT";
        if (m_pauseHelp) { table = "MDS_HELP"; }
        for (unsigned index = 0; OriginalMenuData(table, index) != nullptr; ++index) {
            if (!m_pauseHelp && OriginalMenuData(table, index)->action == 135) { continue; }
            m_pauseItems.push_back(index);
        }
        m_pauseButtonTimes.assign(m_pauseItems.size(), 0);
        m_pauseButtonTimes[0] = focusStart;
    } else {
        m_pauseTime = std::min(start, m_pauseTime + m_pauseDelta);
        m_pauseBodyTime = std::min(bodyEnd, m_pauseBodyTime + m_pauseDelta);
    }
    const float step = float(m_pauseDelta) / (end - start + 1);
    if (m_pausePosition < m_pauseTarget) { m_pausePosition = std::min(m_pauseTarget, m_pausePosition + step); }
    else { m_pausePosition = std::max(m_pauseTarget, m_pausePosition - step); }
    const int first = static_cast<int>(std::floor(m_pausePosition)) - 1;
    unsigned time = m_pauseTime;
    if (time == start) { time += unsigned((m_pausePosition - std::floor(m_pausePosition)) * (end - start)); }
    m_pauseHits.clear();
    // CMenuSystem supplies HEADER with NAVBAR_DISABLED; no trunk buttons.
    const unsigned header = m_movies.Ordinal("GLU_MOVIE_HEADER");
    unsigned headerStart = 0, headerEnd = 0;
    if (!m_movies.GetMovie(header)->GetChapterRange(2, headerStart, headerEnd)) { return false; }
    unsigned headerTime = m_pauseTime;
    if (m_pauseTime == start) { headerTime = headerStart; }
    class PauseHeader : public IMovieRegionCallback {
    public:
        PauseHeader(MovieRenderer &renderer, const SurvivalHudState &snapshot) : movies(renderer), state(snapshot) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 14 || region.index == 15) {
                std::uint32_t value = static_cast<std::uint32_t>(state.coins);
                if (region.index == 15) { value = static_cast<std::uint32_t>(state.warbucks); }
                movies.Text(std::to_string(value), region.x, region.y, 0, 1, 0, region.alpha);
            }
            if (region.index == 16) {
                const unsigned info = movies.Ordinal("GLU_MOVIE_INFO_CLUSTER");
                unsigned start = 0, end = 0;
                start = 0; // INFO_CLUSTER has no chapter track; native code loops duration.
                if (!movies.Draw(info, start, region.x, region.y)) { return false; }
                for (const auto &part : movies.Regions(info, start, region.x, region.y)) {
                    if (part.index == 0) {
                        const float ratio = std::clamp(float(state.experience) / std::max<std::uint64_t>(1, state.experienceDelta), 0.0f, 1.0f);
                        movies.Rectangle(part.x, part.y, int(part.width * ratio), part.height, 1.0f / 255, 149.0f / 255, 215.0f / 255, part.alpha);
                    } else if (part.index < 4) {
                        char digits[16];
                        std::snprintf(digits, sizeof(digits), "%.3u", state.level);
                        movies.Text(std::string(1, digits[part.index - 1]), part.x, part.y + part.height / 2 - std::floor(movies.TextHeight(7) / 2), 7, 1, 0, part.alpha);
                    }
                }
            }
            return true;
        }
        MovieRenderer &movies;
        const SurvivalHudState &state;
    } headerCallback(m_movies, state);
    if (m_pauseHelp && !m_movies.DrawNamed("GLU_MOVIE_BG_OPTIONS", m_pauseTime)) { return false; }
    if (!m_movies.Draw(header, headerTime, 512, 384, 1024, 768, 0, 1, &headerCallback) || !m_movies.Draw(list, time)) { return false; }
    // Shared RADIAL_WIDGET is present even when the root has no BACK action.
    for (const auto &region : m_movies.Regions(list, time)) {
        if (region.index != m_movies.Regions(list, time, 512, 384, true).size() - 3) { continue; }
        const unsigned radial = m_movies.Ordinal("GLU_MOVIE_RADIAL_WIDGET");
        unsigned radialStart = 0, radialEnd = 0;
        if (!m_movies.GetMovie(radial)->GetChapterRange(1, radialStart, radialEnd)) { return false; }
        if (!m_movies.Draw(radial, radialStart, region.x + int(region.width) / 2, region.y + int(region.height) / 2)) { return false; }
    }
    if (m_pauseHelp) {
        // CMenuList::Init :140686 creates BACK_BUTTON at region tagged 1.
        for (const auto &region : m_movies.Regions(list, time)) {
            if (region.index != m_movies.Regions(list, time, 512, 384, true).size() - 3) { continue; }
            const unsigned back = m_movies.Ordinal("GLU_MOVIE_BACK_BUTTON");
            unsigned backStart = 0, backEnd = 0;
            if (!m_movies.GetMovie(back)->GetChapterRange(0, backStart, backEnd)) { return false; }
            const float x = region.x + int(region.width) / 2, y = region.y + int(region.height) / 2;
            if (!m_movies.Draw(back, std::min(m_pauseTime, backEnd), x, y)) { return false; }
            if (m_pauseTime != start) { continue; }
            for (const auto &part : m_movies.Regions(back, backEnd, x, y, true)) {
                if (part.index == 0) { m_pauseHits.insert(m_pauseHits.begin(), {part, UINT32_MAX}); }
            }
        }
    }
    for (const auto &region : m_movies.Regions(list, time)) {
        if (region.type < 2) { continue; }
        const int index = first + int(region.type) - 2;
        if (index < 0 || index >= int(m_pauseItems.size())) { continue; }
        auto &buttonTime = m_pauseButtonTimes[index];
        if (unsigned(index) == m_pauseFocus) { buttonTime = focusStart + (buttonTime - focusStart + m_pauseDelta) % (focusEnd - focusStart + 1); }
        else { buttonTime = std::min(restEnd, buttonTime + m_pauseDelta); }
        const float x = region.x + int(region.width) / 2, y = region.y + int(region.height) / 2;
        const std::string label = PauseText(state, m_pauseItems[index], 0);
        class Caption : public IMovieRegionCallback {
        public:
            Caption(MovieRenderer &renderer, const std::string &caption) : movies(renderer), label(caption) {}
            bool DrawMovieRegion(const MovieRegion &part) override {
                if (part.index == 1) { movies.Text(label, part.x, part.y + int(part.height) / 2 - int(movies.TextHeight(0)) / 2, 0, 1, 0, part.alpha); }
                return true;
            }
            MovieRenderer &movies;
            const std::string &label;
        } callback(m_movies, label);
        if (!m_movies.Draw(button, buttonTime, x, y, 1024, 768, 0, region.alpha, &callback)) { return false; }
        if (m_pauseTime != start) { continue; }
        for (const auto &part : m_movies.Regions(button, buttonTime, x, y, true)) {
            if (part.index == 0) { m_pauseHits.push_back({part, unsigned(index)}); }
        }
    }
    MovieRegion content, scrollBar, pageBounds;
    if (!m_movies.Region(list, 8, time, content) || !m_movies.Region(list, 9, time, scrollBar) ||
        !m_movies.Region(body, 2, 0, pageBounds)) { return false; }
    const auto lines = FormatStoreText(m_movies, PauseText(state, m_pauseItems[m_pauseFocus], 1), content.width - scrollBar.width, {0, 6, 0, 0, 0});
    std::vector<std::vector<StoreTextLine>> pages(1);
    float pageHeight = 0;
    for (const auto &line : lines) {
        if (!pages.back().empty() && pageHeight + line.height > pageBounds.height) { pages.push_back({}); pageHeight = 0; }
        pages.back().push_back(line);
        pageHeight += line.height;
    }
    m_pauseBodyPosition = std::clamp(m_pauseBodyPosition, 0.0f, float(pages.size() - 1));
    const int firstPage = int(std::floor(m_pauseBodyPosition));
    unsigned textTime = m_pauseBodyTime;
    if (textTime == bodyEnd) { textTime = textStart + unsigned((m_pauseBodyPosition - firstPage) * (textEnd - textStart)); }
    if (!m_movies.Draw(body, textTime, content.x, content.y)) { return false; }
    GLint viewport[4], previousClip[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, previousClip);
    const GLboolean clipped = glIsEnabled(GL_SCISSOR_TEST);
    glEnable(GL_SCISSOR_TEST);
    glScissor(int(content.x * viewport[2] / 1024), int((768 - content.y - content.height) * viewport[3] / 768),
        int(content.width * viewport[2] / 1024), int(content.height * viewport[3] / 768));
    for (const auto &region : m_movies.Regions(body, textTime, content.x, content.y)) {
        if (region.type < 2) { continue; }
        const int page = firstPage + int(region.type) - 2;
        if (page < 0 || page >= int(pages.size())) { continue; }
        float y = region.y;
        for (const auto &line : pages[page]) {
            for (const auto &run : line.runs) { m_movies.Text(run.text, region.x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, content.alpha * region.alpha); }
            y += line.height;
        }
    }
    glScissor(previousClip[0], previousClip[1], previousClip[2], previousClip[3]);
    if (!clipped) { glDisable(GL_SCISSOR_TEST); }
    if (pages.size() > 1) {
        const auto *entry = OriginalMenuData("MDS_SCROLLBARS", 1);
        const unsigned bar = m_movies.Ordinal(entry->movies[0]);
        MovieRegion bounds;
        if (!m_movies.Region(bar, 0, 0, bounds)) { return false; }
        if (!m_movies.Draw(bar, unsigned(m_movies.GetMovie(bar)->duration * m_pauseBodyPosition / (pages.size() - 1)),
            scrollBar.x, scrollBar.y + int(scrollBar.height) / 2 - int(bounds.height) / 2)) { return false; }
    }
    m_pauseDelta = 0;
    return m_movies.Failures() == 0;
}

int RunOriginalPauseCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CWindow window;
    if (!window.Open("Gun Bros - Original Pause Check", 1600, 1200)) { return 1; }
    SurvivalHud hud;
    PackTables tables(toc);
    if (!hud.Init(toc, tables)) { return 1; }
    CProfileManager profile;
    const auto directory = std::filesystem::path("out/ui-original-2026-09-09") / ("pause-profile-" + std::to_string(window.GetTicksMs()));
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, profile, directory, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    SurvivalHudState state;
    // Exercise the standalone-map caller too: pause UI must not depend on
    // whether a native archive was attached to the combat context.
    state.originalUi = false;
    state.paused = true;
    state.health = 1;
    state.maximumHealth = 1;
    state.coins = profile.coins;
    state.warbucks = profile.warbucks;
    state.level = progress.GetLevel();
    state.experience = progress.GetExperienceInLevel();
    state.experienceDelta = progress.GetExperienceDelta();
    state.soundEnabled = profile.soundEnabled;
    state.musicEnabled = profile.musicEnabled;
    state.dockedSticks = profile.options.DockedSticks();
    unsigned failures = 0;
    if (!hud.Draw(state) || !hud.m_pauseHits.empty()) { ++failures; }
    hud.AdvanceMenu(3000);
    if (!hud.Draw(state) || hud.m_pauseItems.size() != 6) { ++failures; }
    unsigned hits = 0;
    for (unsigned index = 0; index < hud.m_pauseItems.size(); ++index) {
        hud.m_pauseTarget = std::clamp(float(index) - 1, 0.0f, float(hud.m_pauseItems.size()) - 3);
        hud.AdvanceMenu(3000);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.Draw(state)) { ++failures; }
        bool found = false;
        for (const auto &hit : hud.m_pauseHits) {
            if (hit.second != index) { continue; }
            const float x = hit.first.x + hit.first.width / 2, y = hit.first.y + hit.first.height / 2;
            hud.Pointer(state, x, y, false);
            const auto action = hud.Pointer(state, x, y, true);
            ++hits;
            found = true;
            const unsigned originalAction = OriginalMenuData("MDS_PAUSE_ROOT", hud.m_pauseItems[index])->action;
            if (originalAction == 31 && action != SurvivalHudAction::Resume) { ++failures; }
            if (originalAction == 40 && action != SurvivalHudAction::Exit) { ++failures; }
            if (originalAction == 9) {
                if (action != SurvivalHudAction::Sound) { ++failures; }
                profile.soundEnabled = !profile.soundEnabled;
                state.soundEnabled = profile.soundEnabled;
            }
            if (originalAction == 10) {
                if (action != SurvivalHudAction::Music) { ++failures; }
                profile.musicEnabled = !profile.musicEnabled;
                state.musicEnabled = profile.musicEnabled;
            }
            if (originalAction == 16) {
                if (action != SurvivalHudAction::DockedSticks) { ++failures; }
                profile.options.ToggleDockedSticks();
                state.dockedSticks = profile.options.DockedSticks();
            }
            break;
        }
        if (!found) { ++failures; }
        if (!hud.Draw(state)) { ++failures; }
        if (index == 0 && !window.SaveFrame("out/ui-original-2026-09-09/pause-original-ready.png")) { ++failures; }
        if (hud.m_pauseHelp) { break; }
    }
    if (!hud.m_pauseHelp) { ++failures; }
    hud.AdvanceMenu(3000);
    if (!hud.Draw(state) || hud.m_pauseItems.size() != 14) { ++failures; }
    for (unsigned index = 0; index < hud.m_pauseItems.size(); ++index) {
        hud.m_pauseFocus = index;
        hud.m_pauseBodyTime = 0;
        hud.m_pauseTarget = std::clamp(float(index) - 1, 0.0f, float(hud.m_pauseItems.size()) - 3);
        hud.AdvanceMenu(3000);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.Draw(state) || hud.PauseText(state, index, 0).empty() || hud.PauseText(state, index, 1).empty()) { ++failures; }
        if (index == 0 && !window.SaveFrame("out/ui-original-2026-09-09/pause-original-help.png")) { ++failures; }
    }
    bool returned = false;
    const auto backHits = hud.m_pauseHits;
    for (const auto &hit : backHits) {
        if (hit.second != UINT32_MAX) { continue; }
        const float x = hit.first.x + hit.first.width / 2, y = hit.first.y + hit.first.height / 2;
        hud.Pointer(state, x, y, false);
        hud.Pointer(state, x, y, true);
        returned = !hud.m_pauseHelp;
        break;
    }
    if (!returned || !profile.SaveToDisk(directory)) { ++failures; }
    CProfileManager reloaded;
    if (!LoadNativeProfile(toc, tables, reloaded, directory) || reloaded.soundEnabled != profile.soundEnabled ||
        reloaded.musicEnabled != profile.musicEnabled || reloaded.options.DockedSticks() != profile.options.DockedSticks() ||
        reloaded.coins != state.coins || reloaded.warbucks != state.warbucks) { ++failures; }
    std::printf("[pause-check] native-hits=%u help-items=14 back=%d preference-reload=3 failures=%u\n", hits, returned, failures);
    return failures != 0;
}

std::vector<SurvivalHud::Button> SurvivalHud::OriginalControlButtons(const SurvivalHudState &state) const {
    std::vector<Button> buttons;
    const unsigned base = m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    const unsigned peripheral = m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE");
    unsigned idle = 0, end = 0;
    if (!m_movies.GetMovie(peripheral)->GetChapterRange(3, idle, end)) { return buttons; }
    MovieRegion region;
    if (m_movies.Region(peripheral, 3, idle, region)) { buttons.push_back({region, SurvivalHudAction::Pause, ""}); }
    if (m_movies.Region(base, 2, 0, region)) { buttons.push_back({region, SurvivalHudAction::OpenShop, ""}); }
    if (m_movies.Region(base, 3, 0, region)) { buttons.push_back({region, SurvivalHudAction::SwapWeapon, ""}); }
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (!m_movies.Region(base, 4 + slot, 0, region)) { continue; }
        const float x = region.x + int(region.width) / 2, y = region.y + int(region.height) / 2;
        const char *name = "GLU_MOVIE_POWERUP_BUTTON";
        SurvivalHudAction action = SurvivalHudAction::UseLeft;
        if (slot == 1) { name = "GLU_MOVIE_GRENADE_BUTTON"; action = SurvivalHudAction::UseItem; }
        const unsigned movie = m_movies.Ordinal(name);
        if (!m_movies.GetMovie(movie)->GetChapterRange(2, idle, end)) { continue; }
        for (const auto &part : m_movies.Regions(movie, idle, x, y, true)) {
            if (part.index == 0) { buttons.push_back({part, action, ""}); }
        }
    }
    return buttons;
}

bool SurvivalHud::BackFromSelectorPrompt() {
    if (!m_selectorPromptRequested && !m_selectorPrompt.IsActive()) { return false; }
    m_selectorPromptRequested = false;
    m_selectorPrompt.Hide();
    return true;
}

bool SurvivalHud::DrawOriginalMeter(const MovieRegion &area, unsigned slot) {
    // Original native configuration: border1, outline, full top/bottom,
    // empty top/bottom, separator flag/color. CInputPadMeter::Draw :130857.
    constexpr unsigned colors[2][7] = {
        {0xffb1beca, 0xff01d810, 0xff01a60c, 0xffdb0b0b, 0xffa80b0b, 1, 0xff28750e},
        {0xffb1beca, 0xff0195d7, 0xff0174a6, 0xff8e8e8e, 0xff787878, 0, 0}
    };
    const unsigned *config = colors[slot];
    const float x = area.x, y = area.y;
    const int width = int(area.width), height = int(area.height);
    const unsigned outline = config[0];
    const float red = float((outline >> 16) & 255) / 255, green = float((outline >> 8) & 255) / 255, blue = float(outline & 255) / 255;
    // Utility::DrawRect draws a one-pixel outline; native meter inset is border+1.
    m_movies.Rectangle(x, y, width, 1, red, green, blue, area.alpha);
    m_movies.Rectangle(x, y + height - 1, width, 1, red, green, blue, area.alpha);
    m_movies.Rectangle(x, y, 1, height, red, green, blue, area.alpha);
    m_movies.Rectangle(x + width - 1, y, 1, height, red, green, blue, area.alpha);
    const int interiorWidth = width - 4, interiorHeight = height - 4;
    const int filled = int(interiorWidth * m_meters[slot].GetDrawValue());
    const unsigned highlight = m_meters[slot].GetHighlight();
    unsigned top = 0, bottom = 0, divider = 0;
    for (unsigned channel = 0; channel < 3; ++channel) {
        const unsigned shift = channel * 8;
        top |= std::min(255u, ((config[1] >> shift) & 255) + highlight) << shift;
        bottom |= std::min(255u, ((config[2] >> shift) & 255) + highlight) << shift;
        divider |= std::min(255u, ((config[6] >> shift) & 255) + highlight) << shift;
    }
    int gradientWidth = filled;
    if (config[5] && filled != interiorWidth && filled > 0) {
        --gradientWidth;
        m_movies.Rectangle(x + 2 + gradientWidth, y + 2, 1, interiorHeight,
            float((divider >> 16) & 255) / 255, float((divider >> 8) & 255) / 255, float(divider & 255) / 255, area.alpha);
    }
    return m_movies.Gradient(x + 2, y + 2, gradientWidth, interiorHeight, top, bottom, area.alpha) &&
        m_movies.Gradient(x + 2 + filled, y + 2, interiorWidth - filled, interiorHeight, config[3], config[4], area.alpha);
}

bool SurvivalHud::DrawOriginalPowerup(const SurvivalHudState &state, unsigned slot, float x, float y, unsigned movie, unsigned time) {
    const GameObjectRef *object = &state.leftPowerup;
    unsigned count = state.leftCount;
    if (slot == 1) { object = &state.rightPowerup; count = state.rightCount; }
    const PowerupEntry *powerup = nullptr;
    for (const auto &entry : m_powerups) {
        if (entry.resource.packHash == object->packHash && entry.resource.localIndex == object->localIndex) { powerup = &entry; break; }
    }
    class PowerupCallback : public IMovieRegionCallback {
    public:
        PowerupCallback(SurvivalHud &owner, const PowerupEntry *item, unsigned quantity) : hud(owner), powerup(item), count(quantity) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            // Original binding: region1 = count, region2 = alternate input icon.
            if (powerup == nullptr) { return true; }
            if (region.index == 1) {
                unsigned animation = 87;
                if (count > 9) { animation = 88; }
                const int x = int(region.x) + int(region.width) / 2, y = int(region.y) + int(region.height) / 2;
                if (!hud.m_movies.DrawSprite(0, animation, 0, float(x), float(y), 1, region.alpha)) { return false; }
                const auto text = std::to_string(count);
                return hud.m_movies.Text(text, x - int(hud.m_movies.TextWidth(text, 0)) / 2,
                    y - int(hud.m_movies.TextHeight(0)) / 2, 0, 1, 0, region.alpha);
            }
            if (region.index == 2) {
                const auto &sprite = powerup->data.sprite;
                auto &renderer = *hud.m_powerupRenderers.at(sprite.packHash);
                MovieRegion bounds;
                const unsigned animation = powerup->data.field29;
                if (!renderer.SpriteBounds(sprite.archetype, animation, bounds)) { return false; }
                return renderer.DrawSprite(sprite.archetype, animation, 0,
                    region.x - bounds.x + int(region.width - bounds.width) / 2,
                    region.y - bounds.y + int(region.height - bounds.height) / 2, 1, region.alpha);
            }
            return true;
        }
        SurvivalHud &hud;
        const PowerupEntry *powerup;
        unsigned count;
    } callback(*this, powerup, count);
    return m_movies.Draw(movie, time, x, y, 1024, 768, 0, 1, &callback);
}

bool SurvivalHud::DrawOriginalControls(const SurvivalHudState &state) {
    const float health = state.health / std::max(1.0f, state.maximumHealth);
    const float experience = float(state.experience) / std::max<std::uint64_t>(1, state.experienceDelta);
    if (!m_metersBound) {
        m_metersBound = true;
        m_meters[0].SnapValue(health);
        m_meters[1].SnapValue(experience);
    } else {
        m_meters[0].SetValue(health);
        m_meters[1].SetValue(experience);
    }
    const unsigned base = m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    class BaseCallback : public IMovieRegionCallback {
    public:
        BaseCallback(SurvivalHud &owner, const SurvivalHudState &snapshot) : hud(owner), state(snapshot) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index < 2) { return hud.DrawOriginalMeter(region, region.index); }
            if (region.index == 2 || region.index == 3) {
                unsigned animation = 27;
                // CInputPad::Base::SetState :87545: selector state 7 keeps
                // the red button down (28); returning state 8 restores 27.
                if (state.shopOpen) { animation = 28; }
                if (region.index == 3) {
                    animation = 35;
                    // Base::UpdateInput :88480 uses 36 for a held touch in
                    // region 3. Actions still fire once on the down edge.
                    if ((state.swapKeyDown || (hud.m_previousDown && region.Contains(hud.m_mouseX, hud.m_mouseY))) &&
                        !state.shopOpen && !state.paused && !state.dead && !state.cleared) {
                        animation = 36;
                    }
                }
                return hud.m_movies.DrawSprite(1, animation, hud.m_controlTime, region.x, region.y + region.height, 1, region.alpha);
            }
            return true;
        }
        SurvivalHud &hud;
        const SurvivalHudState &state;
    } baseCallback(*this, state);
    if (!m_movies.Draw(base, 0, 512, 384, 1024, 768, 0, 1, &baseCallback)) { return false; }
    MovieRegion stickBounds;
    if (!m_movies.SpriteBounds(1, 6, stickBounds)) { return false; }
    for (unsigned slot = 0; slot < 2; ++slot) {
        MovieRegion area;
        if (!m_movies.Region(base, 4 + slot, 0, area)) { return false; }
        const float x = area.x + int(area.width) / 2, y = area.y + int(area.height) / 2;
        float directionX = state.moveX, directionY = state.moveY;
        const char *name = "GLU_MOVIE_POWERUP_BUTTON";
        if (slot == 1) { directionX = state.aimX; directionY = state.aimY; name = "GLU_MOVIE_GRENADE_BUTTON"; }
        // Windows keys/mouse supply the original normalized control vector.
        // Floating sticks remain hidden until their corresponding input is active.
        if (!state.dockedSticks && directionX == 0 && directionY == 0) { continue; }
        if (!m_movies.DrawSprite(1, 6 + slot, m_controlTime, x, y)) { return false; }
        const unsigned movie = m_movies.Ordinal(name);
        unsigned start = 0, end = 0;
        if (!m_movies.GetMovie(movie)->GetChapterRange(2, start, end) ||
            !DrawOriginalPowerup(state, slot, x, y, movie, start + m_controlTime % (end - start + 1))) { return false; }
        // Bind radius = sprite width * .42; ControlStick::Draw displacement *.35.
        const float travel = stickBounds.width * 0.42f * 0.35f;
        if (!m_movies.DrawSprite(1, 8, m_controlTime, float(int(x + directionX * travel)), float(int(y + directionY * travel)))) { return false; }
    }
    class PeripheralCallback : public IMovieRegionCallback {
    public:
        PeripheralCallback(MovieRenderer &renderer, const SurvivalHudState &snapshot) : movies(renderer), state(snapshot) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 0) {
                if (!state.horde) {
                    if (!movies.DrawSprite(1, 39, 0, region.x, region.y, 1, region.alpha)) { return false; }
                    // Native UTF-32 format at VA0x3C3DF8 is "%03d%%".
                    char number[32];
                    std::snprintf(number, sizeof(number), "%03d%%", state.xplodiumMultiplier);
                    const std::string text = number;
                    return movies.Text(text, region.x + region.width - movies.TextWidth(text, 0),
                        region.y + region.height - movies.TextHeight(0), 0, 1, 0, region.alpha);
                }
                // OnScoreChange :87887 appends localized suffixes to each number.
                const std::string score = std::to_string(state.score) + movies.NamedString("IDS_HUD_SCORE");
                const std::string kills = std::to_string(state.kills) + movies.NamedString("IDS_HUD_KILLS");
                return movies.Text(score, region.x + region.width - movies.TextWidth(score), region.y, 0, 1, 0, region.alpha) &&
                    movies.Text(kills, region.x + region.width - movies.TextWidth(kills), region.y + movies.TextHeight(0), 0, 1, 0, region.alpha);
            }
            if (region.index == 1 && state.horde) {
                const std::string label = movies.NamedString("IDS_HUD_KILL_STREAK");
                const std::string value = std::to_string(state.killStreak);
                float x = region.x + region.width / 2 - int(movies.TextWidth(value, 12)) / 2;
                if (value.size() > 3) { x = region.x + region.width - movies.TextWidth(value, 12); }
                return movies.Text(label, region.x + region.width / 2 - int(movies.TextWidth(label)) / 2,
                    region.y + movies.TextHeight(12), 0, 1, 0, region.alpha) &&
                    movies.Text(value, x, region.y, 12, 1, 0, region.alpha);
            }
            return true;
        }
        MovieRenderer &movies;
        const SurvivalHudState &state;
    } peripheralCallback(m_movies, state);
    const unsigned peripheral = m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE");
    unsigned start = 0, end = 0;
    if (!m_movies.GetMovie(peripheral)->GetChapterRange(3, start, end)) { return false; }
    return m_movies.Draw(peripheral, start, 512, 384, 1024, 768, 0, 1, &peripheralCallback);
}

int RunOriginalHudCheck(const std::string &bigDirectory) {
    unsigned failures = 0;
    // SetValue must preserve the original linear retarget, cosine draw and rate.
    CInputPadMeter meter;
    meter.SnapValue(1);
    meter.SetValue(0);
    meter.Update(10);
    if (std::abs(meter.GetDrawValue() - 0.5f) > 0.0001f || meter.GetHighlight() != 100) { ++failures; }
    meter.SetValue(0.75f);
    if (std::abs(meter.GetDrawValue() - 0.5f) > 0.0001f) { ++failures; }
    meter.Update(80);
    if (meter.GetDrawValue() != 0.75f || meter.GetHighlight() != 0) { ++failures; }
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CWindow window;
    if (!window.Open("Gun Bros - Original HUD Check", 1600, 1200)) { return 1; }
    SurvivalHud hud;
    if (!hud.Init(toc, tables)) { return 1; }
    SurvivalHudState state;
    state.originalUi = true;
    state.health = state.maximumHealth = 1;
    unsigned hits = 0;
    const auto buttons = hud.OriginalControlButtons(state);
    if (buttons.size() != 5) { ++failures; }
    for (const auto &button : buttons) {
        const float x = button.rect.x + button.rect.width / 2, y = button.rect.y + button.rect.height / 2;
        hud.Pointer(state, x, y, false);
        if (hud.Pointer(state, x, y, true) != button.action) { ++failures; }
        else { ++hits; }
    }
    // Exercise the actual input/draw path: both authored pressed sprites must
    // differ on screen, stay pressed while active, then restore exactly.
    unsigned pressedChecks = 0;
    int width = 0, height = 0;
    window.GetDrawableSize(width, height);
    std::vector<unsigned char> idlePixels(width * height * 4);
    std::vector<unsigned char> pressedPixels(idlePixels.size());
    std::vector<unsigned char> currentPixels(idlePixels.size());
    for (const auto &button : buttons) {
        if (button.action != SurvivalHudAction::OpenShop && button.action != SurvivalHudAction::SwapWeapon) { continue; }
        const float x = button.rect.x + button.rect.width / 2;
        const float y = button.rect.y + button.rect.height / 2;
        hud.Pointer(state, x, y, false);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawOriginalControls(state)) { ++failures; }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, idlePixels.data());
        if (!window.SaveFrame("out/ui-original-2026-09-09/hud-button-idle-" + std::to_string(pressedChecks) + ".png")) { ++failures; }
        if (hud.Pointer(state, x, y, true) != button.action) { ++failures; }
        if (button.action == SurvivalHudAction::OpenShop) { state.shopOpen = true; }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawOriginalControls(state)) { ++failures; }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pressedPixels.data());
        const bool changed = idlePixels != pressedPixels;
        if (!changed) { ++failures; }
        if (!window.SaveFrame("out/ui-original-2026-09-09/hud-button-pressed-" + std::to_string(pressedChecks) + ".png")) { ++failures; }
        if (hud.Pointer(state, x, y, true) != SurvivalHudAction::None) { ++failures; }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawOriginalControls(state)) { ++failures; }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
        if (currentPixels != pressedPixels) { ++failures; }
        if (button.action == SurvivalHudAction::SwapWeapon) {
            // Keyboard state must select the exact same authored pressed art.
            hud.Pointer(state, -1, -1, false);
            state.swapKeyDown = true;
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!hud.DrawOriginalControls(state)) { ++failures; }
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
            if (currentPixels != pressedPixels) { ++failures; }
            state.swapKeyDown = false;
            hud.Pointer(state, x, y, true);
            // Moving off restores the artwork; moving back does not fire a
            // second switch. A paused menu must not leave the HUD held down.
            if (hud.Pointer(state, -1, -1, true) != SurvivalHudAction::None) { ++failures; }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!hud.DrawOriginalControls(state)) { ++failures; }
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
            if (currentPixels != idlePixels) { ++failures; }
            if (hud.Pointer(state, x, y, true) != SurvivalHudAction::None) { ++failures; }
            state.paused = true;
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!hud.DrawOriginalControls(state)) { ++failures; }
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
            if (currentPixels != idlePixels) { ++failures; }
            state.paused = false;
        }
        hud.Pointer(state, x, y, false);
        if (state.shopOpen) {
            // The selector remains active after mouse release, matching
            // CInputPad::Base state 7 rather than a transient hover effect.
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!hud.DrawOriginalControls(state)) { ++failures; }
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
            if (currentPixels != pressedPixels) { ++failures; }
        }
        state.shopOpen = false;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawOriginalControls(state)) { ++failures; }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
        if (currentPixels != idlePixels) { ++failures; }
        std::printf("[hud-button-check] action=%u pressed-pixels-changed=%d restored=%d\n",
            unsigned(button.action), changed, currentPixels == idlePixels);
        ++pressedChecks;
    }
    // Perturb the in-memory parsed Movie: hit geometry must follow its bytes.
    const unsigned base = hud.m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    CMovie *movie = hud.m_movies.GetMovie(base);
    unsigned regionIndex = 0;
    bool mutated = false;
    for (auto &object : movie->objects) {
        if (object.type != 6) { continue; }
        if (regionIndex++ != 2) { continue; }
        const auto original = object;
        for (auto &frame : object.frames) { frame.x += 23; }
        const auto changed = hud.OriginalControlButtons(state);
        for (const auto &button : changed) {
            if (button.action != SurvivalHudAction::OpenShop) { continue; }
            for (const auto &before : buttons) {
                if (before.action == button.action && std::abs(button.rect.x - before.rect.x - 23) < 0.001f) { mutated = true; }
            }
        }
        object = original;
        break;
    }
    if (!mutated) { ++failures; }
    unsigned icons = 0;
    for (const auto &powerup : hud.m_powerups) {
        state.leftPowerup = state.rightPowerup = powerup.resource;
        state.leftCount = 9;
        state.rightCount = 10;
        state.moveX = 1;
        state.aimY = -1;
        if (!hud.DrawOriginalControls(state)) { ++failures; }
        else { ++icons; }
    }
    for (unsigned mode = 0; mode < 3; ++mode) {
        state.horde = mode == 1;
        state.dockedSticks = mode != 2;
        state.kills = 196;
        state.score = 21035;
        state.killStreak = 101;
        state.moveX = 0;
        state.aimY = 0;
        glClearColor(0.04f, 0.07f, 0.09f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawOriginalControls(state) || !window.SaveFrame("out/ui-original-2026-09-09/hud-original-" + std::to_string(mode) + ".png")) { ++failures; }
    }
    // Original notifications: source strings, two level-up Movies, independent
    // perfect title/body and completion on the last actual resource duration.
    hud.ResetNotices();
    state.level = 12;
    state.originalUi = true;
    hud.ObserveProgress(state);
    ++state.level;
    hud.ObserveProgress(state);
    if (hud.NoticeCount() != 2 || hud.HasInterstitial()) { ++failures; }
    unsigned noticeChecks = 0;
    for (unsigned index = 0; index < 2; ++index) {
        if (hud.m_notices.empty()) { ++failures; break; }
        const auto &notice = hud.m_notices.front();
        const unsigned duration = hud.m_movies.GetMovie(notice.movie)->duration;
        if (notice.title.empty() || notice.title.find("%d") != std::string::npos || notice.title.find("%i") != std::string::npos) { ++failures; }
        std::printf("[original-overlay-check] level step=%u movie=%u duration=%u text=%s\n", index, notice.movie, duration, notice.title.c_str());
        hud.Advance(duration / 2);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        hud.DrawNotice();
        if (!window.SaveFrame("out/ui-original-2026-09-09/hud-level-original-" + std::to_string(index) + ".png")) { ++failures; }
        hud.Advance(duration - duration / 2);
        ++noticeChecks;
    }
    for (unsigned mode = 0; mode < 4; ++mode) {
        if (mode == 0) { hud.BeginOriginalLevel(23, false, false); }
        if (mode == 1) { hud.BeginOriginalLevel(2, true, false); }
        if (mode == 2) { hud.BeginOriginalLevel(1, false, true); }
        if (mode == 3) { hud.OnOriginalWaveClear(23, true, 10, false); }
        if (!hud.HasInterstitial() || hud.TakeInterstitialCompletion()) { ++failures; }
        unsigned step = 0;
        while (!hud.m_notices.empty()) {
            const auto &notice = hud.m_notices.front();
            const unsigned duration = hud.m_movies.GetMovie(notice.movie)->duration;
            const bool final = notice.releaseLevel;
            if (notice.title.empty() || notice.title.find("%d") != std::string::npos || notice.footer.find("%d") != std::string::npos ||
                notice.title.find("%i") != std::string::npos || notice.footer.find("%i") != std::string::npos) { ++failures; }
            std::printf("[original-overlay-check] mode=%u movie=%u duration=%u title=%s footer=%s\n", mode, notice.movie, duration, notice.title.c_str(), notice.footer.c_str());
            hud.Advance(duration / 2);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            hud.DrawNotice();
            if (!window.SaveFrame("out/ui-original-2026-09-09/hud-notice-original-" + std::to_string(mode) + "-" + std::to_string(step++) + ".png")) { ++failures; }
            hud.Advance(duration - duration / 2 - 1);
            if (hud.TakeInterstitialCompletion()) { ++failures; }
            hud.Advance(1);
            if (hud.TakeInterstitialCompletion() != final || hud.TakeInterstitialCompletion()) { ++failures; }
            ++noticeChecks;
        }
    }
    hud.OnOriginalWaveClear(1, false, 10, true);
    if (hud.NoticeCount() != 1 || !hud.m_notices.front().title.empty()) { ++failures; }
    if (hud.m_movies.Failures() != 0) { ++failures; }
    std::printf("[original-overlay-check] timelines=%u failures=%u\n", noticeChecks, failures);
    const unsigned errors = glGetError();
    if (errors != 0) { ++failures; }
    std::printf("[original-hud-check] hits=%u alternate-icons=%u region-mutation=%d meter-interpolation=3 gl=%u failures=%u\n", hits, icons, mutated, errors, failures);
    return failures != 0;
}
