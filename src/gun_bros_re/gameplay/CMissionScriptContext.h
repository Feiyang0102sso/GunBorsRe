#include "gun_bros_re/gameplay/GameScriptObject.h"
/** @file CMissionScriptContext.h
 * @brief Collect requirements by executing the original Mission export 2.
 * Mission::FunctionResolver :163806, GetLevelRequirement :163847, IsLocked :164280.
 * Disk source: entries/mission_entry.bt and flow_bytecode.bt.
 */
#ifndef GUN_BROS_RE_CMISSIONSCRIPTCONTEXT_H
#define GUN_BROS_RE_CMISSIONSCRIPTCONTEXT_H
#include "engine/glu/script/CScriptInterpreter.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include <cstdio>

class CMissionScriptContext : public GameScriptObject {
public:
    struct Requirement {
        GameObjectRef object;
        unsigned type = 29;
    };
    bool Bind(const CScript &script) {
        requirements.clear();
        level = 0;
        valid = true;
        if (!script.IsPresent()) { return true; }
        interpreter.SetScript(script, *this);
        interpreter.CallExportFunction(2);
        return valid;
    }
    std::int16_t FunctionResolver(unsigned function, const std::int16_t *args, unsigned count) {
        if (function == 0) { return 1; }
        if (function == 1 && count == 1) { level = args[0]; return 0; }
        if (function == 2 && count == 2) {
            Requirement requirement;
            std::uint32_t ordinal = 0;
            if (!interpreter.GetResource(static_cast<std::uint16_t>(args[0]), requirement.object.packHash, ordinal) || ordinal > 255) {
                valid = false;
                std::printf("[mission-script] invalid resource slot=%d ordinal=%u\n", args[0], ordinal);
                return 0;
            }
            requirement.object.localIndex = static_cast<std::uint8_t>(ordinal);
            requirement.type = static_cast<std::uint16_t>(args[1]);
            requirements.push_back(requirement);
            return 0;
        }
        valid = false;
        std::printf("[mission-script] unsupported native=%u argc=%u\n", function, count);
        return 0;
    }
    std::vector<Requirement> requirements;
    int level = 0;
    bool valid = true;
private:
    CScriptInterpreter interpreter;
};
#endif
