#include <MiniFB.h>
#include "MiniFB_internal.h"
#include "MiniFB_enums.h"
#include "WaylandWindowData.h"

#include <wayland-client.h>
#include <wayland-cursor.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/input-event-codes.h>

#include <sys/mman.h>

SWindowData g_window_data = { 0 };

static void 
destroy(void)
{
    if (! g_window_data.display)
        return;

#define KILL(NAME)                                   \
    do                                               \
    {                                                \
        if (g_window_data.NAME)                      \
            wl_##NAME##_destroy(g_window_data.NAME); \
    } while (0);                                     \
    g_window_data.NAME = 0x0;

    KILL(shell_surface);
    KILL(shell);
    KILL(surface);
    //KILL(buffer);
    if(g_window_data.draw_buffer) {
        wl_buffer_destroy(g_window_data.draw_buffer);
        g_window_data.draw_buffer = 0x0;
    }
    KILL(shm_pool);
    KILL(shm);
    KILL(compositor);
    KILL(keyboard);
    KILL(seat);
    KILL(registry);
#undef KILL
    wl_display_disconnect(g_window_data.display);
    memset(&g_window_data, 0, sizeof(SWindowData));
}

// This event provides a file descriptor to the client which can be memory-mapped 
// to provide a keyboard mapping description.
// format: keymap format
// fd:     keymap file descriptor
// size:   keymap size, in bytes
static void
keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
    kUnused(data);
    kUnused(keyboard);
    kUnused(format);
    kUnused(fd);
    kUnused(size);
}

// Notification that this seat's keyboard focus is on a certain surface.
// serial:  serial number of the enter event
// surface: surface gaining keyboard focus
// keys:    the currently pressed keys
static void 
keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
    kUnused(data);
    kUnused(keyboard);
    kUnused(serial);
    kUnused(surface);
    kUnused(keys);
    kCall(g_active_func, true);
}

// The leave notification is sent before the enter notification for the new focus.
// serial:  serial number of the leave event
// surface: surface that lost keyboard focus
static void 
keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
    kUnused(data);
    kUnused(keyboard);
    kUnused(serial);
    kUnused(surface);
    kCall(g_active_func, false);
}

// A key was pressed or released. The time argument is a timestamp with 
// millisecond granularity, with an undefined base.
// serial: serial number of the key event
// time:   timestamp with millisecond granularity
// key:    key that produced the event
// state:  physical state of the key
static void 
keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    kUnused(data);
    kUnused(keyboard);
    kUnused(serial);
    kUnused(time);
    if(key < 512) {
        Key    kb_key     = (Key) keycodes[key];
        bool   is_pressed = (bool) (state == WL_KEYBOARD_KEY_STATE_PRESSED);
        switch (kb_key)
        {
            case KB_KEY_LEFT_SHIFT:
            case KB_KEY_RIGHT_SHIFT:
                if(is_pressed)
                    g_window_data.mod_keys |= KB_MOD_SHIFT;
                else
                    g_window_data.mod_keys &= ~KB_MOD_SHIFT;
                break;

            case KB_KEY_LEFT_CONTROL:
            case KB_KEY_RIGHT_CONTROL:
                if(is_pressed)
                    g_window_data.mod_keys |= KB_MOD_CONTROL;
                else
                    g_window_data.mod_keys &= ~KB_MOD_CONTROL;
                break;

            case KB_KEY_LEFT_ALT:
            case KB_KEY_RIGHT_ALT:
                if(is_pressed)
                    g_window_data.mod_keys |= KB_MOD_ALT;
                else
                    g_window_data.mod_keys &= ~KB_MOD_ALT;
                break;

            case KB_KEY_LEFT_SUPER:
            case KB_KEY_RIGHT_SUPER:
                if(is_pressed)
                    g_window_data.mod_keys |= KB_MOD_SUPER;
                else
                    g_window_data.mod_keys &= ~KB_MOD_SUPER;
                break;
        }

        kCall(g_keyboard_func, kb_key, (KeyMod)g_window_data.mod_keys, is_pressed);
    }
}

// Notifies clients that the modifier and/or group state has changed, 
// and it should update its local state.
// serial:         serial number of the modifiers event
// mods_depressed: depressed modifiers
// mods_latched:   latched modifiers
// mods_locked:    locked modifiers
// group:          keyboard layout
static void 
keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    kUnused(data);
    kUnused(keyboard);
    kUnused(serial);
    kUnused(mods_depressed);
    kUnused(mods_latched);
    kUnused(mods_locked);
    kUnused(group);
    // it is not easy to identify them here :(
}

// Informs the client about the keyboard's repeat rate and delay.
// rate:  the rate of repeating keys in characters per second
// delay: delay in milliseconds since key down until repeating starts
static void 
keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay)
{
    kUnused(data);
    kUnused(keyboard);
    kUnused(rate);
    kUnused(delay);
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
    .repeat_info = NULL,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Notification that this seat's pointer is focused on a certain surface.
//
// When a seat's focus enters a surface, the pointer image is
// undefined and a client should respond to this event by setting
// an appropriate pointer image with the set_cursor request.
//
// serial:  serial number of the enter event
// surface: surface entered by the pointer
// sx:      surface-local x coordinate
// sy:      surface-local y coordinate
static void
pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
{
    kUnused(data);
    //kUnused(pointer);
    //kUnused(serial);
    kUnused(surface);
    kUnused(sx);
    kUnused(sy);
    struct wl_buffer *buffer;
    struct wl_cursor_image *image;

    image  = g_window_data.default_cursor->images[0];
    buffer = wl_cursor_image_get_buffer(image);

    wl_pointer_set_cursor(pointer, serial, g_window_data.cursor_surface, image->hotspot_x, image->hotspot_y);
    wl_surface_attach(g_window_data.cursor_surface, buffer, 0, 0);
    wl_surface_damage(g_window_data.cursor_surface, 0, 0, image->width, image->height);
    wl_surface_commit(g_window_data.cursor_surface);
    //fprintf(stderr, "Pointer entered surface %p at %d %d\n", surface, sx, sy);
}

// Notification that this seat's pointer is no longer focused on a certain surface.
//
// The leave notification is sent before the enter notification for the new focus.
//
// serial:  serial number of the leave event
// surface: surface left by the pointer
static void
pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
    kUnused(data);
    kUnused(pointer);
    kUnused(serial);
    kUnused(surface);
    //fprintf(stderr, "Pointer left surface %p\n", surface);
}

// Notification of pointer location change. 
//
// The arguments sx and sy are the location relative to the focused surface.
//
// time:  timestamp with millisecond granularity
// sx:    surface-local x coordinate
// sy:    surface-local y coordinate
static void
pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    kUnused(data);
    kUnused(pointer);
    kUnused(time);
    //printf("Pointer moved at %f %f\n", sx / 256.0f, sy / 256.0f);
    kCall(g_mouse_move_func, sx >> 24, sy >> 24);
}

// Mouse button click and release notifications.
//
// The location of the click is given by the last motion or enter
// event. The time argument is a timestamp with millisecond
// granularity, with an undefined base.
//
// The button is a button code as defined in the Linux kernel's
// linux/input-event-codes.h header file, e.g. BTN_LEFT.
//
// Any 16-bit button code value is reserved for future additions to
// the kernel's event code list. All other button codes above
// 0xFFFF are currently undefined but may be used in future
// versions of this protocol.
//
// serial: serial number of the button event
// time:   timestamp with millisecond granularity
// button: button that produced the event
// state:  physical state of the button
static void
pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    kUnused(data);
    kUnused(pointer);
    kUnused(serial);
    kUnused(time);
    //printf("Pointer button '%d'(%d)\n", button, state);
    kCall(g_mouse_btn_func, button - BTN_MOUSE + 1, g_window_data.mod_keys, state == 1);
}

//  Scroll and other axis notifications.
// 
//  For scroll events (vertical and horizontal scroll axes), the
//  value parameter is the length of a vector along the specified
//  axis in a coordinate space identical to those of motion events,
//  representing a relative movement along the specified axis.
// 
//  For devices that support movements non-parallel to axes multiple
//  axis events will be emitted.
// 
//  When applicable, for example for touch pads, the server can
//  choose to emit scroll events where the motion vector is
//  equivalent to a motion event vector.
// 
//  When applicable, a client can transform its content relative to
//  the scroll distance.
//
//  time:  timestamp with millisecond granularity
//  axis:  axis type
//  value: length of vector in surface-local coordinate space
static void
pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
    kUnused(data);
    kUnused(pointer);
    kUnused(time);
    kUnused(axis);
    //printf("Pointer handle axis: axis: %d (0x%x)\n", axis, value);
    if(axis == 0) {
        kCall(g_mouse_wheel_func, g_window_data.mod_keys, 0.0f, -(value / 256.0f));
    }
    else if(axis == 1) {
        kCall(g_mouse_wheel_func, g_window_data.mod_keys, -(value / 256.0f), 0.0f);
    }
}

static void 
frame(void *data, struct wl_pointer *pointer) {
    kUnused(data);
    kUnused(pointer);
}

static void 
axis_source(void *data, struct wl_pointer *pointer, uint32_t axis_source) {
    kUnused(data);
    kUnused(pointer);
    kUnused(axis_source);
}

static void 
axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis) {
    kUnused(data);
    kUnused(pointer);
    kUnused(time);
    kUnused(axis);
}

static void 
axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete) {
    kUnused(data);
    kUnused(pointer);
    kUnused(axis);
    kUnused(discrete);
}

static const struct wl_pointer_listener pointer_listener = {
    .enter         = pointer_enter,
    .leave         = pointer_leave,
    .motion        = pointer_motion,
    .button        = pointer_button,
    .axis          = pointer_axis,
    .frame         = NULL,
    .axis_source   = NULL,
    .axis_stop     = NULL,
    .axis_discrete = NULL,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void 
seat_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
    kUnused(data);
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !g_window_data.keyboard)
    {
        g_window_data.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(g_window_data.keyboard, &keyboard_listener, NULL);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && g_window_data.keyboard)
    {
        wl_keyboard_destroy(g_window_data.keyboard);
        g_window_data.keyboard = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !g_window_data.pointer) 
    {
        g_window_data.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(g_window_data.pointer, &pointer_listener, NULL);
    } 
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && g_window_data.pointer) 
    {
        wl_pointer_destroy(g_window_data.pointer);
        g_window_data.pointer = NULL;
    }
}

static void 
seat_name(void *data, struct wl_seat *seat, const char *name) {
    kUnused(data);
    kUnused(seat);
    printf("Seat '%s'n", name);
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities, 
    .name         = NULL,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// pixel format description
//
// Informs the client about a valid pixel format that can be used
// for buffers. Known formats include argb8888 and xrgb8888.
//
// format: buffer pixel format
static void 
shm_format(void *data, struct wl_shm *shm, uint32_t format)
{
    kUnused(data);
    kUnused(shm);
    if (g_window_data.shm_format == -1u)
    {
        switch (format)
        {
            // We could do RGBA, but that would not be what is expected from minifb...
            // case WL_SHM_FORMAT_ARGB8888: 
            case WL_SHM_FORMAT_XRGB8888:
                g_window_data.shm_format = format;
            break;

            default:
            break;
        }
    }
}

static const struct wl_shm_listener shm_listener = {
    .format = shm_format
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void 
registry_global(void *data, struct wl_registry *registry, uint32_t id, char const *iface, uint32_t version)
{
    kUnused(data);
    kUnused(version);
    if (strcmp(iface, "wl_compositor") == 0)
    {
        g_window_data.compositor = (struct wl_compositor *) wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }
    else if (strcmp(iface, "wl_shm") == 0)
    {
        g_window_data.shm = (struct wl_shm *) wl_registry_bind(registry, id, &wl_shm_interface, 1);
        if (g_window_data.shm) {
            wl_shm_add_listener(g_window_data.shm, &shm_listener, NULL);
            g_window_data.cursor_theme = wl_cursor_theme_load(NULL, 32, g_window_data.shm);
            g_window_data.default_cursor = wl_cursor_theme_get_cursor(g_window_data.cursor_theme, "left_ptr");
        }
    }
    else if (strcmp(iface, "wl_shell") == 0)
    {
        g_window_data.shell = (struct wl_shell *) wl_registry_bind(registry, id, &wl_shell_interface, 1);
    }
    else if (strcmp(iface, "wl_seat") == 0)
    {
        g_window_data.seat = (struct wl_seat *) wl_registry_bind(registry, id, &wl_seat_interface, 1);
        if (g_window_data.seat)
        {
            wl_seat_add_listener(g_window_data.seat, &seat_listener, NULL);
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    .global        = registry_global, 
    .global_remove = NULL,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int 
mfb_open_ex(const char* title, int width, int height, int flags) {
    // TODO: Not yet
    kUnused(flags);
    return mfb_open(title, width, height);
}

int 
mfb_open(const char *title, int width, int height)
{
    int fd = -1;

    g_window_data.shm_format = -1u;

    g_window_data.display = wl_display_connect(NULL);
    if (!g_window_data.display)
        return -1;
    g_window_data.registry = wl_display_get_registry(g_window_data.display);
    wl_registry_add_listener(g_window_data.registry, &registry_listener, NULL);

    init_keycodes();

    if (wl_display_dispatch(g_window_data.display) == -1 || wl_display_roundtrip(g_window_data.display) == -1)
    {
        return -1;
    }

    // did not get a format we want... meh
    if (g_window_data.shm_format == -1u)
        goto out;
    if (!g_window_data.compositor)
        goto out;

    char const *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");
    char shmfile[PATH_MAX];
    uint32_t ret = snprintf(shmfile, sizeof(shmfile), "%s/WaylandMiniFB-SHM-XXXXXX", xdg_rt_dir);
    if (ret >= sizeof(shmfile))
        goto out;

    fd = mkstemp(shmfile);
    if (fd == -1)
        goto out;
    unlink(shmfile);

    uint32_t length = sizeof(uint32_t) * width * height;

    if (ftruncate(fd, length) == -1)
        goto out;

    g_window_data.shm_ptr = (uint32_t *) mmap(NULL, length, PROT_WRITE, MAP_SHARED, fd, 0);
    if (g_window_data.shm_ptr == MAP_FAILED)
        goto out;

    g_window_data.window_width  = width;
    g_window_data.window_height = height;
    g_window_data.buffer_width  = width;
    g_window_data.buffer_height = height;
    g_window_data.buffer_stride = width * sizeof(uint32_t);
    g_window_data.dst_offset_x  = 0;
    g_window_data.dst_offset_y  = 0;
    g_window_data.dst_width     = width;
    g_window_data.dst_height    = height;

    g_window_data.shm_pool    = wl_shm_create_pool(g_window_data.shm, fd, length);
    g_window_data.draw_buffer = wl_shm_pool_create_buffer(g_window_data.shm_pool, 0, 
                                    g_window_data.buffer_width, g_window_data.buffer_height,
                                    g_window_data.buffer_stride, g_window_data.shm_format);

    close(fd);
    fd = -1;

    g_window_data.surface = wl_compositor_create_surface(g_window_data.compositor);
    if (!g_window_data.surface)
        goto out;

    g_window_data.cursor_surface = wl_compositor_create_surface(g_window_data.compositor);

    // There should always be a shell, right?
    if (g_window_data.shell)
    {
        g_window_data.shell_surface = wl_shell_get_shell_surface(g_window_data.shell, g_window_data.surface);
        if (!g_window_data.shell_surface)
            goto out;

        wl_shell_surface_set_title(g_window_data.shell_surface, title);
        wl_shell_surface_set_toplevel(g_window_data.shell_surface);
    }

    wl_surface_attach(g_window_data.surface, g_window_data.draw_buffer, g_window_data.dst_offset_x, g_window_data.dst_offset_y);
    wl_surface_damage(g_window_data.surface, g_window_data.dst_offset_x, g_window_data.dst_offset_y, g_window_data.dst_width, g_window_data.dst_height);
    wl_surface_commit(g_window_data.surface);

    return 1;

out:
    close(fd);
    destroy();
    return 0;
}

// done event
//
// Notify the client when the related request is done.
//
// callback_data: request-specific data for the callback
static void 
frame_done(void *data, struct wl_callback *callback, uint32_t cookie)
{
    kUnused(cookie);
    wl_callback_destroy(callback);
    *(uint32_t *)data = 1;
}

static const struct wl_callback_listener frame_listener = {
    .done = frame_done,
};

int 
mfb_update(void *buffer)
{
    uint32_t done = 0;

    if (!g_window_data.display || wl_display_get_error(g_window_data.display) != 0)
        return -1;

    if (g_window_data.close == true)
        return -1;

    // update shm buffer
    memcpy(g_window_data.shm_ptr, buffer, g_window_data.buffer_stride * g_window_data.buffer_height);

    wl_surface_attach(g_window_data.surface, g_window_data.draw_buffer, g_window_data.dst_offset_x, g_window_data.dst_offset_y);
    wl_surface_damage(g_window_data.surface, g_window_data.dst_offset_x, g_window_data.dst_offset_y, g_window_data.dst_width, g_window_data.dst_height);

    struct wl_callback *frame = wl_surface_frame(g_window_data.surface);
    if (!frame)
        return -1;

    wl_callback_add_listener(frame, &frame_listener, &done);

    wl_surface_commit(g_window_data.surface);

    while (!done && g_window_data.close == false) {
    if (wl_display_dispatch(g_window_data.display) == -1 || wl_display_roundtrip(g_window_data.display) == -1)
        {
            wl_callback_destroy(frame);
            return -1;
        }
    }
    if(g_window_data.close == true) {
        destroy(); 
    }

    //static int counter = 0;
    //printf("update!: %d\n", counter++);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void 
mfb_close(void) { 
    g_window_data.close = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern short int keycodes[512];

void 
init_keycodes(void)
{
    // Clear keys
    for (size_t i = 0; i < sizeof(keycodes) / sizeof(keycodes[0]); ++i) 
        keycodes[i] = 0;

    keycodes[KEY_GRAVE]      = KB_KEY_GRAVE_ACCENT;
    keycodes[KEY_1]          = KB_KEY_1;
    keycodes[KEY_2]          = KB_KEY_2;
    keycodes[KEY_3]          = KB_KEY_3;
    keycodes[KEY_4]          = KB_KEY_4;
    keycodes[KEY_5]          = KB_KEY_5;
    keycodes[KEY_6]          = KB_KEY_6;
    keycodes[KEY_7]          = KB_KEY_7;
    keycodes[KEY_8]          = KB_KEY_8;
    keycodes[KEY_9]          = KB_KEY_9;
    keycodes[KEY_0]          = KB_KEY_0;
    keycodes[KEY_SPACE]      = KB_KEY_SPACE;
    keycodes[KEY_MINUS]      = KB_KEY_MINUS;
    keycodes[KEY_EQUAL]      = KB_KEY_EQUAL;
    keycodes[KEY_Q]          = KB_KEY_Q;
    keycodes[KEY_W]          = KB_KEY_W;
    keycodes[KEY_E]          = KB_KEY_E;
    keycodes[KEY_R]          = KB_KEY_R;
    keycodes[KEY_T]          = KB_KEY_T;
    keycodes[KEY_Y]          = KB_KEY_Y;
    keycodes[KEY_U]          = KB_KEY_U;
    keycodes[KEY_I]          = KB_KEY_I;
    keycodes[KEY_O]          = KB_KEY_O;
    keycodes[KEY_P]          = KB_KEY_P;
    keycodes[KEY_LEFTBRACE]  = KB_KEY_LEFT_BRACKET;
    keycodes[KEY_RIGHTBRACE] = KB_KEY_RIGHT_BRACKET;
    keycodes[KEY_A]          = KB_KEY_A;
    keycodes[KEY_S]          = KB_KEY_S;
    keycodes[KEY_D]          = KB_KEY_D;
    keycodes[KEY_F]          = KB_KEY_F;
    keycodes[KEY_G]          = KB_KEY_G;
    keycodes[KEY_H]          = KB_KEY_H;
    keycodes[KEY_J]          = KB_KEY_J;
    keycodes[KEY_K]          = KB_KEY_K;
    keycodes[KEY_L]          = KB_KEY_L;
    keycodes[KEY_SEMICOLON]  = KB_KEY_SEMICOLON;
    keycodes[KEY_APOSTROPHE] = KB_KEY_APOSTROPHE;
    keycodes[KEY_Z]          = KB_KEY_Z;
    keycodes[KEY_X]          = KB_KEY_X;
    keycodes[KEY_C]          = KB_KEY_C;
    keycodes[KEY_V]          = KB_KEY_V;
    keycodes[KEY_B]          = KB_KEY_B;
    keycodes[KEY_N]          = KB_KEY_N;
    keycodes[KEY_M]          = KB_KEY_M;
    keycodes[KEY_COMMA]      = KB_KEY_COMMA;
    keycodes[KEY_DOT]        = KB_KEY_PERIOD;
    keycodes[KEY_SLASH]      = KB_KEY_SLASH;
    keycodes[KEY_BACKSLASH]  = KB_KEY_BACKSLASH;
    keycodes[KEY_ESC]        = KB_KEY_ESCAPE;
    keycodes[KEY_TAB]        = KB_KEY_TAB;
    keycodes[KEY_LEFTSHIFT]  = KB_KEY_LEFT_SHIFT;
    keycodes[KEY_RIGHTSHIFT] = KB_KEY_RIGHT_SHIFT;
    keycodes[KEY_LEFTCTRL]   = KB_KEY_LEFT_CONTROL;
    keycodes[KEY_RIGHTCTRL]  = KB_KEY_RIGHT_CONTROL;
    keycodes[KEY_LEFTALT]    = KB_KEY_LEFT_ALT;
    keycodes[KEY_RIGHTALT]   = KB_KEY_RIGHT_ALT;
    keycodes[KEY_LEFTMETA]   = KB_KEY_LEFT_SUPER;
    keycodes[KEY_RIGHTMETA]  = KB_KEY_RIGHT_SUPER;
    keycodes[KEY_MENU]       = KB_KEY_MENU;
    keycodes[KEY_NUMLOCK]    = KB_KEY_NUM_LOCK;
    keycodes[KEY_CAPSLOCK]   = KB_KEY_CAPS_LOCK;
    keycodes[KEY_PRINT]      = KB_KEY_PRINT_SCREEN;
    keycodes[KEY_SCROLLLOCK] = KB_KEY_SCROLL_LOCK;
    keycodes[KEY_PAUSE]      = KB_KEY_PAUSE;
    keycodes[KEY_DELETE]     = KB_KEY_DELETE;
    keycodes[KEY_BACKSPACE]  = KB_KEY_BACKSPACE;
    keycodes[KEY_ENTER]      = KB_KEY_ENTER;
    keycodes[KEY_HOME]       = KB_KEY_HOME;
    keycodes[KEY_END]        = KB_KEY_END;
    keycodes[KEY_PAGEUP]     = KB_KEY_PAGE_UP;
    keycodes[KEY_PAGEDOWN]   = KB_KEY_PAGE_DOWN;
    keycodes[KEY_INSERT]     = KB_KEY_INSERT;
    keycodes[KEY_LEFT]       = KB_KEY_LEFT;
    keycodes[KEY_RIGHT]      = KB_KEY_RIGHT;
    keycodes[KEY_DOWN]       = KB_KEY_DOWN;
    keycodes[KEY_UP]         = KB_KEY_UP;
    keycodes[KEY_F1]         = KB_KEY_F1;
    keycodes[KEY_F2]         = KB_KEY_F2;
    keycodes[KEY_F3]         = KB_KEY_F3;
    keycodes[KEY_F4]         = KB_KEY_F4;
    keycodes[KEY_F5]         = KB_KEY_F5;
    keycodes[KEY_F6]         = KB_KEY_F6;
    keycodes[KEY_F7]         = KB_KEY_F7;
    keycodes[KEY_F8]         = KB_KEY_F8;
    keycodes[KEY_F9]         = KB_KEY_F9;
    keycodes[KEY_F10]        = KB_KEY_F10;
    keycodes[KEY_F11]        = KB_KEY_F11;
    keycodes[KEY_F12]        = KB_KEY_F12;
    keycodes[KEY_F13]        = KB_KEY_F13;
    keycodes[KEY_F14]        = KB_KEY_F14;
    keycodes[KEY_F15]        = KB_KEY_F15;
    keycodes[KEY_F16]        = KB_KEY_F16;
    keycodes[KEY_F17]        = KB_KEY_F17;
    keycodes[KEY_F18]        = KB_KEY_F18;
    keycodes[KEY_F19]        = KB_KEY_F19;
    keycodes[KEY_F20]        = KB_KEY_F20;
    keycodes[KEY_F21]        = KB_KEY_F21;
    keycodes[KEY_F22]        = KB_KEY_F22;
    keycodes[KEY_F23]        = KB_KEY_F23;
    keycodes[KEY_F24]        = KB_KEY_F24;
    keycodes[KEY_KPSLASH]    = KB_KEY_KP_DIVIDE;
    keycodes[KEY_KPDOT]      = KB_KEY_KP_MULTIPLY;
    keycodes[KEY_KPMINUS]    = KB_KEY_KP_SUBTRACT;
    keycodes[KEY_KPPLUS]     = KB_KEY_KP_ADD;
    keycodes[KEY_KP0]        = KB_KEY_KP_0;
    keycodes[KEY_KP1]        = KB_KEY_KP_1;
    keycodes[KEY_KP2]        = KB_KEY_KP_2;
    keycodes[KEY_KP3]        = KB_KEY_KP_3;
    keycodes[KEY_KP4]        = KB_KEY_KP_4;
    keycodes[KEY_KP5]        = KB_KEY_KP_5;
    keycodes[KEY_KP6]        = KB_KEY_KP_6;
    keycodes[KEY_KP7]        = KB_KEY_KP_7;
    keycodes[KEY_KP8]        = KB_KEY_KP_8;
    keycodes[KEY_KP9]        = KB_KEY_KP_9;
    keycodes[KEY_KPCOMMA]    = KB_KEY_KP_DECIMAL;
    keycodes[KEY_KPEQUAL]    = KB_KEY_KP_EQUAL;
    keycodes[KEY_KPENTER]    = KB_KEY_KP_ENTER;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool 
mfb_set_viewport(unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {

    if(offset_x + width > g_window_data.window_width) {
        return false;
    }
    if(offset_y + height > g_window_data.window_height) {
        return false;
    }

    // TODO: Not yet
    // g_window_data.dst_offset_x = offset_x;
    // g_window_data.dst_offset_y = offset_y;
    // g_window_data.dst_width    = width;
    // g_window_data.dst_height   = height;

    return false;
}

