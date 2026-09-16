/**
 * @file CSpriteGlu.cpp
 * @brief One pack's SpriteGlu system: the shared tables and its archetypes.
 */

#include "engine/glu/sprite/CSpriteGlu.h"

#include "engine/graphics/ZPNG.h"

#include <cstdio>

const char *const kSpriteGluGlobalName = "SPRITEGLU__BINARY_GLOBAL";
const char *const kSpriteGluArchetypeBaseName = "SPRITEGLU__BINARY_ARCHETYPE_000";
const char *const kTextureMapGlobalName = "TEXTURE_MAP_GLOBAL";
const char *const kBaseTextureMapName = "BASE_TEXTURE_MAP";
const char *const kBaseTexturePageName = "BASE_TEXTURE_PAGE_0";

CSpriteGlu::CSpriteGlu()
    : m_pack(nullptr), m_initialised(false), m_archetypeCount(0) {}

CSpriteGlu::~CSpriteGlu() = default;

bool CSpriteGlu::Init(CResPackTOC &pack) {
    m_pack = &pack;
    m_initialised = false;

    if (!ReadGlobalTables(pack)) {
        return false;
    }
    if (!ReadPageCounts(pack)) {
        return false;
    }

    m_archetypes.resize(m_archetypeCount);
    m_archetypeFailed.assign(m_archetypeCount, false);

    m_initialised = true;
    return true;
}

bool CSpriteGlu::ReadGlobalTables(CResPackTOC &pack) {
    m_imageSlots.clear();
    m_spriteMaps.clear();
    m_archetypeCount = 0;

    std::vector<std::uint8_t> payload;
    const std::uint32_t handle = pack.GetResValue(kSpriteGluGlobalName);
    if (handle == 0 || !pack.GetResource(handle, payload)) {
        std::printf("[spriteglu] %s: no %s\n",
                    pack.GetShortName().c_str(), kSpriteGluGlobalName);
        return false;
    }

    CArrayInputStream stream(payload);

    // Texture pack names. The original keeps them to build resource names;
    // this port addresses pages by handle, so they are only stepped over.
    const std::uint8_t texturePackCount = stream.ReadUInt8();
    for (std::uint8_t i = 0; i < texturePackCount; ++i) {
        const std::uint16_t nameLength = stream.ReadUInt16();
        stream.Skip(nameLength);
    }

    stream.ReadUInt16();  // read and discarded by the original too

    const std::uint16_t imageSlotCount = stream.ReadUInt16();
    m_imageSlots.resize(imageSlotCount);
    for (std::uint16_t i = 0; i < imageSlotCount; ++i) {
        m_imageSlots[i] = stream.ReadUInt16();
        stream.ReadUInt8();
    }

    const std::uint16_t spriteMapCount = stream.ReadUInt16();
    m_spriteMaps.resize(spriteMapCount);
    std::uint32_t additiveCount = 0;
    std::uint32_t additiveOpaqueCount = 0;
    for (std::uint16_t i = 0; i < spriteMapCount; ++i) {
        // Written slot, transform, blend -- but stored blend, slot, transform,
        // which is why the blend byte reads like the last field and is not.
        m_spriteMaps[i].imageSlot = stream.ReadUInt16();
        m_spriteMaps[i].transform = stream.ReadUInt8();
        m_spriteMaps[i].blendFlags = stream.ReadUInt8();

        if ((m_spriteMaps[i].blendFlags & kSpriteMapBlendAdditiveOpaque) != 0) {
            additiveOpaqueCount++;
        } else if ((m_spriteMaps[i].blendFlags & kSpriteMapBlendAdditive) != 0) {
            additiveCount++;
        }
    }

    // Primitives -- coloured rectangles rather than images. A sprite map index
    // at or past the sprite map count names one of these. Only pack7 has any,
    // and CSpriteIterator reports them rather than drawing them.
    const std::uint16_t primitiveCount = stream.ReadUInt16();
    m_primitives.clear();
    m_primitives.resize(primitiveCount);
    for (std::uint16_t i = 0; i < primitiveCount; ++i) {
        Primitive &primitive = m_primitives[i];
        primitive.color = stream.ReadUInt32();  // colour
        primitive.width = stream.ReadUInt16();  // width
        primitive.height = stream.ReadUInt16();  // height
        primitive.type = stream.ReadUInt8();
        std::printf("[spriteglu] primitive=%u type=%u color=%08x size=%ux%u\n", i,
            primitive.type, primitive.color, primitive.width, primitive.height);
    }

    // Sprite map substitution groups, used to swap a character's guns and
    // armour at bind time. Every pack that holds maps declares zero of them.
    const std::uint8_t substitutionGroups = stream.ReadUInt8();
    for (std::uint8_t g = 0; g < substitutionGroups; ++g) {
        const std::uint16_t entryCount = stream.ReadUInt16();
        stream.Skip(static_cast<std::size_t>(entryCount) * 9);
    }

    m_archetypeCount = stream.ReadUInt8();

    if (stream.Overran()) {
        std::printf("[spriteglu] %s: global tables truncated\n",
                    pack.GetShortName().c_str());
        return false;
    }
    if (stream.Available() != 0) {
        std::printf("[spriteglu] %s: %zu bytes left after the global tables\n",
                    pack.GetShortName().c_str(), stream.Available());
    }

    std::printf("[spriteglu] %s: %u archetypes, %zu sprite maps "
                "(%u additive, %u additive-opaque)\n",
                pack.GetShortName().c_str(), m_archetypeCount, m_spriteMaps.size(),
                additiveCount, additiveOpaqueCount);

    return true;
}

bool CSpriteGlu::ReadPageCounts(CResPackTOC &pack) {
    m_pagesPerArchetype.clear();

    std::vector<std::uint8_t> payload;
    const std::uint32_t handle = pack.GetResValue(kTextureMapGlobalName);
    if (handle == 0 || !pack.GetResource(handle, payload)) {
        std::printf("[spriteglu] %s: no %s\n",
                    pack.GetShortName().c_str(), kTextureMapGlobalName);
        return false;
    }

    CArrayInputStream stream(payload);
    stream.ReadUInt16();  // read and discarded by the original too

    m_pagesPerArchetype.resize(m_archetypeCount);
    for (std::uint8_t i = 0; i < m_archetypeCount; ++i) {
        m_pagesPerArchetype[i] = stream.ReadUInt8();
    }

    if (stream.Overran()) {
        std::printf("[spriteglu] %s: page counts truncated\n",
                    pack.GetShortName().c_str());
        return false;
    }

    return true;
}

const ZSpriteArchetype *CSpriteGlu::GetArchetype(std::uint8_t index) {
    if (!m_initialised || index >= m_archetypeCount) {
        return nullptr;
    }
    if (m_archetypes[index]) {
        return m_archetypes[index].get();
    }
    if (m_archetypeFailed[index]) {
        return nullptr;
    }

    std::unique_ptr<ZSpriteArchetype> archetype(new ZSpriteArchetype());
    std::vector<std::uint8_t> payload;

    const std::uint32_t treeHandle =
        m_pack->GetResValue(kSpriteGluArchetypeBaseName) + index;
    if (!m_pack->GetResource(treeHandle, payload)) {
        std::printf("[spriteglu] %s: archetype %u unreadable\n",
                    m_pack->GetShortName().c_str(), index);
        m_archetypeFailed[index] = true;
        return nullptr;
    }

    CArrayInputStream treeStream(payload);
    if (!archetype->InitDrawingTree(treeStream, GetSpriteMapCount())) {
        m_archetypeFailed[index] = true;
        return nullptr;
    }

    const std::uint32_t mapHandle = m_pack->GetResValue(kBaseTextureMapName) + index;
    if (!m_pack->GetResource(mapHandle, payload)) {
        std::printf("[spriteglu] %s: archetype %u has no texture map\n",
                    m_pack->GetShortName().c_str(), index);
        m_archetypeFailed[index] = true;
        return nullptr;
    }

    CArrayInputStream mapStream(payload);
    if (!archetype->InitTextureMap(mapStream)) {
        m_archetypeFailed[index] = true;
        return nullptr;
    }

    if (!LoadPages(index, *archetype)) {
        m_archetypeFailed[index] = true;
        return nullptr;
    }

    std::printf("[spriteglu] %s archetype %u: %u sprites, %u frames, "
                "%u anims, %u rects, %u pages\n",
                m_pack->GetShortName().c_str(), index,
                archetype->GetSpriteCount(), archetype->GetFrameCount(),
                archetype->GetAnimationCount(), archetype->GetRectCount(),
                archetype->GetPageCount());

    m_archetypes[index] = std::move(archetype);
    return m_archetypes[index].get();
}

bool CSpriteGlu::LoadPages(std::uint8_t index, ZSpriteArchetype &archetype) {
    // Pages are one flat run across the whole pack, so an archetype's first
    // page is the sum of everything before it. Nothing in the data says this;
    // it comes from CSpriteGlu::LoadTexturePack.
    std::uint32_t pageBase = 0;
    for (std::uint8_t i = 0; i < index; ++i) {
        pageBase += m_pagesPerArchetype[i];
    }
    const std::uint8_t pageCount = m_pagesPerArchetype[index];

    if (pageCount != archetype.GetDeclaredPageCount()) {
        std::printf("[spriteglu] %s archetype %u: %s says %u pages, "
                    "%s says %u\n",
                    m_pack->GetShortName().c_str(), index,
                    kTextureMapGlobalName, pageCount,
                    kBaseTextureMapName, archetype.GetDeclaredPageCount());
    }

    const std::uint32_t firstPageHandle =
        m_pack->GetResValue(kBaseTexturePageName) + pageBase;

    std::vector<std::unique_ptr<ZTexture>> pages;
    std::vector<std::uint8_t> payload;

    for (std::uint8_t page = 0; page < pageCount; ++page) {
        if (!m_pack->GetResource(firstPageHandle + page, payload)) {
            std::printf("[spriteglu] %s archetype %u: page %u unreadable\n",
                        m_pack->GetShortName().c_str(), index, page);
            return false;
        }

        ZPNGImage decoded;
        if (!PNGDecode(payload, decoded)) {
            std::printf("[spriteglu] %s archetype %u: page %u is not a PNG\n",
                        m_pack->GetShortName().c_str(), index, page);
            return false;
        }

        std::unique_ptr<ZTexture> texture(new ZTexture());
        if (!texture->Create(decoded)) {
            return false;
        }
        pages.push_back(std::move(texture));
    }

    archetype.SetPages(std::move(pages));
    return true;
}

const ZTexture *CSpriteGlu::GetPrimitiveTexture(std::uint16_t spriteMapIndex) const {
    if (spriteMapIndex < m_spriteMaps.size()) { return nullptr; }
    const unsigned index = spriteMapIndex - static_cast<unsigned>(m_spriteMaps.size());
    if (index >= m_primitives.size()) { return nullptr; }
    const Primitive &primitive = m_primitives[index];
    if (primitive.type == 17 || primitive.width == 0 || primitive.height == 0) { return nullptr; }
    if (!primitive.texture) {
        // CSpritePlayer::Draw :59246 uses the low RGB24, replacing stored alpha
        // with the player's current alpha. The quad applies that alpha later.
        ZPNGImage pixels;
        pixels.width = primitive.width;
        pixels.height = primitive.height;
        pixels.pixels.resize(static_cast<std::size_t>(pixels.width) * pixels.height * 4);
        for (std::size_t offset = 0; offset < pixels.pixels.size(); offset += 4) {
            pixels.pixels[offset] = static_cast<std::uint8_t>(primitive.color >> 16);
            pixels.pixels[offset + 1] = static_cast<std::uint8_t>(primitive.color >> 8);
            pixels.pixels[offset + 2] = static_cast<std::uint8_t>(primitive.color);
            pixels.pixels[offset + 3] = 255;
        }
        auto texture = std::make_unique<ZTexture>();
        if (!texture->Create(pixels)) { return nullptr; }
        primitive.texture = std::move(texture);
    }
    return primitive.texture.get();
}

bool CSpriteGlu::ResolveImageIndex(std::uint16_t spriteMapIndex,
                                   std::uint16_t &imageIndex) const {
    // Past the end of the sprite map table means a primitive, not an image.
    if (spriteMapIndex >= m_spriteMaps.size()) {
        return false;
    }

    const std::uint16_t slot = m_spriteMaps[spriteMapIndex].imageSlot;
    if (slot >= m_imageSlots.size()) {
        return false;
    }

    imageIndex = m_imageSlots[slot];
    return true;
}

std::uint8_t CSpriteGlu::GetSpriteMapTransform(std::uint16_t spriteMapIndex) const {
    if (spriteMapIndex >= m_spriteMaps.size()) {
        return 0;
    }
    return m_spriteMaps[spriteMapIndex].transform;
}

std::uint8_t CSpriteGlu::GetSpriteMapBlendFlags(std::uint16_t spriteMapIndex) const {
    if (spriteMapIndex >= m_spriteMaps.size()) {
        return 0;
    }
    return m_spriteMaps[spriteMapIndex].blendFlags;
}
