/**
 * @file can_rx.c
 * @brief Réception CAN (voir can_rx.h).
 *
 * Appelé par la boucle principale (main.c) dès qu'au moins un message attend dans les buffers de
 * réception CAN1 ; un seul message est traité par tour de boucle, la réception étant prioritaire
 * sur les autres tâches.
 *
 * Aiguillage, dans cet ordre (les identifiants sont disjoints, l'ordre n'a donc pas d'effet) :
 *  1. batteries IBS (0x18FFxxD0/D1)            -> Battery_HandleCanFrame (devices/battery.c) ;
 *  2. carte FCCB (0x2FC0000/03/04)              -> Fcs_HandleCanFrame (devices/fcs.c) ;
 *  3. commandes de maintenance (0x2C01D0x, 0x2C0100F) -> Maintenance_HandleCanFrame (hmi/maintenance.c) ;
 *  4. ordres de l'IHM (0x2C01009, 0x25C1000)   -> traités ici ;
 * toute autre trame est ignorée.
 */
#include "src/can/can_rx.h"
#include "src/devices/battery.h"
#include "src/devices/fcs.h"
#include "src/hmi/maintenance.h"
#include "src/hmi/hmi.h"
#include "src/system/smb_state_machine.h"
#include "src/can/can_db.h"
#include "hal/can_port.h"

#define MAINTENANCE_KEY      0x82   /* clé attendue dans l'ordre d'acquittement du défaut */
#define IHM_ACK              1      /* l'IHM confirme avoir pris en compte la RAZ des erreurs */

/* Ordres de l'IHM traités ici (les autres trames sont aiguillées vers leur module).
 * Les deux trames n'ont qu'un signal utile, sur l'octet 0 : elles le décodent dans la même variable. */
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

/* Message en cours de traitement : le driver MCC y recopie l'identifiant, le DLC et les 8 octets
 * de données (au-delà du DLC, les octets valent 0 grâce à la remise à zéro en fin de traitement). */
static uint8_t rx_data[8];
static CAN_MSG_OBJ rx_msg;

/* Prépare l'objet de réception : à appeler une fois au démarrage, avant la boucle principale. */
void CanRx_Init(void)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
        rx_data[i] = 0;
    rx_msg.msgId = 0;
    rx_msg.field.msgfields = 0;
    rx_msg.data = rx_data;
}

/* Lit un message et l'aiguille (voir l'en-tête du fichier). */
void CanRx_Process(void)
{
    uint8_t i;

    if (!CanPort_Receive(&rx_msg))
        return;

    /* Chaque gestionnaire renvoie true s'il a reconnu l'identifiant ; le court-circuit du || arrête
     * l'aiguillage au premier module concerné. */
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

    /* RAZ du buffer : une trame courte suivante ne verra pas les octets de celle-ci */
    for (i = 0; i < 8; i++)
        rx_data[i] = 0;
}
