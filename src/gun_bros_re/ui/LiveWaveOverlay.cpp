/** CInputPad::SetUpCommonInterstitialOverlays / Bind :87675 / :90969.
 * Movie regions and all labels are loaded from BIG; statistics come from combat.
 */
#define NOMINMAX
#include "gun_bros_re/ui/SurvivalHud.h"
#include "gun_bros_re/LocalOnlineServices.h"

void SurvivalHud::BeginLiveWave(const MultiplayerStatistics &player, const MultiplayerStatistics &peer) {
    m_liveStats[0] = player; m_liveStats[1] = peer;
    m_liveWaveRemaining = 15000; // CLevel::OnWaveCleared :116994.
    for (auto &notice : m_notices) { notice.releaseLevel = false; }
    QueueOriginalNotice("GLU_MOVIE_WAVE_WRAPUP", "", "", true);
}

bool SurvivalHud::DrawLiveWave(unsigned time) {
    class Callback : public IMovieRegionCallback {
    public:
        Callback(SurvivalHud &owner) : hud(owner) {}
        bool DrawMovieRegion(const MovieRegion &area) override {
            std::string text;
            unsigned font = 0;
            if (area.index == 0) { text = "PLAYER"; }
            if (area.index == 1) { text = hud.m_livePeerName; }
            if (area.index <= 1) { font = 1; }
            if (area.index == 2 || area.index == 3) {
                unsigned animation = 161 + hud.m_liveBrotherIndex;
                if (area.index == 3) { animation = 163 + hud.m_livePeerIndex; }
                // CInputPad::Bind :91125 uses four authored local/peer faces.
                return hud.m_movies.DrawSprite(0, animation, 0, area.x, area.y, 1, area.alpha);
            }
            if (area.index == 4 && hud.m_liveWaveRemaining < 7000 && hud.m_liveWaveRemaining >= 2000) {
                // OverlayWaveStart :87010 appends digits to a prefix STR,
                // which has no printf placeholder. Both parts use font 1.
                text = hud.m_movies.NamedString("IDS_MULTIPLAYER_WRAPUP_WAVE_START_ENDLESS") +
                    std::to_string((hud.m_liveWaveRemaining - 1000) / 1000);
                font = 1;
            }
            static constexpr const char *labels[] = {"IDS_MULTIPLAYER_WRAPUP_KILLS", "IDS_MULTIPLAYER_WRAPUP_ASSISTS",
                "IDS_MULTIPLAYER_WRAPUP_XPLODIUM", "IDS_MULTIPLAYER_WRAPUP_XP", "IDS_MULTIPLAYER_WRAPUP_DEATHS", "IDS_MULTIPLAYER_WRAPUP_TOTALXP"};
            if (area.index >= 5 && area.index <= 10) { text = hud.m_movies.NamedString(labels[area.index - 5]); }
            if (area.index >= 11 && area.index <= 22) {
                const unsigned peer = (area.index - 11) / 6;
                const unsigned field = (area.index - 11) % 6;
                const auto &stats = hud.m_liveStats[peer];
                const std::uint64_t values[] = {stats.wave.kills, stats.wave.assists, stats.wave.xplodium,
                    stats.wave.experience, stats.wave.deaths, stats.total.experience};
                text = std::to_string(values[field]);
            }
            if (text.empty()) { return true; }
            return hud.m_movies.Text(text, area.x + (area.width - hud.m_movies.TextWidth(text, font)) / 2,
                area.y + (area.height - hud.m_movies.TextHeight(font)) / 2, font, 1, 0, area.alpha);
        }
        SurvivalHud &hud;
    } callback(*this);
    return m_movies.Draw(m_movies.Ordinal("GLU_MOVIE_WAVE_WRAPUP"), time, 512, 384, 1024, 768, 0, 1, &callback);
}
