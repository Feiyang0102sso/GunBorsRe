/** @file CCamera.h
 * @brief Original scale interpolation; desktop framing owns the viewport factor.
 */
#ifndef GUN_BROS_RE_CCAMERA_H
#define GUN_BROS_RE_CCAMERA_H

class CCamera {
public:
    void SnapScale(float scale);
    void SetScale(float scale);
    float GetScale() const;
    void Update(int deltaMs);
private:
    float m_previousScale = 0.8f;
    float m_targetScale = 0.8f;
    float m_scaleProgress = 1.0f;
};
#endif
