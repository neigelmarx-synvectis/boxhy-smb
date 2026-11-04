/**
 * @file maintenance.c
 * @brief Mode maintenance (voir maintenance.h).
 *
 * Les trames de l'IHM sont décodées par les tables de liaison ci-dessous (disposition issue de
 * can_db.h) dans `req`, puis la commande est appliquée selon l'identifiant reçu.
 */
#include "src/hmi/maintenance.h"
#include "src/can/can_db.h"
#include "src/system/smb_state_machine.h"
#include "hal/board_io.h"

#define MAINTENANCE_KEY       0x82
#define MAINTENANCE_CONFIRM   0xFF
#define COMMAND_TOGGLE        1

sMaintenance maintenance;

/* Dernière requête décodée */
static struct
{
    uint8_t key;     /* doit valoir MAINTENANCE_KEY */
    uint8_t value;   /* commande (COMMAND_TOGGLE ou autre = arrêt) ou rapport cyclique ventilateurs */
    uint8_t confirm; /* MAINTENANCE_CONFIRM pour entrer en maintenance */
} req;

/* Trames de commande manuelle : clé (octet 0) + valeur (octet 1) */
#define COMMAND_TABLE(name, MSG, VALUE_SIG)                                   \
    static const sCanBinding name[] CAN_ROM = {                               \
        CAN_BIND(MSG##__MAINTENANCE_KEY, &req.key, CAN_VAR_U8, 1),            \
        CAN_BIND(MSG##__##VALUE_SIG, &req.value, CAN_VAR_U8, 1),              \
    };                                                                        \
    CAN_CHECK_BINDINGS(MSG, name)

COMMAND_TABLE(fan_speed, CANDB_SMB_RX_FAN_SPEED, FAN_SPEED);
COMMAND_TABLE(dcac_input_relay, CANDB_SMB_RX_DCAC_INPUT_RELAY, COMMAND);
COMMAND_TABLE(dcac_precharge_relay, CANDB_SMB_RX_DCAC_PRECHARGE_RELAY, COMMAND);
COMMAND_TABLE(supply_precharge_relay, CANDB_SMB_RX_SUPPLY_PRECHARGE_RELAY, COMMAND);
COMMAND_TABLE(fcs_output_relay, CANDB_SMB_RX_FCS_OUTPUT_RELAY, COMMAND);
COMMAND_TABLE(buzzer, CANDB_SMB_RX_BUZZER, COMMAND);
COMMAND_TABLE(led_left, CANDB_SMB_RX_IHM_LEFT_BUTTON_LED, COMMAND);
COMMAND_TABLE(led_middle, CANDB_SMB_RX_IHM_MIDDLE_BUTTON_LED, COMMAND);
COMMAND_TABLE(led_right, CANDB_SMB_RX_IHM_RIGHT_BUTTON_LED, COMMAND);

static const sCanBinding mode_request[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_RX_MAINTENANCE_REQUEST__MAINTENANCE_KEY, &req.key, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_SMB_RX_MAINTENANCE_REQUEST__MAINTENANCE_CONFIRM, &req.confirm, CAN_VAR_U8, 1),
};
CAN_CHECK_BINDINGS(CANDB_SMB_RX_MAINTENANCE_REQUEST, mode_request);

static const sCanMessage maintenance_rx_messages[] CAN_ROM = {
    CAN_MESSAGE(CANDB_SMB_RX_MAINTENANCE_REQUEST, mode_request),
    CAN_MESSAGE(CANDB_SMB_RX_FAN_SPEED, fan_speed),
    CAN_MESSAGE(CANDB_SMB_RX_DCAC_INPUT_RELAY, dcac_input_relay),
    CAN_MESSAGE(CANDB_SMB_RX_DCAC_PRECHARGE_RELAY, dcac_precharge_relay),
    CAN_MESSAGE(CANDB_SMB_RX_SUPPLY_PRECHARGE_RELAY, supply_precharge_relay),
    CAN_MESSAGE(CANDB_SMB_RX_FCS_OUTPUT_RELAY, fcs_output_relay),
    CAN_MESSAGE(CANDB_SMB_RX_BUZZER, buzzer),
    CAN_MESSAGE(CANDB_SMB_RX_IHM_LEFT_BUTTON_LED, led_left),
    CAN_MESSAGE(CANDB_SMB_RX_IHM_MIDDLE_BUTTON_LED, led_middle),
    CAN_MESSAGE(CANDB_SMB_RX_IHM_RIGHT_BUTTON_LED, led_right),
};

void Maintenance_Init(void)
{
    maintenance.mode_request_flag = false;
}

bool Maintenance_HandleCanFrame(const CAN_MSG_OBJ *msg, uint8_t smb_step)
{
    const sCanMessage *m = CanMsg_Find(maintenance_rx_messages, CAN_ARRAY_SIZE(maintenance_rx_messages), msg->msgId);
    bool in_maintenance;
    bool on;

    if (m == NULL)
        return false;
    CanMsg_Unpack(m, msg->data);

    in_maintenance = (smb_step == SMB_MAINTENANCE) && (req.key == MAINTENANCE_KEY);
    on = (req.value == COMMAND_TOGGLE);

    switch (m->id)
    {
    case CANDB_SMB_RX_MAINTENANCE_REQUEST_ID: /* entrée / sortie du mode maintenance */
        if ((smb_step == SMB_STANDBY) && (req.key == MAINTENANCE_KEY) && (req.confirm == MAINTENANCE_CONFIRM))
            maintenance.mode_request_flag = 1;
        else if ((smb_step == SMB_MAINTENANCE) && (req.key == MAINTENANCE_KEY))
            maintenance.mode_request_flag = 0;
        break;

    case CANDB_SMB_RX_FAN_SPEED_ID:              if (in_maintenance) Board_SetFansDutyCycle(req.value); break;
    case CANDB_SMB_RX_DCAC_INPUT_RELAY_ID:       if (in_maintenance) { if (on) Board_ToggleDcacInputRelay();       else Board_SetDcacInputRelay(false); }       break;
    case CANDB_SMB_RX_DCAC_PRECHARGE_RELAY_ID:   if (in_maintenance) { if (on) Board_ToggleDcacPrechargeRelay();   else Board_SetDcacPrechargeRelay(false); }   break;
    case CANDB_SMB_RX_SUPPLY_PRECHARGE_RELAY_ID: if (in_maintenance) { if (on) Board_ToggleSupplyPrechargeRelay(); else Board_SetSupplyPrechargeRelay(false); } break;
    case CANDB_SMB_RX_FCS_OUTPUT_RELAY_ID:       if (in_maintenance) { if (on) Board_ToggleFcsOutputRelay();       else Board_SetFcsOutputRelay(false); }       break;
    case CANDB_SMB_RX_BUZZER_ID:                 if (in_maintenance) { if (on) Board_ToggleBuzzer();               else Board_SetBuzzer(false); }               break;
    case CANDB_SMB_RX_IHM_LEFT_BUTTON_LED_ID:    if (in_maintenance) { if (on) Board_ToggleLedLeft();              else Board_SetLedLeft(false); }              break;
    case CANDB_SMB_RX_IHM_MIDDLE_BUTTON_LED_ID:  if (in_maintenance) { if (on) Board_ToggleLedMiddle();            else Board_SetLedMiddle(false); }            break;
    case CANDB_SMB_RX_IHM_RIGHT_BUTTON_LED_ID:   if (in_maintenance) { if (on) Board_ToggleLedRight();             else Board_SetLedRight(false); }             break;
    default: break;
    }
    return true;
}
