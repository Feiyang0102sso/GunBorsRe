/** Original CMPMatch resource and local authoritative match state.
 * Wire evidence: entries/mp_match_template.bt, CMPMatch::Template::Init :395713.
 * The Bot life budget is a user-requested Windows rule, not BIG data.
 */
#pragma once
#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/gameplay/multiplayer/bot/ZBotSettings.h"
#include <array>
#include <random>

class CMPMatch {
public:
    struct PickupRule { int weight = 0, unknown = 0, seconds = 0; };
    struct Template {
        bool Init(CArrayInputStream &stream);
        unsigned mode = 0;
        std::vector<GameObjectRef> stores, pickups;
        std::vector<PickupRule> pickupRules;
        unsigned discardedCounts[2]{};
        unsigned health = 0, killLimit = 0, seconds = 0, respawnSeconds = 0;
    };
    struct Entry {
        GameObjectRef resource;
        Template data;
        std::vector<GameObjectRef> guns;
    };
    struct Life {
        unsigned serial = 0, shops = 0, grenades = 0, healthPacks = 0;
        unsigned respawnMs = 0;
        bool dead = false;
    };
    enum class Result { Playing, PlayerWon, BotWon, Draw };
    void SetBotLevel(ZBotSettings::Difficulty level) { m_botSettings.difficulty = level; }
    bool HasHardBot() const { return m_botSettings.IsHard(); }
    bool HasUnlimitedBotPowerups() const { return m_botSettings.HasUnlimitedPowerups(); }
    void Bind(const Template &data, unsigned seed);
    void Restart();
    void Update(unsigned deltaMs);
    bool Kill(unsigned victim, int killer);
    bool Respawn(unsigned peer, bool resumeFromShop = false);
    void Surrender(unsigned peer);
    bool CanShop(unsigned peer) const;
    bool EnterShop(unsigned peer);
    bool CanUse(unsigned peer, bool grenade) const;
    void CommitUse(unsigned peer, bool grenade);
    int ChoosePickup();
    const Template &Data() const { return *m_data; }
    const Life &GetLife(unsigned peer) const { return m_lives[peer]; }
    unsigned Score(unsigned peer) const { return m_scores[peer]; }
    unsigned RemainingMs() const { return m_remainingMs; }
    Result GetResult() const { return m_result; }
    static constexpr int PickupIdBase = 5678; // CLevel::GetMPMatchPickupId :114870.
private:
    const Template *m_data = nullptr;
    ZBotSettings m_botSettings;
    Life m_lives[2];
    unsigned m_scores[2]{};
    unsigned m_remainingMs = 0;
    int m_previousPickup = -1;
    Result m_result = Result::Playing;
    std::mt19937 m_random;
};

bool LoadMPMatches(CResTOCManager &toc, ZPackTables &tables, std::vector<CMPMatch::Entry> &entries);
