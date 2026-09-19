#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
namespace MenuDetail {
namespace {
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
std::string RewardQuantity(const ZDailyPrize &prize) {
    // CChallengeManager::CreateRewardQuantityString :240630.
    if (prize.warbucks != 0) { return "X" + std::to_string(prize.warbucks); }
    if (prize.coins != 0) { return "X" + std::to_string(prize.coins); }
    if (prize.experience != 0) { return std::to_string(prize.experience); }
    return {};
}
class CMenuChallengeOption : public ZMovieRegionCallback {
public:
    CMenuChallengeOption(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile,
        bool details, unsigned index) : view(view), state(state), profile(profile), details(details), index(index) {}

    bool Icon(const CGameAssetRef &asset, const ZMovieRegion &region, bool alignRight = false, float *renderedWidth = nullptr) {
        CStoreItem::Entry icon;
        icon.data.assets[1] = asset;
        return view.Icon(*profile.nativeArchive->toc, *profile.nativeArchive->tables, icon,
            region.x, region.y, region.width, region.height, region.alpha, false, true, alignRight, renderedWidth);
    }

    bool ChallengeSprite(unsigned entryIndex, bool active, float x, float y, float alpha) {
        // Provider177 is the original MDS_ICON_CHALLENGES table, not a new icon map.
        const auto *entry = CMenuDataProvider::Find("MDS_ICON_CHALLENGES", entryIndex);
        if (!entry) { return false; }
        unsigned variant = 0;
        if (active) { variant = 1; }
        const unsigned sprite = entry->sprites[variant];
        return view.movies.DrawSprite(sprite >> 16, sprite & 255, 0, x, y, 1, alpha);
    }

    bool ChallengeSpriteBounds(unsigned entryIndex, ZMovieRegion &bounds) {
        const auto *entry = CMenuDataProvider::Find("MDS_ICON_CHALLENGES", entryIndex);
        if (!entry) { return false; }
        const unsigned sprite = entry->sprites[0];
        return view.movies.SpriteBounds(sprite >> 16, sprite & 255, bounds);
    }

    bool DrawMovieRegion(const ZMovieRegion &region) override {
        auto &social = state.social;
        const auto &challenge = state.challenges.manager.current[index];
        const auto &definition = state.challenges.manager.templates[challenge.templateIndex];
        if (!details) {
            if (region.index == 0 && view.Hit(region.x, region.y, region.width, region.height)) {
                state.challenges.selected = index;
                unsigned start = 0, end = 0;
                if (!Chapter(view.movies, "GLU_MOVIE_BRO_OP_BOX", 1, start, end)) { return false; }
                state.challenges.optionTimes[index] = start;
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
                ZMovieRegion meterBounds;
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
                ZMovieRegion cell = region;
                cell.width /= 3;
                ZMovieRegion check;
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
            const auto lines = CTextBox::Format(view.movies, challenge.description, region.width, {1, 0, 1, 1, 1});
            ZStoreRegionClip clip(view, region);
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
            const auto *button = CMenuDataProvider::Find("MDS_BUTTON_CHALLENGE_INVITE_BRO", 0);
            if (!button) { return false; }
            bool pressed = false;
            if (!CMenuMovieButton::DrawFrame(view, *button, region, view.movies.NamedString(button->strings[0]),
                5, true, pressed)) { return false; }
            if (pressed) { social.socialTab = 1; social.scrollPosition = 0; }
        }
        if (region.index >= 3 && region.index <= 5) {
            const unsigned tier = region.index - 3;
            const auto &prize = challenge.prizes[tier];
            ZMovieRegion icon = region;
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
            ZMovieRegion check, person;
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
    bool details;
    unsigned index;
};

}

bool CMenuChallenges::DrawOption(ZMenuSurface &view, CMenuSystem &state,
    const CProfileManager &profile, const ZMovieRegion &region, unsigned index, bool details) {
    if (index >= manager.current.size()) { return true; }
    const char *name = "GLU_MOVIE_BRO_OP_BOX";
    unsigned time = sidebarTime;
    if (!details) { time = optionTimes.at(index); }
    if (details) { name = "GLU_MOVIE_BRO_OPS_DETAILS"; time = sidebarTime; }
    CMenuChallengeOption callback(view, state, profile, details, index);
    return view.movies.Draw(view.movies.Ordinal(name), time, region.x, region.y,
        kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
}
}
