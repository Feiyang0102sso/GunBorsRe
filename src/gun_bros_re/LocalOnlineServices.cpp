#include "gun_bros_re/LocalOnlineServices.h"
#include <cstdio>

// Host simulation timing, preserving the previously requested four-second wait.
// These are not resource values or claimed StoreKit response times.
static constexpr std::uint64_t ProductResponseMs = 1000;
static constexpr std::uint64_t PurchaseResponseMs = 4000;

void LocalOnlineServices::SetConnected(bool connected) {
    if (m_connected == connected) { return; }
    m_connected = connected;
    std::printf("[local-online] connected=%u\n", connected);
    if (!connected) {
        CancelMatch();
        if (m_purchaseState != PurchaseState::Idle) { m_purchaseState = PurchaseState::Cancelled; }
    }
}

bool LocalOnlineServices::BeginMatch(unsigned mode) {
    // CGameCenterManager::findMultiplayerMatch :260057 requests exactly two
    // players. Stage one has no peer provider, so it never reports a match.
    if (!m_connected || mode < 1 || mode > 2) { return false; }
    m_matching = true;
    std::printf("[local-online] matchmaking mode=%u players=2 status=waiting\n", mode);
    return true;
}

void LocalOnlineServices::CancelMatch() {
    if (m_matching) { std::printf("[local-online] matchmaking cancelled\n"); }
    m_matching = false;
}

bool LocalOnlineServices::BeginPurchase(const std::string &product, std::uint64_t clock) {
    // LaunchIAP :158427 queries the product ID obtained from STORE asset[0].
    if (!m_connected || product.empty() || m_purchaseState != PurchaseState::Idle) { return false; }
    m_product = product;
    m_purchaseStarted = clock;
    m_purchaseState = PurchaseState::QueryingProduct;
    std::printf("[local-iap] query product=%s\n", product.c_str());
    return true;
}

void LocalOnlineServices::UpdatePurchase(std::uint64_t clock) {
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

void LocalOnlineServices::FinishPurchase() {
    m_purchaseState = PurchaseState::Idle;
    m_product.clear();
}
