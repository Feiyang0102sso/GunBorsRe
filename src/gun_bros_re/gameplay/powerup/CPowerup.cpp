/** @file CPowerup.cpp
 * @brief iOS native ordinals; the older flow scripts have a different layout.
 */
#include "gun_bros_re/gameplay/powerup/CPowerup.h"
#include <cstdio>

bool CPowerup::Template::Init(CArrayInputStream &stream) {
    name.Init(stream);
    sprite.Init(stream);
    field28 = stream.ReadUInt8();
    field29 = stream.ReadUInt8();
    script.Load(stream);
    field30 = stream.ReadUInt8();
    field112 = stream.ReadUInt8();
    effect.Init(stream);
    field124 = stream.ReadUInt8();
    return !stream.Overran();
}

void CPowerup::Bind(const Template &data, const ZPowerupStatus &status) {
    m_template = &data;
    m_status = status;
    m_actions.clear();
    m_timerMs = 0;
    m_done = false;
    m_unsupported = 0;
    m_actorActionFailed = false;
    m_interpreter.SetScript(data.script, *this);
}

bool CPowerup::Query(std::uint8_t exportId, int argument) {
    m_interpreter.CallExportFunction(exportId, static_cast<std::int16_t>(argument));
    return *m_interpreter.GetData(m_interpreter.GetReturnValueRef(), 0) == 1;
}

void CPowerup::Equip() { m_interpreter.CallExportFunction(5); }

void CPowerup::Use(bool fromSelector) {
    m_done = false;
    m_timerMs = 0;
    std::uint8_t function = 6;
    // CPowerup::Use :188704 selects export 7 for selector owner 2, not slot 2.
    if (fromSelector) { function = 7; }
    m_interpreter.CallExportFunction(function);
}

void CPowerup::HandleEvent(std::uint8_t event) {
    m_interpreter.HandleEvent(14, event);
    // The input-pad callback may enter damage/recovery states. Only Exit
    // (native 0) ends the powerup, as in CPowerup::Exit :188138.
}

void CPowerup::Update(int deltaMs) {
    if (m_presentation) {
        UpdatePresentation(deltaMs);
        return;
    }
    UpdateTimer(deltaMs);
}

void CPowerup::UpdateTimer(int deltaMs) {
    if (m_timerMs <= 0) { return; }
    m_timerMs -= deltaMs;
    if (m_timerMs <= 0) { m_timerMs = 0; HandleEvent(3); }
}

std::int16_t CPowerup::FunctionResolver(std::uint8_t function,
    const std::int16_t *arguments, std::uint8_t count) {
    if (function == 0) { m_done = true; return 0; }
    if (function == 8) { m_timerMs = arguments[0] * 1000 / 256; return 0; }
    const ZPowerupStatus status = ReadActorStatus();
    if (function == 12) { return status.healthPercent == 100; }
    if (function == 18) { return status.shield; }
    if (function == 19) { return status.frenzy; }
    if (function == 20) { return static_cast<std::int16_t>(status.healthPercent); }
    if (function == 23) { return status.autoFire; }
    if (function == 28) {
        if (arguments[0] < 0 || arguments[0] >= 4) { ++m_unsupported; return 0; }
        return status.frenzyTypes[arguments[0]];
    }
    if (function == 29) { return status.turret; }
    if (function > 29) { ++m_unsupported; std::printf("[powerup] unsupported native=%u\n", function); return 0; }
    ZPowerupAction action;
    action.function = function;
    action.count = count;
    for (unsigned index = 0; index < count; ++index) { action.arguments[index] = arguments[index]; }
    int resource = -1;
    if (function == 1 || function == 6 || function == 9 || function == 15 || function == 16 ||
        function == 17 || function == 22 || function == 24 || function == 27) { resource = arguments[0]; }
    if (function == 7) { resource = arguments[2]; }
    if (resource >= 0) {
        std::uint32_t index = 0;
        if (!m_interpreter.GetResource(resource, action.resource.packHash, index)) { ++m_unsupported; return 0; }
        action.resource.localIndex = static_cast<std::uint8_t>(index);
    }
    if (m_player != nullptr && (function == 10 || function == 11 || function == 16 ||
        function == 17 || function == 22 || function == 24 || function == 25 || function == 27)) {
        // Original native calls mutate the actor before the next Flow statement.
        if (!ApplyActorAction(action)) { m_actorActionFailed = true; }
    } else { m_actions.push_back(action); }
    return 0;
}

std::vector<ZPowerupAction> CPowerup::TakeActions() {
    std::vector<ZPowerupAction> actions;
    actions.swap(m_actions);
    return actions;
}
