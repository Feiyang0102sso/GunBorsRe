/** Local transport for CInputPad selector request/owner/close synchronization.
 * Original request delay :90395; selector limit :90345; first request wins.
 */
#pragma once
#include <algorithm>
#include <cstdint>
class LiveShopSession {
public:
    static constexpr unsigned RequestDelayMs = 750, LimitMs = 10000;
    bool Request(unsigned peer, std::uint64_t now) {
        if (m_active || peer > 1) { return false; }
        m_peer = peer; m_openAt = now + RequestDelayMs; m_active = true;
        return true;
    }
    void Update(std::uint64_t now) { if (m_active && now >= m_openAt + LimitMs) { m_active = false; } }
    bool Close(unsigned peer) { if (!m_active || peer != m_peer) { return false; } m_active = false; return true; }
    bool Active() const { return m_active; }
    bool Visible(std::uint64_t now) const { return m_active && now >= m_openAt; }
    unsigned Owner() const { return m_peer; }
    unsigned Remaining(std::uint64_t now) const {
        if (!m_active) { return 0; }
        if (now < m_openAt) { return LimitMs; }
        return static_cast<unsigned>(m_openAt + LimitMs - std::min(now, m_openAt + LimitMs));
    }
private:
    bool m_active = false;
    unsigned m_peer = 0;
    std::uint64_t m_openAt = 0;
};
