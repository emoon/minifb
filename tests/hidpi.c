#include <MiniFB.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define DIMEN_LOW      512
static unsigned int g_buffer_low[DIMEN_LOW * DIMEN_LOW];

#define DIMEN_HIGH      (2*DIMEN_LOW)
static unsigned int g_buffer_high[DIMEN_HIGH * DIMEN_HIGH];

void
pretty_square(unsigned int *p, int dimen) {
    memset(p, 127, dimen * dimen * 4);
    const int one_half_dimen = dimen / 2;
    const int one_quarter_dimen = one_half_dimen / 2;
    const int three_quarter_dimen = one_half_dimen + one_quarter_dimen;
    for (int x = one_quarter_dimen; x < three_quarter_dimen; x++)
        for (int y = one_quarter_dimen; y < three_quarter_dimen; y++)
            p[y * dimen + x] = (x & 1) ? MFB_ARGB(0xff, 223, 0, (255 * (x - one_quarter_dimen)) / one_half_dimen) : MFB_ARGB(0xff, 0, 0, 0);
}

int
main() {
    pretty_square(g_buffer_low, DIMEN_LOW);
    pretty_square(g_buffer_high, DIMEN_HIGH);

    struct mfb_window *window_low  = mfb_open("LowRes", DIMEN_LOW, DIMEN_LOW);
    struct mfb_window *window_high = mfb_open("HighRes", DIMEN_HIGH / 2, DIMEN_HIGH / 2);

    while (window_high || window_low) {
        if (window_low)
            if (mfb_update_ex(window_low, g_buffer_low, DIMEN_LOW, DIMEN_LOW) != STATE_OK)
                window_low = NULL;

        if (window_high)
            if (mfb_update_ex(window_high, g_buffer_high, DIMEN_HIGH, DIMEN_HIGH) != STATE_OK)
                window_high = NULL;

        if (window_high) mfb_wait_sync(window_high);
        else if(window_low) mfb_wait_sync(window_low);
    }

    return 0;
}
