#pragma once
/** Original resourceLoader.cpp: queued functions and images, 102097..102535.
 * Game keyset resolution stays with the caller. GPU image ownership is scoped
 * to consumers; this loader retains only a weak pool index.
 */
#include "engine/resources/CImagePool.h"
#include <deque>
#include <functional>
class CResourceLoader {
public:
    struct Range { unsigned first = 0; unsigned count = 0; };
    void BindPack(CResPackTOC &pack, Range models, Range images);
    bool ReadMesh(unsigned packHash, unsigned ordinal, std::vector<std::uint8_t> &bytes);
    // Captured owners and image destinations stay alive and at stable addresses
    // until the queue is drained or their pending requests are cancelled.
    void AddFunction(std::function<bool()> function);
    void AddImage(unsigned packHash, unsigned ordinal, std::shared_ptr<ZTexture> &image);
    void RemoveImage(std::shared_ptr<ZTexture> &image);
    bool LoadNext();
    bool LoadImmediate();
    void FlushLoadingData();
    bool IsLoading() const { return !m_requests.empty(); }
    void SetReadObserver(std::function<void()> observer) {
        m_onRead = observer;
        m_images.SetReadObserver(std::move(observer));
    }
private:
    struct Pack { CResPackTOC *toc = nullptr; Range models; Range images; };
    struct Request {
        std::function<bool()> function;
        unsigned packHash = 0;
        unsigned ordinal = 0;
        std::shared_ptr<ZTexture> *image = nullptr;
    };
    std::map<unsigned, Pack> m_packs;
    std::deque<Request> m_requests;
    CImagePool m_images;
    std::function<void()> m_onRead;
};

// Historical ZMeshAssets description; model parsing now belongs to templates,
// image loading to this queue, and logging to resource owners.
/**
 * Fetch one model and the atlas it wears, and report what came out.
 *
 * The one place a mesh ordinal and an image ordinal turn into something
 * drawable.
 *
 * @param label Printed with the result, so a failure names its owner.
 */
