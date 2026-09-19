/** @file CPickup.cpp
 * @brief Collection natives :99742. Rewards always address the human player.
 */
#include "gun_bros_re/gameplay/pickup/CPickup.h"
#include "gun_bros_re/application/CGunBros.h"
#include <cstdio>

bool CPickup::Template::Init(CArrayInputStream &stream) {
    name.Init(stream);
    sprite.Init(stream);
    particleEffect.Init(stream);
    script.Load(stream);
    const unsigned count = stream.ReadUInt8();
    items.resize(count);
    for (GameObjectRef &item : items) { item.Init(stream); }
    return !stream.Overran();
}

bool CPickup::Template::Load(CGunBros &tables, const GameObjectRef &resource) {
    // Original pickup resources and collection-script verification.
    // Disk layout remains CPickup::Template::Init :99591 / pickup_template.bt.
    std::vector<std::uint8_t> payload;
    if (!tables.ReadSectionResource(resource.packHash, ZGameSection::Pickup, resource.localIndex, payload)) {
        std::printf("[pickup] missing template %08x:%u\n", resource.packHash, resource.localIndex);
        return false;
    }
    CArrayInputStream stream(payload);
    if (!Init(stream) || stream.Available() != 0) {
        std::printf("[pickup] invalid %08x:%u remaining=%zu overran=%d\n",
            resource.packHash, resource.localIndex, stream.Available(), stream.Overran());
        return false;
    }
    return true;
}

void CPickup::Bind(const Template &data) {
    OnRemove();
    m_resource = {};
    m_animation.SetAnimation(nullptr);
    m_particles.reset();
    m_effect = nullptr;
    m_template = &data;
    m_collected = false;
    m_unsupported = 0;
    m_actions.clear();
    m_interpreter.SetScript(data.script, *this);
}

bool CPickup::Collect() {
    if (m_collected || m_template == nullptr) { return false; }
    m_collected = true;
    m_interpreter.CallExportFunction(0);
    OnRemove();
    return true;
}

std::vector<ZPickupAction> CPickup::TakeActions() {
    std::vector<ZPickupAction> actions;
    actions.swap(m_actions);
    return actions;
}

std::int16_t CPickup::FunctionResolver(std::uint8_t function,
    const std::int16_t *arguments, std::uint8_t argumentCount) {
    if (function == 4) {
        for (const GameObjectRef &item : m_template->items) {
            ZPickupAction action;
            action.kind = ZPickupAction::Kind::StoreItem;
            action.resource = item;
            m_actions.push_back(action);
        }
        return 0;
    }
    if (function > 4 || argumentCount == 0) {
        ++m_unsupported;
        std::printf("[pickup] unsupported native %u args=%u\n", function, argumentCount);
        return 0;
    }
    ZPickupAction action;
    action.amount = arguments[0];
    if (function == 1) { action.kind = ZPickupAction::Kind::Experience; }
    if (function == 2) { action.kind = ZPickupAction::Kind::Health; }
    if (function == 3) {
        action.kind = ZPickupAction::Kind::Sound;
        std::uint32_t index = 0;
        if (!m_interpreter.GetResource(arguments[0], action.resource.packHash, index)) {
            ++m_unsupported;
            return 0;
        }
        action.resource.localIndex = static_cast<std::uint8_t>(index);
    }
    m_actions.push_back(action);
    return 0;
}
