/**
 * @file fcs.h
 * @brief Pile à combustible (FCS) via la carte FCCB : état et mesures reçus par CAN,
 *        commandes émises par CAN, watchdog et temporisations de démarrage.
 */
#ifndef FCS_H
#define FCS_H

#include <stdint.h>
#include <stdbool.h>
#include "src/utils/generic.h"
#include "mcc_generated_files/can_types.h"

/* États de la carte FCCB (octet 0 de la trame 0x2FC0000) */
#define FCCB_INIT           10
#define FCCB_AUTOCHECK      20
#define FCCB_STANDBY        30
#define FCCB_MAINTENANCE    40
#define FCCB_START_PURGE    50
#define FCCB_VOLTAGE_CHECK  52
#define FCCB_RAMP_UP        54
#define FCCB_WARM_UP        56
#define FCCB_OPERATING      100
#define FCCB_SHUTDOWN       200
#define FCCB_EMERGENCY      250

typedef struct
{
    /* Reçu de la FCCB */
    uint8_t step;                       /**< état FCCB */
    int16_t output_current;             /**< courant pile (0.01 A) */
    uint8_t dcdc_temperature;           /**< température DC/DC (1 °C) */
    uint8_t short_circuit_interval_cnt;
    bool critical_error_flag;
    bool minor_error_flag;
    bool ppmh2_error_flag;              /**< fuite H2 : erreur critique système */
    bool low_current_warning_flag;
    bool dcdc_temperature_warning_flag;
    bool short_circuit_ongoing_flag;

    /* Calculé */
    sTimedFlag8 watchdog;               /**< perte de communication FCCB (1 s) */
    bool general_error_flag;            /**< startup || critical || minor || watchdog */
    sTimedFlag8 low_current_timeout;    /**< courant pile < FCS_LOW_CURRENT_LEVEL trop longtemps (100 ms) ; .error acquitté par la machine d'états */
    sTimedFlag16 start_2_timeout;       /**< durée en SMB_FCS_START_2 (100 ms) ; .error acquitté par la machine d'états */

    /* Dialogue avec la machine d'états (écrits par smb_state_machine) */
    bool startup_error_flag;
    bool startup_retry_flag;
} sFcs;

extern sFcs fcs;

void Fcs_Init(void);
/** Décode les trames 0x2FC0000 / 0x2FC0003 / 0x2FC0004. @return true si l'ID a été traité */
bool Fcs_HandleCanFrame(const CAN_MSG_OBJ *msg);
/** Agrégation general_error_flag (chaque tour de boucle). */
void Fcs_UpdateGeneralError(void);
/** Temporisations 100 ms. @param in_start_2_state machine d'états en SMB_FCS_START_2 */
void Fcs_Update100ms(bool in_start_2_state);
/** Watchdog, toutes les 1 s. */
void Fcs_Update1s(void);

/* Commandes CAN vers la FCCB (0x2FC1001 / 0x2FC1002) */
void Fcs_SendStart(void);
void Fcs_SendStop(void);
void Fcs_SendCriticalStop(void);
void Fcs_SendMaintenanceMode(void);
void Fcs_SendResetErrors(void);
void Fcs_SendCurrentLimit(uint16_t current_limit);

/** Commande RAZ erreurs + effacement des erreurs startup/critical/minor/watchdog/general (bouton hybride). */
void Fcs_ResetErrors(void);
/** Fcs_ResetErrors() + ppmh2, retry, start_2 timeout (sortie du mode défaut). */
void Fcs_ResetAllErrors(void);

#endif /* FCS_H */
