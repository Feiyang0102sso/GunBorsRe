/** CMenuFriends / CMenuChallenges content bindings for the local service. */
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "gun_bros_re/ui/host/ZStoreRegionClip.h"

namespace MenuDetail {
namespace {
// Native initialization in CGunBros::Init :80477-80488, not a resource table.
constexpr const auto &kFriendPowers = CFriendPowerManager::Powers;
// CFriendPowerManager::Init :234195. Labels are resolved from the core BIG.
constexpr const char *kPowerNames[] = {
    "IDS_FRIEND_POWER_DAMAGE", "IDS_FRIEND_POWER_ARMOUR", "IDS_FRIEND_POWER_SPEED",
    "IDS_FRIEND_POWER_REFINERY_OUTPUT", "IDS_FRIEND_POWER_DROP_RATE",
    "IDS_FRIEND_POWER_EXPERIENCE", "IDS_FRIEND_POWER_XPLODIUM", "IDS_FRIEND_POWER_DAILY_BONUS"
};

std::string FormatCount(std::string text, unsigned value) {
    auto position = text.find("%d");
    if (position == std::string::npos) { position = text.find("%i"); }
    if (position != std::string::npos) { text.replace(position, 2, std::to_string(value)); }
    return text;
}

bool Chapter(ZMovieRenderer &movies, const char *name, unsigned chapter, unsigned &start, unsigned &end) {
    const auto *movie = movies.GetMovie(movies.Ordinal(name));
    return movie != nullptr && movie->GetChapterRange(chapter, start, end);
}

enum class ZSocialPart { Friend, LocalFriend, Power, Challenge, Details, PowerDetails, Endcap };

class ZSocialContentCallback : public ZMovieRegionCallback {
public:
    ZSocialContentCallback(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile,
        ZSocialPart part, unsigned index = 0) : view(view), state(state), profile(profile), part(part), index(index) {}

    bool Icon(const CGameAssetRef &asset, const ZMovieRegion &region, bool alignRight = false, float *renderedWidth = nullptr) {
        ZStoreEntry icon;
        icon.data.assets[1] = asset;
        return view.Icon(*profile.nativeArchive->toc, *profile.nativeArchive->tables, icon,
            region.x, region.y, region.width, region.height, region.alpha, false, true, alignRight, renderedWidth);
    }

    bool DrawMovieRegion(const ZMovieRegion &region) override {
        auto &social = state.social;
        if (part == ZSocialPart::Friend || part == ZSocialPart::LocalFriend) {
            unsigned friendIndex = social.selectedLocalFriend;
            if (part == ZSocialPart::LocalFriend) { friendIndex = index; }
            ZLocalBotFriend *friendData = nullptr;
            if (friendIndex != 0) {
                friendData = state.botFriend;
                if (state.botRoster != nullptr) { friendData = state.botRoster->At(friendIndex - 1); }
            }
            if (part == ZSocialPart::LocalFriend && region.index == 0 &&
                view.Hit(region.x, region.y, region.width, region.height)) {
                social.selectedLocalFriend = index;
                unsigned focusStart = 0, focusEnd = 0;
                if (!Chapter(view.movies, "GLU_MOVIE_BROTHER_BOX", 1, focusStart, focusEnd)) { return false; }
                if (index < social.friendTimes.size()) { social.friendTimes[index] = focusStart; }
                if (state.botRoster != nullptr) {
                    if (!state.botRoster->Select(index)) { return false; }
                    state.botFriend = friendData;
                } else if (state.botFriend != nullptr && !state.botFriend->Select(index == 1)) { return false; }
            }
            // CMenuFriendOption::Init :198675 binds avatar/name/level to 1/2/3.
            if (region.index == 1) {
                CGameAssetRef avatar = social.avatar;
                if (friendData != nullptr) {
                    auto &toc = *profile.nativeArchive->toc;
                    auto &tables = *profile.nativeArchive->tables;
                    const int core = toc.GetCorePackIndex();
                    const char *name = "IDB_AVATAR_DEFAULT1";
                    if (friendData->profile.playerBrother != 0) { name = "IDB_AVATAR_DEFAULT2"; }
                    const auto handle = toc.GetPack(core)->GetResValue(name);
                    const auto base = tables.GetObjectPack(core).GetHandle(ZGameSection::Png, 0);
                    if (base == 0 || handle < base) { return false; }
                    avatar.assetId = static_cast<int>(handle - base);
                }
                return Icon(avatar, region);
            }
            if (region.index == 2) {
                if (friendData != nullptr) { return Text(friendData->name, region); }
                return Text(social.brotherName, region);
            }
            if (region.index == 3) {
                if (friendData != nullptr) {
                    CPlayerProgress progress;
                    progress.Bind(profile.nativeArchive->progression);
                    progress.SetExperience(profile.experience);
                    progress.SetExperience(friendData->profile.experience);
                    return Text(view.movies.NamedString("IDS_FRIEND_LEVEL") + std::to_string(progress.GetLevel()), region);
                }
                // CFriendData contains CPlayerProgress, whose constructor
                // initializes level 1 (:194437), independently of the player.
                return Text(view.movies.NamedString("IDS_FRIEND_LEVEL") + "1", region);
            }
        }
        if (part == ZSocialPart::Power && region.index == 2) {
            const auto &power = kFriendPowers[index];
            const char *label = "IDS_FRIEND_POWER_FRIEND_COUNT_PLURAL";
            if (power.friends == 1) { label = "IDS_FRIEND_POWER_FRIEND_COUNT_SINGLE"; }
            if (!Text(FormatCount(view.movies.NamedString(label), power.friends), region)) { return false; }
            // DescriptionCallback :234711 aligns the modifier bottom-right.
            const std::string text = view.movies.NamedString(kPowerNames[power.type]) + " +" + std::to_string(power.percent) + "%";
            return view.movies.Text(text, region.x + region.width - view.movies.TextWidth(text),
                region.y + region.height - view.movies.TextHeight(0), 0, 1, 0, region.alpha);
        }
        if (part == ZSocialPart::PowerDetails) {
            if (region.index == 0) { return Text(view.movies.NamedString("IDS_FRIEND_BUFFS_TITLE"), region, 6, true); }
            // SummaryCallback :195625 skips types without a configured bonus.
            unsigned summary = 1;
            for (unsigned type = 0; type < 8; ++type) {
                unsigned maximum = 0;
                for (const auto &power : kFriendPowers) {
                    if (power.type == type) { maximum += power.percent; }
                }
                if (maximum == 0) { continue; }
                if (region.index == summary) {
                    return Text(view.movies.NamedString(kPowerNames[type]) + " +" +
                        std::to_string(CFriendPowerManager::Bonus(profile.friendCount, type)) + "%", region, 0, true);
                }
                ++summary;
            }
            if (region.index >= 8 && region.index <= 14) {
                // CMenuFriends::SG_ANIM_BROBUFF_DETAILS_ARROW = 16 (:18793).
                ZMovieRegion bounds;
                if (!view.movies.SpriteBounds(6, 16, bounds)) { return false; }
                return view.movies.DrawSprite(6, 16, 0, region.x + (region.width - bounds.width) / 2,
                    region.y + (region.height - bounds.height) / 2, 1, region.alpha);
            }
        }
        if (part == ZSocialPart::Endcap && region.index == 2) {
            // MDS_FRIENDS_ENDCAP[0], the original invite banner sprite.
            const auto *entry = CMenuDataProvider::Find("MDS_FRIENDS_ENDCAP", 0);
            if (!entry) { return false; }
            return view.movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255,
                0, region.x, region.y, 1, region.alpha);
        }
        return true;
    }

private:
    bool Text(const std::string &text, const ZMovieRegion &region, unsigned font = 0, bool centered = false) {
        float x = region.x;
        float y = region.y;
        if (centered) {
            x += (region.width - view.movies.TextWidth(text, font)) / 2;
            y += (region.height - view.movies.TextHeight(font)) / 2;
        }
        return view.movies.Text(text, x, y, font, 1, 0, region.alpha);
    }
    ZMenuSurface &view;
    CMenuSystem &state;
    const CProfileManager &profile;
    ZSocialPart part;
    unsigned index;
};

bool DrawPart(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile,
    const char *name, ZSocialPart part, const ZMovieRegion &region, unsigned index = 0) {
    unsigned start = 0, end = 0;
    unsigned chapter = 0;
    if (part == ZSocialPart::Power && profile.friendCount >= kFriendPowers[index].friends) { chapter = 2; }
    if (part == ZSocialPart::Details || part == ZSocialPart::PowerDetails) { chapter = 1; }
    if (part == ZSocialPart::Friend) { chapter = 1; }
    if (!Chapter(view.movies, name, chapter, start, end)) {
        std::printf("[social-content] missing chapter movie=%s\n", name);
        return false;
    }
    if (part == ZSocialPart::Challenge || part == ZSocialPart::Details) {
        return state.challenges.DrawOption(view, state, profile, region, index, part == ZSocialPart::Details);
    }
    ZSocialContentCallback callback(view, state, profile, part, index);
    unsigned time = end;
    if (part == ZSocialPart::Power && end > start) { time = start + static_cast<unsigned>(view.clock % (end - start + 1)); }
    if (part == ZSocialPart::Challenge) { time = state.challenges.optionTimes[index]; }
    if (part == ZSocialPart::LocalFriend) { time = state.social.friendTimes[index]; }
    if (part == ZSocialPart::Friend) { time = start + static_cast<unsigned>(view.clock % (end - start + 1)); }
    if (part == ZSocialPart::Details) { time = state.challenges.sidebarTime; }
    const bool result = view.movies.Draw(view.movies.Ordinal(name), time, region.x, region.y,
        kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
    if (!result) { std::printf("[social-content] draw failed movie=%s index=%u\n", name, index); }
    return result;
}

class ZSocialListCallback : public ZMovieRegionCallback {
public:
    ZSocialListCallback(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile, unsigned first) :
        view(view), state(state), profile(profile), first(first) {}
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        // CMenuMovieControl region0 is the touch viewport, not an option.
        if (region.index == 0) { return true; }
        const unsigned index = first + region.index - 1;
        if (state.stack.page == 5) {
            if (index >= state.challenges.manager.current.size()) { return true; }
            if (!DrawPart(view, state, profile, "GLU_MOVIE_BRO_OP_BOX", ZSocialPart::Challenge, region, index)) { return false; }
        } else if (state.social.socialTab != 1) {
            unsigned count = 2;
            if (state.botRoster != nullptr) { count = 1 + state.botRoster->Count(); }
            if (index >= count) { return true; }
            if (!DrawPart(view, state, profile, "GLU_MOVIE_BROTHER_BOX", ZSocialPart::LocalFriend, region, index)) { return false; }
        } else {
            if (index >= std::size(kFriendPowers)) { return true; }
            if (!DrawPart(view, state, profile, "GLU_MOVIE_BROBUFF_BOX", ZSocialPart::Power, region, index)) { return false; }
        }
        ++state.social.renderedEntries;
        return true;
    }
    ZMenuSurface &view;
    CMenuSystem &state;
    const CProfileManager &profile;
    unsigned first;
};
} // namespace

bool CMenuFriends::BindContent(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile) {
    auto &social = state.social;
    if (!profile.nativeArchive || !profile.nativeArchive->toc || !profile.nativeArchive->tables) { return false; }
    auto &toc = *profile.nativeArchive->toc;
    auto &tables = *profile.nativeArchive->tables;
    if (!social.contentBound) {
        if (!LoadWeaponCatalog(toc, tables, social.weapons) || !LoadArmorCatalog(toc, tables, social.armors) ||
            !state.challenges.manager.Bind(toc, tables, profile, static_cast<unsigned>(CurrentSeconds()))) { return false; }
        // CGameFlow::Reset :77330 chooses the opposite brother. Configuration
        // Reset :171865 supplies core gun0 + pack5 gun4, and core armour 2/1/0.
        const auto core = toc.GetCorePackIndex();
        const auto pack5 = toc.GetPackIndexFromName("pack5");
        if (core < 0 || pack5 < 0) { return false; }
        social.defaultBrother.configuration.SetDefaults(toc.GetPack(core)->GetPackHash());
        social.defaultBrother.configuration.guns[1].packHash = toc.GetPack(pack5)->GetPackHash();
        social.defaultBrother.configuration.guns[1].localIndex = 4;
        social.contentBound = true;
    }
    social.defaultBrother.playerBrother = 1 - profile.playerBrother;
    const char *name = "IDS_FRIEND_DEFAULT_BRO1";
    const char *avatar = "IDB_AVATAR_DEFAULT1";
    if (social.defaultBrother.playerBrother != 0) { name = "IDS_FRIEND_DEFAULT_BRO2"; avatar = "IDB_AVATAR_DEFAULT2"; }
    social.brotherName = view.movies.NamedString(name);
    const auto core = toc.GetCorePackIndex();
    const auto handle = toc.GetPack(core)->GetResValue(avatar);
    const auto base = tables.GetObjectPack(core).GetHandle(ZGameSection::Png, 0);
    if (handle < base || base == 0 || social.brotherName.empty()) { return false; }
    social.avatar.packHash = toc.GetPack(core)->GetPackHash();
    social.avatar.assetId = static_cast<int>(handle - base);
    return true;
}

bool CMenuFriends::DrawContent(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile, const ZMovieRegion &region) {
    auto &social = state.social;
    if (state.stack.page == 4) {
        if (region.index == 0 && social.socialTab == 1) {
            return view.movies.Text(FormatCount(view.movies.NamedString("IDS_FRIEND_POWER_FRIEND_COUNT_PLURAL"), profile.friendCount), region.x, region.y);
        }
        if (region.index == 2 && social.socialTab != 1) {
            if (!DrawPart(view, state, profile, "GLU_MOVIE_BROTHER_BOX", ZSocialPart::Friend, region)) { return false; }
            ++social.renderedEntries;
        }
        if (region.index == 2 && social.socialTab == 1) {
            // ActiveFriendCallback :195740; local SG_ANIM_BUFFS banner.
            return view.movies.DrawSprite(6, 9, 0, region.x, region.y, 1, region.alpha);
        }
        if (region.index == 6 && social.socialTab != 1) {
            return DrawPart(view, state, profile, "GLU_MOVIE_BROTHER_BOX_END", ZSocialPart::Endcap, region);
        }
    }
    if (state.stack.page == 5 && region.index == 3 && social.socialTab == 0) {
        return state.challenges.DrawDetails(view, state, profile, region);
    }
    if (region.index != 4) { return true; }
    if (state.stack.page == 5 && social.socialTab != 0) { return true; }
    const char *listName = "GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION";
    unsigned count = static_cast<unsigned>(std::size(kFriendPowers));
    unsigned visible = 3;
    if (state.stack.page == 4 && social.socialTab != 1) {
        // MENU_FRIENDS 0x402D70 selects the three-option Movie for all tabs;
        // SetBoundsOptions ends at count-3, not count-4.
        count = 2; // Original default plus the explicitly simulated local peer.
        if (state.botRoster != nullptr) { count = 1 + state.botRoster->Count(); }
        unsigned idleStart = 0, idleEnd = 0, focusStart = 0, focusEnd = 0;
        if (!Chapter(view.movies, "GLU_MOVIE_BROTHER_BOX", 0, idleStart, idleEnd) ||
            !Chapter(view.movies, "GLU_MOVIE_BROTHER_BOX", 1, focusStart, focusEnd)) { return false; }
        if (social.friendTimes.size() != count) {
            social.friendTimes.assign(count, idleEnd);
            social.friendTimes[social.selectedLocalFriend] = focusStart;
        }
        // CMenuFriendOption::Focus :198360 loops chapter1; UnFocus :198345
        // reverses the same authored highlight and then stops.
        for (unsigned index = 0; index < count; ++index) {
            auto &time = social.friendTimes[index];
            if (index == social.selectedLocalFriend) {
                if (time < focusStart) { time = focusStart; }
                time = focusStart + (time - focusStart + social.contentElapsed) % (focusEnd - focusStart + 1);
            } else if (time >= focusStart) {
                if (social.contentElapsed >= time - focusStart) { time = idleEnd; }
                else { time -= social.contentElapsed; }
            }
        }
    }
    if (state.stack.page == 5) {
        listName = "GLU_MOVIE_BROTHER_MENU_SCROLL";
        count = static_cast<unsigned>(state.challenges.manager.current.size());
        visible = 4;
        if (!state.challenges.UpdateOptions(view.movies, social.contentElapsed)) { return false; }
    }
    unsigned start = 0, end = 0, next = 0, nextEnd = 0;
    if (!Chapter(view.movies, listName, 1, start, end) || !Chapter(view.movies, listName, 2, next, nextEnd)) { return false; }
    const unsigned ordinal = view.movies.Ordinal(listName);
    const auto slots = view.movies.Regions(ordinal, start, region.x, region.y);
    if (slots.size() <= visible) { return false; }
    const float stride = slots[2].y - slots[1].y;
    if (stride <= 0 || next <= start) { return false; }
    ZMovieRegion viewport = region;
    // ListCallback :235474 clips at the list's top and retains the screen bottom.
    viewport.height = kMenuHeight - viewport.y;
    const bool inside = view.MouseIn(viewport.x, viewport.y, viewport.width, viewport.height);
    float wheel = 0;
    if (inside) { wheel = view.window.TakeWheelDelta(); }
    unsigned maximum = 0;
    if (count > visible) { maximum = count - visible; }
    social.scrollMotion.Update(social.scrollPosition, view.clock, view.dragY, wheel, view.pointerHeld,
        inside && view.pointerPressed, view.inputEnabled, maximum * stride, stride, next - start, state.stack.page == 5);
    const float position = std::clamp(social.scrollPosition, 0.0f, maximum * stride);
    const float extension = social.scrollPosition - position;
    const unsigned first = static_cast<unsigned>(position / stride);
    const unsigned time = start + static_cast<unsigned>((position / stride - first) * (next - start));
    ZSocialListCallback callback(view, state, profile, first);
    const bool click = view.ExchangeClick(false);
    // Scissoring affects pixels only; never let clipped cards consume a pointer.
    if (inside && !view.pointerHeld) { view.ExchangeClick(click); }
    view.Clip(viewport.x, viewport.y, viewport.width, viewport.height);
    const bool result = view.movies.Draw(ordinal, time, region.x, region.y - extension,
        kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
    view.EndClip();
    if (!inside || view.pointerHeld) { view.ExchangeClick(click); }
    if (result && maximum != 0) {
        // FriendListCallback :195808 places the original vertical scrollbar
        // at the right edge; its timeline represents option progress.
        const auto *entry = CMenuDataProvider::Find("MDS_SCROLLBARS", 1);
        if (!entry) { return false; }
        const unsigned bar = view.movies.Ordinal(entry->movies[0]);
        const auto *movie = view.movies.GetMovie(bar);
        ZMovieRegion bounds;
        if (!movie || !view.movies.Region(bar, 0, 0, bounds)) { return false; }
        const unsigned barTime = static_cast<unsigned>(movie->duration * position / (maximum * stride));
        return view.movies.Draw(bar, barTime, region.x + region.width,
            region.y + region.height / 2 - bounds.height / 2);
    }
    return result;
}

bool CMenuFriends::DrawModel(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile) {
    if (state.stack.page != 4) { return true; }
    ZMovieRegion region;
    if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU"), 3, state.social.socialTime, region)) { return false; }
    ZMovieRegion model = region;
    if (state.social.socialTab == 1) { model.height *= 0.5f; }
        const CProfileManager *brother = &state.social.defaultBrother;
        if (state.social.selectedLocalFriend != 0 && state.botFriend != nullptr) { brother = &state.botFriend->profile; }
        if (!view.playerPreview.Draw(view, *profile.nativeArchive->toc, *profile.nativeArchive->tables,
            *brother, state.social.weapons, state.social.armors, 0, nullptr, &model)) { return false; }
    if (state.social.socialTab == 1) {
        return DrawPart(view, state, profile, "GLU_MOVIE_BRO_BUFFS_DETAILS", ZSocialPart::PowerDetails, region);
    }
    // CMenuFriends::Draw :196440 anchors the duty stamp at the model's feet.
    unsigned start = 0, end = 0;
    if (!Chapter(view.movies, "GLU_MOVIE_ON_DUTY_STAMP", 0, start, end)) { return false; }
    return view.movies.Draw(view.movies.Ordinal("GLU_MOVIE_ON_DUTY_STAMP"), end,
        region.x + region.width / 2, region.y + region.height);
}
} // namespace MenuDetail
