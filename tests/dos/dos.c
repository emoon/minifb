#include "MiniFB_enums.h"
#include <MiniFB.h>
#include <stdlib.h>
#include <string.h>
#define GDB_IMPLEMENTATION
#include "gdbstub.h"

int main(void) {
  int res_x = 640;
  int res_y = 480;
  int buf_x = 320;
  int buf_y = 240;
  gdb_start();
  uint32_t *pixels = (uint32_t *)malloc(sizeof(uint32_t) * buf_x * buf_y);
  memset(pixels, NULL, buf_x * buf_y * 4);
  struct mfb_window *window =
      mfb_open_ex("Noise Test", res_x, res_y, WF_RESIZABLE);

  do {
    for (int i = 0; i < 2000; i++) {
      int x = rand() % buf_x;
      int y = rand() % buf_y;
      int color = MFB_RGB(rand() % 0xff, rand() % 0xff, rand() % 0xff);
      pixels[x + y * buf_x] = color;
    }

    if (mfb_get_mouse_button_buffer(window)[MOUSE_LEFT]) {
      int32_t x = mfb_get_mouse_x(window);
      int32_t y = mfb_get_mouse_y(window);
      x = (int)(((float)x / res_x) * buf_x);
      y = (int)(((float)y / res_y) * buf_y);
      x = x >= buf_x ? buf_x - 1 : x;
      x = x < 0 ? 0 : x;
      y = y >= buf_y ? buf_y - 1 : y;
      y = y < 0 ? 0 : y;
      uint8_t *dst = (uint8_t *)pixels;
      while (y >= 0) {
        memset(dst, 0, x * 4);
        dst += buf_x * 4;
        y--;
      }
    }

    if (mfb_update_ex(window, pixels, buf_x, buf_y) != STATE_OK) {
      break;
    }

    gdb_checkpoint();
  } while (mfb_wait_sync(window));
  return 0;
}