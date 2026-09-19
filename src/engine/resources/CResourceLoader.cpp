#include "engine/resources/CResourceLoader.h"
#include <chrono>
#include <cstdio>
void CResourceLoader::BindPack(CResPackTOC &pack, Range models, Range images) {
    m_packs[pack.GetPackHash()] = {&pack, models, images};
}
bool CResourceLoader::ReadMesh(unsigned hash, unsigned ordinal, std::vector<std::uint8_t> &bytes) {
    const auto found = m_packs.find(hash);
    if (found == m_packs.end() || ordinal >= found->second.models.count) { return false; }
    if (m_onRead) { m_onRead(); }
    return found->second.toc->GetResource(found->second.models.first + ordinal, bytes);
}
void CResourceLoader::AddFunction(std::function<bool()> function) {
    Request request;
    request.function = std::move(function);
    m_requests.push_back(std::move(request));
}
void CResourceLoader::AddImage(unsigned hash, unsigned ordinal, std::shared_ptr<ZTexture> &image) {
    Request request;
    request.packHash = hash;
    request.ordinal = ordinal;
    request.image = &image;
    m_requests.push_back(std::move(request));
}
void CResourceLoader::RemoveImage(std::shared_ptr<ZTexture> &image) {
    // Original RemoveImage also cancels requests targeting an unloaded field.
    for (auto it = m_requests.begin(); it != m_requests.end();) {
        if (it->image == &image) { it = m_requests.erase(it); }
        else { ++it; }
    }
    CImagePool::Remove(image);
}
void CResourceLoader::FlushLoadingData() { m_requests.clear(); }
bool CResourceLoader::LoadNext() {
    const auto start = std::chrono::steady_clock::now();
    while (!m_requests.empty()) {
        Request request = std::move(m_requests.front());
        m_requests.pop_front();
        bool loaded = false;
        if (request.function) { loaded = request.function(); }
        else {
            const auto pack = m_packs.find(request.packHash);
            if (pack != m_packs.end() && request.ordinal < pack->second.images.count) {
                auto image = m_images.GetImage(*pack->second.toc, pack->second.images.first + request.ordinal);
                loaded = image != nullptr;
                if (loaded) { *request.image = std::move(image); }
            }
        }
        if (!loaded) {
            // Current callbacks perform synchronous reads: false is an error,
            // not a deferred asynchronous result. Never leave dangling outputs.
            std::printf("[resource-loader] failed pack=%u ordinal=%u\n", request.packHash, request.ordinal);
            FlushLoadingData();
            return false;
        }
        if (std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(15)) { break; }
    }
    return true;
}
bool CResourceLoader::LoadImmediate() {
    while (IsLoading()) {
        if (!LoadNext()) { return false; }
    }
    return true;
}
