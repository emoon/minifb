#include <stdio.h>

#include "minifb_version.h"

int main(void) {
    printf("MINIFB_VERSION_STRING=%s\n", MINIFB_VERSION_STRING);
    printf("MINIFB_VERSION_NUMERIC=0x%012llX\n", (unsigned long long) MINIFB_VERSION_NUMERIC);
    printf("MINIFB_VERSION_GET_MAJOR=%u\n", (unsigned int) MINIFB_VERSION_GET_MAJOR(MINIFB_VERSION_NUMERIC));
    printf("MINIFB_VERSION_GET_MINOR=%u\n", (unsigned int) MINIFB_VERSION_GET_MINOR(MINIFB_VERSION_NUMERIC));
    printf("MINIFB_VERSION_GET_PATCH=%u\n", (unsigned int) MINIFB_VERSION_GET_PATCH(MINIFB_VERSION_NUMERIC));
    printf("MINIFB_COMMITS_SINCE_TAG=%d\n", MINIFB_COMMITS_SINCE_TAG);
    printf("MINIFB_COMMIT_COUNT=%d\n", MINIFB_COMMIT_COUNT);
    printf("MINIFB_GIT_SHA=%s\n", MINIFB_GIT_SHA);
    printf("MINIFB_GIT_DIRTY=%d\n", MINIFB_GIT_DIRTY);
    return 0;
}
