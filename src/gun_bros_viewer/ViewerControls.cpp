/** Windows host UI only: cached system-font text, with no game UI resource data. */
#include "gun_bros_viewer/ViewerControls.h"
#include "engine/core/CMatrix4d.h"
#include "engine/core/Paths.h"
#define NOMINMAX
#include <Windows.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cwchar>

#pragma comment(lib, "gdi32.lib")

namespace {
// Keep the font and dock spacing together so readability is easy to tune.
constexpr int kFontHeight = 22;
constexpr int kPanelWidth = 480;
constexpr int kCollapsedWidth = 44;
constexpr int kHeaderHeight = 88;
constexpr int kFooterHeight = 38;
constexpr int kRowHeight = 38;
constexpr int kGroupHeight = 42;
constexpr int kKeyWidth = 112;
constexpr int kDescriptionLeft = 144;
constexpr int kToggleWidth = 104;
constexpr COLORREF kBackground = RGB(44, 52, 64);
constexpr COLORREF kKeyBackground = RGB(32, 38, 46);
constexpr COLORREF kText = RGB(237, 241, 245);
constexpr COLORREF kMuted = RGB(174, 185, 199);
constexpr COLORREF kAccent = RGB(225, 177, 106);
constexpr COLORREF kBorder = RGB(66, 78, 94);

/** Own GDI handles until the panel bitmap has been copied into an RGBA texture. */
class PanelCanvas {
public:
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HFONT font = nullptr;
    HGDIOBJ oldBitmap = nullptr, oldFont = nullptr;
    void *pixels = nullptr;
    ~PanelCanvas() {
        if (oldFont != nullptr) { SelectObject(dc, oldFont); }
        if (oldBitmap != nullptr) { SelectObject(dc, oldBitmap); }
        if (font != nullptr) { DeleteObject(font); }
        if (bitmap != nullptr) { DeleteObject(bitmap); }
        if (dc != nullptr) { DeleteDC(dc); }
    }
    bool Create(int width, int height) {
        dc = CreateCompatibleDC(nullptr);
        if (dc == nullptr) { return false; }
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        font = CreateFontW(-kFontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (bitmap == nullptr || font == nullptr) { return false; }
        oldBitmap = SelectObject(dc, bitmap);
        oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        return true;
    }
    void Fill(int x, int y, int width, int height, COLORREF color) {
        RECT rect{x, y, x + width, y + height};
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }
    void Text(int x, int y, int width, int height, const wchar_t *text, COLORREF color) {
        RECT rect{x, y, x + width, y + height};
        SetTextColor(dc, color);
        DrawTextW(dc, text, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
};

std::wstring KeyLabel(const ViewerBinding &binding) {
    switch (binding.input) {
        case ViewerInput::LeftDrag: return L"LMB drag";
        case ViewerInput::RightDrag: return L"RMB drag";
        case ViewerInput::Wheel: return L"Wheel";
        case ViewerInput::Pointer: return L"Mouse";
        case ViewerInput::LeftHold: return L"LMB hold";
        case ViewerInput::RightHold: return L"RMB hold";
        default: break;
    }
    // Labels follow enum values rather than duplicating key strings in each binding.
    static constexpr const wchar_t *names[] = {
        L"-", L"Esc", L"Enter", L"Left", L"Right", L"Up", L"Down", L"Home", L"F3",
        L"T", L"P", L"G", L"X", L"R", L"K", L"M", L"N", L"Space", L".",
        L"W", L"A", L"S", L"D", L"B", L"C", L"E", L"F", L"Q",
        L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9"
    };
    static_assert(sizeof(names) / sizeof(names[0]) == static_cast<int>(KeyCode::Count));
    std::wstring label = names[static_cast<int>(binding.key)];
    if (binding.input == ViewerInput::Hold) { label += L" hold"; }
    return label;
}
}

ViewerControls::ViewerControls(CWindow &window, ViewerBindingSet bindings, bool enabled)
    : m_window(window), m_bindings(bindings), m_enabled(enabled) {}

ViewerControls::~ViewerControls() {
    if (m_enabled) {
        m_window.SetEventFilter(nullptr, nullptr);
        m_window.SetEscapeCloses(true);
    }
}

bool ViewerControls::Init() {
    Layout();
    if (!m_enabled) { return true; }
    if (!m_program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
        !m_batch.Create(m_program)) { return false; }
    m_window.SetEscapeCloses(false);
    m_window.SetEventFilter(Filter, this);
    std::printf("[viewer-controls] bindings=%zu; wheel over panel scrolls help\n", m_bindings.count);
    for (std::size_t index = 0; index < m_bindings.count; ++index) {
        const auto &binding = m_bindings.entries[index];
        const std::wstring key = KeyLabel(binding);
        std::printf("[viewer-controls] %ls / %ls: %ls\n", binding.group, key.c_str(), binding.description);
    }
    return true;
}

const ViewerBinding *ViewerControls::Find(ViewerAction action) const {
    for (std::size_t index = 0; index < m_bindings.count; ++index) {
        if (m_bindings.entries[index].action == action) { return &m_bindings.entries[index]; }
    }
    return nullptr;
}

bool ViewerControls::IsPressed(KeyCode key, ViewerAction action) const {
    const ViewerBinding *binding = Find(action);
    return binding != nullptr && binding->input == ViewerInput::Press && binding->key == key;
}

bool ViewerControls::IsDown(ViewerAction action) const {
    const ViewerBinding *binding = Find(action);
    if (binding == nullptr) { return false; }
    if (binding->input == ViewerInput::Hold) { return m_window.IsKeyDown(binding->key); }
    if (!InsideScene()) { return false; }
    if (binding->input == ViewerInput::LeftHold) { return m_leftDown && m_leftInScene; }
    if (binding->input == ViewerInput::RightHold) { return m_rightDown && m_rightInScene; }
    return false;
}

void ViewerControls::Layout() {
    int width = 0, height = 0;
    m_window.GetDrawableSize(width, height);
    width = std::max(1, width);
    height = std::max(1, height);
    int panelWidth = 0;
    if (m_enabled) {
        panelWidth = kCollapsedWidth;
        if (m_expanded) { panelWidth = kPanelWidth; }
        panelWidth = std::min(panelWidth, width / 2);
    }
    if (width != m_width || height != m_height || panelWidth != m_panelWidth) {
        m_dirty = true;
        m_width = width;
        m_height = height;
        m_panelWidth = panelWidth;
        if (m_enabled) {
            std::printf("[viewer-controls] scene=%dx%d panel=%d %s\n",
                width - panelWidth, height, panelWidth, m_expanded ? "expanded" : "collapsed");
        }
    }
    m_contentHeight = 12;
    const wchar_t *group = L"";
    for (std::size_t index = 0; index < m_bindings.count; ++index) {
        const auto &binding = m_bindings.entries[index];
        if (std::wcscmp(group, binding.group) != 0) {
            group = binding.group;
            m_contentHeight += kGroupHeight;
        }
        m_contentHeight += kRowHeight;
    }
    const int maximum = std::max(0, m_contentHeight - std::max(0, height - kHeaderHeight - kFooterHeight));
    m_scroll = std::clamp(m_scroll, 0, maximum);
}

bool ViewerControls::PumpEvents() {
    Layout();
    if (!m_window.PumpEvents()) { return false; }
    Layout();
    m_keys.clear();
    for (KeyCode key = m_window.TakeKeyPress(); key != KeyCode::None; key = m_window.TakeKeyPress()) {
        if (m_enabled && IsPressed(key, ViewerAction::Back)) { return false; }
        m_keys.push_back(key);
    }
    return true;
}

KeyCode ViewerControls::TakeKeyPress() {
    if (m_keys.empty()) { return KeyCode::None; }
    const KeyCode key = m_keys.front();
    m_keys.erase(m_keys.begin());
    return key;
}

void ViewerControls::GetDrawableSize(int &width, int &height) const {
    width = m_width - m_panelWidth;
    height = m_height;
}

bool ViewerControls::InsideScene() const {
    return m_mouseX >= 0 && m_mouseX < m_width - m_panelWidth &&
        m_mouseY >= 0 && m_mouseY < m_height;
}

bool ViewerControls::GetMousePosition(float &x, float &y) const {
    const ViewerBinding *binding = Find(ViewerAction::Aim);
    if (binding == nullptr || binding->input != ViewerInput::Pointer || !InsideScene()) { return false; }
    x = m_mouseX;
    y = m_mouseY;
    return true;
}

void ViewerControls::TakeDragDelta(int &x, int &y) {
    if (!m_enabled) { m_window.TakeDragDelta(x, y); return; }
    x = m_dragX;
    y = m_dragY;
    m_dragX = 0;
    m_dragY = 0;
}

float ViewerControls::TakeWheelDelta() {
    if (!m_enabled) { return m_window.TakeWheelDelta(); }
    const float wheel = m_wheel;
    m_wheel = 0;
    return wheel;
}

KeyCode ViewerControls::WeaponSelectionKey(KeyCode key) const {
    // Translate viewer bindings to the existing catalogue API's canonical commands.
    // These are API arguments, not a second set of physical viewer bindings.
    const ViewerAction categories[] = {ViewerAction::Category1, ViewerAction::Category2,
        ViewerAction::Category3, ViewerAction::Category4, ViewerAction::Category5,
        ViewerAction::Category6, ViewerAction::Category7};
    for (int index = 0; index < 7; ++index) {
        if (IsPressed(key, categories[index])) {
            return static_cast<KeyCode>(static_cast<int>(KeyCode::Digit1) + index);
        }
    }
    if (IsPressed(key, ViewerAction::PreviousVariant)) { return KeyCode::N; }
    if (IsPressed(key, ViewerAction::NextVariant)) { return KeyCode::M; }
    return KeyCode::None;
}

bool ViewerControls::Filter(void *context, const SDL_Event &event) {
    return static_cast<ViewerControls *>(context)->OnEvent(event);
}

bool ViewerControls::OnEvent(const SDL_Event &event) {
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        m_leftDown = false;
        m_rightDown = false;
        m_leftInScene = false;
        m_rightInScene = false;
        m_dragX = 0;
        m_dragY = 0;
        m_wheel = 0;
        m_mouseX = -1;
        m_mouseY = -1;
        return true;
    }
    if (event.type != SDL_EVENT_MOUSE_MOTION && event.type != SDL_EVENT_MOUSE_WHEEL &&
        event.type != SDL_EVENT_MOUSE_BUTTON_DOWN && event.type != SDL_EVENT_MOUSE_BUTTON_UP) { return true; }
    int width = 1, height = 1;
    SDL_GetWindowSize(SDL_GetWindowFromID(m_window.GetSurfaceId()), &width, &height);
    const float scaleX = static_cast<float>(m_width) / std::max(1, width);
    const float scaleY = static_cast<float>(m_height) / std::max(1, height);
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        m_mouseX = event.motion.x * scaleX;
        m_mouseY = event.motion.y * scaleY;
        const ViewerBinding *drag = Find(ViewerAction::Drag);
        bool dragging = false;
        if (drag != nullptr && drag->input == ViewerInput::LeftDrag) { dragging = m_leftDown && m_leftInScene; }
        if (drag != nullptr && drag->input == ViewerInput::RightDrag) { dragging = m_rightDown && m_rightInScene; }
        if (dragging && InsideScene()) {
            m_dragX += static_cast<int>(event.motion.xrel * scaleX);
            m_dragY += static_cast<int>(event.motion.yrel * scaleY);
        }
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        m_mouseX = event.wheel.mouse_x * scaleX;
        m_mouseY = event.wheel.mouse_y * scaleY;
        float wheel = event.wheel.y;
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) { wheel = -wheel; }
        if (InsideScene()) {
            const ViewerBinding *zoom = Find(ViewerAction::Zoom);
            if (zoom != nullptr && zoom->input == ViewerInput::Wheel) { m_wheel += wheel; }
        } else if (m_expanded) {
            m_scroll -= static_cast<int>(wheel * kRowHeight * 3);
            Layout();
            m_dirty = true;
        }
    } else {
        m_mouseX = event.button.x * scaleX;
        m_mouseY = event.button.y * scaleY;
        const bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        if (event.button.button == SDL_BUTTON_LEFT) {
            m_leftDown = down;
            m_leftInScene = down && InsideScene();
            const bool overToggle = m_mouseX >= m_width - std::min(kToggleWidth, m_panelWidth) &&
                m_mouseX < m_width && m_mouseY >= 0 && m_mouseY < kHeaderHeight;
            if (down && overToggle) {
                m_expanded = !m_expanded;
                m_dragX = 0;
                m_dragY = 0;
                m_wheel = 0;
                Layout();
            }
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            m_rightDown = down;
            m_rightInScene = down && InsideScene();
        }
    }
    // Every mouse gesture belongs to either the panel or the scene, never both.
    return false;
}

bool ViewerControls::RebuildPanel() {
    PanelCanvas canvas;
    if (!canvas.Create(m_panelWidth, m_height)) {
        std::printf("[viewer-controls] cannot create panel bitmap\n");
        return false;
    }
    canvas.Fill(0, 0, m_panelWidth, m_height, kBackground);
    canvas.Fill(0, 0, 1, m_height, kBorder);
    if (!m_expanded) {
        canvas.Text(12, 12, 28, 38, L"<", kAccent);
    } else {
        canvas.Text(16, 8, m_panelWidth - kToggleWidth - 24, 36, L"CONTROLS", kText);
        canvas.Text(16, 46, m_panelWidth - 32, 32, m_bindings.name, kMuted);
        canvas.Text(m_panelWidth - kToggleWidth, 10, kToggleWidth - 12, 36, L"Hide  >", kAccent);
        canvas.Fill(0, kHeaderHeight - 1, m_panelWidth, 1, kBorder);
        const int bottom = std::max(kHeaderHeight, m_height - kFooterHeight);
        IntersectClipRect(canvas.dc, 1, kHeaderHeight, m_panelWidth - 10, bottom);
        int y = kHeaderHeight + 6 - m_scroll;
        const wchar_t *group = L"";
        for (std::size_t index = 0; index < m_bindings.count; ++index) {
            const auto &binding = m_bindings.entries[index];
            if (std::wcscmp(group, binding.group) != 0) {
                group = binding.group;
                canvas.Text(16, y, m_panelWidth - 32, kGroupHeight, group, kAccent);
                y += kGroupHeight;
            }
            canvas.Fill(16, y + 2, kKeyWidth, kRowHeight - 4, kBorder);
            canvas.Fill(17, y + 3, kKeyWidth - 2, kRowHeight - 6, kKeyBackground);
            const std::wstring key = KeyLabel(binding);
            canvas.Text(24, y, kKeyWidth - 16, kRowHeight, key.c_str(), kText);
            canvas.Text(kDescriptionLeft, y, m_panelWidth - kDescriptionLeft - 18,
                kRowHeight, binding.description, kText);
            y += kRowHeight;
        }
        SelectClipRgn(canvas.dc, nullptr);
        const int visible = bottom - kHeaderHeight;
        if (m_contentHeight > visible && visible > 0) {
            const int thumbHeight = std::max(12, visible * visible / m_contentHeight);
            const int thumbY = kHeaderHeight + m_scroll * (visible - thumbHeight) / (m_contentHeight - visible);
            canvas.Fill(m_panelWidth - 6, kHeaderHeight, 3, visible, kKeyBackground);
            canvas.Fill(m_panelWidth - 6, thumbY, 3, thumbHeight, kMuted);
        }
        canvas.Fill(0, bottom, m_panelWidth, 1, kBorder);
        canvas.Text(16, bottom, m_panelWidth - 32, kFooterHeight, L"Wheel here to scroll controls", kMuted);
    }
    PNGImage image;
    image.width = m_panelWidth;
    image.height = m_height;
    image.pixels.resize(static_cast<std::size_t>(m_panelWidth) * m_height * 4);
    GdiFlush();
    const auto *source = static_cast<const unsigned char *>(canvas.pixels);
    for (std::size_t offset = 0; offset < image.pixels.size(); offset += 4) {
        image.pixels[offset] = source[offset + 2];
        image.pixels[offset + 1] = source[offset + 1];
        image.pixels[offset + 2] = source[offset];
        image.pixels[offset + 3] = 255;
    }
    if (!m_texture.Create(image)) { return false; }
    m_dirty = false;
    return true;
}

bool ViewerControls::Draw() {
    if (!m_enabled || m_panelWidth == 0) { return true; }
    if (m_dirty && !RebuildPanel()) { return false; }
    // Restore depth and viewport: turntables set their depth state outside the loop.
    const GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blend = glIsEnabled(GL_BLEND);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLint sourceBlend = 0, destinationBlend = 0;
    glGetIntegerv(GL_BLEND_SRC, &sourceBlend);
    glGetIntegerv(GL_BLEND_DST, &destinationBlend);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, m_width, m_height);
    float projection[16];
    Matrix4dOrthoTopLeft(static_cast<float>(m_width), static_cast<float>(m_height), 1, projection);
    m_batch.Begin();
    SourceRect source{0, 0, static_cast<std::uint16_t>(m_panelWidth), static_cast<std::uint16_t>(m_height)};
    m_batch.AddQuad(m_texture, static_cast<float>(m_width - m_panelWidth), 0,
        static_cast<float>(m_panelWidth), static_cast<float>(m_height), source, false, false, BlendMode::Alpha);
    m_batch.Upload();
    m_batch.Draw(m_program, projection);
    glBlendFunc(sourceBlend, destinationBlend);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if (depth) { glEnable(GL_DEPTH_TEST); }
    if (blend) { glEnable(GL_BLEND); }
    if (scissor) { glEnable(GL_SCISSOR_TEST); }
    return true;
}
