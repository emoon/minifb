#include "MiniFB_internal.h"
#include <stdint.h>

//#define kUseBilinearInterpolation

#if defined(kUseBilinearInterpolation)
static uint32_t interpolate(uint32_t *srcImage, uint32_t x, uint32_t y, uint32_t srcOffsetX, uint32_t srcOffsetY, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcPitch) {
    uint32_t incX = x + 1 < srcWidth ? 1 : 0;
    uint32_t incY = y + 1 < srcHeight ? srcPitch : 0;
    uint8_t *p00 = (uint8_t *) &srcImage[(srcOffsetX >> 16)];
    uint8_t *p01 = (uint8_t *) &srcImage[(srcOffsetX >> 16) + incX];
    uint8_t *p10 = (uint8_t *) &srcImage[(srcOffsetX >> 16) + incY];
    uint8_t *p11 = (uint8_t *) &srcImage[(srcOffsetX >> 16) + incY + incX];

    uint32_t wx2 = srcOffsetX & 0xffff;
    uint32_t wy2 = srcOffsetY & 0xffff;
    uint32_t wx1 = 0x10000 - wx2;
    uint32_t wy1 = 0x10000 - wy2;

    uint32_t w1 = ((uint64_t) wx1 * wy1) >> 16;
    uint32_t w2 = ((uint64_t) wx2 * wy1) >> 16;
    uint32_t w3 = ((uint64_t) wx1 * wy2) >> 16;
    uint32_t w4 = ((uint64_t) wx2 * wy2) >> 16;

    // If you don't have uint64_t
    //uint32_t b = (((p00[0] * wx1 + p01[0] * wx2) >> 16) * wy1 + ((p10[0] * wx1 + p11[0] * wx2) >> 16) * wy2) >> 16;
    //uint32_t g = (((p00[1] * wx1 + p01[1] * wx2) >> 16) * wy1 + ((p10[1] * wx1 + p11[1] * wx2) >> 16) * wy2) >> 16;
    //uint32_t r = (((p00[2] * wx1 + p01[2] * wx2) >> 16) * wy1 + ((p10[2] * wx1 + p11[2] * wx2) >> 16) * wy2) >> 16;
    //uint32_t a = (((p00[3] * wx1 + p01[3] * wx2) >> 16) * wy1 + ((p10[3] * wx1 + p11[3] * wx2) >> 16) * wy2) >> 16;

    uint32_t b = ((p00[0] * w1 + p01[0] * w2) + (p10[0] * w3 + p11[0] * w4)) >> 16;
    uint32_t g = ((p00[1] * w1 + p01[1] * w2) + (p10[1] * w3 + p11[1] * w4)) >> 16;
    uint32_t r = ((p00[2] * w1 + p01[2] * w2) + (p10[2] * w3 + p11[2] * w4)) >> 16;
    uint32_t a = ((p00[3] * w1 + p01[3] * w2) + (p10[3] * w3 + p11[3] * w4)) >> 16;

    return (a << 24) + (r << 16) + (g << 8) + b;
}
#endif

// Only for 32 bits images
void stretch_image(uint32_t *srcImage, uint32_t srcX, uint32_t srcY, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcPitch,
                  uint32_t *dstImage, uint32_t dstX, uint32_t dstY, uint32_t dstWidth, uint32_t dstHeight, uint32_t dstPitch) {

    uint32_t    x, y;
    uint32_t    srcOffsetX, srcOffsetY;

    if(srcImage == 0x0 || dstImage == 0x0)
        return;

    srcImage += srcX + srcY * srcPitch;
    dstImage += dstX + dstY * dstPitch;

    const uint32_t deltaX = (srcWidth  << 16) / dstWidth;
    const uint32_t deltaY = (srcHeight << 16) / dstHeight;

    srcOffsetY = 0;
    for(y=0; y<dstHeight; ++y) {
        srcOffsetX = 0;
        for(x=0; x<dstWidth; ++x) {
#if defined(kUseBilinearInterpolation)
            dstImage[x] = interpolate(srcImage, x+srcX, y+srcY, srcOffsetX, srcOffsetY, srcWidth, srcHeight, srcPitch);
#else
            dstImage[x] = srcImage[srcOffsetX >> 16];
#endif
            srcOffsetX += deltaX;
        }

        srcOffsetY += deltaY;
        if(srcOffsetY >= 0x10000) {
            srcImage += (srcOffsetY >> 16) * srcPitch;
            srcOffsetY &= 0xffff;
        }
        dstImage += dstPitch;
    }
}
