#include <MiniFB_cpp.h>
#include <MiniFB_enums.h>
#include <vector>

//-------------------------------------
mfb_stub *
mfb_stub::GetInstance(struct mfb_window *window) {
    static std::vector<mfb_stub *>  s_instances;

    for(mfb_stub *instance : s_instances) {
        if(instance->m_window == window) {
            return instance;
        }
    }

    s_instances.push_back(new mfb_stub);
    s_instances.back()->m_window = window;

    return s_instances.back();
}

//-------------------------------------
void 
mfb_stub::active_stub(struct mfb_window *window, bool isActive) {
    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_active(window, isActive);
}

//-------------------------------------
void 
mfb_stub::resize_stub(struct mfb_window *window, int width, int height) {
    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_resize(window, width, height);
}

//-------------------------------------
void 
mfb_stub::keyboard_stub(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_keyboard(window, key, mod, isPressed);
}

//-------------------------------------
void 
mfb_stub::char_input_stub(struct mfb_window *window, unsigned int code) {
    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_char_input(window, code);
}

//-------------------------------------
void 
mfb_stub::mouse_btn_stub(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed) {
    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_mouse_btn(window, button, mod, isPressed);
}

//-------------------------------------
void 
mfb_stub::mouse_move_stub(struct mfb_window *window, int x, int y) {
    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_mouse_move(window, x, y);
}

//-------------------------------------
void 
mfb_stub::scroll_stub(struct mfb_window *window, mfb_key_mod mod, float deltaX, float deltaY) {
    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_scroll(window, mod, deltaX, deltaY);
}
