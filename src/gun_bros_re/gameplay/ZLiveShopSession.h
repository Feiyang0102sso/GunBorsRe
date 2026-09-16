/** Local transport for CInputPad selector request/owner/close synchronization.
 * Original request delay :90395; selector limit :90345; first request wins.
 */
#pragma once
#include <algorithm>
#include <cstdint>
class ZLiveShopSession {
public:
    static constexpr unsigned RequestDelayMs = 750, LimitMs = 10000;
    bool Request(unsigned peer, std::uint64_t now, unsigned delayMs = RequestDelayMs, unsigned limitMs = LimitMs) {
        if (m_active || peer > 1) { return false; }
        m_peer = peer; m_openAt = now + delayMs; m_limitMs = limitMs; m_active = true;
        return true;
    }
    /** Host Live bot policy: one ordinary opening per absolute wave.
     * Cheat/death choices do not spend that allowance; rescue always wins.
     */
    bool RequestForWave(unsigned peer, std::uint64_t now, int wave, bool rescuing, bool exempt = false) {
        if (peer > 1 || rescuing) { return false; }
        if (peer == 1 && !exempt && m_botShopWave == wave) { return false; }
        if (!Request(peer, now)) { return false; }
        if (peer == 1 && !exempt) { m_botShopWave = wave; }
        return true;
    }
    void Update(std::uint64_t now) { if (m_active && now >= m_openAt + m_limitMs) { m_active = false; } }
    bool Close(unsigned peer) { if (!m_active || peer != m_peer) { return false; } m_active = false; return true; }
    bool Active() const { return m_active; }
    bool IsTimed() const { return m_limitMs != UINT32_MAX; }
    bool Visible(std::uint64_t now) const { return m_active && now >= m_openAt; }
    unsigned Owner() const { return m_peer; }
    unsigned Remaining(std::uint64_t now) const {
        if (!m_active) { return 0; }
        if (now < m_openAt) { return m_limitMs; }
        return static_cast<unsigned>(m_openAt + m_limitMs - std::min(now, m_openAt + m_limitMs));
    }
private:
    bool m_active = false;
    unsigned m_peer = 0;
    unsigned m_limitMs = LimitMs;
    std::uint64_t m_openAt = 0;
    int m_botShopWave = -1;
};
