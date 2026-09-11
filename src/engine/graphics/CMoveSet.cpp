/** @file CMoveSet.cpp
 * @brief Preserve unknown frame fields, and consume the complete move payload.
 */
#include "engine/graphics/CMoveSet.h"
#include <cstdio>

bool CMoveSet::Init(CArrayInputStream &stream) {
    packHash = stream.ReadUInt32();
    archetype = stream.ReadUInt8();
    action = stream.ReadUInt8();
    const unsigned count = stream.ReadUInt8();
    moves.clear();
    // The archive writer retains a zero allocation word even with no moves;
    // iOS returns before reading it. Consume it so the record stays auditable.
    const unsigned allocatedFrames = stream.ReadUInt16();
    if (count == 0) { return !stream.Overran() && allocatedFrames == 0; }
    unsigned totalFrames = 0;
    for (unsigned index = 0; index < count; ++index) {
        Move move;
        move.animation = stream.ReadUInt8();
        move.looping = stream.ReadUInt8() != 0;
        move.field16 = stream.ReadUInt8();
        move.field20 = stream.ReadUInt8();
        const unsigned frames = stream.ReadUInt8();
        for (unsigned frameIndex = 0; frameIndex < frames; ++frameIndex) {
            Frame frame;
            frame.first = stream.ReadUInt8();
            frame.second = stream.ReadUInt8();
            frame.third = stream.ReadUInt8();
            frame.sound = stream.ReadUInt8();
            move.frames.push_back(frame);
        }
        totalFrames += frames;
        moves.push_back(std::move(move));
    }
    if (totalFrames != allocatedFrames) {
        std::printf("[moveset] frame storage allocated=%u used=%u\n", allocatedFrames, totalFrames);
    }
    return !stream.Overran() && totalFrames <= allocatedFrames;
}
