/**
 * @file CLevel.cpp
 * @brief A level: a map plus the script that drives it.
 */

#include "gun_bros/CLevel.h"

#include "gun_bros/CMap.h"

#include <cstdio>

CLevel::Template::Template() : unknown0(0), unknown1(0), unknown2(0) {}

bool CLevel::Template::Init(CArrayInputStream &stream) {
    mapRef.Init(stream);
    script.Load(stream);
    unknown0 = stream.ReadUInt16();
    unknown1 = stream.ReadUInt16();
    unknown2 = stream.ReadUInt16();

    return !stream.Overran();
}

CLevel::CLevel() : m_template(nullptr), m_map(nullptr), m_unimplementedCalls(0) {
    for (std::uint32_t i = 0; i < kLevelVariableCount; ++i) {
        m_variables[i] = 0;
    }
}

void CLevel::Bind(const Template &levelTemplate, CMap &map) {
    m_template = &levelTemplate;
    m_map = &map;

    if (!levelTemplate.script.IsPresent()) {
        return;
    }

    m_interpreter.SetScript(levelTemplate.script, *this);
    m_interpreter.CallExportFunction(kLevelExportOnLevelStart);
}

std::int16_t CLevel::FunctionResolver(std::uint8_t function, const std::int16_t *arguments,
                                      std::uint8_t argumentCount) {
    if (function == kLevelFunctionSetCameraLayer) {
        SetCameraLayer(arguments, argumentCount);
        return 0;
    }

    if (function == kLevelFunctionSetCollisionLayer) {
        SetCollisionLayer(arguments, argumentCount);
        return 0;
    }
    if (function == 6 && m_map != nullptr && argumentCount > 0) {
        if (!m_map->SetBulletCollisionLayer(arguments[0])) {
            std::printf("[level] invalid bullet collision layer %d\n", arguments[0]);
        } else {
            std::printf("[level] setBulletCollisionLayer( %d )\n", arguments[0]);
        }
        return 0;
    }

    if (function == kLevelFunctionSetTileLayerSpeed) {
        SetTileLayerSpeed(arguments, argumentCount);
        return 0;
    }

    m_unimplementedCalls += 1;
    std::printf("[level] function %u, %u args:", function, argumentCount);
    for (std::uint8_t i = 0; i < argumentCount; ++i) {
        std::printf(" %d", arguments[i]);
    }
    std::printf(" -- not implemented\n");
    return 0;
}

std::int16_t *CLevel::VariableResolver(std::uint8_t variable) {
    if (variable >= kLevelVariableCount) {
        return nullptr;
    }

    return &m_variables[variable];
}

void CLevel::SetCameraLayer(const std::int16_t *arguments, std::uint8_t argumentCount) {
    if (m_map == nullptr || argumentCount < 1) {
        return;
    }

    // The argument indexes the whole layer stack, not the camera layers alone.
    const std::int16_t layerIndex = arguments[0];
    if (layerIndex < 0 ||
        !m_map->SetCameraLayer(static_cast<std::uint32_t>(layerIndex))) {
        std::printf("[level] setCameraLayer( %d ) names no camera layer\n", layerIndex);
        return;
    }

    std::printf("[level] setCameraLayer( %d )\n", layerIndex);
}

void CLevel::SetCollisionLayer(const std::int16_t *arguments,
                               std::uint8_t argumentCount) {
    if (m_map == nullptr || argumentCount < 1) {
        return;
    }

    // Like the camera selector, this indexes the complete map layer stack.
    const std::int16_t layerIndex = arguments[0];
    if (layerIndex < 0 ||
        !m_map->SetCollisionLayer(static_cast<std::uint32_t>(layerIndex))) {
        std::printf("[level] setCollisionLayer( %d ) names no collision layer\n",
                    layerIndex);
        return;
    }

    std::printf("[level] setCollisionLayer( %d )\n", layerIndex);
}

void CLevel::SetTileLayerSpeed(const std::int16_t *arguments, std::uint8_t argumentCount) {
    if (m_map == nullptr || argumentCount < 3) {
        return;
    }

    // The layer index counts tile layers only: the original walks the map's
    // whole layer stack and skips everything whose type is not zero, which is
    // the same list CMap keeps.
    const std::int16_t layerOrdinal = arguments[0];
    if (layerOrdinal < 0 ||
        static_cast<std::uint32_t>(layerOrdinal) >= m_map->GetTileLayerCount()) {
        std::printf("[level] setTileLayerSpeed names tile layer %d; the map has %u\n",
                    layerOrdinal, m_map->GetTileLayerCount());
        return;
    }

    const float speedX = static_cast<float>(arguments[1]) * kLevelSpeedArgumentUnit;
    const float speedY = static_cast<float>(arguments[2]) * kLevelSpeedArgumentUnit;

    m_map->GetTileLayer(static_cast<std::uint32_t>(layerOrdinal)).SetSpeed(speedX, speedY);

    std::printf("[level] setTileLayerSpeed( %d, %.4f, %.4f )\n", layerOrdinal, speedX,
                speedY);
}
