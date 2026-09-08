/**
 * @file CScriptResolver.cpp
 * @brief Where a script's native calls and class variables go.
 */

#include "glu_script/CScriptResolver.h"

#include "gun_bros/CEnemy.h"
#include "gun_bros/CLevel.h"
#include "gun_bros/CBrother.h"
#include "gun_bros/CGun.h"
#include "gun_bros/CBullet.h"
#include "gun_bros/CArmor.h"
#include "gun_bros/CPickup.h"
#include "gun_bros/CProp.h"
#include "gun_bros/CPowerup.h"

#include <cstdio>

namespace ScriptResolver {

std::int16_t ResolveFunction(IScriptObject *host, std::uint16_t functionId,
                             const std::int16_t *arguments,
                             std::uint8_t argumentCount) {
    const std::uint8_t classId = static_cast<std::uint8_t>((functionId >> 8) & 0xFF);
    const std::uint8_t function = static_cast<std::uint8_t>(functionId & 0xFF);

    if (classId == kScriptClassSpawner) {
        CLevel *level = dynamic_cast<CLevel *>(host);
        if (level == nullptr) { level = host->GetLevelContext(); }
        if (level != nullptr) {
            return level->GetSpawner().FunctionResolver(function, arguments, argumentCount);
        }
        std::printf("[spawner] native %u has no level context\n", function);
        return 0;
    }
    if (classId == kScriptClassArmor) {
        // CArmor::FunctionResolver (:176459) has no native functions.
        return 0;
    }
    if (classId == kScriptClassPickup) {
        return static_cast<CPickup *>(host)->FunctionResolver(function, arguments, argumentCount);
    }
    if (classId == kScriptClassPowerup) {
        return static_cast<CPowerup *>(host)->FunctionResolver(function, arguments, argumentCount);
    }
    if (classId == kScriptClassProp) {
        return static_cast<CProp *>(host)->FunctionResolver(function, arguments, argumentCount);
    }

    // CGame functions 1 and 2 are Utility::Random (:74375), callable by every
    // host. Returning zero here silently freezes scripts that randomise speed.
    if (classId == kScriptClassGame) {
        if ((function == 1 || function == 2) && argumentCount >= 2) {
            return host->RandomInteger(arguments[0], arguments[1]);
        }
        return 0;
    }

    if (classId == kScriptClassLevel) {
        // The cast is the original's, made explicit: the host is whatever the
        // id says it is. Every script running today belongs to a CLevel.
        CLevel *level = dynamic_cast<CLevel *>(host);
        if (level == nullptr) { level = host->GetLevelContext(); }
        if (level != nullptr) { return level->FunctionResolver(function, arguments, argumentCount); }
        // Original level natives resolve the global level, not the enemy host.
        // Arena has no level objectives/music controller. Never cast an enemy
        // into CLevel: even its diagnostic counter would corrupt actor memory.
        if (function == 14 || function == 74) { return 0; }
        std::printf("[arena] level native %u has no level context\n", function);
        return 0;
    }
    if (classId == kScriptClassEnemy) {
        return static_cast<CEnemy *>(host)->FunctionResolver(function, arguments,
                                                             argumentCount);
    }
    if (classId == kScriptClassGun) {
        return static_cast<CGun *>(host)->FunctionResolver(function, arguments, argumentCount);
    }
    if (classId == kScriptClassBullet) {
        return static_cast<CBullet *>(host)->FunctionResolver(function, arguments, argumentCount);
    }
    if (classId == kScriptClassBrother) {
        return static_cast<CBrother *>(host)->FunctionResolver(function, arguments, argumentCount);
    }

    std::printf("[script] class %u function %u, %u args:", classId, function,
                argumentCount);
    for (std::uint8_t i = 0; i < argumentCount; ++i) {
        std::printf(" %d", arguments[i]);
    }
    std::printf(" -- class not implemented\n");
    return 0;
}

std::int16_t *ResolveVariable(IScriptObject *host, std::uint16_t variableId) {
    const std::uint8_t classId = static_cast<std::uint8_t>((variableId >> 8) & 0xFF);
    const std::uint8_t variable = static_cast<std::uint8_t>(variableId & 0xFF);

    if (classId == kScriptClassGame) {
        return host->ResolveGameVariable(variable);
    }

    if (classId == kScriptClassArmor) {
        CArmor *armor = dynamic_cast<CArmor *>(host);
        if (armor != nullptr) {
            return armor->VariableResolver(variable);
        }
        return nullptr;
    }

    if (classId == kScriptClassLevel) {
        CLevel *level = dynamic_cast<CLevel *>(host);
        if (level == nullptr) { level = host->GetLevelContext(); }
        if (level != nullptr) { return level->VariableResolver(variable); }
        return nullptr;
    }
    if (classId == kScriptClassEnemy) {
        return static_cast<CEnemy *>(host)->VariableResolver(variable);
    }
    if (classId == kScriptClassGun) { return static_cast<CGun *>(host)->VariableResolver(variable); }
    if (classId == kScriptClassBullet) { return static_cast<CBullet *>(host)->VariableResolver(variable); }
    if (classId == kScriptClassBrother) { return static_cast<CBrother *>(host)->VariableResolver(variable); }
    if (classId == kScriptClassProp) { return static_cast<CProp *>(host)->VariableResolver(variable); }

    // Null is a legitimate answer here, not an error: the original returns it
    // for every class it does not recognise, and the interpreter carries on.
    return nullptr;
}

}  // namespace ScriptResolver
