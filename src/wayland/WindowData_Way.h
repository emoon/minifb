#pragma once

#include <MiniFB_enums.h>
#include <stdint.h>

#define KEYCODE_COUNT 512

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
struct wl_surface;
struct wl_shell_surface;
struct wl_buffer;

typedef struct
{
    struct wl_display                     *display;
    struct wl_registry                    *registry;
    struct wl_compositor                  *compositor;
    struct xdg_wm_base                    *shell;
    struct wl_seat                        *seat;
    struct wp_fractional_scale_manager_v1 *fractional_scale_manager;
    
    struct wl_keyboard                    *keyboard;
    struct xkb_state                      *xkb_state;

    struct wl_pointer                     *pointer;
    struct wl_cursor_theme                *cursor_theme;
    struct wl_cursor                      *default_cursor;
    struct wl_surface                     *cursor_surface;
    struct wp_fractional_scale_v1         *fractional_scale;

    struct wl_shm                         *shm;
    struct wl_shm_pool                    *shm_pool;
    struct wl_surface                     *surface;
    struct xdg_surface                    *shell_surface;
    struct xdg_toplevel                   *toplevel;

    
    uint32_t                               seat_version;
    uint32_t                               shm_format;
    uint32_t                              *shm_ptr;
    uint32_t                               pool_size;
    float                                  scale;

    int                                    fd;

    struct mfb_timer                      *timer;
} SWindowData_Way;
