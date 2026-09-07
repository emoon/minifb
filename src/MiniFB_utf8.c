#include "MiniFB_utf8.h"

#include <stdlib.h>
#include <string.h>

//-------------------------------------
bool
utf8_decode_next(const unsigned char *bytes, size_t length, size_t *index, uint32_t *codepoint) {
    if (bytes == NULL || index == NULL || codepoint == NULL || *index >= length) {
        return false;
    }

    unsigned char c0 = bytes[*index];
    if (c0 < 0x80) {
        *codepoint = c0;
        *index += 1;
        return true;
    }

    if ((c0 & 0xe0) == 0xc0 && *index + 1 < length) {
        unsigned char c1 = bytes[*index + 1];
        if ((c1 & 0xc0) == 0x80) {
            uint32_t cp = ((uint32_t) (c0 & 0x1f) << 6) | (uint32_t) (c1 & 0x3f);
            if (cp >= 0x80) {
                *codepoint = cp;
                *index += 2;
                return true;
            }
        }
    }
    else if ((c0 & 0xf0) == 0xe0 && *index + 2 < length) {
        unsigned char c1 = bytes[*index + 1];
        unsigned char c2 = bytes[*index + 2];
        if ((c1 & 0xc0) == 0x80 && (c2 & 0xc0) == 0x80) {
            uint32_t cp = ((uint32_t) (c0 & 0x0f) << 12) |
                          ((uint32_t) (c1 & 0x3f) << 6) |
                          (uint32_t) (c2 & 0x3f);
            if (cp >= 0x800 && !(cp >= 0xd800 && cp <= 0xdfff)) {
                *codepoint = cp;
                *index += 3;
                return true;
            }
        }
    }
    else if ((c0 & 0xf8) == 0xf0 && *index + 3 < length) {
        unsigned char c1 = bytes[*index + 1];
        unsigned char c2 = bytes[*index + 2];
        unsigned char c3 = bytes[*index + 3];
        if ((c1 & 0xc0) == 0x80 && (c2 & 0xc0) == 0x80 && (c3 & 0xc0) == 0x80) {
            uint32_t cp = ((uint32_t) (c0 & 0x07) << 18) |
                          ((uint32_t) (c1 & 0x3f) << 12) |
                          ((uint32_t) (c2 & 0x3f) << 6) |
                          (uint32_t) (c3 & 0x3f);
            if (cp >= 0x10000 && cp <= 0x10ffff) {
                *codepoint = cp;
                *index += 4;
                return true;
            }
        }
    }

    // Skip invalid lead byte sequence and continue parsing.
    *index += 1;
    return false;
}

//-------------------------------------
bool
utf8_is_valid(const char *text) {
    if (text == NULL) {
        return false;
    }

    const unsigned char *bytes = (const unsigned char *) text;
    size_t   length    = strlen(text);
    size_t   index     = 0;
    uint32_t codepoint = 0;

    while (index < length) {
        if (utf8_decode_next(bytes, length, &index, &codepoint) == false) {
            return false;
        }
    }

    return true;
}

//-------------------------------------
char *
utf8_ascii_copy(const char *text) {
    if (text == NULL) {
        return NULL;
    }

    size_t  length = strlen(text);
    char   *copy   = (char *) malloc(length + 1);
    size_t  used   = 0;

    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = 0; index < length; ++index) {
        unsigned char byte = (unsigned char) text[index];
        if (byte >= 0x20 && byte < 0x7f) {
            copy[used++] = (char) byte;
        }
    }
    copy[used] = '\0';

    return copy;
}
