#include <MiniFB_cpp.h>
#include <MiniFB_enums.h>
#include <vector>

Stub *
Stub::GetInstance(struct Window *window) {
    static std::vector<Stub *>  s_instances;

    for(Stub *instance : s_instances) {
        if(instance->m_window == window) {
            return instance;
        }
    }

    s_instances.push_back(new Stub);
    s_instances.back()->m_window = window;

    return s_instances.back();
}

void Stub::active_stub(struct Window *window, bool isActive) {
    Stub    *stub = Stub::GetInstance(window);
    stub->m_active(window, isActive);
}

void Stub::resize_stub(struct Window *window, int width, int height) {
    Stub    *stub = Stub::GetInstance(window);
    stub->m_resize(window, width, height);
}

void Stub::keyboard_stub(struct Window *window, Key key, KeyMod mod, bool isPressed) {
    Stub    *stub = Stub::GetInstance(window);
    stub->m_keyboard(window, key, mod, isPressed);
}

void Stub::char_input_stub(struct Window *window, unsigned int code) {
    Stub    *stub = Stub::GetInstance(window);
    stub->m_char_input(window, code);
}

void Stub::mouse_btn_stub(struct Window *window, MouseButton button, KeyMod mod, bool isPressed) {
    Stub    *stub = Stub::GetInstance(window);
    stub->m_mouse_btn(window, button, mod, isPressed);
}

void Stub::mouse_move_stub(struct Window *window, int x, int y) {
    Stub    *stub = Stub::GetInstance(window);
    stub->m_mouse_move(window, x, y);
}

void Stub::scroll_stub(struct Window *window, KeyMod mod, float deltaX, float deltaY) {
    Stub    *stub = Stub::GetInstance(window);
    stub->m_scroll(window, mod, deltaX, deltaY);
}
