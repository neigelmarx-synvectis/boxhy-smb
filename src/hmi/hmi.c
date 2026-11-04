/**
 * @file hmi.c
 * @brief Interface homme-machine (voir hmi.h).
 *
 * Toutes les temporisations sont en ticks de 100 ms, sauf la limite de 5 min du buzzer, comptée
 * en secondes. Les seuils sont dans src/smb_config.h.
 */
#include <string.h>
#include "src/hmi/hmi.h"
#include "src/smb_config.h"
#include "src/devices/battery.h"
#include "src/devices/inverter.h"
#include "src/devices/fcs.h"
#include "src/system/smb_state_machine.h"
#include "hal/board_io.h"

/* État public (déclaré extern dans hmi.h). */
sHmi hmi;

/* Position dans la période du motif sonore en cours (ticks de 100 ms, voir BeepPattern_Update). */
static uint8_t buzzer_period_cpt;

/* Remet à zéro l'état IHM et le motif sonore. */
void Hmi_Init(void)
{
    memset(&hmi, 0, sizeof(hmi));
    buzzer_period_cpt = 0;
}

/*
 * Lecture des boutons toutes les 100 ms :
 *  - mode batterie / mode hybride : l'état bascule après 5 lectures consécutives différentes
 *    (BUTTON_DEBOUNCE_THRESHOLD = 4, soit 500 ms) ;
 *  - bouton central : armé après plus de 50 lectures appuyées (5,1 s) ; il reste armé jusqu'à
 *    son acquittement par la machine d'états.
 */
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

/*
 * Fait avancer le double bip (buzzer actif aux ticks 10-11 et 14-15 de la période) et commande
 * le buzzer seulement aux changements, pour ne pas écraser un état fixé ailleurs.
 * @param period durée de la période en ticks de 100 ms (0xFFFF : pas de remise à zéro, le
 *               compteur 8 bits reboucle alors tous les 256 ticks, soit 25,6 s)
 */
static void apply_beep(uint16_t period)
{
    int8_t cmd = BeepPattern_Update(&buzzer_period_cpt, period);
    if (cmd != BEEP_UNCHANGED)
        Board_SetBuzzer(cmd == BEEP_ON);
}

/*
 * Avertissements sonores en mode batterie / hybride, par ordre de priorité :
 *  1. SOC bas en décharge : période (SOC - 10) x 1 s + 2 s, soit de 16 s à 24 % jusqu'à 2 s à 10 % ;
 *  2. SOC haut : période (95 - SOC) x 3 s + 2 s ; l'avertissement n'étant levé qu'au-dessus de
 *     95 % (bug 2), c'est en pratique la période de 25,6 s qui s'applique ;
 *  3. puissance de sortie haute ou SOP bas : période de 1,9 s.
 * Sans avertissement, le buzzer est coupé. Hors de ces états, il n'est pas touché ici.
 */
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

/*
 * Limite du buzzer en mode défaut : après BUZZER_5_MINUTES_TIMEOUT secondes (300 s) passées en
 * SMB_EMERGENCY, buzzer_5_minutes.error est levé et la machine d'états coupe le buzzer.
 */
void Hmi_Update1s(uint8_t smb_step)
{
    TimedFlag16_Update(&hmi.buzzer_5_minutes, smb_step == SMB_EMERGENCY, BUZZER_5_MINUTES_TIMEOUT);
}
