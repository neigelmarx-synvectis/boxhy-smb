/**
 * @file battery.c
 * @brief Batteries IBS (voir battery.h).
 */
#include <math.h>
#include <string.h>
#include "battery.h"
#include "smb_config.h"
#include "hal/can_port.h"

#define IBS_KEEP_ALIVE_ID     0x180AD000UL
#define IBS_INFO_ID_MASK      0xFFFF00FEUL /* 0x18FF xx D0/D1 : xx = index info, bit 0 = batterie */
#define IBS_INFO_ID_BASE      0x18FF00D0UL

sBatteryPack battery;

static uint8_t keepalive_data[1];
static CAN_MSG_OBJ keepalive_msg;

/* Compteurs des supervisions SOC (non remis à zéro à l'acquittement, comme à l'origine) */
static uint8_t soc_very_low_level_error_cpt;
/* TODO(bug 2) : compteur déclaré bool à l'origine : "cpt++" vaut toujours 1, le timeout n'est
 * jamais atteint et l'erreur SOC "très très haut" ne se déclenche jamais. Reproduit ci-dessous. */
static uint8_t soc_very_very_high_level_error_cpt;

void Battery_Init(void)
{
    memset(&battery, 0, sizeof(battery));
    soc_very_low_level_error_cpt = 0;
    soc_very_very_high_level_error_cpt = 0;

    keepalive_data[0] = 0;
    keepalive_msg.msgId = IBS_KEEP_ALIVE_ID;
    keepalive_msg.field.frameType = CAN_FRAME_DATA;
    keepalive_msg.field.idType = CAN_FRAME_EXT;
    keepalive_msg.field.dlc = CAN_DLC_1;
    keepalive_msg.data = keepalive_data;
}

int16_t Battery_DecodeIbsCurrent(uint8_t data0, uint8_t data1, uint8_t data2)
{
    uFLAGSBYTE flags;
    float expo_pow;
    float mant_sum;

    expo_pow = pow(2.0, (((data0 & 0x7F) << 1 | ((data1 & 0x80) >> 7)) - 127.0));

    mant_sum = 1.0;
    flags.byte = data1;
    if (flags.sFLAGSBITS.b6) mant_sum += 0.50000000000000;
    if (flags.sFLAGSBITS.b5) mant_sum += 0.25000000000000;
    if (flags.sFLAGSBITS.b4) mant_sum += 0.12500000000000;
    if (flags.sFLAGSBITS.b3) mant_sum += 0.06250000000000;
    if (flags.sFLAGSBITS.b2) mant_sum += 0.03125000000000;
    if (flags.sFLAGSBITS.b1) mant_sum += 0.01562500000000;
    if (flags.sFLAGSBITS.b0) mant_sum += 0.00781250000000;
    flags.byte = data2;
    if (flags.sFLAGSBITS.b7) mant_sum += 0.00390625000000;
    if (flags.sFLAGSBITS.b6) mant_sum += 0.00195312500000;
    if (flags.sFLAGSBITS.b5) mant_sum += 0.00097656250000;
    if (flags.sFLAGSBITS.b4) mant_sum += 0.00048828125000;

    flags.byte = data0;
    if (flags.sFLAGSBITS.b7)
        return ((int16_t)(expo_pow * mant_sum * 100));
    else
        return ((int16_t)(expo_pow * mant_sum * (-100)));
}

/* Mot 16 bits big-endian signé (1/10) -> 1 unité */
static int8_t decode_temperature_be(uint8_t hi, uint8_t lo)
{
    return (int8_t)(((int16_t)(lo | ((uint16_t)hi << 8))) / 10);
}

/* TODO(bug 5) : poids fort multiplié par 255 au lieu de 256 (origine conservée). */
static int8_t decode_temperature_255(uint8_t hi, uint8_t lo)
{
    return (int8_t)(((int16_t)(lo + (uint16_t)hi * 255)) / 10);
}

bool Battery_HandleCanFrame(const CAN_MSG_OBJ *msg)
{
    uFLAGSBYTE flags;
    const uint8_t *d = msg->data;
    sIBS *ibs;

    if ((msg->msgId & IBS_INFO_ID_MASK) != IBS_INFO_ID_BASE)
        return false;

    ibs = &battery.ibs[msg->msgId & 0x01];

    switch ((uint8_t)(msg->msgId >> 8))
    {
    case 0x02: /* courant, SOP */
        ibs->output_current = Battery_DecodeIbsCurrent(d[0], d[1], d[2]);
        ibs->sop = d[4];
        ibs->watchdog.cnt = 0;
        break;
    case 0x03: /* températures cellules */
        ibs->avg_cell_temperature = decode_temperature_be(d[4], d[5]);
        ibs->max_cell_temperature = decode_temperature_255(d[2], d[3]);
        break;
    case 0x04: /* températures carte */
        ibs->max_board_temperature = decode_temperature_255(d[2], d[3]);
        break;
    case 0x05: /* état */
        ibs->soc = (uint8_t)(((int16_t)(d[1] | ((uint16_t)d[0] << 8))) / 100);
        break;
    case 0x07: /* infos */
        flags.byte = d[1];
        ibs->system_running_flag = flags.sFLAGSBITS.b0;
        ibs->balancing_flag = flags.sFLAGSBITS.b1;
        flags.byte = d[2];
        ibs->error_flag = !flags.sFLAGSBITS.b1;
        ibs->low_soc_warning_flag = flags.sFLAGSBITS.b5;
        break;
    case 0x0C: /* courants max */
        ibs->max_discharge_current = Battery_DecodeIbsCurrent(d[0], d[1], d[2]);
        ibs->max_charge_current = Battery_DecodeIbsCurrent(d[4], d[5], d[6]);
        break;
    default: /* 0x3C et autres : sans effet */
        break;
    }
    return true;
}

static void supervise_soc(void)
{
    /* Niveau haut : TODO(bug 2) les deux tests portent sur VERY_VERY_HIGH et le compteur est un bool */
    if (battery.soc > BATTERY_SOC_VERY_VERY_HIGH_LEVEL)
    {
        battery.soc_high_level_warning_flag = 1;
        if (battery.soc > BATTERY_SOC_VERY_VERY_HIGH_LEVEL)
        {
            soc_very_very_high_level_error_cpt = 1; /* ++ sur un bool (origine) */
            if (soc_very_very_high_level_error_cpt > BATTERY_SOC_VERY_VERY_HIGH_TIMEOUT)
            {
                battery.soc_very_very_high_level_error_flag = 1;
                soc_very_very_high_level_error_cpt = 0;
            }
            else
                battery.soc_very_very_high_level_error_flag = 0;
        }
    }
    else
    {
        battery.soc_high_level_warning_flag = 0;
        battery.soc_very_very_high_level_error_flag = 0;
    }

    /* Niveau bas : l'erreur n'est vraie que le tick du déclenchement (origine) */
    if (battery.soc < BATTERY_SOC_LOW_LEVEL)
    {
        battery.soc_low_level_warning_flag = 1;
        if (battery.soc < BATTERY_SOC_VERY_LOW_LEVEL)
        {
            soc_very_low_level_error_cpt++;
            if (soc_very_low_level_error_cpt > BATTERY_SOC_VERY_LOW_TIMEOUT)
            {
                battery.soc_very_low_level_error_flag = 1;
                soc_very_low_level_error_cpt = 0;
            }
            else
                battery.soc_very_low_level_error_flag = 0;
        }
    }
    else
    {
        battery.soc_low_level_warning_flag = 0;
        battery.soc_very_low_level_error_flag = 0;
    }
}

static void supervise_overcurrent(void)
{
    /* TODO(bug 9) : compteur 8 bits et timeout 450 : l'erreur ne se déclenche jamais (origine). */
    bool cond = battery.ibs[0].output_current > BATTERY_OVERCURRENT_DISCHARGE_LEVEL ||
                battery.ibs[0].output_current < (battery.ibs[0].max_charge_current * BATTERY_OVERCURRENT_CHARGE_RATIO) ||
                battery.ibs[1].output_current > BATTERY_OVERCURRENT_DISCHARGE_LEVEL ||
                battery.ibs[1].output_current < (battery.ibs[1].max_charge_current * BATTERY_OVERCURRENT_CHARGE_RATIO);
    battery.overcurrent_warning_flag = cond;
    TimedFlag8_Update(&battery.overcurrent, cond, BATTERY_OVERCURRENT_TIMEOUT);
}

static void supervise_overtemperature(void)
{
    bool cond;

    battery.max_temperature = Generic_MaxF(battery.ibs[0].max_cell_temperature, battery.ibs[1].max_cell_temperature);

    cond = (battery.max_temperature > BATTERY_MAX_CHARGE_TEMPERATURE) && (battery.output_current < 0);
    battery.overtemperature_charge_warning_flag = cond;
    TimedFlag8_Update(&battery.overtemperature_charge, cond, BATTERY_OVERTEMPERATURE_CHARGE_TIMEOUT);

    cond = (battery.max_temperature > BATTERY_MAX_DISCHARGE_TEMPERATURE) && (battery.output_current > 0);
    battery.overtemperature_discharge_warning_flag = cond;
    TimedFlag8_Update(&battery.overtemperature_discharge, cond, BATTERY_OVERTEMPERATURE_DISCHARGE_TIMEOUT);
}

static void supervise_overvoltage(uint16_t bus_voltage)
{
    bool cond = bus_voltage > BATTERY_OVERVOLTAGE_LEVEL;
    battery.overvoltage_warning_flag = cond;
    TimedFlag8_Update(&battery.overvoltage, cond, BATTERY_OVERVOLTAGE_TIMEOUT);
}

static void supervise_sop(uint8_t smb_step)
{
    if (smb_step >= 30)
    {
        if ((battery.ibs[0].sop < BATTERY_SOP_WARNING_LOW_LEVEL) || (battery.ibs[1].sop < BATTERY_SOP_WARNING_LOW_LEVEL))
        {
            battery.sop_warning_flag = 1;
            if ((battery.ibs[0].sop < BATTERY_SOP_WARNING_VERY_LOW_LEVEL) || (battery.ibs[1].sop < BATTERY_SOP_WARNING_VERY_LOW_LEVEL))
                battery.sop_error_flag = 1;
        }
        else
        {
            battery.sop_warning_flag = 0;
            battery.sop_error_flag = 0;
        }
    }
}

void Battery_Supervise100ms(uint8_t smb_step, uint16_t bus_voltage)
{
    supervise_soc();
    supervise_overcurrent();
    supervise_overtemperature();
    supervise_overvoltage(bus_voltage);
    supervise_sop(smb_step);
}

void Battery_Aggregate100ms(void)
{
    battery.soc = (battery.ibs[0].soc + battery.ibs[1].soc) / 2;
    battery.output_current = (int16_t)(battery.ibs[0].output_current + battery.ibs[1].output_current);
}

void Battery_Update1s(void)
{
    uint8_t i;

    battery.keep_alive_index = !battery.keep_alive_index;
    keepalive_msg.msgId = IBS_KEEP_ALIVE_ID | ((uint16_t)battery.keep_alive_index << 8);
    CanPort_Transmit(CAN_PRIORITY_HIGH, &keepalive_msg);

    for (i = 0; i < BATTERY_NB; i++)
        TimedFlag8_Update(&battery.ibs[i].watchdog, true, WATCHDOG_TIMEOUT);
}

void Battery_ResetErrors(void)
{
    uint8_t i;
    for (i = 0; i < BATTERY_NB; i++)
    {
        battery.ibs[i].error_flag = 0;
        battery.ibs[i].watchdog.error = 0;
    }
    battery.soc_very_low_level_error_flag = 0;
    battery.soc_very_very_high_level_error_flag = 0;
    battery.overtemperature_discharge.error = 0;
    battery.overtemperature_charge.error = 0;
    battery.overcurrent.error = 0;
    battery.overvoltage.error = 0;
    battery.sop_error_flag = 0;
}

bool Battery_HasCriticalError(void)
{
    return battery.ibs[0].error_flag || battery.ibs[1].error_flag ||
           battery.ibs[0].watchdog.error || battery.ibs[1].watchdog.error ||
           battery.soc_very_low_level_error_flag || battery.soc_very_very_high_level_error_flag ||
           battery.overtemperature_discharge.error || battery.overtemperature_charge.error ||
           battery.overcurrent.error || battery.overvoltage.error || battery.sop_error_flag;
}
