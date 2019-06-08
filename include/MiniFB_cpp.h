#pragma once

#if defined(__cplusplus)

#include <functional>
#include "MiniFB.h"

template <class T>
void mfb_active_callback(T *obj, void (T::*method)(void *, bool));

template <class T>
void mfb_resize_callback(T *obj, void (T::*method)(void *, int, int));

template <class T>
void mfb_keyboard_callback(T *obj, void (T::*method)(void *, Key, KeyMod, bool));

template <class T>
void mfb_char_input_callback(T *obj, void (T::*method)(void *, unsigned int));

template <class T>
void mfb_mouse_button_callback(T *obj, void (T::*method)(void *, MouseButton, KeyMod, bool));

template <class T>
void mfb_mouse_move_callback(T *obj, void (T::*method)(void *, int, int));

template <class T>
void mfb_mouse_scroll_callback(T *obj, void (T::*method)(void *, KeyMod, float, float));

//-------------------------------------
// To avoid clumsy hands
//-------------------------------------
class Stub {
    template <class T>
    friend void mfb_active_callback(T *obj, void (T::*method)(void *, bool));
    template <class T>
    friend void mfb_resize_callback(T *obj, void (T::*method)(void *, int, int));
    template <class T>
    friend void mfb_mouse_button_callback(T *obj, void (T::*method)(void *, MouseButton, KeyMod, bool));
    template <class T>
    friend void mfb_keyboard_callback(T *obj, void (T::*method)(void *, Key, KeyMod, bool));
    template <class T>
    friend void mfb_char_input_callback(T *obj, void (T::*method)(void *, unsigned int));
    template <class T>
    friend void mfb_mouse_button_callback(T *obj, void (T::*method)(void *, MouseButton, KeyMod, bool));
    template <class T>
    friend void mfb_mouse_move_callback(T *obj, void (T::*method)(void *, int, int));
    template <class T>
    friend void mfb_mouse_scroll_callback(T *obj, void (T::*method)(void *, KeyMod, float, float));

    static void active_stub(void *user_data, bool isActive);
    static void resize_stub(void *user_data, int width, int height);
    static void keyboard_stub(void *user_data, Key key, KeyMod mod, bool isPressed);
    static void char_input_stub(void *user_data, unsigned int);
    static void mouse_btn_stub(void *user_data, MouseButton button, KeyMod mod, bool isPressed);
    static void mouse_move_stub(void *user_data, int x, int y);
    static void scroll_stub(void *user_data, KeyMod mod, float deltaX, float deltaY);

    static std::function<void(void *user_data, bool)>                        m_active;
    static std::function<void(void *user_data, int, int)>                    m_resize;
    static std::function<void(void *user_data, Key, KeyMod, bool)>           m_keyboard;
    static std::function<void(void *user_data, unsigned int)>                m_char_input;
    static std::function<void(void *user_data, MouseButton, KeyMod, bool)>   m_mouse_btn;
    static std::function<void(void *user_data, int, int)>                    m_mouse_move;
    static std::function<void(void *user_data, KeyMod, float, float)>        m_scroll;
};

//-------------------------------------
template <class T>
inline void mfb_active_callback(T *obj, void (T::*method)(void *user_data, bool)) {
    using namespace std::placeholders;

    Stub::m_active = std::bind(method, obj, _1, _2);
    mfb_active_callback(Stub::active_stub);
}

template <class T>
inline void mfb_resize_callback(T *obj, void (T::*method)(void *user_data, int, int)) {
    using namespace std::placeholders;

    Stub::m_resize = std::bind(method, obj, _1, _2, _3);
    mfb_resize_callback(Stub::resize_stub);
}

template <class T>
inline void mfb_keyboard_callback(T *obj, void (T::*method)(void *user_data, Key, KeyMod, bool)) {
    using namespace std::placeholders;

    Stub::m_keyboard = std::bind(method, obj, _1, _2, _3, _4);
    mfb_keyboard_callback(Stub::keyboard_stub);
}

template <class T>
inline void mfb_char_input_callback(T *obj, void (T::*method)(void *user_data, unsigned int)) {
    using namespace std::placeholders;

    Stub::m_char_input = std::bind(method, obj, _1, _2);
    mfb_char_input_callback(Stub::char_input_stub);
}

template <class T>
inline void mfb_mouse_button_callback(T *obj, void (T::*method)(void *user_data, MouseButton, KeyMod, bool)) {
    using namespace std::placeholders;

    Stub::m_mouse_btn = std::bind(method, obj, _1, _2, _3, _4);
    mfb_mouse_button_callback(Stub::mouse_btn_stub);
}

template <class T>
inline void mfb_mouse_move_callback(T *obj, void (T::*method)(void *user_data, int, int)) {
    using namespace std::placeholders;

    Stub::m_mouse_move = std::bind(method, obj, _1, _2, _3);
    mfb_mouse_move_callback(Stub::mouse_move_stub);
}

template <class T>
inline void mfb_mouse_scroll_callback(T *obj, void (T::*method)(void *user_data, KeyMod, float, float)) {
    using namespace std::placeholders;

    Stub::m_scroll = std::bind(method, obj, _1, _2, _3, _4);
    mfb_mouse_scroll_callback(Stub::scroll_stub);
}

#endif
