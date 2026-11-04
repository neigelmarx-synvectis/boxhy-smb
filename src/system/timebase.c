/**
 * @file timebase.c
 * @brief Base de temps dérivée de TMR1 (voir timebase.h).
 *
 * Divise le tick matériel de 10 ms en périodes de 100 ms, 300 ms et 1 s.
 * Ce code s'exécute en interruption : il doit rester court et ne rien faire d'autre que compter.
 */
#include "src/system/timebase.h"

volatile sTimebase timebase;

/* Pré-diviseurs privés (non volatile : utilisés uniquement dans l'ISR, après l'initialisation) */
static uint8_t cnt_100ms; /* ticks de 10 ms dans la période de 100 ms en cours (0..9) */
static uint8_t cnt_300ms; /* périodes de 100 ms dans la période de 300 ms en cours (0..2) */
static uint8_t cnt_1s;    /* périodes de 100 ms dans la seconde en cours (0..9) */

/* Remise à zéro complète, appelée depuis main() avant le démarrage de TMR1. */
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

/* Interruption TMR1, toutes les 10 ms. */
void Timebase_OnTick(void)
{
    /* Une trame de télémétrie CAN peut partir à chaque tick (une par période de 10 ms) */
    timebase.can_ready = true;

    cnt_100ms++;
    if (cnt_100ms > 9) /* 10e tick : 100 ms écoulées */
    {
        timebase.ticks_100ms++; /* chronomètre de la machine d'états */
        timebase.flag_100ms = true;
        cnt_100ms = 0;

        cnt_1s++;
        if (cnt_1s > 9) /* 10 x 100 ms */
        {
            timebase.flag_1s = true;
            cnt_1s = 0;
        }

        cnt_300ms++;
        if (cnt_300ms > 2) /* 3 x 100 ms */
        {
            timebase.flag_300ms = true;
            cnt_300ms = 0;
        }
    }
}
