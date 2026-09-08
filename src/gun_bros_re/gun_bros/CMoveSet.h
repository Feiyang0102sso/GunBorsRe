/** @file CMoveSet.h
 * @brief Original sprite move table (CMoveSet::Init :99445).
 */
#ifndef GUN_BROS_RE_CMOVESET_H
#define GUN_BROS_RE_CMOVESET_H
#include "engine/CArrayInputStream.h"
#include <vector>

class CMoveSet {
public:
    struct Frame {
        std::uint8_t first = 0;
        std::uint8_t second = 0;
        std::uint8_t third = 0;
        std::uint8_t sound = 255;
    };
    struct Move {
        std::uint8_t animation = 255;
        bool looping = false;
        std::uint8_t field16 = 0;
        std::uint8_t field20 = 0;
        std::vector<Frame> frames;
    };
    bool Init(CArrayInputStream &stream);
    std::uint32_t packHash = 0;
    std::uint8_t archetype = 255;
    std::uint8_t action = 255;
    std::vector<Move> moves;
};
#endif
