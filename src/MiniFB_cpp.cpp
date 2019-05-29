#include <MiniFB_cpp.h>
#include <MiniFB_enums.h>

std::function<void(struct Window *, bool)>                       Stub::m_active;
std::function<void(struct Window *, int, int)>                   Stub::m_resize;
std::function<void(struct Window *, Key, KeyMod, bool)>          Stub::m_keyboard;
std::function<void(struct Window *, unsigned int)>               Stub::m_char_input;
std::function<void(struct Window *, MouseButton, KeyMod, bool)>  Stub::m_mouse_btn;
std::function<void(struct Window *, int, int)>                   Stub::m_mouse_move;
std::function<void(struct Window *, KeyMod, float, float)>       Stub::m_scroll;

void Stub::active_stub(struct Window *window, bool isActive) {
    m_active(window, isActive);
}

void Stub::resize_stub(struct Window *window, int width, int height) {
    m_resize(window, width, height);
}

void Stub::keyboard_stub(struct Window *window, Key key, KeyMod mod, bool isPressed) {
    m_keyboard(window, key, mod, isPressed);
}

void Stub::char_input_stub(struct Window *window, unsigned int code) {
    m_char_input(window, code);
}

void Stub::mouse_btn_stub(struct Window *window, MouseButton button, KeyMod mod, bool isPressed) {
    m_mouse_btn(window, button, mod, isPressed);
}

void Stub::mouse_move_stub(struct Window *window, int x, int y) {
    m_mouse_move(window, x, y);
}

void Stub::scroll_stub(struct Window *window, KeyMod mod, float deltaX, float deltaY) {
    m_scroll(window, mod, deltaX, deltaY);
}
