#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool utf8_decode_next(const unsigned char *bytes, size_t length, size_t *index, uint32_t *codepoint);

bool  utf8_is_valid(const char *text);
// Caller frees. Keeps printable ASCII only.
char *utf8_ascii_copy(const char *text);
