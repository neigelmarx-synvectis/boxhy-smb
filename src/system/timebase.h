/**
 * @file timebase.h
 * @brief Base de temps : drapeaux 100 ms / 300 ms / 1 s dérivés de l'interruption TMR1 (10 ms).
 *
 * Les drapeaux sont levés par l'ISR et acquittés par la boucle principale.
 * ticks_100ms est le chronomètre de la machine d'états (remis à zéro par elle).
 */
#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool can_ready;       /**< une période TMR1 écoulée : autorise l'émission d'une trame CAN */
    bool flag_100ms;
    bool flag_300ms;
    bool flag_1s;
    uint16_t ticks_100ms; /**< chronomètre libre en pas de 100 ms, RAZ par la machine d'états */
} sTimebase;

extern volatile sTimebase timebase;

void Timebase_Init(void);
/** Corps de l'interruption TMR1 (période 10 ms). */
void Timebase_OnTick(void);

#endif /* TIMEBASE_H */
