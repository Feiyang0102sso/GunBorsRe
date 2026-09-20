/** Host configuration compatibility and BGM gain checks; no source assets are written. */
#include "TestOutput.h"
#include "gun_bros_re/host/ZHostSettings.h"
#include "gun_bros_re/gameplay/audio/CBGM.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace {
std::string ReadConfigText(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
}

int RunHostSettingsCheck() {
    unsigned failures = 0;
    const std::filesystem::path path = TestOutput::Path("host-settings.cfg");
    std::filesystem::remove(path);
    ZHostSettings defaults;
    if (!defaults.Load(path)) { return 1; }
    ZHostSettings generated;
    if (!generated.Load(path) || generated.title != "GunBroRe" || generated.soundVolume != 3 ||
        generated.effectsVolume != 3 || !generated.drawFPS || generated.debugMode || generated.isConnected ||
        generated.control != 1 || generated.dmBotLevel != 1 || generated.screenX != 1600 || generated.screenY != 1200 ||
        !generated.startDialog) { ++failures; }

    // Old flat files keep the new defaults without a migration.
    {
        std::ofstream output(path);
        output << "EffectsVolume=5\nDebugMode=1\nIsConnected=1\nDMBotLevel=2\ncontrol=2\nDrawFPS=0\n";
    }
    ZHostSettings legacy;
    if (!legacy.Load(path) || legacy.title != "GunBroRe" || legacy.soundVolume != 3 ||
        legacy.effectsVolume != 5 || !legacy.debugMode || !legacy.isConnected ||
        legacy.control != 2 || legacy.dmBotLevel != 2 || legacy.drawFPS || legacy.screenX != 1600 || legacy.screenY != 1200 ||
        !legacy.startDialog) { ++failures; }

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
    const char *invalidValues[] = {"SoundVolume=-1", "SoundVolume=11", "SoundVolume=bad", "Title=  ",
        "ScreenX=0", "ScreenY=-1", "ScreenX=16385", "ScreenY=abc", "ScreenX=800junk", "ScreenY=999999999999",
        "StartDialog=2", "StartDialog=-1", "StartDialog=bad", "StartDialog=1junk"};
    for (const char *text : invalidValues) {
        {
            std::ofstream output(path);
            output << text << "\n";
        }
        ZHostSettings invalid;
        if (invalid.Load(path)) { ++failures; }
    }

    std::printf("[host-settings-check] loading failures=%u\n", failures);
    // Saving edits must retain comments, section order, unknown content, BOM and CRLF.
    {
        std::ofstream output(path, std::ios::binary);
        output << "\xEF\xBB\xBF# User note\r\n[common]\r\nTitle = Gun Bros ; Keep this title note\r\n"
            << "ScreenX = 800 # Keep width note\r\n\r\n[audio]\r\nSoundVolume=2\r\n"
            << "[debug]\r\nDrawFPS=0\r\nDrawFPS=1\r\n[custom]\r\nUnknown=1\r\n";
    }
    ZHostSettings edited;
    if (!edited.Load(path)) { ++failures; }
    edited.title = u8"Gun Bros 启动测试";
    edited.screenX = 1280; edited.screenY = 960;
    edited.soundVolume = 7; edited.effectsVolume = 4;
    edited.isConnected = true; edited.dmBotLevel = 3; edited.control = 2;
    edited.debugMode = true; edited.drawFPS = false;
    edited.startDialog = false;
    if (!edited.Save(path)) { ++failures; }
    const std::string savedText = ReadConfigText(path);
    if (savedText.find("\xEF\xBB\xBF# User note\r\n") != 0 ||
        savedText.find("ScreenX = 1280 # Keep width note\r\n") == std::string::npos ||
        savedText.find("; Keep this title note\r\n") == std::string::npos ||
        savedText.find("DrawFPS=0\r\nDrawFPS=0\r\n") == std::string::npos ||
        savedText.find("[custom]\r\nUnknown=1\r\n") == std::string::npos ||
        savedText.find("ScreenY=960") > savedText.find("[audio]") ||
        savedText.find("StartDialog=0\r\n") > savedText.find("[audio]") ||
        savedText.find("StartDialog=0\r\n") < savedText.find("[common]")) { ++failures; }
    std::ofstream evidence(TestOutput::Path("host-settings-saved.cfg"), std::ios::binary);
    evidence << savedText;
    evidence.close();
    std::printf("[host-settings-check] saved text failures=%u\n", failures);
    ZHostSettings reloaded;
    if (!reloaded.Load(path) || reloaded.title != edited.title || reloaded.screenX != 1280 || reloaded.screenY != 960 ||
        reloaded.soundVolume != 7 || reloaded.effectsVolume != 4 || !reloaded.isConnected || reloaded.dmBotLevel != 3 ||
        reloaded.control != 2 || !reloaded.debugMode || reloaded.drawFPS || reloaded.startDialog) { ++failures; }
    if (!reloaded.Save(path) || ReadConfigText(path) != savedText) { ++failures; }
    edited.screenX = 0;
    if (edited.Save(path) || ReadConfigText(path) != savedText) { ++failures; }
    edited.screenX = 1280; edited.title = "bad;title";
    if (edited.Save(path) || ReadConfigText(path) != savedText) { ++failures; }

    std::printf("[host-settings-check] save and reload failures=%u\n", failures);
    // Legacy files gain the new keys without losing their existing flat values.
    {
        std::ofstream output(path, std::ios::binary);
        output << "# Flat config\nEffectsVolume=5\ncontrol=2";
    }
    ZHostSettings flat;
    if (!flat.Load(path) || !flat.Save(path)) { ++failures; }
    ZHostSettings flatReloaded;
    if (!flatReloaded.Load(path) || flatReloaded.screenX != 1600 || flatReloaded.screenY != 1200 ||
        flatReloaded.effectsVolume != 5 || flatReloaded.control != 2 || !flatReloaded.startDialog ||
        ReadConfigText(path).find("# Flat config\nEffectsVolume=5\ncontrol=2") != 0) { ++failures; }

    // Observe the actual player gain, including pause/resume and saved music-off.
    const ZHostSettings saved = GameHostSettings();
    std::printf("[host-settings-check] config failures=%u\n", failures);
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
    std::printf("[host-settings-check] defaults, legacy, groups, title, dimensions, lossless save, BGM gain: failures=%u\n", failures);
    return failures != 0;
}
