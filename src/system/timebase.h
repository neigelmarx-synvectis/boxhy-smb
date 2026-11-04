/**
 * @file timebase.h
 * @brief Base de temps : drapeaux 100 ms / 300 ms / 1 s dérivés de l'interruption TMR1 (10 ms).
 *
 * Les drapeaux sont levés par l'ISR et acquittés par la boucle principale.
 * ticks_100ms est le chronomètre de la machine d'états (remis à zéro par elle).
 *
 * Fonctionnement : Timebase_OnTick() est appelée par l'interruption TMR1 toutes les 10 ms
 * (enregistrée dans main.c). Elle se contente de compter et de lever des drapeaux ; tout le
 * travail est fait dans la boucle principale, qui teste chaque drapeau puis le remet à false.
 * Un drapeau déjà levé et pas encore traité n'est pas mémorisé deux fois : si la boucle prend
 * du retard, des périodes sont simplement sautées (comportement d'origine).
 */
#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>
#include <stdbool.h>

/** État de la base de temps, partagé entre l'ISR TMR1 et la boucle principale. */
typedef struct
{
    bool can_ready;       /**< une période TMR1 écoulée : autorise l'émission d'une trame CAN */
    bool flag_100ms;      /**< 100 ms écoulées : tâches 100 ms (supervisions, boutons, buzzer...) */
    bool flag_300ms;      /**< 300 ms écoulées : requête RS232 vers l'onduleur */
    bool flag_1s;         /**< 1 s écoulée : keep-alive, watchdogs, ventilateurs, consigne pile */
    uint16_t ticks_100ms; /**< chronomètre libre en pas de 100 ms, RAZ par la machine d'états */
} sTimebase;

/** Instance unique ; volatile car modifiée sous interruption. */
extern volatile sTimebase timebase;

/** Remet drapeaux et compteurs à zéro (à appeler avant TMR1_Start). */
void Timebase_Init(void);
/** Corps de l'interruption TMR1 (période 10 ms). */
void Timebase_OnTick(void);

#endif /* TIMEBASE_H */
