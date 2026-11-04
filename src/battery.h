/**
 * @file battery.h
 * @brief Batteries IBS (deux packs) : décodage des trames CAN, keep-alive, watchdogs,
 *        agrégation (SOC, courant) et supervisions (SOC, surintensité, surchauffe,
 *        surtension bus, SOP).
 */
#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>
#include "generic.h"
#include "mcc_generated_files/can_types.h"

#define BATTERY_NB 2

/** Données d'un pack IBS. */
typedef struct
{
    int16_t output_current;      /**< courant (0.01 A), positif en décharge */
    int8_t avg_cell_temperature; /**< température moyenne cellules (1 °C) */
    int8_t max_cell_temperature; /**< température max cellules (1 °C) */
    int8_t max_board_temperature;/**< température max carte BMS (1 °C) */
    uint8_t soc;                 /**< % */
    uint8_t sop;                 /**< % */
    bool system_running_flag;    /**< 0 : standby / 1 : en fonctionnement */
    bool balancing_flag;
    bool low_soc_warning_flag;
    bool error_flag;             /**< au moins une erreur IBS */
    sTimedFlag8 watchdog;        /**< perte de communication (1 s) */
    int16_t max_charge_current;  /**< 0.01 A, négatif (convention IBS) */
    int16_t max_discharge_current;
} sIBS;

typedef struct
{
    sIBS ibs[BATTERY_NB];

    /* Agrégats (Battery_Aggregate100ms) */
    uint8_t soc;              /**< moyenne des SOC (%) */
    int16_t output_current;   /**< somme des courants (0.01 A) */
    int8_t max_temperature;   /**< max des températures cellules (1 °C) */

    /* Supervisions (Battery_Supervise100ms) */
    bool soc_high_level_warning_flag;
    bool soc_very_very_high_level_error_flag;
    bool soc_low_level_warning_flag;
    bool soc_very_low_level_error_flag;
    bool overcurrent_warning_flag;
    sTimedFlag8 overcurrent;
    bool overtemperature_charge_warning_flag;
    sTimedFlag8 overtemperature_charge;
    bool overtemperature_discharge_warning_flag;
    sTimedFlag8 overtemperature_discharge;
    bool overvoltage_warning_flag;
    sTimedFlag8 overvoltage;
    bool sop_warning_flag;
    bool sop_error_flag;

    bool keep_alive_index;    /**< alterne à chaque keep-alive */
} sBatteryPack;

extern sBatteryPack battery;

void Battery_Init(void);
/** Décode les trames 0x18FFxxD0 / 0x18FFxxD1. @return true si l'ID appartient aux batteries */
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
