/** @file HudText.h
 * @brief Small shared bitmap text for game HUD and research harnesses.
 */
#ifndef GUN_BROS_RE_HUDTEXT_H
#define GUN_BROS_RE_HUDTEXT_H
#include "engine/graphics/CMarkerBatch.h"
#include <string>

/** Tiny diagnostic font: five columns per glyph, low bit at the top. */
inline void DrawHudText(CMarkerBatch &batch, float x, float y, const std::string &text, float scale = 2) {
    static const unsigned char digits[10][5] = {
        {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},{24,20,18,127,16},
        {39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30}
    };
    static const unsigned char letters[26][5] = {
        {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
        {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
        {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
        {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
        {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
        {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
        {7,8,112,8,7},{97,81,73,69,67}
    };
    const float startX = x;
    for (char c : text) {
        if (c == '\n') { y += 10 * scale; x = startX; continue; }
        if (c >= 'a' && c <= 'z') { c -= 'a' - 'A'; }
        const unsigned char *glyph = nullptr;
        if (c >= '0' && c <= '9') { glyph = digits[c - '0']; }
        if (c >= 'A' && c <= 'Z') { glyph = letters[c - 'A']; }
        if (glyph != nullptr) {
            for (int column = 0; column < 5; ++column) {
                for (int row = 0; row < 7; ++row) {
                    if ((glyph[column] & (1 << row)) != 0) {
                        batch.AddRect(x + column * scale, y + row * scale, scale, scale);
                    }
                }
            }
        } else if (c == '-' || c == '_') { batch.AddRect(x, y + 3 * scale, 5 * scale, scale); }
        else if (c == '.' || c == ':') {
            batch.AddRect(x + 2 * scale, y + 6 * scale, scale, scale);
            if (c == ':') { batch.AddRect(x + 2 * scale, y + 2 * scale, scale, scale); }
        } else if (c == '+') {
            batch.AddRect(x, y + 3 * scale, 5 * scale, scale);
            batch.AddRect(x + 2 * scale, y + scale, scale, 5 * scale);
        } else if (c == '%') {
            batch.AddRect(x, y, 2 * scale, 2 * scale);
            batch.AddRect(x + 3 * scale, y + 5 * scale, 2 * scale, 2 * scale);
            for (int i = 0; i < 5; ++i) { batch.AddRect(x + i * scale, y + (5 - i) * scale, scale, scale); }
        } else if (c == '/') {
            for (int i = 0; i < 5; ++i) { batch.AddRect(x + i * scale, y + (5 - i) * scale, scale, scale); }
        }
        x += 6 * scale;
    }
}

#endif
