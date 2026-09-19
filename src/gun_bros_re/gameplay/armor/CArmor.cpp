/**
 * @file CArmor.cpp
 * @brief The armour template: two model variants, one per brother.
 */

#include "gun_bros_re/gameplay/armor/CArmor.h"

#include <cstdio>
#include "gun_bros_re/gameplay/armor/CArmorDrawing.h"

CArmor::Template::Template() : m_slot(0), m_attachmentNode{} {}

bool CArmor::Template::Init(CArrayInputStream &stream) {
    // The wire order interleaves the variants with their flags, so this does
    // not loop: mesh and image of variant 0, a flag, then variant 1, a flag,
    // then both sprite fallbacks together.
    m_slot = stream.ReadUInt8();
    m_meshRef[0].Init(stream);
    m_imageRef[0].Init(stream);
    m_attachmentNode[0] = stream.ReadUInt8();
    m_meshRef[1].Init(stream);
    m_imageRef[1].Init(stream);
    m_attachmentNode[1] = stream.ReadUInt8();
    m_spriteImageRef[0].Init(stream);
    m_spriteImageRef[1].Init(stream);

    m_script.Load(stream);

    if (stream.Overran()) {
        std::printf("[armor] template truncated\n");
        return false;
    }

    return true;
}

CArmor::CArmor() = default;
CArmor::~CArmor() = default;

void CArmor::Bind(const Template &data) {
    for (std::int16_t &attribute : m_attributes) {
        attribute = 0;
    }
    m_templateData = data;
    m_interpreter.SetScript(m_templateData.GetScript(), *this);
}

void CArmor::Equip() {
    // Interpreter return indicates script control flow, not execution success.
    m_interpreter.CallExportFunction(0);
}

std::int16_t *CArmor::VariableResolver(std::uint8_t variable) {
    if (variable >= kArmorAttributeCount) {
        std::printf("[armor] unknown variable %u\n", variable);
        return nullptr;
    }
    return &m_attributes[variable];
}

std::int16_t CArmor::GetAttribute(std::uint32_t index) const {
    if (index >= kArmorAttributeCount) {
        return 0;
    }
    return m_attributes[index];
}
