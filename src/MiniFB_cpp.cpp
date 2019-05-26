#include <MiniFB_cpp.h>
#include <MiniFB_enums.h>

std::function<void(bool)>                          Stub::m_active;
std::function<void(int, int)>                       Stub::m_resize;
std::function<void(Key, KeyMod, bool)>           Stub::m_keyboard;
std::function<void(unsigned int)>                   Stub::m_char_input;
std::function<void(MouseButton, KeyMod, bool)>   Stub::m_mouse_btn;
std::function<void(int, int)>                       Stub::m_mouse_move;
std::function<void(KeyMod, float, float)>          Stub::m_scroll;

void Stub::active_stub(bool isActive) {
    m_active(isActive);
}

void Stub::resize_stub(int width, int height) {
    m_resize(width, height);
}

void Stub::keyboard_stub(Key key, KeyMod mod, bool isPressed) {
    m_keyboard(key, mod, isPressed);
}

void Stub::char_input_stub(unsigned int code) {
    m_char_input(code);
}

void Stub::mouse_btn_stub(MouseButton button, KeyMod mod, bool isPressed) {
    m_mouse_btn(button, mod, isPressed);
}

void Stub::mouse_move_stub(int x, int y) {
    m_mouse_move(x, y);
}

void Stub::scroll_stub(KeyMod mod, float deltaX, float deltaY) {
    m_scroll(mod, deltaX, deltaY);
}
