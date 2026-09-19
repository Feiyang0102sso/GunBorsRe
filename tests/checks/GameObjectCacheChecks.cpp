/** Pack-owned templates survive later loads and isolate independent owners. */
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/gameplay/armor/CArmor.h"
#include "gun_bros_re/data/store/CStoreItem.h"
#include <cstdio>
namespace {
class ReadCounter : public CGunBros::LoadProgress {
public:
    void OnResourceRead() override { ++count; }
    unsigned count = 0;
};
}
unsigned CheckGameObjectCache(CResTOCManager &toc) {
    ReadCounter reads;
    CGunBros game(toc);
    game.SetLoadProgress(&reads);
    std::vector<CGun::Entry> guns;
    if (!CGun::LoadEntries(toc, game, guns) || guns.empty()) { return 1; }
    const GameObjectRef ref{guns.front().packHash, static_cast<std::uint8_t>(guns.front().ordinal)};
    const CGun::Template *first = CGun::Template::Load(toc, game, ref);
    if (first == nullptr) { return 1; }
    const unsigned before = reads.count;
    const CGun::Template *again = CGun::Template::Load(toc, game, ref);
    if (first != again || reads.count != before) { return 1; }
    std::vector<CArmor::Entry> armor;
    if (!CArmor::LoadEntries(toc, game, armor) || first != CGun::Template::Load(toc, game, ref)) { return 1; }
    std::vector<CStoreItem::Entry> store;
    const unsigned beforeStore = reads.count;
    if (!CStoreItem::LoadEntries(toc, game, store) || reads.count != beforeStore) { return 1; }
    CGunBros other(toc);
    const CGun::Template *isolated = CGun::Template::Load(toc, other, ref);
    if (isolated == nullptr || isolated == first || isolated->GetFireIntervalMs() != first->GetFireIntervalMs()) { return 1; }
    if (CGun::Template::Load(toc, game, GameObjectRef{}) != nullptr) { return 1; }
    std::printf("[object-cache-check] stable-address=1 read-once=1 independent-owner=1 failures=0\n");
    return 0;
}
