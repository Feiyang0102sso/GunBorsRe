/** CMenuSplash binding for the Windows loading surface.
 * Native callbacks :160325, :160500..161008; keysets and payloads stay in BIG.
 */
#ifndef GUN_BROS_RE_CMENUSPLASH_H
#define GUN_BROS_RE_CMENUSPLASH_H
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "gun_bros_re/ui/CMenuSplashData.inc"
#include "engine/graphics/CKeysetResource.h"
#include "gun_bros_re/data/profile/CProfileManager.h"
#include "gun_bros_re/data/profile/CPlayerProgress.h"

enum class ZLoadingMode { Solo, Live, Deathmatch };

class CMenuSplash : public ZMovieRegionCallback {
public:
    bool Init(ZMovieRenderer &movies, unsigned index, const CProfileManager *profile = nullptr, ZLoadingMode mode = ZLoadingMode::Solo) {
        m_movies = &movies;
        m_profile = profile;
        CKeysetResource images, texts;
        auto &core = movies.CorePack();
        if (!images.Load(core, "KEYSET_SPLASH_IMAGES") || !texts.Load(core, "KEYSET_SPLASH_TEXT") ||
            images.handles.size() <= 5 || texts.handles.size() != images.handles.size()) { return false; }
        // CMenuSystem::Init :97481 excludes the five multiplayer/end splashes.
        m_count = static_cast<unsigned>(images.handles.size()) - 5;
        if (mode == ZLoadingMode::Solo) { index %= m_count; }
        // User-required separate pools: the four blue multiplayer wallpapers
        // precede the final red Deathmatch entry in the original KEYSET.
        // iOS 3.6.0's action24 also visits the red entry; see correction research.
        if (mode == ZLoadingMode::Live) { index = m_count + index % (images.handles.size() - m_count - 1); }
        if (mode == ZLoadingMode::Deathmatch) { index = static_cast<unsigned>(images.handles.size() - 1); }
        m_imageHandle = images.handles[index];
        // MENU_GAME_LOAD 0x4031F0 uses GLU_MOVIE_SPLASH and KEYSET images.
        // 0x4031B0 / IDB_SPLASH_MAIN_MP belongs to MENU_BOOT_LOAD, not a level load.
        m_alignment = kSplashImageAlignment;
        m_textHandle = texts.handles[index];
        std::vector<std::uint8_t> bytes;
        ZPNGImage image;
        if (!core.GetResource(m_imageHandle, bytes) || !PNGDecode(bytes, image) || !m_image.Create(image)) { return false; }
        if (!core.GetResource(m_textHandle, bytes)) { return false; }
        m_text.clear();
        for (auto byte : bytes) {
            if (byte == 0) { break; }
            m_text.push_back(static_cast<char>(byte));
        }
        m_ordinal = movies.Ordinal(kSplashMovie);
        const CMovie *movie = movies.GetMovie(m_ordinal);
        if (!movie || !movie->GetChapterRange(1, m_idleStart, m_idleEnd)) { return false; }
        m_duration = movie->duration;
        std::printf("[loading-splash] pack=%s index=%u image=%08x text=%08x movie=%u chapters=%zu\n",
            core.GetShortName().c_str(), index, m_imageHandle, m_textHandle, m_ordinal, movie->chapter.starts.size());
        // Original Bind only formats non-empty strings; a valid empty STR is
        // not a missing resource and must not be replaced with invented tips.
        return true;
    }
    unsigned Count() const { return m_count; }
    unsigned IdleStart() const { return m_idleStart; }
    unsigned ExitStart() const { return m_idleEnd + 1; }
    unsigned Duration() const { return m_duration; }
    unsigned ImageHandle() const { return m_imageHandle; }
    unsigned TextHandle() const { return m_textHandle; }
    bool Draw(unsigned elapsed, bool closing = false) {
        unsigned time = elapsed;
        if (!closing) { time = std::min(elapsed, m_idleStart); }
        if (!m_movies->Draw(m_ordinal, time, 512, 384, 1024, 768, 0, 1, this)) { return false; }
        // CMenuSystem draws HEADER after the branch; NAVBAR_DISABLED keeps metrics.
        const unsigned header = m_movies->Ordinal("GLU_MOVIE_HEADER");
        unsigned start = 0, end = 0;
        if (!m_movies->GetMovie(header)->GetChapterRange(2, start, end)) { return false; }
        Header callback(*m_movies, m_profile);
        if (!m_movies->Draw(header, start, 512, 384, 1024, 768, 0, 1, &callback)) { return false; }
        // CMenuSystem::Bind :97132 positions the indicator from its real bounds.
        ZMovieRegion bounds;
        if (!m_movies->SpriteBounds(0, 124, bounds)) { return false; }
        return m_movies->DrawSprite(0, 124, elapsed, 1024 - bounds.width,
            768 - bounds.height + std::floor(bounds.height / 4));
    }
private:
    class Header : public ZMovieRegionCallback {
    public:
        Header(ZMovieRenderer &renderer, const CProfileManager *profile) : movies(renderer), profile(profile) {}
        bool DrawMovieRegion(const ZMovieRegion &region) override {
            if (!profile) { return true; }
            if (inCluster) {
                if (region.index == 0) {
                    const float fraction = std::min(1.0f, float(progress.GetExperienceInLevel()) / progress.GetExperienceDelta());
                    movies.Rectangle(region.x, region.y, std::floor(region.width * fraction), region.height,
                        1.0f / 255, 149.0f / 255, 215.0f / 255, region.alpha);
                } else if (region.index < 4) {
                    char digits[16];
                    std::snprintf(digits, sizeof(digits), "%.3u", progress.GetLevel());
                    movies.Text(std::string(1, digits[region.index - 1]), region.x,
                        region.y + region.height / 2 - std::floor(movies.TextHeight(7) / 2), 7, 1, 0, region.alpha);
                }
                return true;
            }
            if (region.index == 14 || region.index == 15) {
                auto value = profile->coins;
                if (region.index == 15) { value = profile->warbucks; }
                return movies.Text(std::to_string(value), region.x, region.y, 0, 1, 0, region.alpha);
            }
            if (region.index == 16 && profile->nativeArchive) {
                progress.Bind(profile->nativeArchive->progression);
                progress.SetExperience(profile->experience);
                inCluster = true;
                const bool result = movies.Draw(movies.Ordinal("GLU_MOVIE_INFO_CLUSTER"), 0,
                    region.x, region.y, 1024, 768, 0, region.alpha, this);
                inCluster = false;
                return result;
            }
            return true;
        }
        ZMovieRenderer &movies;
        const CProfileManager *profile;
        CPlayerProgress progress;
        bool inCluster = false;
    };
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index == 0) {
            const float scale = std::max(region.width / m_image.GetWidth(), region.height / m_image.GetHeight());
            const float width = m_image.GetWidth() * scale, height = m_image.GetHeight() * scale;
            float x = region.x;
            if (m_alignment == 1) { x = region.x + (region.width - width) / 2; }
            m_movies->Image(m_image, x, region.y, width, height);
        } else if (region.index == 1) {
            const std::string caption = m_movies->NamedString(kSplashCaption);
            return m_movies->Text(caption, region.x + region.width - m_movies->TextWidth(caption),
                region.y + region.height - m_movies->TextHeight(), 0, 1, 0, region.alpha);
        } else if (region.index == 2) {
            const auto lines = CTextBox::Format(*m_movies, m_text, region.width, {0, 0, 0, 0, 0});
            float y = region.y;
            for (const auto &line : lines) {
                for (const auto &run : line.runs) { m_movies->Text(run.text, region.x + run.x, y, run.font, 1, 0, region.alpha); }
                y += line.height;
            }
        }
        return true;
    }
    ZMovieRenderer *m_movies = nullptr;
    const CProfileManager *m_profile = nullptr;
    ZTexture m_image;
    std::string m_text;
    unsigned m_ordinal = 0, m_count = 0, m_imageHandle = 0, m_textHandle = 0;
    unsigned m_idleStart = 0, m_idleEnd = 0, m_duration = 0;
    unsigned m_alignment = 0;
};
#endif
