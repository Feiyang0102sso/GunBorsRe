/** @file CMovie.cpp
 * @brief Parse all eight movie object types present in the iOS implementation.
 */
#include "engine/glu/movie/CMovie.h"
#include <cstdio>

void CMovie::Playback::Bind(const CMovie &movie) {
    m_movie = &movie;
    m_loop = false;
    ResetPlayback();
}

void CMovie::Playback::ResetPlayback() {
    m_time = 0;
    m_start = 0;
    m_end = 0;
    if (m_movie != nullptr) { m_end = m_movie->duration; }
    m_pendingLoop = false;
    m_reverse = false;
    m_paused = false;
    m_done = false;
    m_completion = false;
    m_cancelled = false;
}

bool CMovie::Playback::SetChapter(unsigned chapter, bool following) {
    unsigned start = 0, end = 0;
    if (m_movie == nullptr || !m_movie->GetChapterRange(chapter, start, end)) { return false; }
    m_start = start;
    m_end = end;
    m_time = start;
    if (m_reverse) { m_time = end; }
    if (following) { m_start = 0; m_end = m_movie->duration; }
    m_done = false;
    m_completion = false;
    m_cancelled = false;
    return true;
}

bool CMovie::Playback::SetLoopChapter(unsigned chapter) {
    unsigned start = 0, end = 0;
    if (m_movie == nullptr || !m_movie->GetChapterRange(chapter, start, end)) { return false; }
    m_loopStart = start;
    m_loopEnd = end;
    m_pendingLoop = true;
    return true;
}

void CMovie::Playback::SetTime(unsigned time) {
    m_time = time;
    m_done = false;
    m_completion = false;
    m_cancelled = false;
}

void CMovie::Playback::Cancel() {
    m_cancelled = true;
    m_completion = false;
    m_done = false;
}

bool CMovie::Playback::TakeCompletion() {
    const bool completed = m_completion;
    m_completion = false;
    return completed;
}

void CMovie::Playback::Update(unsigned deltaMs) {
    if (m_movie == nullptr || m_paused || m_done || m_cancelled || deltaMs == 0) { return; }
    std::int64_t next = static_cast<std::int64_t>(m_time) + deltaMs;
    if (m_reverse) { next = static_cast<std::int64_t>(m_time) - deltaMs; }
    if (m_pendingLoop && ((!m_reverse && next >= m_loopStart) || (m_reverse && next <= m_loopEnd))) {
        m_start = m_loopStart;
        m_end = m_loopEnd;
        m_pendingLoop = false;
    }
    // Native Update crosses inclusive bounds strictly (> end / < start).
    // Its loop divisor is chapter length, not the number of integer samples.
    const bool crossed = (!m_reverse && next > m_end) || (m_reverse && next < m_start);
    if (crossed) {
        const unsigned length = m_end - m_start;
        if (m_reverse) {
            next = m_start;
            if (m_loop && length > 0) {
                const std::uint64_t distance = static_cast<std::uint64_t>(deltaMs) + m_start - m_time;
                next = m_end - distance % length;
            }
        } else {
            if (m_loop && length > 0) { next = m_start + (next - m_start) % length; }
            else { next = m_end; }
        }
        if (!m_loop) { m_done = true; m_completion = true; }
    }
    m_time = static_cast<unsigned>(next);
}

bool CMovie::GetChapterRange(unsigned index, unsigned &start, unsigned &end) const {
    return chapter.GetRange(duration, index, start, end);
}

bool CMovie::Init(CArrayInputStream &stream) {
    objects.clear();
    chapter.starts.clear();
    width = stream.ReadUInt16();
    height = stream.ReadUInt16();
    duration = stream.ReadUInt32();
    const unsigned objectCount = stream.ReadUInt16();
    if (width == 0 || height == 0 || width > 4096 || height > 4096 || objectCount > 255) { return false; }
    for (unsigned index = 0; index < objectCount; ++index) {
        CMovieObject object;
        if (!object.Init(stream)) {
            std::printf("[movie] invalid object index=%u type=%u\n", index, object.type);
            return false;
        }
        if (object.type == 5) {
            chapter.starts.push_back(0);
            for (const auto &frame : object.frames) { chapter.starts.push_back(frame.time); }
        }
        objects.push_back(std::move(object));
    }
    return !stream.Overran() && stream.Available() == 0;
}
