/** @file CCamera.h
 * @brief Original scale interpolation; desktop framing owns the viewport factor.
 */
#ifndef GUN_BROS_RE_CCAMERA_H
#define GUN_BROS_RE_CCAMERA_H
#include <cstdint>

class CCamera {
public:
    // Desktop viewport origin and pixels per world unit; not serialized map data.
    struct Viewport { float x = 0, y = 0, zoom = 1; };
    void SnapScale(float scale);
    void SetScale(float scale);
    float GetScale() const;
    void Update(int deltaMs);
    void Reset(float scale = 0.8f);
    void SetCameraMode(int mode);
    void SetTarget(float x, float y);
    void SetTargetToPlayer();
    void Shake(int durationMs);
    void UpdatePosition(float playerX, float playerY, float left, float top, float width, float height,
        float viewWidth, float viewHeight);
    bool HasPosition() const { return m_hasPosition; }
    float GetX() const { return m_x; }
    float GetY() const { return m_y; }
    int GetMode() const { return m_mode; }
    int GetShakeTime() const { return m_shakeMs; }
private:
    float RandomShake(float previous);
    float m_previousScale = 0.8f;
    float m_targetScale = 0.8f;
    float m_scaleProgress = 1.0f;
    float m_x = 0, m_y = 0, m_startX = 0, m_startY = 0, m_targetX = 0, m_targetY = 0;
    float m_shakeX = 0, m_shakeY = 0;
    int m_mode = 0, m_shakeMs = 0;
    bool m_hasPosition = false, m_targetPlayer = false;
    std::uint32_t m_random = 0xCA4E1234;
};
#endif
