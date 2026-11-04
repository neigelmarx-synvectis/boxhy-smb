/**
 * @file fcs.h
 * @brief Pile à combustible (FCS) via la carte FCCB : état et mesures reçus par CAN,
 *        commandes émises par CAN, watchdog et temporisations de démarrage.
 *
 * La pile à combustible (Fuel Cell System) est pilotée par sa propre carte, la FCCB, qui
 * dialogue avec la SMB sur le bus CAN :
 *  - FCCB -> SMB : état et drapeaux (0x2FC0000), températures (0x2FC0003), infos purge et
 *    court-circuit (0x2FC0004) ;
 *  - SMB -> FCCB : commandes marche / arrêt / maintenance / RAZ (0x2FC1001), consigne de courant
 *    (0x2FC1002). Le message de présence 0x2FC1000 est émis par can_tx.c.
 *
 * Appels :
 *  - Fcs_HandleCanFrame() : réception CAN (can_rx.c) ;
 *  - Fcs_UpdateGeneralError() : machine d'états, à chaque tour de boucle ;
 *  - Fcs_Update100ms() / Fcs_Update1s() : tâches périodiques de main.c ;
 *  - Fcs_Send*() : machine d'états (marche / arrêt) et ems.c (consigne de courant).
 */
#ifndef FCS_H
#define FCS_H

#include <stdint.h>
#include <stdbool.h>
#include "src/utils/generic.h"
#include "mcc_generated_files/can_types.h"

/* États de la carte FCCB (octet 0 de la trame 0x2FC0000) */
#define FCCB_INIT           10  /**< initialisation */
#define FCCB_AUTOCHECK      20  /**< auto-contrôle */
#define FCCB_STANDBY        30  /**< pile à l'arrêt, prête à démarrer */
#define FCCB_MAINTENANCE    40  /**< mode maintenance */
#define FCCB_START_PURGE    50  /**< démarrage : purge */
#define FCCB_VOLTAGE_CHECK  52  /**< démarrage : contrôle de tension */
#define FCCB_RAMP_UP        54  /**< démarrage : montée en courant */
#define FCCB_WARM_UP        56  /**< démarrage : chauffe */
#define FCCB_OPERATING      100 /**< pile en production */
#define FCCB_SHUTDOWN       200 /**< arrêt en cours */
#define FCCB_EMERGENCY      250 /**< défaut FCCB */

/** État de la pile vu par la SMB. */
typedef struct
{
    /* Reçu de la FCCB */
    uint8_t step;                       /**< état FCCB (FCCB_*) */
    int16_t output_current;             /**< courant pile (0.01 A) */
    uint8_t dcdc_temperature;           /**< température DC/DC (1 °C), utilisée par cooling.c */
    uint8_t short_circuit_interval_cnt; /**< intervalle entre courts-circuits (0,1 s), informatif */
    bool critical_error_flag;           /**< erreur critique signalée par la FCCB */
    bool minor_error_flag;              /**< erreur mineure signalée par la FCCB */
    bool ppmh2_error_flag;              /**< fuite H2 : erreur critique système */
    bool low_current_warning_flag;      /**< avertissement courant pile bas (FCCB) */
    bool dcdc_temperature_warning_flag; /**< avertissement température DC/DC haute (FCCB) */
    bool short_circuit_ongoing_flag;    /**< court-circuit de régénération en cours : pas de consigne */

    /* Calculé */
    sTimedFlag8 watchdog;               /**< perte de communication FCCB (1 s) */
    bool general_error_flag;            /**< startup || critical || minor || watchdog */
    /** courant pile < FCS_LOW_CURRENT_LEVEL trop longtemps (ticks 100 ms) ;
     *  .error acquitté par la machine d'états */
    sTimedFlag8 low_current_timeout;
    /** durée passée en SMB_FCS_START_2 (ticks 100 ms) ; .error acquitté par la machine d'états */
    sTimedFlag16 start_2_timeout;

    /* Dialogue avec la machine d'états (écrits par smb_state_machine) */
    bool startup_error_flag;            /**< échec du démarrage pile */
    bool startup_retry_flag;            /**< un second essai de démarrage a déjà été fait */
} sFcs;

/** État de la pile, lu par les autres modules. */
extern sFcs fcs;

/** Remet à zéro l'état pile et la structure de commande. */
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
/** Demande de démarrage de la pile (DLC 3). */
void Fcs_SendStart(void);
/** Demande d'arrêt normal de la pile (DLC 3). */
void Fcs_SendStop(void);
/** Demande d'arrêt critique (immédiat) de la pile (DLC 3). */
void Fcs_SendCriticalStop(void);
/** Passage de la FCCB en mode maintenance (DLC 8, clé 0x82 et confirmation 0xFF). */
void Fcs_SendMaintenanceMode(void);
/** Demande de remise à zéro des erreurs FCCB (DLC 8, confirmation 0xFF). */
void Fcs_SendResetErrors(void);
/** Consigne de courant pile. @param current_limit courant maximal autorisé (0,01 A) */
void Fcs_SendCurrentLimit(uint16_t current_limit);

/** Commande RAZ erreurs + effacement des erreurs startup/critical/minor/watchdog/general (bouton hybride). */
void Fcs_ResetErrors(void);
/** Fcs_ResetErrors() + ppmh2, retry, start_2 timeout (sortie du mode défaut). */
void Fcs_ResetAllErrors(void);

#endif /* FCS_H */
