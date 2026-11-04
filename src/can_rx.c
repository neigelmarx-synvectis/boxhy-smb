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
#include "can_db.h"
#include "hal/can_port.h"

#define MAINTENANCE_KEY      0x82
#define IHM_ACK              1

/* Ordres de l'IHM traités ici (les autres trames sont aiguillées vers leur module) */
static uint8_t order_value;

static const sCanBinding reset_order[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_RX_RESET_ORDER__RESET_KEY, &order_value, CAN_VAR_U8, 1),
};
CAN_CHECK_BINDINGS(CANDB_SMB_RX_RESET_ORDER, reset_order);
static const sCanBinding ihm_errors_ack[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_RX_IHM_ERRORS_ACK__ACK, &order_value, CAN_VAR_U8, 1),
};
CAN_CHECK_BINDINGS(CANDB_SMB_RX_IHM_ERRORS_ACK, ihm_errors_ack);

static const sCanMessage order_messages[] CAN_ROM = {
    CAN_MESSAGE(CANDB_SMB_RX_RESET_ORDER, reset_order),
    CAN_MESSAGE(CANDB_SMB_RX_IHM_ERRORS_ACK, ihm_errors_ack),
};

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
        const sCanMessage *m = CanMsg_Find(order_messages, CAN_ARRAY_SIZE(order_messages), rx_msg.msgId);
        if (m != NULL)
        {
            CanMsg_Unpack(m, rx_data);
            switch (m->id)
            {
            case CANDB_SMB_RX_RESET_ORDER_ID: /* acquittement du défaut par la télémétrie */
                Smb_SetResetOrder((smb.step == SMB_EMERGENCY) && (order_value == MAINTENANCE_KEY));
                /* TODO(bug 3) : pas de break à l'origine, la trame est aussi traitée comme
                 * SMB_RX_IHM_ERRORS_ACK (même octet 0) */
                /* fall through */
            case CANDB_SMB_RX_IHM_ERRORS_ACK_ID: /* l'IHM a pris en compte la RAZ des erreurs */
                if (order_value == IHM_ACK)
                    hmi.raz_erreur_ihm_flag = 0;
                break;
            default:
                break;
            }
        }
    }

    for (i = 0; i < 8; i++)
        rx_data[i] = 0;
}
