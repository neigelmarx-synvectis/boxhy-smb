/**
 * @file smb_state_machine.h
 * @brief Machine d'états principale de la SMB : séquences de démarrage / arrêt de l'onduleur
 *        et de la pile, modes batterie et hybride, maintenance, mode défaut.
 */
#ifndef SMB_STATE_MACHINE_H
#define SMB_STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>

/* États SMB (octet 0 de la trame 0x2C00000) */
#define SMB_INIT          10
#define SMB_AUTOCHECK     20
#define SMB_STANDBY       30
#define SMB_MAINTENANCE   40
#define SMB_DCAC_START_1  50
#define SMB_DCAC_START_2  52
#define SMB_DCAC_START_3  54
#define SMB_BATTERY_MODE  100
#define SMB_FCS_START_1   120
#define SMB_FCS_START_2   122
#define SMB_HYBRID_MODE   150
#define SMB_FC_STOP_1     200
#define SMB_FC_STOP_2     201
#define SMB_DCAC_STOP     210
#define SMB_STOPPING      240
#define SMB_EMERGENCY     250

typedef struct
{
    uint8_t step;             /**< état courant (SMB_*) */
    bool critical_error_flag; /**< erreur critique (verrouillée jusqu'à l'acquittement en mode défaut) */
    bool software_error_flag; /**< état inconnu rencontré */
    bool reset_order_flag;    /**< ordre d'acquittement du défaut reçu par CAN */
} sSmb;

extern sSmb smb;

void Smb_Init(void);
/** Agrégation des erreurs puis exécution de l'état courant (chaque tour de boucle). */
void Smb_Run(void);
/** Ordre d'acquittement du défaut (télémétrie 0x2C01009). */
void Smb_SetResetOrder(bool order);

#endif /* SMB_STATE_MACHINE_H */
