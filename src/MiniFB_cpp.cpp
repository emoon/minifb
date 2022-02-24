#include <MiniFB_cpp.h>
#include <MiniFB_enums.h>
#include <vector>

//-------------------------------------
mfb_stub *
mfb_stub::GetInstance(struct mfb_window *window) {
    struct stub_vector {
        std::vector<mfb_stub *> instances;

        stub_vector() = default;

        ~stub_vector() {
            for(mfb_stub *instance : instances)
                delete instance;
        }

        mfb_stub *Get(struct mfb_window *window) {
            for(mfb_stub *instance : instances) {
                if(instance->m_window == window) {
                    return instance;
                }
            }

            instances.push_back(new mfb_stub);
            instances.back()->m_window = window;

            return instances.back();
        }
    };

    static stub_vector s_instances;

    return s_instances.Get(window);
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
bool
mfb_stub::close_stub(struct mfb_window *window) {
    mfb_stub    *stub = mfb_stub::GetInstance(window);
    return stub->m_close(window);
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

//-------------------------------------

//-------------------------------------
void
mfb_set_active_callback(std::function<void(struct mfb_window *, bool)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_active = std::bind(func, _1, _2);
    mfb_set_active_callback(window, mfb_stub::active_stub);
}

//-------------------------------------
void
mfb_set_resize_callback(std::function<void(struct mfb_window *, int, int)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_resize = std::bind(func, _1, _2, _3);
    mfb_set_resize_callback(window, mfb_stub::resize_stub);
}

//-------------------------------------
void
mfb_set_close_callback(std::function<bool(struct mfb_window *)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_close = std::bind(func, _1);
    mfb_set_close_callback(window, mfb_stub::close_stub);
}

//-------------------------------------
void
mfb_set_keyboard_callback(std::function<void(struct mfb_window *, mfb_key, mfb_key_mod, bool)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_keyboard = std::bind(func, _1, _2, _3, _4);
    mfb_set_keyboard_callback(window, mfb_stub::keyboard_stub);
}

//-------------------------------------
void
mfb_set_char_input_callback(std::function<void(struct mfb_window *, unsigned int)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_char_input = std::bind(func, _1, _2);
    mfb_set_char_input_callback(window, mfb_stub::char_input_stub);
}

//-------------------------------------
void
mfb_set_mouse_button_callback(std::function<void(struct mfb_window *, mfb_mouse_button, mfb_key_mod, bool)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_mouse_btn = std::bind(func, _1, _2, _3, _4);
    mfb_set_mouse_button_callback(window, mfb_stub::mouse_btn_stub);
}

//-------------------------------------
void
mfb_set_mouse_move_callback(std::function<void(struct mfb_window *, int, int)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_mouse_move = std::bind(func, _1, _2, _3);
    mfb_set_mouse_move_callback(window, mfb_stub::mouse_move_stub);
}

//-------------------------------------
void
mfb_set_mouse_scroll_callback(std::function<void(struct mfb_window *, mfb_key_mod, float, float)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_scroll = std::bind(func, _1, _2, _3, _4);
    mfb_set_mouse_scroll_callback(window, mfb_stub::scroll_stub);
}
