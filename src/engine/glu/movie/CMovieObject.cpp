#include "engine/glu/movie/CMovieObject.h"
#include <cstdio>

namespace {
void ReadTransform(CArrayInputStream &stream, ZMovieKeyFrame &frame) {
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

// Original CMovie::InitResource selects each object decoder; all byte widths
// and field orders below remain those of the corresponding CMovie*::Init.
bool CMovieObject::Init(CArrayInputStream &stream) {
    frames.clear();
    type = stream.ReadUInt8();
    const unsigned count = stream.ReadUInt16();
    if (count > stream.Available() / 4) { return false; }
    for (unsigned key = 0; key < count; ++key) {
        ZMovieKeyFrame frame;
        frame.time = stream.ReadUInt32();
        if (type == 0 || type == 1 || type == 6 || type == 7) {
            ReadTransform(stream, frame);
            if (type != 0) {
                // Serialized as words, consumed as signed short by
                // CMovieEmptyRegion::GetMetricsAtTime :182420.
                frame.width = static_cast<std::int16_t>(stream.ReadUInt16());
                frame.height = static_cast<std::int16_t>(stream.ReadUInt16());
            }
            for (std::uint8_t &value : frame.content) { value = stream.ReadUInt8(); }
            if (type == 0) { frame.visible = stream.ReadUInt8() != 0; }
            if (type == 6) {
                frame.region = stream.ReadUInt8(); // Region callback type, not visibility.
                frame.visible = frame.content[0] != 0;
            }
            if (type == 1) {
                // Tiled sprite stores another four bytes, then two 16.16 offsets.
                for (std::uint8_t &value : frame.tiledSprite) { value = stream.ReadUInt8(); }
                frame.tileX = stream.ReadInt32();
                frame.tileY = stream.ReadInt32();
                frame.visible = frame.content[0] != 0;
            }
            if (type == 7) {
                for (std::uint8_t &color : frame.colors) { color = stream.ReadUInt8(); }
                frame.visible = frame.content[0] != 0;
            }
        } else if (type == 3) {
            frame.font = stream.ReadUInt16();
            frame.text = stream.ReadUInt16();
            frame.region = stream.ReadUInt16();
            frame.content[0] = stream.ReadUInt8();
            frame.visible = stream.ReadUInt8() != 0;
        } else if (type == 2 || type == 8) {
            frame.content[0] = stream.ReadUInt8();
            frame.content[1] = stream.ReadUInt8();
        } else if (type == 5) {
            // Chapter track timestamps are also available to the parent movie.
        } else {
            std::printf("[movie] unsupported object type=%u\n", type);
            return false;
        }
        if (!frames.empty() && frame.time < frames.back().time) { return false; }
        frames.push_back(frame);
    }
    return !stream.Overran();
}
