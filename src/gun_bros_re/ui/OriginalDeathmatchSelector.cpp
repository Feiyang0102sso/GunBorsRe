/** CPowerUpSelector DM callbacks :184040, :184169, :185346, :185448.
 * All geometry, warning-stripe animations, labels and card content come from BIG.
 */
#define NOMINMAX
#include "gun_bros_re/ui/SurvivalHud.h"
#include "gun_bros_re/ui/OriginalMenuData.h"
#include "gun_bros_re/ui/OriginalTextLayout.h"
#include "gun_bros_re/ui/MenuInternal.h"
#include "gun_bros_re/data/WeaponCatalog.h"
#include "engine/graphics/CPNG.h"
#include <cmath>

bool SurvivalHud::ConfigureDeathmatch(const std::vector<GameObjectRef> &stores) {
    m_matchGunEntries.clear();
    m_matchGunPosition = 0;
    m_matchGuns = false;
    for (const auto &reference : stores) {
        bool found = false;
        for (unsigned index = 0; index < m_store.size(); ++index) {
            const auto &entry = m_store[index];
            if (entry.ref.packHash != reference.packHash || entry.ref.localIndex != reference.localIndex) { continue; }
            m_matchGunEntries.push_back(index);
            found = true;
            break;
        }
        if (!found) { return false; }
    }
    return !m_matchGunEntries.empty();
}

bool SurvivalHud::DrawMatchGunIcon(const StoreEntry &entry, const MovieRegion &area) {
    const auto &image = entry.data.assets[1];
    const std::uint64_t key = (static_cast<std::uint64_t>(image.packHash) << 32) | image.assetId;
    auto &icon = m_icons[key];
    if (icon == nullptr) {
        std::vector<std::uint8_t> bytes;
        PNGImage png;
        icon = std::make_unique<CTexture>();
        if (!m_tables->ReadSectionResource(image.packHash, GameSection::Png, image.assetId, bytes) ||
            !PNGDecode(bytes, png) || !icon->Create(png)) { return false; }
    }
    // DrawWeaponInfoThumb/DrawWeaponSlot center the original PNG without fitting.
    m_movies.Image(*icon, area.x + (area.width - icon->GetWidth()) / 2,
        area.y + (area.height - icon->GetHeight()) / 2, icon->GetWidth(), icon->GetHeight());
    return true;
}

bool SurvivalHud::DrawMatchGunCard(unsigned index, const MovieRegion &area) {
    const auto &entry = m_store[m_matchGunEntries[index]];
    class Card : public IMovieRegionCallback {
    public:
        Card(SurvivalHud &owner, const StoreEntry &item) : hud(owner), entry(item) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 1) { return hud.DrawMatchGunIcon(entry, region); }
            std::string text = entry.name;
            if (region.index == 3) { text = hud.m_movies.NamedString("IDS_SHOP_SORT3", entry.data.type); }
            if (region.index == 2) {
                // CMPMatch::CreateWeaponLoadOutDescString :396134 uses the
                // compact STORE stat template (mem+100), at mastery zero.
                const auto description = MenuDetail::SubstituteStoreStats(ReadGameString(*hud.m_toc, entry.data.assets[5]), MenuDetail::StoreStatValues(entry.data, 0));
                const auto lines = FormatStoreText(hud.m_movies, description, region.width, {1,1,1,1,0});
                float y = region.y;
                for (const auto &line : lines) {
                    for (const auto &run : line.runs) { hud.m_movies.Text(run.text, region.x + (region.width - line.width) / 2 + run.x, y, run.font, 1, 0, region.alpha); }
                    y += line.height;
                }
                return true;
            }
            return hud.m_movies.Text(text, region.x + (region.width - hud.m_movies.TextWidth(text, 1)) / 2,
                region.y, 1, 1, 0, region.alpha);
        }
        SurvivalHud &hud; const StoreEntry &entry;
    } callback(*this, entry);
    if (!m_movies.Draw(m_movies.Ordinal("GLU_MOVIE_DEATHMATCH_GUN_CARD"), 0, area.x, area.y, 1024, 768, 0, area.alpha, &callback)) { return false; }
    m_selectorHits.push_back({area, SurvivalHudAction::SelectMatchGun, static_cast<int>(m_matchGunEntries[index])});
    return true;
}

void SurvivalHud::AdvanceMatchSelection() {
    m_matchSelectedSlot = 1 - m_matchSelectedSlot;
    m_matchSlotChapter = 6;
    if (m_matchSelectedSlot == 1) { m_matchSlotChapter = 3; }
    m_matchSlotTime = 0;
}

bool SurvivalHud::DrawMatchGuns(const SurvivalHudState &state, const MovieRegion &area) {
    const unsigned layout = m_movies.Ordinal("GLU_MOVIE_GUN_LAYOUT");
    unsigned start = 0, end = 0;
    if (!m_movies.GetMovie(layout)->GetChapterRange(1, start, end)) { return false; }
    const unsigned first = static_cast<unsigned>(m_matchGunPosition);
    unsigned time = start + static_cast<unsigned>((m_matchGunPosition - first) * (end - start + 1));
    if (m_selectorTime < start) { time = m_selectorTime; }
    class Cards : public IMovieRegionCallback {
    public:
        Cards(SurvivalHud &owner, unsigned offset) : hud(owner), first(offset) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 0 || region.index > 4) { return true; }
            const unsigned index = first + region.index - 1;
            if (index >= hud.m_matchGunEntries.size()) { return true; }
            return hud.DrawMatchGunCard(index, region);
        }
        SurvivalHud &hud; unsigned first;
    } cards(*this, first);
    if (!m_movies.Draw(layout, time, area.x + area.width / 2, area.y + area.height / 2, 1024, 768, 0, area.alpha, &cards)) { return false; }
    const unsigned slots = m_movies.Ordinal("GLU_MOVIE_DEATHMATCH_GUN_SLOTS");
    if (!m_movies.GetMovie(slots)->GetChapterRange(m_matchSlotChapter, start, end)) { return false; }
    unsigned slotTime = start + m_matchSlotTime;
    if (slotTime > end && (m_matchSlotChapter == 3 || m_matchSlotChapter == 6)) {
        m_matchSlotChapter = 2;
        if (m_matchSelectedSlot == 1) { m_matchSlotChapter = 4; }
        m_matchSlotTime = 0;
        if (!m_movies.GetMovie(slots)->GetChapterRange(m_matchSlotChapter, start, end)) { return false; }
        slotTime = start;
    } else { slotTime = start + m_matchSlotTime % (end - start + 1); }
    class Slots : public IMovieRegionCallback {
    public:
        Slots(SurvivalHud &owner, const SurvivalHudState &snapshot) : hud(owner), state(snapshot) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index != 0 && region.index != 2) { return true; }
            const unsigned slot = region.index / 2;
            for (unsigned index : hud.m_matchGunEntries) {
                const auto &entry = hud.m_store[index];
                const auto &gun = entry.data.objects.front().object;
                if (gun.packHash == state.guns[slot].packHash && gun.localIndex == state.guns[slot].localIndex) {
                    if (!hud.DrawMatchGunIcon(entry, region)) { return false; }
                    break;
                }
            }
            auto action = SurvivalHudAction::MatchSlot1;
            if (slot == 1) { action = SurvivalHudAction::MatchSlot2; }
            hud.m_selectorHits.push_back({region, action});
            return true;
        }
        SurvivalHud &hud; const SurvivalHudState &state;
    } selected(*this, state);
    return m_movies.Draw(slots, slotTime, 512, 384, 1024, 768, 0, area.alpha, &selected);
}

bool SurvivalHud::DrawMatchTabs(const MovieRegion &area) {
    MovieRegion bounds[2];
    for (unsigned index = 0; index < 2; ++index) {
        const auto *entry = OriginalMenuData("MDS_BUTTON_POWERUP_SELECTOR", index + 4);
        // CMenuMovieButton::Init :144990 stores region1's artwork size at +48.
        if (entry == nullptr || !m_movies.Region(m_movies.Ordinal(entry->movies[0]), 1, 0, bounds[index])) { return false; }
    }
    float x = area.x + (area.width - bounds[0].width - bounds[1].width - 4) / 2;
    for (unsigned index = 0; index < 2; ++index) {
        const auto &entry = *OriginalMenuData("MDS_BUTTON_POWERUP_SELECTOR", index + 4);
        const unsigned movie = m_movies.Ordinal(entry.movies[0]);
        unsigned chapter = 2, start = 0, end = 0;
        if (m_matchGuns == (index == 1)) { chapter = 3; }
        if (!m_movies.GetMovie(movie)->GetChapterRange(chapter, start, end)) { return false; }
        class Label : public IMovieRegionCallback {
        public:
            Label(SurvivalHud &owner, const OriginalMenuEntry &item, bool active) : hud(owner), entry(item), selected(active) {}
            bool DrawMovieRegion(const MovieRegion &region) override {
                if (region.index != 1) { return true; }
                unsigned sprite = entry.sprites[1];
                if (selected) { sprite = entry.sprites[0]; }
                if (!hud.m_movies.DrawSprite(sprite >> 16, sprite & 255, 0, region.x, region.y, 1, region.alpha)) { return false; }
                const auto text = hud.m_movies.NamedString(entry.strings[0]);
                return hud.m_movies.Text(text, region.x + (region.width - hud.m_movies.TextWidth(text, 5)) / 2,
                    region.y + (region.height - hud.m_movies.TextHeight(5)) / 2, 5, 1, 0, region.alpha);
            }
            SurvivalHud &hud; const OriginalMenuEntry &entry; bool selected;
        } label(*this, entry, chapter == 3);
        // DrawModeToggleButtons :185346 packs artwork widths with a native gap of 4.
        const float originX = x - bounds[index].x + 512;
        const float originY = area.y - bounds[index].y + 384;
        if (!m_movies.Draw(movie, start, originX, originY, 1024, 768, 0, area.alpha, &label)) { return false; }
        MovieRegion touch;
        if (!m_movies.Region(movie, 0, start, touch)) { return false; }
        touch.x += originX - 512; touch.y += originY - 384;
        auto action = SurvivalHudAction::ShowPowerups;
        if (index == 1) { action = SurvivalHudAction::ShowGuns; }
        m_selectorHits.push_back({touch, action});
        x += bounds[index].width + 4;
    }
    return true;
}
