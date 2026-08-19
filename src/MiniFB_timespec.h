#pragma once

#include <stdbool.h>
#include <time.h>

//-------------------------------------
struct timespec
ts_add(const struct timespec a, const struct timespec b);

//-------------------------------------
struct timespec
ts_sub_sat(const struct timespec a, const struct timespec b);

//-------------------------------------
bool
ts_is_less(const struct timespec a, const struct timespec b);

//-------------------------------------
bool
ts_is_zero(const struct timespec value);

//-------------------------------------
struct timespec
ms_to_ts(double ms);
