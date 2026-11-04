/**
 * @file can_tx.c
 * @brief Émission CAN (voir can_tx.h).
 *
 * Chaque trame émise est décrite par une table de liaisons « signal DBC <-> variable du firmware ».
 * La disposition des signaux vient de can_db.h (généré depuis can database/BOXHY.dbc) ;
 * le codage est fait par CanMsg_Pack(). Ordonnancement : une trame par créneau TMR1, cycle de
 * 25 créneaux (créneaux 1, 3, 4, 6, 7, 8, 9, 17, 19, 20, 21, 22, 23).
 */
#include "can_tx.h"
#include "can_db.h"
#include "smb_config.h"
#include "smb_state_machine.h"
#include "battery.h"
#include "fcs.h"
#include "ems.h"
#include "inverter.h"
#include "measures.h"
#include "hmi.h"
#include "cooling.h"
#include "hal/board_io.h"
#include "hal/can_port.h"

#define CAN_TX_CYCLE_LENGTH 25

/* Instantané des entrées / sorties de la carte, relu juste avant l'émission de SMB_TX_RAW1. */
static struct
{
    bool btn_left, btn_middle, btn_right, btn_battery_mode, btn_hybrid_mode;
    bool dcac_input_relay, dcac_precharge_relay, supply_precharge_relay, fcs_output_relay;
    bool buzzer, led_left, led_middle, led_right;
} io;

static uint8_t can_frame_ind;

/* ---- Tables de liaison ------------------------------------------------------------------ */

#define M CANDB_SMB_TX_BAT_GENERAL
static const sCanBinding bat_general[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__SOC, &battery.soc, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS0_SYSTEM_RUNNING_FLAG, &battery.ibs[0].system_running_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS0_ERROR_FLAG, &battery.ibs[0].error_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS0_LOW_SOC_WARNING_FLAG, &battery.ibs[0].low_soc_warning_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS0_BALANCING_FLAG, &battery.ibs[0].balancing_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS1_SYSTEM_RUNNING_FLAG, &battery.ibs[1].system_running_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS1_ERROR_FLAG, &battery.ibs[1].error_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS1_LOW_SOC_WARNING_FLAG, &battery.ibs[1].low_soc_warning_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS1_BALANCING_FLAG, &battery.ibs[1].balancing_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__BATTERY_OUTPUT_CURRENT, &battery.output_current, CAN_VAR_I16, 0.01),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS0_AVG_CELL_TEMPERATURE, &battery.ibs[0].avg_cell_temperature, CAN_VAR_I8, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS1_AVG_CELL_TEMPERATURE, &battery.ibs[1].avg_cell_temperature, CAN_VAR_I8, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS0_SOP, &battery.ibs[0].sop, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_GENERAL__IBS1_SOP, &battery.ibs[1].sop, CAN_VAR_U8, 1),
};
CAN_CHECK_BINDINGS(M, bat_general);
static const sCanMessage msg_bat_general CAN_ROM = CAN_MESSAGE(M, bat_general);
#undef M

#define M CANDB_SMB_TX_INV_FLAGS
#define B(SIG, var) CAN_BIND(CANDB_SMB_TX_INV_FLAGS__##SIG, &inverter.var, CAN_VAR_BOOL, 1)
static const sCanBinding inv_flags[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_TX_INV_FLAGS__INVERTER_MODE, &inverter.mode, CAN_VAR_U8, 1),
    B(INVERTER_ERROR_FLAG, error_flag),
    B(DEVICE_SWITCHED_ON_FLAG, device_switched_on_flag),
    B(LOAD_STATUS_FLAG, load_status_flag),
    B(CHARGING_STATUS_FLAG, charging_status_flag),
    B(AC_CHARGING_STATUS_FLAG, ac_charging_status_flag),
    B(SCC_CHARGING_STATUS_FLAG, scc_charging_status_flag),
    B(BUS_OVERVOLTAGE_ERROR_FLAG, bus_overvoltage_error_flag),
    B(BUS_UNDERVOLTAGE_ERROR_FLAG, bus_undervoltage_error_flag),
    B(BUS_SOFT_FAIL_ERROR_FLAG, bus_soft_fail_error_flag),
    B(INVERTER_VOLTAGE_TOO_LOW_ERROR_FLAG, voltage_too_low_error_flag),
    B(INVERTER_VOLTAGE_TOO_HIGH_ERROR_FLAG, voltage_too_high_error_flag),
    B(OVERTEMPERATURE_ERROR_FLAG, overtemperature_error_flag),
    B(FAN_LOCKED_ERROR_FLAG, fan_locked_error_flag),
    B(BATTERY_VOLTAGE_HIGH_ERROR_FLAG, battery_voltage_high_error_flag),
    B(OVERCHARGE_ERROR_FLAG, overcharge_error_flag),
    B(OVERLOAD_ERROR_FLAG, overload_error_flag),
    B(INVERTER_OVER_CURRENT_ERROR_FLAG, over_current_error_flag),
    B(INVERTER_SOFT_START_ERROR_FLAG, soft_start_error_flag),
    B(SELF_TEST_ERROR_FLAG, self_test_error_flag),
    B(AC_OUTPUT_DC_VOLTAGE_TOO_HIGH_ERROR_FLAG, ac_output_dc_voltage_too_high_error_flag),
    B(BATTERY_DISCONNECTED_ERROR_FLAG, battery_disconnected_error_flag),
    B(CURRENT_SENSOR_ERROR_FLAG, current_sensor_error_flag),
    B(BATTERY_SHORT_CIRCUIT_ERROR_FLAG, battery_short_circuit_error_flag),
    B(MPPT_OVERLOAD_ERROR_FLAG, mppt_overload_error_flag),
    B(AC_OUTPUT_APPARENT_POWER_VERY_HIGH_ERROR_FLAG, apparent_power_very_high_error_flag),
    B(LINE_FAIL_WARNING_FLAG, line_fail_warning_flag),
    B(OPV_SHORT_WARNING_FLAG, opv_short_warning_flag),
    B(BUS_SOFT_FAIL_WARNING_FLAG, bus_soft_fail_error_flag), /* recopie de l'erreur (origine) */
    B(OVERTEMPERATURE_WARNING_FLAG, overtemperature_warning_flag),
    B(FAN_LOCKED_WARNING_FLAG, fan_locked_warning_flag),
    B(BATTERY_VOLTAGE_HIGH_WARNING_FLAG, battery_voltage_high_warning_flag),
    B(BATTERY_VOLTAGE_LOW_WARNING_FLAG, battery_voltage_low_warning_flag),
    B(BATTERY_UNDER_SHUTDOWN_VOLTAGE_WARNING_FLAG, battery_under_shutdown_voltage_warning_flag),
    B(BATTERY_DERATING_WARNING_FLAG, battery_derating_warning_flag),
    B(OVERLOAD_WARNING_FLAG, overload_warning_flag),
    B(EEPROM_WARNING_FLAG, eeprom_warning_flag),
    B(POWER_LIMIT_WARNING_FLAG, power_limit_warning_flag),
    B(PV_VOLTAGE_HIGH_WARNING_FLAG, pv_voltage_high_warning_flag),
    B(MPPT_OVERLOAD_WARNING_FLAG, mppt_overload_warning_flag),
    B(BATTERY_TOO_LOW_TO_CHARGE_WARNING_FLAG, battery_too_low_to_charge_warning_flag),
};
#undef B
CAN_CHECK_BINDINGS(M, inv_flags);
static const sCanMessage msg_inv_flags CAN_ROM = CAN_MESSAGE(M, inv_flags);
#undef M

#define M CANDB_SMB_TX_INV_GENERAL
static const sCanBinding inv_general[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_TX_INV_GENERAL__AC_OUTPUT_APPARENT_POWER, &inverter.moy_ac_output_apparent_power, CAN_VAR_U16, 1),
    CAN_BIND(CANDB_SMB_TX_INV_GENERAL__AC_OUTPUT_ACTIVE_POWER, &inverter.ac_output_active_power, CAN_VAR_U16, 1),
    CAN_BIND(CANDB_SMB_TX_INV_GENERAL__AC_OUTPUT_VOLTAGE, &inverter.ac_output_voltage, CAN_VAR_U16, 0.1),
    CAN_BIND(CANDB_SMB_TX_INV_GENERAL__DCAC_TEMP, &inverter.dcac_temp, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_SMB_TX_INV_GENERAL__TOGGLE_QMOD_FLAG, &inverter.toggle_qmod_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_INV_GENERAL__TOGGLE_QPIWS_FLAG, &inverter.toggle_qpiws_flag, CAN_VAR_BOOL, 1),
    CAN_BIND(CANDB_SMB_TX_INV_GENERAL__TOGGLE_QPIGS_FLAG, &inverter.toggle_qpigs_flag, CAN_VAR_BOOL, 1),
};
CAN_CHECK_BINDINGS(M, inv_general);
static const sCanMessage msg_inv_general CAN_ROM = CAN_MESSAGE(M, inv_general);
#undef M

#define M CANDB_SMB_TX_GENERAL
#define B(SIG, var) CAN_BIND(CANDB_SMB_TX_GENERAL__##SIG, &var, CAN_VAR_BOOL, 1)
static const sCanBinding smb_general[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_TX_GENERAL__STEP, &smb.step, CAN_VAR_U8, 1),
    B(CRITICAL_ERROR_FLAG, smb.critical_error_flag),
    B(DCAC_GENERAL_ERROR_FLAG, inverter.general_error_flag),
    B(FCS_GENERAL_ERROR_FLAG, fcs.general_error_flag),
    B(FCS_PPMH2_ERROR_FLAG, fcs.ppmh2_error_flag),
    B(DCAC_RECEIVE_TIMEOUT_FLAG, inverter.recieve_flag),
    B(IBS0_WATCHDOG_ERROR_FLAG, battery.ibs[0].watchdog.error),
    B(IBS1_WATCHDOG_ERROR_FLAG, battery.ibs[1].watchdog.error),
    B(DCAC_WATCHDOG_ERROR_FLAG, inverter.watchdog_error_flag),
    B(FCS_WATCHDOG_ERROR_FLAG, fcs.watchdog.error),
    B(FCS_STARTUP_ERROR_FLAG, fcs.startup_error_flag),
    B(AMBIANT_TEMP_ERROR_FLAG, measures.ac_ambiant_temp.error_flag),
    B(TEMP2_ERROR_FLAG, measures.temp2.error_flag),
    B(SOFTWARE_ERROR_FLAG, smb.software_error_flag),
    B(BATTERY_MODE_BUTTON_FLAG, hmi.battery_mode_button.state),
    B(HYBRID_MODE_BUTTON_FLAG, hmi.hybrid_mode_button.state),
    CAN_BIND(CANDB_SMB_TX_GENERAL__FCS_STEP, &fcs.step, CAN_VAR_U8, 1),
    B(BATTERY_SOC_HIGH_LEVEL_WARNING_FLAG, battery.soc_high_level_warning_flag),
    B(BATTERY_SOC_LOW_LEVEL_WARNING_FLAG, battery.soc_low_level_warning_flag),
    B(AC_OUTPUT_APPARENT_POWER_HIGH_LEVEL_WARNING_FLAG, inverter.apparent_power_high_warning_flag),
    B(BATTERY_SOP_WARNING_FLAG, battery.sop_warning_flag),
    B(BATTERY_SOP_ERROR_FLAG, battery.sop_error_flag),
    B(BATTERY_SOC_VERY_VERY_HIGH_LEVEL_ERROR_FLAG, battery.soc_very_very_high_level_error_flag),
    B(BATTERY_SOC_VERY_LOW_LEVEL_ERROR_FLAG, battery.soc_very_low_level_error_flag),
    B(BATTERY_OVERVOLTAGE_WARNING_FLAG, battery.overvoltage_warning_flag),
    B(BATTERY_OVERVOLTAGE_ERROR_FLAG, battery.overvoltage.error),
    B(BATTERY_OVERCURRENT_WARNING_FLAG, battery.overcurrent_warning_flag),
    B(BATTERY_OVERCURRENT_ERROR_FLAG, battery.overcurrent.error),
    B(BATTERY_OVERTEMPERATURE_CHARGE_ERROR_FLAG, battery.overtemperature_charge.error),
    B(BATTERY_OVERTEMPERATURE_DISCHARGE_ERROR_FLAG, battery.overtemperature_discharge.error),
    B(BATTERY_OVERTEMPERATURE_CHARGE_WARNING_FLAG, battery.overtemperature_charge_warning_flag),
    B(BATTERY_OVERTEMPERATURE_DISCHARGE_WARNING_FLAG, battery.overtemperature_discharge_warning_flag),
};
#undef B
CAN_CHECK_BINDINGS(M, smb_general);
static const sCanMessage msg_smb_general CAN_ROM = CAN_MESSAGE(M, smb_general);
#undef M

#define M CANDB_SMB_TX_RAW1
#define B(SIG, var) CAN_BIND(CANDB_SMB_TX_RAW1__##SIG, &io.var, CAN_VAR_BOOL, 1)
static const sCanBinding smb_raw1[] CAN_ROM = {
    B(IHM_LEFT_BUTTON_STATE, btn_left),
    B(IHM_MIDDLE_BUTTON_STATE, btn_middle),
    B(IHM_RIGHT_BUTTON_STATE, btn_right),
    B(BATTERY_MODE_BUTTON_STATE, btn_battery_mode),
    B(HYBRID_MODE_BUTTON_STATE, btn_hybrid_mode),
    B(DCAC_INPUT_RELAY_STATE, dcac_input_relay),
    B(DCAC_PRECHARGE_RELAY_STATE, dcac_precharge_relay),
    B(SUPPLY_PRECHARGE_RELAY_STATE, supply_precharge_relay),
    B(FCS_OUTPUT_RELAY_STATE, fcs_output_relay),
    B(BUZZER_STATE, buzzer),
    B(IHM_LEFT_BUTTON_LED_STATE, led_left),
    B(IHM_MIDDLE_BUTTON_LED_STATE, led_middle),
    B(IHM_RIGHT_BUTTON_LED_STATE, led_right),
    CAN_BIND(CANDB_SMB_TX_RAW1__AMBIANT_TEMP_RAW, &measures.temp_1_raw, CAN_VAR_U16, 1),
    CAN_BIND(CANDB_SMB_TX_RAW1__TEMP2_RAW, &measures.temp_2_raw, CAN_VAR_U16, 1),
    CAN_BIND(CANDB_SMB_TX_RAW1__SUPPLY_VOLTAGE_RAW, &measures.supply_voltage_raw, CAN_VAR_U16, 1),
    CAN_BIND(CANDB_SMB_TX_RAW1__BUS_VOLTAGE_RAW, &measures.bus_voltage_raw, CAN_VAR_U16, 1),
};
#undef B
CAN_CHECK_BINDINGS(M, smb_raw1);
static const sCanMessage msg_smb_raw1 CAN_ROM = CAN_MESSAGE(M, smb_raw1);
#undef M

#define M CANDB_SMB_TX_TEMPERATURE
static const sCanBinding smb_temperature[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_TX_TEMPERATURE__AC_AMBIANT_TEMP, &measures.ac_ambiant_temp.value_8, CAN_VAR_I8, 1),
    CAN_BIND(CANDB_SMB_TX_TEMPERATURE__TEMP2, &measures.temp2.value_8, CAN_VAR_I8, 1),
    CAN_BIND(CANDB_SMB_TX_TEMPERATURE__SUPPLY_VOLTAGE, &measures.supply_voltage, CAN_VAR_U16, 0.01),
    CAN_BIND(CANDB_SMB_TX_TEMPERATURE__BUS_VOLTAGE, &measures.bus_voltage, CAN_VAR_U16, 0.01),
};
CAN_CHECK_BINDINGS(M, smb_temperature);
static const sCanMessage msg_smb_temperature CAN_ROM = CAN_MESSAGE(M, smb_temperature);
#undef M

#define M CANDB_SMB_TX_CURRENTS
static const sCanBinding smb_currents[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_TX_CURRENTS__FCS_OUTPUT_CURRENT, &fcs.output_current, CAN_VAR_I16, 0.01),
    CAN_BIND(CANDB_SMB_TX_CURRENTS__REQUESTED_FCS_CURRENT, &ems.requested_fcs_current, CAN_VAR_U16, 0.01),
    CAN_BIND(CANDB_SMB_TX_CURRENTS__BATTERY_OUTPUT_CURRENT, &battery.output_current, CAN_VAR_I16, 0.01),
    CAN_BIND(CANDB_SMB_TX_CURRENTS__SYSTEM_OUTPUT_CURRENT, &ems.system_output_current, CAN_VAR_I16, 0.01),
};
CAN_CHECK_BINDINGS(M, smb_currents);
static const sCanMessage msg_smb_currents CAN_ROM = CAN_MESSAGE(M, smb_currents);
#undef M

#define M CANDB_SMB_TX_VERSION
static const sCanBinding smb_version[] CAN_ROM = {
    CAN_BIND_CONST(CANDB_SMB_TX_VERSION__SOFTWARE_VERSION, SMB_VERSION),
    CAN_BIND_CONST(CANDB_SMB_TX_VERSION__SOFTWARE_SUBVERSION, SMB_SUBVERSION),
    CAN_BIND_CONST(CANDB_SMB_TX_VERSION__SOFTWARE_SUBSUBVERSION, SMB_SUBSUBVERSION),
    CAN_BIND(CANDB_SMB_TX_VERSION__BUZZER_5_MINUTES_CNT, &hmi.buzzer_5_minutes.cnt, CAN_VAR_U16, 1),
    CAN_BIND(CANDB_SMB_TX_VERSION__RAZ_ERREUR_IHM_FLAG, &hmi.raz_erreur_ihm_flag, CAN_VAR_BOOL, 1),
};
CAN_CHECK_BINDINGS(M, smb_version);
static const sCanMessage msg_smb_version CAN_ROM = CAN_MESSAGE(M, smb_version);
#undef M

#define M CANDB_SMB_TX_BAT_DETAILS
static const sCanBinding bat_details[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_TX_BAT_DETAILS__IBS0_OUTPUT_CURRENT, &battery.ibs[0].output_current, CAN_VAR_I16, 0.01),
    CAN_BIND(CANDB_SMB_TX_BAT_DETAILS__IBS1_OUTPUT_CURRENT, &battery.ibs[1].output_current, CAN_VAR_I16, 0.01),
    CAN_BIND(CANDB_SMB_TX_BAT_DETAILS__IBS0_MAX_CELL_TEMPERATURE, &battery.ibs[0].max_cell_temperature, CAN_VAR_I8, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_DETAILS__IBS1_MAX_CELL_TEMPERATURE, &battery.ibs[1].max_cell_temperature, CAN_VAR_I8, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_DETAILS__IBS0_MAX_BOARD_TEMPERATURE, &battery.ibs[0].max_board_temperature, CAN_VAR_I8, 1),
    CAN_BIND(CANDB_SMB_TX_BAT_DETAILS__IBS1_MAX_BOARD_TEMPERATURE, &battery.ibs[1].max_board_temperature, CAN_VAR_I8, 1),
};
CAN_CHECK_BINDINGS(M, bat_details);
static const sCanMessage msg_bat_details CAN_ROM = CAN_MESSAGE(M, bat_details);
#undef M

CAN_CHECK_EMPTY(CANDB_SMB_TX_BAT_RESERVED);
static const sCanMessage msg_bat_reserved CAN_ROM = CAN_MESSAGE_EMPTY(CANDB_SMB_TX_BAT_RESERVED);

#define M CANDB_FCCB_RX_KEEP_ALIVE
static const sCanBinding fccb_keep_alive[] CAN_ROM = {
    CAN_BIND_CONST(CANDB_FCCB_RX_KEEP_ALIVE__SMB_PRESENT, 1),
};
CAN_CHECK_BINDINGS(M, fccb_keep_alive);
static const sCanMessage msg_fccb_keep_alive CAN_ROM = CAN_MESSAGE(M, fccb_keep_alive);
#undef M

#define M CANDB_SMB_FAN_CONTROL
static const sCanBinding fan_control[] CAN_ROM = {
    CAN_BIND(CANDB_SMB_FAN_CONTROL__DCAC_COOLING_REQUEST, &cooling.inverter_pwm, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_SMB_FAN_CONTROL__AMBIANT_COOLING_REQUEST, &cooling.ambiant_pwm, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_SMB_FAN_CONTROL__DCDC_COOLING_REQUEST, &cooling.dcdc_pwm, CAN_VAR_U8, 1),
    CAN_BIND(CANDB_SMB_FAN_CONTROL__FAN_PWM, &cooling.fans_duty_cycle, CAN_VAR_U8, 1),
};
CAN_CHECK_BINDINGS(M, fan_control);
static const sCanMessage msg_fan_control CAN_ROM = CAN_MESSAGE(M, fan_control);
#undef M

#define M CANDB_SMB_ENERGY_MANAGEMENT
static const sCanBinding energy_management[] CAN_ROM = {
    CAN_BIND_CONST(CANDB_SMB_ENERGY_MANAGEMENT__MIN_EMS_CURRENT, 0), /* régulation dynamique désactivée */
    CAN_BIND_CONST(CANDB_SMB_ENERGY_MANAGEMENT__MAX_EMS_CURRENT, 0),
    CAN_BIND(CANDB_SMB_ENERGY_MANAGEMENT__REQUESTED_FCS_CURRENT, &ems.requested_fcs_current, CAN_VAR_U16, 0.01),
    CAN_BIND(CANDB_SMB_ENERGY_MANAGEMENT__IBS0_MAX_CHARGE_CURRENT, &battery.ibs[0].max_charge_current, CAN_VAR_I16, 0.01),
    CAN_BIND(CANDB_SMB_ENERGY_MANAGEMENT__IBS0_MAX_DISCHARGE_CURRENT, &battery.ibs[0].max_discharge_current, CAN_VAR_I16, 0.01),
};
CAN_CHECK_BINDINGS(M, energy_management);
static const sCanMessage msg_energy_management CAN_ROM = CAN_MESSAGE(M, energy_management);
#undef M

/* ---- Ordonnancement --------------------------------------------------------------------- */

/* Les trames onduleur ne sont émises qu'onduleur alimenté, plus une fois après son arrêt. */
static bool inverter_frames_enabled(void)
{
    return ((smb.step > SMB_STANDBY) && (smb.step <= SMB_EMERGENCY)) || inverter.raz_data_after_stop_flag;
}

static void after_inverter_measures(void)
{
    inverter.raz_data_after_stop_flag = 0;
}

static void snapshot_io(void)
{
    io.btn_left = Board_GetButtonLeft();
    io.btn_middle = Board_GetButtonMiddle();
    io.btn_right = Board_GetButtonRight();
    io.btn_battery_mode = Board_GetButtonBatteryMode();
    io.btn_hybrid_mode = Board_GetButtonHybridMode();
    io.dcac_input_relay = Board_GetDcacInputRelay();
    io.dcac_precharge_relay = Board_GetDcacPrechargeRelay();
    io.supply_precharge_relay = Board_GetSupplyPrechargeRelay();
    io.fcs_output_relay = Board_GetFcsOutputRelay();
    io.buzzer = Board_GetBuzzer();
    io.led_left = Board_GetLedLeft();
    io.led_middle = Board_GetLedMiddle();
    io.led_right = Board_GetLedRight();
}

typedef struct
{
    uint8_t slot;
    const sCanMessage *msg;
    bool (*enabled)(void); /* NULL : toujours émise */
    void (*before)(void);  /* NULL : rien */
    void (*after)(void);   /* NULL : rien */
} sCanTxSlot;

static const sCanTxSlot slots[] CAN_ROM = {
    {1,  &msg_bat_general,       NULL, NULL, NULL},
    {3,  &msg_inv_flags,         inverter_frames_enabled, NULL, NULL},
    {4,  &msg_inv_general,       inverter_frames_enabled, NULL, after_inverter_measures},
    {6,  &msg_smb_general,       NULL, NULL, NULL},
    {7,  &msg_smb_raw1,          NULL, snapshot_io, NULL},
    {8,  &msg_smb_temperature,   NULL, NULL, NULL},
    {9,  &msg_smb_currents,      NULL, NULL, NULL},
    {17, &msg_smb_version,       NULL, NULL, NULL},
    {19, &msg_bat_reserved,      NULL, NULL, NULL},
    {20, &msg_fccb_keep_alive,   NULL, NULL, NULL},
    {21, &msg_fan_control,       NULL, NULL, NULL},
    {22, &msg_energy_management, NULL, NULL, NULL},
    {23, &msg_bat_details,       NULL, NULL, NULL},
};

void CanTx_Init(void)
{
    can_frame_ind = 0;
}

void CanTx_SendNext(void)
{
    uint8_t i;
    const sCanTxSlot *s;

    for (i = 0; i < CAN_ARRAY_SIZE(slots); i++)
    {
        s = &slots[i];
        if (s->slot != can_frame_ind)
            continue;
        if ((s->enabled == NULL) || s->enabled())
        {
            if (s->before != NULL)
                s->before();
            CanMsg_Send(s->msg, s->msg->dlc, CAN_PRIORITY_LOW);
            if (s->after != NULL)
                s->after();
        }
        break;
    }

    can_frame_ind++;
    if (can_frame_ind > CAN_TX_CYCLE_LENGTH - 1)
        can_frame_ind = 0;
}
