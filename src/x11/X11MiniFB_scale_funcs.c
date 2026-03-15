#include "X11MiniFB_scale_funcs.h"

#include <X11/Xatom.h>
#include <X11/Xresource.h>
#if defined(MINIFB_HAS_XRANDR)
    #include <X11/extensions/Xrandr.h>
#endif

#include <MiniFB.h>
#include <MiniFB_internal.h>

#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//-------------------------------------
static uint16_t
read_u16_xsettings(const unsigned char *ptr, bool little_endian) {
    if (little_endian) {
        return (uint16_t) ptr[0] | ((uint16_t) ptr[1] << 8);
    }
    return ((uint16_t) ptr[0] << 8) | (uint16_t) ptr[1];
}

//-------------------------------------
static uint32_t
read_u32_xsettings(const unsigned char *ptr, bool little_endian) {
    if (little_endian) {
        return (uint32_t) ptr[0] |
               ((uint32_t) ptr[1] << 8) |
               ((uint32_t) ptr[2] << 16) |
               ((uint32_t) ptr[3] << 24);
    }
    return ((uint32_t) ptr[0] << 24) |
           ((uint32_t) ptr[1] << 16) |
           ((uint32_t) ptr[2] << 8) |
           (uint32_t) ptr[3];
}

//-------------------------------------
static size_t
align_to_4(size_t value) {
    return (value + 3u) & ~(size_t) 3u;
}

//-------------------------------------
static bool
get_scale_from_xsettings(Display *display, int screen, float *scale_x, float *scale_y) {
    if (display == NULL || scale_x == NULL || scale_y == NULL) {
        return false;
    }

    char selection_name[32];
    snprintf(selection_name, sizeof(selection_name), "_XSETTINGS_S%d", screen);

    Atom selection_atom = XInternAtom(display, selection_name, True);
    Atom settings_atom = XInternAtom(display, "_XSETTINGS_SETTINGS", True);
    if (selection_atom == None || settings_atom == None) {
        return false;
    }

    Window owner = XGetSelectionOwner(display, selection_atom);
    if (owner == None) {
        return false;
    }

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long n_items = 0;
    unsigned long bytes_after = 0;
    unsigned char *data = NULL;
    int status = XGetWindowProperty(display, owner, settings_atom,
                                    0, 4096, False, settings_atom,
                                    &actual_type, &actual_format,
                                    &n_items, &bytes_after, &data);
    if (status != Success || data == NULL || actual_type != settings_atom || actual_format != 8) {
        if (data != NULL) {
            XFree(data);
        }
        return false;
    }

    size_t size = (size_t) n_items;
    if (size < 12) {
        XFree(data);
        return false;
    }

    bool little_endian = true;
    if (data[0] == MSBFirst || data[0] == 'B') {
        little_endian = false;
    }
    else if (data[0] == LSBFirst || data[0] == 'l') {
        little_endian = true;
    }

    uint32_t setting_count = read_u32_xsettings(data + 8, little_endian);
    size_t offset = 12;
    float xft_scale = 0.0f;
    float gdk_scale = 0.0f;

    for (uint32_t i = 0; i < setting_count; ++i) {
        if (offset + 4 > size) {
            break;
        }

        uint8_t type = data[offset];
        uint16_t name_len = read_u16_xsettings(data + offset + 2, little_endian);
        offset += 4;
        if (offset + name_len > size) {
            break;
        }

        const unsigned char *name = data + offset;
        offset += align_to_4((size_t) name_len);
        if (offset + 4 > size) {
            break;
        }

        // last-change-serial
        offset += 4;

        if (type == 0) {
            if (offset + 4 > size) {
                break;
            }
            uint32_t value = read_u32_xsettings(data + offset, little_endian);
            offset += 4;

            if (name_len == 7 && memcmp(name, "Xft/DPI", 7) == 0) {
                // Xsettings stores DPI as value * 1024.
                if (value > 0) {
                    xft_scale = ((float) value / 1024.0f) / 96.0f;
                }
            }
            else if (name_len == 23 && memcmp(name, "Gdk/WindowScalingFactor", 23) == 0) {
                if (value > 0) {
                    gdk_scale = (float) value;
                }
            }
        }
        else if (type == 1) {
            if (offset + 4 > size) {
                break;
            }
            uint32_t str_len = read_u32_xsettings(data + offset, little_endian);
            offset += 4;
            if (offset + str_len > size) {
                break;
            }
            offset += align_to_4((size_t) str_len);
        }
        else if (type == 2) {
            if (offset + 8 > size) {
                break;
            }
            offset += 8;
        }
        else {
            break;
        }
    }

    XFree(data);

    if (xft_scale > 0.0f) {
        *scale_x = xft_scale;
        *scale_y = xft_scale;
        MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XSettings scale from Xft/DPI = %.6f.", xft_scale);
        return true;
    }

    if (gdk_scale > 0.0f) {
        *scale_x = gdk_scale;
        *scale_y = gdk_scale;
        MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XSettings scale from Gdk/WindowScalingFactor = %.6f.", gdk_scale);
        return true;
    }

    return false;
}

#if defined(MINIFB_HAS_XRANDR)
//-------------------------------------
static bool
get_window_center_in_root(Display *display, Window window, int screen, int *center_x, int *center_y) {
    if (display == NULL || window == None || center_x == NULL || center_y == NULL) {
        return false;
    }

    Window root = RootWindow(display, screen);
    if (root == None) {
        return false;
    }

    int wx = 0, wy = 0;
    Window child = None;
    if (!XTranslateCoordinates(display, window, root, 0, 0, &wx, &wy, &child)) {
        return false;
    }

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(display, window, &attrs)) {
        return false;
    }

    *center_x = wx + (attrs.width > 0 ? attrs.width / 2 : 0);
    *center_y = wy + (attrs.height > 0 ? attrs.height / 2 : 0);
    return true;
}

//-------------------------------------
static bool
get_scale_from_xrandr(Display *display, Window window, int screen, float *scale_x, float *scale_y) {
    if (display == NULL || window == None || scale_x == NULL || scale_y == NULL) {
        MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRandR query skipped due to invalid arguments.");
        return false;
    }

    int event_base = 0;
    int error_base = 0;
    if (!XRRQueryExtension(display, &event_base, &error_base)) {
        MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRandR extension not available on this display.");
        return false;
    }

    Window root = RootWindow(display, screen);
    if (root == None) {
        MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRandR root window is invalid.");
        return false;
    }

    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, root);
    if (resources == NULL) {
        MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRRGetScreenResourcesCurrent returned NULL.");
        return false;
    }

    int center_x = 0, center_y = 0;
    bool have_center = get_window_center_in_root(display, window, screen, &center_x, &center_y);
    bool found_mm = false;
    int best_mm_area = INT_MAX;
    float best_mm_x = 0.0f, best_mm_y = 0.0f;
    bool found_transform = false;
    int best_transform_area = INT_MAX;
    float best_transform_x = 0.0f, best_transform_y = 0.0f;
    int outputs_total = resources->noutput;
    int outputs_connected = 0;
    int outputs_with_crtc = 0;
    int outputs_inside = 0;
    int outputs_with_mm = 0;
    int outputs_with_transform = 0;

    for (int i = 0; i < resources->noutput; ++i) {
        XRROutputInfo *output = XRRGetOutputInfo(display, resources, resources->outputs[i]);
        if (output == NULL) {
            MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRandR output[%d] info is NULL.", i);
            continue;
        }

        MFB_LOG(MFB_LOG_TRACE,
                "X11MiniFB: XRandR output[%d] name=%s connection=%d crtc=%lu mm=%ldx%ld.",
                i,
                output->name ? output->name : "(null)",
                output->connection,
                (unsigned long) output->crtc,
                (long) output->mm_width,
                (long) output->mm_height);

        if (output->connection != RR_Connected) {
            XRRFreeOutputInfo(output);
            continue;
        }
        outputs_connected++;

        if (output->crtc == None) {
            XRRFreeOutputInfo(output);
            continue;
        }
        outputs_with_crtc++;

        XRRCrtcInfo *crtc = XRRGetCrtcInfo(display, resources, output->crtc);
        if (crtc == NULL || crtc->width <= 0 || crtc->height <= 0) {
            MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRandR output[%d] has invalid CRTC info.", i);
            if (crtc != NULL) {
                XRRFreeCrtcInfo(crtc);
            }
            XRRFreeOutputInfo(output);
            continue;
        }

        bool inside = true;
        if (have_center) {
            inside = (center_x >= crtc->x && center_x < (crtc->x + (int) crtc->width) &&
                      center_y >= crtc->y && center_y < (crtc->y + (int) crtc->height));
        }
        MFB_LOG(MFB_LOG_TRACE,
                "X11MiniFB: XRandR output[%d] crtc pos=%d,%d size=%ux%u window_center=%d,%d inside=%d.",
                i, crtc->x, crtc->y, crtc->width, crtc->height, center_x, center_y, inside ? 1 : 0);

        if (inside) {
            outputs_inside++;
            int area = (int) crtc->width * (int) crtc->height;

            if (output->mm_width > 0 && output->mm_height > 0) {
                float dpi_x = ((float) crtc->width * 25.4f) / (float) output->mm_width;
                float dpi_y = ((float) crtc->height * 25.4f) / (float) output->mm_height;
                if (dpi_x > 0.0f && dpi_y > 0.0f) {
                    outputs_with_mm++;
                    MFB_LOG(MFB_LOG_TRACE,
                            "X11MiniFB: XRandR output[%d] physical dpi=%.2f,%.2f scale=%.2f,%.2f.",
                            i, dpi_x, dpi_y, dpi_x / 96.0f, dpi_y / 96.0f);
                    if (!found_mm || area < best_mm_area) {
                        best_mm_x = dpi_x / 96.0f;
                        best_mm_y = dpi_y / 96.0f;
                        best_mm_area = area;
                        found_mm = true;
                    }
                }
            }

            XRRCrtcTransformAttributes *transform = NULL;
            if (XRRGetCrtcTransform(display, output->crtc, &transform) != 0 && transform != NULL) {
                float m00 = (float) transform->currentTransform.matrix[0][0] / 65536.0f;
                float m01 = (float) transform->currentTransform.matrix[0][1] / 65536.0f;
                float m10 = (float) transform->currentTransform.matrix[1][0] / 65536.0f;
                float m11 = (float) transform->currentTransform.matrix[1][1] / 65536.0f;
                float sx = sqrtf((m00 * m00) + (m01 * m01));
                float sy = sqrtf((m10 * m10) + (m11 * m11));
                MFB_LOG(MFB_LOG_TRACE,
                        "X11MiniFB: XRandR output[%d] transform matrix [[%.4f %.4f][%.4f %.4f]] scale=%.4f,%.4f.",
                        i, m00, m01, m10, m11, sx, sy);

                if (sx > 0.0f && sy > 0.0f) {
                    outputs_with_transform++;
                    if (!found_transform || area < best_transform_area) {
                        best_transform_x = sx;
                        best_transform_y = sy;
                        best_transform_area = area;
                        found_transform = true;
                    }
                }
                XFree(transform);
            }
            else {
                MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRandR output[%d] has no transform attributes.", i);
            }
        }

        XRRFreeCrtcInfo(crtc);
        XRRFreeOutputInfo(output);
    }

    XRRFreeScreenResources(resources);

    if (found_mm) {
        *scale_x = best_mm_x;
        *scale_y = best_mm_y;
        MFB_LOG(MFB_LOG_TRACE,
                "X11MiniFB: XRandR scale from physical DPI (outputs=%d connected=%d crtc=%d inside=%d with_mm=%d scale_x=%.2f scale_y=%.2f).",
                outputs_total, outputs_connected, outputs_with_crtc, outputs_inside, outputs_with_mm, *scale_x, *scale_y);
        return true;
    }

    if (found_transform) {
        *scale_x = best_transform_x;
        *scale_y = best_transform_y;
        MFB_LOG(MFB_LOG_TRACE,
                "X11MiniFB: XRandR scale from CRTC transform (outputs=%d connected=%d crtc=%d inside=%d with_transform=%d scale_x=%.2f scale_y=%.2f).",
                outputs_total, outputs_connected, outputs_with_crtc, outputs_inside, outputs_with_transform, *scale_x, *scale_y);
        return true;
    }

    if (!have_center) {
        MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRandR could not map window to root coordinates.");
    }
    MFB_LOG(MFB_LOG_TRACE,
            "X11MiniFB: XRandR scale unavailable (outputs=%d connected=%d crtc=%d inside=%d with_mm=%d with_transform=%d).",
            outputs_total, outputs_connected, outputs_with_crtc, outputs_inside, outputs_with_mm, outputs_with_transform);
    return false;
}

//-------------------------------------
static bool
get_scale_from_xrandr_any_output(Display *display, int screen, float *scale_x, float *scale_y) {
    if (display == NULL || scale_x == NULL || scale_y == NULL) {
        return false;
    }

    Window root = RootWindow(display, screen);
    if (root == None) {
        return false;
    }

    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, root);
    if (resources == NULL) {
        return false;
    }

    bool found_mm = false;
    int best_mm_area = INT_MAX;
    float best_mm_x = 0.0f, best_mm_y = 0.0f;

    for (int i = 0; i < resources->noutput; ++i) {
        XRROutputInfo *output = XRRGetOutputInfo(display, resources, resources->outputs[i]);
        if (output == NULL) {
            continue;
        }
        MFB_LOG(MFB_LOG_TRACE,
                "X11MiniFB: XRandR-any output[%d] name=%s connection=%d crtc=%lu mm=%ldx%ld.",
                i,
                output->name ? output->name : "(null)",
                output->connection,
                (unsigned long) output->crtc,
                (long) output->mm_width,
                (long) output->mm_height);

        if (output->connection != RR_Connected || output->crtc == None || output->mm_width <= 0 || output->mm_height <= 0) {
            XRRFreeOutputInfo(output);
            continue;
        }

        XRRCrtcInfo *crtc = XRRGetCrtcInfo(display, resources, output->crtc);
        if (crtc == NULL || crtc->width <= 0 || crtc->height <= 0) {
            if (crtc != NULL) {
                XRRFreeCrtcInfo(crtc);
            }
            XRRFreeOutputInfo(output);
            continue;
        }

        float dpi_x = ((float) crtc->width * 25.4f) / (float) output->mm_width;
        float dpi_y = ((float) crtc->height * 25.4f) / (float) output->mm_height;
        if (dpi_x > 0.0f && dpi_y > 0.0f) {
            int area = (int) crtc->width * (int) crtc->height;
            if (!found_mm || area < best_mm_area) {
                best_mm_x = dpi_x / 96.0f;
                best_mm_y = dpi_y / 96.0f;
                best_mm_area = area;
                found_mm = true;
            }
        }

        XRRFreeCrtcInfo(crtc);
        XRRFreeOutputInfo(output);
    }

    XRRFreeScreenResources(resources);

    if (!found_mm) {
        return false;
    }

    *scale_x = best_mm_x;
    *scale_y = best_mm_y;
    MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRandR fallback picked any connected output (scale_x=%.2f, scale_y=%.2f).", *scale_x, *scale_y);
    return true;
}
#endif

//-------------------------------------
static bool
get_scale_from_xresources(Display *display, float *scale_x, float *scale_y) {
    if (display == NULL || scale_x == NULL || scale_y == NULL) {
        return false;
    }

    XrmInitialize();
    char *resource_manager = XResourceManagerString(display);
    if (resource_manager == NULL) {
        return false;
    }

    XrmDatabase db = XrmGetStringDatabase(resource_manager);
    if (db == NULL) {
        return false;
    }

    bool ok = false;
    XrmValue value;
    char *value_type = NULL;
    if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &value_type, &value) &&
        value.addr != NULL && value.size > 0) {
        char *end = NULL;
        float dpi = strtof(value.addr, &end);
        if (end != value.addr && dpi > 0.0f) {
            *scale_x = dpi / 96.0f;
            *scale_y = dpi / 96.0f;
            ok = true;
        }
    }

    XrmDestroyDatabase(db);
    return ok;
}

//-------------------------------------
static bool
get_scale_from_display_mm(Display *display, int screen, float *scale_x, float *scale_y) {
    if (display == NULL || scale_x == NULL || scale_y == NULL) {
        return false;
    }

    int width_px = DisplayWidth(display, screen);
    int height_px = DisplayHeight(display, screen);
    int width_mm = DisplayWidthMM(display, screen);
    int height_mm = DisplayHeightMM(display, screen);
    bool ok = false;

    if (width_px > 0 && width_mm > 0) {
        float dpi_x = ((float) width_px * 25.4f) / (float) width_mm;
        if (dpi_x > 0.0f) {
            *scale_x = dpi_x / 96.0f;
            ok = true;
        }
    }

    if (height_px > 0 && height_mm > 0) {
        float dpi_y = ((float) height_px * 25.4f) / (float) height_mm;
        if (dpi_y > 0.0f) {
            *scale_y = dpi_y / 96.0f;
            ok = true;
        }
    }

    return ok;
}

//-------------------------------------
void
mfb_x11_query_scale_methods(Display *display, Window window, int screen, SX11ScaleMethods *methods) {
    if (methods == NULL) {
        return;
    }

    methods->has_xresources = false;
    methods->x_xresources = 1.0f;
    methods->y_xresources = 1.0f;
    methods->has_xsettings = false;
    methods->x_xsettings = 1.0f;
    methods->y_xsettings = 1.0f;
    methods->has_xrandr_window = false;
    methods->x_xrandr_window = 1.0f;
    methods->y_xrandr_window = 1.0f;
    methods->has_xrandr_any = false;
    methods->x_xrandr_any = 1.0f;
    methods->y_xrandr_any = 1.0f;
    methods->has_display_mm = false;
    methods->x_display_mm = 1.0f;
    methods->y_display_mm = 1.0f;

    if (display == NULL) {
        return;
    }
    if (screen < 0) {
        screen = DefaultScreen(display);
    }

    methods->has_xresources = get_scale_from_xresources(display, &methods->x_xresources, &methods->y_xresources);
    methods->has_xsettings = get_scale_from_xsettings(display, screen, &methods->x_xsettings, &methods->y_xsettings);
    methods->has_display_mm = get_scale_from_display_mm(display, screen, &methods->x_display_mm, &methods->y_display_mm);
    #if defined(MINIFB_HAS_XRANDR)
    methods->has_xrandr_window = get_scale_from_xrandr(display, window, screen, &methods->x_xrandr_window, &methods->y_xrandr_window);
    methods->has_xrandr_any = get_scale_from_xrandr_any_output(display, screen, &methods->x_xrandr_any, &methods->y_xrandr_any);
    #else
    MFB_LOG(MFB_LOG_TRACE, "X11MiniFB: XRandR support not available at compile time.");
    #endif
}
