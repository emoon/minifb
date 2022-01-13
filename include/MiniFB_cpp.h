#pragma once

#if defined(__cplusplus)

#include <functional>
#include "MiniFB.h"

//-------------------------------------
// To be able to distinguish these C++ functions, using std::function, from C functions, using raw function pointers, we need to reverse params order.
//
// Note that FROM the compiler point of view
//   mfb_set_XXX_callback(window, &my_c_func)
// and
//   mfb_set_XXX_callback(window, [](...) {})
// have the same parameters.
//-------------------------------------
void mfb_set_active_callback      (std::function<void(struct mfb_window *, bool)>                                func, struct mfb_window *window);
void mfb_set_resize_callback      (std::function<void(struct mfb_window *, int, int)>                            func, struct mfb_window *window);
void mfb_set_close_callback       (std::function<bool(struct mfb_window *)>                                      func, struct mfb_window *window);
void mfb_set_keyboard_callback    (std::function<void(struct mfb_window *, mfb_key, mfb_key_mod, bool)>          func, struct mfb_window *window);
void mfb_set_char_input_callback  (std::function<void(struct mfb_window *, unsigned int)>                        func, struct mfb_window *window);
void mfb_set_mouse_button_callback(std::function<void(struct mfb_window *, mfb_mouse_button, mfb_key_mod, bool)> func, struct mfb_window *window);
void mfb_set_mouse_move_callback  (std::function<void(struct mfb_window *, int, int)>                            func, struct mfb_window *window);
void mfb_set_mouse_scroll_callback(std::function<void(struct mfb_window *, mfb_key_mod, float, float)>           func, struct mfb_window *window);
//-------------------------------------

//-------------------------------------
template <class T>
void mfb_set_active_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, bool));

template <class T>
void mfb_set_resize_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, int, int));

template <class T>
void mfb_set_keyboard_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, mfb_key, mfb_key_mod, bool));

template <class T>
void mfb_set_char_input_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, unsigned int));

template <class T>
void mfb_set_mouse_button_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, mfb_mouse_button, mfb_key_mod, bool));

template <class T>
void mfb_set_mouse_move_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, int, int));

template <class T>
void mfb_set_mouse_scroll_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, mfb_key_mod, float, float));
//-------------------------------------

//-------------------------------------
// To avoid clumsy hands
//-------------------------------------
class mfb_stub {
    mfb_stub() : m_window(0x0) {}

    friend void mfb_set_active_callback      (std::function<void(struct mfb_window *window, bool)>                          func, struct mfb_window *window);
    friend void mfb_set_resize_callback      (std::function<void(struct mfb_window *, int, int)>                            func, struct mfb_window *window);
    friend void mfb_set_close_callback       (std::function<bool(struct mfb_window *)>                                      func, struct mfb_window *window);
    friend void mfb_set_keyboard_callback    (std::function<void(struct mfb_window *, mfb_key, mfb_key_mod, bool)>          func, struct mfb_window *window);
    friend void mfb_set_char_input_callback  (std::function<void(struct mfb_window *, unsigned int)>                        func, struct mfb_window *window);
    friend void mfb_set_mouse_button_callback(std::function<void(struct mfb_window *, mfb_mouse_button, mfb_key_mod, bool)> func, struct mfb_window *window);
    friend void mfb_set_mouse_move_callback  (std::function<void(struct mfb_window *, int, int)>                            func, struct mfb_window *window);
    friend void mfb_set_mouse_scroll_callback(std::function<void(struct mfb_window *, mfb_key_mod, float, float)>           func, struct mfb_window *window);

    template <class T>
    friend void mfb_set_active_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, bool));
    template <class T>
    friend void mfb_set_resize_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, int, int));
    template <class T>
    friend void mfb_set_close_callback(struct mfb_window *window, T *obj, bool (T::*method)(struct mfb_window *));
    template <class T>
    friend void mfb_set_mouse_button_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, mfb_mouse_button, mfb_key_mod, bool));
    template <class T>
    friend void mfb_set_keyboard_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, mfb_key, mfb_key_mod, bool));
    template <class T>
    friend void mfb_set_char_input_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, unsigned int));
    template <class T>
    friend void mfb_set_mouse_button_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, mfb_mouse_button, mfb_key_mod, bool));
    template <class T>
    friend void mfb_set_mouse_move_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, int, int));
    template <class T>
    friend void mfb_set_mouse_scroll_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *, mfb_key_mod, float, float));

    static mfb_stub *GetInstance(struct mfb_window *window);

    static void active_stub(struct mfb_window *window, bool isActive);
    static void resize_stub(struct mfb_window *window, int width, int height);
    static bool close_stub(struct mfb_window *window);
    static void keyboard_stub(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed);
    static void char_input_stub(struct mfb_window *window, unsigned int);
    static void mouse_btn_stub(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed);
    static void mouse_move_stub(struct mfb_window *window, int x, int y);
    static void scroll_stub(struct mfb_window *window, mfb_key_mod mod, float deltaX, float deltaY);

    struct mfb_window                                                           *m_window;
    std::function<void(struct mfb_window *window, bool)>                        m_active;
    std::function<void(struct mfb_window *window, int, int)>                    m_resize;
    std::function<bool(struct mfb_window *window)>                              m_close;
    std::function<void(struct mfb_window *window, mfb_key, mfb_key_mod, bool)>  m_keyboard;
    std::function<void(struct mfb_window *window, unsigned int)>                m_char_input;
    std::function<void(struct mfb_window *window, mfb_mouse_button, mfb_key_mod, bool)>   m_mouse_btn;
    std::function<void(struct mfb_window *window, int, int)>                    m_mouse_move;
    std::function<void(struct mfb_window *window, mfb_key_mod, float, float)>   m_scroll;
};

//-------------------------------------
template <class T>
inline void mfb_set_active_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *window, bool)) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_active = std::bind(method, obj, _1, _2);
    mfb_set_active_callback(window, mfb_stub::active_stub);
}

//-------------------------------------
template <class T>
inline void mfb_set_resize_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *window, int, int)) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_resize = std::bind(method, obj, _1, _2, _3);
    mfb_set_resize_callback(window, mfb_stub::resize_stub);
}

//-------------------------------------
template <class T>
inline void mfb_set_close_callback(struct mfb_window *window, T *obj, bool (T::*method)(struct mfb_window *window)) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_close = std::bind(method, obj, _1);
    mfb_set_close_callback(window, mfb_stub::close_stub);
}

//-------------------------------------
template <class T>
inline void mfb_set_keyboard_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *window, mfb_key, mfb_key_mod, bool)) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_keyboard = std::bind(method, obj, _1, _2, _3, _4);
    mfb_set_keyboard_callback(window, mfb_stub::keyboard_stub);
}

//-------------------------------------
template <class T>
inline void mfb_set_char_input_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *window, unsigned int)) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_char_input = std::bind(method, obj, _1, _2);
    mfb_set_char_input_callback(window, mfb_stub::char_input_stub);
}

//-------------------------------------
template <class T>
inline void mfb_set_mouse_button_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *window, mfb_mouse_button, mfb_key_mod, bool)) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_mouse_btn = std::bind(method, obj, _1, _2, _3, _4);
    mfb_set_mouse_button_callback(window, mfb_stub::mouse_btn_stub);
}

//-------------------------------------
template <class T>
inline void mfb_set_mouse_move_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *window, int, int)) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_mouse_move = std::bind(method, obj, _1, _2, _3);
    mfb_set_mouse_move_callback(window, mfb_stub::mouse_move_stub);
}

//-------------------------------------
template <class T>
inline void mfb_set_mouse_scroll_callback(struct mfb_window *window, T *obj, void (T::*method)(struct mfb_window *window, mfb_key_mod, float, float)) {
    using namespace std::placeholders;

    mfb_stub    *stub = mfb_stub::GetInstance(window);
    stub->m_scroll = std::bind(method, obj, _1, _2, _3, _4);
    mfb_set_mouse_scroll_callback(window, mfb_stub::scroll_stub);
}

#endif
