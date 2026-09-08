/** @file CRefinementManager.h
 * @brief Original refinery data and an offline clock for its twelve slots.
 */
#ifndef GUN_BROS_RE_CREFINEMENTMANAGER_H
#define GUN_BROS_RE_CREFINEMENTMANAGER_H
#include "engine/CArrayInputStream.h"
#include <array>
#include <vector>

constexpr unsigned kRefinementSlotCount = 12;

class CRefinementManager {
public:
    class Template {
    public:
        bool Init(CArrayInputStream &stream);
        std::vector<std::uint32_t> minutes;
        std::vector<std::uint32_t> efficiencyPercent;
        std::vector<std::uint32_t> commonPrice;
        std::vector<std::uint32_t> rarePrice;
        std::vector<std::uint8_t> gate;
    };

    struct CRefinementSlot {
        // Original states: 0 locked, 1 idle, 2 refining, 3 ready to collect.
        unsigned state = 0;
        std::uint64_t amount = 0;
        std::int64_t finishTime = 0;
        float efficiency = 0;
    };

    void Bind(const Template &data);
    bool UnlockSlot(unsigned slot, std::uint64_t &coins, std::uint64_t &warbucks);
    bool BeginRefinement(unsigned slot, unsigned interval, std::uint64_t amount,
        std::uint64_t &xplodium, std::int64_t now);
    void UpdateRefinement(std::int64_t now);
    std::uint64_t GetRefinementSlotYield(unsigned slot) const;
    bool CollectResources(unsigned slot, std::uint64_t &coins);
    bool IsGated(unsigned slot) const;
    std::array<CRefinementSlot, kRefinementSlotCount> slots;

private:
    const Template *m_template = nullptr;
};
#endif
