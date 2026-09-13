#pragma once
/** Editable host diagnostics. Positions use the game's 1024 x 768 canvas; line widths use screen pixels. */
#include "engine/platform/CWindow.h"

namespace DebugConfig {
// RGBA channels are 0..1; alpha=1 is opaque. Example: {1, 1, 0, 1} is yellow.
struct Color { float red, green, blue, alpha; };
// Width is the overlay stroke thickness, independent of collision geometry.
struct LineStyle { float width; Color color; };
struct TextStyle {
    // x/y: top-left on the logical canvas. scale: multiplier, not a point size.
    // width: wrapping width (sidebar/maps only). rowGap: extra sidebar line spacing.
    float x, y, scale, width, rowGap;
    // FONT_KEYSET pair index. See docs/fontbitmap.png for actual fonts and sizes.
    unsigned font;
    // Opacity only; glyph colors are baked into the original PNG atlas.
    float alpha;
};
inline constexpr float CanvasWidth = 1024, CanvasHeight = 768;
// Font 0 is the original blue/white game font. Its colors come from the BIG atlas.
// Change font IDs here, then rebuild. A font ID selects artwork, not font size.
inline constexpr unsigned FpsFont = 11;
inline constexpr unsigned SidebarFont = 0;
// Field order: x, y, scale, wrap width, extra line gap, font ID, opacity.
// Example: FPS scale 0.75 -> 1.0 uses the full native size of the chosen font.
inline constexpr TextStyle FPS{710, 2, 0.75f, 100, 0, FpsFont, 1};
// Example: Sidebar x 16 -> 30 moves all rows right; scale changes all rows together.
inline constexpr TextStyle Sidebar{16, 184, 0.42f, 168, 5, SidebarFont, 1};
inline constexpr unsigned FpsSampleMs = 500;
namespace Tutorial {
inline constexpr float Top = 16;
inline constexpr unsigned BlinkMs = 500;
inline constexpr const char *Notice = "debug Mode Tutorial checking  no save";
inline constexpr const char *Exit = "ESC to exit";
}
inline constexpr unsigned CircleSegments = 48;
// Each category is {stroke width, {red, green, blue, alpha}}.
// Example: Enemy{3, {1, 1, 0, 1}} gives yellow enemy outlines of width 3.
inline constexpr LineStyle Body{6, {0.1f, 0.85f, 1, 1}};
inline constexpr LineStyle BulletWall{4, {1, 0.9f, 0.15f, 1}};
inline constexpr LineStyle Terrain{2, {1, 0.45f, 0.1f, 1}};
inline constexpr LineStyle Brother{2, {0.2f, 1, 0.35f, 1}};
inline constexpr LineStyle Enemy{2, {1, 0.2f, 0.3f, 1}};
inline constexpr LineStyle Projectile{2, {0.85f, 0.25f, 1, 1}};
inline constexpr LineStyle Disabled{1, {0.55f, 0.55f, 0.55f, 1}};



// Desktop-only map browser: these are host layout values, never resource data.
namespace Maps {
struct Rect { float x, y, width, height; };
inline constexpr int RowsPerPage = 15;
inline constexpr Rect List{24, 132, 976, 31}; // Height is one row.
inline constexpr Rect LoadButton{24, 692, 216, 48}, BackButton{780, 692, 220, 48};
inline constexpr Color Background{0.035f, 0.055f, 0.08f, 1}, Selection{0.12f, 0.3f, 0.4f, 1};
inline constexpr float DisabledAlpha = 0.45f;
inline constexpr TextStyle Title{24, 24, 0.8f, 976, 0, 0, 1};
inline constexpr TextStyle Subtitle{24, 68, 0.7f, 976, 0, 1, 1};
inline constexpr TextStyle Help{24, 100, 0.6f, 976, 0, 1, 1};
inline constexpr TextStyle Row{32, 5, 0.65f, 955, 0, 1, 1}; // Y is relative to the row top.
inline constexpr TextStyle Status{24, 614, 0.65f, 976, 0, 1, 1};
inline constexpr TextStyle Notice{24, 650, 0.6f, 976, 0, 1, 1};
inline constexpr TextStyle Load{40, 704, 0.75f, 216, 0, 1, 1}, Back{840, 704, 0.75f, 160, 0, 1, 1};
namespace Text {
inline constexpr const char *Title = "DEBUG MAP BROWSER";
inline constexpr const char *Subtitle = "All BIG maps / linked LEVEL and Mission entries";
inline constexpr const char *Help = "UP/DOWN select   LEFT/RIGHT page   ENTER load   ESC back";
inline constexpr const char *Notice = "Current save equipment / no progress saved. Campaign completion is unverified.";
inline constexpr const char *Load = "LOAD MAP", *Unavailable = "UNAVAILABLE", *Back = "BACK";
inline constexpr const char *NoLevel = "No linked LEVEL; gameplay script unknown";
inline constexpr const char *InvalidMap = "Map parsing failed; original data preserved";
inline constexpr const char *NoPlayer = "No authored player spawn";
inline constexpr const char *ScriptPresent = "Script present; full completion unverified";
inline constexpr const char *NoScript = "LEVEL has no script; map exploration only";
inline constexpr const char *NoMultiplayer = "Multiplayer session is not implemented";
inline constexpr const char *LoadFailed = "Map load failed. See GunBrosRe log for the resource and error.";
inline constexpr const char *Complete = "Mission complete: ", *SelectNext = ". Select a map to continue.";
inline constexpr const char *Map = " MAP ", *Level = " LEVEL ", *Separator = "  /  ", *Campaign = " - CAMPAIGN";
}
}

// Edit display labels below, then rebuild. Keep printf placeholder types/order/count.
// Example: Health="HEALTH %.0f / %.0f" changes the label, not the live values.
// Missing characters cannot be added by changing text: the selected atlas must contain them.
namespace Text {
// Numeric only. %.0f rounds to an integer; %.1f keeps one decimal place.
// "--" means there is not a complete measurement yet, rather than a fake zero.
inline constexpr const char *Fps = "%.0f", *FpsPending = "--";
inline constexpr const char *Heading = "DEBUG";
inline constexpr const char *Map = "%s / MAP %u";
inline constexpr const char *Horde = "HORDE %u", *Time = "TIME %u S";
inline constexpr const char *Wave = "WAVE %u / %u", *Revolution = "REVOLUTION %u / %u";
inline constexpr const char *LevelState = "LEVEL STATE %d";
inline constexpr const char *Health = "HP %.0f / %.0f", *BrotherHealth = "BRO HP %.0f / %.0f";
inline constexpr const char *Position = "XY %.1f / %.1f";
inline constexpr const char *Enemies = "ENEMIES %u / KILLS %u";
inline constexpr const char *Damage = "DAMAGE %.0f / HITS %u";
inline constexpr const char *Weapon = "WEAPON %s", *Effects = "BULLETS %zu / FX %zu";
inline constexpr const char *Experience = "XP %llu / %llu";
inline constexpr const char *Points = "POINTS %u", *Xplodium = "XPLODIUM %llu";
inline constexpr const char *Perfect = "PERFECT %u / %u";
inline constexpr const char *LastNormal = "LAST WAVE: NORMAL", *LastPerfect = "LAST WAVE: PERFECT";
inline constexpr const char *Bonus = "BONUS XPLODIUM +%llu";
inline constexpr const char *CollisionOn = "COLLISIONS ON", *CollisionOff = "COLLISIONS OFF";
inline constexpr const char *BuffNames[] = {"SHIELD", "ATTACK", "DEFENSE", "SPEED", "AUTO AIM", "TANTRUM"};
inline constexpr const char *BuffTimer = "%s %dS", *Turret = "TURRET ACTIVE";
}
}
