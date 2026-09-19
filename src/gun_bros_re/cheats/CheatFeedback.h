#pragma once
#include "engine/glu/movie/ZMovieRenderer.h"
#include <string>

namespace GameCheats {
/** User-created command feedback, independent of original menu resources. */
class CheatFeedback {
public:
    void Show(const std::string &message) { text = message; }
    void Clear() { text.clear(); }
    void Draw(ZMovieRenderer &movies) const {
        if (!text.empty()) { movies.Text(text, 450, 738, 5, 1.45f * 7 / 27.0f); }
    }
private:
    std::string text;
};
}
