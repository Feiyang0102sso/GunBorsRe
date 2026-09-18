/** CPickup::Bind/Spawn/Update/Draw/OnRemove :99677-99937; Windows Sprite submission. */
#include "gun_bros_re/gameplay/pickup/CPickup.h"
#include "gun_bros_re/effects/CParticleSystem.h"

CPickup::~CPickup() { OnRemove(); }

bool CPickup::Bind(const Template &data, const GameObjectRef &resource, CSpriteGlu &glu,
    std::shared_ptr<CParticleSystem> particles, const CParticleEffect *effect) {
    Bind(data);
    if (!m_animation.Init(glu, data.sprite.archetype, data.sprite.animation)) { return false; }
    m_resource = resource;
    m_particles = std::move(particles);
    m_effect = effect;
    return true;
}

void CPickup::Spawn(float x, float y, int objectId, unsigned serial) {
    OnRemove();
    m_x = x;
    m_y = y;
    m_objectId = objectId;
    m_serial = serial;
    // CPickup::Spawn :99889 uses CMap's CParticleSystem, not its effect-layer pool.
    if (m_effect == nullptr) { return; }
    // CPickup::Spawn anchors the looping effect one world unit above it.
    auto *player = m_particles->AddEffect(*m_effect, x, y - 1);
    // AddEffect may legitimately return null when all twenty slots are busy.
    if (player != nullptr) {
        player->SetLooping(true);
        m_effectHandle = m_particles->GetHandle(*player);
    }
}

void CPickup::OnRemove() {
    if (m_particles != nullptr) {
        auto *player = m_particles->Get(m_effectHandle);
        if (player != nullptr) { player->StopSpawning(); }
    }
    m_effectHandle = 0;
}

void CPickup::Update(int deltaMs) {
    m_animation.Update(static_cast<std::uint16_t>(deltaMs));
}

void CPickup::Draw(ZQuadBatch &batch, float scale) const {
    m_animation.Draw(batch, m_x, m_y, scale);
}
