#pragma once
#include "gun_bros_re/ui/hud/ZHudState.h"
#include <cmath>

/** Desktop screen fire owns a press; combat buttons commit only on a click. */
class ZMouseScreenControl {
public:
    ZInputPadAction Update(float x, float y, bool down, ZInputPadAction button,
                          bool enabled, float pixelsPerHudX, float pixelsPerHudY) {
        const bool pressed = down && !m_previousDown;
        const bool released = !down && m_previousDown;
        m_previousDown = down;
        if (!enabled) {
            Cancel();
            m_requiresRelease = true;
            return ZInputPadAction::None;
        }
        if (m_requiresRelease) {
            if (!down) { m_requiresRelease = false; }
            return ZInputPadAction::None;
        }
        if (pressed) {
            m_button = button;
            m_fire = button == ZInputPadAction::None;
            m_pressX = x;
            m_pressY = y;
            m_clickCancelled = false;
        }
        if (m_button != ZInputPadAction::None) {
            // Desktop click tolerance, in drawable pixels; not original asset data.
            constexpr float kClickDragPixels = 6;
            const float dx = (x - m_pressX) * pixelsPerHudX;
            const float dy = (y - m_pressY) * pixelsPerHudY;
            if (button != m_button || std::hypot(dx, dy) > kClickDragPixels) { m_clickCancelled = true; }
        }
        ZInputPadAction action = ZInputPadAction::None;
        if (released) {
            if (!m_clickCancelled && button == m_button) { action = m_button; }
            Cancel();
        }
        return action;
    }

    void Cancel() { m_fire = false; m_button = ZInputPadAction::None; m_clickCancelled = true; }
    bool Firing() const { return m_fire; }

private:
    bool m_previousDown = false, m_requiresRelease = false;
    bool m_fire = false, m_clickCancelled = true;
    ZInputPadAction m_button = ZInputPadAction::None;
    float m_pressX = 0, m_pressY = 0;
};
