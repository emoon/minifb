#include <MiniFB_cpp.h>
#include <MiniFB_enums.h>

std::function<void(void *, bool)>                       Stub::m_active;
std::function<void(void *, int, int)>                   Stub::m_resize;
std::function<void(void *, Key, KeyMod, bool)>          Stub::m_keyboard;
std::function<void(void *, unsigned int)>               Stub::m_char_input;
std::function<void(void *, MouseButton, KeyMod, bool)>  Stub::m_mouse_btn;
std::function<void(void *, int, int)>                   Stub::m_mouse_move;
std::function<void(void *, KeyMod, float, float)>       Stub::m_scroll;

void Stub::active_stub(void *user_data, bool isActive) {
    m_active(user_data, isActive);
}

void Stub::resize_stub(void *user_data, int width, int height) {
    m_resize(user_data, width, height);
}

void Stub::keyboard_stub(void *user_data, Key key, KeyMod mod, bool isPressed) {
    m_keyboard(user_data, key, mod, isPressed);
}

void Stub::char_input_stub(void *user_data, unsigned int code) {
    m_char_input(user_data, code);
}

void Stub::mouse_btn_stub(void *user_data, MouseButton button, KeyMod mod, bool isPressed) {
    m_mouse_btn(user_data, button, mod, isPressed);
}

void Stub::mouse_move_stub(void *user_data, int x, int y) {
    m_mouse_move(user_data, x, y);
}

void Stub::scroll_stub(void *user_data, KeyMod mod, float deltaX, float deltaY) {
    m_scroll(user_data, mod, deltaX, deltaY);
}
