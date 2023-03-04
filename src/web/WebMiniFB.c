#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <emscripten.h>
#include <WindowData.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#define EM_EXPORT __attribute__((used))

static bool g_initialized = false;

EM_ASYNC_JS(void, setup_web_mfb, (), {
    // FIXME currently disabled. This will make requests pile up
    // and potentially execute multiple C calls in parallel (sort of).
    // which leads to stack/heap corruption.
    //
    // Wait for Emscripten to provide a RAF basd emscripten_sleep()
    //
    // Use requestAnimationFrame instead of setTimeout for async processing.
    // Asyncify.handleSleep(wakeUp => {
    //        requestAnimationFrame(wakeUp);
    // });

    window._minifb.keyMap = {
            "Space": 32,
            "Quote": 39,
            "Comma": 44,
            "Minus": 45,
            "Period": 46,
            "Slash": 47,
            "Digit0": 48,
            "Digit1": 49,
            "Digit2": 50,
            "Digit3": 51,
            "Digit4": 52,
            "Digit5": 53,
            "Digit6": 54,
            "Digit7": 55,
            "Digit8": 56,
            "Digit9": 57,
            "Semicolon": 59,
            "Equal": 61,
            "NumpadEqual": 61,
            "KeyA": 65,
            "KeyB": 66,
            "KeyC": 67,
            "KeyD": 68,
            "KeyE": 69,
            "KeyF": 70,
            "KeyG": 71,
            "KeyH": 72,
            "KeyI": 73,
            "KeyJ": 74,
            "KeyK": 75,
            "KeyL": 76,
            "KeyM": 77,
            "KeyN": 78,
            "KeyO": 79,
            "KeyP": 80,
            "KeyQ": 81,
            "KeyR": 82,
            "KeyS": 83,
            "KeyT": 84,
            "KeyU": 85,
            "KeyV": 86,
            "KeyW": 87,
            "KeyX": 88,
            "KeyY": 89,
            "KeyZ": 90,
            "BracketLeft": 91,
            "Backslash": 92,
            "BracketRight": 93,
            "Backquote": 96,

            "Escape": 256,
            "Enter": 257,
            "Tab": 258,
            "Backspace": 259,
            "Insert": 260,
            "Delete": 261,
            "ArrowRight": 262,
            "ArrowLeft": 263,
            "ArrowDown": 264,
            "ArrowUp": 265,
            "PageUp": 266,
            "PageDown": 267,
            "Home": 268,
            "End": 269,
            "CapsLock": 280,
            "ScrollLock": 281,
            "NumLock": 282,
            "PrintScreen": 283,
            "Pause": 284,
            "F1": 290,
            "F2": 291,
            "F3": 292,
            "F4": 293,
            "F5": 294,
            "F6": 295,
            "F7": 296,
            "F8": 297,
            "F9": 298,
            "F10": 299,
            "F11": 300,
            "F12": 301,
            "F13": 302,
            "F14": 303,
            "F15": 304,
            "F16": 305,
            "F17": 306,
            "F18": 307,
            "F19": 308,
            "F20": 309,
            "F21": 310,
            "F22": 311,
            "F23": 312,
            "F24": 313,
            "F25": 314,
            "Numpad0": 320,
            "Numpad1": 321,
            "Numpad2": 322,
            "Numpad3": 323,
            "Numpad4": 324,
            "Numpad5": 325,
            "Numpad6": 326,
            "Numpad7": 327,
            "Numpad8": 328,
            "Numpad9": 329,
            "NumpadComma": 330,
            "NumpadDivide": 331,
            "NumpadMultiply": 332,
            "NumpadSubtract": 333,
            "NumpadAdd": 334,
            "NumpadEnter": 335,
            "NumpadEqual": 336,
            "ShiftLeft": 340,
            "ControlLeft": 341,
            "AltLeft": 342,
            "MetaLeft": 343,
            "ShiftRight": 344,
            "ControlRight": 345,
            "AltRight": 346,
            "MetaRight": 347,
            "ContextMenu": 348
    };
})

EM_EXPORT void reverse_color_channels(uint8_t *src, uint8_t *dst, int width, int height) {
    int32_t numPixels = (width * height) << 2;
    for (int i = 0; i < numPixels; i += 4) {
        uint8_t b = src[i];
        uint8_t g = src[i + 1];
        uint8_t r = src[i + 2];
        uint8_t a = src[i + 3];
        dst[i] = r;
        dst[i + 1] = g;
        dst[i + 2] = b;
        dst[i + 3] = a;
    }
}

EM_EXPORT void window_data_set_mouse_pos(SWindowData *windowData, int x, int y) {
    if (!windowData) return;
    windowData->mouse_pos_x = x;
    windowData->mouse_pos_y = y;
}

EM_EXPORT void window_data_set_mouse_wheel(SWindowData *windowData, float x, float y) {
    if (!windowData) return;
    windowData->mouse_wheel_x = x;
    windowData->mouse_wheel_y = y;
}

EM_EXPORT void window_data_set_mouse_button(SWindowData *windowData, uint8_t button, bool is_pressed) {
    if (!windowData) return;
    if (button > 7) return;
    windowData->mouse_button_status[button] = is_pressed;
}

EM_EXPORT void window_data_set_key(SWindowData *windowData, unsigned key, bool is_pressed) {
    if (!windowData) return;
    if (key > 512) return;
    windowData->key_status[key] = is_pressed;
}

EM_EXPORT void window_data_set_mod_keys(SWindowData *windowData, uint32_t mod) {
    if (!windowData) return;
    windowData->mod_keys = mod;
}

EM_EXPORT void *window_data_get_specific(SWindowData *windowData) {
    if (!windowData) return 0;
    return windowData->specific;
}

EM_EXPORT void window_data_call_active_func(SWindowData *windowData, bool is_active) {
    if (windowData == 0x0) return;
    if (windowData->active_func) windowData->active_func((struct mfb_window*)windowData, is_active);
}

EM_EXPORT void window_data_call_resize_func(SWindowData *windowData, int width, int height) {
    if (windowData == 0x0) return;
    if (windowData->resize_func) windowData->resize_func((struct mfb_window*)windowData, width, height);
}

EM_EXPORT void window_data_call_close_func(SWindowData *windowData) {
    if (windowData == 0x0) return;
    if(windowData->close_func) windowData->close_func((struct mfb_window*)windowData);
}

EM_EXPORT void window_data_call_keyboard_func(SWindowData *windowData, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    if (windowData == 0x0) return;
    if (windowData->keyboard_func) windowData->keyboard_func((struct mfb_window*)windowData, key, mod, is_pressed);
}

EM_EXPORT void window_data_call_char_input_func(SWindowData *windowData, unsigned int code) {
    if (windowData == 0x0) return;
    if(windowData->char_input_func) windowData->char_input_func((struct mfb_window*)windowData, code);
}

EM_EXPORT void window_data_call_mouse_btn_func(SWindowData *windowData, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    if (windowData == 0x0) return;
    if (windowData->mouse_btn_func) windowData->mouse_btn_func((struct mfb_window*)windowData, button, mod, is_pressed);
}

EM_EXPORT void window_data_call_mouse_move_func(SWindowData *windowData, int x, int y) {
    if (windowData == 0x0) return;
    if (windowData->mouse_move_func) windowData->mouse_move_func((struct mfb_window*)windowData, x, y);
}

EM_EXPORT void window_data_call_mouse_wheel_func(SWindowData *windowData, mfb_key_mod mod, float x, float y) {
    if (windowData == 0x0) return;
    if (windowData->mouse_wheel_func) windowData->mouse_wheel_func((struct mfb_window*)windowData, mod, x, y);
}

EM_EXPORT bool window_data_get_close(SWindowData *windowData) {
    return windowData->close;
}

EM_JS(void*, mfb_open_ex_js,(SWindowData *windowData, const char *title, unsigned width, unsigned height, unsigned flags), {
    let canvas = document.getElementById(UTF8ToString(title));
    if (!canvas) return 0;

    if (!window._minifb) {
        window._minifb = {
            nextId: 1,
            windows: [],
        };
    }

    let id = window._minifb.nextId++;
    canvas.width = width;
    canvas.height = height;
    if (!canvas.style.width && !canvas.style.height) {
         canvas.style.width = width + "px";
        canvas.style.height = height + "px";
    }
    if (!canvas.style["image-rendering"]) canvas.style["image-rendering"] = "pixelated";
    if (!canvas.style["user-select"]) canvas.style["user-select"] = "none";
    if (!canvas.style["border"]) canvas.style["border"] = "none";
    if (!canvas.style["outline-style"]) canvas.style["outline-style"] = "none";
    canvas.tabIndex = -1;

    let w = {
        id: id,
        canvas: canvas,
        windowData: windowData,
        activeTouchId: null,
        events: [
            { type: "active" }
        ]
    };

    function toMfbCode(code) {
        return window._minifb.keyMap[code] ? window._minifb.keyMap[code] : -1;
    }


    function getMousePos(event) {
        let rect = canvas.getBoundingClientRect();
        let pos = { x: event.clientX - rect.left, y: event.clientY - rect.top };
        pos.x = pos.x / canvas.clientWidth * width;
        pos.y = pos.y / canvas.clientHeight * height;
        return pos;
    };

    function getMfbKeyModFromEvent(event) {
        // FIXME can we make these global somehow? --pre-js maybe?
        // FIXME need to lookup caps and num lock keystates in windowData->key_status
        const KB_MOD_SHIFT        = 0x0001;
        const KB_MOD_CONTROL      = 0x0002;
        const KB_MOD_ALT          = 0x0004;
        const KB_MOD_SUPER        = 0x0008;

        let mod = 0;
        if (event.shiftKey) mod = mod | KB_MOD_SHIFT;
        if (event.ctrlKey) mod = mod | KB_MOD_CONTROL;
        if (event.altKey) mod = mod | KB_MOD_ALT;
        if (event.metaKey) mod = mod | KB_MOD_SUPER;
        return mod;
    };

    canvas.addEventListener("keydown", (event) => {
        let code = toMfbCode(event.code);
        Module._window_data_set_key(windowData, code, 1);
        let mod = getMfbKeyModFromEvent(event);
        Module._window_data_set_mod_keys(windowData, mod);
        w.events.push({ type: "keydown", code: code, mod: mod });
    });

    canvas.addEventListener("keyup", (event) => {
        let code = toMfbCode(event.code);
        Module._window_data_set_key(windowData, code, 0);
        let mod = getMfbKeyModFromEvent(event);
        Module._window_data_set_mod_keys(windowData, mod);
        w.events.push({ type: "keyup", code: code, mod: mod });
    });

    canvas.addEventListener("mousedown", (event) => {
            if (event.button > 8) return;
            let pos = getMousePos(event);
            let mod = getMfbKeyModFromEvent(event);
            Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
            Module._window_data_set_mouse_button(windowData, event.button + 1, 1);
            Module._window_data_set_mod_keys(windowData, mod);
            w.events.push({ type: "mousebutton", button: event.button + 1, mod: mod, isPressed: true});
    }, false);

    canvas.addEventListener("mousemove", (event) => {
            let pos = getMousePos(event);
            Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
            w.events.push({ type: "mousemove", x: pos.x, y: pos.y});
    }, false);

    canvas.addEventListener("mouseup", (event) => {
            if (event.button > 8) return;
            let pos = getMousePos(event);
            let mod = getMfbKeyModFromEvent(event);
            Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
            Module._window_data_set_mouse_button(windowData, event.button + 1, 0);
            Module._window_data_set_mod_keys(windowData, mod);
            w.events.push({ type: "mousebutton", button: event.button + 1, mod: mod, isPressed: false});
    }, false);

    document.body.addEventListener("mouseup", (event) => {
            if (event.button > 8) return;
            let pos = getMousePos(event);
            let mod = getMfbKeyModFromEvent(event);
            Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
            Module._window_data_set_mouse_button(windowData, event.button + 1, 0);
            Module._window_data_set_mod_keys(windowData, mod);
            w.events.push({ type: "mousebutton", button: event.button + 1, mod: mod, isPressed: false});
    }, false);

    canvas.addEventListener('wheel', (event) => {
            event.preventDefault();
            let mod = getMfbKeyModFromEvent(event);
            Module._window_data_set_mouse_wheel(windowData, event.deltaX, event.deltaY);
            Module._window_data_set_mod_keys(windowData, mod);
            w.events.push({ type: "mousescroll", mod: mod, x: event.deltaX, y: event.deltaY});
    }, false);

    canvas.addEventListener("touchstart", (event) => {
            if (!w.activeTouchId) {
                let touch = event.changedTouches[0];
                let pos = getMousePos(touch);
                Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
                Module._window_data_set_mouse_button(windowData, 1, 1);
                w.activeTouchId = touch.identifier;
                w.events.push({ type: "mousebutton", button: 1, mod: 0, isPressed: true});
            }
            event.preventDefault();
    }, false);

    canvas.addEventListener("touchmove", (event) => {
            if (w.activeTouchId != null) {
                for (let i = 0; i < event.changedTouches.length; i++) {
                    let touch = event.changedTouches[i];
                    if (w.activeTouchId === touch.identifier) {
                        let pos = getMousePos(touch);
                        Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
                        w.events.push({ type: "mousemove", x: pos.x, y: pos.y});
                        break;
                    }
                }
            }
            event.preventDefault();
    }, false);

    function touchEndOrCancel(event) {
        if (w.activeTouchId != null) {
            for (let i = 0; i < event.changedTouches.length; i++) {
                let touch = event.changedTouches[i];
                if (w.activeTouchId === touch.identifier) {
                    let pos = getMousePos(touch);
                    Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
                    Module._window_data_set_mouse_button(windowData, 1, 0);
                    w.activeTouchId = null;
                    w.events.push({ type: "mousebutton", button: 1, mod: 0, isPressed: false});
                    break;
                }
            }
        }
        event.preventDefault();
    }
    canvas.addEventListener("touchend", touchEndOrCancel, false);
    canvas.addEventListener("touchcancel", touchEndOrCancel, false);

    window._minifb.windows[id] = w;
    return id;
});

struct mfb_window *mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    SWindowData *window_data;

    window_data = malloc(sizeof(SWindowData));
    if(window_data == 0x0) {
        printf("Cannot allocate window data\n");
        return 0x0;
    }
    memset(window_data, 0, sizeof(SWindowData));

    void *specific = mfb_open_ex_js(window_data, title, width, height, 0);
    if (!specific) {
        printf("Cannot allocate JavaScript window data\n");
        return 0x0;
    }
    window_data->specific = specific;

    // setup key map if not initialized yet
    if (!g_initialized) {
        setup_web_mfb();
        g_initialized = true;
    }

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

    window_data->is_active = true;
    window_data->is_initialized = true;

    return (struct mfb_window*)window_data;
}

bool mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    return false;
}

EM_JS(mfb_update_state, mfb_update_events_js, (SWindowData * windowData), {
    // FIXME can we make these global somehow? --pre-js maybe?
    const STATE_OK = 0;
    const STATE_EXIT = -1;
    const STATE_INVALID_WINDOW = -2;
    const STATE_INVALID_BUFFER = -3;
    const STATE_INTERNAL_ERROR = -4;
    if (windowData == 0) return STATE_INVALID_WINDOW;
    let windowId = Module._window_data_get_specific(windowData);
    if (!window._minifb) return STATE_INTERNAL_ERROR;
    if (!window._minifb.windows[windowId]) return STATE_INVALID_WINDOW;
    let w = window._minifb.windows[windowId];
    let events = w.events;
    w.events = [];
    for (let i = 0; i < events.length; i++) {
        let event = events[i];
        if (event.type == "active") {
            Module._window_data_call_active_func(windowData, 1);
        } else if (event.type == "mousebutton") {
            Module._window_data_call_mouse_btn_func(windowData, event.button, event.mod, event.isPressed ? 1 : 0);
        } else if (event.type == "mousemove") {
            Module._window_data_call_mouse_move_func(windowData, event.x, event.y);
        } else if (event.type == "mousescroll") {
            Module._window_data_call_mouse_wheel_func(windowData, event.mod, event.x, event.y);
        } else if (event.type == "keydown") {
            Module._window_data_call_keyboard_func(windowData, event.code, event.mod, 1);
        } else if (event.type == "keyup") {
            Module._window_data_call_keyboard_func(windowData, event.code, event.mod, 0);
        }
    }
    return Module._window_data_get_close(windowData);
});

mfb_update_state mfb_update_events(struct mfb_window *window) {
    if (window == 0x0) return STATE_INVALID_WINDOW;
    return mfb_update_events_js((SWindowData *)window);
}

EM_JS(mfb_update_state, mfb_update_js, (struct mfb_window * windowData, void *buffer, int width, int height), {
    // FIXME can we make these global somehow? preamble.js maybe?
    const STATE_OK = 0;
    const STATE_EXIT = -1;
    const STATE_INVALID_WINDOW = -2;
    const STATE_INVALID_BUFFER = -3;
    const STATE_INTERNAL_ERROR = -4;
    if (windowData == 0) return STATE_INVALID_WINDOW;
    let windowId = Module._window_data_get_specific(windowData);
    if (!window._minifb) return STATE_INTERNAL_ERROR;
    if (!window._minifb.windows[windowId]) return STATE_INVALID_WINDOW;
    if (buffer == 0) return STATE_INVALID_BUFFER;
    let w = window._minifb.windows[windowId];
    let canvas = w.canvas;
    if (width <= 0) {
        width = canvas.width;
        height = canvas.height;
    } else {
        if (canvas.width != width) canvas.width = width;
        if (canvas.height != height) canvas.height = height;
    }
    Module._reverse_color_channels(buffer, buffer, width, height);
    let framePixels = new Uint8ClampedArray(HEAPU8.buffer, buffer, width * height * 4);
    let imageData = new ImageData(framePixels, width, height);
    canvas.getContext("2d").putImageData(imageData, 0, 0);
    Module._reverse_color_channels(buffer, buffer, width, height);
    return Module._window_data_get_close(windowData);
});

mfb_update_state mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    mfb_update_state state = mfb_update_js(window, buffer, width, height);
    if (state != STATE_OK) return state;
    state = mfb_update_events_js((SWindowData *)window);
    return state;
}

bool mfb_wait_sync(struct mfb_window *window) {
    emscripten_sleep(0);
    return true;
}

void mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
    if (!window) return;
    if (scale_x) *scale_x = 1.0f;
    if (scale_y) *scale_y = 1.0f;
}

extern double g_timer_frequency;
extern double g_timer_resolution;

void mfb_timer_init(void) {
    g_timer_frequency  = 1e+9;
    g_timer_resolution = 1.0 / g_timer_frequency;
}

EM_JS(double, mfb_timer_tick_js, (), {
   return performance.now();
});

uint64_t mfb_timer_tick(void) {
    uint64_t now = (uint64_t)(mfb_timer_tick_js() * 1e+6);
    return now;
}
