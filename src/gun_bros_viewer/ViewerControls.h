#pragma once
/** Viewer-only input routing and a cached, docked help panel. */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "gun_bros_viewer/ViewerBindings.h"
#include "engine/graphics/CQuadBatch.h"

class ViewerControls {
public:
    ViewerControls(CWindow &window, ViewerBindingSet bindings, bool enabled = true);
    ~ViewerControls();
    bool Init();
    bool PumpEvents();
    KeyCode TakeKeyPress();
    bool IsPressed(KeyCode key, ViewerAction action) const;
    bool IsDown(ViewerAction action) const;
    void TakeDragDelta(int &x, int &y);
    float TakeWheelDelta();
    bool GetMousePosition(float &x, float &y) const;
    /** The game catalogue still accepts its original canonical selection keys. */
    KeyCode WeaponSelectionKey(KeyCode key) const;
    void GetDrawableSize(int &width, int &height) const;
    bool Draw();

private:
    static bool Filter(void *context, const SDL_Event &event);
    bool OnEvent(const SDL_Event &event);
    const ViewerBinding *Find(ViewerAction action) const;
    void Layout();
    bool RebuildPanel();
    bool InsideScene() const;
    CWindow &m_window;
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
    std::vector<KeyCode> m_keys;
    CShaderProgram m_program;
    CQuadBatch m_batch;
    CTexture m_texture;
};
