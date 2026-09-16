/** Host roster configuration. Native profiles retain earned progress separately. */
#include "gun_bros_re/data/ZLocalBotFriend.h"
#include "gun_bros_re/data/CPlayerProgress.h"
#include "gun_bros_re/data/ZPackTables.h"
#include "engine/core/CStringToKey.h"
#include <charconv>
#include <fstream>
#include <sstream>
#include <map>

namespace {
std::string Trim(const std::string &text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) { return {}; }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
bool Number(const std::string &text, unsigned &value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc() && result.ptr == text.data() + text.size();
}
bool Reference(CResTOCManager &toc, ZPackTables &tables, const std::string &value, ZGameSection section, GameObjectRef &ref) {
    if (value == "none" && section == ZGameSection::Armor) { ref = {}; return true; }
    const auto separator = value.find(':');
    if (separator == std::string::npos) { return false; }
    unsigned index = 0;
    if (!Number(value.substr(separator + 1), index) || index > 255) { return false; }
    const auto pack = value.substr(0, separator);
    ref.packHash = CStringToKey(pack.c_str());
    ref.localIndex = static_cast<std::uint8_t>(index);
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (packIndex < 0 || index >= tables.GetObjectPack(packIndex).GetObjectCount(section)) { return false; }
    std::vector<std::uint8_t> bytes;
    return tables.ReadSectionResource(ref.packHash, section, index, bytes);
}
std::string ReferenceText(ZPackTables &tables, const GameObjectRef &ref) {
    if (ref.IsNull()) { return "none"; }
    return tables.GetPackName(ref.packHash) + ":" + std::to_string(ref.localIndex);
}
struct ZDefinition { std::string identity; std::map<std::string, std::string> values; };
}

ZLocalBotFriend *ZLocalBotRoster::At(unsigned index) const {
    if (index >= m_friends.size()) { return nullptr; }
    return m_friends[index].get();
}
bool ZLocalBotRoster::Select(unsigned index) {
    if (index > Count()) { return false; }
    for (unsigned current = 0; current < Count(); ++current) {
        if (!m_friends[current]->Select(index == current + 1)) { return false; }
    }
    m_selected = index;
    return true;
}
ZLocalBotFriend *ZLocalBotRoster::MatchSelected() const {
    // Host matching uses the active BROS friend. The original default brother
    // has no peer account, so use the first configured bot when none is active.
    if (m_selected == 0) { return At(0); }
    return At(m_selected - 1);
}

bool ZLocalBotRoster::Load(CResTOCManager &toc, ZPackTables &tables, const std::filesystem::path &playerPath, CProfileManager &player) {
    auto root = playerPath;
    if (root.extension() == ".dat") { root = root.parent_path(); }
    std::filesystem::create_directories(root);
    const auto config = root / "local-bots.cfg";
    const bool generatedConfig = !std::filesystem::exists(config);
    if (generatedConfig) {
        // Migrate the existing test friend without replacing its earned account.
        ZLocalBotFriend existing;
        if (!existing.Load(toc, tables, root, &player)) { return false; }
        CPlayerProgress progress;
        progress.Bind(existing.profile.nativeArchive->progression);
        progress.SetExperience(existing.profile.experience);
        std::ofstream output(config);
        output << "# Copy a section with a unique ID to add a friend. Restart to reload.\n"
            "# Only changed definitions reconfigure an account; earned progress otherwise survives.\n"
            "# Resources use original BIG pack:index. Armor may be none.\n"
            "# Optional: coins=N, warbucks=N, powerup.pack5:0=N (resource must exist).\n"
            "[" << ZLocalBotFriend::Identity << "]\nname=LOCAL BOT\nlevel=" << progress.GetLevel()
            << "\nbrother=" << existing.profile.playerBrother << '\n';
        for (unsigned slot = 0; slot < 2; ++slot) { output << "gun" << slot << '=' << ReferenceText(tables, existing.profile.configuration.guns[slot]) << '\n'; }
        for (unsigned slot = 0; slot < 3; ++slot) { output << "armor" << slot << '=' << ReferenceText(tables, existing.profile.configuration.armor[slot]) << '\n'; }
        if (!output) { return false; }
    }
    std::ifstream input(config);
    std::vector<ZDefinition> definitions;
    std::string line;
    unsigned lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') { continue; }
        if (line.front() == '[' && line.back() == ']') {
            const auto identity = Trim(line.substr(1, line.size() - 2));
            if (identity.empty() || identity.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != std::string::npos) { return false; }
            for (const auto &definition : definitions) { if (definition.identity == identity) { return false; } }
            definitions.push_back({identity, {}});
            continue;
        }
        const auto separator = line.find('=');
        if (definitions.empty() || separator == std::string::npos) { std::printf("[bot-roster] invalid config line=%u\n", lineNumber); return false; }
        const auto key = Trim(line.substr(0, separator));
        if (!definitions.back().values.emplace(key, Trim(line.substr(separator + 1))).second) { return false; }
    }
    if (definitions.empty()) { std::printf("[bot-roster] empty config\n"); return false; }
    m_friends.clear(); m_selected = 0;
    for (const auto &definition : definitions) {
        auto entry = std::make_unique<ZLocalBotFriend>();
        entry->identity = definition.identity;
        if (!entry->Load(toc, tables, root, &player)) { return false; }
        const auto &progression = entry->profile.nativeArchive->progression;
        std::ostringstream canonical;
        for (const auto &[key, value] : definition.values) { canonical << key << '=' << value << '\n'; }
        const auto definitionPath = entry->Directory() / "definition.txt";
        std::ifstream previous(definitionPath);
        const std::string saved((std::istreambuf_iterator<char>(previous)), std::istreambuf_iterator<char>());
        const bool changed = saved != canonical.str();
        // A generated definition describes the existing account. Keep fractional
        // level XP during migration; explicit later edits may reset the level.
        const bool applyChanges = changed && !generatedConfig;
        for (const auto &[key, value] : definition.values) {
            if (key == "name") { if (value.empty()) { return false; } entry->name = value; continue; }
            unsigned number = 0;
            GameObjectRef ref;
            if (key == "level") {
                if (!Number(value, number) || number < 1 || number > progression.GetMaximumLevel()) { return false; }
                if (applyChanges) { entry->profile.experience = progression.GetExperienceForLevel(number); }
            } else if (key == "brother") {
                if (!Number(value, number) || number > 1) { return false; }
                if (applyChanges) { entry->profile.playerBrother = number; }
            } else if (key == "gun0" || key == "gun1") {
                if (!Reference(toc, tables, value, ZGameSection::Gun, ref)) { return false; }
                if (applyChanges) { entry->profile.configuration.guns[key.back() - '0'] = ref; entry->profile.Grant(6, ref); }
            } else if (key == "armor0" || key == "armor1" || key == "armor2") {
                if (!Reference(toc, tables, value, ZGameSection::Armor, ref)) { return false; }
                if (applyChanges) {
                    entry->profile.configuration.armor[key.back() - '0'] = ref;
                    if (!ref.IsNull()) { entry->profile.Grant(2, ref); }
                }
            } else if (key == "coins" || key == "warbucks") {
                if (!Number(value, number)) { return false; }
                if (applyChanges && key == "coins") { entry->profile.coins = number; }
                if (applyChanges && key == "warbucks") { entry->profile.warbucks = number; }
            } else if (key.compare(0, 8, "powerup.") == 0) {
                if (!Number(value, number) || !Reference(toc, tables, key.substr(8), ZGameSection::Powerup, ref)) { return false; }
                if (applyChanges) { entry->profile.ConsumePowerup(ref, entry->profile.GetPowerupCount(ref)); entry->profile.AddPowerup(ref, number); }
            } else { std::printf("[bot-roster] unknown field id=%s key=%s\n", definition.identity.c_str(), key.c_str()); return false; }
        }
        if (changed) {
            if (!entry->Save()) { return false; }
            std::ofstream output(definitionPath);
            output << canonical.str(); output.flush();
            if (!output) { return false; }
        }
        if (entry->selected) { m_selected = Count() + 1; }
        m_friends.push_back(std::move(entry));
    }
    player.friendCount = Count();
    player.refinery.friendEfficiencyBonus = CFriendPowerManager::Bonus(Count(), 3);
    for (auto &entry : m_friends) {
        entry->profile.friendCount = Count();
        entry->profile.refinery.friendEfficiencyBonus = CFriendPowerManager::Bonus(Count(), 3);
    }
    std::printf("[bot-roster] loaded count=%u selected=%u config=%s\n", Count(), m_selected, config.string().c_str());
    return true;
}
