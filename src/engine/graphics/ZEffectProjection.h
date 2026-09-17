#pragma once
#include <algorithm>
#include <cmath>

/** Windows scene/preview billboard projection.
 * Billboards keep their authored size while position and travel use world units.
 */
struct ZEffectProjection {
    static constexpr float kRadians = 3.14159265f / 180.0f;
    const float *matrix = nullptr;
    float scale = 1;

    explicit ZEffectProjection(const float *projection) : matrix(projection) {
        if (matrix != nullptr) {
            const float x = std::hypot(matrix[0], matrix[4]);
            const float y = std::hypot(matrix[1], matrix[5]);
            const float z = std::hypot(matrix[2], matrix[6]);
            scale = std::max(x, std::max(y, z));
        }
    }
    void Position(float &x, float &y, float z) const {
        if (matrix == nullptr) { return; }
        const float originalX = x;
        x = matrix[0] * originalX + matrix[1] * y + matrix[2] * z + matrix[3];
        y = matrix[4] * originalX + matrix[5] * y + matrix[6] * z + matrix[7];
    }
    float Direction(float degrees) const {
        if (matrix == nullptr) { return degrees; }
        const float x = std::cos(degrees * kRadians), y = std::sin(degrees * kRadians);
        const float dx = matrix[0] * x + matrix[1] * y;
        const float dy = matrix[4] * x + matrix[5] * y;
        if (std::hypot(dx, dy) < scale * 0.0001f) { return -90; }
        return std::atan2(dy, dx) / kRadians;
    }
};
