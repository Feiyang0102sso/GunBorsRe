#include "gun_bros_viewer/scenes/MapTurretPreview.h"
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
using namespace MapDetail;

namespace {
// Research selections, not animation data: all moves, frames and timing come from BIG.
// pack9 ENEMY 0: physical 0006_0x1a5e, states 2/3/7/8.
// pack9 PROP 47: physical 0078_0x432a, states 2/3/0/1 (green/red/off/yellow).
// LEVEL 0008_0x1bf7 @0x288E..0x2930 links enemies 120/121 to props 37/38.
enum class TurretState { Idle, Active, Off, Charging, Count };
}

void MapTurretPreview::Bind(ZLoadedMap &map) {
    m_enemies.clear();
    m_indicators.clear();
    m_state = 0;
    const auto packHash = CStringToKey("pack9");
    for (ZPlacedEnemy &placed : map.enemies) {
        if (placed.templateData->packHash == packHash && placed.templateData->ordinal == 0) {
            m_enemies.push_back(&placed.model->enemy);
        }
    }
    if (m_enemies.empty()) { return; }
    for (ZPlacedProp &prop : map.props) {
        if (prop.sprite->resource.packHash != packHash || prop.sprite->resource.localIndex != 47) { continue; }
        prop.runtime = std::make_shared<CProp>();
        prop.runtime->Bind(prop.sprite->data, &prop.sprite->durations);
        m_indicators.push_back(&prop);
    }
    ApplyState();
}

void MapTurretPreview::Cycle() {
    if (Empty()) { return; }
    m_state = (m_state + 1) % static_cast<unsigned>(TurretState::Count);
    ApplyState();
}

void MapTurretPreview::ApplyState() {
    unsigned enemyState = 2;
    unsigned indicatorState = 2;
    switch (static_cast<TurretState>(m_state)) {
    case TurretState::Active: enemyState = 3; indicatorState = 3; break;
    case TurretState::Off: enemyState = 7; indicatorState = 0; break;
    case TurretState::Charging: enemyState = 8; indicatorState = 1; break;
    default: break;
    }
    // Enter original states; their sequences still own opening, closing and playback.
    for (CEnemy *enemy : m_enemies) { enemy->SetState(static_cast<std::uint8_t>(enemyState)); }
    for (ZPlacedProp *prop : m_indicators) {
        prop->runtime->SetResearchState(static_cast<std::uint8_t>(indicatorState));
    }
    Update(0);
    std::printf("[map-turret] %s enemies=%zu indicators=%zu\n", StateName(), m_enemies.size(), m_indicators.size());
}

void MapTurretPreview::Update(int deltaMs) {
    for (ZPlacedProp *prop : m_indicators) {
        prop->runtime->Update(deltaMs, false);
        prop->background = prop->runtime->GetPlayer(0);
        prop->main = prop->runtime->GetPlayer(1);
        prop->foreground = prop->runtime->GetPlayer(2);
    }
    // Display states emit LEVEL callbacks. This viewer has no level or combat host.
    for (CEnemy *enemy : m_enemies) { enemy->TakeActions(); }
}

const char *MapTurretPreview::StateName() const {
    switch (static_cast<TurretState>(m_state)) {
    case TurretState::Active: return "Active (red)";
    case TurretState::Off: return "Off";
    case TurretState::Charging: return "Charging (yellow)";
    default: return "Idle (green)";
    }
}
