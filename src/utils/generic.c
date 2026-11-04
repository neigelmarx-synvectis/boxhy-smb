/**
 * @file generic.c
 * @brief Fonctions génériques réutilisées dans le firmware (voir generic.h).
 */
#include "src/utils/generic.h"

float Generic_MinF(float a, float b)
{
    if (a > b)
        return b;
    else
        return a;
}

float Generic_MaxF(float a, float b)
{
    if (a < b)
        return b;
    else
        return a;
}

float Generic_Saturation(float min, float value, float max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

float Generic_DynamicsLimiter(float limitedValue, float setPoint, float ramp)
{
    limitedValue += Generic_Saturation(-ramp, setPoint - limitedValue, +ramp);
    return limitedValue;
}

uint8_t Generic_PidController(float Kp, float Ki, float Kd, float *int_part, float target, float measurement)
{
    float output;
    (void)Kd; /* terme dérivé non implémenté (code d'origine) */

    *int_part = *int_part + Ki * (measurement - target);        /* intégrale */
    *int_part = Generic_Saturation(-20.0f / Ki, *int_part, 100.0f / Ki); /* saturation intégrale */

    output = Kp * (measurement - target) + (*int_part);
    output = Generic_Saturation(0.0f, output, 100.0f);

    return (uint8_t)output;
}

uint8_t Generic_CharToDec(char input_char)
{
    if ((input_char > 0x39) || (input_char < 0x30))
        return 0x0F;
    else
        return (uint8_t)(input_char & 0x0F);
}

bool Debounce_Update(sDebounce *d, bool raw, uint8_t threshold)
{
    if (raw != d->state)
    {
        d->cnt++;
        if (d->cnt > threshold)
        {
            d->state = raw;
            d->cnt = 0;
            return true;
        }
    }
    else
    {
        d->cnt = 0;
    }
    return false;
}

bool HoldDetect_Update(sDebounce *d, bool raw, uint8_t threshold)
{
    if (raw)
    {
        d->cnt++;
        if (d->cnt > threshold)
        {
            d->state = true;
            d->cnt = 0;
            return true;
        }
    }
    else
    {
        d->cnt = 0;
    }
    return false;
}

bool TimedFlag8_Update(sTimedFlag8 *tf, bool condition, uint16_t timeout)
{
    if (condition)
    {
        tf->cnt++;
        if (tf->cnt > timeout)
        {
            tf->error = true;
            tf->cnt = 0;
        }
    }
    else
    {
        tf->cnt = 0;
    }
    return tf->error;
}

bool TimedFlag16_Update(sTimedFlag16 *tf, bool condition, uint16_t timeout)
{
    if (condition)
    {
        tf->cnt++;
        if (tf->cnt > timeout)
        {
            tf->error = true;
            tf->cnt = 0;
        }
    }
    else
    {
        tf->cnt = 0;
    }
    return tf->error;
}

bool BlockAverage_Push(sBlockAverage *a, int16_t sample, uint8_t size)
{
    a->sum += sample;
    a->ind++;
    if (a->ind >= size)
    {
        a->value = (int16_t)(a->sum / a->ind);
        a->sum = 0;
        a->ind = 0;
        return true;
    }
    return false;
}

int8_t BeepPattern_Update(uint8_t *cpt, uint16_t period)
{
    (*cpt)++;
    if (*cpt == 10)
        return BEEP_ON;
    else if (*cpt == 12)
        return BEEP_OFF;
    else if (*cpt == 14)
        return BEEP_ON;
    else if (*cpt == 16)
        return BEEP_OFF;
    else if (*cpt >= period)
        *cpt = 0;
    return BEEP_UNCHANGED;
}
