/** Windows launcher UI. Geometry and colors belong to the desktop host, not the original game. */
#include "gun_bros_re/host/ZLaunchDialog.h"
#include "gun_bros_re/host/ZHostSettings.h"
#include "gun_bros_re/host/ZLaunchDialogConfig.h"
#include <gdiplus.h>
#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>

namespace {
using namespace ZLaunchDialogConfig;

std::wstring Wide(const std::string &text) {
    if (text.empty()) { return {}; }
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(count, L'\0');
    if (count > 0) { MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), count); }
    return result;
}

class ZLaunchDialog {
public:
    ZLaunchDialog(ZHostSettings &settings, const std::filesystem::path &path) : m_settings(settings), m_path(path) {}
    ~ZLaunchDialog() {
        if (m_window != nullptr) { DestroyWindow(m_window); }
        DeleteObject(m_font);
        DeleteObject(m_headingFont);
        DeleteObject(m_backgroundBrush);
        DeleteObject(m_panelBrush);
    }

    ZLaunchResult Run(Gdiplus::Image &header) {
        m_header = &header;
        WNDCLASSW type{};
        type.lpfnWndProc = WindowProc;
        type.hInstance = GetModuleHandleW(nullptr);
        type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        type.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        type.lpszClassName = WindowClass;
        if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { return ZLaunchResult::Error; }
        // Scope DPI awareness to the launcher so SDL retains its own DPI policy.
        const auto previousDpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        m_window = CreateWindowExW(WindowExtendedStyle, WindowClass, Wide(m_settings.title).c_str(),
            WindowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
            Width, Height, nullptr, nullptr, type.hInstance, this);
        if (m_window != nullptr) {
            FitLayout();
            ShowWindow(m_window, SW_SHOWNORMAL);
            UpdateWindow(m_window);
            SetFocus(GetDlgItem(m_window, IDOK));
            std::printf("[launcher] ready\n");
            MSG message{};
            while (m_window != nullptr) {
                const int received = GetMessageW(&message, nullptr, 0, 0);
                if (received <= 0) { break; }
                if (!IsDialogMessageW(m_window, &message)) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
        }
        if (previousDpi != nullptr) { SetThreadDpiAwarenessContext(previousDpi); }
        return m_result;
    }

private:
    struct Control {
        HWND window;
        Rect area;
        bool heading;
    };
    ZHostSettings &m_settings;
    const std::filesystem::path &m_path;
    HWND m_window = nullptr;
    Gdiplus::Image *m_header = nullptr;
    ZLaunchResult m_result = ZLaunchResult::Error;
    float m_scale = 1;
    HFONT m_font = nullptr, m_headingFont = nullptr;
    HBRUSH m_backgroundBrush = CreateSolidBrush(Background);
    HBRUSH m_panelBrush = CreateSolidBrush(Panel);
    std::vector<Control> m_controls;
    bool m_controlsValid = true;

    int Px(int value) const { return static_cast<int>(value * m_scale + 0.5f); }
    RECT Area(const Rect &area) const {
        return {Px(area.x), Px(area.y), Px(area.x + area.width), Px(area.y + area.height)};
    }

    HWND Add(const wchar_t *type, const wchar_t *text, DWORD style, int id,
             const Rect &area, bool heading = false) {
        HWND control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
            area.x, area.y, area.width, area.height, m_window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        if (control == nullptr) { m_controlsValid = false; }
        m_controls.push_back({control, area, heading});
        return control;
    }

    void Label(const wchar_t *text, const Rect &area, bool heading = false) {
        Add(L"STATIC", text, SS_LEFT, -1, area, heading);
    }

    HWND Combo(int id, const Rect &area) {
        return Add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, id, area);
    }

    void Checkbox(int id, const wchar_t *text, const Rect &area, bool checked) {
        HWND control = Add(L"BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP, id, area);
        SendMessageW(control, BM_SETCHECK, checked, 0);
    }

    void Choice(HWND control, const wchar_t *text) { SendMessageW(control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text)); }
    void Selected(HWND control, int index) { SendMessageW(control, CB_SETCURSEL, index, 0); }

    void CreateControls() {
        Label(Text::Common, Layout::CommonHeading, true);
        Checkbox(Id::StartDialog, Text::StartDialog, Layout::StartDialog, m_settings.startDialog);
        Label(Text::Resolution, Layout::ResolutionLabel);
        HWND resolution = Combo(Id::Resolution, Layout::Resolution);
        int selected = -1;
        for (const auto &option : ScreenResolutions) {
            const std::wstring label = std::to_wstring(option.width) + Text::DimensionSeparator + std::to_wstring(option.height);
            const auto index = SendMessageW(resolution, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            SendMessageW(resolution, CB_SETITEMDATA, index, static_cast<LPARAM>(option.value));
            if (option.width == m_settings.screenX && option.height == m_settings.screenY) { selected = static_cast<int>(index); }
        }
        // Preserve existing hand-edited cfg dimensions without silently selecting another size.
        if (selected < 0) {
            const std::wstring label = std::to_wstring(m_settings.screenX) + Text::DimensionSeparator + std::to_wstring(m_settings.screenY) + Text::SavedSizeSuffix;
            selected = static_cast<int>(SendMessageW(resolution, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
            SendMessageW(resolution, CB_SETITEMDATA, selected, static_cast<LPARAM>(ZScreenResolution::Saved));
        }
        Selected(resolution, selected);

        Label(Text::Audio, Layout::AudioHeading, true);
        Label(Text::Music, Layout::MusicLabel);
        HWND sound = Combo(Id::Sound, Layout::Music);
        Label(Text::Effects, Layout::EffectsLabel);
        HWND effects = Combo(Id::Effects, Layout::Effects);
        for (int value = MinimumVolume; value <= MaximumVolume; ++value) {
            std::wstring label = std::to_wstring(value);
            if (value == MinimumVolume) { label += Text::MutedVolumeSuffix; }
            Choice(sound, label.c_str()); Choice(effects, label.c_str());
        }
        Selected(sound, m_settings.soundVolume); Selected(effects, m_settings.effectsVolume);

        Label(Text::Game, Layout::GameHeading, true);
        Checkbox(Id::Connected, Text::Connected, Layout::Connected, m_settings.isConnected);
        Label(Text::Bot, Layout::BotLabel);
        HWND bot = Combo(Id::Bot, Layout::Bot);
        for (const auto *option : Text::BotOptions) { Choice(bot, option); }
        Selected(bot, m_settings.dmBotLevel - 1);

        Label(Text::Controls, Layout::ControlsHeading, true);
        HWND control = Combo(Id::Control, Layout::Controls);
        for (const auto *option : Text::ControlOptions) { Choice(control, option); }
        Selected(control, m_settings.control - 1);

        Label(Text::Debug, Layout::DebugHeading, true);
        Checkbox(Id::Debug, Text::DebugMode, Layout::DebugMode, m_settings.debugMode);
        Checkbox(Id::FPS, Text::FPS, Layout::FPS, m_settings.drawFPS);

        Add(L"STATIC", Text::Footer, SS_LEFT, Id::Footer, Layout::Footer);
        Add(L"BUTTON", Text::Cancel, BS_PUSHBUTTON | WS_TABSTOP, IDCANCEL, Layout::Cancel);
        Add(L"BUTTON", Text::Launch, BS_OWNERDRAW | WS_TABSTOP, IDOK, Layout::Launch);
    }

    void FitLayout() {
        MONITORINFO monitor{sizeof(MONITORINFO)};
        GetMonitorInfoW(MonitorFromWindow(m_window, MONITOR_DEFAULTTONEAREST), &monitor);
        const UINT dpi = GetDpiForWindow(m_window);
        m_scale = static_cast<float>(dpi) / BaseDpi;
        RECT outer{0, 0, Px(Width), Px(Height)};
        AdjustWindowRectExForDpi(&outer, WindowStyle, FALSE, WindowExtendedStyle, dpi);
        const int borderWidth = outer.right - outer.left - Px(Width);
        const int borderHeight = outer.bottom - outer.top - Px(Height);
        const int availableWidth = monitor.rcWork.right - monitor.rcWork.left - borderWidth - DesktopMargin;
        const int availableHeight = monitor.rcWork.bottom - monitor.rcWork.top - borderHeight - DesktopMargin;
        m_scale = std::min(m_scale, static_cast<float>(availableWidth) / Width);
        m_scale = std::min(m_scale, static_cast<float>(availableHeight) / Height);
        const int width = Px(Width) + borderWidth, height = Px(Height) + borderHeight;
        SetWindowPos(m_window, nullptr, monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2,
            monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2,
            width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        HFONT font = CreateFontW(-Px(FontSize), 0, 0, 0, FontWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, FontFamily);
        HFONT heading = CreateFontW(-Px(HeadingFontSize), 0, 0, 0, HeadingFontWeight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, FontFamily);
        for (const auto &control : m_controls) {
            HFONT selectedFont = font;
            if (control.heading) { selectedFont = heading; }
            SendMessageW(control.window, WM_SETFONT, reinterpret_cast<WPARAM>(selectedFont), TRUE);
            MoveWindow(control.window, Px(control.area.x), Px(control.area.y), Px(control.area.width), Px(control.area.height), TRUE);
        }
        DeleteObject(m_font); DeleteObject(m_headingFont);
        m_font = font; m_headingFont = heading;
        InvalidateRect(m_window, nullptr, TRUE);
    }

    bool Save() {
        ZHostSettings edited = m_settings;
        edited.startDialog = SendDlgItemMessageW(m_window, Id::StartDialog, BM_GETCHECK, 0, 0) == BST_CHECKED;
        const auto selected = SendDlgItemMessageW(m_window, Id::Resolution, CB_GETCURSEL, 0, 0);
        if (selected == CB_ERR) { return false; }
        const auto resolution = static_cast<ZScreenResolution>(SendDlgItemMessageW(m_window, Id::Resolution, CB_GETITEMDATA, selected, 0));
        for (const auto &option : ScreenResolutions) {
            if (option.value == resolution) { edited.screenX = option.width; edited.screenY = option.height; break; }
        }
        edited.soundVolume = static_cast<int>(SendDlgItemMessageW(m_window, Id::Sound, CB_GETCURSEL, 0, 0));
        edited.effectsVolume = static_cast<int>(SendDlgItemMessageW(m_window, Id::Effects, CB_GETCURSEL, 0, 0));
        edited.dmBotLevel = static_cast<int>(SendDlgItemMessageW(m_window, Id::Bot, CB_GETCURSEL, 0, 0)) + 1;
        edited.control = static_cast<int>(SendDlgItemMessageW(m_window, Id::Control, CB_GETCURSEL, 0, 0)) + 1;
        edited.isConnected = SendDlgItemMessageW(m_window, Id::Connected, BM_GETCHECK, 0, 0) == BST_CHECKED;
        edited.debugMode = SendDlgItemMessageW(m_window, Id::Debug, BM_GETCHECK, 0, 0) == BST_CHECKED;
        edited.drawFPS = SendDlgItemMessageW(m_window, Id::FPS, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!edited.Save(m_path)) {
            MessageBoxW(m_window, Text::SaveError, Text::SaveErrorTitle, MB_OK | MB_ICONERROR);
            return false;
        }
        m_settings = edited;
        return true;
    }

    void Paint() {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(m_window, &paint);
        RECT client{}; GetClientRect(m_window, &client);
        FillRect(dc, &client, m_backgroundBrush);
        HBRUSH border = CreateSolidBrush(Border);
        for (const auto &layout : Layout::Panels) {
            const RECT panel = Area(layout);
            FillRect(dc, &panel, m_panelBrush);
            FrameRect(dc, &panel, border);
        }
        DeleteObject(border);
        {
            Gdiplus::Graphics graphics(dc);
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            graphics.DrawImage(m_header, Px(Layout::Header.x), Px(Layout::Header.y),
                Px(Layout::Header.width), Px(Layout::Header.height));
        }
        EndPaint(m_window, &paint);
    }

    void DrawStartButton(const DRAWITEMSTRUCT &item) {
        COLORREF color = Accent;
        if ((item.itemState & ODS_SELECTED) != 0) { color = AccentPressed; }
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(item.hDC, &item.rcItem, brush); DeleteObject(brush);
        SetBkMode(item.hDC, TRANSPARENT); SetTextColor(item.hDC, TextColor);
        const auto previousFont = SelectObject(item.hDC, m_headingFont);
        RECT text = item.rcItem;
        DrawTextW(item.hDC, Text::Launch, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(item.hDC, previousFont);
        if ((item.itemState & ODS_FOCUS) != 0) {
            InflateRect(&text, -Px(FocusInset), -Px(FocusInset)); DrawFocusRect(item.hDC, &text);
        }
    }

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM key, LPARAM data) {
        auto *self = reinterpret_cast<ZLaunchDialog *>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<ZLaunchDialog *>(reinterpret_cast<CREATESTRUCTW *>(data)->lpCreateParams);
            self->m_window = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self == nullptr) { return DefWindowProcW(window, message, key, data); }
        switch (message) {
        case WM_CREATE:
            self->CreateControls();
            if (!self->m_controlsValid) { return -1; }
            return 0;
        case WM_PAINT: self->Paint(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_DPICHANGED:
            {
                const auto &area = *reinterpret_cast<RECT *>(data);
                SetWindowPos(window, nullptr, area.left, area.top, area.right - area.left, area.bottom - area.top, SWP_NOZORDER | SWP_NOACTIVATE);
                self->FitLayout();
            }
            return 0;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            SetTextColor(reinterpret_cast<HDC>(key), TextColor);
            SetBkColor(reinterpret_cast<HDC>(key), Panel);
            if (GetDlgCtrlID(reinterpret_cast<HWND>(data)) == Id::Footer) {
                SetTextColor(reinterpret_cast<HDC>(key), Muted);
                SetBkColor(reinterpret_cast<HDC>(key), Background);
                return reinterpret_cast<LRESULT>(self->m_backgroundBrush);
            }
            return reinterpret_cast<LRESULT>(self->m_panelBrush);
        case WM_DRAWITEM:
            if (key == IDOK) { self->DrawStartButton(*reinterpret_cast<DRAWITEMSTRUCT *>(data)); return TRUE; }
            break;
        case WM_COMMAND:
            if (LOWORD(key) == IDOK) {
                if (self->Save()) { self->m_result = ZLaunchResult::Start; DestroyWindow(window); }
                return 0;
            }
            if (LOWORD(key) == IDCANCEL) { SendMessageW(window, WM_CLOSE, 0, 0); return 0; }
            break;
        case WM_CLOSE:
            self->m_result = ZLaunchResult::Cancel; DestroyWindow(window); return 0;
        case WM_NCDESTROY:
            self->m_window = nullptr;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            break;
        }
        return DefWindowProcW(window, message, key, data);
    }
};
}

ZLaunchResult ShowLaunchDialog(ZHostSettings &settings, const std::filesystem::path &configPath,
                              const std::filesystem::path &headerPath) {
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) { return ZLaunchResult::Error; }
    ZLaunchResult result = ZLaunchResult::Error;
    {
        Gdiplus::Image header(headerPath.c_str());
        if (header.GetLastStatus() != Gdiplus::Ok) {
            std::printf("[launcher] header load failed: %s\n", headerPath.u8string().c_str());
            const std::wstring message = Text::HeaderErrorPrefix + headerPath.wstring();
            MessageBoxW(nullptr, message.c_str(), Text::HeaderErrorTitle, MB_OK | MB_ICONERROR);
        } else {
            ZLaunchDialog dialog(settings, configPath);
            result = dialog.Run(header);
        }
    }
    Gdiplus::GdiplusShutdown(token);
    return result;
}
