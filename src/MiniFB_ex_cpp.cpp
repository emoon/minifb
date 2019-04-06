#include <MiniFB_ex_cpp.h>
#include <MiniFB_ex_enums.h>

std::function<void(eBool)>                          Stub::m_active;
std::function<void(int, int)>                       Stub::m_resize;
std::function<void(eKey, eKeyMod, eBool)>           Stub::m_keyboard;
std::function<void(unsigned int)>                   Stub::m_char_input;
std::function<void(eMouseButton, eKeyMod, eBool)>   Stub::m_mouse_btn;
std::function<void(int, int)>                       Stub::m_mouse_move;
std::function<void(eKeyMod, float, float)>          Stub::m_scroll;

void Stub::active_stub(eBool isActive) {
    m_active(isActive);
}

void Stub::resize_stub(int width, int height) {
    m_resize(width, height);
}

void Stub::keyboard_stub(eKey key, eKeyMod mod, eBool isPressed) {
    m_keyboard(key, mod, isPressed);
}

void Stub::char_input_stub(unsigned int code) {
    m_char_input(code);
}

void Stub::mouse_btn_stub(eMouseButton button, eKeyMod mod, eBool isPressed) {
    m_mouse_btn(button, mod, isPressed);
}

void Stub::mouse_move_stub(int x, int y) {
    m_mouse_move(x, y);
}

void Stub::scroll_stub(eKeyMod mod, float deltaX, float deltaY) {
    m_scroll(mod, deltaX, deltaY);
}
