#include "gun_bros_re/ZLocalOnlineServices.h"
#include <cstdio>

// Host simulation timing, preserving the previously requested four-second wait.
// These are not resource values or claimed StoreKit response times.
static constexpr std::uint64_t ProductResponseMs = 1000;
static constexpr std::uint64_t PurchaseResponseMs = 4000;

void ZLocalOnlineServices::SetConnected(bool connected) {
    if (m_connected == connected) { return; }
    m_connected = connected;
    std::printf("[local-online] connected=%u\n", connected);
    if (!connected) {
        CancelMatch();
        if (m_purchaseState != PurchaseState::Idle) { m_purchaseState = PurchaseState::Cancelled; }
    }
}

bool ZLocalOnlineServices::BeginMatch(unsigned mode) {
    // CGameCenterManager::findMultiplayerMatch :260057 requests exactly two
    // players. Stage one has no peer provider, so it never reports a match.
    // Stage two supplies the user-requested local bot for cooperative mode.
    // Live and Deathmatch both require the simulated online-services switch.
    if (mode < 1 || mode > 2 || !m_connected) { return false; }
    m_matching = true;
    m_matchMode = mode;
    m_matchClockBound = false;
    std::printf("[local-online] matchmaking mode=%u players=2 status=waiting\n", mode);
    return true;
}

bool ZLocalOnlineServices::AdvanceMatch(std::uint64_t clock) {
    if (!m_matching || (m_matchMode != 1 && m_matchMode != 2) || !m_connected) { return false; }
    if (!m_matchClockBound || clock < m_matchStarted) {
        m_matchStarted = clock;
        m_matchClockBound = true;
        return false;
    }
    // Host delay leaves time to review/cancel the labelled simulated match.
    if (clock - m_matchStarted < 1500) { return false; }
    m_matching = false;
    std::printf("[local-online] matched mode=%u peer=LOCAL_BOT no-network=1\n", m_matchMode);
    return true;
}

void ZLocalOnlineServices::CancelMatch() {
    if (m_matching) { std::printf("[local-online] matchmaking cancelled\n"); }
    m_matching = false;
}

bool ZLocalOnlineServices::BeginPurchase(const std::string &product, std::uint64_t clock) {
    // LaunchIAP :158427 queries the product ID obtained from STORE asset[0].
    if (!m_connected || product.empty() || m_purchaseState != PurchaseState::Idle) { return false; }
    m_product = product;
    m_purchaseStarted = clock;
    m_purchaseState = PurchaseState::QueryingProduct;
    std::printf("[local-iap] query product=%s\n", product.c_str());
    return true;
}

void ZLocalOnlineServices::UpdatePurchase(std::uint64_t clock) {
    if (!m_connected || clock < m_purchaseStarted) { return; }
    const auto elapsed = clock - m_purchaseStarted;
    if (m_purchaseState == PurchaseState::QueryingProduct && elapsed >= ProductResponseMs) {
        m_purchaseState = PurchaseState::Verifying;
        std::printf("[local-iap] verifying product=%s\n", m_product.c_str());
    }
    if (m_purchaseState == PurchaseState::Verifying && elapsed >= PurchaseResponseMs) {
        m_purchaseState = PurchaseState::Completed;
        std::printf("[local-iap] verified product=%s\n", m_product.c_str());
    }
}

void ZLocalOnlineServices::FinishPurchase() {
    m_purchaseState = PurchaseState::Idle;
    m_product.clear();
}
