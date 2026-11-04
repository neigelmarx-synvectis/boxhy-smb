/**
 * @file hmi.h
 * @brief Interface homme-machine : boutons (anti-rebond), buzzer (motifs d'avertissement),
 *        limitation du buzzer à 5 min en mode défaut, drapeau de RAZ erreurs IHM.
 *
 * Boutons de la façade :
 *  - « mode batterie » et « mode hybride » : interrupteurs à position maintenue, lus avec un
 *    anti-rebond symétrique de 500 ms ; leur état filtré pilote la machine d'états ;
 *  - bouton central : un appui maintenu plus de 5 s acquitte le mode défaut.
 * Buzzer : doubles bips périodiques en fonctionnement (SOC bas, SOC haut, puissance ou SOP haute) ;
 * les bips du mode défaut et de l'arrêt pile sont gérés par la machine d'états.
 *
 * Appels (main.c) : Hmi_UpdateButtons100ms() et Hmi_UpdateBuzzer100ms() dans la tâche 100 ms,
 * Hmi_Update1s() dans la tâche 1 s.
 */
#ifndef HMI_H
#define HMI_H

#include <stdint.h>
#include <stdbool.h>
#include "src/utils/generic.h"

/** État de l'IHM, lu par la machine d'états et la télémétrie. */
typedef struct
{
    sDebounce battery_mode_button; /**< .state : mode batterie demandé */
    sDebounce hybrid_mode_button;  /**< .state : mode hybride demandé */
    /** .state : appui long bouton central (acquitté par la machine d'états) */
    sDebounce middle_button;
    /** .error : buzzer coupé après 5 min en mode défaut (acquitté par la machine d'états) */
    sTimedFlag16 buzzer_5_minutes;
    /** demande de RAZ des erreurs côté IHM (levé par la machine d'états, acquitté par CAN) */
    bool raz_erreur_ihm_flag;
} sHmi;

/** État de l'IHM. */
extern sHmi hmi;

/** Remet à zéro boutons, buzzer et drapeaux IHM. */
void Hmi_Init(void);
/** Lecture filtrée des boutons, toutes les 100 ms. Un appui hybride acquitte les erreurs pile. */
void Hmi_UpdateButtons100ms(void);
/**
 * Motifs sonores d'avertissement en modes batterie / hybride (100 <= step < 200), toutes les 100 ms.
 * @param smb_step état SMB courant
 */
void Hmi_UpdateBuzzer100ms(uint8_t smb_step);
/**
 * Compteur 5 min du buzzer en mode défaut, toutes les 1 s.
 * @param smb_step état SMB courant (le compteur n'avance qu'en SMB_EMERGENCY)
 */
void Hmi_Update1s(uint8_t smb_step);

#endif /* HMI_H */
