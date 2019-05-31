#pragma once

#include <MiniFB_enums.h>
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
struct wl_surface;
struct wl_shell_surface;
struct wl_buffer;

typedef struct
{
    struct wl_display       *display;
    struct wl_registry      *registry;
    struct wl_compositor    *compositor;
    struct wl_shell         *shell;
    struct wl_seat          *seat;
    struct wl_keyboard      *keyboard;
    
    struct wl_pointer       *pointer;
    struct wl_cursor_theme  *cursor_theme;
    struct wl_cursor        *default_cursor;
    struct wl_surface       *cursor_surface;
    
    struct wl_shm           *shm;
    struct wl_shm_pool      *shm_pool;
    struct wl_surface       *surface;
    struct wl_shell_surface *shell_surface;

    uint32_t                seat_version;
    uint32_t                shm_format;
    uint32_t                *shm_ptr;

    uint32_t                window_width;
    uint32_t                window_height;
    uint32_t                dst_offset_x;
    uint32_t                dst_offset_y;
    uint32_t                dst_width;
    uint32_t                dst_height;

    struct wl_buffer        *draw_buffer;
    uint32_t                buffer_width;
    uint32_t                buffer_height;
    uint32_t                buffer_stride;

    uint32_t                mod_keys;
    bool                    close;
} SWindowData_Way;
