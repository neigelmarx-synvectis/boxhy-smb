/**
 * @file hmi.h
 * @brief Interface homme-machine : boutons (anti-rebond), buzzer (motifs d'avertissement),
 *        limitation du buzzer à 5 min en mode défaut, drapeau de RAZ erreurs IHM.
 */
#ifndef HMI_H
#define HMI_H

#include <stdint.h>
#include <stdbool.h>
#include "src/utils/generic.h"

typedef struct
{
    sDebounce battery_mode_button; /**< .state : mode batterie demandé */
    sDebounce hybrid_mode_button;  /**< .state : mode hybride demandé */
    sDebounce middle_button;       /**< .state : appui long bouton central (acquitté par la machine d'états) */
    sTimedFlag16 buzzer_5_minutes; /**< .error : buzzer coupé après 5 min en mode défaut (acquitté par la machine d'états) */
    bool raz_erreur_ihm_flag;      /**< demande de RAZ des erreurs côté IHM (levé par la machine d'états, acquitté par CAN) */
} sHmi;

extern sHmi hmi;

void Hmi_Init(void);
/** Lecture filtrée des boutons, toutes les 100 ms. Un appui hybride acquitte les erreurs pile. */
void Hmi_UpdateButtons100ms(void);
/** Motifs sonores d'avertissement en modes batterie / hybride (100 <= step < 200), toutes les 100 ms. */
void Hmi_UpdateBuzzer100ms(uint8_t smb_step);
/** Compteur 5 min du buzzer en mode défaut, toutes les 1 s. */
void Hmi_Update1s(uint8_t smb_step);

#endif /* HMI_H */
