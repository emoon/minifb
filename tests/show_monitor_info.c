// show_monitor_info — prints the properties of every connected monitor.
// No window is created; this is a pure diagnostic tool for verifying
// the mfb_get_num_monitors / mfb_get_monitor_info backend implementations.

#include <MiniFB.h>
#include <stdio.h>

int
main(void) {
    int count = mfb_get_num_monitors();
    printf("monitors: %d\n", count);

    for (int i = 0; i < count; i++) {
        mfb_monitor_info info;
        if (!mfb_get_monitor_info((unsigned) i, &info)) {
            printf("[%d] (failed to retrieve info)\n", i);
            continue;
        }

        printf("[%d]%s\n",        i, info.is_primary ? " primary" : "");
        printf("    name:          \"%s\"\n", info.name);
        printf("    logical pos:   %d, %d\n", info.logical_x, info.logical_y);
        printf("    logical size:  %u, %u\n", info.logical_width, info.logical_height);
        printf("    physical size: %u, %u\n", info.physical_width, info.physical_height);
        printf("    scale:         %.2f, %.2f\n\n", (double) info.scale_x, (double) info.scale_y);
    }

    return 0;
}
