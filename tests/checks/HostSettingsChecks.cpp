/** Host configuration compatibility and BGM gain checks; no source assets are written. */
#include "TestOutput.h"
#include "gun_bros_re/host/ZHostSettings.h"
#include "gun_bros_re/gameplay/audio/CBGM.h"
#include <cmath>
#include <cstdio>
#include <fstream>

int RunHostSettingsCheck() {
    unsigned failures = 0;
    const std::filesystem::path path = TestOutput::Path("host-settings.cfg");
    std::filesystem::remove(path);
    ZHostSettings defaults;
    if (!defaults.Load(path)) { return 1; }
    ZHostSettings generated;
    if (!generated.Load(path) || generated.title != "GunBroRe" || generated.soundVolume != 3 ||
        generated.effectsVolume != 3 || !generated.drawFPS || generated.debugMode || generated.isConnected ||
        generated.control != 1 || generated.dmBotLevel != 1) { ++failures; }

    // Old flat files keep the new defaults without a migration.
    {
        std::ofstream output(path);
        output << "EffectsVolume=5\nDebugMode=1\nIsConnected=1\nDMBotLevel=2\ncontrol=2\nDrawFPS=0\n";
    }
    ZHostSettings legacy;
    if (!legacy.Load(path) || legacy.title != "GunBroRe" || legacy.soundVolume != 3 ||
        legacy.effectsVolume != 5 || !legacy.debugMode || !legacy.isConnected ||
        legacy.control != 2 || legacy.dmBotLevel != 2 || legacy.drawFPS) { ++failures; }

    // Labels do not change lookup; titles preserve spaces and UTF-8 characters.
    {
        std::ofstream output(path);
        output << "[common]\nTitle = Gun Bros " << u8"测试" << " ; Window title\n"
            << "[audio]\nSoundVolume=7 # BGM only\nEffectsVolume=2\n"
            << "[debug]\nDrawFPS=0\n[common]\nDrawFPS=1\n";
    }
    ZHostSettings grouped;
    if (!grouped.Load(path) || grouped.title != u8"Gun Bros 测试" || grouped.soundVolume != 7 ||
        grouped.effectsVolume != 2 || !grouped.drawFPS) { ++failures; }
    const char *invalidValues[] = {"SoundVolume=-1", "SoundVolume=11", "SoundVolume=bad", "Title=  "};
    for (const char *text : invalidValues) {
        {
            std::ofstream output(path);
            output << text << "\n";
        }
        ZHostSettings invalid;
        if (invalid.Load(path)) { ++failures; }
    }

    // Observe the actual player gain, including pause/resume and saved music-off.
    const ZHostSettings saved = GameHostSettings();
    const float effectsGain = ZAudioPlayer::GetEffectsGain();
    CBGM music;
    const int volumes[] = {0, 3, 10};
    for (int volume : volumes) {
        {
            std::ofstream output(path);
            output << "[audio]\nSoundVolume=" << volume << "\n";
        }
        if (!GameHostSettings().Load(path)) { ++failures; }
        music.SetEnabled(true);
        music.SetVolume(1.0f);
        if (std::abs(music.GetPlaybackState().volume - volume * 0.1f) > 0.0001f) { ++failures; }
        music.SetVolume(0.5f);
        if (std::abs(music.GetPlaybackState().volume - volume * 0.05f) > 0.0001f) { ++failures; }
        music.SetEnabled(false);
        music.SetVolume(1.0f);
        if (music.GetPlaybackState().volume != 0) { ++failures; }
    }
    if (ZAudioPlayer::GetEffectsGain() != effectsGain) { ++failures; }
    GameHostSettings() = saved;
    std::printf("[host-settings-check] defaults, legacy, groups, title, BGM gain: failures=%u\n", failures);
    return failures != 0;
}
