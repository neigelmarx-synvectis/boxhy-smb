/**
 * @file battery.c
 * @brief Batteries IBS (voir battery.h).
 */
#include <string.h>
#include "src/devices/battery.h"
#include "src/can/can_db.h"
#include "src/smb_config.h"
#include "hal/can_port.h"

#define IBS_INFO_ID_MASK      0xFFFF00FEUL /* 0x18FF xx D0/D1 : xx = index info, bit 0 = batterie */
#define IBS_INFO_ID_BASE      0x18FF00D0UL

sBatteryPack battery;

/* Compteurs des supervisions SOC (non remis à zéro à l'acquittement, comme à l'origine) */
static uint8_t soc_very_low_level_error_cpt;
/* TODO(bug 2) : compteur déclaré bool à l'origine : "cpt++" vaut toujours 1, le timeout n'est
 * jamais atteint et l'erreur SOC "très très haut" ne se déclenche jamais. Reproduit ci-dessous. */
static uint8_t soc_very_very_high_level_error_cpt;

/* ---- Trames IBS (même disposition pour les deux batteries, n = 0 ou 1) ------------------- */
/* Unités firmware : courants 0,01 A ; températures 1 °C ; SOC et SOP 1 %. */
#define IBS_RX_TABLES(n)                                                                                       \
    static const sCanBinding ibs##n##_current[] CAN_ROM = {                                                    \
        CAN_BIND(CANDB_IBS_CURRENT_##n##__CURRENT, &battery.ibs[n].output_current, CAN_VAR_I16, 0.01),         \
        CAN_BIND(CANDB_IBS_CURRENT_##n##__SOP, &battery.ibs[n].sop, CAN_VAR_U8, 1),                            \
    };                                                                                                         \
    CAN_CHECK_BINDINGS(CANDB_IBS_CURRENT_##n, ibs##n##_current);                                               \
    static const sCanBinding ibs##n##_cell_temperature[] CAN_ROM = {                                           \
        CAN_BIND_NONE(CANDB_IBS_CELL_TEMPERATURE_##n##__MIN_CELL_TEMPERATURE),                                 \
        CAN_BIND(CANDB_IBS_CELL_TEMPERATURE_##n##__MAX_CELL_TEMPERATURE, &battery.ibs[n].max_cell_temperature, CAN_VAR_I8, 1), \
        CAN_BIND(CANDB_IBS_CELL_TEMPERATURE_##n##__AVG_CELL_TEMPERATURE, &battery.ibs[n].avg_cell_temperature, CAN_VAR_I8, 1), \
    };                                                                                                         \
    CAN_CHECK_BINDINGS(CANDB_IBS_CELL_TEMPERATURE_##n, ibs##n##_cell_temperature);                             \
    static const sCanBinding ibs##n##_board_temperature[] CAN_ROM = {                                          \
        CAN_BIND_NONE(CANDB_IBS_BOARD_TEMPERATURE_##n##__MIN_BOARD_TEMPERATURE),                               \
        CAN_BIND(CANDB_IBS_BOARD_TEMPERATURE_##n##__MAX_BOARD_TEMPERATURE, &battery.ibs[n].max_board_temperature, CAN_VAR_I8, 1), \
        CAN_BIND_NONE(CANDB_IBS_BOARD_TEMPERATURE_##n##__AVG_BOARD_TEMPERATURE),                               \
    };                                                                                                         \
    CAN_CHECK_BINDINGS(CANDB_IBS_BOARD_TEMPERATURE_##n, ibs##n##_board_temperature);                           \
    static const sCanBinding ibs##n##_state[] CAN_ROM = {                                                      \
        CAN_BIND(CANDB_IBS_STATE_##n##__SOC, &battery.ibs[n].soc, CAN_VAR_U8, 1),                              \
        CAN_BIND_NONE(CANDB_IBS_STATE_##n##__SOH),                                                             \
    };                                                                                                         \
    CAN_CHECK_BINDINGS(CANDB_IBS_STATE_##n, ibs##n##_state);                                                   \
    static const sCanBinding ibs##n##_info[] CAN_ROM = {                                                       \
        CAN_BIND(CANDB_IBS_INFO_##n##__SYSTEM_RUNNING_FLAG, &battery.ibs[n].system_running_flag, CAN_VAR_BOOL, 1), \
        CAN_BIND(CANDB_IBS_INFO_##n##__BALANCING_FLAG, &battery.ibs[n].balancing_flag, CAN_VAR_BOOL, 1),       \
        CAN_BIND(CANDB_IBS_INFO_##n##__ERROR_FLAG, &battery.ibs[n].error_flag, CAN_VAR_BOOL, 1),               \
        CAN_BIND(CANDB_IBS_INFO_##n##__LOW_SOC_WARNING_FLAG, &battery.ibs[n].low_soc_warning_flag, CAN_VAR_BOOL, 1), \
    };                                                                                                         \
    CAN_CHECK_BINDINGS(CANDB_IBS_INFO_##n, ibs##n##_info);                                                     \
    static const sCanBinding ibs##n##_current_limits[] CAN_ROM = {                                             \
        CAN_BIND(CANDB_IBS_CURRENT_LIMITS_##n##__MAX_DISCHARGE_CURRENT, &battery.ibs[n].max_discharge_current, CAN_VAR_I16, 0.01), \
        CAN_BIND(CANDB_IBS_CURRENT_LIMITS_##n##__MAX_CHARGE_CURRENT, &battery.ibs[n].max_charge_current, CAN_VAR_I16, 0.01), \
    };                                                                                                         \
    CAN_CHECK_BINDINGS(CANDB_IBS_CURRENT_LIMITS_##n, ibs##n##_current_limits);                                 \
    static const sCanBinding ibs##n##_keep_alive[] CAN_ROM = {                                                 \
        CAN_BIND_CONST(CANDB_IBS_RX_KEEP_ALIVE_##n##__KEEP_ALIVE, 0),                                          \
    };                                                                                                         \
    CAN_CHECK_BINDINGS(CANDB_IBS_RX_KEEP_ALIVE_##n, ibs##n##_keep_alive)

#define IBS_RX_MESSAGES(n)                                                                                     \
    CAN_MESSAGE(CANDB_IBS_CURRENT_##n, ibs##n##_current),                                                      \
    CAN_MESSAGE(CANDB_IBS_CELL_TEMPERATURE_##n, ibs##n##_cell_temperature),                                    \
    CAN_MESSAGE(CANDB_IBS_BOARD_TEMPERATURE_##n, ibs##n##_board_temperature),                                  \
    CAN_MESSAGE(CANDB_IBS_STATE_##n, ibs##n##_state),                                                          \
    CAN_MESSAGE(CANDB_IBS_INFO_##n, ibs##n##_info),                                                            \
    CAN_MESSAGE(CANDB_IBS_CURRENT_LIMITS_##n, ibs##n##_current_limits)

IBS_RX_TABLES(0);
IBS_RX_TABLES(1);

static const sCanMessage ibs_rx_messages[] CAN_ROM = {
    IBS_RX_MESSAGES(0),
    IBS_RX_MESSAGES(1),
};

static const sCanMessage ibs_keep_alive[BATTERY_NB] CAN_ROM = {
    CAN_MESSAGE(CANDB_IBS_RX_KEEP_ALIVE_0, ibs0_keep_alive),
    CAN_MESSAGE(CANDB_IBS_RX_KEEP_ALIVE_1, ibs1_keep_alive),
};

void Battery_Init(void)
{
    memset(&battery, 0, sizeof(battery));
    soc_very_low_level_error_cpt = 0;
    soc_very_very_high_level_error_cpt = 0;
}

bool Battery_HandleCanFrame(const CAN_MSG_OBJ *msg)
{
    const sCanMessage *m;

    if ((msg->msgId & IBS_INFO_ID_MASK) != IBS_INFO_ID_BASE)
        return false;

    m = CanMsg_Find(ibs_rx_messages, CAN_ARRAY_SIZE(ibs_rx_messages), msg->msgId);
    if (m != NULL)
    {
        CanMsg_Unpack(m, msg->data);
        if ((m->id == CANDB_IBS_CURRENT_0_ID) || (m->id == CANDB_IBS_CURRENT_1_ID))
            battery.ibs[msg->msgId & 0x01].watchdog.cnt = 0; /* la trame courant fait office de présence */
    }
    /* autres identifiants IBS (0x3C...) : sans effet, mais consommés */
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
    CanMsg_Send(&ibs_keep_alive[battery.keep_alive_index], ibs_keep_alive[battery.keep_alive_index].dlc,
                CAN_PRIORITY_HIGH);

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
