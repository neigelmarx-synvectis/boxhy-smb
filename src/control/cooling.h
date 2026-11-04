/**
 * @file cooling.h
 * @brief Refroidissement partie AC : trois régulateurs PI (ambiance, onduleur, DC/DC)
 *        dont la demande maximale pilote la PWM des ventilateurs, toutes les secondes.
 */
#ifndef COOLING_H
#define COOLING_H

#include <stdint.h>

typedef struct
{
    uint8_t ambiant_pwm;     /**< demande du régulateur ambiance (%) */
    uint8_t inverter_pwm;    /**< demande du régulateur onduleur (%) */
    uint8_t dcdc_pwm;        /**< demande du régulateur DC/DC pile (%) */
    uint8_t fans_duty_cycle; /**< rapport cyclique appliqué (%) */
} sCooling;

extern sCooling cooling;

void Cooling_Init(void);
/**
 * Mise à jour toutes les 1 s.
 * @param ambiant_temp  température ambiante AC (1 °C)
 * @param inverter_temp température onduleur (1 °C)
 * @param dcdc_temp     température DC/DC pile (1 °C)
 */
void Cooling_Update1s(int8_t ambiant_temp, uint8_t inverter_temp, uint8_t dcdc_temp);

#endif /* COOLING_H */
