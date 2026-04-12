#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

bool utf8_decode_next(const unsigned char *bytes, size_t length, size_t *index, uint32_t *codepoint);
