#include <MiniFB_cpp.h>
#include <MiniFB_enums.h>
#include <vector>

struct mfb_stub_vector {
    std::vector<mfb_stub *> instances;

    mfb_stub_vector() = default;

    ~mfb_stub_vector() {
        for (mfb_stub *instance : instances) {
            if (instance != nullptr) {
                delete instance;
            }
        }
        instances.clear();
    }

    mfb_stub *get(struct mfb_window *window) {
        mfb_stub *free_slot = nullptr;
        for (mfb_stub *instance : instances) {
            if (instance == nullptr)
                continue;
            if (instance->m_window == window) {
                return instance;
            }
            if (instance->m_window == nullptr && free_slot == nullptr) {
                free_slot = instance;
            }
        }

        if (free_slot != nullptr) {
            free_slot->m_window = window;
            return free_slot;
        }

        instances.push_back(new mfb_stub);
        instances.back()->m_window = window;

        return instances.back();
    }

    void release(struct mfb_window *window) {
        for (mfb_stub *instance : instances) {
            if (instance == nullptr || instance->m_window != window) {
                continue;
            }

            instance->m_window = nullptr;
            instance->m_active = {};
            instance->m_resize = {};
            instance->m_close = {};
            instance->m_keyboard = {};
            instance->m_char_input = {};
            instance->m_mouse_btn = {};
            instance->m_mouse_move = {};
            instance->m_scroll = {};
            return;
        }
    }
};

static mfb_stub_vector &
get_stub_instances() {
    static mfb_stub_vector s_instances;
    return s_instances;
}

//-------------------------------------
mfb_stub *
mfb_stub::get_instance(struct mfb_window *window) {
    return get_stub_instances().get(window);
}

//-------------------------------------
extern "C" void
release_cpp_stub(struct mfb_window *window) {
    if (window == nullptr) {
        return;
    }

    get_stub_instances().release(window);
}

//-------------------------------------
void
mfb_stub::active_stub(struct mfb_window *window, bool is_active) {
    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_active(window, is_active);
}

//-------------------------------------
void
mfb_stub::resize_stub(struct mfb_window *window, int width, int height) {
    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_resize(window, width, height);
}

//-------------------------------------
bool
mfb_stub::close_stub(struct mfb_window *window) {
    mfb_stub    *stub = mfb_stub::get_instance(window);
    return stub->m_close(window);
}

//-------------------------------------
void
mfb_stub::keyboard_stub(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_keyboard(window, key, mod, is_pressed);
}

//-------------------------------------
void
mfb_stub::char_input_stub(struct mfb_window *window, unsigned int code) {
    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_char_input(window, code);
}

//-------------------------------------
void
mfb_stub::mouse_btn_stub(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_mouse_btn(window, button, mod, is_pressed);
}

//-------------------------------------
void
mfb_stub::mouse_move_stub(struct mfb_window *window, int x, int y) {
    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_mouse_move(window, x, y);
}

//-------------------------------------
void
mfb_stub::scroll_stub(struct mfb_window *window, mfb_key_mod mod, float delta_x, float delta_y) {
    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_scroll(window, mod, delta_x, delta_y);
}

//-------------------------------------

//-------------------------------------
void
mfb_set_active_callback(std::function<void(struct mfb_window *, bool)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_active = std::bind(func, _1, _2);
    mfb_set_active_callback(window, mfb_stub::active_stub);
}

//-------------------------------------
void
mfb_set_resize_callback(std::function<void(struct mfb_window *, int, int)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_resize = std::bind(func, _1, _2, _3);
    mfb_set_resize_callback(window, mfb_stub::resize_stub);
}

//-------------------------------------
void
mfb_set_close_callback(std::function<bool(struct mfb_window *)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_close = std::bind(func, _1);
    mfb_set_close_callback(window, mfb_stub::close_stub);
}

//-------------------------------------
void
mfb_set_keyboard_callback(std::function<void(struct mfb_window *, mfb_key, mfb_key_mod, bool)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_keyboard = std::bind(func, _1, _2, _3, _4);
    mfb_set_keyboard_callback(window, mfb_stub::keyboard_stub);
}

//-------------------------------------
void
mfb_set_char_input_callback(std::function<void(struct mfb_window *, unsigned int)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_char_input = std::bind(func, _1, _2);
    mfb_set_char_input_callback(window, mfb_stub::char_input_stub);
}

//-------------------------------------
void
mfb_set_mouse_button_callback(std::function<void(struct mfb_window *, mfb_mouse_button, mfb_key_mod, bool)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_mouse_btn = std::bind(func, _1, _2, _3, _4);
    mfb_set_mouse_button_callback(window, mfb_stub::mouse_btn_stub);
}

//-------------------------------------
void
mfb_set_mouse_move_callback(std::function<void(struct mfb_window *, int, int)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_mouse_move = std::bind(func, _1, _2, _3);
    mfb_set_mouse_move_callback(window, mfb_stub::mouse_move_stub);
}

//-------------------------------------
void
mfb_set_mouse_scroll_callback(std::function<void(struct mfb_window *, mfb_key_mod, float, float)> func, struct mfb_window *window) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::get_instance(window);
    stub->m_scroll = std::bind(func, _1, _2, _3, _4);
    mfb_set_mouse_scroll_callback(window, mfb_stub::scroll_stub);
}
