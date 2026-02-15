#pragma once

#include <MiniFB_enums.h>
#include <stddef.h>
#include <stdint.h>

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_shell;
struct wl_seat;
struct wl_keyboard;
struct wl_pointer;
struct wl_callback;
struct wl_shm;
struct wl_shm_pool;
struct wl_output;
struct wl_surface;
struct wl_shell_surface;
struct wl_buffer;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

typedef struct
{
    struct wl_display       *display;
    struct wl_registry      *registry;
    struct wl_compositor    *compositor;
    struct xdg_wm_base      *shell;
    struct wl_seat          *seat;
    struct wl_keyboard      *keyboard;

    struct wl_pointer       *pointer;
    struct wl_cursor_theme  *cursor_theme;
    struct wl_cursor        *default_cursor;
    struct wl_surface       *cursor_surface;
    uint32_t                pointer_serial;
    uint8_t                 pointer_serial_valid;
    uint8_t                 startup_state_applied;
    uint8_t                 request_fullscreen;
    uint8_t                 request_maximized;

    struct wl_shm           *shm;
    struct wl_shm_pool      *shm_pool;
    struct wp_fractional_scale_manager_v1 *fractional_scale_manager;
    struct wp_fractional_scale_v1 *fractional_scale;
    uint32_t                preferred_scale_120;
    struct wl_output        *outputs[16];
    uint32_t                output_scales[16];
    uint32_t                output_count;
    struct wl_output        *current_output;
    uint32_t                current_output_scale;
    struct wl_surface       *surface;
    struct xdg_surface      *shell_surface;
    struct xdg_toplevel     *toplevel;
    struct zxdg_decoration_manager_v1 *decoration_manager;
    struct zxdg_toplevel_decoration_v1 *toplevel_decoration;

    uint32_t                seat_version;
    uint32_t                shm_format;
    size_t                  shm_length;
    size_t                  shm_pool_size;
    uint32_t                *shm_ptr;

    int                     fd;

    struct mfb_timer        *timer;
    struct xkb_context      *xkb_context;
    struct xkb_keymap       *xkb_keymap;
    struct xkb_state        *xkb_state;
} SWindowData_Way;
