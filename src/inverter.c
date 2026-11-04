/**
 * @file inverter.c
 * @brief Onduleur DC/AC en RS232 (voir inverter.h).
 */
#include <string.h>
#include "inverter.h"
#include "generic.h"
#include "smb_config.h"
#include "hal/uart_port.h"

#define RS232_RX_FRAME_SIZE   120
#define RS232_START_BYTE      0x28 /* '(' */
#define RS232_NAK_BYTE        0x4E /* 'N' */
#define RS232_END_BYTE        0x0D /* '\r' */
#define QMOD_LENGTH           1
#define QPIWS_LENGTH          32
#define QPIGS_LENGTH          106
#define POWER_AVERAGE_SIZE    10

sInverter inverter;

/* Réception (ISR) */
static char rx_frame[RS232_RX_FRAME_SIZE];
static uint8_t rx_byte_cnt;
static bool frame_receiving;
static uint8_t frame_id; /* requête en cours : RS232FRAME_* */

/* Polling */
static uint8_t poll_ind;

/* Supervision */
static uint8_t watchdog_cnt;
static uint8_t recieve_cnt;
static uint8_t power_error_cpt;
static uint8_t soc_disconnect_cpt;
static uint16_t power_tab[POWER_AVERAGE_SIZE];
static uint8_t power_i;

/* Requêtes (commande + CRC + '\r'), octets identiques à l'ancien printf */
static const uint8_t REQ_QMOD[]  = {'Q', 'M', 'O', 'D', 0x49, 0xC1, 0x0D};
static const uint8_t REQ_QPIWS[] = {'Q', 'P', 'I', 'W', 'S', 0xB4, 0xDA, 0x0D};
static const uint8_t REQ_QPIGS[] = {'Q', 'P', 'I', 'G', 'S', 0xB7, 0xA9, 0x0D};

void Inverter_Init(void)
{
    uint8_t i;
    memset(&inverter, 0, sizeof(inverter));
    for (i = 0; i < RS232_RX_FRAME_SIZE; i++)
        rx_frame[i] = 0;
    rx_byte_cnt = 0;
    frame_receiving = false;
    frame_id = RS232FRAME_NONE;
    poll_ind = 0;
    watchdog_cnt = 0;
    recieve_cnt = 0;
    power_error_cpt = 0;
    soc_disconnect_cpt = 0;
    for (i = 0; i < POWER_AVERAGE_SIZE; i++)
        power_tab[i] = 0;
    power_i = 0;
}

/* ---- Réception ------------------------------------------------------------------- */

void Inverter_OnRxInterrupt(void)
{
    if (UartPort_IsRxReady())
        Inverter_OnRxByte(UartPort_ReadByte());
}

void Inverter_OnRxByte(uint8_t byte)
{
    if (byte == RS232_START_BYTE)
    {
        frame_receiving = true;
        rx_byte_cnt = 0;
    }
    else if (frame_receiving)
    {
        if ((byte == RS232_NAK_BYTE) || (byte == RS232_END_BYTE))
        {
            rx_byte_cnt = 0;
            frame_receiving = false;
        }
        else
        {
            if (rx_byte_cnt < RS232_RX_FRAME_SIZE) /* garde ajoutée : l'origine écrivait hors tableau */
                rx_frame[rx_byte_cnt] = (char)byte;
            rx_byte_cnt++;

            switch (frame_id)
            {
            case RS232FRAME_QMOD:
                if (rx_byte_cnt >= QMOD_LENGTH)
                    inverter.frame_received = true;
                break;
            case RS232FRAME_QPIWS:
                if (rx_byte_cnt >= QPIWS_LENGTH)
                    inverter.frame_received = true;
                break;
            case RS232FRAME_QPIGS:
                if (rx_byte_cnt >= QPIGS_LENGTH)
                    inverter.frame_received = true;
                break;
            default:
                break;
            }
        }
    }
}

/* ---- Décodage -------------------------------------------------------------------- */

static void decode_qmod(void)
{
    switch (rx_frame[0])
    {
    case 'B': inverter.mode = INVERTER_BATTERY_MODE; break;
    case 'F': inverter.mode = INVERTER_FAULT_MODE; break;
    case 'H': inverter.mode = INVERTER_POWER_SAVING_MODE; break;
    case 'L': inverter.mode = INVERTER_AC_SOURCE_MODE; break;
    case 'S': inverter.mode = INVERTER_STANDBY_MODE; break;
    case 'P': inverter.mode = INVERTER_POWER_ON_MODE; break;
    default: break;
    }

    inverter.toggle_qmod_flag = !inverter.toggle_qmod_flag;
    watchdog_cnt = 0;
    recieve_cnt = 0;
    inverter.recieve_flag = 0;
}

static void decode_qpiws(void)
{
    inverter.error_flag = (rx_frame[1] & 0x01);
    inverter.bus_overvoltage_error_flag = (rx_frame[2] & 0x01);
    inverter.bus_undervoltage_error_flag = (rx_frame[3] & 0x01);
    inverter.bus_soft_fail_error_flag = (rx_frame[4] & 0x01);
    inverter.line_fail_warning_flag = (rx_frame[5] & 0x01);
    inverter.opv_short_warning_flag = (rx_frame[6] & 0x01);
    inverter.voltage_too_low_error_flag = (rx_frame[7] & 0x01);
    inverter.voltage_too_high_error_flag = (rx_frame[8] & 0x01);
    inverter.overtemperature_warning_flag = (rx_frame[9] & 0x01);
    if (inverter.error_flag == 1)
        inverter.overtemperature_error_flag = inverter.overtemperature_warning_flag;
    inverter.fan_locked_warning_flag = (rx_frame[10] & 0x01);
    if (inverter.error_flag == 1)
        inverter.fan_locked_error_flag = inverter.fan_locked_warning_flag;
    inverter.battery_voltage_high_warning_flag = (rx_frame[11] & 0x01);
    if (inverter.error_flag == 1)
        inverter.battery_voltage_high_error_flag = inverter.battery_voltage_high_warning_flag;
    inverter.battery_voltage_low_warning_flag = (rx_frame[12] & 0x01);
    inverter.overcharge_error_flag = (rx_frame[13] & 0x01);
    inverter.battery_under_shutdown_voltage_warning_flag = (rx_frame[14] & 0x01);
    inverter.battery_derating_warning_flag = (rx_frame[15] & 0x01);
    inverter.overload_warning_flag = (rx_frame[16] & 0x01);
    if (inverter.error_flag == 1 || inverter.apparent_power_very_high_error_flag == 1)
        inverter.overload_error_flag = inverter.overload_warning_flag;
    inverter.eeprom_warning_flag = (rx_frame[17] & 0x01);
    inverter.over_current_error_flag = (rx_frame[18] & 0x01);
    inverter.soft_start_error_flag = (rx_frame[19] & 0x01);
    inverter.self_test_error_flag = (rx_frame[20] & 0x01);
    inverter.ac_output_dc_voltage_too_high_error_flag = (rx_frame[21] & 0x01);
    inverter.battery_disconnected_error_flag = (rx_frame[22] & 0x01);
    inverter.current_sensor_error_flag = (rx_frame[23] & 0x01);
    inverter.battery_short_circuit_error_flag = (rx_frame[24] & 0x01);
    inverter.power_limit_warning_flag = (rx_frame[25] & 0x01);
    inverter.pv_voltage_high_warning_flag = (rx_frame[26] & 0x01);
    inverter.mppt_overload_error_flag = (rx_frame[27] & 0x01);
    inverter.mppt_overload_warning_flag = (rx_frame[28] & 0x01);
    inverter.battery_too_low_to_charge_warning_flag = (rx_frame[29] & 0x01);

    inverter.toggle_qpiws_flag = !inverter.toggle_qpiws_flag;
    watchdog_cnt = 0;
}

/* Nombre décimal de 4 chiffres ASCII aux positions p0..p3 */
static uint16_t decimal4(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3)
{
    uint16_t v;
    v = (uint16_t)(Generic_CharToDec(rx_frame[p0]) * 1000);
    v += (uint16_t)(Generic_CharToDec(rx_frame[p1]) * 100);
    v += (uint16_t)(Generic_CharToDec(rx_frame[p2]) * 10);
    v += Generic_CharToDec(rx_frame[p3]);
    return v;
}

static void decode_qpigs(void)
{
    /* Champs non exploités (tension/fréquence réseau, fréquence sortie, charge %, bus interne,
     * tension/courants batterie onduleur) : non décodés, comme à l'origine. */
    inverter.ac_output_voltage = decimal4(11, 12, 13, 15);        /* "230.1" -> 2301 */
    inverter.ac_output_apparent_power = decimal4(22, 23, 24, 25);
    inverter.ac_output_active_power = decimal4(27, 28, 29, 30);
    inverter.dcac_temp = (uint8_t)decimal4(54, 55, 56, 57);

    inverter.load_status_flag = (rx_frame[85] & 0x01);
    inverter.charging_status_flag = (rx_frame[87] & 0x01);
    inverter.scc_charging_status_flag = (rx_frame[88] & 0x01);
    inverter.ac_charging_status_flag = (rx_frame[89] & 0x01);
    inverter.charging_to_floating_mode_flag = (rx_frame[104] & 0x01);
    inverter.device_switched_on_flag = (rx_frame[105] & 0x01);

    inverter.toggle_qpigs_flag = !inverter.toggle_qpigs_flag;
    watchdog_cnt = 0;
}

void Inverter_ProcessFrame(void)
{
    if (frame_id == RS232FRAME_QMOD)
        decode_qmod();
    else if (frame_id == RS232FRAME_QPIWS)
        decode_qpiws();
    else if (frame_id == RS232FRAME_QPIGS)
        decode_qpigs();

    frame_id = RS232FRAME_NONE;
    inverter.frame_received = false;
}

/* ---- Polling ---------------------------------------------------------------------- */

static void clear_measures(void)
{
    inverter.moy_ac_output_apparent_power = 0;
    inverter.ac_output_active_power = 0;
    inverter.ac_output_voltage = 0;
}

void Inverter_Poll300ms(uint8_t smb_step)
{
    if ((smb_step > 30) && (smb_step < 200))
    {
        poll_ind++;
        if (poll_ind == 1)
        {
            UartPort_WriteBytes(REQ_QMOD, sizeof(REQ_QMOD));
            frame_id = RS232FRAME_QMOD;
        }
        else if (poll_ind == 6)
        {
            UartPort_WriteBytes(REQ_QPIWS, sizeof(REQ_QPIWS));
            frame_id = RS232FRAME_QPIWS;
        }
        else if (poll_ind == 13)
        {
            UartPort_WriteBytes(REQ_QPIGS, sizeof(REQ_QPIGS));
            frame_id = RS232FRAME_QPIGS;
        }
        else if (poll_ind > 24)
        {
            poll_ind = 0;
            frame_id = RS232FRAME_NONE;
        }
    }
    else
    {
        clear_measures();
        poll_ind = 0;
        frame_id = RS232FRAME_NONE;
    }
}

/* ---- Supervision ------------------------------------------------------------------ */

void Inverter_Supervise100ms(uint8_t battery_soc)
{
    /* Déconnexion de l'onduleur sur SOC bas avec charge présente */
    if (battery_soc < DCAC_SOC_DISCONNECT_LEVEL && inverter.moy_ac_output_apparent_power > DCAC_SOC_DISCONNECT_POWER_LEVEL)
    {
        soc_disconnect_cpt++;
        inverter.soc_disconnect_error_flag = soc_disconnect_cpt >= DCAC_SOC_DISCONNECT_TIMEOUT;
    }
    else if (inverter.soc_disconnect_error_flag && battery_soc < BATTERY_SOC_LOW_LEVEL)
    {
        soc_disconnect_cpt = 0;
        inverter.soc_disconnect_error_flag = true; /* maintenue jusqu'au retour du SOC au-dessus du niveau bas */
    }
    else
    {
        soc_disconnect_cpt = 0;
        inverter.soc_disconnect_error_flag = false;
    }

    /* Surcharge de sortie : deux seuils / deux temporisations ; le compteur n'est pas remis à
     * zéro quand la puissance redescend sous AC_OUTPUT_POWER_HIGH_LEVEL (origine). */
    if (inverter.moy_ac_output_apparent_power > AC_OUTPUT_POWER_HIGH_LEVEL)
    {
        if ((inverter.moy_ac_output_apparent_power > AC_OUTPUT_POWER_VERY_HIGH_LEVEL) ||
            (inverter.moy_ac_output_apparent_power > AC_OUTPUT_POWER_VERY_CRITICAL_HIGH_LEVEL))
        {
            power_error_cpt++;
            if (((inverter.moy_ac_output_apparent_power > AC_OUTPUT_POWER_VERY_HIGH_LEVEL) && (power_error_cpt > AC_OUTPUT_POWER_VERY_HIGH_TIMEOUT)) ||
                ((inverter.moy_ac_output_apparent_power > AC_OUTPUT_POWER_VERY_CRITICAL_HIGH_LEVEL) && (power_error_cpt > AC_OUTPUT_POWER_VERY_CRITICAL_HIGH_TIMEOUT)))
            {
                inverter.apparent_power_very_high_error_flag = 1;
                power_error_cpt = 0;
            }
        }
        else
            power_error_cpt = 0;
    }
}

void Inverter_UpdatePowerAverage100ms(void)
{
    uint8_t i;
    uint16_t sum;

    power_tab[power_i] = inverter.ac_output_apparent_power;
    power_i++;
    if (power_i == POWER_AVERAGE_SIZE)
    {
        power_i = 0;
        /* TODO(bug 11) : somme sur 16 bits comme sur la cible (int = 16 bits) : déborde au-delà
         * d'une moyenne de 6553 VA. */
        sum = 0;
        for (i = 0; i < POWER_AVERAGE_SIZE; i++)
            sum = (uint16_t)(sum + power_tab[i]);
        inverter.moy_ac_output_apparent_power = sum / POWER_AVERAGE_SIZE;
    }
    if (inverter.moy_ac_output_apparent_power < AC_OUTPUT_POWER_HIGH_LEVEL)
        inverter.apparent_power_high_warning_flag = 0;
    else
        inverter.apparent_power_high_warning_flag = 1;
}

void Inverter_Update1s(uint8_t smb_step)
{
    if ((smb_step >= 100) && (smb_step < 200))
    {
        watchdog_cnt++;
        recieve_cnt++;
        if (watchdog_cnt > DCAC_WATCHDOG_TIMEOUT)
        {
            inverter.watchdog_error_flag = 1;
            watchdog_cnt = 0;
        }
        if (recieve_cnt > DCAC_RECEIVE_TIMEOUT)
        {
            inverter.recieve_flag = 1;
            recieve_cnt = 0;
        }
    }
}

void Inverter_UpdateGeneralError(bool ambiant_temp_error)
{
    if (inverter.watchdog_error_flag || inverter.error_flag || ambiant_temp_error)
        inverter.general_error_flag = 1;
}

void Inverter_ClearData(void)
{
    inverter.mode = INVERTER_UNDEFINED_MODE;
    clear_measures();
}

void Inverter_ClearDataAfterStop(void)
{
    inverter.raz_data_after_stop_flag = 1;
    Inverter_ClearData();
}

void Inverter_ResetErrors(void)
{
    inverter.watchdog_error_flag = 0;
    inverter.error_flag = 0;
    inverter.apparent_power_very_high_error_flag = 0;
    inverter.general_error_flag = 0;
}
