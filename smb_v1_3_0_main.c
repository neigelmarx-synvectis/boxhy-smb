/**
 * @brief Code SMB BOXHY
 * @file main.c
 *
 * @author Julian VERNIER, Neigel MARX, Gael GRZELAK
 * @version 1.2.3
 * @date 2022-10-06
 * 
 */

#define FCY 6000000
#include <libpic30.h>
#include "mcc_generated_files/system.h"
#include "mcc_generated_files/mcc.h"
#include <stdio.h>
#include <math.h>

// <editor-fold defaultstate="collapsed" desc="TYPEDEF">
typedef union
{
    struct
    {
        bool b0 : 1, b1 : 1, b2 : 1, b3 : 1, b4 : 1, b5 : 1, b6 : 1, b7 : 1;
    } sFLAGSBITS;
    uint8_t byte;
} uFLAGSBYTE;

typedef struct
{
    uint16_t voltage;            // Tension batterie (0.01V)
    int16_t output_current;      // Courant batterie (0.01A)
    int8_t avg_cell_temperature; // Temperature moyenne des cellules (1ï¿½C)
    uint8_t soc;                 // SOC de la batterie (%)
    bool system_running_flag;    // 0 : Batterie en standby / 1 : Batterie en fonctionnement
    bool balancing_flag;         // 1 : Batterie en balancing
    bool low_soc_warning_flag;   // 1 : Flag d'avertissement SOC bas
    bool error_flag;             // 0 : Pas d'erreur / 1 : Au moins une erreur detectee
    bool power_output_1_state_flag;
    bool power_output_2_state_flag;

    uint8_t watchdog_cnt;
    bool watchdog_error_flag;
    
    int16_t max_charge_current;
    int16_t max_discharge_current;
} sIBS;
// </editor-fold>

#include "CTN_SENSOR_V03.C"

// <editor-fold defaultstate="collapsed" desc="DEFINES">
#define TANK_EV_PWM 1
#define FANS_1_PWM 2
#define FANS_2_PWM 3

#define RS232FRAME_NONE 0
#define RS232FRAME_QPIGS 1
#define RS232FRAME_QMOD 2
#define RS232FRAME_QPIWS 3

#define INVERTER_UNDEFINED_MODE 0
#define INVERTER_FAULT_MODE 1
#define INVERTER_POWER_ON_MODE 2
#define INVERTER_STANDBY_MODE 3
#define INVERTER_AC_SOURCE_MODE 4
#define INVERTER_BATTERY_MODE 5
#define INVERTER_POWER_SAVING_MODE 6

// ADC
#define ADC_CHANNEL_NB 4
#define ADC_SAMPLE_NB 50

// ADC INDICES
#define TEMP_1_ADC_IND 0
#define TEMP_2_ADC_IND 1
#define SUPPLY_VOLTAGE_ADC_IND 2
#define BUS_VOLTAGE_ADC_IND 3

// SMB STATES
#define SMB_INIT 10
#define SMB_AUTOCHECK 20
#define SMB_STANDBY 30
#define SMB_MAINTENANCE 40
#define SMB_DCAC_START_1 50
#define SMB_DCAC_START_2 52
#define SMB_DCAC_START_3 54
#define SMB_BATTERY_MODE 100
#define SMB_FCS_START_1 120
#define SMB_FCS_START_2 122
#define SMB_HYBRID_MODE 150
#define SMB_FC_STOP_1 200
#define SMB_FC_STOP_2 201
#define SMB_DCAC_STOP 210
#define SMB_STOPPING 240
#define SMB_EMERGENCY 250

// FCCB STATES
#define FCCB_INIT 10
#define FCCB_AUTOCHECK 20
#define FCCB_STANDBY 30
#define FCCB_MAINTENANCE 40
#define FCCB_START_PURGE 50
#define FCCB_VOLTAGE_CHECK 52
#define FCCB_RAMP_UP 54
#define FCCB_WARM_UP 56
#define FCCB_OPERATING 100
#define FCCB_SHUTDOWN 200
#define FCCB_EMERGENCY 250
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="CONSTANTS">
const uint8_t battery_soc_very_low_level = 10;
const uint8_t battery_soc_low_level = 25;
const uint8_t battery_soc_high_level = 70;
const uint8_t battery_soc_very_high_level = 85;
const uint8_t battery_soc_very_very_high_level = 95;
const uint8_t battery_soc_very_very_high_level_timeout = 50;
const uint8_t battery_soc_very_low_level_timeout = 50;
const uint16_t buzzer_5_minutes_timeout = 300;

const uint16_t ac_output_apparent_power_high_level = 5000; 
const uint16_t ac_output_apparent_power_very_high_level = 5200;
const uint16_t ac_output_apparent_power_very_critical_high_level = 7200;
const uint8_t ac_output_apparent_power_very_high_level_timeout = 90;
const uint8_t ac_output_apparent_power_very_critical_high_level_timeout = 40;

const uint16_t fcs_low_current_timeout = 600;

//VERSION SOFTWARE
const uint8_t version = 1;
const uint8_t subversion = 2;
const uint8_t subsubversion = 7;

const uint8_t watchdog_timeout = 30;
const uint8_t dcac_watchdog_timeout = 180;
const uint8_t dcac_recieve_timeout = 5;
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="VARIABLES">
// GENERAL
uint8_t step = 0;
bool software_error_flag = 0;
uint16_t _100ms_timer_ticks = 0;
uint16_t step_100ms_cnt = 0;
bool _100ms_timer_flag = 0;
uint8_t _100ms_timer_cnt = 0;
bool _1s_timer_flag = 0;
uint8_t _1s_timer_cnt = 0;
bool _300ms_timer_flag = 0;
uint8_t _300ms_timer_cnt = 0;
uint8_t current_ind = 0;
int16_t system_output_current = 0;
bool critical_error_flag = 0;
uint8_t ac_fans_duty_cycle = 0;
uint8_t data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
bool ac_output_apparent_power_very_high_level_error_flag = 0;
uint8_t ac_output_apparent_power_very_high_level_error_cpt = 0;
bool ac_output_apparent_power_high_level_warning_flag = 0;
uint8_t buzzer_period_cpt = 0;
bool fcs_start_2_timeout_flag = 0;
uint16_t fcs_start_2_timeout_cnt = 0;
bool smb_reset_order_flag = 0;
uint16_t buzzer_5_minutes_cnt = 0;
bool buzzer_5_minutes_flag = 0;

// FAN Control variables
uint8_t dcdc_temperature;
float inverter_int_part = 0;
float ambiant_int_part = 0;
float dcdc_int_part = 0;
uint8_t inverter_cooling_pwm;
uint8_t ambiant_cooling_pwm;
uint8_t dcdc_cooling_pwm;

// ADC
uint32_t adc_sum_tab[ADC_CHANNEL_NB]; // Table avec la somme des rï¿½sultats ADC pour moyennage
uint16_t adc_moy_tab[ADC_CHANNEL_NB]; // Table avec les rï¿½sultats ADC moyennï¿½s
uint8_t adc_samples_cnt = 0;
uint8_t adc_current_channel_ind = 0;
bool adc_samples_ready_flag = 0;
bool adc_conversion_done_flag = 0;
uint8_t adc_current_channel = 0;
uint16_t adc_conversion_result = 0;

// CAN
uFLAGSBYTE can_flagsbyte;
bool can_ready_flag = 0;
uint8_t can_frame_ind = 0;
CAN_MSG_OBJ output_msg;
CAN_MSG_OBJ keepalivemsg;
CAN_MSG_OBJ input_msg;
CAN_MSG_OBJ rsmsgdebug;
CAN_MSG_OBJ command_msg;

// RS232
uint8_t rs232_frame_id = RS232FRAME_NONE;
char rs232_rx_byte = ' ';
char rs232_rx_frame[120];
uint8_t rs232_rx_byte_cnt = 0;
bool rs232_frame_recieved_flag = 0;
bool rs232_frame_recieving_flag = 0;
uint8_t rs232_frame_id_ind = 0;
bool toggle_qpigs_flag = 0;
bool toggle_qpiws_flag = 0;
bool toggle_qmod_flag = 0;
// uint16_t crc = 0;
uint16_t i = 0;

//IHM 
bool raz_erreur_ihm_flag = 0;
//bool raz_erreur_ihm_ok_flag = 0;

// DCAC
uint8_t inverter_mode = INVERTER_UNDEFINED_MODE;
bool inverter_error_flag = 0;
bool bus_overvoltage_error_flag = 0;
bool bus_undervoltage_error_flag = 0;
bool bus_soft_fail_error_flag = 0;
bool line_fail_warning_flag = 0;
bool opv_short_warning_flag = 0;
bool inverter_voltage_too_low_error_flag = 0;
bool inverter_voltage_too_high_error_flag = 0;
bool overtemperature_warning_flag = 0;
bool overtemperature_error_flag = 0;
bool fan_locked_warning_flag = 0;
bool fan_locked_error_flag = 0;
bool battery_voltage_high_warning_flag = 0;
bool battery_voltage_high_error_flag = 0;
bool battery_voltage_low_warning_flag = 0;
bool overcharge_error_flag = 0;
bool battery_under_shutdown_voltage_warning_flag = 0;
bool battery_derating_warning_flag = 0;
bool overload_warning_flag = 0;
bool overload_error_flag = 0;
bool eeprom_warning_flag = 0;
bool inverter_over_current_error_flag = 0;
bool inverter_soft_start_error_flag = 0;
bool self_test_error_flag = 0;
bool ac_output_dc_voltage_too_high_error_flag = 0;
bool battery_disconnected_error_flag = 0;
bool current_sensor_error_flag = 0;
bool battery_short_circuit_error_flag = 0;
bool power_limit_warning_flag = 0;
bool pv_voltage_high_warning_flag = 0;
bool mppt_overload_error_flag = 0;
bool mppt_overload_warning_flag = 0;
bool battery_too_low_to_charge_warning_flag = 0;

bool load_status_flag = 0;
uint16_t ac_output_voltage = 0;
uint16_t ac_output_frequency = 0;
uint16_t grid_voltage = 0;
uint16_t grid_frequency = 0;
uint16_t ac_output_apparent_power = 0;
uint8_t ac_output_apparent_power_i = 0;
uint16_t moy_ac_output_apparent_power = 0;
uint16_t tab_ac_output_apparent_power[10];
uint16_t ac_output_active_power = 0;
uint8_t output_load_percent = 0;
uint16_t battery_charging_current = 0;
uint8_t dcac_temp = 0;
uint16_t battery_discharge_current = 0;
uint16_t inverter_battery_voltage = 0;
uint16_t internal_bus_voltage = 0;
bool charging_status_flag = 0;
bool scc_charging_status_flag = 0;
bool ac_charging_status_flag = 0;
bool device_switched_on_flag = 0;
bool charging_to_floating_mode_flag = 0;
bool dcac_watchdog_error_flag = 0;
bool dcac_recieve_flag = 0;
uint8_t dcac_watchdog_cnt = 0;
uint8_t dcac_recieve_cnt = 0;
bool dcac_general_error_flag = 0;
bool inverter_raz_data_after_stop_flag = 0;

// INPUTS
bool imh_left_button_flag = 0;
uint8_t imh_left_button_cnt = 0;
bool imh_middle_button_flag = 0;
uint8_t imh_middle_button_cnt = 0;
bool imh_right_button_flag = 0;
uint8_t imh_right_button_cnt = 0;
bool start_button_flag = 0;
uint8_t start_button_cnt = 0;
sCTN_SENSOR ac_ambiant_temp;
sCTN_SENSOR temp2;
uint16_t supply_voltage = 0;
uint16_t bus_voltage = 0;
uint16_t supply_voltage_b = 0;
uint16_t bus_voltage_b = 0;
uint16_t temp_1_b = 0;
uint16_t temp_2_b = 0;
bool battery_mode_button_flag = 0;
uint8_t battery_mode_button_cnt = 0;
bool hybrid_mode_button_flag = 0;
uint8_t hybrid_mode_button_cnt = 0;
bool maintenance_mode_request_flag = 0;

// FCS
bool fcs_watchdog_error_flag = 0;
bool fcs_startup_error_flag = 0;
uint8_t fcs_watchdog_cnt = 0;
uint8_t fcs_step = 0;
bool fcs_critical_error_flag = 0;
bool fcs_minor_error_flag = 0;
bool fcs_ppmh2_error_flag = 0;
bool fcs_general_error_flag = 0;
int16_t fcs_output_current = 0;
int16_t fcs_output_current_moy = 0;
int32_t fcs_output_current_sum = 0;
bool fcs_short_circuit_ongoing_flag = 0;
uint16_t requested_fcs_current = 0;
uint16_t requested_fcs_current_sat = 0;
bool fcs_dcdc_temperature_warning_flag = 0;
bool fcs_low_current_warning_flag = 0;
uint8_t fcs_short_circuit_interval_cnt = 0;
bool fcs_startup_retry_flag = 0;
bool fcs_low_current_timeout_flag = 0;
uint8_t fcs_low_current_timeout_cpt = 0;

// BATTERY
sIBS ibs_battery_0;
sIBS ibs_battery_1;
bool ibs_keep_alive_index = 0;
float mant_sum;
float expo_pow;
uint8_t battery_soc = 0;
int16_t battery_output_current = 0;
int16_t battery_output_current_moy = 0;
int32_t battery_output_current_sum = 0;
bool battery_soc_high_level_warning_flag = 0;
bool battery_soc_very_very_high_level_error_flag = 0;
bool battery_soc_very_very_high_level_error_cpt = 0;
bool battery_soc_low_level_warning_flag = 0;
bool battery_soc_very_low_level_error_flag = 0;
uint8_t battery_soc_very_low_level_error_cpt = 0;

// DCAC disconnect error
uint8_t dcac_soc_disconnect_level = 20; //20% de SOC
uint8_t dcac_soc_disconnect_level_error_cpt = 0;
uint8_t dcac_soc_disconnect_level_error_timeout = 50; // 10 = 100ms
bool dcac_soc_disconnect_level_error_flag = false;

// BATTERY overcurrent disconnect
bool battery_overcurrent_warning_flag = 0;
bool battery_overcurrent_error_flag = 0;
uint8_t battery_overcurrent_error_cpt = 0;
uint16_t battery_overcurrent_timeout = 450;
uint8_t battery_hard_overcurrent_timeout = 8;
int16_t ibs_battery_global_max_charge_current = 0;

// BATTERY overtemperature disconnect
int8_t battery_0_max_temperature = 0; //températures cellules
int8_t battery_1_max_temperature = 0; //températures cellules
int8_t battery_max_temperature = 0; //températures cellules
int8_t battery_0_actual_max_board_temperature = 0; //température BMS
int8_t battery_1_actual_max_board_temperature = 0; //température BMS
int8_t battery_actual_max_board_temperature = 0; //température BMS

int8_t battery_max_charge_temperature = 45; //45°C max en température de charge (sécu IBS à 50°C)
bool battery_overtemperature_charge_warning_flag = 0;
bool battery_overtemperature_charge_error_flag = 0;
uint8_t battery_overtemperature_charge_error_cpt = 0;
uint8_t battery_overtemperature_charge_timeout = 50; //5s

int8_t battery_max_discharge_temperature = 55; //55°C max en température de charge (sécu IBS à 60°C)
bool battery_overtemperature_discharge_warning_flag = 0;
bool battery_overtemperature_discharge_error_flag = 0;
uint8_t battery_overtemperature_discharge_error_cpt = 0;
uint8_t battery_overtemperature_discharge_timeout = 50; //5s

//int8_t battery_max_board_temperature = 50; //50°C max en température de charge (sécu IBS à 55°C)
//bool battery_board_overtemperature_warning_flag = 0;
//bool battery_board_overtemperature_error_flag = 0;
uint8_t battery_board_overtemperature_error_cpt = 0;
uint8_t battery_board_overtemperature_timeout = 50; //5s

// BATTERY overvoltage disconnect
uint16_t battery_overvoltage_level = 6000; //Tension max du bus à 60V
bool battery_overvoltage_warning_flag = 0;
bool battery_overvoltage_error_flag = 0;
uint8_t battery_overvoltage_error_cpt = 0;
const uint8_t battery_overvoltage_timeout = 50; //5s

// BATTERY SOP disconnect
uint8_t ibs_battery_0_sop = 0;
uint8_t ibs_battery_1_sop = 0;
uint8_t ibs_battery_sop_warning_low_level = 30;
uint8_t ibs_battery_sop_warning_very_low_level = 20;
//uint8_t ibs_battery_sop_error_cpt = 0;
bool ibs_battery_sop_warning_flag = 0;
uint8_t ibs_battery_sop_timeout = 100;
bool ibs_battery_sop_error_flag = 0;

// Gestion ï¿½nergï¿½tique
float max_ems_current = 0;
float min_ems_current = 0;

// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="FUNCTIONS PROTOTYPES">
void TMR1_interruptHandler(void);
void ADC1_interruptHandler(void);
void UART1_Rx_interruptHandler(void);

void PWM2_SetDutyCycle(uint8_t duty_cycle);
// void TANKEV_SetDutyCycle(uint8_t duty_cycle);
// void PWM1_SetDutyCycle(uint8_t duty_cycle);
// void DAC_SetOutput(uint8_t voltage_reference);

uint8_t charToDec(char input_char);
// uint16_t cal_crc_half(char *pin, uint8_t len);

void FCS_Start(void);
void FCS_Stop(void);
void FCS_MaintenanceMode(void);
void FCS_CriticalStop(void);
void FCS_ResetErrors(void);
void FCS_SetCurrentLimit(uint16_t current_limit);

float min (float a, float b);
float max(float a, float b);
int16_t IBS_CalculateCurrent(uint8_t data0, uint8_t data1, uint8_t data2);
uint8_t PID_controller(float Kp, float Ki, float Kd, float *int_part, float target, float measurement);
float saturation(float min, float value, float max);
float dynamicsLimiter (float limitedValue, float setPoint, float ramp);
// </editor-fold>

int main(void)
{   
    // Initialize the device
    SYSTEM_Initialize();

    // Assign interrupts
    TMR1_SetInterruptHandler(&TMR1_interruptHandler);
    ADC1_SetInterruptHandler(&ADC1_interruptHandler);
    UART1_SetRxInterruptHandler(&UART1_Rx_interruptHandler);

    // <editor-fold defaultstate="collapsed" desc="DAC INITIALIZE">
    //    CVR1CONbits.VREFSEL = 1;    //CVREFIN = CVREF+
    //    CVR1CONbits.CVROE = 1;      //Voltage level is output on CVREF1O pin
    //    CVR1CONbits.CVRSS = 0;      //Comparator reference source is AVDD-AVSS
    //    CVR1CONbits.CVR = 0;        //Voltage reference set to 0V
    //    CVR1CONbits.CVREN = 1;      //Comparator voltage reference circuit is powered on
    CVR1CONbits.CVREN = 0; // Comparator voltage reference circuit is powered off
    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="PWM INITIALIZE">
    PTCON = 0x0000;                       // Disable PWM module
    PTPER = 1000;                         // Set PWM period
    PHASE1 = PHASE2 = PHASE3 = 0;         // No phase shift
    PDC1 = PDC2 = PDC3 = 0;               // Set duty cycle to 0
    DTR1 = DTR2 = DTR3 = 0;               // No dead time
    ALTDTR1 = ALTDTR2 = ALTDTR3 = 0;      // No dead time
    IOCON1 = IOCON2 = IOCON3 = 0x8800;    // Set PWM Mode to Push-Pull
    PWMCON1 = PWMCON2 = PWMCON3 = 0x0000; // Set Primary Time Base, Edge-Aligned Mode and Independent Duty Cycles
    FCLCON1 = FCLCON2 = FCLCON3 = 0x0003; // Fault current limit disabled
    PTCON2 = 0x0001;                      // 1:1 prescaler (24MHz clock)
    PTCON = 0x8000;                       // Enable PWM module
    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="ADC INITIALIZE">
    ADC1_Enable();
    ADC1_ChannelSelect(TEMP_1_ADC_IND);
    ADC1_InterruptEnable();
    ADC1_SoftwareTriggerEnable();

    for (i = 0; i < ADC_CHANNEL_NB; i++)
    {
        adc_moy_tab[i] = 0; // RAZ du tableau
        adc_sum_tab[i] = 0; // RAZ du tableau
    }
    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="CAN INITIALIZE">
    CAN1_TransmitEnable();
    CAN1_ReceiveEnable();
    CAN1_OperationModeSet(CAN_NORMAL_2_0_MODE);

    output_msg.msgId = 0x08200;
    output_msg.field.frameType = CAN_FRAME_DATA;
    output_msg.field.idType = CAN_FRAME_EXT;
    output_msg.field.dlc = CAN_DLC_8;
    output_msg.data = data;

    keepalivemsg.msgId = 0x180AD000;
    keepalivemsg.field.frameType = CAN_FRAME_DATA;
    keepalivemsg.field.idType = CAN_FRAME_EXT;
    keepalivemsg.field.dlc = CAN_DLC_1;
    keepalivemsg.data = data;

    rsmsgdebug.msgId = 0x5555;
    rsmsgdebug.field.frameType = CAN_FRAME_DATA;
    rsmsgdebug.field.idType = CAN_FRAME_EXT;
    rsmsgdebug.field.dlc = CAN_DLC_8;
    rsmsgdebug.data = data;

    command_msg.msgId = 0x2FC1001;
    command_msg.field.frameType = CAN_FRAME_DATA;
    command_msg.field.idType = CAN_FRAME_EXT;
    command_msg.field.dlc = CAN_DLC_4;
    command_msg.data = data;

    input_msg.data = data;
    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="CTN INITIALIZE">
    ctnSensorInit(&ac_ambiant_temp);
    ac_ambiant_temp = (sCTN_SENSOR){
        .high_level = 75,
        .low_level = 0,
        .warning_high_level = 50,
    };

    ctnSensorInit(&temp2);
    temp2 = (sCTN_SENSOR){
        .high_level = 90,
        .low_level = 0,
        .warning_high_level = 50,
    };
    // </editor-fold>
    
    TMR1_Start();
    TMR5_Start();

    step = 10;
    _100ms_timer_ticks = 0; // RAZ

    for (;;)
    {
        // <editor-fold defaultstate="collapsed" desc="CHECK ERROR">.
        if (software_error_flag 
			|| ibs_battery_0.error_flag 
			|| ibs_battery_1.error_flag 
			|| ibs_battery_0.watchdog_error_flag 
			|| ibs_battery_1.watchdog_error_flag 
			|| fcs_ppmh2_error_flag 
			|| battery_soc_very_low_level_error_flag 
			|| battery_soc_very_very_high_level_error_flag
            || battery_overtemperature_discharge_error_flag
            || battery_overtemperature_charge_error_flag
//            || battery_board_overtemperature_error_flag
            || battery_overcurrent_error_flag
            || battery_overvoltage_error_flag
            || ibs_battery_sop_error_flag)
            critical_error_flag = 1;

        if (fcs_startup_error_flag 
			|| fcs_critical_error_flag 
			|| fcs_minor_error_flag 
			|| fcs_watchdog_error_flag)
            fcs_general_error_flag = 1;
        else
            fcs_general_error_flag = 0;

        if (dcac_watchdog_error_flag 
			|| inverter_error_flag 
			|| ac_ambiant_temp.error_flag 
			/*|| ac_output_apparent_power_very_high_level_error_flag*/)
            dcac_general_error_flag = 1;
        
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="STEPS">
        switch (step)
        {
        // <editor-fold defaultstate="collapsed" desc="10 - INIT">
        case SMB_INIT:	//Etape 10
			// Replay de pr?charge alim 24V
            SUPPLY_PRECHARGE_RELAY_SetHigh();
            
            // Pilotage contacteur précharge DCAC
            DCAC_PRECHARGE_RELAY_SetLow();

            // Pilotage contacteur direct DCAC
            DCAC_INPUT_RELAY_SetLow();

            // Pilotage contacteur FC
            FCS_OUTPUT_RELAY_SetLow();
            if (_100ms_timer_ticks >= 20) // Apr?s 2s
            {
                step = 20; // Passage ? l'?tape 20
                _100ms_timer_ticks = 0;
            }
            break;
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="20 - AUTO-CHECK">
        case SMB_AUTOCHECK:	//Etape 20
			//Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = LOW
			//FCS_OUTPUT_RELAY = LOW
		    if (critical_error_flag || dcac_general_error_flag)
            {
                step = SMB_EMERGENCY; // Passage ? l'?tape 240
                _100ms_timer_ticks = 0;
            }
            else if (_100ms_timer_ticks >= 30) // Apr?s 3s
            {
                step = SMB_STANDBY; // Passage ? l'?tape 30
                _100ms_timer_ticks = 0;
            }
            break;
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="30 - STANDBY">
        case SMB_STANDBY:	//Etape 30
			//Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = LOW
			//FCS_OUTPUT_RELAY = LOW
            if (critical_error_flag || dcac_general_error_flag)
                step = SMB_EMERGENCY; // Passage ? l'?tape 240
            else if (battery_mode_button_flag && !dcac_general_error_flag)
                step = SMB_DCAC_START_1; //Passage à l'étape 50
            else if (maintenance_mode_request_flag)
            {
                step = SMB_MAINTENANCE; // Passage ? l'?tape 40
                FCS_MaintenanceMode();
            }
            _100ms_timer_ticks = 0;
            break;
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="40 - MAINTENANCE MODE">
        case SMB_MAINTENANCE:	//Etape 40
            if (critical_error_flag || dcac_general_error_flag)
                step = SMB_EMERGENCY; //Passage à l'étape 250
            else if (!maintenance_mode_request_flag)
                step = SMB_STOPPING; //Passage à l'étape 240
            _100ms_timer_ticks = 0;
            break;
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="50-99 - GOING BATTERY MODE">
        case SMB_DCAC_START_1:	//Etape 50
			//Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = LOW
			//FCS_OUTPUT_RELAY = LOW
            if (critical_error_flag || dcac_general_error_flag)
            {
                step = SMB_EMERGENCY; // Passage ? l'?tape 240
                _100ms_timer_ticks = 0;
            }
            else if (_100ms_timer_ticks >= 30) // Apr?s 3s
            {
                step = SMB_DCAC_START_2; // Passage ? l'?tape 52
                _100ms_timer_ticks = 0;
            }
            DCAC_PRECHARGE_RELAY_SetHigh();
            break;

        case SMB_DCAC_START_2:	//Etape 52
			//Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = HIGH
			//DCAC_INPUT_RELAY = LOW
			//FCS_OUTPUT_RELAY = LOW
            if (critical_error_flag || dcac_general_error_flag)
            {
                step = SMB_EMERGENCY; // Passage ? l'?tape 240
                _100ms_timer_ticks = 0;
            }
            else if (_100ms_timer_ticks >= 5) // Apr?s 0.5s
            {
                DCAC_PRECHARGE_RELAY_SetLow();
                DCAC_INPUT_RELAY_SetHigh();
                step = SMB_DCAC_START_3; // Passage ? l'?tape 54
                _100ms_timer_ticks = 0;
            }
            //DCAC_INPUT_RELAY_SetHigh();
            break;

        case SMB_DCAC_START_3:	//Etape 54
			//Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = HIGH
			//FCS_OUTPUT_RELAY = LOW
            if ((critical_error_flag) || (inverter_mode == INVERTER_FAULT_MODE))
            {
                step = SMB_EMERGENCY; // Passage ? l'?tape 240
                _100ms_timer_ticks = 0;
            }
            else if ((inverter_mode > 3) || (_100ms_timer_ticks >= 150)) // Apr?s 1.5s et si inverter_mode > 3
            {
                step = SMB_BATTERY_MODE; // Passage ? l'?tape 100
                _100ms_timer_ticks = 0;
            }
            break;

        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="100 - BATTERY MODE">
        case SMB_BATTERY_MODE:	//Etape 100
			//Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = HIGH
			//FCS_OUTPUT_RELAY = LOW
			// Pilotage contacteur direct DCAC
            if (dcac_soc_disconnect_level_error_flag) DCAC_INPUT_RELAY_SetLow(); //Désactivation DCAC
            else if(!dcac_soc_disconnect_level_error_flag) DCAC_INPUT_RELAY_SetHigh(); //Activation DCAC
            if (critical_error_flag || dcac_general_error_flag || ac_output_apparent_power_very_high_level_error_flag)
            {
                step = SMB_EMERGENCY; // Passage ? l'?tape 250
                _100ms_timer_ticks = 0;
            }
            else if (!battery_mode_button_flag && !hybrid_mode_button_flag)
            {
                step = SMB_STOPPING; // Passage ? l'?tape 240
                _100ms_timer_ticks = 0;
            }
            else if (hybrid_mode_button_flag && !fcs_general_error_flag && (battery_soc <= battery_soc_high_level))
            {
                step = SMB_FCS_START_1;	//Passage à l'étape 120
                _100ms_timer_ticks = 0;
            }
            else if (hybrid_mode_button_flag && fcs_general_error_flag)
            {
                step = SMB_EMERGENCY; // Passage ? l'?tape 250
                _100ms_timer_ticks = 0;
            }
			else if ((!hybrid_mode_button_flag && !battery_mode_button_flag) )//|| battery_overcurrent_error_flag)
            {
                step = SMB_DCAC_STOP;
            }
            FCS_OUTPUT_RELAY_SetLow();

            _100ms_timer_ticks = 0;
            break;
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="120-122		- GOING HYBRID MODE">
        case SMB_FCS_START_1: //Etape 120 
			//Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = HIGH
			//FCS_OUTPUT_RELAY = LOW
			if (dcac_soc_disconnect_level_error_flag) DCAC_INPUT_RELAY_SetLow(); //Désactivation DCAC
            if (critical_error_flag)
            {
                step = SMB_EMERGENCY; // Passage ? l'?tape 240
                _100ms_timer_ticks = 0;
            }
            else if (fcs_step == FCCB_STANDBY) //FCCB en étape 30
            {
                FCS_Start();
                FCS_OUTPUT_RELAY_SetHigh();
                step = SMB_FCS_START_2; // Passage à l'étape 122
                _100ms_timer_ticks = 0;
            }
            else if ((_100ms_timer_ticks >= 30) 
					|| (fcs_step == FCCB_EMERGENCY)
					|| fcs_general_error_flag 
					|| dcac_general_error_flag) // Apr?s 3s ou FCCB_EMERGENCY ou Pas mode hybride flag ou erreur dcac
            {
                FCS_OUTPUT_RELAY_SetLow();
                fcs_startup_error_flag = 1;
                FCS_Stop();
                step = SMB_FC_STOP_1; // Passage ? l'?tape 200
                _100ms_timer_ticks = 0;
            }
            break;
        case SMB_FCS_START_2: //Etape 122
			//Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = HIGH
			//FCS_OUTPUT_RELAY = HIGH
			if (dcac_soc_disconnect_level_error_flag) DCAC_INPUT_RELAY_SetLow(); //Désactivation DCAC
            else if (critical_error_flag)
            {
                step = SMB_EMERGENCY; // Passage ? l'?tape 240
                _100ms_timer_ticks = 0;
            }
            else if ((fcs_step == FCCB_EMERGENCY) 
					|| fcs_general_error_flag 
					|| dcac_general_error_flag)
            {
                FCS_OUTPUT_RELAY_SetLow();
                fcs_startup_error_flag = 1;
                step = SMB_FC_STOP_1; //Passage en étape 200
                _100ms_timer_ticks = 0;
            }
            else if (fcs_step == FCCB_OPERATING)
            {
                step = SMB_HYBRID_MODE; // Passage ? l'?tape 150
                _100ms_timer_ticks = 0;
            }
            else if ((fcs_start_2_timeout_flag == 1) && (fcs_startup_retry_flag == 0))
            {
                fcs_startup_retry_flag = 1;
                fcs_start_2_timeout_flag = 0; 
                step = SMB_FCS_START_1;
                _100ms_timer_ticks = 0;
            }
            else if (fcs_startup_retry_flag == 1)
            {
                step = SMB_EMERGENCY;
                _100ms_timer_ticks = 0;
            }
            break;
		// </editor-fold>
        
		// <editor-fold defaultstate="collapsed" desc="150 - SMB_HYBRID_MODE">
		case SMB_HYBRID_MODE: //Etape 150
			//Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = HIGH
			//FCS_OUTPUT_RELAY = HIGH
			if (dcac_soc_disconnect_level_error_flag) DCAC_INPUT_RELAY_SetLow(); //Désactivation DCAC
            else if (!dcac_soc_disconnect_level_error_flag) DCAC_INPUT_RELAY_SetHigh(); //Activation DCAC
            if (critical_error_flag)
            {
                step = SMB_EMERGENCY; // Passage en étape 240
                _100ms_timer_ticks = 0;
            }
            else if (!hybrid_mode_button_flag
                    || battery_soc >= battery_soc_very_high_level
                    || (fcs_step != FCCB_OPERATING) 
                    || fcs_general_error_flag 
                    || dcac_general_error_flag
                    || fcs_low_current_timeout_flag
                    || ac_output_apparent_power_very_high_level_error_flag)
                    //|| fcs_low_current_warning_flag)
                    //|| battery_overcurrent_error_flag)
            {
                fcs_low_current_timeout_flag = 0;   
                step = SMB_FC_STOP_1;
                _100ms_timer_ticks = 0;
            }
            break;

        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="200 - SMB_FC_STOP_1">
        case SMB_FC_STOP_1: //Etape 200
            //Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = HIGH
			//FCS_OUTPUT_RELAY = HIGH

            // Pilotage contacteur direct DCAC
            if (dcac_soc_disconnect_level_error_flag) DCAC_INPUT_RELAY_SetLow(); //Désactivation DCAC
          
            // Transitions
            else if (critical_error_flag || fcs_general_error_flag || dcac_general_error_flag)
            {
                step = SMB_EMERGENCY; // Passage en étape 240 
                _100ms_timer_ticks = 0;
            }
            else if (_100ms_timer_ticks >= 2)   //Après 200ms
            {
                FCS_Stop(); //Requête arrêt de la pile
                step = SMB_FC_STOP_2;
                _100ms_timer_ticks = 0;
            }
            break;
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="201 - SMB_FC_STOP_2">
        case SMB_FC_STOP_2: //Etape 201
            //Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = HIGH
			//FCS_OUTPUT_RELAY = HIGH

            // Pilotage contacteur direct DCAC
            if (dcac_soc_disconnect_level_error_flag) DCAC_INPUT_RELAY_SetLow(); //Désactivation DCAC

            // Pilotage contacteur FC
            FCS_OUTPUT_RELAY_SetLow();

            // Pilotage Buzzer - Bip pour signaler l'arrêt de la pile - 
            if ((_100ms_timer_ticks == 25) && (fcs_step == FCCB_SHUTDOWN))
            {
                BUZZER_SetHigh();
            }
            else if (_100ms_timer_ticks == 30)
            {
                BUZZER_SetLow();
            }
            else if ((_100ms_timer_ticks == 35) && (fcs_step == FCCB_SHUTDOWN))
            {
                BUZZER_SetHigh();
            }
            else if (_100ms_timer_ticks == 40)
            {
                BUZZER_SetLow();
            }
            else
            {
                BUZZER_SetLow();
            }

            // Transitions
            if (critical_error_flag || fcs_step == FCCB_EMERGENCY)
            {
                step = SMB_EMERGENCY;   // Passage en étape 240
                _100ms_timer_ticks = 0; // RAZ compteur
            }
            else if (fcs_step == FCCB_STANDBY) // A MODIFIER POUR INTEGRER LA VALEUR CORRESPONDANT A L'ARRET
            {
                step = SMB_BATTERY_MODE; // Passage en mode batterie après arrêt pile
                _100ms_timer_ticks = 0;  // RAZ Compteur
            }
            break;
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="210 - SMB_DCAC_STOP">
        case SMB_DCAC_STOP:
            //Lorsque l'on passe dans cette étape, les états des relais sont :
			//SUPPLY_PRECHARGE_RELAY = HIGH
			//DCAC_PRECHARGE_RELAY = LOW
			//DCAC_INPUT_RELAY = HIGH
			//FCS_OUTPUT_RELAY = Dépend mode batteries ou hybride
            
            // Pilotage contacteur direct DCAC
            DCAC_INPUT_RELAY_SetLow();

            // Transitions
            if (critical_error_flag)
            {
                step = SMB_EMERGENCY;   // Passage en étape 240
                _100ms_timer_ticks = 0; // RAZ compteur
            }
            else if (_100ms_timer_ticks == 15)
            {
                inverter_raz_data_after_stop_flag = 1;
                inverter_mode = 0;
                moy_ac_output_apparent_power = 0;
                ac_output_active_power = 0;
                ac_output_voltage = 0;

                step = SMB_STANDBY;     // Passage en mod estandby
                _100ms_timer_ticks = 0; // RAZ compteur
            }
            break;
        // </editor-fold>
            
        // <editor-fold defaultstate="collapsed" desc="240 - STOPPING MODE">
        case SMB_STOPPING: // Etape 240
            if (critical_error_flag)
            {
                step = SMB_EMERGENCY; // Passage ? l'?tape 22
                FCS_CriticalStop();

                _100ms_timer_ticks = 0;
                step_100ms_cnt = 0;
            }
            else if (_100ms_timer_ticks > 0)
            {
                DCAC_PRECHARGE_RELAY_SetLow();

                step_100ms_cnt++;
                if ((step_100ms_cnt == 1) && (fcs_step = FCCB_OPERATING)) // Apr?s 100ms
                {
                    FCS_Stop();
                }
                else if (step_100ms_cnt == 2) // Apr?s 200ms
                {
                    FCS_OUTPUT_RELAY_SetLow();
                }
                else if (step_100ms_cnt == 15) // Apr?s 1.5s
                {
                    DCAC_INPUT_RELAY_SetLow();
                }
                else if (step_100ms_cnt >= 50)
                {
                    DCAC_PRECHARGE_RELAY_SetLow();
                    FCS_OUTPUT_RELAY_SetLow();
                    DCAC_INPUT_RELAY_SetLow();
                    BUZZER_SetLow();

                    inverter_raz_data_after_stop_flag = 1;
                    inverter_mode = 0;
                    moy_ac_output_apparent_power = 0;
                    ac_output_active_power = 0;
                    ac_output_voltage = 0;

                    step = 30;

                    step_100ms_cnt = 0; // RAZ cpt
                }
                _100ms_timer_ticks = 0;
            }
            break;
        // </editor-fold>    

        // <editor-fold defaultstate="collapsed" desc="250 - SMB_EMERGENCY">
        case SMB_EMERGENCY: //Etape 250
            // Replay de pr?charge alim 24V
            SUPPLY_PRECHARGE_RELAY_SetHigh();
            
            // Pilotage contacteur précharge DCAC
            DCAC_PRECHARGE_RELAY_SetLow();

            // Pilotage contacteur direct DCAC
            DCAC_INPUT_RELAY_SetLow();

            // Pilotage contacteur FC
            FCS_OUTPUT_RELAY_SetLow();

            // Pilotage Buzzer
            if (fcs_ppmh2_error_flag && !buzzer_5_minutes_flag) // Bip continu si défaut fuite H2 durant 5 minutes
            {
                BUZZER_SetHigh();
            }
            else if ((_100ms_timer_ticks > 2) && !fcs_ppmh2_error_flag && !buzzer_5_minutes_flag) // Bip alterné si autre défaut
            {
                BUZZER_Toggle();
                _100ms_timer_ticks = 0;
            }
            else if (buzzer_5_minutes_flag)
            {
                BUZZER_SetLow();
            }

            // Autres actions
//            if (fcs_step != FCCB_EMERGENCY && _100ms_timer_ticks > 2)
//            {
//                _100ms_timer_ticks = 0;
//                FCS_Stop(); // Arrêt de la pile si pas encore arreté.
//            }
            inverter_mode = 0;
            moy_ac_output_apparent_power = 0;
            ac_output_active_power = 0;
            ac_output_voltage = 0;

            // Transitions
            if (imh_middle_button_flag || smb_reset_order_flag) // Sortie de mode défaut
            {
                smb_reset_order_flag = 0;
                imh_middle_button_flag = 0;
                FCS_ResetErrors();
                buzzer_5_minutes_flag = 0;
                
                software_error_flag = 0;
                ibs_battery_0.error_flag = 0; 
                ibs_battery_1.error_flag = 0;
                ibs_battery_0.watchdog_error_flag = 0;
                ibs_battery_1.watchdog_error_flag = 0;
                fcs_ppmh2_error_flag = 0;
                battery_soc_very_low_level_error_flag = 0;
                battery_soc_very_very_high_level_error_flag = 0;
                battery_overtemperature_discharge_error_flag = 0;
                battery_overtemperature_charge_error_flag = 0;
//                battery_board_overtemperature_error_flag = 0;
                battery_overcurrent_error_flag = 0;
                battery_overvoltage_error_flag = 0;
                ibs_battery_sop_error_flag = 0;
//                battery_soo_error_flag = 0;
                critical_error_flag = 0;

                fcs_startup_error_flag = 0;
                fcs_critical_error_flag = 0;
                fcs_minor_error_flag = 0;
                fcs_watchdog_error_flag = 0;
                fcs_general_error_flag = 0;

                fcs_general_error_flag = 0;
                fcs_startup_retry_flag = 0;
                fcs_start_2_timeout_flag = 0;

                dcac_watchdog_error_flag = 0;
                inverter_error_flag = 0;
                ac_ambiant_temp.error_flag = 0;
                ac_output_apparent_power_very_high_level_error_flag = 0;
                dcac_general_error_flag = 0;
                ctnSensorInit(&temp2);
                ctnSensorInit(&ac_ambiant_temp);
                raz_erreur_ihm_flag = 1;
                
                if (_100ms_timer_ticks == 8)
                {
                    BUZZER_SetHigh();
                }
                else if (_100ms_timer_ticks == 9)
                {
                    BUZZER_SetLow();
                }
                else if (_100ms_timer_ticks == 10)
                {
                    BUZZER_SetHigh();
                }
                else if (_100ms_timer_ticks == 11)
                {
                    BUZZER_SetLow();
                    _100ms_timer_ticks = 0;
                }
                else
                {
                    BUZZER_SetLow();
                }
                
                
                step = SMB_AUTOCHECK;
                _100ms_timer_ticks = 0; // RAZ compteur
            }
            else // Si pas de transitions
            {
                if (_100ms_timer_ticks > 12)
                {
                    _100ms_timer_ticks = 0; // RAZ compteur
                }
            }
            break;
        // </editor-fold>

        default:
            // Replay de pr?charge alim 24V
            SUPPLY_PRECHARGE_RELAY_SetHigh();
            
            // Pilotage contacteur précharge DCAC
            DCAC_PRECHARGE_RELAY_SetLow();

            // Pilotage contacteur direct DCAC
            DCAC_INPUT_RELAY_SetLow();

            // Pilotage contacteur FC
            FCS_OUTPUT_RELAY_SetLow();
            
            // Informations
            _100ms_timer_ticks = 0;
            software_error_flag = 1;
            
            // Transitions
            step = SMB_EMERGENCY;
            break;
        }
        // </editor-fold>        

        // <editor-fold defaultstate="collapsed" desc="TRAITEMENT CAN">
        if (CAN1_ReceivedMessageCountGet() > 0)
        {
            if (CAN1_Receive(&input_msg))
            {
                switch (input_msg.msgId)
                {
                    // <editor-fold defaultstate="collapsed" desc="BATTERY 0">
                    //                    case 0x18FF01D0:
                    //                        ibs_battery_0.voltage = (uint16_t)((0 | (int32_t)input_msg.data[3] | ((int32_t)input_msg.data[2] << 8) | ((int32_t)input_msg.data[1] << 16) | ((int32_t)input_msg.data[0] << 24)) / 10);
                    //                        ibs_battery_0.watchdog_cnt = 0;
                    //                        break;
                case 0x18FF02D0:
                    ibs_battery_0.output_current = IBS_CalculateCurrent(input_msg.data[0], input_msg.data[1], input_msg.data[2]);
                    ibs_battery_0_sop = input_msg.data[4];
                    ibs_battery_0.watchdog_cnt = 0;
                    break;
                case 0x18FF03D0:                                                                                                 // CELL TEMPERATURE
                    ibs_battery_0.avg_cell_temperature = (int8_t)((input_msg.data[5] | ((int16_t)input_msg.data[4] << 8)) / 10); // 1ï¿½C
                    battery_0_max_temperature = (int8_t)((input_msg.data[3] + ((int16_t)input_msg.data[2] * 255))/10);  //temp max en °C
                    break;
                case 0x18FF04D0:
                    battery_0_actual_max_board_temperature = (int8_t)((input_msg.data[3] + ((int16_t)input_msg.data[2] * 255))/10);  //temp max en °C
                    break;
                case 0x18FF05D0:                                                                                  // STATE
                    ibs_battery_0.soc = (uint8_t)((input_msg.data[1] | ((int16_t)input_msg.data[0] << 8)) / 100); // 1%
                    break;
                case 0x18FF07D0: // INFOS
                    can_flagsbyte.byte = input_msg.data[1];
                    ibs_battery_0.system_running_flag = can_flagsbyte.sFLAGSBITS.b0;
                    ibs_battery_0.balancing_flag = can_flagsbyte.sFLAGSBITS.b1;

                    can_flagsbyte.byte = input_msg.data[2];
                    ibs_battery_0.error_flag = !can_flagsbyte.sFLAGSBITS.b1;
                    ibs_battery_0.low_soc_warning_flag = can_flagsbyte.sFLAGSBITS.b5;
                    break;
                    
                case 0x18FF3CD0: // INFOS
                    can_flagsbyte.byte = input_msg.data[1];
                    break;
                    
//                case 0x18FF03D0: //Infos températures
//                    battery_0_max_temperature = (int16_t)(input_msg.data[2] + input_msg.data[3] * 255);
                case 0x18FF0CD0: // INFOS
                    //can_flagsbyte.byte = input_msg.data[1];
                    ibs_battery_0.max_discharge_current = (IBS_CalculateCurrent(input_msg.data[0], input_msg.data[1], input_msg.data[2])) * (1);
                    ibs_battery_0.max_charge_current = (IBS_CalculateCurrent(input_msg.data[4], input_msg.data[5], input_msg.data[6])) * (1);
                    break;
                    // </editor-fold>

                    // <editor-fold defaultstate="collapsed" desc="BATTERY 1">
                    //                    case 0x18FF01D1:
                    //                        ibs_battery_1.voltage = (uint16_t)((0 | (int32_t)input_msg.data[3] | ((int32_t)input_msg.data[2] << 8) | ((int32_t)input_msg.data[1] << 16) | ((int32_t)input_msg.data[0] << 24)) / 10);
                    //                        ibs_battery_1.watchdog_cnt = 0;
                    //                        break;
                case 0x18FF02D1:
                    ibs_battery_1.output_current = IBS_CalculateCurrent(input_msg.data[0], input_msg.data[1], input_msg.data[2]);
                    ibs_battery_1_sop = input_msg.data[4];
                    ibs_battery_1.watchdog_cnt = 0;
                    break;
                case 0x18FF03D1:                                                                                                 // CELL TEMPERATURE
                    ibs_battery_1.avg_cell_temperature = (int8_t)((input_msg.data[5] | ((int16_t)input_msg.data[4] << 8)) / 10); // 1ï¿½C
                    battery_1_max_temperature = (int8_t)((input_msg.data[3] + ((int16_t)input_msg.data[2] * 255)) / 10); //temp max en °C
                    break;
                case 0x18FF04D1:
                    battery_1_actual_max_board_temperature = (int8_t)((input_msg.data[3] + ((int16_t)input_msg.data[2] * 255))/10);  //temp max en °C
                    break;
                case 0x18FF05D1:                                                                                  // STATE
                    ibs_battery_1.soc = (uint8_t)((input_msg.data[1] | ((int16_t)input_msg.data[0] << 8)) / 100); // 1%
                    break;
                case 0x18FF07D1: // INFOS
                    can_flagsbyte.byte = input_msg.data[1];
                    ibs_battery_1.system_running_flag = can_flagsbyte.sFLAGSBITS.b0;
                    ibs_battery_1.balancing_flag = can_flagsbyte.sFLAGSBITS.b1;

                    can_flagsbyte.byte = input_msg.data[2];
                    ibs_battery_1.error_flag = !can_flagsbyte.sFLAGSBITS.b1;
                    ibs_battery_1.low_soc_warning_flag = can_flagsbyte.sFLAGSBITS.b5;
                    break;
                    
                case 0x18FF3CD1: // INFOS
                    can_flagsbyte.byte = input_msg.data[1];
//                    ibs_battery_1.max_discharge_current = ((float)(input_msg.data[0] + input_msg.data[1] * 255)) / 20.0 - 1600.0;
//                    ibs_battery_1.max_charge_current = ((float)(input_msg.data[2] + input_msg.data[3] * 255)) / 20.0 - 1600.0;
                    break;
                    
//                case 0x18FF03D1: //Infos températures
//                battery_1_max_temperature = (int16_t)(input_msg.data[2] + input_msg.data[3] * 255);
                case 0x18FF0CD1: // INFOS
                    ibs_battery_1.max_discharge_current = (IBS_CalculateCurrent(input_msg.data[0], input_msg.data[1], input_msg.data[2])) * (1);
                    ibs_battery_1.max_charge_current = (IBS_CalculateCurrent(input_msg.data[4], input_msg.data[5], input_msg.data[6])) * (1);
                    break;
                // </editor-fold>

                // <editor-fold defaultstate="collapsed" desc="MANUAL">
                case 0x2C01D00:
                    if ((step == 40) && (input_msg.data[0] == 0x82))
                    {
                        PWM2_SetDutyCycle(input_msg.data[1]);
                    }
                    break;
                case 0x2C01D01:
                    if ((step == 40) && (input_msg.data[0] == 0x82))
                    {
                        if (input_msg.data[1] == 1)
                            DCAC_INPUT_RELAY_Toggle();
                        else
                            DCAC_INPUT_RELAY_SetLow();
                    }
                    break;
                case 0x2C01D02:
                    if ((step == 40) && (input_msg.data[0] == 0x82))
                    {
                        if (input_msg.data[1] == 1)
                            DCAC_PRECHARGE_RELAY_Toggle();
                        else
                            DCAC_PRECHARGE_RELAY_SetLow();
                    }
                    break;
                case 0x2C01D03:
                    if ((step == 40) && (input_msg.data[0] == 0x82))
                    {
                        if (input_msg.data[1] == 1)
                            SUPPLY_PRECHARGE_RELAY_Toggle();
                        else
                            SUPPLY_PRECHARGE_RELAY_SetLow();
                    }
                    break;
                case 0x2C01D04:
                    if ((step == 40) && (input_msg.data[0] == 0x82))
                    {
                        if (input_msg.data[1] == 1)
                            FCS_OUTPUT_RELAY_Toggle();
                        else
                            FCS_OUTPUT_RELAY_SetLow();
                    }
                    break;
                case 0x2C01D05:
                    if ((step == 40) && (input_msg.data[0] == 0x82))
                    {
                        if (input_msg.data[1] == 1)
                            BUZZER_Toggle();
                        else
                            BUZZER_SetLow();
                    }
                    break;
                case 0x2C01D06:
                    if ((step == 40) && (input_msg.data[0] == 0x82))
                    {
                        if (input_msg.data[1] == 1)
                            IHM_LEFT_BUTTON_LED_Toggle();
                        else
                            IHM_LEFT_BUTTON_LED_SetLow();
                    }
                    break;
                case 0x2C01D07:
                    if ((step == 40) && (input_msg.data[0] == 0x82))
                    {
                        if (input_msg.data[1] == 1)
                            IHM_MIDDLE_BUTTON_LED_Toggle();
                        else
                            IHM_MIDDLE_BUTTON_LED_SetLow();
                    }
                    break;
                case 0x2C01D08:
                    if ((step == 40) && (input_msg.data[0] == 0x82))
                    {
                        if (input_msg.data[1] == 1)
                            IHM_RIGHT_BUTTON_LED_Toggle();
                        else
                            IHM_RIGHT_BUTTON_LED_SetLow();
                    }
                    break;

                case 0x2C0100F:
                    if ((step == 30) && (input_msg.data[0] == 0x82) && (input_msg.data[7] == 0xFF))
                        maintenance_mode_request_flag = 1;
                    else if ((step == 40) && (input_msg.data[0] == 0x82))
                        maintenance_mode_request_flag = 0;
                    break;
                // </editor-fold>

                // <editor-fold defaultstate="collapsed" desc="FCCB">
                case 0x2FC0000:
                    fcs_watchdog_cnt = 0;
                    fcs_watchdog_error_flag = 0;
                    fcs_step = input_msg.data[0];
                    can_flagsbyte.byte = input_msg.data[1];
                    fcs_critical_error_flag = can_flagsbyte.sFLAGSBITS.b0;
                    fcs_minor_error_flag = can_flagsbyte.sFLAGSBITS.b1;
                    fcs_ppmh2_error_flag = can_flagsbyte.sFLAGSBITS.b2;
                    can_flagsbyte.byte = input_msg.data[2];
                    fcs_low_current_warning_flag = can_flagsbyte.sFLAGSBITS.b1;
                    fcs_dcdc_temperature_warning_flag = can_flagsbyte.sFLAGSBITS.b2;
                    can_flagsbyte.byte = input_msg.data[5];
                    fcs_short_circuit_ongoing_flag = can_flagsbyte.sFLAGSBITS.b0;
                    fcs_output_current = (uint16_t)(0 | input_msg.data[6] | (input_msg.data[7] << 8));
                    break;
                case 0x2FC0004:
                    fcs_short_circuit_interval_cnt = input_msg.data[6];
                    break;
                case 0x2FC0003:
                    dcdc_temperature = input_msg.data[3];
                    break;
                    // </editor-fold>

                    // <editor-fold defaultstate="collapsed" desc="RESET SMB TELEMETRIE">
                case 0x2C01009:
                    if ((step == 250) && (input_msg.data[0] == 0x82)) smb_reset_order_flag = 1;
                    else smb_reset_order_flag = 0;
                    // </editor-fold>
                    
                // <editor-fold defaultstate="collapsed" desc="RAZ ERREURS IHM">
                case 0x25C1000:
                    if (input_msg.data[0] == 1) raz_erreur_ihm_flag = 0;
                    // </editor-fold>    
                    
                default:
                    break;
                }
                for (i = 0; i < 8; i++)
                    input_msg.data[i] = 0; // RAZ
            }
        }
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="TRAITEMENT MESURES">
        // <editor-fold defaultstate="collapsed" desc="LECTURE DES RESULTATS & MOYENNAGE">
        else if (adc_conversion_done_flag)
        {
            adc_sum_tab[adc_current_channel_ind] += ADC1_ConversionResultGet(adc_current_channel);

            adc_current_channel_ind++;
            if (adc_current_channel_ind >= ADC_CHANNEL_NB)
            {
                adc_samples_cnt++;
                if (adc_samples_cnt >= ADC_SAMPLE_NB)
                {
                    for (i = 0; i < ADC_CHANNEL_NB; i++)
                    {
                        adc_moy_tab[i] = (uint16_t)(adc_sum_tab[i] / ADC_SAMPLE_NB); // Calcul de la moyenne
                        adc_sum_tab[i] = 0;                                          // RAZ du tableau
                    }

                    adc_samples_ready_flag = 1;
                    adc_samples_cnt = 0;
                }
                adc_current_channel_ind = 0;
            }

            // <editor-fold defaultstate="collapsed" desc="AIGUILLAGE DE LA CHANNEL SUIVANTE">
            switch (adc_current_channel_ind)
            {
            case TEMP_1_ADC_IND:
                adc_current_channel = AUX_TEMP_1;
                break;
            case TEMP_2_ADC_IND:
                adc_current_channel = AUX_TEMP_2;
                break;
            case BUS_VOLTAGE_ADC_IND:
                adc_current_channel = BUS_VOLTAGE;
                break;
            case SUPPLY_VOLTAGE_ADC_IND:
                adc_current_channel = SUPPLY_VOLTAGE;
                break;
            default:
                adc_current_channel_ind = TEMP_1_ADC_IND;
                adc_current_channel = AUX_TEMP_1;
                break;
            }

            ADC1_ChannelSelect(adc_current_channel);
            // </editor-fold>

            adc_conversion_done_flag = false;
        }
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="CONVERSION DES MESURES">
        else if (adc_samples_ready_flag)
        {
            temp_1_b = adc_moy_tab[TEMP_1_ADC_IND];
            temp_2_b = adc_moy_tab[TEMP_2_ADC_IND];
            bus_voltage_b = adc_moy_tab[BUS_VOLTAGE_ADC_IND];
            supply_voltage_b = adc_moy_tab[SUPPLY_VOLTAGE_ADC_IND];

            supply_voltage = adc_moy_tab[SUPPLY_VOLTAGE_ADC_IND] * 1.969;
            bus_voltage = adc_moy_tab[BUS_VOLTAGE_ADC_IND] * 1.962;

            ac_ambiant_temp.value = convertTemperatureUSP10976(adc_moy_tab[TEMP_1_ADC_IND]);
            temp2.value = convertTemperatureUSP10976(adc_moy_tab[TEMP_2_ADC_IND]);

            adc_samples_ready_flag = 0;
        }
        // </editor-fold>
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="TOUTES LES 100ms">
        if (_100ms_timer_flag)
        {
            // <editor-fold defaultstate="collapsed" desc="GESTION DES ERREURS">
            ctnSensorCheck(&ac_ambiant_temp);
            ctnSensorCheck(&temp2);

            // <editor-fold defaultstate="collapsed" desc="BATTERY SOC">
            // <editor-fold defaultstate="collapsed" desc="HIGH LEVEL">
            if (battery_soc > battery_soc_very_very_high_level)
            {
                battery_soc_high_level_warning_flag = 1;
                if (battery_soc > battery_soc_very_very_high_level)
                {
                    battery_soc_very_very_high_level_error_cpt++;
                    if (battery_soc_very_very_high_level_error_cpt > battery_soc_very_very_high_level_timeout)
                    {
                        battery_soc_very_very_high_level_error_flag = 1;
                        battery_soc_very_very_high_level_error_cpt = 0;
                    }
                    else
                        battery_soc_very_very_high_level_error_flag = 0;
                }
            }
            else
            {
                battery_soc_high_level_warning_flag = 0;
                battery_soc_very_very_high_level_error_flag = 0;
            }
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="LOW LEVEL">
            if (battery_soc < battery_soc_low_level)
            {
                battery_soc_low_level_warning_flag = 1;
                if (battery_soc < battery_soc_very_low_level)
                {
                    battery_soc_very_low_level_error_cpt++;
                    if (battery_soc_very_low_level_error_cpt > battery_soc_very_low_level_timeout)
                    {
                        battery_soc_very_low_level_error_flag = 1;
                        battery_soc_very_low_level_error_cpt = 0;
                    }
                    else
                        battery_soc_very_low_level_error_flag = 0;
                }
            }
            else
            {
                battery_soc_low_level_warning_flag = 0;
                battery_soc_very_low_level_error_flag = 0;
            }
            // </editor-fold>
            
            // <editor-fold defaultstate="collapsed" desc="DCAC_DISCONNECT">
            if (battery_soc < dcac_soc_disconnect_level && moy_ac_output_apparent_power > 50)
            {
                dcac_soc_disconnect_level_error_cpt++;
                dcac_soc_disconnect_level_error_flag = 
                        dcac_soc_disconnect_level_error_cpt >= dcac_soc_disconnect_level_error_timeout;
            }
            else if (dcac_soc_disconnect_level_error_flag && battery_soc < battery_soc_low_level)
            {
                dcac_soc_disconnect_level_error_cpt = 0;
                dcac_soc_disconnect_level_error_flag = true;
            }
            else
            {
                dcac_soc_disconnect_level_error_cpt = 0;
                dcac_soc_disconnect_level_error_flag = false;
            }
            // </editor-fold>
            
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="OUTPUT POWER">
            if (moy_ac_output_apparent_power > ac_output_apparent_power_high_level)
            {
                //ac_output_apparent_power_high_level_warning_flag = 1;
                if ((moy_ac_output_apparent_power > ac_output_apparent_power_very_high_level) || (moy_ac_output_apparent_power > ac_output_apparent_power_very_critical_high_level))
                {
                    ac_output_apparent_power_very_high_level_error_cpt++;
                    if (((moy_ac_output_apparent_power > ac_output_apparent_power_very_high_level) && (ac_output_apparent_power_very_high_level_error_cpt > ac_output_apparent_power_very_high_level_timeout))
                            || ((moy_ac_output_apparent_power > ac_output_apparent_power_very_critical_high_level) && (ac_output_apparent_power_very_high_level_error_cpt > ac_output_apparent_power_very_critical_high_level_timeout)))
                    {
                        ac_output_apparent_power_very_high_level_error_flag = 1;
                        ac_output_apparent_power_very_high_level_error_cpt = 0;
                    }
                }
                else ac_output_apparent_power_very_high_level_error_cpt = 0;
            }
            /*else
            {
                ac_output_apparent_power_very_high_level_error_cpt = 0;
                //ac_output_apparent_power_high_level_warning_flag = 0;
            }*/
            // </editor-fold>
            
            // <editor-fold defaultstate="collapsed" desc="BATTERY OVERCURRENT SHUTDOWN">
            if (ibs_battery_0.output_current > 6900 ||                   
                ibs_battery_0.output_current < (ibs_battery_0.max_charge_current * 0.85) ||
                ibs_battery_1.output_current > 6900 || 
                ibs_battery_1.output_current < (ibs_battery_1.max_charge_current * 0.85))
            {
                battery_overcurrent_warning_flag = 1;
                battery_overcurrent_error_cpt++;
//                if (battery_overcurrent_error_cpt > battery_hard_overcurrent_timeout)     //XXXXX A TERMINER SI BESOIN
//                {
//                    battery_overcurrent_error_flag = 1;
//                    battery_overcurrent_error_cpt = 0;
//                }
                if (battery_overcurrent_error_cpt > battery_overcurrent_timeout)
                {
                    battery_overcurrent_error_flag = 1;
                    battery_overcurrent_error_cpt = 0;
                }
            }
            else
            {
                battery_overcurrent_error_cpt = 0;
                battery_overcurrent_warning_flag = 0;
            }
            // </editor-fold>
            
            // <editor-fold defaultstate="collapsed" desc="BATTERY OVERTEMPERATURE SHUTDOWN">
            
            battery_max_temperature = max(battery_0_max_temperature, battery_1_max_temperature);
            battery_actual_max_board_temperature = max(battery_1_actual_max_board_temperature, battery_0_actual_max_board_temperature);
            
            if ((battery_max_temperature > battery_max_charge_temperature) && (battery_output_current < 0))
            {
                battery_overtemperature_charge_warning_flag = 1;
                battery_overtemperature_charge_error_cpt++;
                if (battery_overtemperature_charge_error_cpt > battery_overtemperature_charge_timeout)
                {
                    battery_overtemperature_charge_error_flag = 1;
                    battery_overtemperature_charge_error_cpt = 0;
                }
            }
            else
            {
                battery_overtemperature_charge_warning_flag = 0;
                battery_overtemperature_charge_error_cpt = 0;
            }
            if ((battery_max_temperature > battery_max_discharge_temperature) && (battery_output_current > 0))
            {
                battery_overtemperature_discharge_warning_flag = 1;
                battery_overtemperature_discharge_error_cpt++;
                if (battery_overtemperature_discharge_error_cpt > battery_overtemperature_discharge_timeout) //Erreur si temp batt > 
                {
                    battery_overtemperature_discharge_error_flag = 1;
                    battery_overtemperature_discharge_error_cpt = 0;
                }
            }
            else
            {
                battery_overtemperature_discharge_warning_flag = 0;
                battery_overtemperature_discharge_error_cpt = 0;
            }
            
//            if (battery_actual_max_board_temperature > battery_max_board_temperature)
//            {
//                battery_board_overtemperature_warning_flag = 1;
//                battery_board_overtemperature_error_cpt++;
//                if (battery_board_overtemperature_error_cpt > battery_board_overtemperature_timeout) //Erreur si temp batt > 
//                {
//                    battery_board_overtemperature_error_flag = 1;
//                    battery_board_overtemperature_error_cpt = 0;
//                }
//            }
//            else
//            {
//                battery_board_overtemperature_warning_flag = 0;
//                battery_board_overtemperature_error_cpt = 0;
//            }
            // </editor-fold>
            
            // <editor-fold defaultstate="collapsed" desc="BATTERY OVERVOLTAGE SHUTDOWN">
            if (bus_voltage > battery_overvoltage_level)
            {
                battery_overvoltage_warning_flag = 1;
                battery_overvoltage_error_cpt++;
                if (battery_overvoltage_error_cpt > battery_overvoltage_timeout) //Erreur si temp batt > 
                {
                    battery_overvoltage_error_flag = 1;
                    battery_overvoltage_error_cpt = 0;
                }
            }
            else
            {
                battery_overvoltage_warning_flag = 0;
                battery_overvoltage_error_cpt = 0;
            }
          
            // </editor-fold>
            
            // <editor-fold defaultstate="collapsed" desc="FCS LOW CURRENT SHUTDOWN">
            if (fcs_output_current < 1000)
            {
                fcs_low_current_timeout_cpt++;
                if (fcs_low_current_timeout_cpt > fcs_low_current_timeout)
                {
                    fcs_low_current_timeout_flag = 1;
                    fcs_low_current_timeout_cpt = 0;
                }
            }
            else fcs_low_current_timeout_cpt = 0;
            // </editor-fold>
            
            // <editor-fold defaultstate="collapsed" desc="BATTERY LOW SOP SHUTDOWN">
            if(step >= 30)
            {
                if ((ibs_battery_0_sop < ibs_battery_sop_warning_low_level) || (ibs_battery_1_sop < ibs_battery_sop_warning_low_level))
                {
                    ibs_battery_sop_warning_flag = 1;
                    if ((ibs_battery_0_sop < ibs_battery_sop_warning_very_low_level) || (ibs_battery_1_sop < ibs_battery_sop_warning_very_low_level)) //Erreur si temp batt > 
                    {
                        ibs_battery_sop_error_flag = 1;
                    }
                }
                else
                {
                    ibs_battery_sop_warning_flag = 0;
                    ibs_battery_sop_error_flag = 0;
                    //ibs_battery_sop_error_cpt = 0;
                }
            }
            
            // </editor-fold>
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="BATTERY MODE BUTTON">
            if (battery_mode_button_flag)
            {
                if (BATTERY_MODE_BUTTON_GetValue() == 0)
                {
                    battery_mode_button_cnt++;       // Si appui sur le BP START
                    if (battery_mode_button_cnt > 4) // Si bouton appuyï¿½ pendant 5ms en continu
                    {
                        battery_mode_button_flag = 0; // Set flag : BP appuyï¿½
                        battery_mode_button_cnt = 0;  // RAZ
                    }
                }
                else
                    battery_mode_button_cnt = 0; // Filtrage
            }
            else
            {
                if (BATTERY_MODE_BUTTON_GetValue() == 1)
                {
                    battery_mode_button_cnt++;       // Si appui sur le BP START
                    if (battery_mode_button_cnt > 4) // Si bouton appuyï¿½ pendant 5ms en continu
                    {
                        battery_mode_button_flag = 1; // Set flag : BP appuyï¿½
                        battery_mode_button_cnt = 0;  // RAZ
                    }
                }
                else
                    battery_mode_button_cnt = 0; // Filtrage
            }
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="HYBRID MODE BUTTON">
            if (hybrid_mode_button_flag)
            {
                if (HYBRID_MODE_BUTTON_GetValue() == 0)
                {
                    hybrid_mode_button_cnt++;       // Si appui sur le BP START
                    if (hybrid_mode_button_cnt > 4) // Si bouton appuyï¿½ pendant 5ms en continu
                    {
                        hybrid_mode_button_flag = 0; // Set flag : BP appuyï¿½
                        hybrid_mode_button_cnt = 0;  // RAZ
                    }
                }
                else
                    hybrid_mode_button_cnt = 0; // Filtrage
            }
            else
            {
                if (HYBRID_MODE_BUTTON_GetValue() == 1)
                {
                    hybrid_mode_button_cnt++;       // Si appui sur le BP START
                    if (hybrid_mode_button_cnt > 4) // Si bouton appuyï¿½ pendant 5ms en continu
                    {
                        if (fcs_general_error_flag)
                        {
                            FCS_ResetErrors();

                            fcs_startup_error_flag = 0;
                            fcs_critical_error_flag = 0;
                            fcs_minor_error_flag = 0;
                            fcs_watchdog_error_flag = 0;
                            fcs_general_error_flag = 0;
                        }

                        hybrid_mode_button_flag = 1; // Set flag : BP appuyï¿½
                        hybrid_mode_button_cnt = 0;  // RAZ
                    }
                }
                else
                    hybrid_mode_button_cnt = 0; // Filtrage
            }
            // </editor-fold>
           
            // <editor-fold defaultstate="collapsed" desc="IHM MIDDLE BUTTON">
           
            if (IHM_MIDDLE_BUTTON_GetValue() == 1)
                {
                    imh_middle_button_cnt++;       // Si appui sur le BP START
                    if (imh_middle_button_cnt > 50) // Si bouton appuyï¿½ pendant 5ms en continu
                    {
                        imh_middle_button_flag = 1; // Set flag : BP appuyï¿½
                        imh_middle_button_cnt = 0;  // RAZ
                    }
                }
            else
                imh_middle_button_cnt = 0; // Filtrage
            
            // </editor-fold>
            
            // <editor-fold defaultstate="collapsed" desc="FCS_START_2 TIMEOUT">
            if(step == SMB_FCS_START_2)
            {
                fcs_start_2_timeout_cnt++;
                if (fcs_start_2_timeout_cnt > 600)
                {
                    fcs_start_2_timeout_flag = 1;
                    fcs_start_2_timeout_cnt = 0;
                }
            }
            else
                fcs_start_2_timeout_cnt = 0;
            // </editor-fold>
                        
            // <editor-fold defaultstate="collapsed" desc="CURRENT CALCULATION">
            battery_soc = (ibs_battery_0.soc + ibs_battery_1.soc) / 2;

            battery_output_current = (int16_t)((ibs_battery_0.output_current + ibs_battery_1.output_current));

            battery_output_current_sum += battery_output_current;
            fcs_output_current_sum += fcs_output_current;

            current_ind++;
            if (current_ind > 9)
            {
                battery_output_current_moy = (int16_t)(battery_output_current_sum / current_ind);
                fcs_output_current_moy = (int16_t)(fcs_output_current_sum / current_ind);
                system_output_current = (int16_t)(battery_output_current_moy + fcs_output_current_moy);

                battery_output_current_sum = 0;
                fcs_output_current_sum = 0;
                current_ind = 0;
            }
            // XXX Pas calculer le courant pendant court circuit
            //  </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="GESTION BUZZER">
            if ((step >= 100) && (step < 200))
            {
            if (battery_soc_low_level_warning_flag && battery_output_current > 0)
            {
                buzzer_period_cpt++;
                if (buzzer_period_cpt == 10)
                    BUZZER_SetHigh();
                else if (buzzer_period_cpt == 12)
                    BUZZER_SetLow();
                else if (buzzer_period_cpt == 14)
                    BUZZER_SetHigh();
                else if (buzzer_period_cpt == 16)
                    BUZZER_SetLow();
                else if ((battery_soc >= battery_soc_very_low_level) && (buzzer_period_cpt >= (((battery_soc - battery_soc_very_low_level) * 10) + 20)))
                    buzzer_period_cpt = 0;
            }
            else if (battery_soc_high_level_warning_flag)
            {
                buzzer_period_cpt++;
                if (buzzer_period_cpt == 10)
                    BUZZER_SetHigh();
                else if (buzzer_period_cpt == 12)
                    BUZZER_SetLow();
                else if (buzzer_period_cpt == 14)
                    BUZZER_SetHigh();
                else if (buzzer_period_cpt == 16)
                    BUZZER_SetLow();
                else if ((battery_soc <= battery_soc_very_very_high_level) && (buzzer_period_cpt >= (((battery_soc_very_very_high_level - battery_soc) * 30) + 20)))
                    buzzer_period_cpt = 0;
            }
            else if (ac_output_apparent_power_high_level_warning_flag || ibs_battery_sop_warning_flag )
            {
                buzzer_period_cpt++;
                if (buzzer_period_cpt == 10)
                    BUZZER_SetHigh();
                else if (buzzer_period_cpt == 12)
                    BUZZER_SetLow();
                else if (buzzer_period_cpt == 14)
                    BUZZER_SetHigh();
                else if (buzzer_period_cpt == 16)
                    BUZZER_SetLow();
//                else if ((ac_output_apparent_power <= ac_output_apparent_power_very_high_level) && (buzzer_period_cpt >= ((ac_output_apparent_power_very_high_level - ac_output_apparent_power) + 20)))
                else if (buzzer_period_cpt > 18)  
                    buzzer_period_cpt = 0;
            }
            else
                BUZZER_SetLow();
            }
            // </editor-fold>
            
            // <editor-fold defaultstate="collapsed" desc="DELAIS 5 MINUTES BUZZER">
//            if (step == 250)
//            {
//                buzzer_5_minutes_cnt++;
//                if (buzzer_5_minutes_cnt > buzzer_5_minutes_timeout) buzzer_5_minutes_flag = 1;
//                else 
//                {
//                    buzzer_5_minutes_flag = 0;
//                    buzzer_5_minutes_cnt = 0;
//                }
//            }
//            else 
//            {
//                buzzer_5_minutes_cnt = 0;
//                buzzer_5_minutes_flag = 0;
//            }
            
            // </editor-fold>
            
            tab_ac_output_apparent_power[ac_output_apparent_power_i] = ac_output_apparent_power;
            ac_output_apparent_power_i++;
            if(ac_output_apparent_power_i == 10) 
            {
                ac_output_apparent_power_i = 0;
                moy_ac_output_apparent_power = (tab_ac_output_apparent_power[0] + tab_ac_output_apparent_power[1] + tab_ac_output_apparent_power[2]
                        + tab_ac_output_apparent_power[3] + tab_ac_output_apparent_power[4] + tab_ac_output_apparent_power[5] + tab_ac_output_apparent_power[6]
                        + tab_ac_output_apparent_power[7] + tab_ac_output_apparent_power[8] + tab_ac_output_apparent_power[9]) / 10;
            }
            if (moy_ac_output_apparent_power < 5000) ac_output_apparent_power_high_level_warning_flag = 0;
            else ac_output_apparent_power_high_level_warning_flag = 1;

            _100ms_timer_flag = 0;
        }
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="TOUTES LES 300MS">
        else if(_300ms_timer_flag)
        {
            // <editor-fold defaultstate="collapsed" desc="REQUETE RS232">
            if ((step > 30) && (step < 200))
            {
                rs232_frame_id_ind++;
                if (rs232_frame_id_ind == 1)
                {
                    printf("QMOD%c%c\r", 0x49, 0xC1); // QMOD
                    rs232_frame_id = RS232FRAME_QMOD;
                }
                else if (rs232_frame_id_ind == 6) // QPIWS
                {
                    printf("QPIWS%c%c\r", 0xB4, 0xDA);
                    rs232_frame_id = RS232FRAME_QPIWS;
                }
                else if (rs232_frame_id_ind == 13) // QPIGS
                {
                    printf("QPIGS%c%c\r", 0xB7, 0xA9);
                    rs232_frame_id = RS232FRAME_QPIGS;
                }
                else if (rs232_frame_id_ind > 24)
                {
                    rs232_frame_id_ind = 0;
                    rs232_frame_id = RS232FRAME_NONE;
                }
            }
            else
            {
                moy_ac_output_apparent_power = 0;
                ac_output_active_power = 0;
                ac_output_voltage = 0;

                rs232_frame_id_ind = 0;
                rs232_frame_id = RS232FRAME_NONE;
            }
            // </editor-fold>

            _300ms_timer_flag = 0;

        }
        // </editor-fold>
        
        // <editor-fold defaultstate="collapsed" desc="TOUTES LES 1s">
        else if (_1s_timer_flag)
        {
            // <editor-fold defaultstate="collapsed" desc="IBS KEEP ALIVE">
            ibs_keep_alive_index = !ibs_keep_alive_index;

            keepalivemsg.msgId = 0x180AD000 | ((uint16_t)ibs_keep_alive_index << 8);
            CAN1_Transmit(CAN_PRIORITY_HIGH, &keepalivemsg);
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="CONTROLE VENTILATEURS partie AC">
            // Calcul des besoins pwm
            ambiant_cooling_pwm = PID_controller(1.0, 0.1, 0.0, &ambiant_int_part, 40.0, (float) ac_ambiant_temp.value_8);  // Ambiant temperature PID controller
            inverter_cooling_pwm = PID_controller(1.0, 0.1, 0.0, &inverter_int_part, 55.0, (float) dcac_temp);              // Inverter temperature PID controller
            dcdc_cooling_pwm = PID_controller(1.0, 0.1, 0.0, &dcdc_int_part, 45.0, (float) dcdc_temperature);               // DCDC temperature PID controller - Valeur de tempÃ©rature Ã  remonter

            // Pilotage des ventilateurs en utilisant la valeur la plus importante
            ac_fans_duty_cycle = (uint8_t) saturation(15, max(ambiant_cooling_pwm, max(inverter_cooling_pwm, dcdc_cooling_pwm)), 100);
            PWM2_SetDutyCycle(ac_fans_duty_cycle);
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="GESTION ENERGIE">
            // Trï¿½s simple
//            if ((step == 150) && !fcs_short_circuit_ongoing_flag)
//            {
//                min_ems_current = max(800.0, system_output_current + 210 * max(ibs_battery_0.max_discharge_current, ibs_battery_1.max_discharge_current));
//                max_ems_current = min(5500, 210 * min(ibs_battery_0.max_charge_current, ibs_battery_1.max_charge_current) + system_output_current);
//                
//                if ((battery_soc < battery_soc_high_level) && (system_output_current >= 5))
//                    requested_fcs_current = (uint16_t) dynamicsLimiter((float) requested_fcs_current, (float) (system_output_current + 2000.0), 100.0);
//                else
//                    requested_fcs_current = (uint16_t) dynamicsLimiter((float) requested_fcs_current, 2000.0, 100.0);
//                
//                requested_fcs_current_sat = (uint16_t) saturation(min_ems_current, (float) requested_fcs_current, max_ems_current);
//                FCS_SetCurrentLimit(requested_fcs_current_sat);
//            }
//            else
//                requested_fcs_current = 0;            
            
            ibs_battery_global_max_charge_current = max(ibs_battery_0.max_charge_current, ibs_battery_1.max_charge_current);
            ibs_battery_global_max_charge_current = (ibs_battery_global_max_charge_current * 0.7 * 2) * (-1);
            if ((step == 150) && !fcs_short_circuit_ongoing_flag)
            {
                if ((system_output_current >= 0) && (ibs_battery_global_max_charge_current > 2000))
                    requested_fcs_current = system_output_current + 2000;
                else if ((system_output_current >= 0) && (ibs_battery_global_max_charge_current <= 2000))
                    requested_fcs_current = system_output_current + ibs_battery_global_max_charge_current;
                else
                    requested_fcs_current = 2000;

                if (requested_fcs_current < 800)
                    requested_fcs_current = 800;
                else if (requested_fcs_current > 5500)
                    requested_fcs_current = 5500;
                FCS_SetCurrentLimit(requested_fcs_current);
            }
            else
                requested_fcs_current = 0;
            
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="WATCHDOGS">
            // <editor-fold defaultstate="collapsed" desc="WATCHDOG BAT0">
            ibs_battery_0.watchdog_cnt++;
            if (ibs_battery_0.watchdog_cnt > watchdog_timeout) // Si dï¿½passement de la valeur
            {
                ibs_battery_0.watchdog_error_flag = 1; // Set error flag
                ibs_battery_0.watchdog_cnt = 0;        // RAZ
            }
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="WATCHDOG BAT1">
            ibs_battery_1.watchdog_cnt++;
            if (ibs_battery_1.watchdog_cnt > watchdog_timeout) // Si dï¿½passement de la valeur
            {
                ibs_battery_1.watchdog_error_flag = 1; // Set error flag
                ibs_battery_1.watchdog_cnt = 0;        // RAZ
            }
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="WATCHDOG FCS">
            fcs_watchdog_cnt++;
            if (fcs_watchdog_cnt > watchdog_timeout) // Si dï¿½passement de la valeur
            {
                fcs_watchdog_error_flag = 1; // Set error flag
                fcs_watchdog_cnt = 0;        // RAZ
            }
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="WATCHDOG DCAC">
            if ((step >= 100) && (step < 200))
            {
                dcac_watchdog_cnt++;
                dcac_recieve_cnt++;
                if (dcac_watchdog_cnt > dcac_watchdog_timeout) // Si dï¿½passement de la valeur
                {
                    dcac_watchdog_error_flag = 1; // Set error flag
                    dcac_watchdog_cnt = 0;        // RAZ
                }
                if (dcac_recieve_cnt > dcac_recieve_timeout) // Si dï¿½passement de la valeur
                {
                    dcac_recieve_flag = 1; // Set error flag
                    dcac_recieve_cnt = 0;        // RAZ
                }

            }
            // </editor-fold>
            // </editor-fold>
            
            // <editor-fold defaultstate="collapsed" desc="DELAIS 5 MINUTES BUZZER">
            if (step == 250)
            {
                buzzer_5_minutes_cnt++;
                if (buzzer_5_minutes_cnt > buzzer_5_minutes_timeout) 
                {
                    buzzer_5_minutes_flag = 1;
                    buzzer_5_minutes_cnt = 0;
                }
            }
            else 
            {
                buzzer_5_minutes_cnt = 0;
                //buzzer_5_minutes_flag = 0;
            }
            
            // </editor-fold>
            
            _1s_timer_flag = 0;
        }
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="TRAITEMENT TRAME RS232">
        else if (rs232_frame_recieved_flag)
        {
            // <editor-fold defaultstate="collapsed" desc="QMOD">
            if (rs232_frame_id == RS232FRAME_QMOD)
            {
                if (rs232_rx_frame[0] == 'B')
                    inverter_mode = INVERTER_BATTERY_MODE;
                else if (rs232_rx_frame[0] == 'F')
                    inverter_mode = INVERTER_FAULT_MODE;
                else if (rs232_rx_frame[0] == 'H')
                    inverter_mode = INVERTER_POWER_SAVING_MODE;
                else if (rs232_rx_frame[0] == 'L')
                    inverter_mode = INVERTER_AC_SOURCE_MODE;
                else if (rs232_rx_frame[0] == 'S')
                    inverter_mode = INVERTER_STANDBY_MODE;
                else if (rs232_rx_frame[0] == 'P')
                    inverter_mode = INVERTER_POWER_ON_MODE;

                toggle_qmod_flag = !toggle_qmod_flag;
                dcac_watchdog_cnt = 0;
                dcac_recieve_cnt = 0;
                if (dcac_recieve_flag == 1){
                    dcac_recieve_flag = 0;
                } 
            }
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="QPIWS">
            else if (rs232_frame_id == RS232FRAME_QPIWS)
            {
                inverter_error_flag = (rs232_rx_frame[1] & 0x01);
                bus_overvoltage_error_flag = (rs232_rx_frame[2] & 0x01);
                bus_undervoltage_error_flag = (rs232_rx_frame[3] & 0x01);
                bus_soft_fail_error_flag = (rs232_rx_frame[4] & 0x01);
                line_fail_warning_flag = (rs232_rx_frame[5] & 0x01);
                opv_short_warning_flag = (rs232_rx_frame[6] & 0x01);
                inverter_voltage_too_low_error_flag = (rs232_rx_frame[7] & 0x01);
                inverter_voltage_too_high_error_flag = (rs232_rx_frame[8] & 0x01);
                overtemperature_warning_flag = (rs232_rx_frame[9] & 0x01);
                if (inverter_error_flag == 1)
                    overtemperature_error_flag = overtemperature_warning_flag;
                fan_locked_warning_flag = (rs232_rx_frame[10] & 0x01);
                if (inverter_error_flag == 1)
                    fan_locked_error_flag = fan_locked_warning_flag;
                battery_voltage_high_warning_flag = (rs232_rx_frame[11] & 0x01);
                if (inverter_error_flag == 1)
                    battery_voltage_high_error_flag = battery_voltage_high_warning_flag;
                battery_voltage_low_warning_flag = (rs232_rx_frame[12] & 0x01);
                overcharge_error_flag = (rs232_rx_frame[13] & 0x01);
                battery_under_shutdown_voltage_warning_flag = (rs232_rx_frame[14] & 0x01);
                battery_derating_warning_flag = (rs232_rx_frame[15] & 0x01);
                overload_warning_flag = (rs232_rx_frame[16] & 0x01);
                if (inverter_error_flag == 1 || ac_output_apparent_power_very_high_level_error_flag == 1 )
                    overload_error_flag = overload_warning_flag;
                eeprom_warning_flag = (rs232_rx_frame[17] & 0x01);
                inverter_over_current_error_flag = (rs232_rx_frame[18] & 0x01);
                inverter_soft_start_error_flag = (rs232_rx_frame[19] & 0x01);
                self_test_error_flag = (rs232_rx_frame[20] & 0x01);
                ac_output_dc_voltage_too_high_error_flag = (rs232_rx_frame[21] & 0x01);
                battery_disconnected_error_flag = (rs232_rx_frame[22] & 0x01);
                current_sensor_error_flag = (rs232_rx_frame[23] & 0x01);
                battery_short_circuit_error_flag = (rs232_rx_frame[24] & 0x01);
                power_limit_warning_flag = (rs232_rx_frame[25] & 0x01);
                pv_voltage_high_warning_flag = (rs232_rx_frame[26] & 0x01);
                mppt_overload_error_flag = (rs232_rx_frame[27] & 0x01);
                mppt_overload_warning_flag = (rs232_rx_frame[28] & 0x01);
                battery_too_low_to_charge_warning_flag = (rs232_rx_frame[29] & 0x01);

                //                if(rs232_rx_frame[1] == 0x30) inverter_error_flag = 0;
                //                else inverter_error_flag = 1;
                //                if(rs232_rx_frame[2] == 0x30) bus_overvoltage_error_flag = 0;
                //                else bus_overvoltage_error_flag = 1;
                //                if(rs232_rx_frame[3] == 0x30) bus_undervoltage_error_flag = 0;
                //                else bus_undervoltage_error_flag = 1;
                //                if(rs232_rx_frame[4] == 0x30) bus_soft_fail_error_flag = 0;
                //                else bus_soft_fail_error_flag = 1;
                //                if(rs232_rx_frame[5] == 0x30) line_fail_warning_flag = 0;
                //                else line_fail_warning_flag = 1;
                //                if(rs232_rx_frame[6] == 0x30) opv_short_warning_flag = 0;
                //                else opv_short_warning_flag = 1;
                //                if(rs232_rx_frame[7] == 0x30) inverter_voltage_too_low_error_flag = 0;
                //                else inverter_voltage_too_low_error_flag = 1;
                //                if(rs232_rx_frame[8] == 0x30) inverter_voltage_too_high_error_flag = 0;
                //                else inverter_voltage_too_high_error_flag = 1;
                //                if(rs232_rx_frame[9] == 0x30) overtemperature_warning_flag = 0;
                //                else overtemperature_warning_flag = 1;
                //                if(inverter_error_flag == 1) overtemperature_error_flag = overtemperature_warning_flag;
                //                if(rs232_rx_frame[10] == 0x30) fan_locked_warning_flag = 0;
                //                else fan_locked_warning_flag = 1;
                //                if(inverter_error_flag == 1) fan_locked_error_flag = fan_locked_warning_flag;
                //                if(rs232_rx_frame[11] == 0x30) battery_voltage_high_warning_flag = 0;
                //                else battery_voltage_high_warning_flag = 1;
                //                if(inverter_error_flag == 1) battery_voltage_high_error_flag = battery_voltage_high_warning_flag;
                //                if(rs232_rx_frame[12] == 0x30) battery_voltage_low_warning_flag = 0;
                //                else battery_voltage_low_warning_flag = 1;
                //                if(rs232_rx_frame[13] == 0x30) overcharge_error_flag = 0;
                //                else overcharge_error_flag = 1;
                //                if(rs232_rx_frame[14] == 0x30) battery_under_shutdown_voltage_warning_flag = 0;
                //                else battery_under_shutdown_voltage_warning_flag = 1;
                //                if(rs232_rx_frame[15] == 0x30) battery_derating_warning_flag = 0;
                //                else battery_derating_warning_flag = 1;
                //                if(rs232_rx_frame[16] == 0x30) overload_warning_flag = 0;
                //                else overload_warning_flag = 1;
                //                if(inverter_error_flag == 1) overload_error_flag = overload_warning_flag;
                //                if(rs232_rx_frame[17] == 0x30) eeprom_warning_flag = 0;
                //                else eeprom_warning_flag = 1;
                //                if(rs232_rx_frame[18] == 0x30) inverter_over_current_error_flag = 0;
                //                else inverter_over_current_error_flag = 1;
                //                if(rs232_rx_frame[19] == 0x30) inverter_soft_start_error_flag = 0;
                //                else inverter_soft_start_error_flag = 1;
                //                if(rs232_rx_frame[20] == 0x30) self_test_error_flag = 0;
                //                else self_test_error_flag = 1;
                //                if(rs232_rx_frame[21] == 0x30) ac_output_dc_voltage_too_high_error_flag = 0;
                //                else ac_output_dc_voltage_too_high_error_flag = 1;
                //                if(rs232_rx_frame[22] == 0x30) battery_disconnected_error_flag = 0;
                //                else battery_disconnected_error_flag = 1;
                //                if(rs232_rx_frame[23] == 0x30) current_sensor_error_flag = 0;
                //                else current_sensor_error_flag = 1;
                //                if(rs232_rx_frame[24] == 0x30) battery_short_circuit_error_flag = 0;
                //                else battery_short_circuit_error_flag = 1;
                //                if(rs232_rx_frame[25] == 0x30) power_limit_warning_flag = 0;
                //                else power_limit_warning_flag = 1;
                //                if(rs232_rx_frame[26] == 0x30) pv_voltage_high_warning_flag = 0;
                //                else pv_voltage_high_warning_flag = 1;
                //                if(rs232_rx_frame[27] == 0x30) mppt_overload_error_flag = 0;
                //                else mppt_overload_error_flag = 1;
                //                if(rs232_rx_frame[28] == 0x30) mppt_overload_warning_flag = 0;
                //                else mppt_overload_warning_flag = 1;
                //                if(rs232_rx_frame[29] == 0x30) battery_too_low_to_charge_warning_flag = 0;
                //                else battery_too_low_to_charge_warning_flag = 1;

                toggle_qpiws_flag = !toggle_qpiws_flag;
                dcac_watchdog_cnt = 0;
            }
            // </editor-fold>

            // <editor-fold defaultstate="collapsed" desc="QPIGS">
            else if (rs232_frame_id == RS232FRAME_QPIGS)
            {
                //                grid_voltage = (charToDec(rs232_rx_frame[0]) * 1000);
                //                grid_voltage += (charToDec(rs232_rx_frame[1]) * 100);
                //                grid_voltage += (charToDec(rs232_rx_frame[2]) * 10);
                //                grid_voltage += charToDec(rs232_rx_frame[4]);

                //                grid_frequency = (charToDec(rs232_rx_frame[6]) * 100);
                //                grid_frequency += (charToDec(rs232_rx_frame[7]) * 10);
                //                grid_frequency += charToDec(rs232_rx_frame[9]);

                ac_output_voltage = (charToDec(rs232_rx_frame[11]) * 1000);
                ac_output_voltage += (charToDec(rs232_rx_frame[12]) * 100);
                ac_output_voltage += (charToDec(rs232_rx_frame[13]) * 10);
                ac_output_voltage += charToDec(rs232_rx_frame[15]);

                //                ac_output_frequency = (charToDec(rs232_rx_frame[17]) * 100);
                //                ac_output_frequency += (charToDec(rs232_rx_frame[18]) * 10);
                //                ac_output_frequency += charToDec(rs232_rx_frame[20]);

                ac_output_apparent_power = (charToDec(rs232_rx_frame[22]) * 1000);
                ac_output_apparent_power += (charToDec(rs232_rx_frame[23]) * 100);
                ac_output_apparent_power += (charToDec(rs232_rx_frame[24]) * 10);
                ac_output_apparent_power += charToDec(rs232_rx_frame[25]);
                
                
//                tab_ac_output_apparent_power[ac_output_apparent_power_i] = ac_output_apparent_power;
//                ac_output_apparent_power_i++;
//                if(ac_output_apparent_power_i == 10) 
//                {
//                    ac_output_apparent_power_i = 0;
//                    moy_ac_output_apparent_power = (tab_ac_output_apparent_power[0] + tab_ac_output_apparent_power[1] + tab_ac_output_apparent_power[2]
//                            + tab_ac_output_apparent_power[3] + tab_ac_output_apparent_power[4] + tab_ac_output_apparent_power[5] + tab_ac_output_apparent_power[6]
//                            + tab_ac_output_apparent_power[7] + tab_ac_output_apparent_power[8] + tab_ac_output_apparent_power[9]) / 10;
//                }

                ac_output_active_power = (charToDec(rs232_rx_frame[27]) * 1000);
                ac_output_active_power += (charToDec(rs232_rx_frame[28]) * 100);
                ac_output_active_power += (charToDec(rs232_rx_frame[29]) * 10);
                ac_output_active_power += charToDec(rs232_rx_frame[30]);

                //                output_load_percent = (charToDec(rs232_rx_frame[32]) * 100);
                //                output_load_percent += (charToDec(rs232_rx_frame[33]) * 10);
                //                output_load_percent += charToDec(rs232_rx_frame[34]);

                //                internal_bus_voltage = (charToDec(rs232_rx_frame[36]) * 100.0);
                //                internal_bus_voltage += (charToDec(rs232_rx_frame[37]) * 10.0);
                //                internal_bus_voltage += charToDec(rs232_rx_frame[38]);

                //                inverter_battery_voltage = (charToDec(rs232_rx_frame[40]) * 1000.0);
                //                inverter_battery_voltage += (charToDec(rs232_rx_frame[41]) * 100.0);
                //                inverter_battery_voltage += (charToDec(rs232_rx_frame[43]) * 10.0);
                //                inverter_battery_voltage += charToDec(rs232_rx_frame[44]);

                //                battery_charging_current = (charToDec(rs232_rx_frame[46]) * 100);
                //                battery_charging_current += (charToDec(rs232_rx_frame[47]) * 10);
                //                battery_charging_current += charToDec(rs232_rx_frame[48]);

                dcac_temp = (charToDec(rs232_rx_frame[54]) * 1000);
                dcac_temp += (charToDec(rs232_rx_frame[55]) * 100);
                dcac_temp += (charToDec(rs232_rx_frame[56]) * 10);
                dcac_temp += charToDec(rs232_rx_frame[57]);

                //                battery_discharge_current = (charToDec(rs232_rx_frame[76]) * 10000);
                //                battery_discharge_current += (charToDec(rs232_rx_frame[77]) * 1000);
                //                battery_discharge_current += (charToDec(rs232_rx_frame[78]) * 100);
                //                battery_discharge_current += (charToDec(rs232_rx_frame[79]) * 10);
                //                battery_discharge_current += charToDec(rs232_rx_frame[80]);

                //                if(rs232_rx_frame[85] == 0x31) load_status_flag = 1;
                //                else load_status_flag = 0;

                //                if(rs232_rx_frame[87] == ascii_1) charging_status_flag = 1;
                //                else charging_status_flag = 0;
                //
                //                if(rs232_rx_frame[88] == ascii_1) scc_charging_status_flag = 1;
                //                else scc_charging_status_flag = 0;
                //
                //                if(rs232_rx_frame[89] == ascii_1) ac_charging_status_flag = 1;
                //                else ac_charging_status_flag = 0;
                //
                //                if(rs232_rx_frame[104] == ascii_1) charging_to_floating_mode_flag = 1;
                //                else charging_to_floating_mode_flag = 0;
                //
                //                if(rs232_rx_frame[105] == ascii_1) device_switched_on_flag = 1;
                //                else device_switched_on_flag = 0;

                load_status_flag = (rs232_rx_frame[85] & 0x01);
                charging_status_flag = (rs232_rx_frame[87] & 0x01);
                scc_charging_status_flag = (rs232_rx_frame[88] & 0x01);
                ac_charging_status_flag = (rs232_rx_frame[89] & 0x01);
                charging_to_floating_mode_flag = (rs232_rx_frame[104] & 0x01);
                device_switched_on_flag = (rs232_rx_frame[105] & 0x01);

                toggle_qpigs_flag = !toggle_qpigs_flag;
                dcac_watchdog_cnt = 0;
            }
            // </editor-fold>

            rs232_frame_id = RS232FRAME_NONE;
            rs232_frame_recieved_flag = 0;
        }
        // </editor-fold>

        // <editor-fold defaultstate="collapsed" desc="ENVOI CAN">
        else if ((can_ready_flag) && (step > 10))
        {
            for (i = 0; i < 8; i++)
                output_msg.data[i] = 0; // RAZ output_buffer
            can_flagsbyte.byte = 0;

            output_msg.field.idType = CAN_FRAME_EXT;

            switch (can_frame_ind)
            {
            case 1:
                output_msg.data[0] = battery_soc;
                can_flagsbyte.byte = 0;
                can_flagsbyte.sFLAGSBITS.b0 = ibs_battery_0.system_running_flag;
                can_flagsbyte.sFLAGSBITS.b1 = ibs_battery_0.error_flag;
                can_flagsbyte.sFLAGSBITS.b2 = ibs_battery_0.low_soc_warning_flag;
                can_flagsbyte.sFLAGSBITS.b3 = ibs_battery_0.balancing_flag;
                can_flagsbyte.sFLAGSBITS.b4 = ibs_battery_1.system_running_flag;
                can_flagsbyte.sFLAGSBITS.b5 = ibs_battery_1.error_flag;
                can_flagsbyte.sFLAGSBITS.b6 = ibs_battery_1.low_soc_warning_flag;
                can_flagsbyte.sFLAGSBITS.b7 = ibs_battery_1.balancing_flag;
                output_msg.data[1] = can_flagsbyte.byte;
                output_msg.data[2] = (uint8_t) battery_output_current;
                output_msg.data[3] = (uint8_t)(battery_output_current >> 8);
                output_msg.data[4] = (uint8_t)(ibs_battery_0.avg_cell_temperature);
                output_msg.data[5] = (uint8_t)(ibs_battery_1.avg_cell_temperature);
                output_msg.data[6] = (uint8_t)(ibs_battery_0_sop);
                output_msg.data[7] = (uint8_t)(ibs_battery_1_sop);
                output_msg.field.dlc = CAN_DLC_8;
                output_msg.msgId = 0x2BA0000;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;
            case 3:
                if (((step > 30) && (step <= 250)) || (inverter_raz_data_after_stop_flag))
                {
                    output_msg.data[0] = inverter_mode;
                    can_flagsbyte.byte = 0;
                    can_flagsbyte.sFLAGSBITS.b0 = inverter_error_flag;
                    can_flagsbyte.sFLAGSBITS.b1 = device_switched_on_flag;
                    can_flagsbyte.sFLAGSBITS.b2 = load_status_flag;
                    can_flagsbyte.sFLAGSBITS.b3 = charging_status_flag;
                    can_flagsbyte.sFLAGSBITS.b4 = ac_charging_status_flag;
                    can_flagsbyte.sFLAGSBITS.b5 = scc_charging_status_flag;
                    output_msg.data[1] = can_flagsbyte.byte;
                    can_flagsbyte.byte = 0;
                    can_flagsbyte.sFLAGSBITS.b0 = bus_overvoltage_error_flag;
                    can_flagsbyte.sFLAGSBITS.b1 = bus_undervoltage_error_flag;
                    can_flagsbyte.sFLAGSBITS.b2 = bus_soft_fail_error_flag;
                    can_flagsbyte.sFLAGSBITS.b3 = inverter_voltage_too_low_error_flag;
                    can_flagsbyte.sFLAGSBITS.b4 = inverter_voltage_too_high_error_flag;
                    can_flagsbyte.sFLAGSBITS.b5 = overtemperature_error_flag;
                    can_flagsbyte.sFLAGSBITS.b6 = fan_locked_error_flag;
                    can_flagsbyte.sFLAGSBITS.b7 = battery_voltage_high_error_flag;
                    output_msg.data[2] = can_flagsbyte.byte;
                    can_flagsbyte.byte = 0;
                    can_flagsbyte.sFLAGSBITS.b0 = overcharge_error_flag;
                    can_flagsbyte.sFLAGSBITS.b1 = overload_error_flag;
                    can_flagsbyte.sFLAGSBITS.b2 = inverter_over_current_error_flag;
                    can_flagsbyte.sFLAGSBITS.b3 = inverter_soft_start_error_flag;
                    can_flagsbyte.sFLAGSBITS.b4 = self_test_error_flag;
                    can_flagsbyte.sFLAGSBITS.b5 = ac_output_dc_voltage_too_high_error_flag;
                    can_flagsbyte.sFLAGSBITS.b6 = battery_disconnected_error_flag;
                    can_flagsbyte.sFLAGSBITS.b7 = current_sensor_error_flag;
                    output_msg.data[3] = can_flagsbyte.byte;
                    can_flagsbyte.byte = 0;
                    can_flagsbyte.sFLAGSBITS.b0 = battery_short_circuit_error_flag;
                    can_flagsbyte.sFLAGSBITS.b1 = mppt_overload_error_flag;
                    can_flagsbyte.sFLAGSBITS.b2 = ac_output_apparent_power_very_high_level_error_flag;
                    output_msg.data[4] = can_flagsbyte.byte;
                    can_flagsbyte.byte = 0;
                    can_flagsbyte.sFLAGSBITS.b0 = line_fail_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b1 = opv_short_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b2 = bus_soft_fail_error_flag;
                    can_flagsbyte.sFLAGSBITS.b3 = overtemperature_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b4 = fan_locked_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b5 = battery_voltage_high_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b6 = battery_voltage_low_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b7 = battery_under_shutdown_voltage_warning_flag;
                    output_msg.data[5] = can_flagsbyte.byte;
                    can_flagsbyte.byte = 0;
                    can_flagsbyte.sFLAGSBITS.b0 = battery_derating_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b1 = overload_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b2 = eeprom_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b3 = power_limit_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b4 = pv_voltage_high_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b5 = mppt_overload_warning_flag;
                    can_flagsbyte.sFLAGSBITS.b6 = battery_too_low_to_charge_warning_flag;
                    output_msg.data[6] = can_flagsbyte.byte;
                    output_msg.field.dlc = CAN_DLC_7;
                    output_msg.msgId = 0x2AC0000;
                    CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                }
                break;
            case 4:
                if (((step > 30) && (step <= 250)) || (inverter_raz_data_after_stop_flag))
                {
                    output_msg.data[0] = (uint8_t)moy_ac_output_apparent_power;
                    output_msg.data[1] = (uint8_t)(moy_ac_output_apparent_power >> 8);
                    output_msg.data[2] = (uint8_t)ac_output_active_power;
                    output_msg.data[3] = (uint8_t)(ac_output_active_power >> 8);
                    output_msg.data[4] = (uint8_t)ac_output_voltage;
                    output_msg.data[5] = (uint8_t)(ac_output_voltage >> 8);
                    output_msg.data[6] = (uint8_t)dcac_temp;
                    can_flagsbyte.byte = 0;
                    can_flagsbyte.sFLAGSBITS.b0 = toggle_qmod_flag;
                    can_flagsbyte.sFLAGSBITS.b1 = toggle_qpiws_flag;
                    can_flagsbyte.sFLAGSBITS.b2 = toggle_qpigs_flag;
                    output_msg.data[7] = can_flagsbyte.byte;
                    output_msg.field.dlc = CAN_DLC_8;
                    output_msg.msgId = 0x2AC0001;
                    CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                    inverter_raz_data_after_stop_flag = 0;
                }
                break;
            case 6:
                output_msg.data[0] = step;
                can_flagsbyte.byte = 0;
                can_flagsbyte.sFLAGSBITS.b0 = critical_error_flag;
                can_flagsbyte.sFLAGSBITS.b1 = dcac_general_error_flag;
                can_flagsbyte.sFLAGSBITS.b2 = fcs_general_error_flag;
                can_flagsbyte.sFLAGSBITS.b3 = fcs_ppmh2_error_flag;
                can_flagsbyte.sFLAGSBITS.b4 = dcac_recieve_flag;
                output_msg.data[1] = can_flagsbyte.byte;
                can_flagsbyte.byte = 0;
                can_flagsbyte.sFLAGSBITS.b0 = ibs_battery_0.watchdog_error_flag;
                can_flagsbyte.sFLAGSBITS.b1 = ibs_battery_1.watchdog_error_flag;
                can_flagsbyte.sFLAGSBITS.b2 = dcac_watchdog_error_flag;
                can_flagsbyte.sFLAGSBITS.b3 = fcs_watchdog_error_flag;
                can_flagsbyte.sFLAGSBITS.b4 = fcs_startup_error_flag;
                can_flagsbyte.sFLAGSBITS.b5 = ac_ambiant_temp.error_flag;
                can_flagsbyte.sFLAGSBITS.b6 = temp2.error_flag;
                can_flagsbyte.sFLAGSBITS.b7 = software_error_flag;
                output_msg.data[2] = can_flagsbyte.byte;
                can_flagsbyte.byte = 0;
                can_flagsbyte.sFLAGSBITS.b0 = battery_mode_button_flag;
                can_flagsbyte.sFLAGSBITS.b1 = hybrid_mode_button_flag;
                output_msg.data[3] = can_flagsbyte.byte;
                output_msg.data[4] = fcs_step;
                can_flagsbyte.byte = 0;
                can_flagsbyte.sFLAGSBITS.b0 = battery_soc_high_level_warning_flag;
                can_flagsbyte.sFLAGSBITS.b1 = battery_soc_low_level_warning_flag;
                can_flagsbyte.sFLAGSBITS.b2 = ac_output_apparent_power_high_level_warning_flag;
                can_flagsbyte.sFLAGSBITS.b3 = ibs_battery_sop_warning_flag;
                can_flagsbyte.sFLAGSBITS.b4 = ibs_battery_sop_error_flag;
                output_msg.data[5] = can_flagsbyte.byte;
                can_flagsbyte.byte = 0;
                can_flagsbyte.sFLAGSBITS.b0 = battery_soc_very_very_high_level_error_flag;
                can_flagsbyte.sFLAGSBITS.b1 = battery_soc_very_low_level_error_flag;
                can_flagsbyte.sFLAGSBITS.b2 = battery_overvoltage_warning_flag;
                can_flagsbyte.sFLAGSBITS.b3 = battery_overvoltage_error_flag;
                output_msg.data[6] = can_flagsbyte.byte;
                can_flagsbyte.sFLAGSBITS.b0 = battery_overcurrent_warning_flag;
                can_flagsbyte.sFLAGSBITS.b1 = battery_overcurrent_error_flag;
                can_flagsbyte.sFLAGSBITS.b2 = battery_overtemperature_charge_error_flag;
                can_flagsbyte.sFLAGSBITS.b3 = battery_overtemperature_discharge_error_flag;
//                can_flagsbyte.sFLAGSBITS.b4 = battery_board_overtemperature_error_flag;
                can_flagsbyte.sFLAGSBITS.b5 = battery_overtemperature_charge_warning_flag;
                can_flagsbyte.sFLAGSBITS.b6 = battery_overtemperature_discharge_warning_flag;
//                can_flagsbyte.sFLAGSBITS.b7 = battery_board_overtemperature_warning_flag;
                output_msg.data[7] = can_flagsbyte.byte;
                output_msg.field.dlc = CAN_DLC_8;
                output_msg.msgId = 0x2C00000;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;

            case 7:
                can_flagsbyte.byte = 0;
                can_flagsbyte.sFLAGSBITS.b0 = IHM_LEFT_BUTTON_GetValue();
                can_flagsbyte.sFLAGSBITS.b1 = IHM_MIDDLE_BUTTON_GetValue();
                can_flagsbyte.sFLAGSBITS.b2 = IHM_RIGHT_BUTTON_GetValue();
                can_flagsbyte.sFLAGSBITS.b3 = BATTERY_MODE_BUTTON_GetValue();
                can_flagsbyte.sFLAGSBITS.b4 = HYBRID_MODE_BUTTON_GetValue();
                output_msg.data[0] = can_flagsbyte.byte;
                can_flagsbyte.byte = 0;
                can_flagsbyte.sFLAGSBITS.b0 = DCAC_INPUT_RELAY_GetValue();
                can_flagsbyte.sFLAGSBITS.b1 = DCAC_PRECHARGE_RELAY_GetValue();
                can_flagsbyte.sFLAGSBITS.b2 = SUPPLY_PRECHARGE_RELAY_GetValue();
                can_flagsbyte.sFLAGSBITS.b3 = FCS_OUTPUT_RELAY_GetValue();
                can_flagsbyte.sFLAGSBITS.b4 = BUZZER_GetValue();
                can_flagsbyte.sFLAGSBITS.b5 = IHM_LEFT_BUTTON_LED_GetValue();
                can_flagsbyte.sFLAGSBITS.b6 = IHM_MIDDLE_BUTTON_LED_GetValue();
                can_flagsbyte.sFLAGSBITS.b7 = IHM_RIGHT_BUTTON_LED_GetValue();
                output_msg.data[1] = can_flagsbyte.byte;
                output_msg.data[2] = (uint8_t)(temp_1_b / 100);
                output_msg.data[3] = (uint8_t)(temp_2_b / 100);
                output_msg.data[4] = (uint8_t)supply_voltage_b;
                output_msg.data[5] = (uint8_t)(supply_voltage_b >> 8);
                output_msg.data[6] = (uint8_t)bus_voltage_b;
                output_msg.data[7] = (uint8_t)(bus_voltage_b >> 8);
                output_msg.field.dlc = CAN_DLC_8;
                output_msg.msgId = 0x2C00001;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;
            case 8:
                output_msg.data[0] = (uint8_t)ac_ambiant_temp.value_8;
                output_msg.data[1] = (uint8_t)temp2.value_8;
                output_msg.data[4] = (uint8_t)supply_voltage;
                output_msg.data[5] = (uint8_t)(supply_voltage >> 8);
                output_msg.data[6] = (uint8_t)bus_voltage;
                output_msg.data[7] = (uint8_t)(bus_voltage >> 8);
                output_msg.field.dlc = CAN_DLC_8;
                output_msg.msgId = 0x2C00002;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;
            case 9:
                output_msg.data[0] = (uint8_t)fcs_output_current;
                output_msg.data[1] = (uint8_t)(fcs_output_current >> 8);
                output_msg.data[2] = (uint8_t)requested_fcs_current;
                output_msg.data[3] = (uint8_t)(requested_fcs_current >> 8);
                output_msg.data[4] = (uint8_t)battery_output_current;
                output_msg.data[5] = (uint8_t)(battery_output_current >> 8);
                output_msg.data[6] = (uint8_t)system_output_current;
                output_msg.data[7] = (uint8_t)(system_output_current >> 8);
                output_msg.field.dlc = CAN_DLC_8;
                output_msg.msgId = 0x2C00003;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;
                
            case 17:
                output_msg.data[0] = version;
                output_msg.data[1] = subversion;
                output_msg.data[2] = subsubversion;
                output_msg.data[3] = (uint8_t) buzzer_5_minutes_cnt;
                output_msg.data[4] = (uint8_t) buzzer_5_minutes_cnt >> 8;
                output_msg.data[5] = (uint8_t) raz_erreur_ihm_flag;
                output_msg.field.dlc = CAN_DLC_6;
                output_msg.msgId = 0x2C00004;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;
                
//            case 18: 
//                output_msg.data[0] = (uint8_t) battery_0_sop_charge;
//                output_msg.data[1] = (uint8_t) battery_0_sop_charge >> 8;
//                output_msg.data[2] = (uint8_t) battery_1_sop_charge;
//                output_msg.data[3] = (uint8_t) battery_1_sop_charge >> 8;
//                output_msg.data[4] = (uint8_t) battery_0_sop_discharge;
//                output_msg.data[5] = (uint8_t) battery_0_sop_discharge >> 8;
//                output_msg.data[6] = (uint8_t) battery_1_sop_discharge;
//                output_msg.data[7] = (uint8_t) battery_1_sop_discharge >> 8;
//                output_msg.field.dlc = CAN_DLC_8;
//                output_msg.msgId = 0x2BA0002;
//                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
//                break;    
//                
            case 19: 
//                output_msg.data[0] = (uint8_t) worst_battery_soo;
//                output_msg.data[1] = (uint8_t) worst_battery_soo >> 8;
                output_msg.field.dlc = CAN_DLC_2;
                output_msg.msgId = 0x2BA0003;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;  
                
            case 20:
                output_msg.data[0] = 0;
                output_msg.data[1] = 1;
                output_msg.field.dlc = CAN_DLC_2;
                output_msg.msgId = 0x2FC1000;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;

            case 21:
                output_msg.data[0] = (uint8_t) inverter_cooling_pwm;
                output_msg.data[1] = (uint8_t) ambiant_cooling_pwm;
                output_msg.data[2] = (uint8_t) dcdc_cooling_pwm;
                output_msg.data[3] = (uint8_t) ac_fans_duty_cycle;
                output_msg.field.dlc = CAN_DLC_4;
                output_msg.msgId = 0xCFF0027;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;

            case 22:
                output_msg.data[0] = (uint8_t) min_ems_current / 100;
                output_msg.data[1] = (uint8_t) max_ems_current / 100;
                output_msg.data[2] = (uint8_t) requested_fcs_current / 100;
                output_msg.data[3] = (uint8_t) ibs_battery_0.max_charge_current;
                output_msg.data[4] = (uint8_t) (ibs_battery_0.max_charge_current >> 8);
                output_msg.data[5] = (uint8_t) ibs_battery_0.max_discharge_current;
                output_msg.data[6] = (uint8_t) (ibs_battery_0.max_discharge_current >> 8);
                output_msg.field.dlc = CAN_DLC_7;
                output_msg.msgId = 0xCFF0127;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;
                
            case 23:
                output_msg.data[0] = (uint8_t) ibs_battery_0.output_current;
                output_msg.data[1] = (uint8_t) (ibs_battery_0.output_current >> 8);
                output_msg.data[2] = (uint8_t) ibs_battery_1.output_current;
                output_msg.data[3] = (uint8_t) (ibs_battery_1.output_current >> 8);
                output_msg.data[4] = (uint8_t) battery_0_max_temperature;
                output_msg.data[5] = (uint8_t) battery_1_max_temperature;
                output_msg.data[6] = (uint8_t) battery_0_actual_max_board_temperature;
                output_msg.data[7] = (uint8_t) battery_1_actual_max_board_temperature;
                output_msg.field.dlc = CAN_DLC_8;
                output_msg.msgId = 0x2BA0001;
                CAN1_Transmit(CAN_PRIORITY_LOW, &output_msg);
                break;

            default:
                break;
            }
            can_frame_ind++; // Passage ï¿½ l'indice suivant
            if (can_frame_ind > 24)
                can_frame_ind = 0; // RAZ
            can_ready_flag = 0;
        }
        // </editor-fold>
    }

    return 0;
}

void TMR1_interruptHandler(void)
{
    can_ready_flag = 1;

    _100ms_timer_cnt++;
    if (_100ms_timer_cnt > 9)
    {
        _100ms_timer_ticks++;
        _100ms_timer_flag = 1;
        _100ms_timer_cnt = 0;

        _1s_timer_cnt++;
        if (_1s_timer_cnt > 9)
        {
            _1s_timer_flag = 1;
            _1s_timer_cnt = 0;
        }
        
        _300ms_timer_cnt++;
        if(_300ms_timer_cnt > 2)
        {  
            _300ms_timer_flag = 1;  //Set flag
            _300ms_timer_cnt = 0;   //RAZ
        }
    }
}

void ADC1_interruptHandler(void)
{
    if (ADC1_IsConversionComplete(adc_current_channel))
        adc_conversion_done_flag = 1;
}

void PWM2_SetDutyCycle(uint8_t duty_cycle)
{
    if (duty_cycle > 100)
        duty_cycle = 100;
    PDC3 = (uint16_t)duty_cycle * 10;
}
// void TANKEV_SetDutyCycle(uint8_t duty_cycle)
//{
//     PDC1 = (uint16_t)duty_cycle * 10;
// }
// void PWM1_SetDutyCycle(uint8_t duty_cycle)
//{
//     if(duty_cycle > 100) duty_cycle = 100;
//     PDC2 = (uint16_t)duty_cycle * 10;
// }
// void DAC_SetOutput(uint8_t voltage_reference)
//{
//     if(voltage_reference > 100) voltage_reference = 100;
//     CVR1CONbits.CVR = voltage_reference;        //Voltage reference set to 0V
// }

// <editor-fold defaultstate="collapsed" desc="DCAC MANAGEMENT">
//
void UART1_Rx_interruptHandler(void)
{
    if (UART1_IsRxReady())
    {
        rs232_rx_byte = UART1_Read();

        if (rs232_rx_byte == 0x28)
        {
            rs232_frame_recieving_flag = 1;
            rs232_rx_byte_cnt = 0;
        }
        else if (rs232_frame_recieving_flag)
        {
            if ((rs232_rx_byte == 0x4E) || (rs232_rx_byte == 0x0D))
            {
                rs232_rx_byte_cnt = 0;
                rs232_frame_recieving_flag = 0;
            }
            else
            {
                rs232_rx_frame[rs232_rx_byte_cnt] = rs232_rx_byte;
                rs232_rx_byte_cnt++;

                switch (rs232_frame_id)
                {
                case RS232FRAME_QMOD:
                    if (rs232_rx_byte_cnt >= 1)
                        rs232_frame_recieved_flag = 1;
                    break;
                case RS232FRAME_QPIWS:
                    if (rs232_rx_byte_cnt >= 32)
                        rs232_frame_recieved_flag = 1;
                    break;
                case RS232FRAME_QPIGS:
                    if (rs232_rx_byte_cnt >= 106)
                        rs232_frame_recieved_flag = 1;
                    break;
                default:
                    break;
                }
            }
        }

        //        rsmsgdebug.msgId = 0xCCCCC;
        //        rsmsgdebug.data[0] = (uint8_t)rs232_rx_byte;
        //        rsmsgdebug.data[1] = (uint8_t)rs232_rx_byte_cnt;
        //        rsmsgdebug.field.dlc = CAN_DLC_2;
        //        CAN1_Transmit(CAN_PRIORITY_HIGH, &rsmsgdebug);
    }
}

// uint16_t cal_crc_half(char *pin, uint8_t len)
//{
//     uint16_t crc;
//     uint8_t da;
//     char *ptr;
//     uint8_t bCRCHign;
//     uint8_t bCRCLow;
//     uint16_t crc_ta[16]=
//     {
//         0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
//         0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
//     };
//
//     ptr=pin;
//     crc=0;
//     while(len--!=0)
//     {
//         da=((uint8_t)(crc>>8))>>4;
//         crc<<=4;
//         crc^=crc_ta[da^(*ptr>>4)];
//         da=((uint8_t)(crc>>8))>>4;
//         crc<<=4;
//         crc^=crc_ta[da^(*ptr&0x0f)];
//         ptr++;
//     }
//     bCRCLow = crc;
//     bCRCHign= (uint8_t)(crc>>8);
//     if(bCRCLow==0x28||bCRCLow==0x0d||bCRCLow==0x0a)
//     {
//         bCRCLow++;
//     }
//     if(bCRCHign==0x28||bCRCHign==0x0d||bCRCHign==0x0a)
//     {
//         bCRCHign++;
//     }
//     crc = ((uint16_t)bCRCHign)<<8;
//     crc += bCRCLow;
//     return(crc);
// }

uint8_t charToDec(char input_char)
{
    if ((input_char > 0x39) || (input_char < 0x30))
        return 0x0F;
    else
        return (uint8_t)(input_char & 0x0F);
    /*switch(input_char)
    {
        case 0x30:
            result = 0;
            break;
        case 0x31:
            result = 1;
            break;
        case 0x32:
            result = 2;
            break;
        case 0x33:
            result = 3;
            break;
        case 0x34:
            result = 4;
            break;
        case 0x35:
            result = 5;
            break;
        case 0x36:
            result = 6;
            break;
        case 0x37:
            result = 7;
            break;
        case 0x38:
            result = 8;
            break;
        case 0x39:
            result = 9;
            break;
        default:
            result = 0xF;
            break;
    }
    return result;*/
}
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="FCS COMMANDS">
void FCS_Start(void)
{
    command_msg.data[0] = 0;
    command_msg.data[1] = 0;
    command_msg.data[2] = 1;
    command_msg.field.dlc = CAN_DLC_3;
    command_msg.msgId = 0x2FC1001;
    CAN1_Transmit(CAN_PRIORITY_HIGH, &command_msg);
}
void FCS_Stop(void)
{
    command_msg.data[0] = 0;
    command_msg.data[1] = 1;
    command_msg.data[2] = 0;
    command_msg.field.dlc = CAN_DLC_3;
    command_msg.msgId = 0x2FC1001;
    CAN1_Transmit(CAN_PRIORITY_HIGH, &command_msg);
}
void FCS_CriticalStop(void)
{
    command_msg.data[0] = 1;
    command_msg.data[1] = 0;
    command_msg.data[2] = 0;
    command_msg.field.dlc = CAN_DLC_3;
    command_msg.msgId = 0x2FC1001;
    CAN1_Transmit(CAN_PRIORITY_HIGH, &command_msg);
}
void FCS_MaintenanceMode(void)
{
    command_msg.data[0] = 0;
    command_msg.data[1] = 0;
    command_msg.data[2] = 0x82;
    command_msg.data[7] = 0xFF;
    command_msg.field.dlc = CAN_DLC_8;
    command_msg.msgId = 0x2FC1001;
    CAN1_Transmit(CAN_PRIORITY_HIGH, &command_msg);
}
void FCS_ResetErrors(void)
{
    command_msg.data[0] = 0;
    command_msg.data[1] = 0;
    command_msg.data[2] = 0;
    command_msg.data[3] = 0x01;
    command_msg.data[7] = 0xFF;
    command_msg.field.dlc = CAN_DLC_8;
    command_msg.msgId = 0x2FC1001;
    CAN1_Transmit(CAN_PRIORITY_HIGH, &command_msg);
}
void FCS_SetCurrentLimit(uint16_t current_limit)
{
    command_msg.data[0] = current_limit;
    command_msg.data[1] = (current_limit >> 8);
    command_msg.field.dlc = CAN_DLC_2;
    command_msg.msgId = 0x2FC1002;
    CAN1_Transmit(CAN_PRIORITY_HIGH, &command_msg);
}
// </editor-fold>

int16_t IBS_CalculateCurrent(uint8_t data0, uint8_t data1, uint8_t data2)
{
    expo_pow = pow(2.0, (((data0 & 0x7F) << 1 | ((data1 & 0x80) >> 7)) - 127.0));

    mant_sum = 1.0;
    can_flagsbyte.byte = data1;
    if (can_flagsbyte.sFLAGSBITS.b6)
        mant_sum += 0.50000000000000;
    if (can_flagsbyte.sFLAGSBITS.b5)
        mant_sum += 0.25000000000000;
    if (can_flagsbyte.sFLAGSBITS.b4)
        mant_sum += 0.12500000000000;
    if (can_flagsbyte.sFLAGSBITS.b3)
        mant_sum += 0.06250000000000;
    if (can_flagsbyte.sFLAGSBITS.b2)
        mant_sum += 0.03125000000000;
    if (can_flagsbyte.sFLAGSBITS.b1)
        mant_sum += 0.01562500000000;
    if (can_flagsbyte.sFLAGSBITS.b0)
        mant_sum += 0.00781250000000;
    can_flagsbyte.byte = data2;
    if (can_flagsbyte.sFLAGSBITS.b7)
        mant_sum += 0.00390625000000;
    if (can_flagsbyte.sFLAGSBITS.b6)
        mant_sum += 0.00195312500000;
    if (can_flagsbyte.sFLAGSBITS.b5)
        mant_sum += 0.00097656250000;
    if (can_flagsbyte.sFLAGSBITS.b4)
        mant_sum += 0.00048828125000;

    can_flagsbyte.byte = data0;
    if (can_flagsbyte.sFLAGSBITS.b7)
        return ((int16_t)(expo_pow * mant_sum * 100));
    else
        return ((int16_t)(expo_pow * mant_sum * (-100)));
}

/**
 * @brief Min value of two floats
 * 
 * @param a
 * @param b
 * @return 
 */
float min (float a, float b)
{
    if(a>b)
        return b;
    else
        return a;
}

/**
 * @brief Max value of two floats
 * 
 * @param a
 * @param b
 * @return 
 */
float max (float a, float b)
{
    if(a<b)
        return b;
    else
        return a;
}

/**
 * @brief Value saturation between specified minimum and maximum values
 *
 * @param min
 * @param value
 * @param max
 * @return float
 */
float saturation(float min, float value, float max)
{
    if (value < min)
    {
        return min;
    }
    else if (value > max)
    {
        return max;
    }
    else
    {
        return value;
    }
}

/**
 * @brief Dynamics limiter function
 * 
 * @param limitedValue
 * @param setPoint
 * @param ramp
 * @return 
 */
float dynamicsLimiter (float limitedValue, float setPoint, float ramp )
{
    limitedValue += saturation(-ramp, setPoint - limitedValue, +ramp);
    return limitedValue;
}

/**
 * @brief PID Controller function
 *
 * @param Kp
 * @param Ki
 * @param Kd
 * @param int_part
 * @param target
 * @param measurement
 * @return uint8_t
 */
uint8_t PID_controller(float Kp, float Ki, float Kd, float *int_part, float target, float measurement)
{
    float output; // Output of the PID Controller

    *int_part = *int_part + Ki * (measurement - target);    // Integral part calculation
    *int_part = saturation(-20.0 / Ki, *int_part, 100.0 / Ki); // Integral part saturation

    output = Kp * (measurement - target) + (*int_part);     // Output calculation
    output = saturation(0, output, 100);                     // Output saturation
    
    uint8_t output_int = (uint8_t) output;

    return output_int;
}

/**
 * @brief timer function 
 * 
 * @param counter
 * @param duration The duration in ms
 * @param reset
 * @return 
 */
bool timer(uint16_t *counter, uint16_t duration, bool reset)
{
    // Timer output
    bool output = false;
    
    // Increment counter by 100ms
    *counter += 100;
    
    if(reset)
    {
        *counter = 0;
    }
    else if (*counter >= duration)
    {
       output = true; 
    }
    
    return output;
}



/**
 End of File
*/
