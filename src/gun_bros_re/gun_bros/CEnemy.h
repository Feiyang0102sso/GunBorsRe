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

#include "glu_script/CScript.h"
#include "glu_script/CScriptInterpreter.h"
#include "glu_script/CScriptResolver.h"
#include "gun_bros/CMesh.h"
#include "gun_bros/CMoveSetMesh.h"
#include "gun_bros/CMoveSetMeshController.h"

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

/** One of the eight slots, in the order Bind leaves them. */
struct EnemyPart {
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

    EnemyPart();
};

class CEnemy : public IScriptObject {
public:
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
    EnemyPart &GetPart(std::size_t index) { return m_parts[index]; }
    const EnemyPart &GetPart(std::size_t index) const { return m_parts[index]; }

    /**
     * Move every live part's clock on, then let the script's own animation
     * sequence advance. Reference: :67876, where CEnemy::Update does the same.
     */
    void Update(std::int32_t deltaMs);

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
    const CMoveSetMesh *m_moveSet;
    EnemyPart m_parts[kEnemyPartSlots];

    // Bind leaves this at one: an enemy is a single model until its script
    // says otherwise.
    std::uint32_t m_partCount;

    CScriptInterpreter m_interpreter;

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
