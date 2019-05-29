#pragma once

#if defined(__cplusplus)

#include <functional>
#include "MiniFB.h"

template <class T>
void mfb_active_callback(T *obj, void (T::*method)(struct Window *, bool));

template <class T>
void mfb_resize_callback(T *obj, void (T::*method)(struct Window *, int, int));

template <class T>
void mfb_keyboard_callback(T *obj, void (T::*method)(struct Window *, Key, KeyMod, bool));

template <class T>
void mfb_char_input_callback(T *obj, void (T::*method)(struct Window *, unsigned int));

template <class T>
void mfb_mouse_button_callback(T *obj, void (T::*method)(struct Window *, MouseButton, KeyMod, bool));

template <class T>
void mfb_mouse_move_callback(T *obj, void (T::*method)(struct Window *, int, int));

template <class T>
void mfb_mouse_scroll_callback(T *obj, void (T::*method)(struct Window *, KeyMod, float, float));

//-------------------------------------
// To avoid clumsy hands
//-------------------------------------
class Stub {
    template <class T>
    friend void mfb_active_callback(T *obj, void (T::*method)(struct Window *, bool));
    template <class T>
    friend void mfb_resize_callback(T *obj, void (T::*method)(struct Window *, int, int));
    template <class T>
    friend void mfb_mouse_button_callback(T *obj, void (T::*method)(struct Window *, MouseButton, KeyMod, bool));
    template <class T>
    friend void mfb_keyboard_callback(T *obj, void (T::*method)(struct Window *, Key, KeyMod, bool));
    template <class T>
    friend void mfb_char_input_callback(T *obj, void (T::*method)(struct Window *, unsigned int));
    template <class T>
    friend void mfb_mouse_button_callback(T *obj, void (T::*method)(struct Window *, MouseButton, KeyMod, bool));
    template <class T>
    friend void mfb_mouse_move_callback(T *obj, void (T::*method)(struct Window *, int, int));
    template <class T>
    friend void mfb_mouse_scroll_callback(T *obj, void (T::*method)(struct Window *, KeyMod, float, float));

    static void active_stub(struct Window *window, bool isActive);
    static void resize_stub(struct Window *window, int width, int height);
    static void keyboard_stub(struct Window *window, Key key, KeyMod mod, bool isPressed);
    static void char_input_stub(struct Window *window, unsigned int);
    static void mouse_btn_stub(struct Window *window, MouseButton button, KeyMod mod, bool isPressed);
    static void mouse_move_stub(struct Window *window, int x, int y);
    static void scroll_stub(struct Window *window, KeyMod mod, float deltaX, float deltaY);

    static std::function<void(struct Window *window, bool)>                        m_active;
    static std::function<void(struct Window *window, int, int)>                    m_resize;
    static std::function<void(struct Window *window, Key, KeyMod, bool)>           m_keyboard;
    static std::function<void(struct Window *window, unsigned int)>                m_char_input;
    static std::function<void(struct Window *window, MouseButton, KeyMod, bool)>   m_mouse_btn;
    static std::function<void(struct Window *window, int, int)>                    m_mouse_move;
    static std::function<void(struct Window *window, KeyMod, float, float)>        m_scroll;
};

//-------------------------------------
template <class T>
inline void mfb_active_callback(T *obj, void (T::*method)(struct Window *window, bool)) {
    using namespace std::placeholders;

    Stub::m_active = std::bind(method, obj, _1, _2);
    mfb_active_callback(Stub::active_stub);
}

template <class T>
inline void mfb_resize_callback(T *obj, void (T::*method)(struct Window *window, int, int)) {
    using namespace std::placeholders;

    Stub::m_resize = std::bind(method, obj, _1, _2, _3);
    mfb_resize_callback(Stub::resize_stub);
}

template <class T>
inline void mfb_keyboard_callback(T *obj, void (T::*method)(struct Window *window, Key, KeyMod, bool)) {
    using namespace std::placeholders;

    Stub::m_keyboard = std::bind(method, obj, _1, _2, _3, _4);
    mfb_keyboard_callback(Stub::keyboard_stub);
}

template <class T>
inline void mfb_char_input_callback(T *obj, void (T::*method)(struct Window *window, unsigned int)) {
    using namespace std::placeholders;

    Stub::m_char_input = std::bind(method, obj, _1, _2);
    mfb_char_input_callback(Stub::char_input_stub);
}

template <class T>
inline void mfb_mouse_button_callback(T *obj, void (T::*method)(struct Window *window, MouseButton, KeyMod, bool)) {
    using namespace std::placeholders;

    Stub::m_mouse_btn = std::bind(method, obj, _1, _2, _3, _4);
    mfb_mouse_button_callback(Stub::mouse_btn_stub);
}

template <class T>
inline void mfb_mouse_move_callback(T *obj, void (T::*method)(struct Window *window, int, int)) {
    using namespace std::placeholders;

    Stub::m_mouse_move = std::bind(method, obj, _1, _2, _3);
    mfb_mouse_move_callback(Stub::mouse_move_stub);
}

template <class T>
inline void mfb_mouse_scroll_callback(T *obj, void (T::*method)(struct Window *window, KeyMod, float, float)) {
    using namespace std::placeholders;

    Stub::m_scroll = std::bind(method, obj, _1, _2, _3, _4);
    mfb_mouse_scroll_callback(Stub::scroll_stub);
}

#endif
