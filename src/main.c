/**
 * @brief Firmware SMB BOXHY (System Management Board) - point d'entrée et ordonnanceur.
 * @file main.c
 *
 * @author Julian VERNIER, Neigel MARX, Gael GRZELAK
 * @version 2.0.0
 *
 * Rôle : initialiser le matériel et les modules, brancher les routines d'interruption, puis
 * exécuter indéfiniment la boucle principale (ordonnanceur coopératif sans RTOS).
 *
 * La boucle principale enchaîne, à chaque tour : la machine d'états, puis une seule des tâches
 * "événement" (réception CAN, conversion ADC, moyennes ADC), puis une seule des tâches
 * "périodiques" (100 ms, 300 ms, 1 s, trame RS232 reçue, émission CAN), dans cet ordre de
 * priorité. Les ISR ne font que lever des drapeaux ou assembler des octets.
 *
 * Cadencement (voir src/system/timebase.c) : TMR1 interrompt toutes les 10 ms et lève les
 * drapeaux 100 ms, 300 ms et 1 s ainsi que l'autorisation d'émettre une trame CAN. Une tâche
 * périodique n'est donc pas exécutée à l'instant exact de son drapeau mais au premier tour de
 * boucle où aucune tâche plus prioritaire n'est en attente ; le drapeau est acquitté ici.
 */
#include "mcc_generated_files/system.h"
#include "mcc_generated_files/tmr1.h"
#include "mcc_generated_files/tmr5.h"
#include "mcc_generated_files/adc1.h"
#include "mcc_generated_files/uart1.h"

#include "hal/board_io.h"
#include "hal/can_port.h"
#include "src/system/timebase.h"
#include "src/measures/measures.h"
#include "src/devices/battery.h"
#include "src/devices/fcs.h"
#include "src/control/ems.h"
#include "src/devices/inverter.h"
#include "src/hmi/hmi.h"
#include "src/control/cooling.h"
#include "src/hmi/maintenance.h"
#include "src/can/can_rx.h"
#include "src/can/can_tx.h"
#include "src/system/smb_state_machine.h"

/*
 * Tâches exécutées toutes les 100 ms.
 * L'ordre reproduit celui du firmware d'origine et a des conséquences :
 *  - les supervisions batterie utilisent le SOC et le courant agrégés au tour précédent
 *    (Battery_Aggregate100ms est appelée après Battery_Supervise100ms) ;
 *  - la supervision onduleur utilise la moyenne de puissance du cycle précédent
 *    (Inverter_UpdatePowerAverage100ms est appelée en dernier) ;
 *  - le buzzer utilise le courant batterie fraîchement agrégé.
 */
static void tasks_100ms(void)
{
    Measures_Check100ms();                                       /* seuils des sondes CTN */
    Battery_Supervise100ms(smb.step, measures.bus_voltage);      /* SOC, surintensité, surchauffe... */
    Inverter_Supervise100ms(battery.soc);                        /* déconnexion SOC bas, surcharge */
    Fcs_Update100ms(smb.step == SMB_FCS_START_2);                /* temporisations pile */
    Hmi_UpdateButtons100ms();                                    /* anti-rebond des boutons */
    Battery_Aggregate100ms();                                    /* SOC moyen, courant total */
    Ems_Update100ms(battery.output_current, fcs.output_current); /* moyennes de courant sur 1 s */
    Hmi_UpdateBuzzer100ms(smb.step);                             /* motifs sonores d'avertissement */
    Inverter_UpdatePowerAverage100ms();                          /* moyenne de puissance AC */
}

/*
 * Tâches exécutées toutes les secondes : keep-alive et watchdogs de communication,
 * régulation des ventilateurs, consigne de courant pile, compteur 5 min du buzzer.
 */
static void tasks_1s(void)
{
    Battery_Update1s();
    Cooling_Update1s(measures.ac_ambiant_temp.value_8, inverter.dcac_temp, fcs.dcdc_temperature);
    Ems_Update1s(smb.step == SMB_HYBRID_MODE, fcs.short_circuit_ongoing_flag,
                 battery.ibs[0].max_charge_current, battery.ibs[1].max_charge_current);
    Fcs_Update1s();
    Inverter_Update1s(smb.step);
    Hmi_Update1s(smb.step);
}

int main(void)
{
    /* Horloge, broches, périphériques (configuration générée par MCC) */
    SYSTEM_Initialize();

    /* Routines d'interruption : de simples fonctions applicatives enregistrées auprès de MCC */
    TMR1_SetInterruptHandler(&Timebase_OnTick);               /* toutes les 10 ms */
    ADC1_SetInterruptHandler(&Measures_OnConversionDone);     /* fin de conversion ADC */
    UART1_SetRxInterruptHandler(&Inverter_OnRxInterrupt);     /* octet reçu de l'onduleur */

    /* Initialisation des modules (avant le démarrage des timers : aucune ISR TMR1 avant la fin) */
    Board_Init();
    Measures_Init();
    CanPort_Init();
    CanTx_Init();
    CanRx_Init();
    Fcs_Init();
    Battery_Init();
    Inverter_Init();
    Hmi_Init();
    Cooling_Init();
    Ems_Init();
    Maintenance_Init();
    Timebase_Init();
    Smb_Init();                 /* machine d'états en SMB_INIT, chronomètre à zéro */

    TMR1_Start();               /* base de temps 10 ms */
    TMR5_Start();               /* déclenche les conversions ADC toutes les 100 µs (SSRC = TMR5) */

    for (;;)
    {
        /* 1. Machine d'états : évaluée à chaque tour, sans attente */
        Smb_Run();

        /* 2. Une seule tâche "événement" par tour, par ordre de priorité */
        if (CanPort_ReceivedCount() > 0)
            CanRx_Process();                /* une trame CAN reçue */
        else if (measures.conversion_done)
            Measures_ProcessConversion();   /* résultat ADC disponible -> voie suivante */
        else if (measures.samples_ready)
            Measures_Convert();             /* 50 échantillons par voie -> grandeurs physiques */

        /* 3. Une seule tâche "périodique" par tour, par ordre de priorité */
        if (timebase.flag_100ms)
        {
            tasks_100ms();
            timebase.flag_100ms = false;
        }
        else if (timebase.flag_300ms)
        {
            Inverter_Poll300ms(smb.step);   /* requête RS232 suivante vers l'onduleur */
            timebase.flag_300ms = false;
        }
        else if (timebase.flag_1s)
        {
            tasks_1s();
            timebase.flag_1s = false;
        }
        else if (inverter.frame_received)
            Inverter_ProcessFrame();        /* décodage d'une réponse onduleur complète */
        else if (timebase.can_ready && (smb.step > SMB_INIT))
        {
            /* Une trame de télémétrie par période TMR1 (10 ms), pas pendant l'initialisation */
            CanTx_SendNext();
            timebase.can_ready = false;
        }
    }

    return 0; /* jamais atteint */
}
