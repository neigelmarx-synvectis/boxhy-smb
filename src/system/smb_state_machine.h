/**
 * @file smb_state_machine.h
 * @brief Machine d'états principale de la SMB : séquences de démarrage / arrêt de l'onduleur
 *        et de la pile, modes batterie et hybride, maintenance, mode défaut.
 *
 * Parcours nominal :
 * @verbatim
 *  INIT -> AUTOCHECK -> STANDBY --(bouton batterie)--> DCAC_START_1 -> DCAC_START_2 -> DCAC_START_3
 *       -> BATTERY_MODE --(bouton hybride, SOC <= 70 %)--> FCS_START_1 -> FCS_START_2 -> HYBRID_MODE
 *  HYBRID_MODE --(fin du mode hybride)--> FC_STOP_1 -> FC_STOP_2 -> BATTERY_MODE
 *  BATTERY_MODE --(aucun bouton)--> STOPPING -> STANDBY
 *  STANDBY --(demande CAN)--> MAINTENANCE --(fin de demande)--> STOPPING
 *  tout état --(erreur critique)--> EMERGENCY --(bouton central ou ordre CAN)--> AUTOCHECK
 * @endverbatim
 *
 * Smb_Run() est appelée à chaque tour de la boucle principale (sans attente). Les durées des
 * états sont mesurées avec timebase.ticks_100ms, remis à zéro à chaque changement d'état.
 */
#ifndef SMB_STATE_MACHINE_H
#define SMB_STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>

/* États SMB (octet 0 de la trame 0x2C00000). Les valeurs sont celles du firmware d'origine,
 * reprises dans la base CAN (VAL_ du signal STEP) : ne pas les renuméroter. */
#define SMB_INIT          10  /**< mise sous tension, relais en sécurité (2 s) */
#define SMB_AUTOCHECK     20  /**< attente de 3 s sans erreur avant le standby */
#define SMB_STANDBY       30  /**< repos : attente du bouton mode batterie ou d'une demande de maintenance */
#define SMB_MAINTENANCE   40  /**< commandes manuelles des sorties par CAN */
#define SMB_DCAC_START_1  50  /**< précharge de l'onduleur (3 s) */
#define SMB_DCAC_START_2  52  /**< bascule précharge -> contacteur direct (0,5 s) */
#define SMB_DCAC_START_3  54  /**< attente du démarrage de l'onduleur (15 s max) */
#define SMB_BATTERY_MODE  100 /**< onduleur alimenté par les batteries seules */
#define SMB_FCS_START_1   120 /**< attente de la FCCB en standby, envoi de l'ordre de démarrage */
#define SMB_FCS_START_2   122 /**< démarrage de la pile en cours (60 s max, une nouvelle tentative) */
#define SMB_HYBRID_MODE   150 /**< pile + batteries ; consigne de courant pile envoyée chaque seconde */
#define SMB_FC_STOP_1     200 /**< arrêt pile : ordre d'arrêt après 200 ms */
#define SMB_FC_STOP_2     201 /**< arrêt pile : attente du retour de la FCCB en standby */
#define SMB_DCAC_STOP     210 /**< arrêt de l'onduleur seul (inatteignable, conservé de l'origine) */
#define SMB_STOPPING      240 /**< arrêt complet séquencé (5 s) puis retour en standby */
#define SMB_EMERGENCY     250 /**< mode défaut : relais en sécurité, buzzer, attente d'acquittement */

/** État de la machine d'états, lu par la télémétrie (trame SMB_TX_GENERAL) et par les modules. */
typedef struct
{
    uint8_t step;             /**< état courant (SMB_*) */
    bool critical_error_flag; /**< erreur critique (verrouillée jusqu'à l'acquittement en mode défaut) */
    bool software_error_flag; /**< état inconnu rencontré */
    bool reset_order_flag;    /**< ordre d'acquittement du défaut reçu par CAN */
} sSmb;

/** Instance unique de la machine d'états. */
extern sSmb smb;

/** Démarre en SMB_INIT avec toutes les erreurs effacées et le chronomètre à zéro. */
void Smb_Init(void);
/** Agrégation des erreurs puis exécution de l'état courant (chaque tour de boucle). */
void Smb_Run(void);
/** Ordre d'acquittement du défaut (télémétrie 0x2C01009). */
void Smb_SetResetOrder(bool order);

#endif /* SMB_STATE_MACHINE_H */
