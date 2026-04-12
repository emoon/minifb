#pragma once

#include <MiniFB_enums.h>
#include <stddef.h>
#include <stdint.h>

#define WAYLAND_MAX_OUTPUTS 16
#define WAYLAND_BUFFER_SLOTS 3
#define WAYLAND_CURSOR_THEME_CACHE_SIZE 8

struct wl_display;
struct wl_event_queue;
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
struct wp_viewporter;
struct wp_viewport;
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct xkb_compose_table;
struct xkb_compose_state;

typedef struct {
    struct wl_shm_pool  *pool;
    struct wl_buffer    *wl_buf;
    uint32_t            *shm_ptr;       // mmap'd pointer for this slot
    int                  fd;
    size_t               pool_size;
    unsigned             width;         // dimensions this wl_buf was created with
    unsigned             height;
    uint8_t              busy;          // 1 = compositor owns it
} SWaylandBufferSlot;

typedef struct {
    struct wl_display       *display;
    struct wl_registry      *registry;
    struct wl_compositor    *compositor;
    struct xdg_wm_base      *shell;
    struct wl_seat          *seat;
    struct wl_keyboard      *keyboard;

    struct wl_pointer       *pointer;
    struct wl_cursor_theme  *cursor_theme;
    struct wl_cursor        *default_cursor;
    struct wl_cursor_theme  *cursor_theme_cache[WAYLAND_CURSOR_THEME_CACHE_SIZE];
    struct wl_cursor        *default_cursor_cache[WAYLAND_CURSOR_THEME_CACHE_SIZE];
    uint32_t                cursor_theme_cache_scales[WAYLAND_CURSOR_THEME_CACHE_SIZE];
    uint32_t                cursor_theme_cache_count;
    uint32_t                cursor_theme_scale;
    struct wl_surface       *cursor_surface;
    uint32_t                pointer_serial;
    uint32_t                pointer_enter_serial;
    uint8_t                 pointer_serial_valid;
    uint8_t                 startup_state_applied;
    uint8_t                 request_fullscreen;
    uint8_t                 request_maximized;

    struct wl_shm           *shm;
    struct wp_fractional_scale_manager_v1 *fractional_scale_manager;
    struct wp_fractional_scale_v1 *fractional_scale;
    uint32_t                preferred_scale_120;
    struct wp_viewporter    *viewporter;
    struct wp_viewport      *viewport;
    struct wl_output        *outputs[WAYLAND_MAX_OUTPUTS];
    uint32_t                output_ids[WAYLAND_MAX_OUTPUTS];
    uint32_t                output_scales[WAYLAND_MAX_OUTPUTS];
    uint8_t                 output_entered[WAYLAND_MAX_OUTPUTS]; // 1 = surface is on this output
    uint32_t                output_count;
    uint32_t                integer_output_scale;                // max scale of entered outputs (fallback)
    struct wl_event_queue   *window_queue;
    struct wl_event_queue   *render_queue;
    struct wl_display       *window_display_wrapper;
    struct wl_display       *render_display_wrapper;

    struct wl_surface       *surface;
    struct wl_surface       *surface_wrapper;
    struct xdg_surface      *shell_surface;
    struct xdg_toplevel     *toplevel;
    struct zxdg_decoration_manager_v1 *decoration_manager;
    struct zxdg_toplevel_decoration_v1 *toplevel_decoration;

    uint32_t                compositor_version;
    uint32_t                seat_version;
    uint32_t                shm_format;
    struct wl_callback      *throttle_callback;
    SWaylandBufferSlot      slots[WAYLAND_BUFFER_SLOTS];
    int                     front_slot;

    struct mfb_timer        *timer;
    struct xkb_context      *xkb_context;
    struct xkb_keymap       *xkb_keymap;
    struct xkb_state        *xkb_state;
    struct xkb_compose_table *xkb_compose_table;
    struct xkb_compose_state *xkb_compose_state;
    uint32_t                compose_sequence[8]; // keycodes buffered during compose
    uint8_t                 compose_sequence_count;
} SWindowData_Way;
