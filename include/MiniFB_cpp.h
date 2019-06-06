#pragma once

#if defined(__cplusplus)

#include <functional>
#include "MiniFB.h"

template <class T>
void mfb_active_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, bool));

template <class T>
void mfb_resize_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, int, int));

template <class T>
void mfb_keyboard_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, Key, KeyMod, bool));

template <class T>
void mfb_char_input_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, unsigned int));

template <class T>
void mfb_mouse_button_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, MouseButton, KeyMod, bool));

template <class T>
void mfb_mouse_move_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, int, int));

template <class T>
void mfb_mouse_scroll_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, KeyMod, float, float));

//-------------------------------------
// To avoid clumsy hands
//-------------------------------------
class Stub {
    template <class T>
    friend void mfb_active_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, bool));
    template <class T>
    friend void mfb_resize_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, int, int));
    template <class T>
    friend void mfb_mouse_button_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, MouseButton, KeyMod, bool));
    template <class T>
    friend void mfb_keyboard_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, Key, KeyMod, bool));
    template <class T>
    friend void mfb_char_input_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, unsigned int));
    template <class T>
    friend void mfb_mouse_button_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, MouseButton, KeyMod, bool));
    template <class T>
    friend void mfb_mouse_move_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, int, int));
    template <class T>
    friend void mfb_mouse_scroll_callback(struct Window *window, T *obj, void (T::*method)(struct Window *, KeyMod, float, float));

    static Stub *GetInstance(struct Window *window);

    static void active_stub(struct Window *window, bool isActive);
    static void resize_stub(struct Window *window, int width, int height);
    static void keyboard_stub(struct Window *window, Key key, KeyMod mod, bool isPressed);
    static void char_input_stub(struct Window *window, unsigned int);
    static void mouse_btn_stub(struct Window *window, MouseButton button, KeyMod mod, bool isPressed);
    static void mouse_move_stub(struct Window *window, int x, int y);
    static void scroll_stub(struct Window *window, KeyMod mod, float deltaX, float deltaY);

    struct Window                                                           *m_window;
    std::function<void(struct Window *window, bool)>                        m_active;
    std::function<void(struct Window *window, int, int)>                    m_resize;
    std::function<void(struct Window *window, Key, KeyMod, bool)>           m_keyboard;
    std::function<void(struct Window *window, unsigned int)>                m_char_input;
    std::function<void(struct Window *window, MouseButton, KeyMod, bool)>   m_mouse_btn;
    std::function<void(struct Window *window, int, int)>                    m_mouse_move;
    std::function<void(struct Window *window, KeyMod, float, float)>        m_scroll;
};

//-------------------------------------
template <class T>
inline void mfb_active_callback(struct Window *window, T *obj, void (T::*method)(struct Window *window, bool)) {
    using namespace std::placeholders;

    Stub    *stub = Stub::GetInstance(window);
    stub->m_active = std::bind(method, obj, _1, _2);
    mfb_active_callback(window, Stub::active_stub);
}

template <class T>
inline void mfb_resize_callback(struct Window *window, T *obj, void (T::*method)(struct Window *window, int, int)) {
    using namespace std::placeholders;

    Stub    *stub = Stub::GetInstance(window);
    stub->m_resize = std::bind(method, obj, _1, _2, _3);
    mfb_resize_callback(window, Stub::resize_stub);
}

template <class T>
inline void mfb_keyboard_callback(struct Window *window, T *obj, void (T::*method)(struct Window *window, Key, KeyMod, bool)) {
    using namespace std::placeholders;

    Stub    *stub = Stub::GetInstance(window);
    stub->m_keyboard = std::bind(method, obj, _1, _2, _3, _4);
    mfb_keyboard_callback(window, Stub::keyboard_stub);
}

template <class T>
inline void mfb_char_input_callback(struct Window *window, T *obj, void (T::*method)(struct Window *window, unsigned int)) {
    using namespace std::placeholders;

    Stub    *stub = Stub::GetInstance(window);
    stub->m_char_input = std::bind(method, obj, _1, _2);
    mfb_char_input_callback(window, Stub::char_input_stub);
}

template <class T>
inline void mfb_mouse_button_callback(struct Window *window, T *obj, void (T::*method)(struct Window *window, MouseButton, KeyMod, bool)) {
    using namespace std::placeholders;

    Stub    *stub = Stub::GetInstance(window);
    stub->m_mouse_btn = std::bind(method, obj, _1, _2, _3, _4);
    mfb_mouse_button_callback(window, Stub::mouse_btn_stub);
}

template <class T>
inline void mfb_mouse_move_callback(struct Window *window, T *obj, void (T::*method)(struct Window *window, int, int)) {
    using namespace std::placeholders;

    Stub    *stub = Stub::GetInstance(window);
    stub->m_mouse_move = std::bind(method, obj, _1, _2, _3);
    mfb_mouse_move_callback(window, Stub::mouse_move_stub);
}

template <class T>
inline void mfb_mouse_scroll_callback(struct Window *window, T *obj, void (T::*method)(struct Window *window, KeyMod, float, float)) {
    using namespace std::placeholders;

    Stub    *stub = Stub::GetInstance(window);
    stub->m_scroll = std::bind(method, obj, _1, _2, _3, _4);
    mfb_mouse_scroll_callback(window, Stub::scroll_stub);
}

#endif
