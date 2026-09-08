/** @file CMovie.cpp
 * @brief Parse all eight movie object types present in the iOS implementation.
 */
#include "gun_bros/CMovie.h"
#include <cstdio>

namespace {
void ReadTransform(CArrayInputStream &stream, MovieKeyFrame &frame) {
    frame.x = stream.ReadInt16();
    frame.y = stream.ReadInt16();
    frame.alpha = stream.ReadInt32() / 65536.0f;
    frame.scaleX = stream.ReadInt32() / 65536.0f;
    frame.scaleY = stream.ReadInt32() / 65536.0f;
    frame.rotation = stream.ReadInt32() / 65536.0f;
    frame.layer = stream.ReadUInt8();
    frame.selfAnchor = stream.ReadUInt8();
    frame.parentAnchor = stream.ReadUInt8();
    frame.parent = stream.ReadUInt8();
}
}

bool CMovie::Init(CArrayInputStream &stream) {
    objects.clear();
    chapters.clear();
    width = stream.ReadUInt16();
    height = stream.ReadUInt16();
    duration = stream.ReadUInt32();
    const unsigned objectCount = stream.ReadUInt16();
    if (width == 0 || height == 0 || width > 4096 || height > 4096 || objectCount > 255) { return false; }
    for (unsigned index = 0; index < objectCount; ++index) {
        MovieObject object;
        object.type = stream.ReadUInt8();
        const unsigned count = stream.ReadUInt16();
        if (count > stream.Available() / 4) { return false; }
        if (object.type == 5) { chapters.push_back(0); }
        for (unsigned key = 0; key < count; ++key) {
            MovieKeyFrame frame;
            frame.time = stream.ReadUInt32();
            if (object.type == 0 || object.type == 1 || object.type == 6 || object.type == 7) {
                ReadTransform(stream, frame);
                if (object.type != 0) {
                    // Serialized as words, consumed as signed short by
                    // CMovieEmptyRegion::GetMetricsAtTime :182420.
                    frame.width = static_cast<std::int16_t>(stream.ReadUInt16());
                    frame.height = static_cast<std::int16_t>(stream.ReadUInt16());
                }
                for (std::uint8_t &value : frame.content) { value = stream.ReadUInt8(); }
                if (object.type == 0) { frame.visible = stream.ReadUInt8() != 0; }
                if (object.type == 6) {
                    frame.region = stream.ReadUInt8(); // Region callback type, not visibility.
                    frame.visible = frame.content[0] != 0;
                }
                if (object.type == 1) {
                    // Tiled sprite stores another four bytes, then two 16.16 offsets.
                    for (std::uint8_t &value : frame.tiledSprite) { value = stream.ReadUInt8(); }
                    frame.tileX = stream.ReadInt32();
                    frame.tileY = stream.ReadInt32();
                    frame.visible = frame.content[0] != 0;
                }
                if (object.type == 7) {
                    for (std::uint8_t &color : frame.colors) { color = stream.ReadUInt8(); }
                    frame.visible = frame.content[0] != 0;
                }
            } else if (object.type == 3) {
                frame.font = stream.ReadUInt16();
                frame.text = stream.ReadUInt16();
                frame.region = stream.ReadUInt16();
                frame.content[0] = stream.ReadUInt8();
                frame.visible = stream.ReadUInt8() != 0;
            } else if (object.type == 2 || object.type == 8) {
                frame.content[0] = stream.ReadUInt8();
                frame.content[1] = stream.ReadUInt8();
            } else if (object.type == 5) {
                chapters.push_back(frame.time);
            } else {
                std::printf("[movie] unsupported object type=%u object=%u\n", object.type, index);
                return false;
            }
            if (!object.frames.empty() && frame.time < object.frames.back().time) { return false; }
            object.frames.push_back(frame);
        }
        objects.push_back(std::move(object));
    }
    return !stream.Overran() && stream.Available() == 0;
}
