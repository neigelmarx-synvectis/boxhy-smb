/**
 * @file maintenance.c
 * @brief Mode maintenance (voir maintenance.h).
 */
#include "maintenance.h"
#include "smb_state_machine.h"
#include "hal/board_io.h"

#define MAINTENANCE_KEY       0x82

sMaintenance maintenance;

void Maintenance_Init(void)
{
    maintenance.mode_request_flag = false;
}

bool Maintenance_HandleCanFrame(const CAN_MSG_OBJ *msg, uint8_t smb_step)
{
    const uint8_t *d = msg->data;
    bool in_maintenance = (smb_step == SMB_MAINTENANCE) && (d[0] == MAINTENANCE_KEY);
    bool on = (d[1] == 1);

    switch (msg->msgId)
    {
    case 0x2C0100FUL: /* entrée / sortie du mode maintenance */
        if ((smb_step == SMB_STANDBY) && (d[0] == MAINTENANCE_KEY) && (d[7] == 0xFF))
            maintenance.mode_request_flag = 1;
        else if ((smb_step == SMB_MAINTENANCE) && (d[0] == MAINTENANCE_KEY))
            maintenance.mode_request_flag = 0;
        return true;

    case 0x2C01D00UL: if (in_maintenance) Board_SetFansDutyCycle(d[1]); return true;
    case 0x2C01D01UL: if (in_maintenance) { if (on) Board_ToggleDcacInputRelay();      else Board_SetDcacInputRelay(false); }      return true;
    case 0x2C01D02UL: if (in_maintenance) { if (on) Board_ToggleDcacPrechargeRelay();  else Board_SetDcacPrechargeRelay(false); }  return true;
    case 0x2C01D03UL: if (in_maintenance) { if (on) Board_ToggleSupplyPrechargeRelay();else Board_SetSupplyPrechargeRelay(false); }return true;
    case 0x2C01D04UL: if (in_maintenance) { if (on) Board_ToggleFcsOutputRelay();      else Board_SetFcsOutputRelay(false); }      return true;
    case 0x2C01D05UL: if (in_maintenance) { if (on) Board_ToggleBuzzer();              else Board_SetBuzzer(false); }              return true;
    case 0x2C01D06UL: if (in_maintenance) { if (on) Board_ToggleLedLeft();             else Board_SetLedLeft(false); }             return true;
    case 0x2C01D07UL: if (in_maintenance) { if (on) Board_ToggleLedMiddle();           else Board_SetLedMiddle(false); }           return true;
    case 0x2C01D08UL: if (in_maintenance) { if (on) Board_ToggleLedRight();            else Board_SetLedRight(false); }            return true;

    default:
        return false;
    }
}
