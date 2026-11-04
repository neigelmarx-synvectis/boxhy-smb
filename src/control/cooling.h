/**
 * @file cooling.h
 * @brief Refroidissement partie AC : trois régulateurs PI (ambiance, onduleur, DC/DC)
 *        dont la demande maximale pilote la PWM des ventilateurs, toutes les secondes.
 *
 * Les mêmes ventilateurs refroidissent le compartiment AC, l'onduleur et le convertisseur DC/DC
 * de la pile. Chaque température a son propre régulateur PI et sa consigne (40, 55 et 45 °C,
 * voir src/smb_config.h) ; les ventilateurs suivent la demande la plus forte, avec un minimum
 * de 15 % pour assurer une ventilation permanente.
 *
 * Appel : Cooling_Update1s() dans la tâche 1 s de main.c. Les demandes sont publiées en
 * télémétrie (trame SMB_FAN_CONTROL).
 */
#ifndef COOLING_H
#define COOLING_H

#include <stdint.h>

/** Demandes des régulateurs et rapport cyclique appliqué. */
typedef struct
{
    uint8_t ambiant_pwm;     /**< demande du régulateur ambiance (%) */
    uint8_t inverter_pwm;    /**< demande du régulateur onduleur (%) */
    uint8_t dcdc_pwm;        /**< demande du régulateur DC/DC pile (%) */
    uint8_t fans_duty_cycle; /**< rapport cyclique appliqué (%) */
} sCooling;

/** État du refroidissement, lu par la télémétrie. */
extern sCooling cooling;

/** Remet à zéro les demandes et les termes intégraux des régulateurs. */
void Cooling_Init(void);
/**
 * Mise à jour toutes les 1 s.
 * @param ambiant_temp  température ambiante AC (1 °C)
 * @param inverter_temp température onduleur (1 °C)
 * @param dcdc_temp     température DC/DC pile (1 °C)
 */
void Cooling_Update1s(int8_t ambiant_temp, uint8_t inverter_temp, uint8_t dcdc_temp);

#endif /* COOLING_H */
