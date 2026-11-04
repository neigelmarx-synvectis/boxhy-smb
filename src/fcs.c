/**
 * @file fcs.c
 * @brief Pile à combustible via la carte FCCB (voir fcs.h).
 */
#include <string.h>
#include "fcs.h"
#include "can_db.h"
#include "smb_config.h"
#include "hal/can_port.h"

#define FCS_COMMAND_CONFIRM   0xFF /* octet 7 des commandes longues (maintenance, RAZ erreurs) */
#define FCS_MAINTENANCE_KEY   0x82

sFcs fcs;

/* ---- Réception ------------------------------------------------------------------------- */

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

static const sCanMessage fcs_rx_messages[] CAN_ROM = {
    CAN_MESSAGE(CANDB_FCCB_TX_GENERAL, fccb_general),
    CAN_MESSAGE(CANDB_FCCB_TX_TEMPERATURE, fccb_temperature),
    CAN_MESSAGE(CANDB_FCCB_TX_PURGE_CC_INFOS, fccb_purge_cc_infos),
};

/* ---- Commandes ------------------------------------------------------------------------- */

static struct
{
    uint8_t critical_stop;
    uint8_t stop;
    uint8_t start;          /* 1 : démarrage ; FCS_MAINTENANCE_KEY : mode maintenance */
    uint8_t reset_errors;
    uint8_t confirm;
    uint16_t current_limit; /* 0,01 A */
} cmd;

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

#define M CANDB_FCCB_RX_OUPUT_CURRENT_LIMIT
static const sCanBinding fccb_current_limit[] CAN_ROM = {
    CAN_BIND(CANDB_FCCB_RX_OUPUT_CURRENT_LIMIT__OUTPUT_CURRENT_LIMIT, &cmd.current_limit, CAN_VAR_U16, 0.01),
};
CAN_CHECK_BINDINGS(M, fccb_current_limit);
static const sCanMessage msg_current_limit CAN_ROM = CAN_MESSAGE(M, fccb_current_limit);
#undef M

void Fcs_Init(void)
{
    memset(&cmd, 0, sizeof(cmd));
    memset(&fcs, 0, sizeof(fcs));
}

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

void Fcs_UpdateGeneralError(void)
{
    if (fcs.startup_error_flag || fcs.critical_error_flag || fcs.minor_error_flag || fcs.watchdog.error)
        fcs.general_error_flag = 1;
    else
        fcs.general_error_flag = 0;
}

void Fcs_Update100ms(bool in_start_2_state)
{
    /* TODO(bug 10) : compteur 8 bits et timeout 600 : l'erreur ne se déclenche jamais (origine). */
    TimedFlag8_Update(&fcs.low_current_timeout, fcs.output_current < FCS_LOW_CURRENT_LEVEL, FCS_LOW_CURRENT_TIMEOUT);
    TimedFlag16_Update(&fcs.start_2_timeout, in_start_2_state, FCS_START_2_TIMEOUT);
}

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

void Fcs_SendStart(void)           { send_on_off(0, 0, 1, 0, false); }
void Fcs_SendStop(void)            { send_on_off(0, 1, 0, 0, false); }
void Fcs_SendCriticalStop(void)    { send_on_off(1, 0, 0, 0, false); }
void Fcs_SendMaintenanceMode(void) { send_on_off(0, 0, FCS_MAINTENANCE_KEY, 0, true); }
void Fcs_SendResetErrors(void)     { send_on_off(0, 0, 0, 1, true); }

void Fcs_SendCurrentLimit(uint16_t current_limit)
{
    cmd.current_limit = current_limit;
    CanMsg_Send(&msg_current_limit, msg_current_limit.dlc, CAN_PRIORITY_HIGH);
}

void Fcs_ResetErrors(void)
{
    Fcs_SendResetErrors();
    fcs.startup_error_flag = 0;
    fcs.critical_error_flag = 0;
    fcs.minor_error_flag = 0;
    fcs.watchdog.error = 0;
    fcs.general_error_flag = 0;
}

void Fcs_ResetAllErrors(void)
{
    Fcs_ResetErrors();
    fcs.ppmh2_error_flag = 0;
    fcs.startup_retry_flag = 0;
    fcs.start_2_timeout.error = 0;
}
