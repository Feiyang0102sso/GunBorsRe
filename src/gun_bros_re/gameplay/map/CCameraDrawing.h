#pragma once
/** Desktop projection around the original CCamera position and scale. */
#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
namespace MapDetail {

const char *const kShaderDirectory = Paths::Shaders().c_str();

// GameView uses a fixed 4:3 world view independent of window pixels and map
// bounds. The framing matches the default stage, not an original engine constant.
constexpr float kGameViewWorldWidth = 572.0f;
constexpr float kGameViewWorldHeight = 429.0f;
// CBrother::Bind stores 22.0 as the player diameter. CPlayer::Move passes half
// of it to CLayerCollision, whose half-unit edge allowance makes 11.5.
// Source correction: :139098 initializes a RADIUS of 22, not a diameter;
// only wall collision halves it. Player/enemy circles must use GetRadius().
constexpr float kPlayerCollisionRadius = 11.5f;
constexpr float kRadiansToDegrees = 180.0f / 3.14159265f;

/**
 * Z-order groups, from CProp::GetZOrderGroup.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:123406
 *
 * The render queue sorts on these before it sorts on y, so a prop that only
 * has a background sprite sits behind every prop that has a main one, however
 * far down the screen it is. The map is group 0 with a z of -100000, which is
 * why the tiles simply go in first rather than being sorted with the props.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:92795 (CMap::GetZOrder)
 */
constexpr int kZGroupBackgroundOnly = 0;
constexpr int kZGroupNormal = 3;
constexpr int kZGroupForegroundOnly = 6;
void ProjectEnemyHealthBars(std::vector<CLevel::HealthBar> &bars,
    float cameraX, float cameraY, float zoom, int width, int height);
void FollowPlayerCamera(const CMap &loaded, int viewWidth, int viewHeight,
                        CCamera::Viewport &camera);
float GameViewCameraZoom(int viewWidth,
                         int viewHeight);
bool VisibleBoundsScissor(const CMap &loaded, const CCamera::Viewport &camera,
                          int drawableWidth, int drawableHeight, int scissor[4]);
}

namespace MapDetail {
// Terrain is flat; preserve room for the tallest placed model in the projection.
constexpr float kMapDepthRange = 4096.0f;
// CLevel::Bind snaps the original CCamera to this scale (:120747).
constexpr float kLevelCameraScale = 0.8f;
}
