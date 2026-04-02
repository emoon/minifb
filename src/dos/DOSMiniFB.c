#include "MiniFB.h"
#include "MiniFB_enums.h"
#include "MiniFB_internal.h"
#include "WindowData.h"

#include <dpmi.h>
#include <pc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/movedata.h>
#include <sys/segments.h>
#include <time.h>

#include "vesa.h"

//-------------------------------------
static const uint32_t scancode_to_mfb_key[] = {
    MFB_KB_KEY_UNKNOWN,
    MFB_KB_KEY_ESCAPE,
    MFB_KB_KEY_1,
    MFB_KB_KEY_2,
    MFB_KB_KEY_3,
    MFB_KB_KEY_4,
    MFB_KB_KEY_5,
    MFB_KB_KEY_6,
    MFB_KB_KEY_7,
    MFB_KB_KEY_8,
    MFB_KB_KEY_9,
    MFB_KB_KEY_0,
    MFB_KB_KEY_MINUS,
    MFB_KB_KEY_EQUAL,
    MFB_KB_KEY_BACKSPACE,
    MFB_KB_KEY_TAB,
    MFB_KB_KEY_Q,
    MFB_KB_KEY_W,
    MFB_KB_KEY_E,
    MFB_KB_KEY_R,
    MFB_KB_KEY_T,
    MFB_KB_KEY_Y,
    MFB_KB_KEY_U,
    MFB_KB_KEY_I,
    MFB_KB_KEY_O,
    MFB_KB_KEY_P,
    MFB_KB_KEY_LEFT_BRACKET,
    MFB_KB_KEY_RIGHT_BRACKET,
    MFB_KB_KEY_ENTER,
    MFB_KB_KEY_LEFT_CONTROL,
    MFB_KB_KEY_A,
    MFB_KB_KEY_S,
    MFB_KB_KEY_D,
    MFB_KB_KEY_F,
    MFB_KB_KEY_G,
    MFB_KB_KEY_H,
    MFB_KB_KEY_J,
    MFB_KB_KEY_K,
    MFB_KB_KEY_L,
    MFB_KB_KEY_SEMICOLON,
    MFB_KB_KEY_APOSTROPHE,
    MFB_KB_KEY_GRAVE_ACCENT,
    MFB_KB_KEY_LEFT_SHIFT,
    MFB_KB_KEY_BACKSLASH,
    MFB_KB_KEY_Z,
    MFB_KB_KEY_X,
    MFB_KB_KEY_C,
    MFB_KB_KEY_V,
    MFB_KB_KEY_B,
    MFB_KB_KEY_N,
    MFB_KB_KEY_M,
    MFB_KB_KEY_COMMA,
    MFB_KB_KEY_PERIOD,
    MFB_KB_KEY_SLASH,
    MFB_KB_KEY_RIGHT_SHIFT,
    MFB_KB_KEY_KP_MULTIPLY,   // 0x37 (numpad *); real Print Screen sends E0+0x37
    MFB_KB_KEY_LEFT_ALT,
    MFB_KB_KEY_SPACE,
    MFB_KB_KEY_CAPS_LOCK,
    MFB_KB_KEY_F1,
    MFB_KB_KEY_F2,
    MFB_KB_KEY_F3,
    MFB_KB_KEY_F4,
    MFB_KB_KEY_F5,
    MFB_KB_KEY_F6,
    MFB_KB_KEY_F7,
    MFB_KB_KEY_F8,
    MFB_KB_KEY_F9,
    MFB_KB_KEY_F10,
    MFB_KB_KEY_NUM_LOCK,
    MFB_KB_KEY_SCROLL_LOCK,
    MFB_KB_KEY_HOME,
    MFB_KB_KEY_UP,
    MFB_KB_KEY_PAGE_UP,
    MFB_KB_KEY_KP_SUBTRACT,   // 0x4A (numpad -)
    MFB_KB_KEY_LEFT,
    MFB_KB_KEY_KP_5,          // 0x4C (numpad 5, center)
    MFB_KB_KEY_RIGHT,
    MFB_KB_KEY_KP_ADD,
    MFB_KB_KEY_END,
    MFB_KB_KEY_DOWN,
    MFB_KB_KEY_PAGE_DOWN,
    MFB_KB_KEY_INSERT,
    MFB_KB_KEY_DELETE,
    MFB_KB_KEY_UNKNOWN,       // 0x54 (SysRq / Alt+PrScr)
    MFB_KB_KEY_UNKNOWN,       // 0x55 (undefined)
    MFB_KB_KEY_UNKNOWN,       // 0x56 (ISO extra key between LShift and Z)
    MFB_KB_KEY_F11,           // 0x57
    MFB_KB_KEY_F12,           // 0x58
};

//-------------------------------------
static const char scancode_to_ascii[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9',  '0', '-', '=',  0,
    0,   'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',  '[', ']', 0,    0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z',
    'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   0,    0,   ' ',
};

//-------------------------------------
static const char scancode_to_ascii_shift[] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
    0,   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,   0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z',
    'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   0,   0,   ' ',
};

//-------------------------------------
#define RING_BUFFER_SIZE 512

//-------------------------------------
typedef struct ring_buffer {
  uint32_t read_index;
  uint32_t write_index;
  uint8_t buffer[RING_BUFFER_SIZE];
} ring_buffer;

//-------------------------------------
static bool
ring_buffer_push(ring_buffer *buffer, uint8_t value) {
  uint32_t next = buffer->write_index + 1;
  if (next >= RING_BUFFER_SIZE)
    next = 0;

  if (next == buffer->read_index)
    return false;

  buffer->buffer[buffer->write_index] = value;
  buffer->write_index = next;

  return true;
}

//-------------------------------------
static bool
ring_buffer_pop(ring_buffer *buffer, uint8_t *value) {
  if (buffer->write_index == buffer->read_index)
    return false;

  uint32_t next = buffer->read_index + 1;
  if (next >= RING_BUFFER_SIZE)
    next = 0;

  *value = buffer->buffer[buffer->read_index];
  buffer->read_index = next;

  return true;
}

//-------------------------------------
typedef struct keyboard_state {
  bool initialized;
  ring_buffer buffer;
  _go32_dpmi_seginfo old_keyboard_handler;
  _go32_dpmi_seginfo new_keyboard_handler;
  uint8_t last_scancode_was_extended; // 0=no, 1=E0, 2=E1, 3=E2
  bool caps_lock;
} keyboard_state;

//-------------------------------------
static keyboard_state g_keyboard = {0};

//-------------------------------------
typedef struct SWindowData_DOS {
  uint32_t actual_width, actual_height, actual_bpp, bytes_per_scanline;
  uint32_t *scale_buffer;
  uint8_t *scanline_buffer;
} SWindowData_DOS;

//-------------------------------------
static SWindowData *g_window = NULL;
static bool g_mouse_present = false;

//-------------------------------------
__attribute__((destructor))
static void
tear_down() {
  vesa_dispose();
  if (g_keyboard.initialized) {
    _go32_dpmi_set_protected_mode_interrupt_vector(
        0x9, &g_keyboard.old_keyboard_handler);
    _go32_dpmi_free_iret_wrapper(&g_keyboard.new_keyboard_handler);
  }
}

//-------------------------------------
static void
init_mouse(SWindowData *window_data) {
  __dpmi_regs regs;

  // AX=0: reset driver and check presence; returns AX=0xFFFF if driver present
  regs.x.ax = 0;
  __dpmi_int(0x33, &regs);
  if (regs.x.ax == 0) {
    MFB_LOG(MFB_LOG_WARNING, "No mouse driver detected, mouse support disabled");
    return;
  }
  g_mouse_present = true;

  // Use the actual VESA resolution for the mouse range, not the user-requested
  // window size, so the cursor covers the full screen area.
  SWindowData_DOS *window_data_specific = (SWindowData_DOS *) window_data->specific;
  uint32_t width  = window_data_specific ? window_data_specific->actual_width  : window_data->window_width;
  uint32_t height = window_data_specific ? window_data_specific->actual_height : window_data->window_height;

  regs.x.ax = 7;
  regs.x.cx = 0;
  regs.x.dx = width - 1;
  __dpmi_int(0x33, &regs);

  regs.x.ax = 8;
  regs.x.cx = 0;
  regs.x.dx = height - 1;
  __dpmi_int(0x33, &regs);

  regs.x.ax = 2;
  __dpmi_int(0x33, &regs);
}

//-------------------------------------
static void
keyboard_handler() {
  ring_buffer_push(&g_keyboard.buffer, inp(0x60));
  outportb(0x20, 0x20);
}

//-------------------------------------
static void
init_keyboard(void) {
  if (g_keyboard.initialized)
    return;

  _go32_dpmi_lock_data(&g_keyboard, sizeof(g_keyboard));
  _go32_dpmi_lock_code(keyboard_handler, 4096);
  _go32_dpmi_lock_code(ring_buffer_push, 4096);

  _go32_dpmi_get_protected_mode_interrupt_vector(
      0x9, &g_keyboard.old_keyboard_handler);

  g_keyboard.new_keyboard_handler.pm_offset = (int) keyboard_handler;
  g_keyboard.new_keyboard_handler.pm_selector = _my_cs();

  _go32_dpmi_allocate_iret_wrapper(&g_keyboard.new_keyboard_handler);
  _go32_dpmi_set_protected_mode_interrupt_vector(
      0x9, &g_keyboard.new_keyboard_handler);

  g_keyboard.initialized = true;
}

//-------------------------------------
static void
destroy_window_data(SWindowData *window_data) {
  if (window_data == NULL)
    return;

  release_cpp_stub((struct mfb_window *) window_data);

  g_window = NULL;
  vesa_dispose();

  SWindowData_DOS *window_data_specific = (SWindowData_DOS *) window_data->specific;
  if (window_data_specific != NULL) {
    free(window_data_specific->scale_buffer);
    free(window_data_specific->scanline_buffer);
    memset(window_data_specific, 0, sizeof(SWindowData_DOS));
    free(window_data_specific);
  }

  window_data->specific = NULL;
  memset(window_data, 0, sizeof(SWindowData));
  free(window_data);
}

//-------------------------------------
struct mfb_window *
mfb_open_ex(const char *title, unsigned width, unsigned height, unsigned flags) {
  (void) title;
  uint32_t buffer_stride = 0;

  if (!calculate_buffer_layout(width, height, &buffer_stride, NULL)) {
    MFB_LOG(MFB_LOG_ERROR, "DOSMiniFB: invalid window size %ux%u", width, height);
    return NULL;
  }

  if (flags != 0u) {
    MFB_LOG(MFB_LOG_WARNING, "DOSMiniFB: window flags 0x%x are ignored by the DOS backend", flags);
  }

  if (g_window) {
    MFB_LOG(MFB_LOG_WARNING, "mfb_open_ex called while DOS backend window is already open");
    return NULL;
  }

  uint32_t actual_width, actual_height, actual_bpp, bytes_per_scanline;
  if (!vesa_init(width, height, &actual_width, &actual_height, &actual_bpp, &bytes_per_scanline)) {
    MFB_LOG(MFB_LOG_ERROR, "Couldn't set VESA mode %ux%u", width, height);
    return NULL;
  }

  SWindowData *window_data;

  window_data = malloc(sizeof(SWindowData));
  if (window_data == NULL) {
    MFB_LOG(MFB_LOG_ERROR, "Cannot allocate window data");
    vesa_dispose();
    return NULL;
  }
  memset(window_data, 0, sizeof(SWindowData));
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

  SWindowData_DOS *window_data_specific = malloc(sizeof(SWindowData_DOS));
  if (!window_data_specific) {
    MFB_LOG(MFB_LOG_ERROR, "Cannot allocate DOS window data");
    free(window_data);
    vesa_dispose();
    return NULL;
  }
  window_data_specific->actual_width = actual_width;
  window_data_specific->actual_height = actual_height;
  window_data_specific->actual_bpp = actual_bpp;
  window_data_specific->bytes_per_scanline = bytes_per_scanline;
  window_data_specific->scale_buffer = (uint32_t *) malloc((size_t) actual_width * actual_height * sizeof(uint32_t));
  window_data_specific->scanline_buffer =
      actual_bpp != 32 || bytes_per_scanline != width << 2
          ? (uint8_t *) malloc(actual_height * bytes_per_scanline)
          : NULL;

  if (!window_data_specific->scale_buffer) {
    MFB_LOG(MFB_LOG_ERROR, "Cannot allocate DOS scale buffer");
    free(window_data_specific);
    free(window_data);
    vesa_dispose();
    return NULL;
  }

  if ((actual_bpp != 32 || bytes_per_scanline != width << 2) && !window_data_specific->scanline_buffer) {
    MFB_LOG(MFB_LOG_ERROR, "Cannot allocate DOS scanline buffer");
    free(window_data_specific->scale_buffer);
    free(window_data_specific);
    free(window_data);
    vesa_dispose();
    return NULL;
  }

  window_data->specific = window_data_specific;

  mfb_set_keyboard_callback((struct mfb_window *) window_data, keyboard_default);

  window_data->is_active = true;
  window_data->is_initialized = true;
  window_data->is_cursor_visible = false;

  g_window = window_data;

  init_mouse(window_data);
  init_keyboard();

  // Sync Caps Lock initial state from BIOS (INT 16h AH=02h: read shift flags, bit 6 = Caps Lock)
  {
    __dpmi_regs regs;
    regs.x.ax = 0x0200;
    __dpmi_int(0x16, &regs);
    if (regs.h.al & 0x40) {
      g_keyboard.caps_lock = true;
      window_data->mod_keys |= MFB_KB_MOD_CAPS_LOCK;
    }
  }

  MFB_LOG(MFB_LOG_DEBUG,
          "DOS window created (%ux%u, actual %ux%u, bpp=%u, pitch=%u)", width,
          height, actual_width, actual_height, actual_bpp, bytes_per_scanline);

  return (struct mfb_window *) window_data;
}

//-------------------------------------
bool
mfb_set_viewport(struct mfb_window *window, unsigned offset_x, unsigned offset_y, unsigned width, unsigned height) {
  SWindowData *window_data = (SWindowData *) window;
  if (!mfb_validate_viewport(window_data, offset_x, offset_y, width, height, "DOSMiniFB")) {
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
static void
update_mouse(SWindowData *window_data) {
  if (!g_mouse_present)
    return;

  __dpmi_regs regs;
  regs.x.ax = 0x3;
  __dpmi_int(0x33, &regs);
  int32_t old_x = window_data->mouse_pos_x;
  int32_t old_y = window_data->mouse_pos_y;
  uint8_t old_left_pressed   = window_data->mouse_button_status[MFB_MOUSE_LEFT];
  uint8_t old_right_pressed  = window_data->mouse_button_status[MFB_MOUSE_RIGHT];
  uint8_t old_middle_pressed = window_data->mouse_button_status[MFB_MOUSE_MIDDLE];
  uint8_t left_pressed   = (regs.x.bx >> 0) & 1;
  uint8_t right_pressed  = (regs.x.bx >> 1) & 1;
  uint8_t middle_pressed = (regs.x.bx >> 2) & 1;
  window_data->mouse_button_status[MFB_MOUSE_LEFT]   = left_pressed;
  window_data->mouse_button_status[MFB_MOUSE_RIGHT]  = right_pressed;
  window_data->mouse_button_status[MFB_MOUSE_MIDDLE] = middle_pressed;
  window_data->mouse_pos_x = regs.x.cx;
  window_data->mouse_pos_y = regs.x.dx;

  mfb_key_mod mod = (mfb_key_mod) window_data->mod_keys;

  if (old_left_pressed != left_pressed && window_data->mouse_btn_func)
    window_data->mouse_btn_func((struct mfb_window *) window_data, MFB_MOUSE_LEFT,   mod, left_pressed);

  if (old_right_pressed != right_pressed && window_data->mouse_btn_func)
    window_data->mouse_btn_func((struct mfb_window *) window_data, MFB_MOUSE_RIGHT,  mod, right_pressed);

  if (old_middle_pressed != middle_pressed && window_data->mouse_btn_func)
    window_data->mouse_btn_func((struct mfb_window *) window_data, MFB_MOUSE_MIDDLE, mod, middle_pressed);

  if ((old_x != regs.x.cx || old_y != regs.x.dx) && window_data->mouse_move_func)
    window_data->mouse_move_func((struct mfb_window *) window_data, regs.x.cx, regs.x.dx);
}

//-------------------------------------
static void
update_keyboard(SWindowData *window_data) {
  uint8_t raw_scancode;

  while (ring_buffer_pop(&g_keyboard.buffer, &raw_scancode)) {
    if (raw_scancode == 0xe0 || raw_scancode == 0xe1 || raw_scancode == 0xe2) {
      g_keyboard.last_scancode_was_extended = raw_scancode + 1 - 0xe0;
      continue;
    }

    uint8_t scancode = raw_scancode & 0x7f;
    if (scancode >= sizeof(scancode_to_mfb_key) / sizeof(scancode_to_mfb_key[0]))
      continue;

    bool pressed = !(raw_scancode & 0x80);
    uint32_t key_code = scancode_to_mfb_key[scancode];
    bool is_extended = g_keyboard.last_scancode_was_extended != 0;

    // Some DOS mouse drivers emulate wheel by injecting extended Up/Down keys.
    // Translate those to mouse wheel callbacks to avoid spurious keyboard events.
    if (is_extended && (key_code == MFB_KB_KEY_UP || key_code == MFB_KB_KEY_DOWN)) {
      if (pressed) {
        float delta_y = (key_code == MFB_KB_KEY_UP) ? 1.0f : -1.0f;
        window_data->mouse_wheel_x = 0.0f;
        window_data->mouse_wheel_y = delta_y;
        if (window_data->mouse_wheel_func) {
          window_data->mouse_wheel_func((struct mfb_window *) window_data,
                                        (mfb_key_mod) window_data->mod_keys,
                                        0.0f,
                                        delta_y
          );
        }
      }
      g_keyboard.last_scancode_was_extended = 0;
      continue;
    }

    char ascii = 0;
    if (scancode < sizeof(scancode_to_ascii)) {
      char base_ascii = scancode_to_ascii[scancode];
      bool is_letter  = (base_ascii >= 'a' && base_ascii <= 'z');
      // Caps Lock toggles shift only for letter keys; Shift always applies to all keys
      bool use_shift  = (window_data->mod_keys & MFB_KB_MOD_SHIFT) != 0;
      if ((window_data->mod_keys & MFB_KB_MOD_CAPS_LOCK) && is_letter)
        use_shift = !use_shift;
      ascii = use_shift ? scancode_to_ascii_shift[scancode] : base_ascii;
    }

    //MFB_LOG(MFB_LOG_TRACE, "scancode=%u key=%s ascii=%u pressed=%u",
    //        (unsigned) scancode, mfb_get_key_name((mfb_key) key_code),
    //        (unsigned)(uint8_t) ascii, (unsigned) pressed);

    // MFB_KB_KEY_UNKNOWN == -1 -> as uint32_t it becomes 0xFFFFFFFF, which would
    // overflow key_status[MFB_MAX_KEYS]. Guard the write with a bounds check.
    if (key_code < MFB_MAX_KEYS)
      window_data->key_status[key_code] = pressed;

    if (key_code == MFB_KB_KEY_LEFT_SHIFT || key_code == MFB_KB_KEY_RIGHT_SHIFT) {
      if (pressed)
        window_data->mod_keys |= MFB_KB_MOD_SHIFT;
      else
        window_data->mod_keys &= ~MFB_KB_MOD_SHIFT;
    }

    if (key_code == MFB_KB_KEY_LEFT_ALT || key_code == MFB_KB_KEY_RIGHT_ALT) {
      if (pressed)
        window_data->mod_keys |= MFB_KB_MOD_ALT;
      else
        window_data->mod_keys &= ~MFB_KB_MOD_ALT;
    }

    if (key_code == MFB_KB_KEY_LEFT_CONTROL || key_code == MFB_KB_KEY_RIGHT_CONTROL) {
      if (pressed)
        window_data->mod_keys |= MFB_KB_MOD_CONTROL;
      else
        window_data->mod_keys &= ~MFB_KB_MOD_CONTROL;
    }

    if (key_code == MFB_KB_KEY_CAPS_LOCK && !pressed) {
      g_keyboard.caps_lock = !g_keyboard.caps_lock;
      if (g_keyboard.caps_lock)
        window_data->mod_keys |= MFB_KB_MOD_CAPS_LOCK;
      else
        window_data->mod_keys &= ~MFB_KB_MOD_CAPS_LOCK;
    }

    if (window_data->keyboard_func)
      window_data->keyboard_func((struct mfb_window *) window_data,
                                 key_code,
                                 window_data->mod_keys,
                                 pressed);

    if (window_data->char_input_func && pressed && ascii != 0)
      window_data->char_input_func((struct mfb_window *) window_data, ascii);

    // FIXME we currently ignore extended keys
    g_keyboard.last_scancode_was_extended = 0;
  }
}

//-------------------------------------
mfb_update_state
mfb_update_events(struct mfb_window *window) {
  if (!window) {
    MFB_LOG(MFB_LOG_DEBUG, "mfb_update_events: invalid window");
    return MFB_STATE_INVALID_WINDOW;
  }

  SWindowData *window_data = (SWindowData *) window;
  if (window_data->close) {
    destroy_window_data(window_data);
    return MFB_STATE_EXIT;
  }

  window_data->mouse_wheel_x = 0.0f;
  window_data->mouse_wheel_y = 0.0f;

  update_mouse(window_data);
  update_keyboard(window_data);

  if (window_data->close) {
    destroy_window_data(window_data);
    return MFB_STATE_EXIT;
  }

  return MFB_STATE_OK;
}

//-------------------------------------
//static void
//convert_to_24_bpp(uint32_t *source, uint32_t width, uint32_t height) {}

//-------------------------------------
mfb_update_state
mfb_update_ex(struct mfb_window *window, void *buffer, unsigned width, unsigned height) {
  uint32_t buffer_stride = 0;

  if (!window) {
    MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: invalid window");
    return MFB_STATE_INVALID_WINDOW;
  }

  if (!buffer) {
    MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: invalid buffer");
    return MFB_STATE_INVALID_BUFFER;
  }

  if (!calculate_buffer_layout(width, height, &buffer_stride, NULL)) {
    MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: invalid buffer size %ux%u", width, height);
    return MFB_STATE_INVALID_BUFFER;
  }

  SWindowData *window_data = (SWindowData *) window;
  mfb_update_state state = mfb_update_events(window);
  if (state)
    return state;

  SWindowData_DOS *window_data_specific = window_data->specific;
  if (!window_data_specific) {
    MFB_LOG(MFB_LOG_DEBUG, "mfb_update_ex: invalid window specific data");
    return MFB_STATE_INVALID_WINDOW;
  }

  window_data->buffer_width  = width;
  window_data->buffer_height = height;
  window_data->buffer_stride = buffer_stride;

  uint32_t *scale_buffer        = window_data_specific->scale_buffer;
  uint8_t  *scanline_buffer     = window_data_specific->scanline_buffer;
  uint32_t a_width              = window_data_specific->actual_width;
  uint32_t a_height             = window_data_specific->actual_height;
  uint32_t a_bpp                = window_data_specific->actual_bpp;
  uint32_t a_bytes_per_scanline = window_data_specific->bytes_per_scanline;
  bool has_viewport             = window_data->dst_offset_x != 0 || window_data->dst_offset_y != 0 ||
                                  window_data->dst_width != window_data->window_width ||
                                  window_data->dst_height != window_data->window_height;

  // Exact match, copy directly. Unlikely to happen on a real device.
  // Happens in DOSBox.
  if (!has_viewport &&
      a_width == width && a_height == height && a_bpp == 32 &&
      a_bytes_per_scanline == a_width * sizeof(uint32_t)) {
    movedata(_my_ds(),
             (unsigned int) buffer,
             vesa_get_frame_buffer_selector(),
             0,
             height * a_bytes_per_scanline
    );

    return MFB_STATE_OK;
  }

  // Else we need to transfer to scale, convert to 24-bit, pad the scanlines,
  // or all of the above...
  uint8_t *frame = NULL;
  if (has_viewport) {
    memset(scale_buffer, 0, a_width * a_height * sizeof(uint32_t));

    uint32_t vp_x = (uint32_t)(((uint64_t) window_data->dst_offset_x * a_width) /
                               window_data->window_width);
    uint32_t vp_y = (uint32_t)(((uint64_t) window_data->dst_offset_y * a_height) /
                               window_data->window_height);
    uint32_t vp_w = (uint32_t)(((uint64_t) window_data->dst_width * a_width) /
                               window_data->window_width);
    uint32_t vp_h = (uint32_t)(((uint64_t) window_data->dst_height * a_height) /
                               window_data->window_height);

    if (vp_w == 0)
      vp_w = 1;

    if (vp_h == 0)
      vp_h = 1;

    if (vp_x < a_width && vp_y < a_height) {
      if (vp_x + vp_w > a_width)
        vp_w = a_width - vp_x;
      if (vp_y + vp_h > a_height)
        vp_h = a_height - vp_y;

      stretch_image(buffer, 0, 0, width, height, width, scale_buffer, vp_x,
                    vp_y, vp_w, vp_h, a_width);
    }

    frame = (uint8_t *) scale_buffer;
  }

  else if (a_width != width || a_height != height) {
    stretch_image(buffer,       0, 0, width,   height,   width,
                  scale_buffer, 0, 0, a_width, a_height, a_width);
    frame = (uint8_t *) scale_buffer;
  }

  else {
    frame = (uint8_t *) buffer;
  }

  if (a_bpp == 24) {
    // Neither pitch nor bpp matched, worst case
    uint8_t *source = frame;
    uint8_t *dest = scanline_buffer;
    uint32_t dest_skip = a_bytes_per_scanline - a_width * 3;

    for (uint32_t y = 0, i = 0; y < a_height; y++) {
      for (uint32_t x = 0, xe = a_width; x < xe; x++, i += 4, dest += 3) {
        dest[0] = source[i];
        dest[1] = source[i + 1];
        dest[2] = source[i + 2];
      }
      dest += dest_skip;
    }

    movedata(_my_ds(),
             (unsigned int) scanline_buffer,
             vesa_get_frame_buffer_selector(),
             0,
             a_height * a_bytes_per_scanline
    );
  }

  else {
    if (a_bytes_per_scanline != a_width * 4) {
      // bpp matched, but pitch didn't. very unlikely to happen...
      uint8_t *source = (uint8_t *) frame;
      uint8_t *dest = (uint8_t *) scanline_buffer;
      uint32_t source_pitch = a_width << 2;
      for (uint32_t y = 0; y < a_height; y++, dest += a_bytes_per_scanline, source += source_pitch) {
        memcpy(dest, source, source_pitch);
      }

      movedata(_my_ds(),
               (unsigned int) scanline_buffer,
               vesa_get_frame_buffer_selector(),
               0,
               a_height * a_bytes_per_scanline
      );
    }
    else {
      // Only stretched
      movedata(_my_ds(),
               (unsigned int) frame,
               vesa_get_frame_buffer_selector(),
               0,
               a_height * a_width * sizeof(uint32_t)
      );
    }
  }

  return MFB_STATE_OK;
}

//-------------------------------------
bool
mfb_wait_sync(struct mfb_window *window) {
  if (!window) {
    MFB_LOG(MFB_LOG_DEBUG, "mfb_wait_sync: invalid window");
    return false;
  }

  mfb_update_state state = mfb_update_events(window);
  return (state == MFB_STATE_OK);
}

//-------------------------------------
void
mfb_get_monitor_scale(struct mfb_window *window, float *scale_x, float *scale_y) {
  kUnused(window);

  if (scale_x)
    *scale_x = 1.0f;

  if (scale_y)
    *scale_y = 1.0f;
}

//-------------------------------------
extern double g_timer_frequency;
extern double g_timer_resolution;

//-------------------------------------
void
mfb_timer_init(void) {
  g_timer_frequency = UCLOCKS_PER_SEC;
  g_timer_resolution = 1.0 / g_timer_frequency;
}

//-------------------------------------
uint64_t
mfb_timer_tick(void) {
  return uclock();
}

//-------------------------------------
void
mfb_show_cursor(struct mfb_window *window, bool show) {
    (void) window;
    (void) show;
    // Hardware cursor not supported in VESA mode; is_cursor_visible is always false
}
