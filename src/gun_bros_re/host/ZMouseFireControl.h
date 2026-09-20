#pragma once
#include <algorithm>
#include <cmath>

/** Desktop mouse gesture for the authored right stick; no world or UI actions. */
class ZMouseFireControl {
public:
    struct Geometry { float x = 0, y = 0, radius = 0; };

    void Update(const Geometry &geometry, float x, float y, bool down, bool canStart, bool enabled) {
        const bool pressed = down && !m_previousDown;
        m_previousDown = down;
        if (!enabled) {
            Cancel();
            m_requiresRelease = true;
            return;
        }
        // SDL reports no down button while unfocused. Do not interpret a held
        // button on focus restoration as a new press on the stick.
        if (m_requiresRelease) {
            if (!down) { m_requiresRelease = false; }
            Cancel();
            return;
        }
        if (!down) { Cancel(); return; }
        const float dx = x - geometry.x;
        const float dy = y - geometry.y;
        const float distance = std::hypot(dx, dy);
        // ControlStick::Update :249034 acquires within radius * 1.3 and
        // retains the touch outside that circle until release.
        if (pressed && canStart && distance <= geometry.radius * 1.3f) { m_captured = true; }
        if (!m_captured) { return; }
        m_x = 0;
        m_y = 0;
        // CInputPad::Bind :90846 supplies a one-unit dead zone. The desktop
        // adapter keeps the authored docked center throughout the gesture.
        if (distance <= 1.0f) { return; }
        const float strength = std::min(1.0f, (distance - 1.0f) / (geometry.radius - 1.0f));
        m_x = dx / distance * strength;
        m_y = dy / distance * strength;
    }

    // Preserve the down edge across menus; resuming requires a fresh press.
    void Cancel() { m_captured = false; m_x = 0; m_y = 0; }
    bool Captured() const { return m_captured; }
    bool Firing() const { return m_captured && (m_x != 0 || m_y != 0); }
    float X() const { return m_x; }
    float Y() const { return m_y; }

private:
    bool m_previousDown = false, m_captured = false;
    bool m_requiresRelease = false;
    float m_x = 0, m_y = 0;
};
