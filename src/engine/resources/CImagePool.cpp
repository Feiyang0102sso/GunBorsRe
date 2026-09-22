/**
 * @file CImagePool.cpp
 * @brief Original CImagePool used by CResourceLoader::LoadNext/RemoveImage.
 *
 * Shared handles replace the original manual reference counts. The weak index
 * never keeps GPU objects alive after their last renderer has released them.
 */

#include "engine/resources/CImagePool.h"
#include <cstdio>
std::shared_ptr<ZTexture> CImagePool::GetImage(CResPackTOC &pack, unsigned handle) {
    const std::uintptr_t context = ZTexture::GetCurrentContext();
    if (context == 0) { return {}; }
    // Different windows may use the same BIG with unshared GL namespaces.
    const Key key(pack.GetPackHash(), handle, context);
    for (auto it = m_images.begin(); it != m_images.end();) {
        if (it->second.expired()) { it = m_images.erase(it); }
        else { ++it; }
    }
    const auto found = m_images.find(key);
    if (found != m_images.end()) { return found->second.lock(); }
    std::vector<std::uint8_t> bytes;
    ZPNGImage decoded;
    if (m_onRead) { m_onRead(); }
    if (!pack.GetResource(handle, bytes) || !PNGDecode(bytes, decoded)) {
        std::printf("[image-pool] invalid image pack=%u handle=%u\n", pack.GetPackHash(), handle);
        return {};
    }
    auto image = std::make_shared<ZTexture>();
    // Models tile their textures, unlike sprite atlases. Sprite/movie pools
    // keep their existing clamped backend; this pool serves model atlases.
    if (!image->Create(decoded, GL_REPEAT)) { return {}; }
    m_images.emplace(key, image);
    return image;
}
