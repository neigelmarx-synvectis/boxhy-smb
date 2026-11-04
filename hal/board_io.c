/**
 * @file hal/board_io.c
 * @brief Initialisation PWM / DAC et commande du rapport cyclique ventilateurs (cible dsPIC33EV).
 *
 * Seules ces deux fonctions ne sont pas inline (voir board_io.h) : elles écrivent plusieurs
 * registres et n'ont pas d'équivalent MCC. Fichier compilé uniquement pour la cible ; les tests
 * sur PC utilisent tests/mocks/hal/mocks.c à la place.
 */
#include <xc.h>
#include "hal/board_io.h"

/* Appelée une fois par main() après SYSTEM_Initialize(), avant le démarrage des timers. */
void Board_Init(void)
{
    /* Référence de tension (DAC) non utilisée : circuit coupé */
    CVR1CONbits.CVREN = 0;

    /* Module PWM : 3 générateurs push-pull, période 1000, rapport cyclique 0.
     * Seul le générateur 3 est utilisé (ventilateurs AC) ; 1 et 2 restent à 0 % (électrovanne
     * réservoir et seconde voie ventilateurs, non pilotées par ce firmware). */
    PTCON = 0x0000;                       /* Disable PWM module */
    PTPER = 1000;                         /* Set PWM period */
    PHASE1 = PHASE2 = PHASE3 = 0;         /* No phase shift */
    PDC1 = PDC2 = PDC3 = 0;               /* Set duty cycle to 0 */
    DTR1 = DTR2 = DTR3 = 0;               /* No dead time */
    ALTDTR1 = ALTDTR2 = ALTDTR3 = 0;      /* No dead time */
    IOCON1 = IOCON2 = IOCON3 = 0x8800;    /* Set PWM Mode to Push-Pull */
    PWMCON1 = PWMCON2 = PWMCON3 = 0x0000; /* Primary Time Base, Edge-Aligned, Independent Duty Cycles */
    FCLCON1 = FCLCON2 = FCLCON3 = 0x0003; /* Fault current limit disabled */
    PTCON2 = 0x0001;                      /* 1:1 prescaler (24MHz clock) */
    PTCON = 0x8000;                       /* Enable PWM module */
}

/* Rapport cyclique en % : PDC3 va de 0 à PTPER (1000), soit 10 pas par %.
 * Appelée par cooling.c toutes les secondes et par le mode maintenance (commande manuelle). */
void Board_SetFansDutyCycle(uint8_t duty_cycle)
{
    if (duty_cycle > 100)
        duty_cycle = 100;
    PDC3 = (uint16_t)duty_cycle * 10; /* générateur 3 = ventilateurs AC (ex-PWM2_SetDutyCycle) */
}
