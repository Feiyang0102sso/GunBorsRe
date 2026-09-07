/**
 * @file CLevel.h
 * @brief A level: a map plus the script that drives it.
 *
 * Port of CLevel (src/gunbros/level.cpp), cut down to what M3.4 needs.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:114771 (Template::Init),
 *            :114371 (VariableResolver), :117365 (FunctionResolver),
 *            :120988 (where CLevel binds its script and calls export 0)
 *
 * The template is shallow -- a map reference, the script, three uint16 -- and
 * the level object exists here only to be the script's host. Everything a
 * level really does (spawning, objectives, triggers, the camera) is M4 and M5.
 *
 * Most native functions are still unimplemented, and
 * calling one logs its id and arguments rather than failing. That log is the
 * list of what the levels in these archives actually ask for, which is how the
 * rest of the resolver should be prioritised.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CLEVEL_H
#define GUN_BROS_RE_GUN_BROS_CLEVEL_H

#include "engine/CArrayInputStream.h"
#include "glu_script/CScript.h"
#include "glu_script/CScriptInterpreter.h"
#include "glu_script/CScriptResolver.h"
#include "gun_bros/CGameAssetRef.h"

#include <cstdint>

class CMap;

// The CLevel functions implemented so far, all of which only touch the map.
// Reference: :117497 (setCameraLayer), :117508 (setCollisionLayer),
//            :117823 (setTileLayerSpeed)
constexpr std::uint8_t kLevelFunctionSetCameraLayer = 1;
constexpr std::uint8_t kLevelFunctionSetCollisionLayer = 2;
constexpr std::uint8_t kLevelFunctionSetTileLayerSpeed = 42;

// Both speed arguments are fixed-point, and the resolver divides by this
// before passing them to CLayerTile::SetSpeed. Reference: :117827
constexpr float kLevelSpeedArgumentUnit = 1.0f / 256.0f;

// The export the engine calls once the map is bound. Reference: :121003
constexpr std::uint8_t kLevelExportOnLevelStart = 0;

// How many variables CLevel::VariableResolver answers for. Reference: :114371
constexpr std::uint32_t kLevelVariableCount = 8;

/** A level and the script it runs. */
class CLevel : public IScriptObject {
public:
    /**
     * What a LEVEL resource holds.
     *
     * Wire format:
     *   GameObjectRef mapRef
     *   CScript       script
     *   uint16        unknown[3]
     */
    struct Template {
        GameObjectRef mapRef;
        CScript script;

        // Fields 92, 94 and 96 of the template. Read to keep the stream in
        // step; no reader for them has been traced yet.
        std::uint16_t unknown0;
        std::uint16_t unknown1;
        std::uint16_t unknown2;

        Template();

        /** @return false when the stream ran out before the template ended. */
        bool Init(CArrayInputStream &stream);
    };

    CLevel();

    /**
     * Bind a template and its map, then run the script's start handler.
     *
     * Same order as the original: the script is set first, the map second, and
     * only then is export 0 called -- which is what lets OnLevelStart reach
     * into the map and set a layer scrolling.
     *
     * The template and map must outlive the level.
     */
    void Bind(const Template &levelTemplate, CMap &map);

    /** How many native calls were made that nothing implements. */
    std::uint32_t GetUnimplementedCallCount() const { return m_unimplementedCalls; }

    /**
     * Run one of CLevel's native functions. Called by ScriptResolver for
     * class id 5. Reference: :117365
     */
    std::int16_t FunctionResolver(std::uint8_t function, const std::int16_t *arguments,
                                  std::uint8_t argumentCount);

    /**
     * Where one of CLevel's eight script variables lives. Reference: :114371
     * @return null when the variable is outside that range.
     */
    std::int16_t *VariableResolver(std::uint8_t variable);

private:
    /** setCameraLayer. Reference: :117497 */
    void SetCameraLayer(const std::int16_t *arguments, std::uint8_t argumentCount);

    /** setCollisionLayer. Reference: :117508 */
    void SetCollisionLayer(const std::int16_t *arguments,
                           std::uint8_t argumentCount);

    /** setTileLayerSpeed. Reference: :117823 */
    void SetTileLayerSpeed(const std::int16_t *arguments, std::uint8_t argumentCount);

    const Template *m_template;
    CMap *m_map;
    CScriptInterpreter m_interpreter;

    // The eight variables CLevel::VariableResolver hands out pointers to. In
    // the original these are fields of the level object; here they are just
    // storage, so a script that reads or writes one behaves consistently even
    // though nothing else touches them yet. Their meanings come with M4.
    std::int16_t m_variables[kLevelVariableCount];

    std::uint32_t m_unimplementedCalls;
};

#endif  // GUN_BROS_RE_GUN_BROS_CLEVEL_H
