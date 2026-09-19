/** Native DataStore responsibilities; see saves/GB_save_profile.bt and save_payloads.bt. */
#include "gun_bros_re/data/profile/CProfileManagerStorage.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
using namespace ProfileStorageDetail;

/** Native client 1008, CRefinementManager 176889/178471; save_payloads.bt. */
bool CRefinementManager::ReadSavedData(const std::vector<std::uint8_t> &refinement) {
    const unsigned checkpoint = Get32(refinement, 0);
    for (unsigned slot = 0; slot < slots.size(); ++slot) {
        CArrayInputStream entry(refinement.data() + 4 + slot * 28, 28);
        auto &value = slots[slot];
        value.state = entry.ReadUInt32();
        const unsigned rawEfficiency = entry.ReadUInt32();
        std::memcpy(&value.efficiency, &rawEfficiency, 4);
        const int remainingMs = entry.ReadInt32();
        value.startTimeSeconds = entry.ReadUInt32();
        value.totalDurationMs = entry.ReadInt32();
        value.amount = Get64(entry);
        value.finishTimeMs = 0;
        value.finishTime = 0;
        if (value.state == 2) {
            value.finishTimeMs = static_cast<std::int64_t>(checkpoint) * 1000 + remainingMs;
            value.finishTime = (value.finishTimeMs + 999) / 1000;
        }
        if (value.state > 3 || !std::isfinite(value.efficiency)) { return false; }
    }
    return true;
}

bool CRefinementManager::WriteSavedData(std::vector<std::uint8_t> &refinement) const {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    bool activeRefinement = false;
    for (unsigned slot = 0; slot < slots.size(); ++slot) {
        const auto &value = slots[slot];
        const std::size_t offset = 4 + slot * 28;
        const unsigned oldState = Get32(refinement, offset);
        Put32(refinement, offset, value.state);
        unsigned efficiency = 0;
        std::memcpy(&efficiency, &value.efficiency, 4);
        Put32(refinement, offset + 4, efficiency);
        if (value.state == 2) {
            const std::int64_t remaining = std::max<std::int64_t>(0, value.finishTimeMs - now * 1000);
            if (remaining > INT32_MAX) { return false; }
            Put32(refinement, offset + 8, static_cast<unsigned>(remaining));
            activeRefinement = true;
        } else if (oldState != value.state) { Put32(refinement, offset + 8, 0); }
        Put32(refinement, offset + 12, value.startTimeSeconds);
        Put32(refinement, offset + 16, static_cast<unsigned>(value.totalDurationMs));
        Put64(refinement, offset + 20, value.amount);
    }
    if (activeRefinement) { Put32(refinement, 0, static_cast<unsigned>(now)); }
    return true;
}
