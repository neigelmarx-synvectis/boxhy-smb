/**
 * @file can_rx.c
 * @brief Réception CAN (voir can_rx.h).
 */
#include "can_rx.h"
#include "battery.h"
#include "fcs.h"
#include "maintenance.h"
#include "hmi.h"
#include "smb_state_machine.h"
#include "hal/can_port.h"

#define SMB_RESET_ORDER_ID   0x2C01009UL
#define IHM_RAZ_ERREUR_ID    0x25C1000UL
#define MAINTENANCE_KEY      0x82

static uint8_t rx_data[8];
static CAN_MSG_OBJ rx_msg;

void CanRx_Init(void)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
        rx_data[i] = 0;
    rx_msg.msgId = 0;
    rx_msg.field.msgfields = 0;
    rx_msg.data = rx_data;
}

void CanRx_Process(void)
{
    uint8_t i;

    if (!CanPort_Receive(&rx_msg))
        return;

    if (Battery_HandleCanFrame(&rx_msg) ||
        Fcs_HandleCanFrame(&rx_msg) ||
        Maintenance_HandleCanFrame(&rx_msg, smb.step))
    {
        /* traité par le module */
    }
    else
    {
        switch (rx_msg.msgId)
        {
        case SMB_RESET_ORDER_ID: /* acquittement du défaut par la télémétrie */
            Smb_SetResetOrder((smb.step == SMB_EMERGENCY) && (rx_data[0] == MAINTENANCE_KEY));
            /* TODO(bug 3) : pas de break à l'origine, la trame est aussi traitée comme 0x25C1000 */
            /* fall through */
        case IHM_RAZ_ERREUR_ID: /* l'IHM a pris en compte la RAZ des erreurs */
            if (rx_data[0] == 1)
                hmi.raz_erreur_ihm_flag = 0;
            break;
        default:
            break;
        }
    }

    for (i = 0; i < 8; i++)
        rx_data[i] = 0;
}
