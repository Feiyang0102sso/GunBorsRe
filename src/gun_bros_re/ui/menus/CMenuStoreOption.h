#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/ui/content/CStoreAggregator.h"
#include "gun_bros_re/ui/controls/CMenuMovieButton.h"
#include "gun_bros_re/ui/content/CMenuDataProvider.h"

namespace MenuDetail {
class ZMenuSurface;
class CMenuSystem;

/** CMenuStoreOption owns SHOP_BOX focus playback and preview state. */
class CMenuStoreOption {
public:
    /** One card face placed on screen, with the belt's own fade applied. */
    struct Face {
        float x = 0;
        float y = 0;
        float alpha = 1;
        unsigned time = 0;
    };
    /** Shared compact/expanded store status, distinct from the preview slot. */
    enum class Action { None, Purchase, Upgrade, Focus };
    static bool DrawCompact(ZMenuSurface &view, CResTOCManager &toc, CGunBros &tables,
        CProfileManager &profile, const CStoreItem::Entry &item, const std::vector<CGun::Entry> &weapons,
        unsigned slotKind, unsigned shopBox, const Face &face, bool cardEnabled, bool actionEnabled,
        CMenuMovieButton &button, Action &result);
    bool Draw(ZMenuSurface &view, CResTOCManager &toc, CGunBros &tables,
        CProfileManager &profile, const CStoreItem::Entry &item, const std::vector<CGun::Entry> &weapons,
        unsigned slot, const Face &face, CMenuMovieButton &actionButton, Action &action);
    /** Focus/UnFocus :181356/:181402 reverse chapter 1; Update :181486 uses 4x.
     * Keep a closing card modal until its last frame, so a click cannot buy the card
     * underneath it. Bounds are read each time from CMovie, including resource edits. */
    bool Update(std::uint64_t clock, const CMovie &movie);
    CMenuMovieButton previewButton;
    std::uint64_t shopDetailStart = 0;
    std::uint64_t shopDetailLastTick = 0;
    unsigned shopDetailTime = 0;
    bool shopDetailClosing = false;
    float shopFocusAmount = 0;
    // Selecting a card never previews; only the PREVIEW button does.
    bool shopPreview = false;
    bool shopDetailOpen = false;
};

// GLU_MOVIE_SHOP_BOX regions. Chapter 0 is the folded 254x164 card; chapter 2
// expands the same card to 500x328 and reveals the stats, the upgrade meter,
// the description and the action row.
constexpr unsigned kCardBodyRegion = 0;
// Region 1 is the hex plate the OWNED/EQUIPPED stamps lie across; the icon
// itself fits region 5, which is what the original card art measures.
constexpr unsigned kCardStampRegion = 1;
constexpr unsigned kCardCategoryRegion = 2;
constexpr unsigned kCardBadgeRegion = 3;
constexpr unsigned kCardRightRegion = 4;
constexpr unsigned kCardIconRegion = 5;
constexpr unsigned kCardNameRegion = 6;
constexpr unsigned kCardPriceRegion = 7;
constexpr unsigned kCardUpgradeRegion = 8;
constexpr unsigned kCardDescriptionRegion = 9;
constexpr unsigned kCardActionRegion = 10;
constexpr unsigned kCardStatsRegion = 11;
constexpr unsigned kCardFoldedTime = 0;
// CMenuStoreOption::Update :181486 advances SHOP_BOX by 4 * elapsed MS.
constexpr unsigned kCardPlaybackRate = 4;
// Owned and equipped markers, from the sprite character the store loads.
constexpr unsigned kOwnedStamp = 17;
constexpr unsigned kEquippedStamp = 18;
// The two promotional cards in the first column: the invite/loot panel and the
// free Warbucks money pile, both from the same sprite character.
constexpr unsigned kInviteCard = 52;
constexpr unsigned kFreeWarbucksCard = 36;
// Bronze, silver and gold mastery badges for the folded card's corner region.
// The actual folded badge binding is archetype 26, animations 24..26 (:150159).
// CGun::Template::GetMasteryLevel tops out here; a mastered gun cannot upgrade.
constexpr unsigned kMaxMasteryLevel = 3;
// Currency icons come from the sprite character CMenuSystem::Load pulls with
// the menu itself: 23:1 is the coin stack, 23:7 the Warbuck bundle.
constexpr unsigned kCurrencyCharacter = 23;
constexpr unsigned kCoinIcon = 1;
constexpr unsigned kWarbuckIcon = 7;
// MDS_BUTTON_STORE_ITEMS rows: buy, equip and upgrade.
constexpr unsigned kBuyButtonEntry = 0;
constexpr unsigned kEquipButtonEntry = 3;
constexpr unsigned kUpgradeButtonEntry = 4;
// Action dimensions now come from each MDS button movie, region 1.
// Filter bits. Categories keep the low bits so a gun category maps directly.
// GLU_MOVIE_WEAPON_UPGRADE_MASTERY is positioned by its origin and has no
// fitting region, so its authored extent is used to centre it.
// Historical popup dimensions are no longer used by the store child movie.

/** SHOP_BOX regions resolved for a card whose own origin sits at (x, y). */
bool CardRegion(ZMenuSurface &view, unsigned card, unsigned index, const CMenuStoreOption::Face &face, ZMovieRegion &region);

/** Draw one MDS_BUTTON_STORE_ITEMS plate. Its width is the width of the button
 * movie that row names, right aligned on `right`: BUY and EQUIP take the small
 * plate, UPGRADE the large one, which is why UPGRADE reaches further left. */
bool StoreItemButton(ZMenuSurface &view, CMenuMovieButton &button, unsigned entryIndex,
    float right, float y, float height, bool enabled, float alpha = 1);

/** Right aligned price: the original prints the currency sprite and the number,
 * never the currency's name. */
// CreateItemCostString :157573 resolves IDS_SHOP_COMMON/RARE (Omega/delta
// glyph plus %i in this BIG). The currency icons are glyphs in font 0; drawing
// an unrelated sprite at 1.3 times the row height duplicated their layout.
std::string StoreCostText(ZMenuSurface &view, const CStoreItem &item);

void DrawCardPrice(ZMenuSurface &view, const CStoreItem &item, const ZMovieRegion &row, float alpha);

/** Category caption under the icon, from the weapon or armour catalogue. */
// CreateItemCategoryString :157413 indexes IDS_SHOP_SORT3 + STORE.category.
std::string StoreItemKind(ZMenuSurface &view, const CStoreItem::Entry &item);

/** The three cell upgrade meter. CMenuStore::Load pulls sprite character 26
 * for exactly this movie, so the store shares the upgrade popup's artwork. */
// The former popup-meter implementation was replaced by the store child movie below.

/** Store display strings are templates: `^fN` marks a font switch and `#KEY` a
 * value slot. The `^fN` code indexes a font table this rebuild has not resolved
 * yet; against the iOS card, labels use the blue menu font and values the blue
 * digit font. Pieces wrap inside their own card region and each line is centred
 * there, which is what stacks POWER above its number. */
// Correction to the historical approximation above: the table is now resolved.
// CMenuStoreOptionGroup::InitOption :233898 -> SetFont :181499 ->
// SetupTextBox :181194 maps ^f0..4 to menu fonts 1,2,4,3,0. CTextBox::paint
// :104153 centers when byte 0 is set; byte 1, not byte 0, means right alignment.
// Use real newlines, font metrics and whitespace; never invent a row after #KEY.

std::string SubstituteStoreStats(const std::string &text,
    const std::vector<std::pair<std::string, std::string>> &values);

/** CTextBox intersects the animated region with its parent's scissor and restores
 * it after painting (:104019). Keep the belt's clip when painting folded cards. */

void DrawStoreTemplate(ZMenuSurface &view, const std::string &text, const ZMovieRegion &area,
    const std::vector<std::pair<std::string, std::string>> &values, bool centered = false, float layoutWidth = 0);

/** CMenuDataProvider::CreateContentMovie :149246 and CMenuStoreOption::Bind
 * :181883 bind the store-specific eight-region movie, not the upgrade popup.
 * Child time is derived from CGun mastery XP and that movie's chapter lengths. */
bool StoreMasteryTarget(const CMovie &movie, const CGun::Template &weapon, unsigned experience, unsigned &target);

bool DrawMasteryMeter(ZMenuSurface &view, const CGun::Entry &weapon, unsigned experience,
    const ZMovieRegion &area, unsigned elapsed);

/** Powerups use their own child movie; row locations and visibility live in BIG.
 * Bind :182003, GameTypeCallback :180652, GameTypeCompatibilityCallback :180688. */
bool DrawPowerupCompatibility(ZMenuSurface &view, const CStoreItem &item, const ZMovieRegion &area, unsigned elapsed);

/** The current mastery tier's values for the card templates. */
std::vector<std::pair<std::string, std::string>> StoreStatValues(const CStoreItem &item, std::size_t mastery);

/** CornerCallback :180757 and CreateContentSprite :149994 display quantity
 * in a 0:87/88 badge, with the original numeric font; no handwritten OWN label. */
void DrawStoreQuantity(ZMenuSurface &view, const CProfileManager &profile, const GameObjectTypeRef &ref,
    const ZMovieRegion &area, float alpha = 1);

/** Currency entries have no object references and no cost string. The original
 * LevelCallback :180839 therefore places BUY/CONVERT in the bottom right.
 * Focus :181402 requires a cost string, so these cards do not expand. */
bool DrawCurrencyCard(ZMenuSurface &view, CResTOCManager &toc, CGunBros &tables,
    const CStoreItem::Entry &item, unsigned index, unsigned movie, const CMenuStoreOption::Face &face,
    bool enabled, CMenuSystem &state, CProfileManager &profile, const std::filesystem::path &savePath);

}
