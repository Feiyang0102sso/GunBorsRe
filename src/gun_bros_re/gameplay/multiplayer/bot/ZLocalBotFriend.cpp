#include "gun_bros_re/gameplay/multiplayer/bot/ZLocalBotFriend.h"
#include <fstream>

bool ZLocalBotFriend::Load(CResTOCManager &toc, CGunBros &tables, const std::filesystem::path &playerPath, const CProfileManager *seed) {
    m_directory = playerPath / "local-friends" / identity;
    if (playerPath.extension() == ".dat") { m_directory = playerPath.parent_path() / "local-friends" / identity; }
    const bool exists = std::filesystem::exists(m_directory);
    // Original BIG templates create an independent peer account.
    if (!(profile).LoadNative(toc, tables, m_directory)) { return false; }
    if (!exists && seed != nullptr) {
        // Test-peer provisioning copies a loadout once, not a second game data
        // table. From here onward the two inventories/progress evolve independently.
        profile.configuration = seed->configuration;
        profile.inventory = seed->inventory;
        profile.powerups = seed->powerups;
        profile.weaponMastery = seed->weaponMastery;
        profile.experience = seed->experience;
        profile.coins = seed->coins;
        profile.warbucks = seed->warbucks;
        profile.playerBrother = 1 - seed->playerBrother;
        profile.firstLaunch = false;
    }
    const auto selection = m_directory / "friend.txt";
    if (std::filesystem::exists(selection)) {
        std::ifstream input(selection);
        std::string savedIdentity;
        unsigned version = 0, active = 0;
        if (!(input >> savedIdentity >> version >> active) || savedIdentity != identity || version != 1 || active > 1) { return false; }
        selected = active != 0;
    }
    return Save();
}
bool ZLocalBotFriend::Save() const {
    if (m_directory.empty()) { return true; }
    if (!profile.SaveToDisk(m_directory)) { return false; }
    std::ofstream output(m_directory / "friend.txt", std::ios::trunc);
    output << identity << " 1 " << unsigned(selected) << '\n';
    output.flush();
    return bool(output);
}
bool ZLocalBotFriend::Select(bool active) {
    if (selected == active) { return true; }
    selected = active;
    return Save();
}
