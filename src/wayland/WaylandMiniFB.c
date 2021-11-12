#include <MiniFB.h>
#include "MiniFB_internal.h"
#include "MiniFB_enums.h"
#include "WindowData.h"
#include "WindowData_Way.h"

#include <wayland-client.h>
#include <wayland-cursor.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>

#include <sys/mman.h>

void init_keycodes();

static void
destroy_window_data(SWindowData *window_data)
{
    if(window_data == 0x0)
        return;

    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if(window_data_way != 0x0) {
        mfb_timer_destroy(window_data_way->timer);
        memset(window_data_way, 0, sizeof(SWindowData_Way));
        free(window_data_way);
    }
    memset(window_data, 0, sizeof(SWindowData));
    free(window_data);
}

static void
destroy(SWindowData *window_data)
{
    if(window_data == 0x0)
        return;

    SWindowData_Way *window_data_way = (SWindowData_Way *) window_data->specific;
    if (window_data_way == 0x0 || window_data_way->display == 0x0) {
        destroy_window_data(window_data);
        return;
    }

#define KILL(NAME)                                      \
    do                                                  \
    {                                                   \
        if (window_data_way->NAME)                      \
            wl_##NAME##_destroy(window_data_way->NAME); \
    } while (0);                                        \
    window_data_way->NAME = 0x0;

    KILL(shell_surface);
    KILL(shell);
    KILL(surface);
    //KILL(buffer);
    if(window_data->draw_buffer) {
        wl_buffer_destroy(window_data->draw_buffer);
        window_data->draw_buffer = 0x0;
    }
    KILL(shm_pool);
    KILL(shm);
    KILL(compositor);
    KILL(keyboard);
    KILL(seat);
    KILL(registry);
#undef KILL
    wl_display_disconnect(window_data_way->display);

    destroy_window_data(window_data);
    close(window_data_way->fd);
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
    kUnused(keyboard);
    kUnused(serial);
    kUnused(surface);
    kUnused(keys);

    SWindowData *window_data = (SWindowData *) data;
    window_data->is_active = true;
    kCall(active_func, true);
}

// The leave notification is sent before the enter notification for the new focus.
// serial:  serial number of the leave event
// surface: surface that lost keyboard focus
static void
keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
    kUnused(keyboard);
    kUnused(serial);
    kUnused(surface);

    SWindowData *window_data = (SWindowData *) data;
    window_data->is_active = false;
    kCall(active_func, false);
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
    kUnused(keyboard);
    kUnused(serial);
    kUnused(time);

    SWindowData *window_data = (SWindowData *) data;
    if(key < 512) {
        mfb_key key_code = (mfb_key) g_keycodes[key];
        bool   is_pressed = (bool) (state == WL_KEYBOARD_KEY_STATE_PRESSED);
        switch (key_code)
        {
            case KB_KEY_LEFT_SHIFT:
            case KB_KEY_RIGHT_SHIFT:
                if(is_pressed)
                    window_data->mod_keys |= KB_MOD_SHIFT;
                else
                    window_data->mod_keys &= ~KB_MOD_SHIFT;
                break;

            case KB_KEY_LEFT_CONTROL:
            case KB_KEY_RIGHT_CONTROL:
                if(is_pressed)
                    window_data->mod_keys |= KB_MOD_CONTROL;
                else
                    window_data->mod_keys &= ~KB_MOD_CONTROL;
                break;

            case KB_KEY_LEFT_ALT:
            case KB_KEY_RIGHT_ALT:
                if(is_pressed)
                    window_data->mod_keys |= KB_MOD_ALT;
                else
                    window_data->mod_keys &= ~KB_MOD_ALT;
                break;

            case KB_KEY_LEFT_SUPER:
            case KB_KEY_RIGHT_SUPER:
                if(is_pressed)
                    window_data->mod_keys |= KB_MOD_SUPER;
                else
                    window_data->mod_keys &= ~KB_MOD_SUPER;
                break;
        }

        window_data->key_status[key_code] = is_pressed;
        kCall(keyboard_func, key_code, (mfb_key_mod) window_data->mod_keys, is_pressed);
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

static const struct
wl_keyboard_listener keyboard_listener = {
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
    .repeat_info = 0x0,
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
    //kUnused(pointer);
    //kUnused(serial);
    kUnused(surface);
    kUnused(sx);
    kUnused(sy);
    struct wl_buffer *buffer;
    struct wl_cursor_image *image;

    SWindowData *window_data = (SWindowData *) data;
    SWindowData_Way *window_data_way = (SWindowData_Way *) window_data->specific;

    image  = window_data_way->default_cursor->images[0];
    buffer = wl_cursor_image_get_buffer(image);

    wl_pointer_set_cursor(pointer, serial, window_data_way->cursor_surface, image->hotspot_x, image->hotspot_y);
    wl_surface_attach(window_data_way->cursor_surface, buffer, 0, 0);
    wl_surface_damage(window_data_way->cursor_surface, 0, 0, image->width, image->height);
    wl_surface_commit(window_data_way->cursor_surface);
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
    kUnused(pointer);
    kUnused(time);

    //printf("Pointer moved at %f %f\n", sx / 256.0f, sy / 256.0f);
    SWindowData *window_data = (SWindowData *) data;
    window_data->mouse_pos_x = sx >> 24;
    window_data->mouse_pos_y = sy >> 24;
    kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);
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
    kUnused(pointer);
    kUnused(serial);
    kUnused(time);

    //printf("Pointer button '%d'(%d)\n", button, state);
    SWindowData *window_data = (SWindowData *) data;
    window_data->mouse_button_status[(button - BTN_MOUSE + 1) & 0x07] = (state == 1);
    kCall(mouse_btn_func, (mfb_mouse_button) (button - BTN_MOUSE + 1), (mfb_key_mod) window_data->mod_keys, state == 1);
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
    kUnused(pointer);
    kUnused(time);
    kUnused(axis);

    //printf("Pointer handle axis: axis: %d (0x%x)\n", axis, value);
    SWindowData *window_data = (SWindowData *) data;
    if(axis == 0) {
        window_data->mouse_wheel_y = -(value / 256.0f);
        kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, 0.0f, window_data->mouse_wheel_y);
    }
    else if(axis == 1) {
        window_data->mouse_wheel_x = -(value / 256.0f);
        kCall(mouse_wheel_func, (mfb_key_mod) window_data->mod_keys, window_data->mouse_wheel_x, 0.0f);
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

static const struct
wl_pointer_listener pointer_listener = {
    .enter         = pointer_enter,
    .leave         = pointer_leave,
    .motion        = pointer_motion,
    .button        = pointer_button,
    .axis          = pointer_axis,
    .frame         = 0x0,
    .axis_source   = 0x0,
    .axis_stop     = 0x0,
    .axis_discrete = 0x0,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
seat_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
    kUnused(data);

    SWindowData         *window_data = (SWindowData *) data;
    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !window_data_way->keyboard)
    {
        window_data_way->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(window_data_way->keyboard, &keyboard_listener, window_data);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && window_data_way->keyboard)
    {
        wl_keyboard_destroy(window_data_way->keyboard);
        window_data_way->keyboard = 0x0;
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !window_data_way->pointer)
    {
        window_data_way->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(window_data_way->pointer, &pointer_listener, window_data);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && window_data_way->pointer)
    {
        wl_pointer_destroy(window_data_way->pointer);
        window_data_way->pointer = 0x0;
    }
}

static void
seat_name(void *data, struct wl_seat *seat, const char *name) {
    kUnused(data);
    kUnused(seat);
    printf("Seat '%s'n", name);
}

static const struct
wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name         = 0x0,
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
    kUnused(shm);

    SWindowData         *window_data     = (SWindowData *) data;
    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if (window_data_way->shm_format == -1u)
    {
        switch (format)
        {
            // We could do RGBA, but that would not be what is expected from minifb...
            // case WL_SHM_FORMAT_ARGB8888:
            case WL_SHM_FORMAT_XRGB8888:
                window_data_way->shm_format = format;
            break;

            default:
            break;
        }
    }
}

static const struct
wl_shm_listener shm_listener = {
    .format = shm_format
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
registry_global(void *data, struct wl_registry *registry, uint32_t id, char const *iface, uint32_t version)
{
    kUnused(version);

    SWindowData         *window_data     = (SWindowData *) data;
    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if (strcmp(iface, "wl_compositor") == 0)
    {
        window_data_way->compositor = (struct wl_compositor *) wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }
    else if (strcmp(iface, "wl_shm") == 0)
    {
        window_data_way->shm = (struct wl_shm *) wl_registry_bind(registry, id, &wl_shm_interface, 1);
        if (window_data_way->shm) {
            wl_shm_add_listener(window_data_way->shm, &shm_listener, window_data);
            window_data_way->cursor_theme = wl_cursor_theme_load(0x0, 32, window_data_way->shm);
            window_data_way->default_cursor = wl_cursor_theme_get_cursor(window_data_way->cursor_theme, "left_ptr");
        }
    }
    else if (strcmp(iface, "wl_shell") == 0)
    {
        window_data_way->shell = (struct wl_shell *) wl_registry_bind(registry, id, &wl_shell_interface, 1);
    }
    else if (strcmp(iface, "wl_seat") == 0)
    {
        window_data_way->seat = (struct wl_seat *) wl_registry_bind(registry, id, &wl_seat_interface, 1);
        if (window_data_way->seat)
        {
            wl_seat_add_listener(window_data_way->seat, &seat_listener, window_data);
        }
    }
}

static const struct
wl_registry_listener registry_listener = {
    .global        = registry_global,
    .global_remove = 0x0,
};

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
    kUnused(data);
    wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
    kUnused(data);
    kUnused(shell_surface);
    kUnused(edges);
    kUnused(width);
    kUnused(height);
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
    kUnused(data);
    kUnused(shell_surface);
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    handle_ping,
    handle_configure,
    handle_popup_done
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags)
{
    SWindowData *window_data = (SWindowData *) malloc(sizeof(SWindowData));
    if(window_data == 0x0) {
        return 0x0;
    }
    memset(window_data, 0, sizeof(SWindowData));

    SWindowData_Way *window_data_way = (SWindowData_Way *) malloc(sizeof(SWindowData_Way));
    if(window_data_way == 0x0) {
        free(window_data);
        return 0x0;
    }
    memset(window_data_way, 0, sizeof(SWindowData_Way));
    window_data->specific = window_data_way;

    window_data_way->shm_format = -1u;

    window_data_way->display = wl_display_connect(0x0);
    if (!window_data_way->display) {
        free(window_data);
        free(window_data_way);
        return 0x0;
    }
    window_data_way->registry = wl_display_get_registry(window_data_way->display);
    wl_registry_add_listener(window_data_way->registry, &registry_listener, window_data);

    init_keycodes();

    if (wl_display_dispatch(window_data_way->display) == -1 ||
        wl_display_roundtrip(window_data_way->display) == -1) {
        return 0x0;
    }

    // did not get a format we want... meh
    if (window_data_way->shm_format == -1u)
        goto out;
    if (!window_data_way->compositor)
        goto out;

    char const *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");
    char shmfile[PATH_MAX];
    uint32_t ret = snprintf(shmfile, sizeof(shmfile), "%s/WaylandMiniFB-SHM-XXXXXX", xdg_rt_dir);
    if (ret >= sizeof(shmfile))
        goto out;

    window_data_way->fd = mkstemp(shmfile);
    if (window_data_way->fd == -1)
        goto out;
    unlink(shmfile);

    uint32_t length = sizeof(uint32_t) * width * height;

    if (ftruncate(window_data_way->fd, length) == -1)
        goto out;

    window_data_way->shm_ptr = (uint32_t *) mmap(0x0, length, PROT_WRITE, MAP_SHARED, window_data_way->fd, 0);
    if (window_data_way->shm_ptr == MAP_FAILED)
        goto out;

    window_data->window_width  = width;
    window_data->window_height = height;
    window_data->buffer_width  = width;
    window_data->buffer_height = height;
    window_data->buffer_stride = width * sizeof(uint32_t);
    calc_dst_factor(window_data, width, height);

    window_data_way->shm_pool  = wl_shm_create_pool(window_data_way->shm, window_data_way->fd, length);
    window_data->draw_buffer   = wl_shm_pool_create_buffer(window_data_way->shm_pool, 0,
                                    window_data->buffer_width, window_data->buffer_height,
                                    window_data->buffer_stride, window_data_way->shm_format);

    window_data_way->surface = wl_compositor_create_surface(window_data_way->compositor);
    if (!window_data_way->surface)
        goto out;

    window_data_way->cursor_surface = wl_compositor_create_surface(window_data_way->compositor);

    // There should always be a shell, right?
    if (window_data_way->shell)
    {
        window_data_way->shell_surface = wl_shell_get_shell_surface(window_data_way->shell, window_data_way->surface);
        if (!window_data_way->shell_surface)
            goto out;

        wl_shell_surface_set_title(window_data_way->shell_surface, title);
        wl_shell_surface_add_listener(window_data_way->shell_surface, &shell_surface_listener, 0x0);
        wl_shell_surface_set_toplevel(window_data_way->shell_surface);
    }

    wl_surface_attach(window_data_way->surface, (struct wl_buffer *) window_data->draw_buffer, window_data->dst_offset_x, window_data->dst_offset_y);
    wl_surface_damage(window_data_way->surface, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height);
    wl_surface_commit(window_data_way->surface);

    window_data_way->timer = mfb_timer_create();

    mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

#if defined(_DEBUG) || defined(DEBUG)
    printf("Window created using Wayland API\n");
#endif

    window_data->is_initialized = true;
    return (struct mfb_window *) window_data;

out:
    close(window_data_way->fd);
    destroy(window_data);

    return 0x0;
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

static const struct
wl_callback_listener frame_listener = {
    .done = frame_done,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height)
{
    uint32_t done = 0;

    if(window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        destroy(window_data);
        return STATE_EXIT;
    }

    if(buffer == 0x0) {
        return STATE_INVALID_BUFFER;
    }

    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if (!window_data_way->display || wl_display_get_error(window_data_way->display) != 0)
        return STATE_INTERNAL_ERROR;

    if(window_data->buffer_width != width || window_data->buffer_height != height) {
        uint32_t oldLength = sizeof(uint32_t) * window_data->buffer_width * window_data->buffer_height;
        uint32_t length    = sizeof(uint32_t) * width * height;

        // For some reason it crash when you make it smaller
        if(oldLength < length) {
            if (ftruncate(window_data_way->fd, length) == -1)
                return STATE_INTERNAL_ERROR;

            //munmap(window_data_way->shm_ptr, sizeof(uint32_t) * window_data->buffer_width * window_data->buffer_height);
            window_data_way->shm_ptr = (uint32_t *) mmap(0x0, length, PROT_WRITE, MAP_SHARED, window_data_way->fd, 0);
            if (window_data_way->shm_ptr == MAP_FAILED)
                return STATE_INTERNAL_ERROR;

            wl_shm_pool_resize(window_data_way->shm_pool, length);
        }

        window_data->buffer_width  = width;
        window_data->buffer_height = height;
        window_data->buffer_stride = width * sizeof(uint32_t);

        // This must be in the resize event but we don't have it for Wayland :(
        resize_dst(window_data, width, height);

        wl_buffer_destroy(window_data->draw_buffer);
        window_data->draw_buffer = wl_shm_pool_create_buffer(window_data_way->shm_pool, 0,
                                        window_data->buffer_width, window_data->buffer_height,
                                        window_data->buffer_stride, window_data_way->shm_format);
    }

    // update shm buffer
    memcpy(window_data_way->shm_ptr, buffer, window_data->buffer_stride * window_data->buffer_height);

    wl_surface_attach(window_data_way->surface, (struct wl_buffer *) window_data->draw_buffer, window_data->dst_offset_x, window_data->dst_offset_y);
    wl_surface_damage(window_data_way->surface, window_data->dst_offset_x, window_data->dst_offset_y, window_data->dst_width, window_data->dst_height);
    struct wl_callback *frame_callback = wl_surface_frame(window_data_way->surface);
    if (!frame_callback) {
        return STATE_INTERNAL_ERROR;
    }
    wl_callback_add_listener(frame_callback, &frame_listener, &done);
    wl_surface_commit(window_data_way->surface);

    while (!done && window_data->close == false) {
        if (wl_display_dispatch(window_data_way->display) == -1 || wl_display_roundtrip(window_data_way->display) == -1) {
            wl_callback_destroy(frame_callback);
            return STATE_INTERNAL_ERROR;
        }
    }

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mfb_update_state
mfb_update_events(struct mfb_window *window)
{
    if(window == 0x0) {
        return STATE_INVALID_WINDOW;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        destroy(window_data);
        return STATE_EXIT;
    }

    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    if (!window_data_way->display || wl_display_get_error(window_data_way->display) != 0)
        return STATE_INTERNAL_ERROR;

    if (wl_display_dispatch_pending(window_data_way->display) == -1) {
        return STATE_INTERNAL_ERROR;
    }

    return STATE_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern double   g_time_for_frame;

bool
mfb_wait_sync(struct mfb_window *window) {
    if(window == 0x0) {
        return false;
    }

    SWindowData *window_data = (SWindowData *) window;
    if(window_data->close) {
        destroy(window_data);
        return false;
    }

    SWindowData_Way   *window_data_way = (SWindowData_Way *) window_data->specific;
    double      current;
    uint32_t    millis = 1;
    while(1) {
        current = mfb_timer_now(window_data_way->timer);
        if (current >= g_time_for_frame * 0.96) {
            mfb_timer_reset(window_data_way->timer);
            return true;
        }
        else if(current >= g_time_for_frame * 0.8) {
            millis = 0;
        }

        usleep(millis * 1000);
        //sched_yield();

        if(millis == 1) {
            if (wl_display_dispatch_pending(window_data_way->display) == -1) {
                return false;
            }

            if(window_data->close) {
                destroy_window_data(window_data);
                return false;
            }
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern short int g_keycodes[512];

void
init_keycodes(void)
{
    // Clear keys
    for (size_t i = 0; i < sizeof(g_keycodes) / sizeof(g_keycodes[0]); ++i)
        g_keycodes[i] = 0;

    g_keycodes[KEY_GRAVE]      = KB_KEY_GRAVE_ACCENT;
    g_keycodes[KEY_1]          = KB_KEY_1;
    g_keycodes[KEY_2]          = KB_KEY_2;
    g_keycodes[KEY_3]          = KB_KEY_3;
    g_keycodes[KEY_4]          = KB_KEY_4;
    g_keycodes[KEY_5]          = KB_KEY_5;
    g_keycodes[KEY_6]          = KB_KEY_6;
    g_keycodes[KEY_7]          = KB_KEY_7;
    g_keycodes[KEY_8]          = KB_KEY_8;
    g_keycodes[KEY_9]          = KB_KEY_9;
    g_keycodes[KEY_0]          = KB_KEY_0;
    g_keycodes[KEY_SPACE]      = KB_KEY_SPACE;
    g_keycodes[KEY_MINUS]      = KB_KEY_MINUS;
    g_keycodes[KEY_EQUAL]      = KB_KEY_EQUAL;
    g_keycodes[KEY_Q]          = KB_KEY_Q;
    g_keycodes[KEY_W]          = KB_KEY_W;
    g_keycodes[KEY_E]          = KB_KEY_E;
    g_keycodes[KEY_R]          = KB_KEY_R;
    g_keycodes[KEY_T]          = KB_KEY_T;
    g_keycodes[KEY_Y]          = KB_KEY_Y;
    g_keycodes[KEY_U]          = KB_KEY_U;
    g_keycodes[KEY_I]          = KB_KEY_I;
    g_keycodes[KEY_O]          = KB_KEY_O;
    g_keycodes[KEY_P]          = KB_KEY_P;
    g_keycodes[KEY_LEFTBRACE]  = KB_KEY_LEFT_BRACKET;
    g_keycodes[KEY_RIGHTBRACE] = KB_KEY_RIGHT_BRACKET;
    g_keycodes[KEY_A]          = KB_KEY_A;
    g_keycodes[KEY_S]          = KB_KEY_S;
    g_keycodes[KEY_D]          = KB_KEY_D;
    g_keycodes[KEY_F]          = KB_KEY_F;
    g_keycodes[KEY_G]          = KB_KEY_G;
    g_keycodes[KEY_H]          = KB_KEY_H;
    g_keycodes[KEY_J]          = KB_KEY_J;
    g_keycodes[KEY_K]          = KB_KEY_K;
    g_keycodes[KEY_L]          = KB_KEY_L;
    g_keycodes[KEY_SEMICOLON]  = KB_KEY_SEMICOLON;
    g_keycodes[KEY_APOSTROPHE] = KB_KEY_APOSTROPHE;
    g_keycodes[KEY_Z]          = KB_KEY_Z;
    g_keycodes[KEY_X]          = KB_KEY_X;
    g_keycodes[KEY_C]          = KB_KEY_C;
    g_keycodes[KEY_V]          = KB_KEY_V;
    g_keycodes[KEY_B]          = KB_KEY_B;
    g_keycodes[KEY_N]          = KB_KEY_N;
    g_keycodes[KEY_M]          = KB_KEY_M;
    g_keycodes[KEY_COMMA]      = KB_KEY_COMMA;
    g_keycodes[KEY_DOT]        = KB_KEY_PERIOD;
    g_keycodes[KEY_SLASH]      = KB_KEY_SLASH;
    g_keycodes[KEY_BACKSLASH]  = KB_KEY_BACKSLASH;
    g_keycodes[KEY_ESC]        = KB_KEY_ESCAPE;
    g_keycodes[KEY_TAB]        = KB_KEY_TAB;
    g_keycodes[KEY_LEFTSHIFT]  = KB_KEY_LEFT_SHIFT;
    g_keycodes[KEY_RIGHTSHIFT] = KB_KEY_RIGHT_SHIFT;
    g_keycodes[KEY_LEFTCTRL]   = KB_KEY_LEFT_CONTROL;
    g_keycodes[KEY_RIGHTCTRL]  = KB_KEY_RIGHT_CONTROL;
    g_keycodes[KEY_LEFTALT]    = KB_KEY_LEFT_ALT;
    g_keycodes[KEY_RIGHTALT]   = KB_KEY_RIGHT_ALT;
    g_keycodes[KEY_LEFTMETA]   = KB_KEY_LEFT_SUPER;
    g_keycodes[KEY_RIGHTMETA]  = KB_KEY_RIGHT_SUPER;
    g_keycodes[KEY_MENU]       = KB_KEY_MENU;
    g_keycodes[KEY_NUMLOCK]    = KB_KEY_NUM_LOCK;
    g_keycodes[KEY_CAPSLOCK]   = KB_KEY_CAPS_LOCK;
    g_keycodes[KEY_PRINT]      = KB_KEY_PRINT_SCREEN;
    g_keycodes[KEY_SCROLLLOCK] = KB_KEY_SCROLL_LOCK;
    g_keycodes[KEY_PAUSE]      = KB_KEY_PAUSE;
    g_keycodes[KEY_DELETE]     = KB_KEY_DELETE;
    g_keycodes[KEY_BACKSPACE]  = KB_KEY_BACKSPACE;
    g_keycodes[KEY_ENTER]      = KB_KEY_ENTER;
    g_keycodes[KEY_HOME]       = KB_KEY_HOME;
    g_keycodes[KEY_END]        = KB_KEY_END;
    g_keycodes[KEY_PAGEUP]     = KB_KEY_PAGE_UP;
    g_keycodes[KEY_PAGEDOWN]   = KB_KEY_PAGE_DOWN;
    g_keycodes[KEY_INSERT]     = KB_KEY_INSERT;
    g_keycodes[KEY_LEFT]       = KB_KEY_LEFT;
    g_keycodes[KEY_RIGHT]      = KB_KEY_RIGHT;
    g_keycodes[KEY_DOWN]       = KB_KEY_DOWN;
    g_keycodes[KEY_UP]         = KB_KEY_UP;
    g_keycodes[KEY_F1]         = KB_KEY_F1;
    g_keycodes[KEY_F2]         = KB_KEY_F2;
    g_keycodes[KEY_F3]         = KB_KEY_F3;
    g_keycodes[KEY_F4]         = KB_KEY_F4;
    g_keycodes[KEY_F5]         = KB_KEY_F5;
    g_keycodes[KEY_F6]         = KB_KEY_F6;
    g_keycodes[KEY_F7]         = KB_KEY_F7;
    g_keycodes[KEY_F8]         = KB_KEY_F8;
    g_keycodes[KEY_F9]         = KB_KEY_F9;
    g_keycodes[KEY_F10]        = KB_KEY_F10;
    g_keycodes[KEY_F11]        = KB_KEY_F11;
    g_keycodes[KEY_F12]        = KB_KEY_F12;
    g_keycodes[KEY_F13]        = KB_KEY_F13;
    g_keycodes[KEY_F14]        = KB_KEY_F14;
    g_keycodes[KEY_F15]        = KB_KEY_F15;
    g_keycodes[KEY_F16]        = KB_KEY_F16;
    g_keycodes[KEY_F17]        = KB_KEY_F17;
    g_keycodes[KEY_F18]        = KB_KEY_F18;
    g_keycodes[KEY_F19]        = KB_KEY_F19;
    g_keycodes[KEY_F20]        = KB_KEY_F20;
    g_keycodes[KEY_F21]        = KB_KEY_F21;
    g_keycodes[KEY_F22]        = KB_KEY_F22;
    g_keycodes[KEY_F23]        = KB_KEY_F23;
    g_keycodes[KEY_F24]        = KB_KEY_F24;
    g_keycodes[KEY_KPSLASH]    = KB_KEY_KP_DIVIDE;
    g_keycodes[KEY_KPDOT]      = KB_KEY_KP_MULTIPLY;
    g_keycodes[KEY_KPMINUS]    = KB_KEY_KP_SUBTRACT;
    g_keycodes[KEY_KPPLUS]     = KB_KEY_KP_ADD;
    g_keycodes[KEY_KP0]        = KB_KEY_KP_0;
    g_keycodes[KEY_KP1]        = KB_KEY_KP_1;
    g_keycodes[KEY_KP2]        = KB_KEY_KP_2;
    g_keycodes[KEY_KP3]        = KB_KEY_KP_3;
    g_keycodes[KEY_KP4]        = KB_KEY_KP_4;
    g_keycodes[KEY_KP5]        = KB_KEY_KP_5;
    g_keycodes[KEY_KP6]        = KB_KEY_KP_6;
    g_keycodes[KEY_KP7]        = KB_KEY_KP_7;
    g_keycodes[KEY_KP8]        = KB_KEY_KP_8;
    g_keycodes[KEY_KP9]        = KB_KEY_KP_9;
    g_keycodes[KEY_KPCOMMA]    = KB_KEY_KP_DECIMAL;
    g_keycodes[KEY_KPEQUAL]    = KB_KEY_KP_EQUAL;
    g_keycodes[KEY_KPENTER]    = KB_KEY_KP_ENTER;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {

    SWindowData *window_data = (SWindowData *) window;

    if(offset_x + width > window_data->window_width) {
        return false;
    }
    if(offset_y + height > window_data->window_height) {
        return false;
    }

    // TODO: Not yet
    window_data->dst_offset_x = offset_x;
    window_data->dst_offset_y = offset_y;
    window_data->dst_width    = width;
    window_data->dst_height   = height;
    resize_dst(window_data, width, height);

    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
    float x = 96.0, y = 96.0;

    if(window != 0x0) {
        //SWindowData     *window_data     = (SWindowData *) window;
        //SWindowData_X11 *window_data_x11 = (SWindowData_X11 *) window_data->specific;

        // I cannot find a way to get dpi under VirtualBox
    }

    if (scale_x) {
        *scale_x = x / 96.0f;
        if(*scale_x == 0) {
            *scale_x = 1.0f;
        }
    }

    if (scale_y) {
        *scale_y = y / 96.0f;
        if (*scale_y == 0) {
            *scale_y = 1.0f;
        }
    }
}
