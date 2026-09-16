#pragma once
/** Viewer-only input routing and a cached, docked help panel. */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "gun_bros_viewer/ViewerBindings.h"
#include "engine/graphics/ZQuadBatch.h"
#include <memory>

class ViewerControls {
public:
    ViewerControls(ZWindow &window, ViewerBindingSet bindings, bool enabled = true);
    ~ViewerControls();
    bool Init();
    bool PumpEvents();
    ZKeyCode TakeKeyPress();
    bool IsPressed(ZKeyCode key, ViewerAction action) const;
    bool IsDown(ViewerAction action) const;
    void TakeDragDelta(int &x, int &y);
    float TakeWheelDelta();
    bool GetMousePosition(float &x, float &y) const;
    /** The game catalogue still accepts its original canonical selection keys. */
    ZKeyCode WeaponSelectionKey(ZKeyCode key) const;
    void GetDrawableSize(int &width, int &height) const;
    bool Draw();
    /** Screen-space system-font labels, cached by call order between frames. */
    void DrawLabel(const std::string &text, float x, float y, int width,
        int fontHeight, const float *projection);

private:
    static bool Filter(void *context, const SDL_Event &event);
    bool OnEvent(const SDL_Event &event);
    const ViewerBinding *Find(ViewerAction action) const;
    void Layout();
    bool RebuildPanel();
    bool InsideScene() const;
    ZWindow &m_window;
    ViewerBindingSet m_bindings;
    bool m_enabled;
    bool m_expanded = true;
    bool m_dirty = true;
    int m_width = 1, m_height = 1, m_panelWidth = 1;
    int m_scroll = 0, m_contentHeight = 0;
    float m_mouseX = -1, m_mouseY = -1;
    bool m_leftDown = false, m_rightDown = false;
    bool m_leftInScene = false, m_rightInScene = false;
    int m_dragX = 0, m_dragY = 0;
    float m_wheel = 0;
    std::vector<ZKeyCode> m_keys;
    ZShaderProgram m_program;
    ZQuadBatch m_batch;
    ZTexture m_texture;
    struct Label {
        std::string text;
        int width = 0;
        int fontHeight = 0;
        ZTexture texture;
    };
    std::vector<std::unique_ptr<Label>> m_labels;
    std::size_t m_nextLabel = 0;
    bool m_labelFailed = false;
};
