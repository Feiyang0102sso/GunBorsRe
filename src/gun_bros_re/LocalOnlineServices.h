/** Windows local service adapter. No retired NGS, StoreKit or Game Center traffic. */
#pragma once
#include <cstdint>
#include <string>

class LocalOnlineServices {
public:
    enum class PurchaseState { Idle, QueryingProduct, Verifying, Completed, Cancelled };

    void SetConnected(bool connected);
    bool IsConnected() const { return m_connected; }
    bool BeginMatch(unsigned mode);
    /** Explicit host identity; never inserted into native friend/save records. */
    static constexpr const char *BotName = "LOCAL BOT";
    bool AdvanceMatch(std::uint64_t clock);
    void CancelMatch();
    bool IsMatching() const { return m_matching; }
    bool BeginPurchase(const std::string &product, std::uint64_t clock);
    void UpdatePurchase(std::uint64_t clock);
    PurchaseState GetPurchaseState() const { return m_purchaseState; }
    const std::string &GetProduct() const { return m_product; }
    void FinishPurchase();

private:
    bool m_connected = false;
    bool m_matching = false;
    unsigned m_matchMode = 0;
    std::uint64_t m_matchStarted = 0;
    bool m_matchClockBound = false;
    PurchaseState m_purchaseState = PurchaseState::Idle;
    std::string m_product;
    std::uint64_t m_purchaseStarted = 0;
};
