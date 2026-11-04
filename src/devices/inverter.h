/**
 * @file inverter.h
 * @brief Onduleur DC/AC interrogé en RS232 (protocole QMOD / QPIWS / QPIGS) : réception des
 *        trames, décodage, watchdog, moyenne de puissance de sortie, déconnexion sur SOC bas.
 *
 * L'onduleur convertit le bus DC (batteries + pile) en 230 V AC. Il est relié à l'UART1 de la
 * SMB et répond à trois requêtes texte :
 *  - QMOD  : mode de fonctionnement (une lettre : B, F, H, L, S, P) ;
 *  - QPIWS : 32 caractères '0'/'1', un par avertissement ou défaut ;
 *  - QPIGS : 106 caractères, mesures décimales séparées par des espaces (tension, puissances,
 *            température...) et drapeaux d'état.
 * Une réponse commence par '(' et se termine par '\r' ; "(NAK" signale une requête refusée.
 *
 * Appels :
 *  - Inverter_OnRxInterrupt() : ISR de réception UART1, octet par octet ;
 *  - Inverter_ProcessFrame() : boucle principale, quand frame_received est levé ;
 *  - Inverter_Poll300ms() : tâche 300 ms (une requête toutes les quelques périodes) ;
 *  - Inverter_Supervise100ms() / Inverter_UpdatePowerAverage100ms() : tâche 100 ms ;
 *  - Inverter_Update1s() : tâche 1 s (watchdogs de la liaison) ;
 *  - Inverter_UpdateGeneralError(), Inverter_Clear*(), Inverter_ResetErrors() : machine d'états.
 */
#ifndef INVERTER_H
#define INVERTER_H

#include <stdint.h>
#include <stdbool.h>

/* Mode onduleur (réponse QMOD). La machine d'états considère l'onduleur démarré au-delà
 * de INVERTER_STANDBY_MODE. */
#define INVERTER_UNDEFINED_MODE    0
#define INVERTER_FAULT_MODE        1
#define INVERTER_POWER_ON_MODE     2
#define INVERTER_STANDBY_MODE      3
#define INVERTER_AC_SOURCE_MODE    4
#define INVERTER_BATTERY_MODE      5
#define INVERTER_POWER_SAVING_MODE 6

/* Requête en cours : détermine la longueur attendue et le décodage de la réponse */
#define RS232FRAME_NONE  0
#define RS232FRAME_QPIGS 1
#define RS232FRAME_QMOD  2
#define RS232FRAME_QPIWS 3

/** État de l'onduleur vu par la SMB (réponses RS232 et supervisions). */
typedef struct
{
    uint8_t mode; /**< INVERTER_*_MODE */

    /* Avertissements / erreurs onduleur (QPIWS), un caractère '0'/'1' par drapeau. Certaines erreurs
     * (surchauffe, ventilateur, tension batterie, surcharge) ne sont recopiées depuis l'avertissement
     * que lorsque l'onduleur signale un défaut : voir decode_qpiws(). */
    bool error_flag; /**< au moins un défaut onduleur */
    bool bus_overvoltage_error_flag;
    bool bus_undervoltage_error_flag;
    bool bus_soft_fail_error_flag;
    bool line_fail_warning_flag;
    bool opv_short_warning_flag;
    bool voltage_too_low_error_flag;
    bool voltage_too_high_error_flag;
    bool overtemperature_warning_flag;
    bool overtemperature_error_flag;
    bool fan_locked_warning_flag;
    bool fan_locked_error_flag;
    bool battery_voltage_high_warning_flag;
    bool battery_voltage_high_error_flag;
    bool battery_voltage_low_warning_flag;
    bool overcharge_error_flag;
    bool battery_under_shutdown_voltage_warning_flag;
    bool battery_derating_warning_flag;
    bool overload_warning_flag;
    bool overload_error_flag;
    bool eeprom_warning_flag;
    bool over_current_error_flag;
    bool soft_start_error_flag;
    bool self_test_error_flag;
    bool ac_output_dc_voltage_too_high_error_flag;
    bool battery_disconnected_error_flag;
    bool current_sensor_error_flag;
    bool battery_short_circuit_error_flag;
    bool power_limit_warning_flag;
    bool pv_voltage_high_warning_flag;
    bool mppt_overload_error_flag;
    bool mppt_overload_warning_flag;
    bool battery_too_low_to_charge_warning_flag;

    /* Mesures et états (QPIGS) */
    uint16_t ac_output_voltage;        /**< 0.1 V */
    uint16_t ac_output_apparent_power; /**< VA */
    uint16_t ac_output_active_power;   /**< W */
    uint8_t dcac_temp;                 /**< °C (tronqué sur 8 bits) */
    bool load_status_flag;
    bool charging_status_flag;
    bool scc_charging_status_flag;
    bool ac_charging_status_flag;
    bool charging_to_floating_mode_flag;
    bool device_switched_on_flag;

    uint16_t moy_ac_output_apparent_power; /**< moyenne sur 10 x 100 ms (VA), base des supervisions */

    bool toggle_qmod_flag;  /**< basculent à chaque réponse reçue (vie de la liaison) */
    bool toggle_qpiws_flag; /**< idem pour QPIWS */
    bool toggle_qpigs_flag; /**< idem pour QPIGS */

    /* Supervision */
    bool watchdog_error_flag;     /**< aucune réponse depuis DCAC_WATCHDOG_TIMEOUT */
    bool recieve_flag;            /**< pas de réponse QMOD depuis DCAC_RECEIVE_TIMEOUT */
    bool general_error_flag;      /**< watchdog || error_flag || sonde ambiance ; verrouillé */
    bool soc_disconnect_error_flag;        /**< déconnexion onduleur sur SOC bas */
    bool apparent_power_very_high_error_flag; /**< surcharge de sortie prolongée (verrouillée) */
    bool apparent_power_high_warning_flag;    /**< puissance moyenne >= AC_OUTPUT_POWER_HIGH_LEVEL */
    /** force une dernière émission des trames onduleur après arrêt (acquitté par can_tx) */
    bool raz_data_after_stop_flag;

    volatile bool frame_received;  /**< trame complète disponible (ISR -> boucle principale) */
} sInverter;

/** État de l'onduleur, lu par les autres modules. */
extern sInverter inverter;

/** Remet à zéro l'état onduleur, la réception RS232, le polling et les supervisions. */
void Inverter_Init(void);
/** Corps de l'interruption de réception UART1. */
void Inverter_OnRxInterrupt(void);
/** Assemblage d'une trame à partir d'un octet reçu (appelé par l'ISR). */
void Inverter_OnRxByte(uint8_t byte);
/** Décodage de la trame reçue (boucle principale, quand frame_received). */
void Inverter_ProcessFrame(void);
/**
 * Émission cyclique des requêtes QMOD / QPIWS / QPIGS, toutes les 300 ms.
 * @param smb_step état SMB : le polling n'a lieu que onduleur alimenté (31 à 199)
 */
void Inverter_Poll300ms(uint8_t smb_step);
/**
 * Déconnexion sur SOC bas et surcharge de sortie, toutes les 100 ms
 * (utilise la moyenne du cycle précédent).
 * @param battery_soc SOC moyen des batteries (%)
 */
void Inverter_Supervise100ms(uint8_t battery_soc);
/** Moyenne de puissance apparente et avertissement puissance haute, toutes les 100 ms. */
void Inverter_UpdatePowerAverage100ms(void);
/**
 * Watchdogs de la liaison, toutes les 1 s (actifs en mode batterie / hybride seulement).
 * @param smb_step état SMB courant
 */
void Inverter_Update1s(uint8_t smb_step);
/**
 * Agrégation general_error_flag (chaque tour de boucle).
 * @param ambiant_temp_error erreur sonde ambiance AC
 */
void Inverter_UpdateGeneralError(bool ambiant_temp_error);
/** Efface mode et mesures (mode défaut). */
void Inverter_ClearData(void);
/** Efface mode et mesures et demande une dernière émission CAN (arrêt onduleur). */
void Inverter_ClearDataAfterStop(void);
/** Acquittement des erreurs (sortie du mode défaut). */
void Inverter_ResetErrors(void);

#endif /* INVERTER_H */
