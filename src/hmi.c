/**
 * @file hmi.c
 * @brief Interface homme-machine (voir hmi.h).
 */
#include <string.h>
#include "hmi.h"
#include "smb_config.h"
#include "battery.h"
#include "inverter.h"
#include "fcs.h"
#include "smb_state_machine.h"
#include "hal/board_io.h"

sHmi hmi;

static uint8_t buzzer_period_cpt;

void Hmi_Init(void)
{
    memset(&hmi, 0, sizeof(hmi));
    buzzer_period_cpt = 0;
}

void Hmi_UpdateButtons100ms(void)
{
    Debounce_Update(&hmi.battery_mode_button, Board_GetButtonBatteryMode(), BUTTON_DEBOUNCE_THRESHOLD);

    if (Debounce_Update(&hmi.hybrid_mode_button, Board_GetButtonHybridMode(), BUTTON_DEBOUNCE_THRESHOLD) &&
        hmi.hybrid_mode_button.state)
    {
        /* Appui sur le bouton hybride : acquittement des erreurs pile */
        if (fcs.general_error_flag)
            Fcs_ResetErrors();
    }

    HoldDetect_Update(&hmi.middle_button, Board_GetButtonMiddle(), BUTTON_MIDDLE_HOLD_THRESHOLD);
}

static void apply_beep(uint16_t period)
{
    int8_t cmd = BeepPattern_Update(&buzzer_period_cpt, period);
    if (cmd != BEEP_UNCHANGED)
        Board_SetBuzzer(cmd == BEEP_ON);
}

void Hmi_UpdateBuzzer100ms(uint8_t smb_step)
{
    if ((smb_step >= SMB_BATTERY_MODE) && (smb_step < SMB_FC_STOP_1))
    {
        if (battery.soc_low_level_warning_flag && battery.output_current > 0)
        {
            /* Période d'autant plus courte que le SOC approche du niveau très bas ;
             * sous ce niveau, pas de remise à zéro explicite (compteur 8 bits). */
            apply_beep((battery.soc >= BATTERY_SOC_VERY_LOW_LEVEL)
                           ? (uint16_t)(((battery.soc - BATTERY_SOC_VERY_LOW_LEVEL) * 10) + 20)
                           : 0xFFFF);
        }
        else if (battery.soc_high_level_warning_flag)
        {
            apply_beep((battery.soc <= BATTERY_SOC_VERY_VERY_HIGH_LEVEL)
                           ? (uint16_t)(((BATTERY_SOC_VERY_VERY_HIGH_LEVEL - battery.soc) * 30) + 20)
                           : 0xFFFF);
        }
        else if (inverter.apparent_power_high_warning_flag || battery.sop_warning_flag)
        {
            apply_beep(19);
        }
        else
            Board_SetBuzzer(false);
    }
}

void Hmi_Update1s(uint8_t smb_step)
{
    TimedFlag16_Update(&hmi.buzzer_5_minutes, smb_step == SMB_EMERGENCY, BUZZER_5_MINUTES_TIMEOUT);
}
