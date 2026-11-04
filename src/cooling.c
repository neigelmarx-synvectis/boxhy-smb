/**
 * @file cooling.c
 * @brief Refroidissement partie AC (voir cooling.h).
 */
#include "cooling.h"
#include "generic.h"
#include "smb_config.h"
#include "hal/board_io.h"

sCooling cooling;

static float ambiant_int_part;
static float inverter_int_part;
static float dcdc_int_part;

void Cooling_Init(void)
{
    cooling.ambiant_pwm = 0;
    cooling.inverter_pwm = 0;
    cooling.dcdc_pwm = 0;
    cooling.fans_duty_cycle = 0;
    ambiant_int_part = 0;
    inverter_int_part = 0;
    dcdc_int_part = 0;
}

void Cooling_Update1s(int8_t ambiant_temp, uint8_t inverter_temp, uint8_t dcdc_temp)
{
    cooling.ambiant_pwm = Generic_PidController(COOLING_PID_KP, COOLING_PID_KI, COOLING_PID_KD,
                                                &ambiant_int_part, COOLING_AMBIANT_TARGET, (float)ambiant_temp);
    cooling.inverter_pwm = Generic_PidController(COOLING_PID_KP, COOLING_PID_KI, COOLING_PID_KD,
                                                 &inverter_int_part, COOLING_INVERTER_TARGET, (float)inverter_temp);
    cooling.dcdc_pwm = Generic_PidController(COOLING_PID_KP, COOLING_PID_KI, COOLING_PID_KD,
                                             &dcdc_int_part, COOLING_DCDC_TARGET, (float)dcdc_temp);

    /* Les ventilateurs suivent la demande la plus forte */
    cooling.fans_duty_cycle = (uint8_t)Generic_Saturation(
        COOLING_FANS_MIN_DUTY,
        Generic_MaxF(cooling.ambiant_pwm, Generic_MaxF(cooling.inverter_pwm, cooling.dcdc_pwm)),
        COOLING_FANS_MAX_DUTY);
    Board_SetFansDutyCycle(cooling.fans_duty_cycle);
}
