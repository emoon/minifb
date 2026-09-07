#pragma once

#include "WindowData.h"

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>

#include <stdbool.h>
#include <stdint.h>

//-------------------------------------
// Dead keys and Compose sequences for the backends that take their text from xkbcommon.
// They feed keysyms in and text comes out, so X11 and Wayland cannot drift apart on what a
// composition produces.
//-------------------------------------
#define MFB_XKB_COMPOSE_MAX_SEQUENCE 8

typedef struct {
    struct xkb_compose_table *table;
    struct xkb_compose_state *state;
    xkb_keysym_t              sequence[MFB_XKB_COMPOSE_MAX_SEQUENCE];
    uint8_t                   sequence_count;
} SXkbCompose;

//-------------------------------------
// The context stays owned by the caller and has to outlive the compose state.
bool     mfb_xkb_compose_init(SXkbCompose *compose, struct xkb_context *context);
void     mfb_xkb_compose_destroy(SXkbCompose *compose);
void     mfb_xkb_compose_reset(SXkbCompose *compose);

// True when the composition took charge of this keysym, whether it completed a sequence,
// cancelled one, or is waiting for the next key. False when the caller has to emit the text
// of the key itself, which is what happens whenever no sequence is involved.
bool     mfb_xkb_compose_feed(SXkbCompose *compose, SWindowData *window_data, xkb_keysym_t keysym);

// The code point of a keysym, or the spacing character a dead key stands for.
uint32_t mfb_xkb_keysym_codepoint(xkb_keysym_t keysym);
