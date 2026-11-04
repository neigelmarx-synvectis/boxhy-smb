/**
 * @file fcs.c
 * @brief Pile à combustible via la carte FCCB (voir fcs.h).
 */
#include <string.h>
#include "fcs.h"
#include "smb_config.h"
#include "hal/can_port.h"

#define FCS_COMMAND_ID        0x2FC1001UL
#define FCS_CURRENT_LIMIT_ID  0x2FC1002UL

sFcs fcs;

static uint8_t command_data[8];
static CAN_MSG_OBJ command_msg;

void Fcs_Init(void)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
        command_data[i] = 0;
    command_msg.msgId = FCS_COMMAND_ID;
    command_msg.field.frameType = CAN_FRAME_DATA;
    command_msg.field.idType = CAN_FRAME_EXT;
    command_msg.field.dlc = CAN_DLC_4;
    command_msg.data = command_data;

    memset(&fcs, 0, sizeof(fcs));
}

bool Fcs_HandleCanFrame(const CAN_MSG_OBJ *msg)
{
    uFLAGSBYTE flags;
    const uint8_t *d = msg->data;

    switch (msg->msgId)
    {
    case 0x2FC0000UL: /* état et erreurs FCCB */
        fcs.watchdog.cnt = 0;
        fcs.watchdog.error = 0;
        fcs.step = d[0];
        flags.byte = d[1];
        fcs.critical_error_flag = flags.sFLAGSBITS.b0;
        fcs.minor_error_flag = flags.sFLAGSBITS.b1;
        fcs.ppmh2_error_flag = flags.sFLAGSBITS.b2;
        flags.byte = d[2];
        fcs.low_current_warning_flag = flags.sFLAGSBITS.b1;
        fcs.dcdc_temperature_warning_flag = flags.sFLAGSBITS.b2;
        flags.byte = d[5];
        fcs.short_circuit_ongoing_flag = flags.sFLAGSBITS.b0;
        fcs.output_current = (int16_t)(uint16_t)(d[6] | ((uint16_t)d[7] << 8));
        return true;
    case 0x2FC0004UL:
        fcs.short_circuit_interval_cnt = d[6];
        return true;
    case 0x2FC0003UL:
        fcs.dcdc_temperature = d[3];
        return true;
    default:
        return false;
    }
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

/* ---- Commandes ------------------------------------------------------------------ */

static void send_command(uint32_t id, const uint8_t *bytes, uint8_t dlc)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
        command_data[i] = (i < dlc) ? bytes[i] : 0;
    command_msg.field.dlc = dlc;
    command_msg.msgId = id;
    CanPort_Transmit(CAN_PRIORITY_HIGH, &command_msg);
}

void Fcs_SendStart(void)
{
    static const uint8_t b[3] = {0, 0, 1};
    send_command(FCS_COMMAND_ID, b, CAN_DLC_3);
}

void Fcs_SendStop(void)
{
    static const uint8_t b[3] = {0, 1, 0};
    send_command(FCS_COMMAND_ID, b, CAN_DLC_3);
}

void Fcs_SendCriticalStop(void)
{
    static const uint8_t b[3] = {1, 0, 0};
    send_command(FCS_COMMAND_ID, b, CAN_DLC_3);
}

void Fcs_SendMaintenanceMode(void)
{
    static const uint8_t b[8] = {0, 0, 0x82, 0, 0, 0, 0, 0xFF};
    send_command(FCS_COMMAND_ID, b, CAN_DLC_8);
}

void Fcs_SendResetErrors(void)
{
    static const uint8_t b[8] = {0, 0, 0, 0x01, 0, 0, 0, 0xFF};
    send_command(FCS_COMMAND_ID, b, CAN_DLC_8);
}

void Fcs_SendCurrentLimit(uint16_t current_limit)
{
    uint8_t b[2];
    b[0] = (uint8_t)current_limit;
    b[1] = (uint8_t)(current_limit >> 8);
    send_command(FCS_CURRENT_LIMIT_ID, b, CAN_DLC_2);
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
