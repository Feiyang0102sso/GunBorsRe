/**
 * @file CImagePool.h
 * @brief Original CImagePool used by CResourceLoader::LoadNext/RemoveImage.
 *
 * Shared handles replace the original manual reference counts. The weak index
 * never keeps GPU objects alive after their last renderer has released them.
 */
#pragma once
#include "engine/graphics/ZTexture.h"
#include "engine/resources/CResPackTOC.h"
#include <map>
#include <memory>
#include <tuple>
#include <functional>
class CImagePool {
public:
    std::shared_ptr<ZTexture> GetImage(CResPackTOC &pack, unsigned handle);
    static void Remove(std::shared_ptr<ZTexture> &image) { image.reset(); }
    void SetReadObserver(std::function<void()> observer) { m_onRead = std::move(observer); }
private:
    using Key = std::tuple<unsigned, unsigned, std::uintptr_t>;
    std::map<Key, std::weak_ptr<ZTexture>> m_images;
    std::function<void()> m_onRead;
};
