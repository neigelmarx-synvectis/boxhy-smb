/**
 * @file timebase.c
 * @brief Base de temps dérivée de TMR1 (voir timebase.h).
 */
#include "src/system/timebase.h"

volatile sTimebase timebase;

static uint8_t cnt_100ms;
static uint8_t cnt_300ms;
static uint8_t cnt_1s;

void Timebase_Init(void)
{
    timebase.can_ready = false;
    timebase.flag_100ms = false;
    timebase.flag_300ms = false;
    timebase.flag_1s = false;
    timebase.ticks_100ms = 0;
    cnt_100ms = 0;
    cnt_300ms = 0;
    cnt_1s = 0;
}

void Timebase_OnTick(void)
{
    timebase.can_ready = true;

    cnt_100ms++;
    if (cnt_100ms > 9)
    {
        timebase.ticks_100ms++;
        timebase.flag_100ms = true;
        cnt_100ms = 0;

        cnt_1s++;
        if (cnt_1s > 9)
        {
            timebase.flag_1s = true;
            cnt_1s = 0;
        }

        cnt_300ms++;
        if (cnt_300ms > 2)
        {
            timebase.flag_300ms = true;
            cnt_300ms = 0;
        }
    }
}
