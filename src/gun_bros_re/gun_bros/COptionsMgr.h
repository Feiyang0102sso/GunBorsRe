/** @file COptionsMgr.h
 * @brief Original 32-byte options payload; platform file access stays in NativeProfile.
 * Sources: COptionsMgr::Reset :51336, Write :51400, Read :51427;
 * saves/options.bt. File "p" is separate from numbered DataStores.
 */
#ifndef GUN_BROS_RE_COPTIONSMGR_H
#define GUN_BROS_RE_COPTIONSMGR_H
#include <array>
#include <cstdint>

class COptionsMgr {
public:
    COptionsMgr() { Reset(); }
    void Reset() {
        payload.fill(0); // Original constructor clears mem+12..47.
        payload[4] = 1; // mem+20: sound effects.
        payload[5] = 1; // mem+21: music.
        // mem+22/+23 are device capability flags. Windows has no iOS platform
        // value to import; constructor zero remains until a real file is read.
        payload[8] = 2; // mem+24, original Reset :51389; meaning not yet confirmed.
        payload[16] = 2; // mem+32: AutoBro ASK (0=OFF, 1=ON, 2=ASK).
        payload[21] = 1; // mem+37, original Reset.
        payload[22] = 1; // mem+38: docked sticks.
    }
    bool SoundEnabled() const { return payload[4] != 0; }
    bool MusicEnabled() const { return payload[5] != 0; }
    void SetSoundEnabled(bool enabled) { payload[4] = enabled; }
    void SetMusicEnabled(bool enabled) { payload[5] = enabled; }
    bool NotificationsEnabled() const { return payload[21] != 0; }
    void ToggleNotifications() { payload[21] = !NotificationsEnabled(); }
    bool DockedSticks() const { return payload[22] != 0; }
    void ToggleDockedSticks() { payload[22] = !DockedSticks(); }
    unsigned AutoBro() const {
        unsigned value = 0;
        for (unsigned index = 0; index < 4; ++index) { value |= unsigned(payload[16 + index]) << (index * 8); }
        return value;
    }
    /** CMenuAction::DoAction :93857 cycles OFF -> ON -> ASK -> OFF. */
    void CycleAutoBro() {
        const unsigned value = (AutoBro() + 1) % 3;
        for (unsigned index = 0; index < 4; ++index) { payload[16 + index] = static_cast<std::uint8_t>(value >> (index * 8)); }
    }
    // Preserve unconsumed native bytes on every read/write.
    std::array<std::uint8_t, 32> payload{};
};
#endif
