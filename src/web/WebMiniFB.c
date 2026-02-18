#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <emscripten.h>
#include <WindowData.h>
#include "WindowData_Web.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define EM_EXPORT __attribute__((used))

static bool g_initialized = false;
static bool g_cursor_warning_logged = false;

static SWindowData_Web *
mfb_web_get_data(SWindowData *window_data) {
    if (window_data == NULL) {
        return NULL;
    }

    return (SWindowData_Web *) window_data->specific;
}

static uint8_t *
mfb_web_ensure_swizzle_buffer(SWindowData *window_data, unsigned width, unsigned height) {
    SWindowData_Web *window_data_web = mfb_web_get_data(window_data);
    if (window_data_web == NULL || width == 0 || height == 0) {
        return NULL;
    }

    if ((size_t) width > SIZE_MAX / (size_t) height) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: swizzle buffer size overflow for %ux%u.", width, height);
        return NULL;
    }

    size_t pixels = (size_t) width * (size_t) height;
    if (pixels > SIZE_MAX / 4) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: swizzle buffer byte size overflow for %ux%u.", width, height);
        return NULL;
    }

    size_t required = pixels * 4;
    if (required > window_data_web->swizzle_buffer_size) {
        uint8_t *swizzle_buffer = realloc(window_data_web->swizzle_buffer, required);
        if (swizzle_buffer == NULL) {
            mfb_log(MFB_LOG_ERROR, "WebMiniFB: failed to allocate %zu bytes for swizzle buffer.", required);
            return NULL;
        }
        window_data_web->swizzle_buffer = swizzle_buffer;
        window_data_web->swizzle_buffer_size = required;
    }

    return window_data_web->swizzle_buffer;
}

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
    if (key >= 512) return;
    windowData->key_status[key] = is_pressed;
}

EM_EXPORT void window_data_set_mod_keys(SWindowData *windowData, uint32_t mod) {
    if (!windowData) return;
    windowData->mod_keys = mod;
}

EM_EXPORT void window_data_set_active(SWindowData *windowData, bool is_active) {
    if (!windowData) return;
    windowData->is_active = is_active;
}

EM_EXPORT void window_data_set_buffer_size(SWindowData *windowData, unsigned width, unsigned height) {
    if (!windowData) return;
    if (width > (UINT32_MAX / 4)) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: buffer stride overflow for width=%u.", width);
        windowData->buffer_width = 0;
        windowData->buffer_height = 0;
        windowData->buffer_stride = 0;
        return;
    }
    windowData->buffer_width = width;
    windowData->buffer_height = height;
    windowData->buffer_stride = width * 4;
}

EM_EXPORT void window_data_update_window_size(SWindowData *windowData, unsigned width, unsigned height) {
    if (!windowData || width == 0 || height == 0) return;

    bool changed = (windowData->window_width != width) || (windowData->window_height != height);
    windowData->window_width = width;
    windowData->window_height = height;

    if (windowData->dst_width == 0 || windowData->dst_height == 0) {
        windowData->dst_offset_x = 0;
        windowData->dst_offset_y = 0;
        windowData->dst_width = width;
        windowData->dst_height = height;
        calc_dst_factor(windowData, width, height);
    }
    else if (changed) {
        resize_dst(windowData, width, height);
    }

    if (changed && windowData->resize_func) {
        windowData->resize_func((struct mfb_window *) windowData, (int) width, (int) height);
    }
}

EM_EXPORT void *window_data_get_specific(SWindowData *windowData) {
    SWindowData_Web *window_data_web = mfb_web_get_data(windowData);
    if (window_data_web == NULL) return 0;
    return (void *) window_data_web->window_id;
}

EM_EXPORT uint8_t *window_data_get_swizzle_buffer(SWindowData *windowData, unsigned width, unsigned height) {
    return mfb_web_ensure_swizzle_buffer(windowData, width, height);
}

EM_EXPORT unsigned window_data_get_dst_offset_x(SWindowData *windowData) {
    if (!windowData) return 0;
    return windowData->dst_offset_x;
}

EM_EXPORT unsigned window_data_get_dst_offset_y(SWindowData *windowData) {
    if (!windowData) return 0;
    return windowData->dst_offset_y;
}

EM_EXPORT unsigned window_data_get_dst_width(SWindowData *windowData) {
    if (!windowData) return 0;
    return windowData->dst_width;
}

EM_EXPORT unsigned window_data_get_dst_height(SWindowData *windowData) {
    if (!windowData) return 0;
    return windowData->dst_height;
}

EM_EXPORT void window_data_call_active_func(SWindowData *windowData, bool is_active) {
    if (windowData == NULL) return;
    if (windowData->active_func) windowData->active_func((struct mfb_window*)windowData, is_active);
}

EM_EXPORT void window_data_call_resize_func(SWindowData *windowData, int width, int height) {
    if (windowData == NULL) return;
    if (windowData->resize_func) windowData->resize_func((struct mfb_window*)windowData, width, height);
}

EM_EXPORT void window_data_call_close_func(SWindowData *windowData) {
    if (windowData == NULL) return;
    if (windowData->close_func) windowData->close_func((struct mfb_window*)windowData);
}

EM_EXPORT void window_data_call_keyboard_func(SWindowData *windowData, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    if (windowData == NULL) return;
    if (windowData->keyboard_func) windowData->keyboard_func((struct mfb_window*)windowData, key, mod, is_pressed);
}

EM_EXPORT void window_data_call_char_input_func(SWindowData *windowData, unsigned int code) {
    if (windowData == NULL) return;
    if (windowData->char_input_func) windowData->char_input_func((struct mfb_window*)windowData, code);
}

EM_EXPORT void window_data_call_mouse_btn_func(SWindowData *windowData, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    if (windowData == NULL) return;
    if (windowData->mouse_btn_func) windowData->mouse_btn_func((struct mfb_window*)windowData, button, mod, is_pressed);
}

EM_EXPORT void window_data_call_mouse_move_func(SWindowData *windowData, int x, int y) {
    if (windowData == NULL) return;
    if (windowData->mouse_move_func) windowData->mouse_move_func((struct mfb_window*)windowData, x, y);
}

EM_EXPORT void window_data_call_mouse_wheel_func(SWindowData *windowData, mfb_key_mod mod, float x, float y) {
    if (windowData == NULL) return;
    if (windowData->mouse_wheel_func) windowData->mouse_wheel_func((struct mfb_window*)windowData, mod, x, y);
}

EM_EXPORT bool window_data_get_close(SWindowData *windowData) {
    if (!windowData) return true;
    return windowData->close;
}

EM_JS(void*, mfb_open_ex_js,(SWindowData *windowData, const char *title, unsigned width, unsigned height, int wantsFullscreen), {
    let canvas = document.getElementById(UTF8ToString(title));
    if (!canvas) return 0;

    if (!window._minifb) {
        window._minifb = {
            nextId: 1,
            windows: [],
        };
    }

    const MAX_QUEUED_EVENTS = 2048;
    const NON_PASSIVE = { passive: false };

    let id = window._minifb.nextId++;
    canvas.width = width;
    canvas.height = height;
    if (!canvas.style.width && !canvas.style.height) {
         canvas.style.width = width + "px";
        canvas.style.height = height + "px";
    }
    if (!canvas.style["image-rendering"]) canvas.style["image-rendering"] = "pixelated";
    if (!canvas.style["user-select"]) canvas.style["user-select"] = "none";
    if (!canvas.style["touch-action"]) canvas.style["touch-action"] = "none";
    if (!canvas.style["border"]) canvas.style["border"] = "none";
    if (!canvas.style["outline-style"]) canvas.style["outline-style"] = "none";
    canvas.tabIndex = -1;

    let ctx = canvas.getContext("2d");
    if (!ctx) return 0;

    let w = {
        id: id,
        canvas: canvas,
        ctx: ctx,
        backCanvas: null,
        backCtx: null,
        windowData: windowData,
        wantsFullscreen: wantsFullscreen !== 0,
        activeTouchId: null,
        isActive: true,
        handlers: {},
        events: [
            { type: "active", isActive: true }
        ]
    };

    Module._window_data_update_window_size(windowData, canvas.width, canvas.height);

    function toMfbCode(code) {
        return window._minifb.keyMap[code] ? window._minifb.keyMap[code] : -1;
    }

    function enqueueEvent(eventObj) {
        if (w.events.length >= MAX_QUEUED_EVENTS) {
            // Drop oldest input when producer outruns consumer to cap memory growth.
            w.events.shift();
        }
        w.events.push(eventObj);
    }

    function requestFullscreenIfNeeded() {
        if (!w.wantsFullscreen) return;
        w.wantsFullscreen = false;
        if (canvas.requestFullscreen) {
            canvas.requestFullscreen().catch(() => {});
        }
    }

    function shouldPreventDefaultKey(code) {
        return code === "ArrowUp" ||
               code === "ArrowDown" ||
               code === "ArrowLeft" ||
               code === "ArrowRight" ||
               code === "PageUp" ||
               code === "PageDown" ||
               code === "Home" ||
               code === "End" ||
               code === "Space";
    }

    function setActive(isActive) {
        if (w.isActive === isActive) return;
        w.isActive = isActive;
        Module._window_data_set_active(windowData, isActive ? 1 : 0);
        enqueueEvent({ type: "active", isActive: isActive });
    }

    function getMousePos(event) {
        let rect = canvas.getBoundingClientRect();
        let pos = { x: event.clientX - rect.left, y: event.clientY - rect.top };
        let clientWidth = canvas.clientWidth > 0 ? canvas.clientWidth : canvas.width;
        let clientHeight = canvas.clientHeight > 0 ? canvas.clientHeight : canvas.height;
        pos.x = pos.x / clientWidth * canvas.width;
        pos.y = pos.y / clientHeight * canvas.height;
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

    w.handlers.keydown = (event) => {
        if (shouldPreventDefaultKey(event.code)) {
            event.preventDefault();
        }
        let code = toMfbCode(event.code);
        Module._window_data_set_key(windowData, code, 1);
        let mod = getMfbKeyModFromEvent(event);
        Module._window_data_set_mod_keys(windowData, mod);
        enqueueEvent({ type: "keydown", code: code, mod: mod });
    };
    canvas.addEventListener("keydown", w.handlers.keydown);

    w.handlers.keyup = (event) => {
        if (shouldPreventDefaultKey(event.code)) {
            event.preventDefault();
        }
        let code = toMfbCode(event.code);
        Module._window_data_set_key(windowData, code, 0);
        let mod = getMfbKeyModFromEvent(event);
        Module._window_data_set_mod_keys(windowData, mod);
        enqueueEvent({ type: "keyup", code: code, mod: mod });
    };
    canvas.addEventListener("keyup", w.handlers.keyup);

    w.handlers.keypress = (event) => {
        if (!event.key || event.key.length === 0) return;
        let codePoint = event.key.codePointAt(0);
        if (codePoint !== undefined) {
            enqueueEvent({ type: "char", code: codePoint });
        }
    };
    canvas.addEventListener("keypress", w.handlers.keypress);

    w.handlers.focus = () => {
        setActive(true);
    };
    canvas.addEventListener("focus", w.handlers.focus);

    w.handlers.blur = () => {
        setActive(false);
    };
    canvas.addEventListener("blur", w.handlers.blur);

    w.handlers.windowFocus = () => {
        setActive(true);
    };
    window.addEventListener("focus", w.handlers.windowFocus);

    w.handlers.windowBlur = () => {
        setActive(false);
    };
    window.addEventListener("blur", w.handlers.windowBlur);

    w.handlers.visibilityChange = () => {
        setActive(!document.hidden);
    };
    document.addEventListener("visibilitychange", w.handlers.visibilityChange);

    w.handlers.mousedown = (event) => {
            if (event.button > 6) return;
            canvas.focus();
            requestFullscreenIfNeeded();
            let pos = getMousePos(event);
            let mod = getMfbKeyModFromEvent(event);
            Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
            Module._window_data_set_mouse_button(windowData, event.button + 1, 1);
            Module._window_data_set_mod_keys(windowData, mod);
            enqueueEvent({ type: "mousebutton", button: event.button + 1, mod: mod, isPressed: true});
    };
    canvas.addEventListener("mousedown", w.handlers.mousedown, false);

    w.handlers.mousemove = (event) => {
            let pos = getMousePos(event);
            Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
            enqueueEvent({ type: "mousemove", x: pos.x, y: pos.y});
    };
    canvas.addEventListener("mousemove", w.handlers.mousemove, false);

    w.handlers.mouseup = (event) => {
            if (event.button > 6) return;
            let pos = getMousePos(event);
            let mod = getMfbKeyModFromEvent(event);
            Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
            Module._window_data_set_mouse_button(windowData, event.button + 1, 0);
            Module._window_data_set_mod_keys(windowData, mod);
            enqueueEvent({ type: "mousebutton", button: event.button + 1, mod: mod, isPressed: false});
    };
    canvas.addEventListener("mouseup", w.handlers.mouseup, false);

    w.handlers.bodyMouseup = (event) => {
            if (event.button > 6) return;
            let pos = getMousePos(event);
            let mod = getMfbKeyModFromEvent(event);
            Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
            Module._window_data_set_mouse_button(windowData, event.button + 1, 0);
            Module._window_data_set_mod_keys(windowData, mod);
            enqueueEvent({ type: "mousebutton", button: event.button + 1, mod: mod, isPressed: false});
    };
    document.body.addEventListener("mouseup", w.handlers.bodyMouseup, false);

    w.handlers.wheel = (event) => {
            event.preventDefault();
            let mod = getMfbKeyModFromEvent(event);
            Module._window_data_set_mouse_wheel(windowData, event.deltaX, event.deltaY);
            Module._window_data_set_mod_keys(windowData, mod);
            enqueueEvent({ type: "mousescroll", mod: mod, x: event.deltaX, y: event.deltaY});
    };
    canvas.addEventListener('wheel', w.handlers.wheel, NON_PASSIVE);

    w.handlers.touchstart = (event) => {
            if (w.activeTouchId === null) {
                canvas.focus();
                requestFullscreenIfNeeded();
                let touch = event.changedTouches[0];
                let pos = getMousePos(touch);
                Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
                Module._window_data_set_mouse_button(windowData, 1, 1);
                w.activeTouchId = touch.identifier;
                enqueueEvent({ type: "mousebutton", button: 1, mod: 0, isPressed: true});
            }
            event.preventDefault();
    };
    canvas.addEventListener("touchstart", w.handlers.touchstart, NON_PASSIVE);

    w.handlers.touchmove = (event) => {
            if (w.activeTouchId != null) {
                for (let i = 0; i < event.changedTouches.length; i++) {
                    let touch = event.changedTouches[i];
                    if (w.activeTouchId === touch.identifier) {
                        let pos = getMousePos(touch);
                        Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
                        enqueueEvent({ type: "mousemove", x: pos.x, y: pos.y});
                        break;
                    }
                }
            }
            event.preventDefault();
    };
    canvas.addEventListener("touchmove", w.handlers.touchmove, NON_PASSIVE);

    w.handlers.touchEndOrCancel = (event) => {
        if (w.activeTouchId != null) {
            for (let i = 0; i < event.changedTouches.length; i++) {
                let touch = event.changedTouches[i];
                if (w.activeTouchId === touch.identifier) {
                    let pos = getMousePos(touch);
                    Module._window_data_set_mouse_pos(windowData, pos.x, pos.y);
                    Module._window_data_set_mouse_button(windowData, 1, 0);
                    w.activeTouchId = null;
                    enqueueEvent({ type: "mousebutton", button: 1, mod: 0, isPressed: false});
                    break;
                }
            }
        }
        event.preventDefault();
    };
    canvas.addEventListener("touchend", w.handlers.touchEndOrCancel, NON_PASSIVE);
    canvas.addEventListener("touchcancel", w.handlers.touchEndOrCancel, NON_PASSIVE);

    window._minifb.windows[id] = w;
    return id;
});

EM_JS(void, mfb_close_js, (uintptr_t windowId), {
    if (!window._minifb || !window._minifb.windows[windowId]) return;
    let w = window._minifb.windows[windowId];
    if (w.handlers) {
        w.canvas.removeEventListener("keydown", w.handlers.keydown);
        w.canvas.removeEventListener("keyup", w.handlers.keyup);
        w.canvas.removeEventListener("keypress", w.handlers.keypress);
        w.canvas.removeEventListener("focus", w.handlers.focus);
        w.canvas.removeEventListener("blur", w.handlers.blur);
        window.removeEventListener("focus", w.handlers.windowFocus);
        window.removeEventListener("blur", w.handlers.windowBlur);
        document.removeEventListener("visibilitychange", w.handlers.visibilityChange);
        w.canvas.removeEventListener("mousedown", w.handlers.mousedown, false);
        w.canvas.removeEventListener("mousemove", w.handlers.mousemove, false);
        w.canvas.removeEventListener("mouseup", w.handlers.mouseup, false);
        document.body.removeEventListener("mouseup", w.handlers.bodyMouseup, false);
        w.canvas.removeEventListener("wheel", w.handlers.wheel, false);
        w.canvas.removeEventListener("touchstart", w.handlers.touchstart, false);
        w.canvas.removeEventListener("touchmove", w.handlers.touchmove, false);
        w.canvas.removeEventListener("touchend", w.handlers.touchEndOrCancel, false);
        w.canvas.removeEventListener("touchcancel", w.handlers.touchEndOrCancel, false);
    }
    delete window._minifb.windows[windowId];
});

static void
mfb_destroy_window_data(SWindowData *window_data) {
    if (window_data == NULL) {
        return;
    }

    SWindowData_Web *window_data_web = (SWindowData_Web *) window_data->specific;
    if (window_data_web != NULL) {
        if (window_data_web->window_id != 0) {
            mfb_close_js(window_data_web->window_id);
        }

        free(window_data_web->swizzle_buffer);
        window_data_web->swizzle_buffer = NULL;
        window_data_web->swizzle_buffer_size = 0;

        free(window_data_web);
        window_data->specific = NULL;
    }

    free(window_data);
}

struct mfb_window *mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    SWindowData *window_data;

    window_data = malloc(sizeof(SWindowData));
    if (window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: failed to allocate SWindowData.");
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    int wants_fullscreen = ((flags & WF_FULLSCREEN) || (flags & WF_FULLSCREEN_DESKTOP)) ? 1 : 0;
    void *specific = mfb_open_ex_js(window_data, title, width, height, wants_fullscreen);
    if (!specific) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: failed to initialize JavaScript window data for title '%s'.", title ? title : "(null)");
        free(window_data);
        return NULL;
    }

    SWindowData_Web *window_data_web = malloc(sizeof(SWindowData_Web));
    if (window_data_web == NULL) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: failed to allocate SWindowData_Web.");
        mfb_close_js((uintptr_t) specific);
        free(window_data);
        return NULL;
    }
    memset(window_data_web, 0, sizeof(SWindowData_Web));
    window_data_web->window_id = (uintptr_t) specific;
    window_data->specific = window_data_web;

    // setup key map if not initialized yet
    if (!g_initialized) {
        setup_web_mfb();
        g_initialized = true;
    }

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

    window_data->window_width = width;
    window_data->window_height = height;
    window_data->buffer_width = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * 4;
    window_data->dst_offset_x = 0;
    window_data->dst_offset_y = 0;
    window_data->dst_width = width;
    window_data->dst_height = height;
    calc_dst_factor(window_data, width, height);

    window_data->is_active = true;
    window_data->is_initialized = true;
    window_data->is_cursor_visible = true;

    mfb_log(MFB_LOG_DEBUG, "WebMiniFB: window created using Web API (title='%s', size=%ux%u, flags=0x%x).",
            title ? title : "(null)", width, height, flags);
    return (struct mfb_window*)window_data;
}

bool mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: mfb_set_viewport called with a null window pointer.");
        return false;
    }

    if (offset_x > window_data->window_width || width > window_data->window_width - offset_x) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: viewport exceeds window width (offset_x=%u, width=%u, window_width=%u).",
                offset_x, width, window_data->window_width);
        return false;
    }
    if (offset_y > window_data->window_height || height > window_data->window_height - offset_y) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: viewport exceeds window height (offset_y=%u, height=%u, window_height=%u).",
                offset_y, height, window_data->window_height);
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width = width;
    window_data->dst_height = height;
    calc_dst_factor(window_data, window_data->window_width, window_data->window_height);

    return true;
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
            Module._window_data_call_active_func(windowData, event.isActive ? 1 : 0);
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
        } else if (event.type == "char") {
            Module._window_data_call_char_input_func(windowData, event.code);
        }
    }
    return Module._window_data_get_close(windowData) ? STATE_EXIT : STATE_OK;
});

mfb_update_state mfb_update_events(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_events called with an invalid window.");
        return STATE_INVALID_WINDOW;
    }
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_events aborted because the window is marked for close.");
        mfb_destroy_window_data(window_data);
        return STATE_EXIT;
    }

    mfb_update_state state = mfb_update_events_js(window_data);
    if (state == STATE_EXIT) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_events detected close request after event processing.");
        mfb_destroy_window_data(window_data);
        return STATE_EXIT;
    }
    if (state == STATE_INVALID_WINDOW) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: mfb_update_events_js returned invalid window.");
    }
    else if (state == STATE_INTERNAL_ERROR) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: mfb_update_events_js returned internal error.");
    }

    return state;
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
    }
    else {
        if (canvas.width != width) canvas.width = width;
        if (canvas.height != height) canvas.height = height;
    }
    Module._window_data_update_window_size(windowData, canvas.width, canvas.height);
    Module._window_data_set_buffer_size(windowData, width, height);

    let swizzleBuffer = Module._window_data_get_swizzle_buffer(windowData, width, height);
    if (swizzleBuffer == 0) return STATE_INTERNAL_ERROR;

    Module._reverse_color_channels(buffer, swizzleBuffer, width, height);
    let framePixels = new Uint8ClampedArray(HEAPU8.buffer, swizzleBuffer, width * height * 4);
    let imageData = new ImageData(framePixels, width, height);
    let dstOffsetX = Module._window_data_get_dst_offset_x(windowData);
    let dstOffsetY = Module._window_data_get_dst_offset_y(windowData);
    let dstWidth = Module._window_data_get_dst_width(windowData);
    let dstHeight = Module._window_data_get_dst_height(windowData);
    let ctx = w.ctx;
    if (!ctx) return STATE_INTERNAL_ERROR;
    ctx.imageSmoothingEnabled = false;

    // Clear when viewport does not cover the full canvas to avoid stale pixels.
    if (dstOffsetX !== 0 || dstOffsetY !== 0 || dstWidth !== canvas.width || dstHeight !== canvas.height) {
        ctx.clearRect(0, 0, canvas.width, canvas.height);
    }

    if (dstWidth === width && dstHeight === height) {
        ctx.putImageData(imageData, dstOffsetX, dstOffsetY);
    } else {
        if (!w.backCanvas || w.backCanvas.width !== width || w.backCanvas.height !== height) {
            w.backCanvas = document.createElement("canvas");
            w.backCanvas.width = width;
            w.backCanvas.height = height;
            w.backCtx = w.backCanvas.getContext("2d");
        }
        if (!w.backCtx) return STATE_INTERNAL_ERROR;
        w.backCtx.putImageData(imageData, 0, 0);
        ctx.drawImage(w.backCanvas, 0, 0, width, height, dstOffsetX, dstOffsetY, dstWidth, dstHeight);
    }
    return Module._window_data_get_close(windowData) ? STATE_EXIT : STATE_OK;
});

mfb_update_state mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex called with an invalid window.");
        return STATE_INVALID_WINDOW;
    }
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex aborted because the window is marked for close.");
        mfb_destroy_window_data(window_data);
        return STATE_EXIT;
    }

    mfb_update_state state = mfb_update_js(window, buffer, width, height);
    if (state == STATE_EXIT) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex detected close request after frame update.");
        mfb_destroy_window_data(window_data);
        return STATE_EXIT;
    }
    if (state != STATE_OK) {
        if (state == STATE_INVALID_BUFFER) {
            mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex called with an invalid buffer.");
        }
        else if (state == STATE_INVALID_WINDOW) {
            mfb_log(MFB_LOG_ERROR, "WebMiniFB: mfb_update_js reported invalid window.");
        }
        else if (state == STATE_INTERNAL_ERROR) {
            mfb_log(MFB_LOG_ERROR, "WebMiniFB: mfb_update_js reported internal error.");
        }
        return state;
    }

    state = mfb_update_events_js(window_data);
    if (state == STATE_EXIT) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex detected close request after event processing.");
        mfb_destroy_window_data(window_data);
        return STATE_EXIT;
    }
    if (state == STATE_INVALID_WINDOW) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: mfb_update_events_js returned invalid window after frame update.");
    }
    else if (state == STATE_INTERNAL_ERROR) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: mfb_update_events_js returned internal error after frame update.");
    }

    return state;
}

bool mfb_wait_sync(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_wait_sync called with an invalid window.");
        return false;
    }
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_wait_sync aborted because the window is marked for close.");
        mfb_destroy_window_data(window_data);
        return false;
    }

    emscripten_sleep(0);
    if (window_data->close) {
        mfb_log(MFB_LOG_DEBUG, "WebMiniFB: mfb_wait_sync detected close request while waiting for sync.");
        mfb_destroy_window_data(window_data);
        return false;
    }

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

void
mfb_show_cursor(struct mfb_window *window, bool show) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        mfb_log(MFB_LOG_ERROR, "WebMiniFB: mfb_show_cursor called with a null window pointer.");
        return;
    }

    window_data->is_cursor_visible = show;
    if (!show && !g_cursor_warning_logged) {
        mfb_log(MFB_LOG_WARNING, "WebMiniFB: cursor hiding is not implemented in the web backend yet.");
        g_cursor_warning_logged = true;
    }
}
