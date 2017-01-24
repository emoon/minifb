#include <MiniFB.h>

#include <wayland-client.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/input.h>

#include <sys/mman.h>

static struct wl
{
   struct wl_display *display;
   struct wl_registry *registry;
   struct wl_compositor *compositor;
   struct wl_shell *shell;
   struct wl_seat *seat;
   struct wl_keyboard *keyboard;
   struct wl_shm *shm;
   struct wl_shm_pool *shm_pool;
   struct wl_surface *surface;
   struct wl_shell_surface *shell_surface;

   uint32_t seat_version;
   uint32_t shm_format;
   uint32_t width;
   uint32_t height;
   uint32_t stride;
   uint32_t *shm_ptr;
   struct wl_buffer *buffer;
   int should_close;
} wl;

static void destroy(void)
{
   if (! wl.display)
      return;

#define KILL(NAME)                     \
   do                                  \
   {                                   \
      if (wl.NAME)                     \
         wl_##NAME##_destroy(wl.NAME); \
   } while (0)
   KILL(shell_surface);
   KILL(shell);
   KILL(surface);
   KILL(buffer);
   KILL(shm_pool);
   KILL(shm);
   KILL(compositor);
   KILL(keyboard);
   KILL(seat);
   KILL(registry);
#undef KILL
   wl_display_disconnect(wl.display);
   memset(&wl, 0, sizeof(wl));
}

static void nop() {}

#define NO_FUNC (void (*)()) nop

static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
                         uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state)
{
   if (state == WL_KEYBOARD_KEY_STATE_RELEASED && key == KEY_ESC)
   {
      wl.should_close = 1;
   }
}

static const struct wl_keyboard_listener keyboard_listener = {
   .keymap = NO_FUNC,
   .enter = NO_FUNC,
   .leave = NO_FUNC,
   .key = keyboard_key,
   .modifiers = NO_FUNC,
   .repeat_info = NO_FUNC,
};

static void seat_capabilities(void *data, struct wl_seat *seat,
                              enum wl_seat_capability caps)
{
   if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl.keyboard)
   {
      wl.keyboard = wl_seat_get_keyboard(seat);
      wl_keyboard_add_listener(wl.keyboard, &keyboard_listener, NULL);
   }
   else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl.keyboard)
   {
      wl_keyboard_destroy(wl.keyboard);
      wl.keyboard = NULL;
   }
}

static const struct wl_seat_listener seat_listener = {
   .capabilities = seat_capabilities, .name = NO_FUNC,
};

static void shm_format(void *data, struct wl_shm *shm, uint32_t format)
{
   if (wl.shm_format == -1u)
   {
      switch (format)
      {
      // We could do RGBA, but that would not be what is expected from minifb...
      /* case WL_SHM_FORMAT_ARGB8888: */
      case WL_SHM_FORMAT_XRGB8888:
         wl.shm_format = format;
         break;

      default:
         break;
      }
   }
}

static const struct wl_shm_listener shm_listener = {.format = shm_format};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t id, char const *iface, uint32_t version)
{
   if (strcmp(iface, "wl_compositor") == 0)
   {
      wl.compositor =
         wl_registry_bind(registry, id, &wl_compositor_interface, 1);
   }
   else if (strcmp(iface, "wl_shm") == 0)
   {
      wl.shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
      if (wl.shm)
         wl_shm_add_listener(wl.shm, &shm_listener, NULL);
   }
   else if (strcmp(iface, "wl_shell") == 0)
   {
      wl.shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
   }
   else if (strcmp(iface, "wl_seat") == 0)
   {
      wl.seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
      if (wl.seat)
      {
         wl_seat_add_listener(wl.seat, &seat_listener, NULL);
      }
   }
}

static const struct wl_registry_listener registry_listener = {
   .global = registry_global, .global_remove = NO_FUNC,
};

int mfb_open(const char *title, int width, int height)
{
   int fd = -1;

   wl.display = wl_display_connect(NULL);
   if (!wl.display)
      return -1;
   wl.registry = wl_display_get_registry(wl.display);
   wl_registry_add_listener(wl.registry, &registry_listener, NULL);
   if (wl_display_roundtrip(wl.display) == -1 ||
       wl_display_roundtrip(wl.display) == -1)
   {
      return -1;
   }

   // did not get a format we want... meh
   if (wl.shm_format == -1)
      goto out;
   if (!wl.compositor)
      goto out;

   char const *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");
   char shmfile[PATH_MAX];
   int ret = snprintf(shmfile, sizeof(shmfile), "%s/WaylandMiniFB-SHM-XXXXXX",
                      xdg_rt_dir);
   if (ret >= sizeof(shmfile))
      goto out;

   fd = mkstemp(shmfile);
   if (fd == -1)
      goto out;
   unlink(shmfile);

   uint32_t length = sizeof(uint32_t) * width * height;

   if (ftruncate(fd, length) == -1)
      goto out;

   wl.shm_ptr = mmap(NULL, length, PROT_WRITE, MAP_SHARED, fd, 0);
   if (wl.shm_ptr == MAP_FAILED)
      goto out;

   wl.width = width;
   wl.height = height;
   wl.stride = width * sizeof(uint32_t);
   wl.shm_pool = wl_shm_create_pool(wl.shm, fd, length);
   wl.buffer = wl_shm_pool_create_buffer(wl.shm_pool, 0, wl.width, wl.height,
                                         wl.stride, wl.shm_format);

   close(fd);
   fd = -1;

   wl.surface = wl_compositor_create_surface(wl.compositor);
   if (!wl.surface)
      goto out;

   // There should always be a shell, right?
   if (wl.shell)
   {
      wl.shell_surface = wl_shell_get_shell_surface(wl.shell, wl.surface);
      if (!wl.shell_surface)
         goto out;

      wl_shell_surface_set_title(wl.shell_surface, title);
      wl_shell_surface_set_toplevel(wl.shell_surface);
   }

   wl_surface_attach(wl.surface, wl.buffer, 0, 0);
   wl_surface_damage(wl.surface, 0, 0, width, height);
   wl_surface_commit(wl.surface);

   return 1;

out:
   close(fd);
   destroy();
   return 0;
}

static void frame_done(void *data, struct wl_callback *callback,
                       uint32_t cookie)
{
   wl_callback_destroy(callback);
   *(uint32_t *)data = 1;
}

static const struct wl_callback_listener frame_listener = {
   .done = frame_done,
};

int mfb_update(void *buffer)
{
   uint32_t done = 0;

   if (!wl.display || wl_display_get_error(wl.display) != 0)
      return -1;

   if (wl.should_close)
      return -1;

   // update shm buffer
   memcpy(wl.shm_ptr, buffer, wl.stride * wl.height);

   wl_surface_attach(wl.surface, wl.buffer, 0, 0);
   wl_surface_damage(wl.surface, 0, 0, wl.width, wl.height);

   struct wl_callback *frame = wl_surface_frame(wl.surface);
   if (!frame)
      return -1;

   wl_callback_add_listener(frame, &frame_listener, &done);

   wl_surface_commit(wl.surface);

   while (!done)
      if (wl_display_dispatch(wl.display) == -1)
      {
         wl_callback_destroy(frame);
         return -1;
      }

   return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mfb_close(void) { destroy(); }
