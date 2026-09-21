#pragma once
#include "ZProductVersion.h"
#include "gun_bros_re/host/ZScreenResolution.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

/** All launcher presentation settings. Geometry uses logical pixels at 96 DPI. */
namespace ZLaunchDialogConfig {
inline constexpr wchar_t WindowClass[] = L"GunBrosLaunchDialog";
// Icon resource GunBrosRe.rc embeds; both sides read the same macro.
inline constexpr WORD AppIconId = GB_ICON_ID;
inline constexpr char HeaderPath[] = "assets/startup/Gun_Bros_Header_Art.png";
inline constexpr DWORD WindowStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
inline constexpr DWORD WindowExtendedStyle = WS_EX_CONTROLPARENT;
inline constexpr int Width = 622, Height = 646;
inline constexpr int BaseDpi = 96, DesktopMargin = 24, FocusInset = 4;
inline constexpr wchar_t FontFamily[] = L"Segoe UI";
inline constexpr int FontSize = 14, HeadingFontSize = 15;
inline constexpr int FontWeight = FW_NORMAL, HeadingFontWeight = FW_SEMIBOLD;
inline constexpr COLORREF Background = RGB(229, 233, 232);
inline constexpr COLORREF Panel = RGB(247, 248, 245);
inline constexpr COLORREF TextColor = RGB(40, 54, 52);
inline constexpr COLORREF Muted = RGB(100, 113, 107);
inline constexpr COLORREF Border = RGB(173, 179, 151);
inline constexpr COLORREF Accent = RGB(181, 161, 108);
inline constexpr COLORREF AccentPressed = RGB(163, 142, 87);

namespace Text {
inline constexpr wchar_t Common[] = L"Common";
inline constexpr wchar_t StartDialog[] = L"Show launch dialog at startup";
inline constexpr wchar_t Resolution[] = L"Resolution";
inline constexpr wchar_t Audio[] = L"Audio";
inline constexpr wchar_t Music[] = L"Music";
inline constexpr wchar_t Effects[] = L"Effects";
inline constexpr wchar_t Game[] = L"Game";
inline constexpr wchar_t Connected[] = L"Enable local online connection";
inline constexpr wchar_t Bot[] = L"Bot difficulty";
inline constexpr const wchar_t *BotOptions[] = {L"Easy", L"Normal", L"Hard"};
inline constexpr wchar_t Controls[] = L"Controls";
inline constexpr const wchar_t *ControlOptions[] = {L"Mouse aim", L"Right-stick drag"};
inline constexpr wchar_t Debug[] = L"Debug";
inline constexpr wchar_t DebugMode[] = L"Debug mode";
inline constexpr wchar_t FPS[] = L"Show FPS";
inline constexpr wchar_t Footer[] = L"Changes are saved when you launch.";
inline constexpr wchar_t Cancel[] = L"Cancel";
inline constexpr wchar_t Launch[] = L"Launch game";
inline constexpr wchar_t DimensionSeparator[] = L" x ";
inline constexpr wchar_t SavedSizeSuffix[] = L" (saved)";
inline constexpr wchar_t MutedVolumeSuffix[] = L"  Off";
inline constexpr wchar_t SaveError[] = L"Could not save settings. Check that the cfg file and its folder are writable.";
inline constexpr wchar_t SaveErrorTitle[] = L"Save failed";
inline constexpr wchar_t HeaderErrorPrefix[] = L"Cannot load header image: ";
inline constexpr wchar_t HeaderErrorTitle[] = L"Header image unavailable";
}

namespace Id {
inline constexpr int StartDialog = 100, Resolution = 101;
inline constexpr int Sound = 110, Effects = 111, Connected = 120, Bot = 121;
inline constexpr int Control = 130, Debug = 140, FPS = 141, Footer = 151;
}

struct Rect { int x, y, width, height; };
namespace Layout {
inline constexpr int Margin = 24, PanelPadding = 14, ColumnGap = 20;
inline constexpr int LabelHeight = 22, DropdownHeight = 210;
inline constexpr int Left = Margin + PanelPadding;
inline constexpr int LeftPanelWidth = 278;
inline constexpr int RightPanel = Margin + LeftPanelWidth + ColumnGap;
inline constexpr int Right = RightPanel + PanelPadding;
inline constexpr int ContentWidth = Width - Margin * 2;
inline constexpr int RightPanelWidth = Width - Margin - RightPanel;
inline constexpr Rect Header{Margin, 16, ContentWidth, 190};
inline constexpr Rect Panels[] = {
    {Margin, 216, ContentWidth, 112},
    {Margin, 340, LeftPanelWidth, 124}, {RightPanel, 340, RightPanelWidth, 124},
    {Margin, 476, LeftPanelWidth, 94}, {RightPanel, 476, RightPanelWidth, 94}
};
inline constexpr Rect CommonHeading{Left, 226, 230, LabelHeight};
inline constexpr Rect StartDialog{Left, 290, 442, 26};
inline constexpr Rect ResolutionLabel{Left, 259, 90, LabelHeight};
inline constexpr Rect Resolution{140, 254, 442, DropdownHeight};
inline constexpr Rect AudioHeading{Left, 350, 240, LabelHeight};
inline constexpr Rect MusicLabel{Left, 385, 85, LabelHeight};
inline constexpr Rect Music{130, 380, 154, DropdownHeight};
inline constexpr Rect EffectsLabel{Left, 423, 85, LabelHeight};
inline constexpr Rect Effects{130, 418, 154, DropdownHeight};
inline constexpr Rect GameHeading{Right, 350, 245, LabelHeight};
inline constexpr Rect Connected{Right, 382, 246, 26};
inline constexpr Rect BotLabel{Right, 423, 86, LabelHeight};
inline constexpr Rect Bot{426, 418, 156, DropdownHeight};
inline constexpr Rect ControlsHeading{Left, 486, 246, LabelHeight};
inline constexpr Rect Controls{Left, 520, 246, DropdownHeight};
inline constexpr Rect DebugHeading{Right, 486, 246, LabelHeight};
inline constexpr Rect DebugMode{Right, 520, 128, 25};
inline constexpr Rect FPS{468, 520, 114, 25};
inline constexpr Rect Footer{Margin, 600, 352, LabelHeight};
inline constexpr int ButtonY = 592, ButtonHeight = 34, ButtonGap = 12;
inline constexpr Rect Launch{486, ButtonY, 112, ButtonHeight};
inline constexpr Rect Cancel{Launch.x - ButtonGap - 80, ButtonY, 80, ButtonHeight};
}

// Preset values remain strongly typed; custom cfg sizes use the Saved enum value.
inline constexpr ZScreenResolutionOption ScreenResolutions[] = {
    {ZScreenResolution::Size640x480, 640, 480},
    {ZScreenResolution::Size800x600, 800, 600},
    {ZScreenResolution::Size1024x768, 1024, 768},
    {ZScreenResolution::Size1280x960, 1280, 960},
    {ZScreenResolution::Size1600x1200, 1600, 1200},
    {ZScreenResolution::Size1920x1440, 1920, 1440},
    {ZScreenResolution::Size2048x1536, 2048, 1536}
};
inline constexpr int MinimumVolume = 0, MaximumVolume = 10;
}
