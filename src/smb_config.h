/**
 * @file smb_config.h
 * @brief Constantes "métier" du firmware SMB BOXHY : version, seuils, temporisations.
 *
 * Toutes les valeurs reproduisent celles du firmware v1.3.0 d'origine.
 * Les temporisations sont exprimées dans l'unité de la boucle qui les évalue
 * (ticks de 100 ms ou de 1 s), comme indiqué en commentaire.
 *
 * Convention des temporisations : un compteur est incrémenté à chaque tick tant que la condition
 * est vraie et l'action se déclenche quand il DÉPASSE le seuil (cpt > TIMEOUT). La durée réelle est
 * donc (TIMEOUT + 1) ticks, par exemple 50 ticks de 100 ms -> environ 5,1 s.
 *
 * Unités internes du firmware (identiques aux unités brutes de la base CAN) :
 * courants en 0,01 A, tensions en 0,01 V, températures en °C, SOC / SOP en %.
 */
#ifndef SMB_CONFIG_H
#define SMB_CONFIG_H

/* ---- Version logicielle (trame CAN 0x2C00004) ------------------------------ */
/* TODO(bug 6) : le firmware est la v1.3.0 mais la télémétrie annonce 1.2.7 (valeur d'origine conservée). */
#define SMB_VERSION                    1
#define SMB_SUBVERSION                 2
#define SMB_SUBSUBVERSION              7

/* ---- Watchdogs de communication (ticks de 1 s) ---------------------------- */
/* Un équipement est déclaré perdu s'il n'a envoyé aucune trame depuis TIMEOUT + 1 secondes. */
#define WATCHDOG_TIMEOUT               30   /* IBS et FCCB : ~31 s */
#define DCAC_WATCHDOG_TIMEOUT          180  /* onduleur : perte totale (~3 min), erreur bloquante */
#define DCAC_RECEIVE_TIMEOUT           5    /* onduleur : pas de réponse QMOD (~6 s), info */

/* ---- Batterie : niveaux de SOC (%) ----------------------------------------- */
#define BATTERY_SOC_VERY_LOW_LEVEL             10   /* sous ce seuil : erreur critique après temporisation */
#define BATTERY_SOC_LOW_LEVEL                  25   /* avertissement (bips) ; seuil de reconnexion onduleur */
#define BATTERY_SOC_HIGH_LEVEL                 70   /* au-dessus : mode hybride refusé */
#define BATTERY_SOC_VERY_HIGH_LEVEL            85   /* en mode hybride : arrêt de la pile */
#define BATTERY_SOC_VERY_VERY_HIGH_LEVEL       95   /* avertissement SOC haut (TODO(bug 2)) */
#define BATTERY_SOC_VERY_VERY_HIGH_TIMEOUT     50   /* ticks 100 ms */
#define BATTERY_SOC_VERY_LOW_TIMEOUT           50   /* ticks 100 ms (~5 s) */

/* ---- Batterie : surintensité (0.01 A) -------------------------------------- */
#define BATTERY_OVERCURRENT_DISCHARGE_LEVEL    6900 /* 69 A en décharge, par batterie */
#define BATTERY_OVERCURRENT_CHARGE_RATIO       0.85 /* charge : alarme au-delà de 85 % de la limite IBS */
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
/* SOP (State Of Power) : puissance disponible annoncée par l'IBS. Surveillé à partir de SMB_STANDBY. */
#define BATTERY_SOP_WARNING_LOW_LEVEL          30   /* avertissement (bips) */
#define BATTERY_SOP_WARNING_VERY_LOW_LEVEL     20   /* erreur critique, sans temporisation */

/* ---- Onduleur : puissance apparente de sortie (VA) ------------------------- */
/* Comparés à la moyenne de puissance apparente sur 1 s (10 réponses QPIGS). */
#define AC_OUTPUT_POWER_HIGH_LEVEL                     5000 /* avertissement (bips) */
#define AC_OUTPUT_POWER_VERY_HIGH_LEVEL                5200 /* surcharge : erreur après ~9 s */
#define AC_OUTPUT_POWER_VERY_CRITICAL_HIGH_LEVEL       7200 /* surcharge forte : erreur après ~4 s */
#define AC_OUTPUT_POWER_VERY_HIGH_TIMEOUT              90   /* ticks 100 ms */
#define AC_OUTPUT_POWER_VERY_CRITICAL_HIGH_TIMEOUT     40   /* ticks 100 ms */

/* ---- Onduleur : déconnexion sur SOC bas ------------------------------------ */
/* L'onduleur est déconnecté si le SOC passe sous 20 % alors qu'une charge est présente,
 * puis reconnecté quand le SOC remonte à BATTERY_SOC_LOW_LEVEL (hystérésis 20 % / 25 %). */
#define DCAC_SOC_DISCONNECT_LEVEL              20   /* % */
#define DCAC_SOC_DISCONNECT_POWER_LEVEL        50   /* VA : en dessous, pas de charge */
#define DCAC_SOC_DISCONNECT_TIMEOUT            50   /* ticks 100 ms (~5 s) */

/* ---- Pile à combustible --------------------------------------------------- */
#define FCS_LOW_CURRENT_LEVEL                  1000 /* 0.01 A : 10 A */
#define FCS_LOW_CURRENT_TIMEOUT                600  /* ticks 100 ms, compteur 8 bits (voir TODO(bug 10)) */
#define FCS_START_2_TIMEOUT                    600  /* ticks 100 ms : ~60 s pour OPERATING */

/* ---- Gestion énergie (consigne de courant pile, 0.01 A) -------------------- */
#define EMS_FCS_CURRENT_MIN                    800  /* 8 A : consigne minimale envoyée à la FCCB */
#define EMS_FCS_CURRENT_MAX                    5500 /* 55 A : consigne maximale */
#define EMS_FCS_CURRENT_MARGIN                 2000 /* 20 A : recharge visée en plus de la conso */
#define EMS_BATTERY_CHARGE_RATIO               0.7  /* part de la limite de charge IBS */

/* ---- Refroidissement partie AC -------------------------------------------- */
/* Températures de consigne (°C) des trois régulateurs PI ; la sortie est un rapport cyclique (%). */
#define COOLING_AMBIANT_TARGET                 40.0f
#define COOLING_INVERTER_TARGET                55.0f
#define COOLING_DCDC_TARGET                    45.0f
#define COOLING_PID_KP                         1.0f /* % par °C d'écart */
#define COOLING_PID_KI                         0.1f /* % par °C et par seconde */
#define COOLING_PID_KD                         0.0f /* terme dérivé non utilisé */
#define COOLING_FANS_MIN_DUTY                  15   /* % : ventilation minimale permanente */
#define COOLING_FANS_MAX_DUTY                  100

/* ---- Sondes CTN (°C) ------------------------------------------------------- */
/* Seuils appliqués à l'initialisation (voir Measures_Init) ; l'erreur haute ou basse est déclarée
 * après CTN_SENSOR_LEVEL_TIMEOUT (ctn_sensor.h), l'avertissement haut est immédiat. */
#define CTN_AC_AMBIANT_HIGH_LEVEL              75   /* sonde 1 : ambiance partie AC */
#define CTN_AC_AMBIANT_LOW_LEVEL               0
#define CTN_AC_AMBIANT_WARNING_HIGH_LEVEL      50
#define CTN_TEMP2_HIGH_LEVEL                   90   /* sonde 2 */
#define CTN_TEMP2_LOW_LEVEL                    0
#define CTN_TEMP2_WARNING_HIGH_LEVEL           50

/* ---- Mesures analogiques --------------------------------------------------- */
#define ADC_SAMPLE_NB                          50    /* échantillons moyennés par voie */
#define SUPPLY_VOLTAGE_GAIN                    1.969 /* points ADC -> 0,01 V (alim 24 V) */
#define BUS_VOLTAGE_GAIN                       1.962 /* points ADC -> 0,01 V (pont diviseur bus DC) */

/* ---- IHM -------------------------------------------------------------------- */
#define BUTTON_DEBOUNCE_THRESHOLD              4    /* ticks 100 ms, boutons de mode (~0,5 s) */
#define BUTTON_MIDDLE_HOLD_THRESHOLD           50   /* ticks 100 ms, bouton central (~5 s) */
#define BUZZER_5_MINUTES_TIMEOUT               300  /* ticks 1 s : buzzer coupé après 5 min */

#endif /* SMB_CONFIG_H */
