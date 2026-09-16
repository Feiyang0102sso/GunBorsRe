/**
 * @file ZSpriteArchetype.cpp
 * @brief One SpriteGlu character: its drawing tree and its atlas pages.
 */

#include "engine/glu/sprite/ZSpriteArchetype.h"

#include <cstdio>

namespace {

// The file stores step durations in tens of milliseconds.
constexpr std::uint16_t kDurationUnitMs = 10;

}  // namespace

ZSpriteArchetype::ZSpriteArchetype()
    : m_actionCount(0), m_declaredPageCount(0) {}

bool ZSpriteArchetype::InitDrawingTree(CArrayInputStream &stream,
                                          std::uint16_t spriteMapCount) {
    m_sprites.clear();
    m_frames.clear();
    m_animations.clear();
    m_actionCount = 0;

    // --- sprites: each one a list of sprite maps at offsets ---
    const std::uint16_t spriteCount = stream.ReadUInt16();
    m_sprites.resize(spriteCount);
    for (std::uint16_t i = 0; i < spriteCount; ++i) {
        const std::uint8_t partCount = stream.ReadUInt8();
        m_sprites[i].resize(partCount);

        for (std::uint8_t p = 0; p < partCount; ++p) {
            m_sprites[i][p].spriteMapIndex = stream.ReadUInt16();
            m_sprites[i][p].offsetX = stream.ReadInt16();
            m_sprites[i][p].offsetY = stream.ReadInt16();
        }
    }

    // --- frames: each one a list of sprites at offsets ---
    const std::uint16_t frameCount = stream.ReadUInt16();
    m_frames.resize(frameCount);
    for (std::uint16_t i = 0; i < frameCount; ++i) {
        const std::uint8_t partCount = stream.ReadUInt8();
        m_frames[i].resize(partCount);

        for (std::uint8_t p = 0; p < partCount; ++p) {
            m_frames[i][p].spriteIndex = stream.ReadUInt16();
            m_frames[i][p].offsetX = stream.ReadInt16();
            m_frames[i][p].offsetY = stream.ReadInt16();
        }
    }

    // --- animations: each one a list of (frame, duration) steps ---
    const std::uint16_t animationCount = stream.ReadUInt16();
    m_animations.resize(animationCount);
    for (std::uint16_t i = 0; i < animationCount; ++i) {
        m_animations[i].unknown0 = stream.ReadUInt8();

        const std::uint8_t stepCount = stream.ReadUInt8();
        m_animations[i].steps.resize(stepCount);

        for (std::uint8_t s = 0; s < stepCount; ++s) {
            m_animations[i].steps[s].frameIndex = stream.ReadUInt16();
            m_animations[i].steps[s].durationMs = stream.ReadUInt16() * kDurationUnitMs;
        }
    }

    // --- actions: a used-sprite-map bitmask each, which nothing reads back ---
    // The bitmask exists so LoadCharacter can work out which atlas pages an
    // action needs. Everything here loads every page, so it is stepped over.
    m_actionCount = stream.ReadUInt8();
    const std::size_t bitmaskBytes = (spriteMapCount + 7) / 8;
    for (std::uint32_t i = 0; i < m_actionCount; ++i) {
        stream.Skip(bitmaskBytes);
        stream.ReadUInt8();
    }

    if (stream.Overran()) {
        std::printf("[archetype] drawing tree truncated\n");
        return false;
    }
    if (stream.Available() != 0) {
        std::printf("[archetype] %zu bytes left after the drawing tree\n",
                    stream.Available());
    }

    return true;
}

bool ZSpriteArchetype::InitTextureMap(CArrayInputStream &stream) {
    m_rects.clear();
    m_rectIndices.clear();

    // Page formats: the original hands each one to the image loader. Every
    // page here is decoded as a plain PNG, so they are read and dropped.
    m_declaredPageCount = stream.ReadUInt8();
    stream.Skip(m_declaredPageCount);

    const std::uint16_t rectCount = stream.ReadUInt16();
    m_rects.resize(rectCount);
    for (std::uint16_t i = 0; i < rectCount; ++i) {
        // The page byte leads on the wire even though it is the last field.
        m_rects[i].page = stream.ReadUInt8();
        m_rects[i].x = stream.ReadUInt16();
        m_rects[i].y = stream.ReadUInt16();
        m_rects[i].width = stream.ReadUInt16();
        m_rects[i].height = stream.ReadUInt16();
    }

    const std::uint16_t indexCount = stream.ReadUInt16();
    m_rectIndices.resize(indexCount);
    for (std::uint16_t i = 0; i < indexCount; ++i) {
        m_rectIndices[i] = stream.ReadUInt16();
    }

    if (stream.Overran()) {
        std::printf("[archetype] texture map truncated\n");
        return false;
    }
    if (stream.Available() != 0) {
        std::printf("[archetype] %zu bytes left after the texture map\n",
                    stream.Available());
    }

    return true;
}

void ZSpriteArchetype::SetPages(std::vector<std::unique_ptr<ZTexture>> &&pages) {
    m_pages = std::move(pages);
}

const ZAtlasRect *ZSpriteArchetype::FindRect(std::uint16_t imageIndex) const {
    if (imageIndex >= m_rectIndices.size()) {
        return nullptr;
    }

    const std::uint16_t rectOrdinal = m_rectIndices[imageIndex];
    if (rectOrdinal >= m_rects.size()) {
        return nullptr;
    }

    return &m_rects[rectOrdinal];
}

const ZTexture *ZSpriteArchetype::GetPage(std::uint8_t page) const {
    if (page >= m_pages.size()) {
        return nullptr;
    }
    return m_pages[page].get();
}
