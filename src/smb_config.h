/**
 * @file smb_config.h
 * @brief Constantes "métier" du firmware SMB BOXHY : version, seuils, temporisations.
 *
 * Toutes les valeurs reproduisent celles du firmware v1.3.0 d'origine.
 * Les temporisations sont exprimées dans l'unité de la boucle qui les évalue
 * (ticks de 100 ms ou de 1 s), comme indiqué en commentaire.
 */
#ifndef SMB_CONFIG_H
#define SMB_CONFIG_H

/* ---- Version logicielle (trame CAN 0x2C00004) ------------------------------ */
/* TODO(bug 6) : le firmware est la v1.3.0 mais la télémétrie annonce 1.2.7 (valeur d'origine conservée). */
#define SMB_VERSION                    1
#define SMB_SUBVERSION                 2
#define SMB_SUBSUBVERSION              7

/* ---- Watchdogs de communication (ticks de 1 s) ---------------------------- */
#define WATCHDOG_TIMEOUT               30   /* IBS et FCCB */
#define DCAC_WATCHDOG_TIMEOUT          180  /* onduleur : perte totale */
#define DCAC_RECEIVE_TIMEOUT           5    /* onduleur : absence de réponse QMOD */

/* ---- Batterie : niveaux de SOC (%) ----------------------------------------- */
#define BATTERY_SOC_VERY_LOW_LEVEL             10
#define BATTERY_SOC_LOW_LEVEL                  25
#define BATTERY_SOC_HIGH_LEVEL                 70
#define BATTERY_SOC_VERY_HIGH_LEVEL            85
#define BATTERY_SOC_VERY_VERY_HIGH_LEVEL       95
#define BATTERY_SOC_VERY_VERY_HIGH_TIMEOUT     50   /* ticks 100 ms */
#define BATTERY_SOC_VERY_LOW_TIMEOUT           50   /* ticks 100 ms */

/* ---- Batterie : surintensité (0.01 A) -------------------------------------- */
#define BATTERY_OVERCURRENT_DISCHARGE_LEVEL    6900
#define BATTERY_OVERCURRENT_CHARGE_RATIO       0.85
#define BATTERY_OVERCURRENT_TIMEOUT            450  /* ticks 100 ms, compteur 8 bits (voir TODO(bug 9)) */

/* ---- Batterie : surchauffe cellules (°C) ------------------------------------ */
#define BATTERY_MAX_CHARGE_TEMPERATURE         45   /* sécurité IBS à 50 °C */
#define BATTERY_OVERTEMPERATURE_CHARGE_TIMEOUT 50   /* ticks 100 ms */
#define BATTERY_MAX_DISCHARGE_TEMPERATURE      55   /* sécurité IBS à 60 °C */
#define BATTERY_OVERTEMPERATURE_DISCHARGE_TIMEOUT 50

/* ---- Batterie : surtension bus (0.01 V) ------------------------------------ */
#define BATTERY_OVERVOLTAGE_LEVEL              6000 /* 60 V */
#define BATTERY_OVERVOLTAGE_TIMEOUT            50   /* ticks 100 ms */

/* ---- Batterie : SOP (%) ---------------------------------------------------- */
#define BATTERY_SOP_WARNING_LOW_LEVEL          30
#define BATTERY_SOP_WARNING_VERY_LOW_LEVEL     20

/* ---- Onduleur : puissance apparente de sortie (VA) ------------------------- */
#define AC_OUTPUT_POWER_HIGH_LEVEL                     5000
#define AC_OUTPUT_POWER_VERY_HIGH_LEVEL                5200
#define AC_OUTPUT_POWER_VERY_CRITICAL_HIGH_LEVEL       7200
#define AC_OUTPUT_POWER_VERY_HIGH_TIMEOUT              90   /* ticks 100 ms */
#define AC_OUTPUT_POWER_VERY_CRITICAL_HIGH_TIMEOUT     40   /* ticks 100 ms */

/* ---- Onduleur : déconnexion sur SOC bas ------------------------------------ */
#define DCAC_SOC_DISCONNECT_LEVEL              20   /* % */
#define DCAC_SOC_DISCONNECT_POWER_LEVEL        50   /* VA */
#define DCAC_SOC_DISCONNECT_TIMEOUT            50   /* ticks 100 ms */

/* ---- Pile à combustible --------------------------------------------------- */
#define FCS_LOW_CURRENT_LEVEL                  1000 /* 0.01 A */
#define FCS_LOW_CURRENT_TIMEOUT                600  /* ticks 100 ms, compteur 8 bits (voir TODO(bug 10)) */
#define FCS_START_2_TIMEOUT                    600  /* ticks 100 ms */

/* ---- Gestion énergie (consigne de courant pile, 0.01 A) -------------------- */
#define EMS_FCS_CURRENT_MIN                    800
#define EMS_FCS_CURRENT_MAX                    5500
#define EMS_FCS_CURRENT_MARGIN                 2000
#define EMS_BATTERY_CHARGE_RATIO               0.7

/* ---- Refroidissement partie AC -------------------------------------------- */
#define COOLING_AMBIANT_TARGET                 40.0f
#define COOLING_INVERTER_TARGET                55.0f
#define COOLING_DCDC_TARGET                    45.0f
#define COOLING_PID_KP                         1.0f
#define COOLING_PID_KI                         0.1f
#define COOLING_PID_KD                         0.0f
#define COOLING_FANS_MIN_DUTY                  15
#define COOLING_FANS_MAX_DUTY                  100

/* ---- Sondes CTN (°C) ------------------------------------------------------- */
#define CTN_AC_AMBIANT_HIGH_LEVEL              75
#define CTN_AC_AMBIANT_LOW_LEVEL               0
#define CTN_AC_AMBIANT_WARNING_HIGH_LEVEL      50
#define CTN_TEMP2_HIGH_LEVEL                   90
#define CTN_TEMP2_LOW_LEVEL                    0
#define CTN_TEMP2_WARNING_HIGH_LEVEL           50

/* ---- Mesures analogiques --------------------------------------------------- */
#define ADC_SAMPLE_NB                          50
#define SUPPLY_VOLTAGE_GAIN                    1.969
#define BUS_VOLTAGE_GAIN                       1.962

/* ---- IHM -------------------------------------------------------------------- */
#define BUTTON_DEBOUNCE_THRESHOLD              4    /* ticks 100 ms, boutons mode batterie / hybride */
#define BUTTON_MIDDLE_HOLD_THRESHOLD           50   /* ticks 100 ms, bouton central (acquittement défaut) */
#define BUZZER_5_MINUTES_TIMEOUT               300  /* ticks 1 s */

#endif /* SMB_CONFIG_H */
