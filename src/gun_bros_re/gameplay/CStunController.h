/** @file CStunController.h
 * @brief Original timed stun and alternating screen-space offset (:394408).
 */
#ifndef GUN_BROS_RE_CSTUNCONTROLLER_H
#define GUN_BROS_RE_CSTUNCONTROLLER_H

class CStunController {
public:
    void SetStunned(int durationMs, int intervalMs, int amplitude);
    void ClearStunned();
    /** Returns true only on the update that expires the effect. */
    bool Update(int deltaMs);
    bool IsActive() const { return m_remainingMs > 0; }
    int GetRemainingMs() const { return m_remainingMs; }
    int GetOffset() const { return m_offset; }
private:
    int m_remainingMs = 0;
    int m_intervalMs = 0;
    int m_amplitude = 0;
    int m_offset = 0;
};
#endif
