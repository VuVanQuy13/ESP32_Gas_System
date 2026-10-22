#pragma once

#include <stdbool.h>

/* Chuyen muc logic D0 thanh trang thai phat hien lua theo cuc tinh da cau hinh. */
static inline bool flame_d0_level_detects_fire(int level, bool active_low)
{
    if (level != 0 && level != 1)
        return false;

    return active_low ? (level == 0) : (level == 1);
}
