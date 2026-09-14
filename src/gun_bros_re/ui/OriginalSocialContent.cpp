/** CMenuFriends / CMenuChallenges content bindings for the local service. */
#include "gun_bros_re/ui/MenuInternal.h"
#include "gun_bros_re/ui/StoreRegionClip.h"

namespace MenuDetail {
namespace {
struct FriendPower { unsigned friends, type, percent; };
// Native initialization in CGunBros::Init :80477-80488, not a resource table.
constexpr FriendPower kFriendPowers[] = {
    {1, 7, 10}, {2, 2, 5}, {3, 5, 10}, {4, 1, 10}, {5, 3, 10},
    {6, 7, 10}, {7, 2, 5}, {8, 0, 10}, {9, 6, 10}, {10, 1, 5}
};
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

bool Chapter(MovieRenderer &movies, const char *name, unsigned chapter, unsigned &start, unsigned &end) {
    const auto *movie = movies.GetMovie(movies.Ordinal(name));
    return movie != nullptr && movie->GetChapterRange(chapter, start, end);
}

std::string RewardQuantity(const DailyPrize &prize) {
    // CChallengeManager::CreateRewardQuantityString :240630.
    if (prize.warbucks != 0) { return "X" + std::to_string(prize.warbucks); }
    if (prize.coins != 0) { return "X" + std::to_string(prize.coins); }
    if (prize.experience != 0) { return std::to_string(prize.experience); }
    return {};
}

enum class SocialPart { Friend, Power, Challenge, Details, PowerDetails, Endcap };

class SocialContentCallback : public IMovieRegionCallback {
public:
    SocialContentCallback(GameMenu &view, MenuState &state, const CProfileManager &profile,
        SocialPart part, unsigned index = 0) : view(view), state(state), profile(profile), part(part), index(index) {}

    bool Icon(const CGameAssetRef &asset, const MovieRegion &region, bool alignRight = false, float *renderedWidth = nullptr) {
        StoreEntry icon;
        icon.data.assets[1] = asset;
        return view.Icon(*profile.nativeArchive->toc, *profile.nativeArchive->tables, icon,
            region.x, region.y, region.width, region.height, region.alpha, false, true, alignRight, renderedWidth);
    }

    bool ChallengeSprite(unsigned entryIndex, bool active, float x, float y, float alpha) {
        // Provider177 is the original MDS_ICON_CHALLENGES table, not a new icon map.
        const auto *entry = OriginalMenuData("MDS_ICON_CHALLENGES", entryIndex);
        if (!entry) { return false; }
        unsigned variant = 0;
        if (active) { variant = 1; }
        const unsigned sprite = entry->sprites[variant];
        return view.movies.DrawSprite(sprite >> 16, sprite & 255, 0, x, y, 1, alpha);
    }

    bool ChallengeSpriteBounds(unsigned entryIndex, MovieRegion &bounds) {
        const auto *entry = OriginalMenuData("MDS_ICON_CHALLENGES", entryIndex);
        if (!entry) { return false; }
        const unsigned sprite = entry->sprites[0];
        return view.movies.SpriteBounds(sprite >> 16, sprite & 255, bounds);
    }

    bool DrawMovieRegion(const MovieRegion &region) override {
        auto &social = state.social;
        if (part == SocialPart::Friend) {
            // CMenuFriendOption::Init :198675 binds avatar/name/level to 1/2/3.
            if (region.index == 1) { return Icon(social.avatar, region); }
            if (region.index == 2) { return Text(social.brotherName, region); }
            if (region.index == 3) {
                // CFriendData contains CPlayerProgress, whose constructor
                // initializes level 1 (:194437), independently of the player.
                return Text(view.movies.NamedString("IDS_FRIEND_LEVEL") + "1", region);
            }
        }
        if (part == SocialPart::Power && region.index == 2) {
            const auto &power = kFriendPowers[index];
            const char *label = "IDS_FRIEND_POWER_FRIEND_COUNT_PLURAL";
            if (power.friends == 1) { label = "IDS_FRIEND_POWER_FRIEND_COUNT_SINGLE"; }
            if (!Text(FormatCount(view.movies.NamedString(label), power.friends), region)) { return false; }
            // DescriptionCallback :234711 aligns the modifier bottom-right.
            const std::string text = view.movies.NamedString(kPowerNames[power.type]) + " +" + std::to_string(power.percent) + "%";
            return view.movies.Text(text, region.x + region.width - view.movies.TextWidth(text),
                region.y + region.height - view.movies.TextHeight(0), 0, 1, 0, region.alpha);
        }
        if (part == SocialPart::PowerDetails) {
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
                    return Text(view.movies.NamedString(kPowerNames[type]) + " +0%", region, 0, true);
                }
                ++summary;
            }
            if (region.index >= 8 && region.index <= 14) {
                // CMenuFriends::SG_ANIM_BROBUFF_DETAILS_ARROW = 16 (:18793).
                MovieRegion bounds;
                if (!view.movies.SpriteBounds(6, 16, bounds)) { return false; }
                return view.movies.DrawSprite(6, 16, 0, region.x + (region.width - bounds.width) / 2,
                    region.y + (region.height - bounds.height) / 2, 1, region.alpha);
            }
        }
        if (part == SocialPart::Endcap && region.index == 2) {
            // MDS_FRIENDS_ENDCAP[0], the original invite banner sprite.
            const auto *entry = OriginalMenuData("MDS_FRIENDS_ENDCAP", 0);
            if (!entry) { return false; }
            return view.movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255,
                0, region.x, region.y, 1, region.alpha);
        }
        if (part != SocialPart::Challenge && part != SocialPart::Details) { return true; }
        const auto &challenge = social.challenges.current[index];
        const auto &definition = social.challenges.templates[challenge.templateIndex];
        if (part == SocialPart::Challenge) {
            if (region.index == 0 && view.Hit(region.x, region.y, region.width, region.height)) {
                social.selectedChallenge = index;
                unsigned start = 0, end = 0;
                if (!Chapter(view.movies, "GLU_MOVIE_BRO_OP_BOX", 1, start, end)) { return false; }
                social.challengeTimes[index] = start;
            }
            if (region.index == 1) { return Text(challenge.name, region); }
            if (region.index == 2) {
                // CMenuChallengeOption::ProgressCallback and BRO_OP_METER.
                // Provider54 :149209 returns both Movie layers. They have a
                // continuous timeline, no chapters; the blue layer owns the base.
                for (const char *name : {"GLU_MOVIE_BRO_OP_METER_BLUE", "GLU_MOVIE_BRO_OP_METER"}) {
                    const unsigned ordinal = view.movies.Ordinal(name);
                    const auto *meter = view.movies.GetMovie(ordinal);
                    if (!meter) { return false; }
                    const unsigned time = meter->duration * std::min(100u, challenge.progress) / 100;
                    if (!view.movies.Draw(ordinal, time, region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha)) { return false; }
                }
                std::string text = std::to_string(challenge.achieved) + "/" + std::to_string(challenge.target);
                MovieRegion meterBounds;
                if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BRO_OP_METER_BLUE"), 0, 0, meterBounds)) { return false; }
                float x = region.x + region.width - view.movies.TextWidth(text, 1);
                // Bind :237977 requests units when the meter fills the region.
                if (meterBounds.width >= region.width) {
                    text += view.movies.NamedString("IDS_CHALLENGES_PROGRESS_KILLS", challenge.progressLabel);
                    x = region.x + 1;
                }
                return view.movies.Text(text, x,
                    region.y, 1, 1, 0, region.alpha);
            }
            if (region.index == 3) {
                // PrizeIconCallback :237431 divides this region into 3 cells.
                MovieRegion cell = region;
                cell.width /= 3;
                MovieRegion check;
                if (!ChallengeSpriteBounds(1, check)) { return false; }
                for (unsigned tier = 0; tier < 3; ++tier) {
                    if (!Icon(challenge.prizes[tier].image, cell)) { return false; }
                    const bool available = challenge.progress == 100 &&
                        definition.participationRequired[tier] <= challenge.completedFriends;
                    // PrizeIconCallback :237488 / :237525: lower-right marker,
                    // inset by half its authored width inside each of the three cells.
                    if (!ChallengeSprite(1, available, cell.x + cell.width - check.width - check.width / 2,
                        cell.y + cell.height - check.height, cell.alpha)) { return false; }
                    cell.x += cell.width;
                }
            }
            return true;
        }
        // CMenuChallenges::Init :237100 binds the original sidebar Movie.
        if (region.index == 0) {
            const auto title = view.movies.NamedString("IDS_CHALLENGES_SIDEBAR_TITLE");
            return view.movies.Text(title, region.x + (region.width - view.movies.TextWidth(title, 6)) / 2,
                region.y, 6, 1, 0, region.alpha);
        }
        if (region.index == 6) {
            // ChallengeTitleCallback :235054 uses font0, sidebar heading font6.
            return view.movies.Text(challenge.name, region.x + (region.width - view.movies.TextWidth(challenge.name)) / 2,
                region.y, 0, 1, 0, region.alpha);
        }
        if (region.index == 1) {
            // BindSideBarContent :235793: font slot0=font1, slot1=font0;
            // CTextBox centers the formatted description in the original sidebar.
            const auto lines = FormatStoreText(view.movies, challenge.description, region.width, {1, 0, 1, 1, 1});
            StoreRegionClip clip(view, region);
            float y = region.y;
            for (const auto &line : lines) {
                const float x = region.x + (region.width - line.width) / 2;
                for (const auto &run : line.runs) {
                    view.movies.Text(run.text, x + run.x, y + (line.height - run.height) / 2,
                        run.font, 1, 0, region.alpha);
                }
                y += line.height;
            }
        }
        if (region.index == 2) {
            const auto *button = OriginalMenuData("MDS_BUTTON_CHALLENGE_INVITE_BRO", 0);
            if (!button) { return false; }
            bool pressed = false;
            if (!DrawOriginalMovieButton(view, *button, region, view.movies.NamedString(button->strings[0]),
                5, true, pressed)) { return false; }
            if (pressed) { social.socialTab = 1; social.scrollPosition = 0; }
        }
        if (region.index >= 3 && region.index <= 5) {
            const unsigned tier = region.index - 3;
            const auto &prize = challenge.prizes[tier];
            MovieRegion icon = region;
            icon.x += region.width - region.height;
            icon.width = region.height;
            float imageWidth = 0;
            if (!Icon(prize.image, icon, true, &imageWidth)) { return false; }
            // CreateRewardTierStatusString :240515 supplies the challenge
            // status; CPrize::name can instead be a friend-notification format.
            unsigned remaining = 0;
            if (definition.participationRequired[tier] > challenge.completedFriends) {
                remaining = definition.participationRequired[tier] - challenge.completedFriends;
            }
            std::string status;
            if (challenge.progress == 100 && remaining == 0) { status = view.movies.NamedString("IDS_CHALLENGES_REWARD_COLLECTED"); }
            else if (tier == 0) { status = view.movies.NamedString("IDS_CHALLENGES_REWARD_TIER1"); }
            else if (remaining == 0) { status = view.movies.NamedString("IDS_CHALLENGES_REWARD_TIER_COMPLETED"); }
            else { status = FormatCount(view.movies.NamedString("IDS_CHALLENGES_REWARD_TIER2"), remaining); }
            const float statusWidth = view.movies.TextWidth(status, 1);
            float statusX = region.x + (region.width - statusWidth) / 2;
            if (statusWidth > region.width - 2 * imageWidth) {
                statusX = region.x + region.width - imageWidth - statusWidth;
            }
            if (!view.movies.Text(status, statusX, region.y, 1, 1, 0, region.alpha)) { return false; }
            MovieRegion check, person;
            if (!ChallengeSpriteBounds(1, check) || !ChallengeSpriteBounds(0, person)) { return false; }
            const bool available = challenge.progress == 100 && remaining == 0;
            if (!ChallengeSprite(1, available, region.x, region.y + (region.height - check.height) / 2,
                region.alpha)) { return false; }
            float personX = region.x + 4 * check.width;
            for (unsigned friendIndex = 0; friendIndex < definition.participationRequired[tier]; ++friendIndex) {
                // RewardCallback uses the filled person for completed recruits,
                // and the second (outline) sprite for recruits still required.
                if (!ChallengeSprite(0, friendIndex >= challenge.completedFriends, personX,
                    region.y + person.height, region.alpha)) { return false; }
                personX += person.width;
            }
            if (tier == 0 && !Text(view.movies.NamedString("IDS_CHALLENGES_REWARD_SOLO"), region, 0, true)) { return false; }
            const auto quantity = RewardQuantity(prize);
            return view.movies.Text(quantity, region.x + region.width - view.movies.TextWidth(quantity),
                region.y + region.height - view.movies.TextHeight(0), 0, 1, 0, region.alpha);
        }
        return true;
    }

private:
    bool Text(const std::string &text, const MovieRegion &region, unsigned font = 0, bool centered = false) {
        float x = region.x;
        float y = region.y;
        if (centered) {
            x += (region.width - view.movies.TextWidth(text, font)) / 2;
            y += (region.height - view.movies.TextHeight(font)) / 2;
        }
        return view.movies.Text(text, x, y, font, 1, 0, region.alpha);
    }
    GameMenu &view;
    MenuState &state;
    const CProfileManager &profile;
    SocialPart part;
    unsigned index;
};

bool DrawPart(GameMenu &view, MenuState &state, const CProfileManager &profile,
    const char *name, SocialPart part, const MovieRegion &region, unsigned index = 0) {
    unsigned start = 0, end = 0;
    unsigned chapter = 0;
    if (part == SocialPart::Details || part == SocialPart::PowerDetails) { chapter = 1; }
    if (!Chapter(view.movies, name, chapter, start, end)) {
        std::printf("[social-content] missing chapter movie=%s\n", name);
        return false;
    }
    SocialContentCallback callback(view, state, profile, part, index);
    unsigned time = end;
    if (part == SocialPart::Challenge) { time = state.social.challengeTimes[index]; }
    if (part == SocialPart::Details) { time = state.social.sidebarTime; }
    const bool result = view.movies.Draw(view.movies.Ordinal(name), time, region.x, region.y,
        kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
    if (!result) { std::printf("[social-content] draw failed movie=%s index=%u\n", name, index); }
    return result;
}

class SocialListCallback : public IMovieRegionCallback {
public:
    SocialListCallback(GameMenu &view, MenuState &state, const CProfileManager &profile, unsigned first) :
        view(view), state(state), profile(profile), first(first) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        // CMenuMovieControl region0 is the touch viewport, not an option.
        if (region.index == 0) { return true; }
        const unsigned index = first + region.index - 1;
        if (state.page == 5) {
            if (index >= state.social.challenges.current.size()) { return true; }
            if (!DrawPart(view, state, profile, "GLU_MOVIE_BRO_OP_BOX", SocialPart::Challenge, region, index)) { return false; }
        } else {
            if (index >= std::size(kFriendPowers)) { return true; }
            if (!DrawPart(view, state, profile, "GLU_MOVIE_BROBUFF_BOX", SocialPart::Power, region, index)) { return false; }
        }
        ++state.social.renderedEntries;
        return true;
    }
    GameMenu &view;
    MenuState &state;
    const CProfileManager &profile;
    unsigned first;
};
} // namespace

bool BindOriginalSocialContent(GameMenu &view, MenuState &state, const CProfileManager &profile) {
    auto &social = state.social;
    if (!profile.nativeArchive || !profile.nativeArchive->toc || !profile.nativeArchive->tables) { return false; }
    auto &toc = *profile.nativeArchive->toc;
    auto &tables = *profile.nativeArchive->tables;
    if (!social.contentBound) {
        if (!LoadWeaponCatalog(toc, tables, social.weapons) || !LoadArmorCatalog(toc, tables, social.armors) ||
            !social.challenges.Bind(toc, tables, profile, static_cast<unsigned>(CurrentSeconds()))) { return false; }
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
    const auto base = tables.GetObjectPack(core).GetHandle(GameSection::Png, 0);
    if (handle < base || base == 0 || social.brotherName.empty()) { return false; }
    social.avatar.packHash = toc.GetPack(core)->GetPackHash();
    social.avatar.assetId = static_cast<int>(handle - base);
    return true;
}

bool DrawOriginalSocialContent(GameMenu &view, MenuState &state, const CProfileManager &profile, const MovieRegion &region) {
    auto &social = state.social;
    if (state.page == 4) {
        if (region.index == 0 && social.socialTab == 1) {
            return view.movies.Text(FormatCount(view.movies.NamedString("IDS_FRIEND_POWER_FRIEND_COUNT_PLURAL"), 0), region.x, region.y);
        }
        if (region.index == 2 && social.socialTab != 1) {
            if (!DrawPart(view, state, profile, "GLU_MOVIE_BROTHER_BOX", SocialPart::Friend, region)) { return false; }
            ++social.renderedEntries;
        }
        if (region.index == 2 && social.socialTab == 1) {
            // ActiveFriendCallback :195740; local SG_ANIM_BUFFS banner.
            return view.movies.DrawSprite(6, 9, 0, region.x, region.y, 1, region.alpha);
        }
        if (region.index == 6 && social.socialTab != 1) {
            return DrawPart(view, state, profile, "GLU_MOVIE_BROTHER_BOX_END", SocialPart::Endcap, region);
        }
    }
    if (state.page == 5 && region.index == 3 && social.socialTab == 0) {
        unsigned start = 0, end = 0;
        if (!Chapter(view.movies, "GLU_MOVIE_BRO_OPS_DETAILS", 1, start, end)) { return false; }
        if (!social.sidebarBound) {
            social.sidebarBound = true;
            social.sidebarChallenge = social.selectedChallenge;
            social.sidebarTime = start;
            if (!view.animateNavigation) { social.sidebarTime = end; }
            social.sidebarReverse = false;
        }
        // CMenuChallenges::Refresh action106 :236507 reverses the old sidebar.
        // Update :236264 uses 2*dt and rebinds only when it reaches the hidden pose.
        if (social.sidebarChallenge != social.selectedChallenge) { social.sidebarReverse = true; }
        unsigned elapsed = 2 * social.contentElapsed;
        if (social.sidebarReverse) {
            const unsigned remaining = social.sidebarTime - start;
            if (elapsed < remaining) {
                social.sidebarTime -= elapsed;
                elapsed = 0;
            } else {
                elapsed -= remaining;
                social.sidebarTime = start;
                social.sidebarChallenge = social.selectedChallenge;
                social.sidebarReverse = false;
            }
        }
        if (!social.sidebarReverse) { social.sidebarTime = std::min(end, social.sidebarTime + elapsed); }
        return DrawPart(view, state, profile, "GLU_MOVIE_BRO_OPS_DETAILS", SocialPart::Details,
            region, social.sidebarChallenge);
    }
    if (region.index != 4) { return true; }
    if (state.page == 4 && social.socialTab != 1) { return true; }
    if (state.page == 5 && social.socialTab != 0) { return true; }
    const char *listName = "GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION";
    unsigned count = static_cast<unsigned>(std::size(kFriendPowers));
    unsigned visible = 3;
    if (state.page == 5) {
        listName = "GLU_MOVIE_BROTHER_MENU_SCROLL";
        count = static_cast<unsigned>(social.challenges.current.size());
        visible = 4;
        unsigned idleStart = 0, idleEnd = 0, focusStart = 0, focusEnd = 0;
        if (!Chapter(view.movies, "GLU_MOVIE_BRO_OP_BOX", 0, idleStart, idleEnd) ||
            !Chapter(view.movies, "GLU_MOVIE_BRO_OP_BOX", 1, focusStart, focusEnd)) { return false; }
        if (social.challengeTimes.size() != count) {
            social.challengeTimes.assign(count, idleEnd);
            social.challengeTimes[social.selectedChallenge] = focusStart;
        }
        // Focus loops the authored glow. UnFocus plays that same chapter in reverse.
        for (unsigned index = 0; index < count; ++index) {
            auto &time = social.challengeTimes[index];
            if (index == social.selectedChallenge) {
                if (time < focusStart) { time = focusStart; }
                time = focusStart + (time - focusStart + social.contentElapsed) % (focusEnd - focusStart + 1);
            } else if (time >= focusStart) {
                if (social.contentElapsed >= time - focusStart) { time = idleEnd; }
                else { time -= social.contentElapsed; }
            }
        }
    }
    unsigned start = 0, end = 0, next = 0, nextEnd = 0;
    if (!Chapter(view.movies, listName, 1, start, end) || !Chapter(view.movies, listName, 2, next, nextEnd)) { return false; }
    const unsigned ordinal = view.movies.Ordinal(listName);
    const auto slots = view.movies.Regions(ordinal, start, region.x, region.y);
    if (slots.size() <= visible) { return false; }
    const float stride = slots[2].y - slots[1].y;
    if (stride <= 0 || next <= start) { return false; }
    MovieRegion viewport = region;
    // ListCallback :235474 clips at the list's top and retains the screen bottom.
    viewport.height = kMenuHeight - viewport.y;
    const bool inside = view.MouseIn(viewport.x, viewport.y, viewport.width, viewport.height);
    float wheel = 0;
    if (inside) { wheel = view.window.TakeWheelDelta(); }
    unsigned maximum = 0;
    if (count > visible) { maximum = count - visible; }
    social.scrollMotion.Update(social.scrollPosition, view.clock, view.dragY, wheel, view.pointerHeld,
        inside && view.pointerPressed, view.inputEnabled, maximum * stride, stride, next - start, state.page == 5);
    const float position = std::clamp(social.scrollPosition, 0.0f, maximum * stride);
    const float extension = social.scrollPosition - position;
    const unsigned first = static_cast<unsigned>(position / stride);
    const unsigned time = start + static_cast<unsigned>((position / stride - first) * (next - start));
    SocialListCallback callback(view, state, profile, first);
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
        const auto *entry = OriginalMenuData("MDS_SCROLLBARS", 1);
        if (!entry) { return false; }
        const unsigned bar = view.movies.Ordinal(entry->movies[0]);
        const auto *movie = view.movies.GetMovie(bar);
        MovieRegion bounds;
        if (!movie || !view.movies.Region(bar, 0, 0, bounds)) { return false; }
        const unsigned barTime = static_cast<unsigned>(movie->duration * position / (maximum * stride));
        return view.movies.Draw(bar, barTime, region.x + region.width,
            region.y + region.height / 2 - bounds.height / 2);
    }
    return result;
}

bool DrawOriginalSocialModel(GameMenu &view, MenuState &state, const CProfileManager &profile) {
    if (state.page != 4) { return true; }
    MovieRegion region;
    if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU"), 3, state.social.socialTime, region)) { return false; }
    MovieRegion model = region;
    if (state.social.socialTab == 1) { model.height *= 0.5f; }
    if (!view.DrawEquippedPlayer(*profile.nativeArchive->toc, *profile.nativeArchive->tables,
        state.social.defaultBrother, state.social.weapons, state.social.armors, 0, nullptr, &model)) { return false; }
    if (state.social.socialTab == 1) {
        return DrawPart(view, state, profile, "GLU_MOVIE_BRO_BUFFS_DETAILS", SocialPart::PowerDetails, region);
    }
    // CMenuFriends::Draw :196440 anchors the duty stamp at the model's feet.
    unsigned start = 0, end = 0;
    if (!Chapter(view.movies, "GLU_MOVIE_ON_DUTY_STAMP", 0, start, end)) { return false; }
    return view.movies.Draw(view.movies.Ordinal("GLU_MOVIE_ON_DUTY_STAMP"), end,
        region.x + region.width / 2, region.y + region.height);
}
} // namespace MenuDetail
