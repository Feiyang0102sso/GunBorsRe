#define NOMINMAX
#include "gun_bros_re/ui/hud/ZHudResources.h"
#include "gun_bros_re/data/store/CStoreItem.h"
#include <algorithm>
#include <cmath>
void ZHudResources::Icon(unsigned type, const GameObjectRef &object, const ZMovieRegion &region) {
    if (object.IsNull()) { return; }
    if (type == 17 && region.width < 60) {
        for (const CPowerup::Entry &entry : m_powerups) {
            if (entry.resource.packHash != object.packHash || entry.resource.localIndex != object.localIndex) { continue; }
            const CGameSpriteGluRef &sprite = entry.data.sprite;
            m_powerupRenderers[sprite.packHash]->DrawSpriteFitted(sprite.archetype, sprite.animation, 0,
                region.x, region.y, region.width, region.height);
            return;
        }
    }
    for (const CStoreItem::Entry &entry : m_store) {
        // Bundle thumbnails depict several products; a single equipped icon
        // must resolve the matching single-product offer instead.
        bool singleProduct = true;
        for (const GameObjectTypeRef &ref : entry.data.objects) {
            if (ref.type != type || ref.object.packHash != object.packHash || ref.object.localIndex != object.localIndex) { singleProduct = false; break; }
        }
        if (!singleProduct) { continue; }
        for (const GameObjectTypeRef &reference : entry.data.objects) {
            if (reference.type != type || reference.object.packHash != object.packHash || reference.object.localIndex != object.localIndex) { continue; }
            const CGameAssetRef &image = entry.data.assets[1];
            if (image.IsNull() || image.assetId < 0) { continue; }
            const std::uint64_t key = (static_cast<std::uint64_t>(image.packHash) << 32) | image.assetId;
            if (m_icons.count(key) == 0) {
                std::vector<std::uint8_t> bytes;
                ZPNGImage decoded;
                auto texture = std::make_unique<ZTexture>();
                if (!m_tables->ReadSectionResource(image.packHash, ZGameSection::Png, image.assetId, bytes) ||
                    !PNGDecode(bytes, decoded) || !texture->Create(decoded)) { return; }
                m_icons[key] = std::move(texture);
            }
            m_movies.Image(*m_icons[key], region.x, region.y, region.width, region.height);
            return;
        }
    }
}
