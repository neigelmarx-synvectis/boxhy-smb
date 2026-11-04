/**
 * @file battery.h
 * @brief Batteries IBS (deux packs) : décodage des trames CAN, keep-alive, watchdogs,
 *        agrégation (SOC, courant) et supervisions (SOC, surintensité, surchauffe,
 *        surtension bus, SOP).
 *
 * Le système BOXHY embarque deux batteries lithium IBS (index 0 et 1) reliées au bus CAN.
 * Chaque IBS émet périodiquement ses mesures (trames 0x18FFxxD0 pour la batterie 0,
 * 0x18FFxxD1 pour la batterie 1) et attend un keep-alive de la SMB chaque seconde.
 *
 * Appels :
 *  - Battery_HandleCanFrame() : depuis la réception CAN (can_rx.c), à chaque trame reçue ;
 *  - Battery_Supervise100ms() puis Battery_Aggregate100ms() : tâche 100 ms de main.c ;
 *  - Battery_Update1s() : tâche 1 s de main.c (keep-alive et watchdogs) ;
 *  - Battery_HasCriticalError() / Battery_ResetErrors() : machine d'états SMB.
 *
 * Unités firmware : courants en 0,01 A (positif = décharge), températures en °C,
 * SOC et SOP en %.
 */
#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>
#include "src/utils/generic.h"
#include "mcc_generated_files/can_types.h"

/** Nombre de packs IBS gérés par la SMB. */
#define BATTERY_NB 2

/** Données d'un pack IBS, mises à jour par les trames CAN reçues. */
typedef struct
{
    int16_t output_current;      /**< courant (0.01 A), positif en décharge */
    int8_t avg_cell_temperature; /**< température moyenne cellules (1 °C) */
    int8_t max_cell_temperature; /**< température max cellules (1 °C) */
    int8_t max_board_temperature;/**< température max carte BMS (1 °C) */
    uint8_t soc;                 /**< état de charge (%) */
    uint8_t sop;                 /**< état de puissance disponible (%) */
    bool system_running_flag;    /**< 0 : standby / 1 : en fonctionnement */
    bool balancing_flag;         /**< équilibrage des cellules en cours */
    bool low_soc_warning_flag;   /**< avertissement SOC bas émis par l'IBS */
    bool error_flag;             /**< au moins une erreur IBS */
    sTimedFlag8 watchdog;        /**< perte de communication (s), réarmé par la trame courant */
    int16_t max_charge_current;  /**< 0.01 A, négatif (convention IBS) */
    int16_t max_discharge_current; /**< 0.01 A, limite de décharge annoncée par l'IBS */
} sIBS;

/** Ensemble des deux batteries : données brutes, agrégats et résultats des supervisions. */
typedef struct
{
    sIBS ibs[BATTERY_NB]; /**< données par pack (index 0 = trames xxD0, 1 = trames xxD1) */

    /* Agrégats (Battery_Aggregate100ms) */
    uint8_t soc;              /**< moyenne des SOC (%) */
    int16_t output_current;   /**< somme des courants (0.01 A) */
    int8_t max_temperature;   /**< max des températures cellules (1 °C) */

    /* Supervisions (Battery_Supervise100ms). Les avertissements suivent la condition ;
     * les erreurs (sTimedFlag8.error) sont verrouillées jusqu'à Battery_ResetErrors(). */
    bool soc_high_level_warning_flag;         /**< SOC > BATTERY_SOC_VERY_VERY_HIGH_LEVEL (voir bug 2) */
    bool soc_very_very_high_level_error_flag; /**< jamais levée (bug 2) */
    bool soc_low_level_warning_flag;          /**< SOC < BATTERY_SOC_LOW_LEVEL */
    bool soc_very_low_level_error_flag;       /**< SOC < BATTERY_SOC_VERY_LOW_LEVEL trop longtemps */
    bool overcurrent_warning_flag;            /**< courant hors limites sur au moins un pack */
    sTimedFlag8 overcurrent;                  /**< erreur surintensité temporisée */
    bool overtemperature_charge_warning_flag; /**< cellules trop chaudes pendant une charge */
    sTimedFlag8 overtemperature_charge;       /**< erreur surchauffe en charge temporisée */
    bool overtemperature_discharge_warning_flag; /**< cellules trop chaudes pendant une décharge */
    sTimedFlag8 overtemperature_discharge;    /**< erreur surchauffe en décharge temporisée */
    bool overvoltage_warning_flag;            /**< tension bus > BATTERY_OVERVOLTAGE_LEVEL */
    sTimedFlag8 overvoltage;                  /**< erreur surtension bus temporisée */
    bool sop_warning_flag;                    /**< SOP d'un pack < BATTERY_SOP_WARNING_LOW_LEVEL */
    bool sop_error_flag;                      /**< SOP d'un pack < BATTERY_SOP_WARNING_VERY_LOW_LEVEL */

    bool keep_alive_index;    /**< alterne à chaque keep-alive : batterie destinataire de la trame */
} sBatteryPack;

/** État des batteries, lu par les autres modules (écriture réservée à battery.c). */
extern sBatteryPack battery;

/** Remet à zéro toutes les données batteries et les compteurs de supervision. */
void Battery_Init(void);
/**
 * Décode les trames 0x18FFxxD0 / 0x18FFxxD1 (via les tables de liaison CAN).
 * @return true si l'ID appartient aux batteries (même s'il n'est pas exploité)
 */
bool Battery_HandleCanFrame(const CAN_MSG_OBJ *msg);
/**
 * Supervisions toutes les 100 ms (utilise les agrégats du cycle précédent, comme à l'origine).
 * @param smb_step    état SMB courant (le SOP n'est surveillé qu'à partir de SMB_STANDBY)
 * @param bus_voltage tension bus mesurée (0.01 V)
 */
void Battery_Supervise100ms(uint8_t smb_step, uint16_t bus_voltage);
/** Calcul du SOC moyen et du courant total, toutes les 100 ms. */
void Battery_Aggregate100ms(void);
/** Keep-alive IBS et watchdogs, toutes les 1 s. */
void Battery_Update1s(void);
/** Acquittement des erreurs (sortie du mode défaut). */
void Battery_ResetErrors(void);
/** Une erreur batterie justifiant le passage en SMB_EMERGENCY est présente. */
bool Battery_HasCriticalError(void);

#endif /* BATTERY_H */
