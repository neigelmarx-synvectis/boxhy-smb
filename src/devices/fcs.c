/**
 * @file fcs.c
 * @brief Pile à combustible via la carte FCCB (voir fcs.h).
 *
 * Contenu :
 *  - tables de liaison des trames reçues de la FCCB (seuls les signaux utiles à la SMB sont liés,
 *    les autres sont déclarés CAN_BIND_NONE) ;
 *  - tables des commandes émises, remplies via la structure privée `cmd` puis codées par
 *    CanMsg_Send() ;
 *  - watchdog de communication (1 s) et temporisations de démarrage (100 ms).
 */
#include <string.h>
#include "src/devices/fcs.h"
#include "src/can/can_db.h"
#include "src/smb_config.h"
#include "hal/can_port.h"

#define FCS_COMMAND_CONFIRM   0xFF /* octet 7 des commandes longues (maintenance, RAZ erreurs) */
#define FCS_MAINTENANCE_KEY   0x82 /* valeur de l'octet « démarrage » qui demande le mode maintenance */

/* État public de la pile (déclaré extern dans fcs.h). */
sFcs fcs;

/* ---- Réception ------------------------------------------------------------------------- */
/* L'alias local M désigne le message en cours de description (préfixe des macros de can_db.h). */

/* FCCB_TX_GENERAL (0x2FC0000) : état FCCB, drapeaux d'erreur, courant de sortie pile.
 * spac_output_current est un U16 en 0,01 A stocké dans un int16 (même comportement que l'origine). */

#define M CANDB_FCCB_TX_GENERAL
static const sCanBinding fccb_general[] CAN_ROM = {
    CAN_BIND(CANDB_FCCB_TX_GENERAL__STEP, &fcs.step, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_FCCB_TX_GENERAL__CRITICAL_ERROR_FLAG, &fcs.critical_error_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_FCCB_TX_GENERAL__MINOR_ERROR_FLAG, &fcs.minor_error_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_FCCB_TX_GENERAL__PPMH2_ERROR_FLAG, &fcs.ppmh2_error_flag, CAN_VAR_BOOL, 1),
    CAN_BIND_NONE(CANDB_FCCB_TX_GENERAL__CHECK_H2_SUPPLY_WARNING_FLAG),
    CAN_BIND(CANDB_FCCB_TX_GENERAL__FC_CURRENT_LOW_LEVEL_WARNING_FLAG, &fcs.low_current_warning_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_FCCB_TX_GENERAL__DCDC_TEMPERATURE_HIGH_LEVEL_WARNING_FLAG, &fcs.dcdc_temperature_warning_flag, CAN_VAR_BOOL, 1),
    CAN_BIND_NONE(CANDB_FCCB_TX_GENERAL__PURGE_REQUEST_FLAG),
    CAN_BIND_NONE(CANDB_FCCB_TX_GENERAL__SHORT_CIRCUIT_REQUEST_FLAG),
    CAN_BIND_NONE(CANDB_FCCB_TX_GENERAL__CAN_START_REQUEST_FLAG),
    CAN_BIND_NONE(CANDB_FCCB_TX_GENERAL__CAN_STOP_REQUEST_FLAG),
    CAN_BIND_NONE(CANDB_FCCB_TX_GENERAL__CAN_CRITICAL_STOP_REQUEST_FLAG),
    CAN_BIND_NONE(CANDB_FCCB_TX_GENERAL__INSTANT_H2_CONSUMPTION),
    CAN_BIND(CANDB_FCCB_TX_GENERAL__SHORT_CIRCUIT_ONGOING_FLAG, &fcs.short_circuit_ongoing_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_FCCB_TX_GENERAL__SPAC_OUTPUT_CURRENT, &fcs.output_current, CAN_VAR_I16, 0.01),
};
CAN_CHECK_BINDINGS(M, fccb_general);
#undef M

/* FCCB_TX_TEMPERATURE (0x2FC0003) : seule la température du DC/DC (°C, signée sur le bus mais
 * stockée en uint8 comme à l'origine) sert à la régulation des ventilateurs. */
#define M CANDB_FCCB_TX_TEMPERATURE
static const sCanBinding fccb_temperature[] CAN_ROM = {
    CAN_BIND_NONE(CANDB_FCCB_TX_TEMPERATURE__FC_TOP_TEMPERATURE),
    CAN_BIND_NONE(CANDB_FCCB_TX_TEMPERATURE__FC_BOT_TEMPERATURE),
    CAN_BIND_NONE(CANDB_FCCB_TX_TEMPERATURE__BOARD_TEMPERATURE),
    CAN_BIND(CANDB_FCCB_TX_TEMPERATURE__DCDC_TEMPERATURE, &fcs.dcdc_temperature, CAN_VAR_U8, 1),
    CAN_BIND_NONE(CANDB_FCCB_TX_TEMPERATURE__FC_OPTIMUM_TEMPERATURE),
    CAN_BIND_NONE(CANDB_FCCB_TX_TEMPERATURE__FILTERED_STACK_FANS_DUTY_CYCLE),
    CAN_BIND_NONE(CANDB_FCCB_TX_TEMPERATURE__PPMH2),
    CAN_BIND_NONE(CANDB_FCCB_TX_TEMPERATURE__FC_MEAN_TEMPERATURE),
};
CAN_CHECK_BINDINGS(M, fccb_temperature);
#undef M

/* FCCB_TX_PURGE_CC_INFOS (0x2FC0004) : intervalle entre courts-circuits, en 0,1 s (variable dans
 * la même unité que le signal, donc sans conversion). */
#define M CANDB_FCCB_TX_PURGE_CC_INFOS
static const sCanBinding fccb_purge_cc_infos[] CAN_ROM = {
    CAN_BIND_NONE(CANDB_FCCB_TX_PURGE_CC_INFOS__PURGE_CNT),
    CAN_BIND_NONE(CANDB_FCCB_TX_PURGE_CC_INFOS__SHORT_CIRCUIT_CNT),
    CAN_BIND_NONE(CANDB_FCCB_TX_PURGE_CC_INFOS__PURGE_INTERVAL_CNT),
    CAN_BIND(CANDB_FCCB_TX_PURGE_CC_INFOS__SHORT_CIRCUIT_INTERVAL_CNT, &fcs.short_circuit_interval_cnt, CAN_VAR_U8, 0.1),
    CAN_BIND_NONE(CANDB_FCCB_TX_PURGE_CC_INFOS__INLET_PRESSURE),
};
CAN_CHECK_BINDINGS(M, fccb_purge_cc_infos);
#undef M

/* Trames FCCB décodées par la SMB, recherchées par identifiant dans Fcs_HandleCanFrame(). */
static const sCanMessage fcs_rx_messages[] CAN_ROM = {
    CAN_MESSAGE(CANDB_FCCB_TX_GENERAL, fccb_general),
    CAN_MESSAGE(CANDB_FCCB_TX_TEMPERATURE, fccb_temperature),
    CAN_MESSAGE(CANDB_FCCB_TX_PURGE_CC_INFOS, fccb_purge_cc_infos),
};

/* ---- Commandes ------------------------------------------------------------------------- */

/* Valeurs à émettre, renseignées par send_on_off() / Fcs_SendCurrentLimit() juste avant le codage
 * de la trame. Chaque champ est lié à un signal des trames FCCB_RX_ON_OFF et
 * FCCB_RX_OUPUT_CURRENT_LIMIT. */
static struct
{
    uint8_t critical_stop;
    uint8_t stop;
    uint8_t start;          /* 1 : démarrage ; FCS_MAINTENANCE_KEY : mode maintenance */
    uint8_t reset_errors;
    uint8_t confirm;
    uint16_t current_limit; /* 0,01 A */
} cmd;

/* FCCB_RX_ON_OFF (0x2FC1001) : un octet par commande ; octet 7 = confirmation des commandes longues. */
#define M CANDB_FCCB_RX_ON_OFF
static const sCanBinding fccb_on_off[] CAN_ROM = {
    CAN_BIND(CANDB_FCCB_RX_ON_OFF__CAN_CRITICAL_STOP_REQUEST, &cmd.critical_stop, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_FCCB_RX_ON_OFF__CAN_STOP_REQUEST, &cmd.stop, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_FCCB_RX_ON_OFF__CAN_START_REQUEST, &cmd.start, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_FCCB_RX_ON_OFF__RESET_ERRORS_REQUEST, &cmd.reset_errors, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_FCCB_RX_ON_OFF__COMMAND_CONFIRM, &cmd.confirm, CAN_VAR_U8, 1),
};
CAN_CHECK_BINDINGS(M, fccb_on_off);
static const sCanMessage msg_on_off CAN_ROM = CAN_MESSAGE(M, fccb_on_off);
#undef M

/* FCCB_RX_OUPUT_CURRENT_LIMIT (0x2FC1002) : consigne de courant pile sur 16 bits, en 0,01 A. */
#define M CANDB_FCCB_RX_OUPUT_CURRENT_LIMIT
static const sCanBinding fccb_current_limit[] CAN_ROM = {
    CAN_BIND(CANDB_FCCB_RX_OUPUT_CURRENT_LIMIT__OUTPUT_CURRENT_LIMIT, &cmd.current_limit, CAN_VAR_U16, 0.01),
};
CAN_CHECK_BINDINGS(M, fccb_current_limit);
static const sCanMessage msg_current_limit CAN_ROM = CAN_MESSAGE(M, fccb_current_limit);
#undef M

/* Remet à zéro l'état pile et les dernières commandes. */
void Fcs_Init(void)
{
    memset(&cmd, 0, sizeof(cmd));
    memset(&fcs, 0, sizeof(fcs));
}

/*
 * Décode une trame FCCB si son identifiant est connu. La trame d'état 0x2FC0000 réarme aussi le
 * watchdog de communication et efface une erreur de watchdog déjà levée.
 */
bool Fcs_HandleCanFrame(const CAN_MSG_OBJ *msg)
{
    const sCanMessage *m = CanMsg_Find(fcs_rx_messages, CAN_ARRAY_SIZE(fcs_rx_messages), msg->msgId);

    if (m == NULL)
        return false;
    CanMsg_Unpack(m, msg->data);
    if (m->id == CANDB_FCCB_TX_GENERAL_ID)
    {
        /* la trame d'état fait office de présence FCCB */
        fcs.watchdog.cnt = 0;
        fcs.watchdog.error = 0;
    }
    return true;
}

/*
 * Erreur générale pile : échec de démarrage, erreur critique ou mineure FCCB, ou perte de
 * communication. Recalculée à chaque tour de boucle (non verrouillée). La fuite H2 (ppmh2)
 * n'en fait pas partie : elle est traitée comme erreur critique système par la machine d'états.
 */
void Fcs_UpdateGeneralError(void)
{
    if (fcs.startup_error_flag || fcs.critical_error_flag || fcs.minor_error_flag || fcs.watchdog.error)
        fcs.general_error_flag = 1;
    else
        fcs.general_error_flag = 0;
}

/*
 * Temporisations comptées toutes les 100 ms :
 *  - courant pile < FCS_LOW_CURRENT_LEVEL (10 A) pendant FCS_LOW_CURRENT_TIMEOUT ticks (60 s) ;
 *  - temps passé dans l'état SMB_FCS_START_2, limité à FCS_START_2_TIMEOUT ticks (60 s).
 */
void Fcs_Update100ms(bool in_start_2_state)
{
    /* TODO(bug 10) : compteur 8 bits et timeout 600 : l'erreur ne se déclenche jamais (origine). */
    TimedFlag8_Update(&fcs.low_current_timeout, fcs.output_current < FCS_LOW_CURRENT_LEVEL, FCS_LOW_CURRENT_TIMEOUT);
    TimedFlag16_Update(&fcs.start_2_timeout, in_start_2_state, FCS_START_2_TIMEOUT);
}

/* Watchdog de communication : erreur si aucune trame d'état FCCB pendant WATCHDOG_TIMEOUT (30 s). */
void Fcs_Update1s(void)
{
    TimedFlag8_Update(&fcs.watchdog, true, WATCHDOG_TIMEOUT);
}

/* ---- Émission des commandes ------------------------------------------------------------- */

/* Commandes courtes (arrêt critique, arrêt, démarrage) en DLC 3 ; commandes longues
 * (maintenance, RAZ des erreurs) en DLC 8 avec l'octet de confirmation 0xFF. */
static void send_on_off(uint8_t critical_stop, uint8_t stop, uint8_t start, uint8_t reset_errors, bool long_command)
{
    cmd.critical_stop = critical_stop;
    cmd.stop = stop;
    cmd.start = start;
    cmd.reset_errors = reset_errors;
    cmd.confirm = long_command ? FCS_COMMAND_CONFIRM : 0;
    CanMsg_Send(&msg_on_off, long_command ? CAN_DLC_8 : CAN_DLC_3, CAN_PRIORITY_HIGH);
}

/* Commandes publiques : une seule des trois demandes courtes est à 1 à la fois. */
void Fcs_SendStart(void)           { send_on_off(0, 0, 1, 0, false); }
void Fcs_SendStop(void)            { send_on_off(0, 1, 0, 0, false); }
void Fcs_SendCriticalStop(void)    { send_on_off(1, 0, 0, 0, false); }
void Fcs_SendMaintenanceMode(void) { send_on_off(0, 0, FCS_MAINTENANCE_KEY, 0, true); }
void Fcs_SendResetErrors(void)     { send_on_off(0, 0, 0, 1, true); }

/* Consigne de courant pile (0,01 A), émise toutes les secondes en mode hybride par ems.c. */
void Fcs_SendCurrentLimit(uint16_t current_limit)
{
    cmd.current_limit = current_limit;
    CanMsg_Send(&msg_current_limit, msg_current_limit.dlc, CAN_PRIORITY_HIGH);
}

/*
 * Acquittement des erreurs pile : demande la RAZ à la FCCB et efface les erreurs locales qui
 * composent general_error_flag. Utilisé par l'appui sur le bouton hybride.
 */
void Fcs_ResetErrors(void)
{
    Fcs_SendResetErrors();
    fcs.startup_error_flag = 0;
    fcs.critical_error_flag = 0;
    fcs.minor_error_flag = 0;
    fcs.watchdog.error = 0;
    fcs.general_error_flag = 0;
}

/* Acquittement complet lors de la sortie du mode défaut : ajoute fuite H2, essai de redémarrage
 * et dépassement de durée en SMB_FCS_START_2. */
void Fcs_ResetAllErrors(void)
{
    Fcs_ResetErrors();
    fcs.ppmh2_error_flag = 0;
    fcs.startup_retry_flag = 0;
    fcs.start_2_timeout.error = 0;
}
