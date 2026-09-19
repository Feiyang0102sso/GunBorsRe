/** Shared level cache; loading belongs to the original bullet template. */
#include "gun_bros_re/gameplay/level/CLevel.h"
namespace {
std::uint64_t ResourceKey(const GameObjectRef &ref) {
    return (static_cast<std::uint64_t>(ref.packHash) << 32) | ref.localIndex;
}

}
const CBullet::Template *CLevel::GetBulletTemplate(const GameObjectRef &ref) {
    const std::uint64_t key = ResourceKey(ref);
    auto found = m_bulletTemplates.find(key);
    if (found != m_bulletTemplates.end()) { return found->second.get(); }
    auto data = std::make_unique<CBullet::Template>();
    if (!data->Load(*m_tables, *m_program, ref)) { return nullptr; }
    const CBullet::Template *result = data.get();
    m_bulletTemplates[key] = std::move(data);
    return result;
}
