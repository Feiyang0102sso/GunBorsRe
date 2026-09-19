#include "gun_bros_re/host/ZGameScriptObject.h"
/** @file CPowerup.h
 * @brief Original consumable template (CPowerup::Template::Init :187947).
 */
#ifndef GUN_BROS_RE_CPOWERUP_H
#define GUN_BROS_RE_CPOWERUP_H
#include "gun_bros_re/application/CGunBros.h"
#include <string>
#include "gun_bros_re/data/objects/CGameAssetRef.h"
#include "engine/glu/script/CScriptInterpreter.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include <array>
#include <memory>

class CResTOCManager;
class CGunBros;
class CLevel;
class CPowerUpSelector;

#include "gun_bros_re/gameplay/brother/CBrother.h"
class CLevel;

struct ZPowerupAction {
    std::uint8_t function = 0;
    std::array<std::int16_t, 8> arguments{};
    std::uint8_t count = 0;
    GameObjectRef resource;
};

/** Native queries read a snapshot; gameplay applies emitted actions itself.
 * Script research retains this snapshot. Bound runtime actors are queried live.
 */
struct ZPowerupStatus {
    int healthPercent = 50;
    bool shield = false;
    bool frenzy = false;
    bool autoFire = false;
    bool turret = false;
    std::array<bool, 4> frenzyTypes{};
};

class CPowerup : public ZGameScriptObject {
public:
    /** Host listing snapshot; parsed object ownership remains in the pack. */
    struct Entry;
    static bool LoadEntries(CResTOCManager &toc, CGunBros &tables, std::vector<Entry> &entries);
    CPowerup();
    CPowerup(CResTOCManager &toc, CGunBros &tables, CLevel &scene);
    ~CPowerup();
    struct Template {
        CGameAssetRef name;
        CGameSpriteGluRef sprite;
        std::uint8_t field28 = 0;
        std::uint8_t field29 = 0;
        CScript script;
        std::uint8_t field30 = 0;
        std::uint8_t field112 = 0;
        GameObjectRef effect;
        std::uint8_t field124 = 0;
        static const Template *Load(CResTOCManager &toc, CGunBros &tables, const GameObjectRef &ref);
        bool Init(CArrayInputStream &stream);
    };
    void Bind(const Template &data, const ZPowerupStatus &status = {});
    /** Original exports: 0 equipable, 1 usable, 2 selector, 3 decrement, 4 default. */
    bool Query(std::uint8_t exportId, int argument = kScriptNoArgument);
    void Equip();
    void Use(bool fromSelector = false);
    void Update(int deltaMs);
    void HandleEvent(std::uint8_t event);
    void OnSelectorHidden() { HandleEvent(2); }
    void OnItemsHidden() { HandleEvent(1); }
    void OnInputPadAnimationComplete() { HandleEvent(4); }
    void OnScriptStateEntered() override { m_timerMs = 0; }
    bool IsDone() const { return m_done; }
    unsigned GetStateId() const { return m_interpreter.GetStateId(); }
    unsigned GetUnsupportedCount() const { return m_unsupported; }
    std::vector<ZPowerupAction> TakeActions();
    std::int16_t FunctionResolver(std::uint8_t function, const std::int16_t *arguments, std::uint8_t count);
    std::int16_t ResolveNativeFunction(std::uint16_t id, const std::int16_t *arguments, std::uint8_t count) override;

    /** CPowerup owns its Movie, screen effects and completion events (:188652).
     * The presentation constructor binds desktop drawing resources; the default
     * constructor keeps template queries and script-only execution independent.
     */
    bool Start(const CPowerup::Entry &entry, bool fromSelector = false, unsigned stock = 1);
    /** Stable model ownership survives replacement of its current weapon bank. */
    void BindActor(CBrother &player, CBrother::Vitals &vitals);
    void SetOwner(Collision::ObjectId owner);
    /** Report actual projectile creation; inventory remains the caller's job. */
    unsigned TakeThrownPowerups(GameObjectRef &resource);
    bool Draw();
    void Reset();
    bool IsActive() const;
    bool IsPresentationActive() const;
    unsigned GetElapsed() const;
    bool IsForegroundMovie() const;
    bool HasSelectorFrame() const;
    bool IsSelectorFrameClosing() const;
    unsigned movieCompletions = 0, splashCount = 0, effectCount = 0, failures = 0;
private:
    friend class CLevel;
    friend class CPowerUpSelector;
    CPowerUpSelector *m_selector = nullptr;
    struct Presentation;
    std::unique_ptr<Presentation> m_presentation;
    bool ApplyPresentationActions();
    bool StartMovie(const ZPowerupAction &action);
    void UpdatePresentation(int deltaMs);
    void UpdateTimer(int deltaMs);
    void ResetExecution();
    ZPowerupStatus ReadActorStatus() const;
    bool ApplyActorAction(const ZPowerupAction &action);
    CBrother *m_player = nullptr;
    CBrother::Vitals *m_vitals = nullptr;
    Collision::ObjectId m_owner = Collision::Player;
    GameObjectRef m_resource;
    GameObjectRef m_pendingGrenade;
    unsigned m_stock = 0;
    bool m_actorActionFailed = false;
    bool m_pausedLevel = false;
    const Template *m_template = nullptr;
    CScriptInterpreter m_interpreter;
    ZPowerupStatus m_status;
    std::vector<ZPowerupAction> m_actions;
    int m_timerMs = 0;
    bool m_done = false;
    unsigned m_unsupported = 0;
};
struct CPowerup::Entry {
    GameObjectRef resource;
    CPowerup::Template data;
    std::string name;
    std::string owner;
};
#endif
