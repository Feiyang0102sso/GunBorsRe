/** Check the actual action plate pixels, not only inventory after a click. */
#include "ui/MenuChecks.h"
#include "gun_bros_re/debug/Capture.h"
#include "TestOutput.h"

namespace {
unsigned ActionPlatePixels(ZMenuSurface &view, const ZMovieRegion &area,
    const ZMovieRegion &label, bool expanded) {
    int width = 0, height = 0;
    view.window.GetDrawableSize(width, height);
    // Sample inside the right side of the authored plate, away from the
    // card border and the centered required-level text. Hidden area is black.
    const float x = area.x + area.width - label.width / 4;
    float y = area.y + label.height * 0.4f;
    if (!expanded) { y = area.y + area.height - label.height * 0.6f; }
    const int pixelsWide = std::max(1, int(label.width / 8 * width / kMenuWidth));
    const int pixelsHigh = std::max(1, int(label.height / 5 * height / kMenuHeight));
    std::vector<unsigned char> pixels(pixelsWide * pixelsHigh * 4);
    glReadPixels(int(x * width / kMenuWidth), int((kMenuHeight - y) * height / kMenuHeight) - pixelsHigh,
        pixelsWide, pixelsHigh, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    unsigned colored = 0;
    for (std::size_t index = 0; index < pixels.size(); index += 4) {
        if (pixels[index] > 16 || pixels[index + 1] > 16 || pixels[index + 2] > 16) { ++colored; }
    }
    return colored;
}
}

int RunStoreEquippedCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    std::vector<ZStoreEntry> store;
    std::vector<ZWeaponEntry> weapons;
    std::vector<ZArmorEntry> armor;
    CRefinementManager::Template refinement;
    if (!LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor) || !LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    const ZStoreEntry *gunItem = nullptr, *armorItem = nullptr;
    unsigned armorSlot = 0;
    for (const auto &item : store) {
        if (item.data.objects.size() != 1 || item.data.singlePurchase != 0) { continue; }
        if (item.name == "WTF7000" && item.data.objects[0].type == 6) { gunItem = &item; }
        if (armorItem != nullptr || item.data.objects[0].type != 2) { continue; }
        for (unsigned slot = 2; slot < 5; ++slot) {
            if (CStoreAggregator::MatchesEquipmentSlot(item, slot, weapons, armor)) {
                armorItem = &item;
                armorSlot = slot;
                break;
            }
        }
    }
    if (gunItem == nullptr || armorItem == nullptr) { return 1; }
    const auto *gun = CStoreAggregator::FindWeaponEntry(weapons, gunItem->data.objects[0].object);
    if (gun == nullptr) { return 1; }
    ZMenuSurface view;
    if (!view.Open(toc, tables)) { return 1; }
    const unsigned card = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
    const auto *movie = view.movies.GetMovie(card);
    const auto *equip = CMenuDataProvider::Find("MDS_BUTTON_STORE_ITEMS", kEquipButtonEntry);
    unsigned start = 0, end = 0;
    ZMovieRegion label;
    if (movie == nullptr || equip == nullptr || !movie->GetChapterRange(1, start, end) ||
        !view.movies.Region(view.movies.Ordinal(equip->movies[0]), 1, 0, label)) { return 1; }
    struct Scenario { const char *name; bool armor; int equippedSlot; bool gold; bool visible; };
    const Scenario scenarios[] = {
        {"gold-active", false, 0, true, false},
        {"gold-other-slot", false, 1, true, false},
        {"equipped-armor", true, 0, false, false},
        {"owned-armor", true, -1, false, true},
        {"owned-unequipped", false, -1, true, true},
        {"equipped-upgradeable", false, 0, false, true},
    };
    unsigned failures = 0;
    for (const auto &scenario : scenarios) {
        const ZStoreEntry *item = gunItem;
        unsigned slot = 0;
        if (scenario.armor) { item = armorItem; slot = armorSlot; }
        const auto &object = item->data.objects[0];
        CProfileManager profile;
        profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
        profile.configuration.guns = {};
        profile.configuration.armor = {};
        profile.Grant(object.type, object.object);
        if (scenario.equippedSlot >= 0) {
            if (scenario.armor) { profile.configuration.armor[kArmorSlots[slot]] = object.object; }
            else { profile.configuration.guns[scenario.equippedSlot] = object.object; }
        }
        if (scenario.gold) {
            const unsigned maximum = gun->data.GetMasteryThreshold(2);
            profile.AddWeaponExperience(object.object, maximum, maximum);
        }
        for (bool expanded : {false, true}) {
            CMenuStoreOption option;
            option.shopDetailOpen = true;
            option.shopDetailTime = end;
            CMenuStoreOption::Face face{512, 384, 1, 0};
            if (expanded) { face.time = end; }
            CMenuMovieButton button;
            CMenuStoreOption::Action action;
            view.Begin();
            view.clock = 1000;
            bool drawn = false;
            if (expanded) { drawn = option.Draw(view, toc, tables, profile, *item, weapons, slot, face, button, action); }
            else { drawn = CMenuStoreOption::DrawCompact(view, toc, tables, profile, *item, weapons,
                slot, card, face, true, true, button, action); }
            ZMovieRegion area;
            unsigned region = kCardPriceRegion;
            if (expanded) { region = kCardActionRegion; }
            if (!drawn || !CardRegion(view, card, region, face, area)) { return 1; }
            const unsigned colored = ActionPlatePixels(view, area, label, expanded);
            if ((colored != 0) != scenario.visible) { ++failures; }
            const std::string capture = TestOutput::Path(scenario.name) + "-" + std::to_string(expanded) + ".png";
            if (glGetError() != 0 || !Capture::SaveFrame(view.window, capture)) { return 1; }
            std::printf("[store-equipped-check] case=%s expanded=%u plate-pixels=%u expected-visible=%u failures=%u\n",
                scenario.name, expanded, colored, scenario.visible, failures);
            if (!scenario.armor && scenario.equippedSlot < 0) {
                // A selected EQUIP must disappear and cancel if the loadout
                // changes before its release animation dispatches the action.
                float tapY = area.y + label.height / 2;
                if (!expanded) { tapY = area.y + area.height - label.height / 2; }
                view.InjectTap({area.x + area.width - label.width / 2, tapY});
                if (expanded) { drawn = option.Draw(view, toc, tables, profile, *item, weapons, slot, face, button, action); }
                else { drawn = CMenuStoreOption::DrawCompact(view, toc, tables, profile, *item, weapons,
                    slot, card, face, true, true, button, action); }
                if (!drawn || !button.IsSelected()) { return 1; }
                profile.configuration.guns[0] = object.object;
                view.Begin();
                view.clock += 1000;
                if (expanded) { drawn = option.Draw(view, toc, tables, profile, *item, weapons, slot, face, button, action); }
                else { drawn = CMenuStoreOption::DrawCompact(view, toc, tables, profile, *item, weapons,
                    slot, card, face, true, true, button, action); }
                if (!drawn || button.IsSelected() || action != CMenuStoreOption::Action::None ||
                    ActionPlatePixels(view, area, label, expanded) != 0) { ++failures; }
                std::printf("[store-equipped-check] selected-to-equipped expanded=%u failures=%u\n", expanded, failures);
                profile.configuration.guns[0] = {};
            }
        }
    }
    return failures != 0;
}
