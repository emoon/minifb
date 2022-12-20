#include "vesa.h"
#include <dpmi.h>
#include <go32.h>
#include <stdio.h>
#include <string.h>
#include <sys/farptr.h>
#include <sys/movedata.h>
#include <sys/nearptr.h>

int vesa_mode = 0;

__dpmi_meminfo vesa_frame_buffer_mapping;

int vesa_frame_buffer_selector;

typedef struct vesa_info {
  unsigned char vesa_signature[4];
  unsigned short vesa_version __attribute__((packed));
  unsigned long oem_string_ptr __attribute__((packed));
  unsigned char capabilities[4];
  unsigned long video_mode_ptr __attribute__((packed));
  unsigned short total_memory __attribute__((packed));
  unsigned short oem_software_rev __attribute__((packed));
  unsigned long oem_vendor_name_ptr __attribute__((packed));
  unsigned long oem_product_name_ptr __attribute__((packed));
  unsigned long oem_product_rev_ptr __attribute__((packed));
  unsigned char reserved[222];
  unsigned char oem_data[256];
} vesa_info_t;

typedef struct mode_info {
  unsigned short mode_attributes __attribute__((packed));
  unsigned char win_a_attributes;
  unsigned char win_b_attributes;
  unsigned short win_granularity __attribute__((packed));
  unsigned short win_size __attribute__((packed));
  unsigned short win_a_segment __attribute__((packed));
  unsigned short win_b_segment __attribute__((packed));
  unsigned long win_func_ptr __attribute__((packed));
  unsigned short bytes_per_scanLine __attribute__((packed));
  unsigned short width __attribute__((packed));
  unsigned short height __attribute__((packed));
  unsigned char x_char_size;
  unsigned char y_char_size;
  unsigned char number_of_planes;
  unsigned char bits_per_pixel;
  unsigned char number_of_banks;
  unsigned char memory_model;
  unsigned char bank_size;
  unsigned char number_of_image_pages;
  unsigned char reserved_page;
  unsigned char red_mask_size;
  unsigned char red_mask_pos;
  unsigned char green_mask_size;
  unsigned char green_mask_pos;
  unsigned char blue_mask_size;
  unsigned char blue_mask_pos;
  unsigned char reserved_mask_size;
  unsigned char reserved_mask_pos;
  unsigned char direct_color_mode_info;
  unsigned long physical_base_ptr __attribute__((packed));
  unsigned long off_screen_mem_offset __attribute__((packed));
  unsigned short offscreen_mem_size __attribute__((packed));
  unsigned char reserved[206];
} mode_info_t;

static bool get_info(vesa_info_t *vesa_info) {
  ;
  long dosbuf = __tb & 0xFFFFF;
  for (size_t i = 0; i < sizeof(vesa_info_t); i++)
    _farpokeb(_dos_ds, dosbuf + i, 0);

  dosmemput("VBE2", 4, dosbuf);
  __dpmi_regs regs;
  regs.x.ax = 0x4F00;
  regs.x.di = dosbuf & 0xF;
  regs.x.es = (dosbuf >> 4) & 0xFFFF;
  __dpmi_int(0x10, &regs);
  if (regs.h.ah)
    return false;

  dosmemget(dosbuf, sizeof(vesa_info_t), vesa_info);
  if (strncmp((const char *)vesa_info->vesa_signature, "VESA", 4) != 0)
    return false;

  return true;
}

static bool get_mode_info(int mode, mode_info_t *info) {
  long dosbuf = __tb & 0xFFFFF;
  for (size_t i = 0; i < sizeof(mode_info_t); i++)
    _farpokeb(_dos_ds, dosbuf + i, 0);

  __dpmi_regs regs;
  regs.x.ax = 0x4F01;
  regs.x.di = dosbuf & 0xF;
  regs.x.es = (dosbuf >> 4) & 0xFFFF;
  regs.x.cx = mode;
  __dpmi_int(0x10, &regs);
  if (regs.h.ah)
    return false;

  dosmemget(dosbuf, sizeof(mode_info_t), info);
  return true;
}

static bool set_vesa_mode(int mode_number) {
  if (!mode_number)
    return false;

  __dpmi_regs regs;
  regs.x.ax = 0x4F02;
  regs.x.bx = mode_number;
  __dpmi_int(0x10, &regs);
  if (regs.h.ah)
    return false;

  return true;
}

void set_vga_mode(int mode) {
  __dpmi_regs regs;
  regs.x.ax = mode;
  __dpmi_int(0x10, &regs);
}

bool vesa_init(uint32_t width, uint32_t height, uint32_t *actual_width,
               uint32_t *actual_height) {
  if (vesa_mode != 0) {
    vesa_dispose();
  }

  vesa_info_t vesa_info = {0};
  if (!get_info(&vesa_info))
    return false;

  int mode_list[256];
  int number_of_modes = 0;
  unsigned long mode_ptr = ((vesa_info.video_mode_ptr & 0xFFFF0000) >> 12) +
                           (vesa_info.video_mode_ptr & 0xFFFF);
  while (_farpeekw(_dos_ds, mode_ptr) != 0xFFFF) {
    mode_list[number_of_modes] = _farpeekw(_dos_ds, mode_ptr);
    number_of_modes++;
    mode_ptr += 2;
  }

  int found_mode = 0;
  mode_info_t mode_info = {0};
  for (int i = 0; i < number_of_modes; i++) {
    if (!get_mode_info(mode_list[i], &mode_info)) {
      printf("Couldn't get mode info: %i\n", i);
      continue;
    }

    if (!(mode_info.width == width || mode_info.width == width * 2))
      continue;
    if (!(mode_info.height == height || mode_info.height == height * 2))
      continue;
    if ((mode_info.bits_per_pixel != 32))
      continue;
    if ((mode_info.memory_model != 6))
      continue;
    if (!(mode_info.mode_attributes & (1 << 7)))
      continue;

    // printf("mode: %i, res: %ix%i bpp: %i, mem: %i, planes: %i, bps: %i\n",
    // mode_list[i], mode_info.width, mode_info.height,
    // mode_info.bits_per_pixel, mode_info.memory_model,
    // mode_info.number_of_planes, mode_info.bytes_per_scanLine);

    found_mode = mode_list[i];
    if (mode_info.width == width && mode_info.height == height)
      break;
  }

  if (!found_mode) {
    printf("Couldn't find fitting mode for %ix%i.\n", (int)width, (int)height);
    return false;
  }

  vesa_frame_buffer_mapping.address = mode_info.physical_base_ptr;
  vesa_frame_buffer_mapping.size = vesa_info.total_memory << 16;
  if (__dpmi_physical_address_mapping(&vesa_frame_buffer_mapping) != 0) {
    printf("Couldn't create VESA frame buffer address mapping.\n");
    return false;
  }

  vesa_frame_buffer_selector = __dpmi_allocate_ldt_descriptors(1);
  if (vesa_frame_buffer_selector < 0) {
    printf("Couldn't create VESA frame buffer selector.\n");
    __dpmi_free_physical_address_mapping(&vesa_frame_buffer_mapping);
    return false;
  }

  __dpmi_set_segment_base_address(vesa_frame_buffer_selector,
                                  vesa_frame_buffer_mapping.address);
  __dpmi_set_segment_limit(vesa_frame_buffer_selector,
                           vesa_frame_buffer_mapping.size - 1);

  if (!set_vesa_mode(found_mode | 0x4000)) {
    printf("Couldn't set VESA mode.\n");
    __dpmi_free_physical_address_mapping(&vesa_frame_buffer_mapping);
    __dpmi_free_ldt_descriptor(vesa_frame_buffer_selector);
    return false;
  }
  vesa_mode = found_mode;
  *actual_width = mode_info.width;
  *actual_height = mode_info.height;
  return true;
}

int vesa_get_frame_buffer_selector() { return vesa_frame_buffer_selector; }

void vesa_dispose() {
  if (!vesa_mode)
    return;
  __dpmi_free_physical_address_mapping(&vesa_frame_buffer_mapping);
  __dpmi_free_ldt_descriptor(vesa_frame_buffer_selector);
  vesa_mode = 0;
  set_vga_mode(0x3);
}