/**
 * @file can_tx.c
 * @brief Émission CAN (voir can_tx.h).
 *
 * Les créneaux (1, 3, 4, 6, 7, 8, 9, 17, 19, 20, 21, 22, 23 sur 25) et le contenu des trames
 * sont ceux du firmware d'origine, octet pour octet.
 */
#include "can_tx.h"
#include "generic.h"
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

static uint8_t data[8];
static CAN_MSG_OBJ output_msg;
static uint8_t can_frame_ind;

void CanTx_Init(void)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
        data[i] = 0;
    output_msg.msgId = 0x08200;
    output_msg.field.frameType = CAN_FRAME_DATA;
    output_msg.field.idType = CAN_FRAME_EXT;
    output_msg.field.dlc = CAN_DLC_8;
    output_msg.data = data;
    can_frame_ind = 0;
}

static void transmit(uint32_t id, uint8_t dlc)
{
    output_msg.field.dlc = dlc;
    output_msg.msgId = id;
    CanPort_Transmit(CAN_PRIORITY_LOW, &output_msg);
}

static void put_u16(uint8_t index, uint16_t value)
{
    data[index] = (uint8_t)value;
    data[index + 1] = (uint8_t)(value >> 8);
}

/* Les trames onduleur ne sont émises qu'onduleur alimenté, plus une fois après son arrêt. */
static bool inverter_frames_enabled(void)
{
    return ((smb.step > SMB_STANDBY) && (smb.step <= SMB_EMERGENCY)) || inverter.raz_data_after_stop_flag;
}

/* 0x2BA0000 : état batteries */
static void send_battery_status(void)
{
    uFLAGSBYTE f;
    data[0] = battery.soc;
    f.byte = 0;
    f.sFLAGSBITS.b0 = battery.ibs[0].system_running_flag;
    f.sFLAGSBITS.b1 = battery.ibs[0].error_flag;
    f.sFLAGSBITS.b2 = battery.ibs[0].low_soc_warning_flag;
    f.sFLAGSBITS.b3 = battery.ibs[0].balancing_flag;
    f.sFLAGSBITS.b4 = battery.ibs[1].system_running_flag;
    f.sFLAGSBITS.b5 = battery.ibs[1].error_flag;
    f.sFLAGSBITS.b6 = battery.ibs[1].low_soc_warning_flag;
    f.sFLAGSBITS.b7 = battery.ibs[1].balancing_flag;
    data[1] = f.byte;
    put_u16(2, (uint16_t)battery.output_current);
    data[4] = (uint8_t)battery.ibs[0].avg_cell_temperature;
    data[5] = (uint8_t)battery.ibs[1].avg_cell_temperature;
    data[6] = battery.ibs[0].sop;
    data[7] = battery.ibs[1].sop;
    transmit(0x2BA0000UL, CAN_DLC_8);
}

/* 0x2AC0000 : mode et défauts onduleur */
static void send_inverter_flags(void)
{
    uFLAGSBYTE f;
    data[0] = inverter.mode;
    f.byte = 0;
    f.sFLAGSBITS.b0 = inverter.error_flag;
    f.sFLAGSBITS.b1 = inverter.device_switched_on_flag;
    f.sFLAGSBITS.b2 = inverter.load_status_flag;
    f.sFLAGSBITS.b3 = inverter.charging_status_flag;
    f.sFLAGSBITS.b4 = inverter.ac_charging_status_flag;
    f.sFLAGSBITS.b5 = inverter.scc_charging_status_flag;
    data[1] = f.byte;
    f.byte = 0;
    f.sFLAGSBITS.b0 = inverter.bus_overvoltage_error_flag;
    f.sFLAGSBITS.b1 = inverter.bus_undervoltage_error_flag;
    f.sFLAGSBITS.b2 = inverter.bus_soft_fail_error_flag;
    f.sFLAGSBITS.b3 = inverter.voltage_too_low_error_flag;
    f.sFLAGSBITS.b4 = inverter.voltage_too_high_error_flag;
    f.sFLAGSBITS.b5 = inverter.overtemperature_error_flag;
    f.sFLAGSBITS.b6 = inverter.fan_locked_error_flag;
    f.sFLAGSBITS.b7 = inverter.battery_voltage_high_error_flag;
    data[2] = f.byte;
    f.byte = 0;
    f.sFLAGSBITS.b0 = inverter.overcharge_error_flag;
    f.sFLAGSBITS.b1 = inverter.overload_error_flag;
    f.sFLAGSBITS.b2 = inverter.over_current_error_flag;
    f.sFLAGSBITS.b3 = inverter.soft_start_error_flag;
    f.sFLAGSBITS.b4 = inverter.self_test_error_flag;
    f.sFLAGSBITS.b5 = inverter.ac_output_dc_voltage_too_high_error_flag;
    f.sFLAGSBITS.b6 = inverter.battery_disconnected_error_flag;
    f.sFLAGSBITS.b7 = inverter.current_sensor_error_flag;
    data[3] = f.byte;
    f.byte = 0;
    f.sFLAGSBITS.b0 = inverter.battery_short_circuit_error_flag;
    f.sFLAGSBITS.b1 = inverter.mppt_overload_error_flag;
    f.sFLAGSBITS.b2 = inverter.apparent_power_very_high_error_flag;
    data[4] = f.byte;
    f.byte = 0;
    f.sFLAGSBITS.b0 = inverter.line_fail_warning_flag;
    f.sFLAGSBITS.b1 = inverter.opv_short_warning_flag;
    f.sFLAGSBITS.b2 = inverter.bus_soft_fail_error_flag;
    f.sFLAGSBITS.b3 = inverter.overtemperature_warning_flag;
    f.sFLAGSBITS.b4 = inverter.fan_locked_warning_flag;
    f.sFLAGSBITS.b5 = inverter.battery_voltage_high_warning_flag;
    f.sFLAGSBITS.b6 = inverter.battery_voltage_low_warning_flag;
    f.sFLAGSBITS.b7 = inverter.battery_under_shutdown_voltage_warning_flag;
    data[5] = f.byte;
    f.byte = 0;
    f.sFLAGSBITS.b0 = inverter.battery_derating_warning_flag;
    f.sFLAGSBITS.b1 = inverter.overload_warning_flag;
    f.sFLAGSBITS.b2 = inverter.eeprom_warning_flag;
    f.sFLAGSBITS.b3 = inverter.power_limit_warning_flag;
    f.sFLAGSBITS.b4 = inverter.pv_voltage_high_warning_flag;
    f.sFLAGSBITS.b5 = inverter.mppt_overload_warning_flag;
    f.sFLAGSBITS.b6 = inverter.battery_too_low_to_charge_warning_flag;
    data[6] = f.byte;
    transmit(0x2AC0000UL, CAN_DLC_7);
}

/* 0x2AC0001 : mesures onduleur */
static void send_inverter_measures(void)
{
    uFLAGSBYTE f;
    put_u16(0, inverter.moy_ac_output_apparent_power);
    put_u16(2, inverter.ac_output_active_power);
    put_u16(4, inverter.ac_output_voltage);
    data[6] = inverter.dcac_temp;
    f.byte = 0;
    f.sFLAGSBITS.b0 = inverter.toggle_qmod_flag;
    f.sFLAGSBITS.b1 = inverter.toggle_qpiws_flag;
    f.sFLAGSBITS.b2 = inverter.toggle_qpigs_flag;
    data[7] = f.byte;
    transmit(0x2AC0001UL, CAN_DLC_8);
    inverter.raz_data_after_stop_flag = 0;
}

/* 0x2C00000 : état SMB et drapeaux */
static void send_smb_status(void)
{
    uFLAGSBYTE f;
    data[0] = smb.step;
    f.byte = 0;
    f.sFLAGSBITS.b0 = smb.critical_error_flag;
    f.sFLAGSBITS.b1 = inverter.general_error_flag;
    f.sFLAGSBITS.b2 = fcs.general_error_flag;
    f.sFLAGSBITS.b3 = fcs.ppmh2_error_flag;
    f.sFLAGSBITS.b4 = inverter.recieve_flag;
    data[1] = f.byte;
    f.byte = 0;
    f.sFLAGSBITS.b0 = battery.ibs[0].watchdog.error;
    f.sFLAGSBITS.b1 = battery.ibs[1].watchdog.error;
    f.sFLAGSBITS.b2 = inverter.watchdog_error_flag;
    f.sFLAGSBITS.b3 = fcs.watchdog.error;
    f.sFLAGSBITS.b4 = fcs.startup_error_flag;
    f.sFLAGSBITS.b5 = measures.ac_ambiant_temp.error_flag;
    f.sFLAGSBITS.b6 = measures.temp2.error_flag;
    f.sFLAGSBITS.b7 = smb.software_error_flag;
    data[2] = f.byte;
    f.byte = 0;
    f.sFLAGSBITS.b0 = hmi.battery_mode_button.state;
    f.sFLAGSBITS.b1 = hmi.hybrid_mode_button.state;
    data[3] = f.byte;
    data[4] = fcs.step;
    f.byte = 0;
    f.sFLAGSBITS.b0 = battery.soc_high_level_warning_flag;
    f.sFLAGSBITS.b1 = battery.soc_low_level_warning_flag;
    f.sFLAGSBITS.b2 = inverter.apparent_power_high_warning_flag;
    f.sFLAGSBITS.b3 = battery.sop_warning_flag;
    f.sFLAGSBITS.b4 = battery.sop_error_flag;
    data[5] = f.byte;
    f.byte = 0;
    f.sFLAGSBITS.b0 = battery.soc_very_very_high_level_error_flag;
    f.sFLAGSBITS.b1 = battery.soc_very_low_level_error_flag;
    f.sFLAGSBITS.b2 = battery.overvoltage_warning_flag;
    f.sFLAGSBITS.b3 = battery.overvoltage.error;
    data[6] = f.byte;
    f.byte = 0; /* (origine : octet non remis à zéro, mais b4 et b7 valaient déjà 0) */
    f.sFLAGSBITS.b0 = battery.overcurrent_warning_flag;
    f.sFLAGSBITS.b1 = battery.overcurrent.error;
    f.sFLAGSBITS.b2 = battery.overtemperature_charge.error;
    f.sFLAGSBITS.b3 = battery.overtemperature_discharge.error;
    f.sFLAGSBITS.b5 = battery.overtemperature_charge_warning_flag;
    f.sFLAGSBITS.b6 = battery.overtemperature_discharge_warning_flag;
    data[7] = f.byte;
    transmit(0x2C00000UL, CAN_DLC_8);
}

/* 0x2C00001 : entrées / sorties brutes */
static void send_smb_io(void)
{
    uFLAGSBYTE f;
    f.byte = 0;
    f.sFLAGSBITS.b0 = Board_GetButtonLeft();
    f.sFLAGSBITS.b1 = Board_GetButtonMiddle();
    f.sFLAGSBITS.b2 = Board_GetButtonRight();
    f.sFLAGSBITS.b3 = Board_GetButtonBatteryMode();
    f.sFLAGSBITS.b4 = Board_GetButtonHybridMode();
    data[0] = f.byte;
    f.byte = 0;
    f.sFLAGSBITS.b0 = Board_GetDcacInputRelay();
    f.sFLAGSBITS.b1 = Board_GetDcacPrechargeRelay();
    f.sFLAGSBITS.b2 = Board_GetSupplyPrechargeRelay();
    f.sFLAGSBITS.b3 = Board_GetFcsOutputRelay();
    f.sFLAGSBITS.b4 = Board_GetBuzzer();
    f.sFLAGSBITS.b5 = Board_GetLedLeft();
    f.sFLAGSBITS.b6 = Board_GetLedMiddle();
    f.sFLAGSBITS.b7 = Board_GetLedRight();
    data[1] = f.byte;
    data[2] = (uint8_t)(measures.temp_1_raw / 100);
    data[3] = (uint8_t)(measures.temp_2_raw / 100);
    put_u16(4, measures.supply_voltage_raw);
    put_u16(6, measures.bus_voltage_raw);
    transmit(0x2C00001UL, CAN_DLC_8);
}

/* 0x2C00002 : mesures converties */
static void send_smb_measures(void)
{
    data[0] = (uint8_t)measures.ac_ambiant_temp.value_8;
    data[1] = (uint8_t)measures.temp2.value_8;
    put_u16(4, measures.supply_voltage);
    put_u16(6, measures.bus_voltage);
    transmit(0x2C00002UL, CAN_DLC_8);
}

/* 0x2C00003 : courants */
static void send_currents(void)
{
    put_u16(0, (uint16_t)fcs.output_current);
    put_u16(2, ems.requested_fcs_current);
    put_u16(4, (uint16_t)battery.output_current);
    put_u16(6, (uint16_t)ems.system_output_current);
    transmit(0x2C00003UL, CAN_DLC_8);
}

/* 0x2C00004 : version logicielle */
static void send_version(void)
{
    data[0] = SMB_VERSION;
    data[1] = SMB_SUBVERSION;
    data[2] = SMB_SUBSUBVERSION;
    data[3] = (uint8_t)hmi.buzzer_5_minutes.cnt;
    data[4] = (uint8_t)hmi.buzzer_5_minutes.cnt >> 8; /* TODO(bug 4) : cast avant décalage, toujours 0 */
    data[5] = (uint8_t)hmi.raz_erreur_ihm_flag;
    transmit(0x2C00004UL, CAN_DLC_6);
}

/* 0x2BA0001 : détail batteries */
static void send_battery_details(void)
{
    put_u16(0, (uint16_t)battery.ibs[0].output_current);
    put_u16(2, (uint16_t)battery.ibs[1].output_current);
    data[4] = (uint8_t)battery.ibs[0].max_cell_temperature;
    data[5] = (uint8_t)battery.ibs[1].max_cell_temperature;
    data[6] = (uint8_t)battery.ibs[0].max_board_temperature;
    data[7] = (uint8_t)battery.ibs[1].max_board_temperature;
    transmit(0x2BA0001UL, CAN_DLC_8);
}

/* 0x2BA0003 : réservé (contenu nul à l'origine) */
static void send_battery_reserved(void)
{
    transmit(0x2BA0003UL, CAN_DLC_2);
}

/* 0x2FC1000 : présence SMB vers la FCCB */
static void send_fcs_heartbeat(void)
{
    data[0] = 0;
    data[1] = 1;
    transmit(0x2FC1000UL, CAN_DLC_2);
}

/* 0xCFF0027 : refroidissement */
static void send_cooling(void)
{
    data[0] = cooling.inverter_pwm;
    data[1] = cooling.ambiant_pwm;
    data[2] = cooling.dcdc_pwm;
    data[3] = cooling.fans_duty_cycle;
    transmit(0xCFF0027UL, CAN_DLC_4);
}

/* 0xCFF0127 : gestion énergie */
static void send_ems(void)
{
    data[0] = 0; /* min_ems_current : toujours 0 (régulation dynamique désactivée à l'origine) */
    data[1] = 0; /* max_ems_current : idem */
    data[2] = (uint8_t)ems.requested_fcs_current / 100; /* TODO(bug 4) : cast avant division */
    put_u16(3, (uint16_t)battery.ibs[0].max_charge_current);
    put_u16(5, (uint16_t)battery.ibs[0].max_discharge_current);
    transmit(0xCFF0127UL, CAN_DLC_7);
}

void CanTx_SendNext(void)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
        data[i] = 0;
    output_msg.field.idType = CAN_FRAME_EXT;

    switch (can_frame_ind)
    {
    case 1:  send_battery_status(); break;
    case 3:  if (inverter_frames_enabled()) send_inverter_flags(); break;
    case 4:  if (inverter_frames_enabled()) send_inverter_measures(); break;
    case 6:  send_smb_status(); break;
    case 7:  send_smb_io(); break;
    case 8:  send_smb_measures(); break;
    case 9:  send_currents(); break;
    case 17: send_version(); break;
    case 19: send_battery_reserved(); break;
    case 20: send_fcs_heartbeat(); break;
    case 21: send_cooling(); break;
    case 22: send_ems(); break;
    case 23: send_battery_details(); break;
    default: break;
    }

    can_frame_ind++;
    if (can_frame_ind > CAN_TX_CYCLE_LENGTH - 1)
        can_frame_ind = 0;
}
