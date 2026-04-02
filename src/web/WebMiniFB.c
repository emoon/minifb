#include <MiniFB.h>
#include <MiniFB_internal.h>
#include <emscripten.h>
#include <WindowData.h>
#include "WindowData_Web.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//-------------------------------------
#define EM_EXPORT __attribute__((used))

//-------------------------------------
static bool g_initialized = false;

//-------------------------------------
static SWindowData_Web *
mfb_web_get_data(SWindowData *window_data) {
    if (window_data == NULL) {
        return NULL;
    }

    return (SWindowData_Web *) window_data->specific;
}

//-------------------------------------
static uint8_t *
mfb_web_ensure_swizzle_buffer(SWindowData *window_data, unsigned width, unsigned height) {
    SWindowData_Web *window_data_specific = mfb_web_get_data(window_data);
    if (window_data_specific == NULL || width == 0 || height == 0) {
        return NULL;
    }

    if ((size_t) width > SIZE_MAX / (size_t) height) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: swizzle buffer size overflow for %ux%u.", width, height);
        return NULL;
    }

    size_t pixels = (size_t) width * (size_t) height;
    if (pixels > SIZE_MAX / 4) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: swizzle buffer byte size overflow for %ux%u.", width, height);
        return NULL;
    }

    size_t required = pixels * 4;
    if (required > window_data_specific->swizzle_buffer_size) {
        uint8_t *swizzle_buffer = realloc(window_data_specific->swizzle_buffer, required);
        if (swizzle_buffer == NULL) {
            MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: failed to allocate %zu bytes for swizzle buffer.", required);
            return NULL;
        }
        window_data_specific->swizzle_buffer = swizzle_buffer;
        window_data_specific->swizzle_buffer_size = required;
    }

    return window_data_specific->swizzle_buffer;
}

//-------------------------------------
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
            "NumpadDecimal": 330,
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

//-------------------------------------
EM_EXPORT void
reverse_color_channels(uint8_t *src, uint8_t *dst, int width, int height) {
    size_t num_bytes = (size_t) width * (size_t) height * 4;
    for (size_t i = 0; i < num_bytes; i += 4) {
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

//-------------------------------------
EM_EXPORT void
window_data_set_mouse_pos(SWindowData *window_data, int x, int y) {
    if (!window_data) return;
    window_data->mouse_pos_x = x;
    window_data->mouse_pos_y = y;
}

//-------------------------------------
EM_EXPORT void
window_data_set_mouse_wheel(SWindowData *window_data, float x, float y) {
    if (!window_data) return;
    window_data->mouse_wheel_x = x;
    window_data->mouse_wheel_y = y;
}

//-------------------------------------
EM_EXPORT void
window_data_set_mouse_button(SWindowData *window_data, uint8_t button, bool is_pressed) {
    if (!window_data) return;
    if (button > 7) return;
    window_data->mouse_button_status[button] = is_pressed;
}

//-------------------------------------
EM_EXPORT void
window_data_set_key(SWindowData *window_data, unsigned key, bool is_pressed) {
    if (!window_data) return;
    if (key >= 512) return;
    window_data->key_status[key] = is_pressed;
}

//-------------------------------------
EM_EXPORT void
window_data_set_mod_keys(SWindowData *window_data, uint32_t mod) {
    if (!window_data) return;
    window_data->mod_keys = mod;
}

//-------------------------------------
EM_EXPORT void
window_data_set_active(SWindowData *window_data, bool is_active) {
    if (!window_data) return;
    window_data->is_active = is_active;
}

//-------------------------------------
EM_EXPORT void
window_data_set_buffer_size(SWindowData *window_data, unsigned width, unsigned height) {
    uint32_t buffer_stride = 0;

    if (!window_data) return;
    if (!calculate_buffer_layout(width, height, &buffer_stride, NULL)) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: invalid buffer size %ux%u.", width, height);
        window_data->buffer_width = 0;
        window_data->buffer_height = 0;
        window_data->buffer_stride = 0;
        return;
    }
    window_data->buffer_width = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = buffer_stride;
}

//-------------------------------------
EM_EXPORT void
window_data_update_window_size(SWindowData *window_data, unsigned width, unsigned height) {
    if (!window_data || width == 0 || height == 0) return;

    bool changed = (window_data->window_width != width) || (window_data->window_height != height);
    window_data->window_width = width;
    window_data->window_height = height;

    if (window_data->dst_width == 0 || window_data->dst_height == 0) {
        window_data->dst_offset_x = 0;
        window_data->dst_offset_y = 0;
        window_data->dst_width = width;
        window_data->dst_height = height;
        calc_dst_factor(window_data, width, height);
    }
    else if (changed) {
        resize_dst(window_data, width, height);
    }

    if (changed && window_data->resize_func) {
        window_data->resize_func((struct mfb_window *) window_data, (int) width, (int) height);
    }
}

//-------------------------------------
EM_EXPORT void *
window_data_get_specific(SWindowData *window_data) {
    SWindowData_Web *window_data_specific = mfb_web_get_data(window_data);
    if (window_data_specific == NULL) return 0;
    return (void *) window_data_specific->window_id;
}

//-------------------------------------
EM_EXPORT uint8_t *
window_data_get_swizzle_buffer(SWindowData *window_data, unsigned width, unsigned height) {
    return mfb_web_ensure_swizzle_buffer(window_data, width, height);
}

//-------------------------------------
EM_EXPORT unsigned
window_data_get_dst_offset_x(SWindowData *window_data) {
    if (!window_data) return 0;
    return window_data->dst_offset_x;
}

//-------------------------------------
EM_EXPORT unsigned
window_data_get_dst_offset_y(SWindowData *window_data) {
    if (!window_data) return 0;
    return window_data->dst_offset_y;
}

//-------------------------------------
EM_EXPORT unsigned
window_data_get_dst_width(SWindowData *window_data) {
    if (!window_data) return 0;
    return window_data->dst_width;
}

//-------------------------------------
EM_EXPORT unsigned
window_data_get_dst_height(SWindowData *window_data) {
    if (!window_data) return 0;
    return window_data->dst_height;
}

//-------------------------------------
EM_EXPORT void
window_data_call_active_func(SWindowData *window_data, bool is_active) {
    if (window_data == NULL) return;
    if (window_data->active_func) window_data->active_func((struct mfb_window *) window_data, is_active);
}

//-------------------------------------
EM_EXPORT void
window_data_call_resize_func(SWindowData *window_data, int width, int height) {
    if (window_data == NULL) return;
    if (window_data->resize_func) window_data->resize_func((struct mfb_window *) window_data, width, height);
}

//-------------------------------------
EM_EXPORT void
window_data_call_close_func(SWindowData *window_data) {
    if (window_data == NULL) return;
    if (window_data->close_func) window_data->close_func((struct mfb_window *) window_data);
}

//-------------------------------------
EM_EXPORT void
window_data_call_keyboard_func(SWindowData *window_data, mfb_key key, mfb_key_mod mod, bool is_pressed) {
    if (window_data == NULL) return;
    if (window_data->keyboard_func) window_data->keyboard_func((struct mfb_window *) window_data, key, mod, is_pressed);
}

//-------------------------------------
EM_EXPORT void
window_data_call_char_input_func(SWindowData *window_data, unsigned int code) {
    if (window_data == NULL) return;
    if (window_data->char_input_func) window_data->char_input_func((struct mfb_window *) window_data, code);
}

//-------------------------------------
EM_EXPORT void
window_data_call_mouse_btn_func(SWindowData *window_data, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed) {
    if (window_data == NULL) return;
    if (window_data->mouse_btn_func) window_data->mouse_btn_func((struct mfb_window *) window_data, button, mod, is_pressed);
}

//-------------------------------------
EM_EXPORT void
window_data_call_mouse_move_func(SWindowData *window_data, int x, int y) {
    if (window_data == NULL) return;
    if (window_data->mouse_move_func) window_data->mouse_move_func((struct mfb_window *) window_data, x, y);
}

//-------------------------------------
EM_EXPORT void
window_data_call_mouse_wheel_func(SWindowData *window_data, mfb_key_mod mod, float x, float y) {
    if (window_data == NULL) return;
    if (window_data->mouse_wheel_func) window_data->mouse_wheel_func((struct mfb_window *) window_data, mod, x, y);
}

//-------------------------------------
EM_EXPORT bool
window_data_get_close(SWindowData *window_data) {
    if (!window_data) return true;
    return window_data->close;
}

//-------------------------------------
EM_JS(int, mfb_canvas_exists_js, (const char *title), {
    let canvas = document.getElementById(UTF8ToString(title));
    return canvas ? 1 : 0;
});

//-------------------------------------
EM_JS(void *, mfb_open_ex_js,(SWindowData *window_data, const char *title, unsigned width, unsigned height, int wants_full_screen), {
    let canvasId = UTF8ToString(title);
    let canvas = document.getElementById(canvasId);
    if (!canvas) {
        canvas = document.createElement("canvas");
        canvas.id = canvasId;
        if (document.body) {
            document.body.appendChild(canvas);
        }
        else if (document.documentElement) {
            document.documentElement.appendChild(canvas);
        }
        else {
            return 0;
        }
    }

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
        window_data: window_data,
        wants_full_screen: wants_full_screen !== 0,
        activeTouchId: null,
        globalMouseupTarget: null,
        is_active: true,
        handlers: {},
        events: [
            { type: "active", is_active: true }
        ]
    };

    Module._window_data_update_window_size(window_data, canvas.width, canvas.height);

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
        if (!w.wants_full_screen) return;
        w.wants_full_screen = false;
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

    function setActive(is_active) {
        if (w.is_active === is_active) return;
        w.is_active = is_active;
        Module._window_data_set_active(window_data, is_active ? 1 : 0);
        enqueueEvent({ type: "active", is_active: is_active });
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

    // JS: 0=left, 1=middle, 2=right
    // MFB: 1=left(BTN_1), 2=right(BTN_2), 3=middle(BTN_3)
    function mapMouseButton(jsButton) {
        const map = [1, 3, 2];
        return jsButton < map.length ? map[jsButton] : jsButton + 1;
    }

    function getMfbKeyModFromEvent(event) {
        const MFB_KB_MOD_SHIFT     = 0x0001;
        const MFB_KB_MOD_CONTROL   = 0x0002;
        const MFB_KB_MOD_ALT       = 0x0004;
        const MFB_KB_MOD_SUPER     = 0x0008;
        const MFB_KB_MOD_CAPS_LOCK = 0x0010;
        const MFB_KB_MOD_NUM_LOCK  = 0x0020;

        let mod = 0;
        if (event.shiftKey) mod = mod | MFB_KB_MOD_SHIFT;
        if (event.ctrlKey) mod = mod | MFB_KB_MOD_CONTROL;
        if (event.altKey) mod = mod | MFB_KB_MOD_ALT;
        if (event.metaKey) mod = mod | MFB_KB_MOD_SUPER;
        if (event.getModifierState && event.getModifierState("CapsLock")) mod = mod | MFB_KB_MOD_CAPS_LOCK;
        if (event.getModifierState && event.getModifierState("NumLock"))  mod = mod | MFB_KB_MOD_NUM_LOCK;
        return mod;
    };

    w.handlers.keydown = (event) => {
        if (shouldPreventDefaultKey(event.code)) {
            event.preventDefault();
        }
        let code = toMfbCode(event.code);
        Module._window_data_set_key(window_data, code, 1);
        let mod = getMfbKeyModFromEvent(event);
        Module._window_data_set_mod_keys(window_data, mod);
        enqueueEvent({ type: "keydown", code: code, mod: mod });
    };
    canvas.addEventListener("keydown", w.handlers.keydown);

    w.handlers.keyup = (event) => {
        if (shouldPreventDefaultKey(event.code)) {
            event.preventDefault();
        }
        let code = toMfbCode(event.code);
        Module._window_data_set_key(window_data, code, 0);
        let mod = getMfbKeyModFromEvent(event);
        Module._window_data_set_mod_keys(window_data, mod);
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

    w.handlers.contextmenu = (event) => { event.preventDefault(); };
    canvas.addEventListener("contextmenu", w.handlers.contextmenu);

    w.handlers.mousedown = (event) => {
            if (event.button > 6) return;
            event.preventDefault();
            canvas.focus();
            requestFullscreenIfNeeded();
            let pos = getMousePos(event);
            let mod = getMfbKeyModFromEvent(event);
            let btn = mapMouseButton(event.button);
            Module._window_data_set_mouse_pos(window_data, pos.x, pos.y);
            Module._window_data_set_mouse_button(window_data, btn, 1);
            Module._window_data_set_mod_keys(window_data, mod);
            enqueueEvent({ type: "mousebutton", button: btn, mod: mod, is_pressed: true});
    };
    canvas.addEventListener("mousedown", w.handlers.mousedown, false);

    w.handlers.mousemove = (event) => {
            let pos = getMousePos(event);
            Module._window_data_set_mouse_pos(window_data, pos.x, pos.y);
            enqueueEvent({ type: "mousemove", x: pos.x, y: pos.y});
    };
    canvas.addEventListener("mousemove", w.handlers.mousemove, false);

    w.handlers.mouseup = (event) => {
            if (event.button > 6) return;
            let pos = getMousePos(event);
            let mod = getMfbKeyModFromEvent(event);
            let btn = mapMouseButton(event.button);
            Module._window_data_set_mouse_pos(window_data, pos.x, pos.y);
            Module._window_data_set_mouse_button(window_data, btn, 0);
            Module._window_data_set_mod_keys(window_data, mod);
            enqueueEvent({ type: "mousebutton", button: btn, mod: mod, is_pressed: false});
    };
    canvas.addEventListener("mouseup", w.handlers.mouseup, false);

    w.handlers.bodyMouseup = (event) => {
            if (event.button > 6) return;
            if (event.target === canvas) return;  // already handled by canvas mouseup
            let pos = getMousePos(event);
            let mod = getMfbKeyModFromEvent(event);
            let btn = mapMouseButton(event.button);
            Module._window_data_set_mouse_pos(window_data, pos.x, pos.y);
            Module._window_data_set_mouse_button(window_data, btn, 0);
            Module._window_data_set_mod_keys(window_data, mod);
            enqueueEvent({ type: "mousebutton", button: btn, mod: mod, is_pressed: false});
    };
    w.globalMouseupTarget = document.body || document.documentElement || document;
    if (w.globalMouseupTarget) {
        w.globalMouseupTarget.addEventListener("mouseup", w.handlers.bodyMouseup, false);
    }

    w.handlers.wheel = (event) => {
            event.preventDefault();
            let mod = getMfbKeyModFromEvent(event);
            let dx = event.deltaX;
            let dy = event.deltaY;
            if (event.deltaMode === 0) {
                // DOM_DELTA_PIXEL (Chrome default): normalize to ~1.0 per notch
                dx /= 100.0;
                dy /= 100.0;
            }
            else if (event.deltaMode === 2) {
                // DOM_DELTA_PAGE: convert pages to notches
                dx *= 3.0;
                dy *= 3.0;
            }
            // DOM_DELTA_LINE (mode 1, Firefox default): already ~1-3 per notch, pass through
            Module._window_data_set_mouse_wheel(window_data, dx, dy);
            Module._window_data_set_mod_keys(window_data, mod);
            enqueueEvent({ type: "mousescroll", mod: mod, x: dx, y: dy});
    };
    canvas.addEventListener('wheel', w.handlers.wheel, NON_PASSIVE);

    w.handlers.touchstart = (event) => {
            if (w.activeTouchId === null) {
                canvas.focus();
                requestFullscreenIfNeeded();
                let touch = event.changedTouches[0];
                let pos = getMousePos(touch);
                Module._window_data_set_mouse_pos(window_data, pos.x, pos.y);
                Module._window_data_set_mouse_button(window_data, 1, 1);
                w.activeTouchId = touch.identifier;
                enqueueEvent({ type: "mousebutton", button: 1, mod: 0, is_pressed: true});
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
                        Module._window_data_set_mouse_pos(window_data, pos.x, pos.y);
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
                    Module._window_data_set_mouse_pos(window_data, pos.x, pos.y);
                    Module._window_data_set_mouse_button(window_data, 1, 0);
                    w.activeTouchId = null;
                    enqueueEvent({ type: "mousebutton", button: 1, mod: 0, is_pressed: false});
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

EM_JS(void, mfb_close_js, (uintptr_t window_id), {
    if (!window._minifb || !window._minifb.windows[window_id]) return;
    let w = window._minifb.windows[window_id];
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
        if (w.globalMouseupTarget) {
            w.globalMouseupTarget.removeEventListener("mouseup", w.handlers.bodyMouseup, false);
        }
        w.canvas.removeEventListener("wheel", w.handlers.wheel, false);
        w.canvas.removeEventListener("touchstart", w.handlers.touchstart, false);
        w.canvas.removeEventListener("touchmove", w.handlers.touchmove, false);
        w.canvas.removeEventListener("touchend", w.handlers.touchEndOrCancel, false);
        w.canvas.removeEventListener("touchcancel", w.handlers.touchEndOrCancel, false);
    }
    delete window._minifb.windows[window_id];
});

//-------------------------------------
static void
destroy_window_data(SWindowData *window_data) {
    if (window_data == NULL) {
        return;
    }

    release_cpp_stub((struct mfb_window *) window_data);

    SWindowData_Web *window_data_specific = (SWindowData_Web *) window_data->specific;
    if (window_data_specific != NULL) {
        if (window_data_specific->window_id != 0) {
            mfb_close_js(window_data_specific->window_id);
        }

        free(window_data_specific->swizzle_buffer);
        window_data_specific->swizzle_buffer = NULL;
        window_data_specific->swizzle_buffer_size = 0;

        memset(window_data_specific, 0, sizeof(SWindowData_Web));
        free(window_data_specific);
        window_data->specific = NULL;
    }

    memset(window_data, 0, sizeof(SWindowData));
    free(window_data);
}

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
    const unsigned supported_flags = MFB_WF_FULLSCREEN | MFB_WF_FULLSCREEN_DESKTOP;
    unsigned effective_flags = flags;
    const char *window_title = (title != NULL && title[0] != '\0') ? title : "minifb";
    uint32_t buffer_stride = 0;
    SWindowData *window_data;

    if (!calculate_buffer_layout(width, height, &buffer_stride, NULL)) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: invalid window size %ux%u.", width, height);
        return NULL;
    }

    if ((effective_flags & ~supported_flags) != 0u) {
        MFB_LOG(MFB_LOG_WARNING, "WebMiniFB: window flags 0x%x are not supported by the web backend and will be ignored.", effective_flags & ~supported_flags);
    }

    if ((effective_flags & MFB_WF_FULLSCREEN) && (effective_flags & MFB_WF_FULLSCREEN_DESKTOP)) {
        MFB_LOG(MFB_LOG_WARNING, "WebMiniFB: MFB_WF_FULLSCREEN and MFB_WF_FULLSCREEN_DESKTOP were both requested; MFB_WF_FULLSCREEN takes precedence.");
        effective_flags &= ~MFB_WF_FULLSCREEN_DESKTOP;
    }

    window_data = malloc(sizeof(SWindowData));
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: failed to allocate SWindowData.");
        return NULL;
    }
    memset(window_data, 0, sizeof(SWindowData));

    int wants_fullscreen = ((effective_flags & MFB_WF_FULLSCREEN) || (effective_flags & MFB_WF_FULLSCREEN_DESKTOP)) ? 1 : 0;
    int canvas_existed = mfb_canvas_exists_js(window_title);
    void *specific = mfb_open_ex_js(window_data, window_title, width, height, wants_fullscreen);
    if (!specific) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: failed to initialize JavaScript window data for title '%s'.", window_title);
        free(window_data);
        return NULL;
    }
    if (!canvas_existed) {
        MFB_LOG(MFB_LOG_WARNING, "WebMiniFB: canvas with id '%s' was not found; created a new canvas and appended it to the document.", window_title);
    }

    SWindowData_Web *window_data_specific = malloc(sizeof(SWindowData_Web));
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: failed to allocate SWindowData_Web.");
        mfb_close_js((uintptr_t) specific);
        free(window_data);
        return NULL;
    }
    memset(window_data_specific, 0, sizeof(SWindowData_Web));
    window_data_specific->window_id = (uintptr_t) specific;
    window_data->specific = window_data_specific;

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
    window_data->buffer_stride = buffer_stride;
    window_data->dst_offset_x = 0;
    window_data->dst_offset_y = 0;
    window_data->dst_width = width;
    window_data->dst_height = height;
    calc_dst_factor(window_data, width, height);

    window_data->is_active = true;
    window_data->is_initialized = true;
    window_data->is_cursor_visible = true;

    MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: window created using Web API (title='%s', size=%ux%u, flags=0x%x).",
            window_title, width, height, flags);
    return (struct mfb_window *) window_data;
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    if (!mfb_validate_viewport(window_data, offset_x, offset_y, width, height, "WebMiniFB")) {
        return false;
    }

    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width = width;
    window_data->dst_height = height;
    calc_dst_factor(window_data, window_data->window_width, window_data->window_height);

    return true;
}

//-------------------------------------
void
mfb_set_title(struct mfb_window *window, const char *title) {
    (void) window;
    (void) title;
}

//-------------------------------------
EM_JS(mfb_update_state, mfb_update_events_js, (SWindowData * window_data), {
    // FIXME can we make these global somehow? --pre-js maybe?
    const MFB_STATE_OK = 0;
    const MFB_STATE_EXIT = -1;
    const MFB_STATE_INVALID_WINDOW = -2;
    const MFB_STATE_INVALID_BUFFER = -3;
    const MFB_STATE_INTERNAL_ERROR = -4;
    if (window_data == 0) return MFB_STATE_INVALID_WINDOW;
    let window_id = Module._window_data_get_specific(window_data);
    if (!window._minifb) return MFB_STATE_INTERNAL_ERROR;
    if (!window._minifb.windows[window_id]) return MFB_STATE_INVALID_WINDOW;
    let w = window._minifb.windows[window_id];
    let events = w.events;
    w.events = [];
    for (let i = 0; i < events.length; i++) {
        let event = events[i];
        if (event.type == "active") {
            Module._window_data_call_active_func(window_data, event.is_active ? 1 : 0);
        }
        else if (event.type == "mousebutton") {
            Module._window_data_call_mouse_btn_func(window_data, event.button, event.mod, event.is_pressed ? 1 : 0);
        }
        else if (event.type == "mousemove") {
            Module._window_data_call_mouse_move_func(window_data, event.x, event.y);
        }
        else if (event.type == "mousescroll") {
            Module._window_data_call_mouse_wheel_func(window_data, event.mod, event.x, event.y);
        }
        else if (event.type == "keydown") {
            Module._window_data_call_keyboard_func(window_data, event.code, event.mod, 1);
        }
        else if (event.type == "keyup") {
            Module._window_data_call_keyboard_func(window_data, event.code, event.mod, 0);
        }
        else if (event.type == "char") {
            Module._window_data_call_char_input_func(window_data, event.code);
        }
    }
    return Module._window_data_get_close(window_data) ? MFB_STATE_EXIT : MFB_STATE_OK;
});

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_events called with an invalid window.");
        return MFB_STATE_INVALID_WINDOW;
    }
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_events aborted because the window is marked for close.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    mfb_update_state state = mfb_update_events_js(window_data);
    if (state == MFB_STATE_EXIT) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_events detected close request after event processing.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }
    if (state == MFB_STATE_INVALID_WINDOW) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_update_events_js returned invalid window.");
    }
    else if (state == MFB_STATE_INTERNAL_ERROR) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_update_events_js returned internal error.");
    }

    return state;
}

//-------------------------------------
EM_JS(mfb_update_state, mfb_update_js, (struct mfb_window * window_data, void *buffer, int width, int height), {
    // FIXME can we make these global somehow? preamble.js maybe?
    const MFB_STATE_OK = 0;
    const MFB_STATE_EXIT = -1;
    const MFB_STATE_INVALID_WINDOW = -2;
    const MFB_STATE_INVALID_BUFFER = -3;
    const MFB_STATE_INTERNAL_ERROR = -4;
    if (window_data == 0) return MFB_STATE_INVALID_WINDOW;
    let window_id = Module._window_data_get_specific(window_data);
    if (!window._minifb) return MFB_STATE_INTERNAL_ERROR;
    if (!window._minifb.windows[window_id]) return MFB_STATE_INVALID_WINDOW;
    if (buffer == 0) return MFB_STATE_INVALID_BUFFER;
    let w = window._minifb.windows[window_id];
    let canvas = w.canvas;
    if (width <= 0) {
        width = canvas.width;
        height = canvas.height;
    }
    else {
        if (canvas.width != width) canvas.width = width;
        if (canvas.height != height) canvas.height = height;
    }
    Module._window_data_update_window_size(window_data, canvas.width, canvas.height);
    Module._window_data_set_buffer_size(window_data, width, height);

    let swizzleBuffer = Module._window_data_get_swizzle_buffer(window_data, width, height);
    if (swizzleBuffer == 0) return MFB_STATE_INTERNAL_ERROR;

    Module._reverse_color_channels(buffer, swizzleBuffer, width, height);
    let framePixels = new Uint8ClampedArray(HEAPU8.buffer, swizzleBuffer, width * height * 4);
    let imageData = new ImageData(framePixels, width, height);
    let dstOffsetX = Module._window_data_get_dst_offset_x(window_data);
    let dstOffsetY = Module._window_data_get_dst_offset_y(window_data);
    let dstWidth = Module._window_data_get_dst_width(window_data);
    let dstHeight = Module._window_data_get_dst_height(window_data);
    let ctx = w.ctx;
    if (!ctx) return MFB_STATE_INTERNAL_ERROR;
    ctx.imageSmoothingEnabled = false;

    // Clear when viewport does not cover the full canvas to avoid stale pixels.
    if (dstOffsetX !== 0 || dstOffsetY !== 0 || dstWidth !== canvas.width || dstHeight !== canvas.height) {
        ctx.clearRect(0, 0, canvas.width, canvas.height);
    }

    if (dstWidth === width && dstHeight === height) {
        ctx.putImageData(imageData, dstOffsetX, dstOffsetY);
    }
    else {
        if (!w.backCanvas || w.backCanvas.width !== width || w.backCanvas.height !== height) {
            w.backCanvas = document.createElement("canvas");
            w.backCanvas.width = width;
            w.backCanvas.height = height;
            w.backCtx = w.backCanvas.getContext("2d");
        }
        if (!w.backCtx) return MFB_STATE_INTERNAL_ERROR;
        w.backCtx.putImageData(imageData, 0, 0);
        ctx.drawImage(w.backCanvas, 0, 0, width, height, dstOffsetX, dstOffsetY, dstWidth, dstHeight);
    }
    return Module._window_data_get_close(window_data) ? MFB_STATE_EXIT : MFB_STATE_OK;
});

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex called with an invalid window.");
        return MFB_STATE_INVALID_WINDOW;
    }
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex aborted because the window is marked for close.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }

    if (buffer == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex called with an invalid buffer.");
        return MFB_STATE_INVALID_BUFFER;
    }

    uint32_t buffer_stride;
    if (!calculate_buffer_layout(width, height, &buffer_stride, NULL)) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex called with invalid buffer size %ux%u.", width, height);
        return MFB_STATE_INVALID_BUFFER;
    }

    window_data->draw_buffer   = buffer;
    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = buffer_stride;

    mfb_update_state state = mfb_update_js(window, buffer, width, height);
    if (state == MFB_STATE_EXIT) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex detected close request after frame update.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }
    if (state != MFB_STATE_OK) {
        if (state == MFB_STATE_INVALID_BUFFER) {
            MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex called with an invalid buffer.");
        }
        else if (state == MFB_STATE_INVALID_WINDOW) {
            MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_update_js reported invalid window.");
        }
        else if (state == MFB_STATE_INTERNAL_ERROR) {
            MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_update_js reported internal error.");
        }
        return state;
    }

    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    state = mfb_update_events_js(window_data);
    if (state == MFB_STATE_EXIT) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_update_ex detected close request after event processing.");
        destroy_window_data(window_data);
        return MFB_STATE_EXIT;
    }
    if (state == MFB_STATE_INVALID_WINDOW) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_update_events_js returned invalid window after frame update.");
    }
    else if (state == MFB_STATE_INTERNAL_ERROR) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_update_events_js returned internal error after frame update.");
    }

    return state;
}

//-------------------------------------
bool
mfb_wait_sync(struct mfb_window *window) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_wait_sync called with an invalid window.");
        return false;
    }
    if (window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_wait_sync aborted because the window is marked for close.");
        destroy_window_data(window_data);
        return false;
    }

    emscripten_sleep(0);

    window_data->mouse_wheel_x = 0.0f;
    window_data->mouse_wheel_y = 0.0f;

    mfb_update_state state = mfb_update_events_js(window_data);
    if (state == MFB_STATE_EXIT || window_data->close) {
        MFB_LOG(MFB_LOG_DEBUG, "WebMiniFB: mfb_wait_sync detected close request while waiting for sync/events.");
        destroy_window_data(window_data);
        return false;
    }
    if (state == MFB_STATE_INVALID_WINDOW) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_wait_sync update-events returned invalid window.");
        return false;
    }
    if (state == MFB_STATE_INTERNAL_ERROR) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_wait_sync update-events returned internal error.");
        return false;
    }

    return true;
}

//-------------------------------------
EM_JS(double, mfb_device_pixel_ratio_js, (), {
    return (typeof window !== "undefined" && window.devicePixelRatio > 0)
        ? window.devicePixelRatio
        : 1.0;
})

//-------------------------------------
void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
    kUnused(window);
    float scale = (float) mfb_device_pixel_ratio_js();

    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    if (scale_x) {
        *scale_x = scale;
    }

    if (scale_y) {
        *scale_y = scale;
    }
}

//-------------------------------------
extern double g_timer_frequency;
extern double g_timer_resolution;

//-------------------------------------
void
mfb_timer_init(void) {
    g_timer_frequency  = 1e+9;
    g_timer_resolution = 1.0 / g_timer_frequency;
}

//-------------------------------------
EM_JS(double, mfb_timer_tick_js, (), {
   return performance.now();
});

//-------------------------------------
uint64_t
mfb_timer_tick(void) {
    uint64_t now = (uint64_t)(mfb_timer_tick_js() * 1e+6);
    return now;
}

//-------------------------------------
EM_JS(void, mfb_show_cursor_js, (uintptr_t window_id, int show), {
    if (!window._minifb || !window._minifb.windows[window_id]) return;
    window._minifb.windows[window_id].canvas.style.cursor = show ? "" : "none";
})

//-------------------------------------
void
mfb_show_cursor(struct mfb_window *window, bool show) {
    SWindowData *window_data = (SWindowData *) window;
    if (window_data == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_show_cursor called with a null window pointer.");
        return;
    }

    window_data->is_cursor_visible = show;

    SWindowData_Web *window_data_specific = mfb_web_get_data(window_data);
    if (window_data_specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "WebMiniFB: mfb_show_cursor missing web-specific window data.");
        return;
    }

    mfb_show_cursor_js(window_data_specific->window_id, show ? 1 : 0);
}
