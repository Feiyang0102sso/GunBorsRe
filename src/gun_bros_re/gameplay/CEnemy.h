#include "gun_bros_re/gameplay/ZGameScriptObject.h"
/**
 * @file CEnemy.h
 * @brief An enemy's part table, and the script functions that build it.
 *
 * Port of the assembly half of CEnemy (src/gunbros/enemy.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:73381 (Bind),
 *            :71692 (ResolveFunctionLocally), :67499 (Draw)
 *
 * **An enemy's shape is decided by its own script, not by its template.**
 * Bind lays out eight empty part slots, points every one of them at the single
 * move set the template carries, sets the part count to ONE and every bone
 * index to -1 -- and stops. Nothing in the resource data says a tank has three
 * pieces or where its turret goes. The script says it, on spawn, through four
 * native calls:
 *
 *   0x0A  SetPartCount(n)
 *   0x0B  SetPart(part, move)            -- and the three-argument form
 *         SetPart(part, move, bone)         attaches it to a bone as well
 *   0x0C  SetPartRadius(part, radius)    -- collision, not drawing
 *   0x10  SetPartDirection(part, ...)    -- the extra turn a part gets
 *
 * A move names a mesh config, so choosing a move chooses which of the set's
 * models that part shows. That is how one move set with four configs becomes
 * a body, a turret and two guns.
 *
 * Everything else CEnemy does -- health, AI, collision, pathing, spawning --
 * is M4a and M5. This class carries the fields those will need to sit next to
 * but implements only what standing a model up requires; unrecognised script
 * calls are logged with their arguments, which is how the list of what to
 * build next gets collected.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CENEMY_H
#define GUN_BROS_RE_GUN_BROS_CENEMY_H
#include "gun_bros_re/gameplay/CStunController.h"

#include "engine/glu/script/CScript.h"
#include "engine/glu/script/CScriptInterpreter.h"
#include "gun_bros_re/gameplay/CLinkPathFinder.h"
#include "engine/glu/script/ScriptResolver.h"
#include "engine/graphics/CMesh.h"
#include "engine/graphics/CMoveSetMesh.h"
#include "engine/graphics/CMoveSetMeshController.h"
#include "gun_bros_re/gameplay/ZCombatTypes.h"
#include "gun_bros_re/gameplay/CCollisionData.h"
#include <array>

#include <cstdint>
#include <vector>

// Bind walks 672 bytes in 84-byte steps, so the slot count is fixed at eight
// whatever the enemy turns out to need. Reference: :73381.
constexpr std::size_t kEnemyPartSlots = 8;

// What a part's bone index holds when it hangs off nothing.
constexpr std::int32_t kEnemyNoBoneIndex = -1;

/**
 * The part a state's animation sequence drives.
 *
 * The original keeps this in a field, but Bind writes zero into it and nothing
 * in the binary ever writes it again -- so a state's sequence always plays on
 * part 0, which is also the part everything else hangs off.
 */
constexpr std::size_t kEnemyScriptedPart = 0;

// The four script functions that build the assembly, by their ordinal within
// class 7. Named for what they do; the original has no symbols for them.
constexpr std::uint8_t kEnemyScriptSetPartCount = 0x0A;
constexpr std::uint8_t kEnemyScriptSetPart = 0x0B;
constexpr std::uint8_t kEnemyScriptSetPartRadius = 0x0C;
constexpr std::uint8_t kEnemyScriptSetPartDirection = 0x10;

class CEnemy : public ZGameScriptObject {
public:
    /** One of the eight slots, in the order Bind leaves them. */
    struct Part {
        CMoveSetMeshController controller;

        // Which bone of PART 0's mesh this hangs off, or -1 for none. Part 0 is
        // itself always -1: it is the parent, so it has nothing to hang from.
        std::int32_t boneIndex;

        // The extra turn, as DrawHeirarchy takes it: degrees about an axis. Bind
        // seeds the axis pointing along y and both the angle and the speed at
        // zero.
        //
        // The SPEED is what makes a blade spin: CEnemy::Update (:67820) does
        // `angle += speed * deltaSeconds` for every part whose speed is not zero,
        // and nothing else ever writes the angle. So a part turns forever or not
        // at all, and which of the two is a script call away.
        float extraAngleDegrees;
        float extraAngleDegreesPerSecond;
        float extraAxisX, extraAxisY, extraAxisZ;

        // Collision radius, set by 0x0C. Kept because the script writes it and
        // dropping it would silently lose data; nothing reads it until M5.
        float radius;
        bool visible = true;
        bool followsFacing = true;
        float hitFlash = 0;

        Part();
    };

    /** Script-native side effects are consumed by the scene after actor update. */
    struct Action {
        enum class Kind {
            Bullet, Effect, LinkedEffect, StopEffect, Sound, LoopSound, StopSound,
            Splash, Broadcast, SpawnEnemy, RemoveBullet, CollisionResolved,
            Shake, Reward, Stun, LevelEvent, SpawnPickup, TurretActive, Teleported
        };
        Kind kind = Kind::Effect;
        GameObjectRef resource;
        int part = 0;
        int node = -1;
        int slot = 0;
        int effectGroup = 3;
        bool alignEffect = false;
        float effectScale = 1;
        float x = 0;
        float y = 0;
        float direction = 0;
        float speed = 0;
        float damage = 0;
        float radius = 0;
        float force = 0;
        int durationMs = 0;
        ZCombatId projectile = 0;
        ZCombatId owner = 0;
        ZHitResult result = ZHitResult::Pending;
    };

    /** Runtime state next to the original enemy's script and part table.
     * Grouped for the desktop port; not a separate original CEnemyCombat class.
     */
    struct CombatState {
        GameObjectRef templateRef;
        // IDs are the original class 7 variable IDs (:68982), not field offsets.
        // 0 move speed; 1 facing mode; 2 hit part; 3 damage /256;
        // 4 splash-hit flag; 5 hit world angle; 6 bullet speed; 7 hit edge group;
        // 8 facing; 9 relative hit angle; 10 turn speed; 11 aim node;
        // 12 contact force; 13 contact force time; 14 active part;
        // 15 health bar visibility; 16 collision allegiance; 17 contact damage;
        // 18 path index; 19 critical hit; 20 wave index.
        std::array<std::int16_t, 21> variables{};
        bool enabled = false;
        ZCombatId id = 0;
        // Local equivalent of the projectile creator's participant ownership.
        ZCombatId summoner = 0;
        float x = 0;
        float y = 0;
        float previousX = 0;
        float previousY = 0;
        float facing = 0;
        float health = 0;
        float maxHealth = 0;
        float lastDamage = 0;
        float totalDamage = 0;
        float hitFlash = 0;
        int healthBarFlashMs = 0; // CEnemy::Damage mem+1240, independent of mesh tint.
        bool dead = false;
        bool removed = false;
        int portalObjectId = -1;
        bool portalActive = false;
        bool targetable = true;
        bool turret = false;
        int targetType = 0;
        ZCombatId targetId = 0;
        float targetX = 0;
        float targetY = 0;
        bool hasNavigationTarget = false;
        float navigationX = 0;
        float navigationY = 0;
        // CFlock snapshot for this logical tick; independent of script state.
        float flockX = 0;
        float flockY = 0;
        bool targetAlive = false;
        float targetRange = 100000;
        int behaviour = 7;
        float arrivalDistance = 0;
        bool arrived = false;
        float destinationX = 0;
        float destinationY = 0;
        int movementTimer = -1;
        float rotationStart = 0;
        float rotationEnd = 0;
        float rotationTime = 0;
        float rotationDuration = 0;
        bool rotationSmooth = false;
        int functionTimer = 0;
        int timerFunction = 0;
        int eventTimer = 0;
        int autoFireInterval = 0;
        int autoFireTimer = 0;
        int autoFireResource = -1;
        float triggerDistance = 0;
        bool triggerInside = false;
        float scaleFactor = 1;
        int deathCount = 0;
        int hitCount = 0;
        unsigned deferredMechanisms = 0; // 1 boss presentation, 2 level context, 4 rewards.
        std::uint32_t randomState = 1;
        GameObjectRef bullet;
        CCollisionData collision;
        ZCombatHit pendingHit;
        bool collisionPending = false;
        ZHitResult collisionResult = ZHitResult::Pending;
        std::vector<Action> actions;
    };

    CEnemy();

    /**
     * Lay out the eight slots and bind the script, as CEnemy::Bind does.
     *
     * `configMeshes` is indexed by mesh config and must be as long as the move
     * set's config list -- the original's resource loader has already loaded
     * every config by this point, so every slot can be handed the whole set.
     * Nothing here is owned; all three arguments must outlive this object.
     */
    void Bind(const CScript &script, const CMoveSetMesh &moveSet,
              const std::vector<const CMesh *> &configMeshes);

    /**
     * Run the script far enough to assemble the model.
     *
     * The original spawns an enemy through several paths; the one that exists
     * purely to show a model is SpawnForUI (:72858), which calls export 3.
     * State 0 is entered first, because a state may override any export and
     * because the assembly calls often live in the first state's enter code.
     *
     * @return whether anything ran at all.
     */
    bool SpawnForUI();

    /**
     * Run the script the way a level does: export 0.
     *
     * Both overloads of CEnemy::Spawn (:73239, :73284) end in
     * `CallExportFunction(interpreter, 0)`, and that is the only spawn a real
     * enemy ever gets. Export 3 is the menu's, and for eighteen of the
     * seventy-eight templates it does nothing at all -- a turret assembles its
     * barrel in export 0 and stays a bare base without it.
     *
     * @return whether anything ran at all.
     */
    bool Spawn();

    /**
     * Enter one of the script's states, as the script's own transitions do.
     *
     * A state carries an animation SEQUENCE -- a list of move indices -- and
     * entering one starts that sequence playing. This is how the game reaches
     * an enemy's idle, attack and death animations: they are states, not
     * moves, and a move on its own is only one link of the chain.
     *
     * Hands the body back to the script, since the script is driving again.
     */
    bool SetState(std::uint8_t stateId);

    std::uint8_t GetStateId() const { return m_interpreter.GetStateId(); }

    /** The script this enemy is running, for a caller that wants to read it. */
    const CScriptInterpreter &GetInterpreter() const { return m_interpreter; }

    std::uint32_t GetPartCount() const { return m_partCount; }
    Part &GetPart(std::size_t index) { return m_parts[index]; }
    const Part &GetPart(std::size_t index) const { return m_parts[index]; }

    /**
     * Move every live part's clock on, then let the script's own animation
     * sequence advance. Reference: :67876, where CEnemy::Update does the same.
     */
    void Update(std::int32_t deltaMs);

    // Arena enables world simulation before loading the model. Existing model
    // viewers still execute scripts, but do not move their display objects.
    CombatState combat;
    CStunController stun;
    void ConfigureTemplate(float radius, bool targetable,
        const GameObjectRef &bullet, const CCollisionData &collision);
    void SetTarget(ZCombatId id, float x, float y, bool alive);
    void SetPath(const ILayerPath *path);
    bool CanReceiveProjectile(int ownerType, ZCombatId owner) const;
    /** CEnemy::CanCollide :67243, specialized for a player (object type 0). */
    bool CanCollideWithPlayer() const;
    ZHitResult ReceiveHit(const ZCombatHit &hit);
    void Damage(float amount);
    bool TriggerEvent(std::uint8_t event);
    void HandleMessage(int message);
    void OnScriptStateEntered() override;
    std::vector<Action> TakeActions();
    std::size_t GetUnsupportedFunctionCount() const;

    // --- the script's view of this object ---

    std::int16_t FunctionResolver(std::uint8_t function,
                                  const std::int16_t *arguments,
                                  std::uint8_t argumentCount);

    std::int16_t *VariableResolver(std::uint8_t variable);

    /**
     * Stop the script from choosing part 0's move, or let it again.
     *
     * **Not in the original**, and the one thing in this class that is not.
     * A state carries an animation sequence, and Refresh steps through it
     * every time the current move plays out -- so a caller that sets a move by
     * hand has it taken back a moment later. Locking leaves the script running
     * and every other part alone; it only makes SetScriptSequenceFrame a
     * no-op, which is what an inspector wants and what a level never asks for.
     */
    void SetBodyMoveLocked(bool locked) { m_bodyMoveLocked = locked; }
    bool IsBodyMoveLocked() const { return m_bodyMoveLocked; }

    /**
     * Play a move on part 0. This is the second slot of the host interface,
     * and for an enemy a "sequence frame" IS a move index: the original's
     * CEnemy::OnMoveChanged (:68811) hands it straight to SetMove.
     */
    void SetScriptSequenceFrame(std::uint8_t frame) override;

    /**
     * Whether part 0's move has just played out, which is what lets the
     * sequence step on. The original reaches this through a method IDA named
     * CEnemy::GetMoveLooped (:68817) -- a misnomer: the byte it reads is the
     * animation controller's FINISHED flag, not its loop flag.
     */
    bool IsScriptSequenceFrameFinished() override;

private:
    bool ResolveCombatFunction(std::uint8_t function, const std::int16_t *arguments,
        std::uint8_t argumentCount, std::int16_t &result);
    void UpdateCombatBeforeAnimation(int deltaMs);
    void UpdatePathFinder(float waypointX, float waypointY, float seconds);
    void UpdateCombatAfterAnimation(int deltaMs);
    void UpdateCombatTimers(int deltaMs);
    void SetBehaviour(const std::int16_t *arguments, int count);
    void QueueBullet(const GameObjectRef &resource, int part, int node, float direction);
    GameObjectRef ScriptResource(int index) const;
    float Random(float minimum, float maximum);
    float TargetAngle() const;
    void ResolvePendingHit(bool apply);
    const CMoveSetMesh *m_moveSet;
    std::vector<const CMesh *> m_configMeshes;
    Part m_parts[kEnemyPartSlots];

    // Bind leaves this at one: an enemy is a single model until its script
    // says otherwise.
    std::uint32_t m_partCount;

    CScriptInterpreter m_interpreter;
    const ILayerPath *m_path = nullptr;
    CLinkPathFinder m_linkPathFinder;

    // Whether SetBodyMoveLocked has taken part 0's move away from the script.
    bool m_bodyMoveLocked;

    // Somewhere for VariableResolver to point at, so a script writing a class
    // variable this port has not implemented does not write through null. The
    // original hands back a real field for each; this hands back scratch and
    // logs which one was wanted.
    std::int16_t m_variableScratch;

    // Which unimplemented ids have already been reported. A running script
    // calls the same handful many times a second, and a line per call buries
    // everything else; a line per id is the list of what to build next, which
    // is the whole point of logging them.
    bool m_reportedFunction[256];
    bool m_reportedVariable[256];
};

#endif  // GUN_BROS_RE_GUN_BROS_CENEMY_H
