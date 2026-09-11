/** Original menu mesh touch rotation, independent of the Windows input adapter. */
#pragma once

class CMenuMesh {
public:
    /** HandleTouchInput :168943 and UpdateRotation :168846; degrees internally.
     * Bounds are supplied by the BIG Movie region, never a second layout table. */
    void UpdateRotation(unsigned deltaMs, bool down, float touchX, float touchY,
        float x, float y, float width, float height, bool enabled) {
        const int currentX = static_cast<int>(touchX);
        const int currentY = static_cast<int>(touchY);
        if (down && !previousDown) {
            startX = currentX;
            dragging = enabled && width != 0 && height != 0 &&
                currentX >= x && currentX <= x + width && currentY >= y && currentY <= y + height;
        }
        if (!down) { dragging = false; }
        previousDown = down;
        if (dragging) {
            degrees = (startX - currentX) / width * 180.0f;
            if (degrees < 0) { degrees += 360.0f; }
        } else if (degrees != 0) {
            float step = static_cast<float>(deltaMs >> 1);
            if (degrees <= 180) { step = -step; }
            degrees += step;
            if (degrees < 0 || degrees > 360) { degrees = 0; }
        }
    }

    float GetDegrees() const { return degrees; }
    float GetRadians() const { return degrees * 3.14159265358979323846f / 180.0f; }

private:
    float degrees = 0;
    int startX = 0;
    bool dragging = false;
    bool previousDown = false;
};
