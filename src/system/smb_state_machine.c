/**
 * @file smb_state_machine.c
 * @brief Machine d'états principale (voir smb_state_machine.h).
 *
 * Chaque état est une fonction. Le chronomètre d'état est timebase.ticks_100ms (pas de 100 ms),
 * remis à zéro à chaque transition par go_to().
 */
#include "src/system/smb_state_machine.h"
#include "src/smb_config.h"
#include "src/system/timebase.h"
#include "src/devices/battery.h"
#include "src/devices/fcs.h"
#include "src/devices/inverter.h"
#include "src/measures/measures.h"
#include "src/hmi/hmi.h"
#include "src/hmi/maintenance.h"
#include "hal/board_io.h"

/* Temporisations des états (ticks de 100 ms) */
#define INIT_DURATION            20   /* 2 s */
#define AUTOCHECK_DURATION       30   /* 3 s */
#define DCAC_PRECHARGE_DURATION  30   /* 3 s */
#define DCAC_SWITCH_DURATION     5    /* 0.5 s */
#define DCAC_MODE_TIMEOUT        150  /* 15 s : attente du mode onduleur */
#define FCS_START_1_TIMEOUT      30   /* 3 s : attente FCCB en standby */
#define FC_STOP_1_DURATION       2    /* 200 ms */
#define DCAC_STOP_DURATION       15   /* 1.5 s */
#define STOPPING_DURATION        50   /* 5 s */

sSmb smb;

static uint16_t step_100ms_cnt; /* sous-compteur de l'état STOPPING */

void Smb_Init(void)
{
    smb.step = SMB_INIT;
    smb.critical_error_flag = false;
    smb.software_error_flag = false;
    smb.reset_order_flag = false;
    step_100ms_cnt = 0;
    timebase.ticks_100ms = 0;
}

void Smb_SetResetOrder(bool order)
{
    smb.reset_order_flag = order;
}

/* ---- Utilitaires ------------------------------------------------------------------- */

static void go_to(uint8_t step)
{
    smb.step = step;
    timebase.ticks_100ms = 0;
}

/* Ouvre l'onduleur si la protection SOC bas est active (ne le referme pas ici). */
static void apply_soc_disconnect(void)
{
    if (inverter.soc_disconnect_error_flag)
        Board_SetDcacInputRelay(false);
}

/* Ouvre / ferme l'onduleur selon la protection SOC bas. */
static void apply_soc_disconnect_both_ways(void)
{
    Board_SetDcacInputRelay(!inverter.soc_disconnect_error_flag);
}

static void update_error_flags(void)
{
    if (smb.software_error_flag || Battery_HasCriticalError() || fcs.ppmh2_error_flag)
        smb.critical_error_flag = 1;

    Fcs_UpdateGeneralError();
    Inverter_UpdateGeneralError(measures.ac_ambiant_temp.error_flag);
}

/* Acquittement de toutes les erreurs (sortie du mode défaut). */
static void reset_all_errors(void)
{
    Fcs_ResetAllErrors();
    hmi.buzzer_5_minutes.error = 0;
    smb.software_error_flag = 0;
    Battery_ResetErrors();
    smb.critical_error_flag = 0;
    Inverter_ResetErrors();
    Measures_ResetSensorErrors();
    hmi.raz_erreur_ihm_flag = 1;
}

/* ---- États ------------------------------------------------------------------------- */

static void state_init(void)
{
    Board_RelaysSafeState();
    if (timebase.ticks_100ms >= INIT_DURATION)
        go_to(SMB_AUTOCHECK);
}

static void state_autocheck(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms >= AUTOCHECK_DURATION)
        go_to(SMB_STANDBY);
}

static void state_standby(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        smb.step = SMB_EMERGENCY;
    else if (hmi.battery_mode_button.state && !inverter.general_error_flag)
        smb.step = SMB_DCAC_START_1;
    else if (maintenance.mode_request_flag)
    {
        smb.step = SMB_MAINTENANCE;
        Fcs_SendMaintenanceMode();
    }
    timebase.ticks_100ms = 0;
}

static void state_maintenance(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        smb.step = SMB_EMERGENCY;
    else if (!maintenance.mode_request_flag)
        smb.step = SMB_STOPPING;
    timebase.ticks_100ms = 0;
}

static void state_dcac_start_1(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms >= DCAC_PRECHARGE_DURATION)
        go_to(SMB_DCAC_START_2);
    Board_SetDcacPrechargeRelay(true);
}

static void state_dcac_start_2(void)
{
    if (smb.critical_error_flag || inverter.general_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms >= DCAC_SWITCH_DURATION)
    {
        Board_SetDcacPrechargeRelay(false);
        Board_SetDcacInputRelay(true);
        go_to(SMB_DCAC_START_3);
    }
}

static void state_dcac_start_3(void)
{
    if (smb.critical_error_flag || (inverter.mode == INVERTER_FAULT_MODE))
        go_to(SMB_EMERGENCY);
    else if ((inverter.mode > INVERTER_STANDBY_MODE) || (timebase.ticks_100ms >= DCAC_MODE_TIMEOUT))
        go_to(SMB_BATTERY_MODE);
}

static void state_battery_mode(void)
{
    apply_soc_disconnect_both_ways();

    if (smb.critical_error_flag || inverter.general_error_flag || inverter.apparent_power_very_high_error_flag)
        go_to(SMB_EMERGENCY);
    else if (!hmi.battery_mode_button.state && !hmi.hybrid_mode_button.state)
        go_to(SMB_STOPPING);
    else if (hmi.hybrid_mode_button.state && !fcs.general_error_flag && (battery.soc <= BATTERY_SOC_HIGH_LEVEL))
        go_to(SMB_FCS_START_1);
    else if (hmi.hybrid_mode_button.state && fcs.general_error_flag)
        go_to(SMB_EMERGENCY);
    /* (origine) une dernière branche vers SMB_DCAC_STOP reprenait la condition de SMB_STOPPING : inatteignable */

    Board_SetFcsOutputRelay(false);
    timebase.ticks_100ms = 0;
}

static void state_fcs_start_1(void)
{
    apply_soc_disconnect();
    if (smb.critical_error_flag)
        go_to(SMB_EMERGENCY);
    else if (fcs.step == FCCB_STANDBY)
    {
        Fcs_SendStart();
        Board_SetFcsOutputRelay(true);
        go_to(SMB_FCS_START_2);
    }
    else if ((timebase.ticks_100ms >= FCS_START_1_TIMEOUT) || (fcs.step == FCCB_EMERGENCY) ||
             fcs.general_error_flag || inverter.general_error_flag)
    {
        Board_SetFcsOutputRelay(false);
        fcs.startup_error_flag = 1;
        Fcs_SendStop();
        go_to(SMB_FC_STOP_1);
    }
}

static void state_fcs_start_2(void)
{
    /* TODO(bug 7) : tant que la protection SOC est active, les autres transitions ne sont pas évaluées */
    if (inverter.soc_disconnect_error_flag)
        Board_SetDcacInputRelay(false);
    else if (smb.critical_error_flag)
        go_to(SMB_EMERGENCY);
    else if ((fcs.step == FCCB_EMERGENCY) || fcs.general_error_flag || inverter.general_error_flag)
    {
        Board_SetFcsOutputRelay(false);
        fcs.startup_error_flag = 1;
        go_to(SMB_FC_STOP_1);
    }
    else if (fcs.step == FCCB_OPERATING)
        go_to(SMB_HYBRID_MODE);
    else if ((fcs.start_2_timeout.error == 1) && (fcs.startup_retry_flag == 0))
    {
        fcs.startup_retry_flag = 1;
        fcs.start_2_timeout.error = 0;
        go_to(SMB_FCS_START_1);
    }
    else if (fcs.startup_retry_flag == 1)
        go_to(SMB_EMERGENCY);
}

static void state_hybrid_mode(void)
{
    apply_soc_disconnect_both_ways();
    if (smb.critical_error_flag)
        go_to(SMB_EMERGENCY);
    else if (!hmi.hybrid_mode_button.state ||
             battery.soc >= BATTERY_SOC_VERY_HIGH_LEVEL ||
             (fcs.step != FCCB_OPERATING) ||
             fcs.general_error_flag ||
             inverter.general_error_flag ||
             fcs.low_current_timeout.error ||
             inverter.apparent_power_very_high_error_flag)
    {
        fcs.low_current_timeout.error = 0;
        go_to(SMB_FC_STOP_1);
    }
}

static void state_fc_stop_1(void)
{
    /* TODO(bug 7) : idem SMB_FCS_START_2 */
    if (inverter.soc_disconnect_error_flag)
        Board_SetDcacInputRelay(false);
    else if (smb.critical_error_flag || fcs.general_error_flag || inverter.general_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms >= FC_STOP_1_DURATION)
    {
        Fcs_SendStop();
        go_to(SMB_FC_STOP_2);
    }
}

static void state_fc_stop_2(void)
{
    apply_soc_disconnect();
    Board_SetFcsOutputRelay(false);

    /* Double bip signalant l'arrêt de la pile */
    if ((timebase.ticks_100ms == 25) && (fcs.step == FCCB_SHUTDOWN))
        Board_SetBuzzer(true);
    else if (timebase.ticks_100ms == 30)
        Board_SetBuzzer(false);
    else if ((timebase.ticks_100ms == 35) && (fcs.step == FCCB_SHUTDOWN))
        Board_SetBuzzer(true);
    else if (timebase.ticks_100ms == 40)
        Board_SetBuzzer(false);
    else
        Board_SetBuzzer(false);

    if (smb.critical_error_flag || fcs.step == FCCB_EMERGENCY)
        go_to(SMB_EMERGENCY);
    else if (fcs.step == FCCB_STANDBY)
        go_to(SMB_BATTERY_MODE);
}

static void state_dcac_stop(void)
{
    Board_SetDcacInputRelay(false);

    if (smb.critical_error_flag)
        go_to(SMB_EMERGENCY);
    else if (timebase.ticks_100ms == DCAC_STOP_DURATION)
    {
        Inverter_ClearDataAfterStop();
        go_to(SMB_STANDBY);
    }
}

static void state_stopping(void)
{
    if (smb.critical_error_flag)
    {
        smb.step = SMB_EMERGENCY;
        Fcs_SendCriticalStop();
        timebase.ticks_100ms = 0;
        step_100ms_cnt = 0;
    }
    else if (timebase.ticks_100ms > 0)
    {
        Board_SetDcacPrechargeRelay(false);

        step_100ms_cnt++;
        if (step_100ms_cnt == 1) /* après 100 ms */
        {
            /* TODO(bug 1) : l'origine écrit "fcs_step = FCCB_OPERATING" (affectation) dans la
             * condition : toujours vraie, et l'état FCCB est écrasé jusqu'à la prochaine trame. */
            fcs.step = FCCB_OPERATING;
            Fcs_SendStop();
        }
        else if (step_100ms_cnt == 2) /* après 200 ms */
            Board_SetFcsOutputRelay(false);
        else if (step_100ms_cnt == 15) /* après 1.5 s */
            Board_SetDcacInputRelay(false);
        else if (step_100ms_cnt >= STOPPING_DURATION)
        {
            Board_SetDcacPrechargeRelay(false);
            Board_SetFcsOutputRelay(false);
            Board_SetDcacInputRelay(false);
            Board_SetBuzzer(false);
            Inverter_ClearDataAfterStop();
            smb.step = SMB_STANDBY;
            step_100ms_cnt = 0;
        }
        timebase.ticks_100ms = 0;
    }
}

static void state_emergency(void)
{
    Board_RelaysSafeState();

    /* Buzzer : continu sur fuite H2, alterné sinon, coupé après 5 minutes */
    if (fcs.ppmh2_error_flag && !hmi.buzzer_5_minutes.error)
        Board_SetBuzzer(true);
    else if ((timebase.ticks_100ms > 2) && !fcs.ppmh2_error_flag && !hmi.buzzer_5_minutes.error)
    {
        Board_ToggleBuzzer();
        timebase.ticks_100ms = 0;
    }
    else if (hmi.buzzer_5_minutes.error)
        Board_SetBuzzer(false);

    Inverter_ClearData();

    if (hmi.middle_button.state || smb.reset_order_flag) /* sortie du mode défaut */
    {
        smb.reset_order_flag = 0;
        hmi.middle_button.state = 0;
        reset_all_errors();

        /* (origine) motif sonore évalué une seule fois sur la valeur courante du chronomètre */
        if (timebase.ticks_100ms == 8)
            Board_SetBuzzer(true);
        else if (timebase.ticks_100ms == 9)
            Board_SetBuzzer(false);
        else if (timebase.ticks_100ms == 10)
            Board_SetBuzzer(true);
        else if (timebase.ticks_100ms == 11)
        {
            Board_SetBuzzer(false);
            timebase.ticks_100ms = 0;
        }
        else
            Board_SetBuzzer(false);

        go_to(SMB_AUTOCHECK);
    }
    else if (timebase.ticks_100ms > 12)
        timebase.ticks_100ms = 0;
}

static void state_default(void)
{
    Board_RelaysSafeState();
    timebase.ticks_100ms = 0;
    smb.software_error_flag = 1;
    smb.step = SMB_EMERGENCY;
}

void Smb_Run(void)
{
    update_error_flags();

    switch (smb.step)
    {
    case SMB_INIT:          state_init(); break;
    case SMB_AUTOCHECK:     state_autocheck(); break;
    case SMB_STANDBY:       state_standby(); break;
    case SMB_MAINTENANCE:   state_maintenance(); break;
    case SMB_DCAC_START_1:  state_dcac_start_1(); break;
    case SMB_DCAC_START_2:  state_dcac_start_2(); break;
    case SMB_DCAC_START_3:  state_dcac_start_3(); break;
    case SMB_BATTERY_MODE:  state_battery_mode(); break;
    case SMB_FCS_START_1:   state_fcs_start_1(); break;
    case SMB_FCS_START_2:   state_fcs_start_2(); break;
    case SMB_HYBRID_MODE:   state_hybrid_mode(); break;
    case SMB_FC_STOP_1:     state_fc_stop_1(); break;
    case SMB_FC_STOP_2:     state_fc_stop_2(); break;
    case SMB_DCAC_STOP:     state_dcac_stop(); break;
    case SMB_STOPPING:      state_stopping(); break;
    case SMB_EMERGENCY:     state_emergency(); break;
    default:                state_default(); break;
    }
}
