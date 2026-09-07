#include "MiniFB_xkb.h"

#include "MiniFB_internal.h"
#include "MiniFB_utf8.h"

#include <stdlib.h>

//-------------------------------------
// The Compose table belongs to the locale, and a program that never calls setlocale still
// has the environment saying which one the user picked.
//-------------------------------------
static const char *
compose_locale(void) {
    const char *locale;

    locale = getenv("LC_ALL");
    if (locale != NULL && *locale != '\0') {
        return locale;
    }
    locale = getenv("LC_CTYPE");
    if (locale != NULL && *locale != '\0') {
        return locale;
    }
    locale = getenv("LANG");
    if (locale != NULL && *locale != '\0') {
        return locale;
    }

    return "C";
}

//-------------------------------------
// A dead key has no code point of its own, so replaying a cancelled composition loses the
// accent unless it is mapped to the spacing character that stands for it.
//-------------------------------------
static uint32_t
dead_keysym_codepoint(xkb_keysym_t keysym) {
    switch (keysym) {
        case XKB_KEY_dead_grave:       return 0x0060;
        case XKB_KEY_dead_acute:       return 0x00b4;
        case XKB_KEY_dead_circumflex:  return 0x005e;
        case XKB_KEY_dead_tilde:       return 0x007e;
        case XKB_KEY_dead_diaeresis:   return 0x00a8;
        default:                       return 0;
    }
}

//-------------------------------------
uint32_t
mfb_xkb_keysym_codepoint(xkb_keysym_t keysym) {
    uint32_t codepoint = xkb_keysym_to_utf32(keysym);

    if (codepoint == 0) {
        codepoint = dead_keysym_codepoint(keysym);
    }

    return codepoint;
}

//-------------------------------------
bool
mfb_xkb_compose_init(SXkbCompose *compose, struct xkb_context *context) {
    if (compose == NULL || context == NULL) {
        return false;
    }

    mfb_xkb_compose_destroy(compose);

    const char               *locale = compose_locale();
    struct xkb_compose_table *table  = xkb_compose_table_new_from_locale(context, locale,
                                                                        XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (table == NULL) {
        MFB_LOG(MFB_LOG_DEBUG, "xkb_compose_table_new_from_locale('%s') failed; dead keys will not work", locale);
        return false;
    }

    struct xkb_compose_state *state = xkb_compose_state_new(table, XKB_COMPOSE_STATE_NO_FLAGS);
    if (state == NULL) {
        xkb_compose_table_unref(table);
        MFB_LOG(MFB_LOG_WARNING, "xkb_compose_state_new failed; dead keys will not work");
        return false;
    }

    compose->table = table;
    compose->state = state;

    return true;
}

//-------------------------------------
void
mfb_xkb_compose_destroy(SXkbCompose *compose) {
    if (compose == NULL) {
        return;
    }

    // xkbcommon objects are refcounted, so these unref instead of destroy.
    if (compose->state != NULL) {
        xkb_compose_state_unref(compose->state);
        compose->state = NULL;
    }
    if (compose->table != NULL) {
        xkb_compose_table_unref(compose->table);
        compose->table = NULL;
    }

    compose->sequence_count = 0;
}

//-------------------------------------
void
mfb_xkb_compose_reset(SXkbCompose *compose) {
    if (compose == NULL) {
        return;
    }

    if (compose->state != NULL) {
        xkb_compose_state_reset(compose->state);
    }

    compose->sequence_count = 0;
}

//-------------------------------------
// A completed sequence usually has a keysym of its own, but a Compose file can also map one
// to a string that no single keysym stands for, so the UTF-8 form is the fallback.
//-------------------------------------
static void
emit_composed_text(SXkbCompose *compose, SWindowData *window_data) {
    xkb_keysym_t composed = xkb_compose_state_get_one_sym(compose->state);

    if (composed != XKB_KEY_NoSymbol) {
        uint32_t codepoint = xkb_keysym_to_utf32(composed);
        if (codepoint != 0) {
            mfb_dispatch_char_input(window_data, codepoint);
            return;
        }
    }

    char text[64];
    int  length = xkb_compose_state_get_utf8(compose->state, text, sizeof(text));

    if (length <= 0) {
        return;
    }

    size_t   available = ((size_t) length < sizeof(text) - 1) ? (size_t) length : sizeof(text) - 1;
    size_t   index     = 0;
    uint32_t codepoint = 0;

    while (utf8_decode_next((const unsigned char *) text, available, &index, &codepoint) == true) {
        if (codepoint != 0) {
            mfb_dispatch_char_input(window_data, codepoint);
        }
    }
}

//-------------------------------------
// An accent followed by a letter that does not combine with it is not text the user meant to
// throw away, so the sequence comes out as the characters it was made of.
//-------------------------------------
static void
emit_cancelled_sequence(SXkbCompose *compose, SWindowData *window_data, xkb_keysym_t keysym) {
    for (uint8_t index = 0; index < compose->sequence_count; ++index) {
        uint32_t buffered = mfb_xkb_keysym_codepoint(compose->sequence[index]);
        if (buffered != 0) {
            mfb_dispatch_char_input(window_data, buffered);
        }
    }

    uint32_t codepoint = mfb_xkb_keysym_codepoint(keysym);
    if (codepoint != 0) {
        mfb_dispatch_char_input(window_data, codepoint);
    }
}

//-------------------------------------
bool
mfb_xkb_compose_feed(SXkbCompose *compose, SWindowData *window_data, xkb_keysym_t keysym) {
    if (compose == NULL || compose->state == NULL) {
        return false;
    }

    xkb_compose_state_feed(compose->state, keysym);

    switch (xkb_compose_state_get_status(compose->state)) {
        case XKB_COMPOSE_COMPOSED:
            emit_composed_text(compose, window_data);
            mfb_xkb_compose_reset(compose);
            return true;

        case XKB_COMPOSE_CANCELLED:
            emit_cancelled_sequence(compose, window_data, keysym);
            mfb_xkb_compose_reset(compose);
            return true;

        case XKB_COMPOSE_COMPOSING:
            // Nothing to show yet. The keysym is kept because a sequence that ends up
            // cancelled has to come back out, and by then the key state has moved on.
            if (compose->sequence_count < MFB_XKB_COMPOSE_MAX_SEQUENCE) {
                compose->sequence[compose->sequence_count++] = keysym;
            }
            return true;

        case XKB_COMPOSE_NOTHING:
        default:
            return false;
    }
}
