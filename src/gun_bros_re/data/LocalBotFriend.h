/** Windows friend identity. Original 1006 is an XP gift ring, not a roster.
 * See tests/live-friend-save-research.md; never manufacture NGS credentials A.
 */
#pragma once
#include "gun_bros_re/data/CProfileManager.h"
class LocalBotFriend {
public:
    static constexpr const char *Identity = "windows-test-bot-1";
    bool Load(CResTOCManager &toc, PackTables &tables, const std::filesystem::path &playerPath, const CProfileManager *seed = nullptr);
    bool Save() const;
    bool Select(bool active);
    std::string identity = Identity;
    std::string name = "LOCAL BOT";
    CProfileManager profile;
    bool selected = false;
    const std::filesystem::path &Directory() const { return m_directory; }
private:
    std::filesystem::path m_directory;
};

/** Editable Windows peer definitions; equipment values remain BIG references. */
class LocalBotRoster {
public:
    bool Load(CResTOCManager &toc, PackTables &tables, const std::filesystem::path &playerPath, CProfileManager &player);
    bool Select(unsigned index); // Zero selects the original default brother.
    LocalBotFriend *At(unsigned index) const;
    LocalBotFriend *MatchSelected() const;
    unsigned Count() const { return static_cast<unsigned>(m_friends.size()); }
    unsigned Selected() const { return m_selected; }
private:
    std::vector<std::unique_ptr<LocalBotFriend>> m_friends;
    unsigned m_selected = 0;
};
