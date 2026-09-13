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
    PurchaseState m_purchaseState = PurchaseState::Idle;
    std::string m_product;
    std::uint64_t m_purchaseStarted = 0;
};
