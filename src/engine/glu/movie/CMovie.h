/** @file CMovie.h
 * @brief Original Glu movie timeline and its typed keyframes.
 */
#ifndef GUN_BROS_RE_CMOVIE_H
#define GUN_BROS_RE_CMOVIE_H
#include "engine/resources/CArrayInputStream.h"
#include <array>

#include "engine/glu/movie/CMovieObject.h"
#include "engine/glu/movie/CMovieChapter.h"

class CMovie {
public:
    /** Per-instance playback over shared resource data. The nested state keeps
     * cached movies independent; semantics follow SetChapter :108873 and
     * Update :109057. Completion belongs to the consumer, never the renderer. */
    class Playback {
    public:
        void Bind(const CMovie &movie);
        void ResetPlayback();
        bool SetChapter(unsigned chapter, bool following = false);
        bool SetLoopChapter(unsigned chapter);
        void SetLoop(bool loop) { m_loop = loop; }
        void SetReverse(bool reverse) { m_reverse = reverse; }
        void SetPaused(bool paused) { m_paused = paused; }
        void SetTime(unsigned time);
        void Cancel();
        void Update(unsigned deltaMs);
        unsigned GetTime() const { return m_time; }
        bool IsDone() const { return m_done; }
        bool IsBound() const { return m_movie != nullptr; }
        bool TakeCompletion();
    private:
        const CMovie *m_movie = nullptr;
        unsigned m_time = 0, m_start = 0, m_end = 0;
        unsigned m_loopStart = 0, m_loopEnd = 0;
        bool m_pendingLoop = false, m_loop = false, m_reverse = false;
        bool m_paused = false, m_done = false, m_completion = false, m_cancelled = false;
    };
    /** iOS CMovie::InitResource :109263; exact serialized order, no padding. */
    bool Init(CArrayInputStream &stream);
    /** Inclusive playback bounds, from CMovieChapter::GetChapterLengthMS :109576.
     * Intermediate chapters stop one millisecond before the next chapter;
     * the final chapter may reach the movie duration. Missing chapters fail. */
    bool GetChapterRange(unsigned chapter, unsigned &start, unsigned &end) const;
    unsigned width = 0, height = 0;
    std::uint32_t duration = 0;
    std::vector<CMovieObject> objects;
    CMovieChapter chapter;
};
#endif
