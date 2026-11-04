/**
 * @brief Firmware SMB BOXHY (System Management Board) - point d'entrée et ordonnanceur.
 * @file main.c
 *
 * @author Julian VERNIER, Neigel MARX, Gael GRZELAK
 * @version 1.3.0
 *
 * La boucle principale enchaîne, à chaque tour : la machine d'états, puis une seule des tâches
 * "événement" (réception CAN, conversion ADC, moyennes ADC), puis une seule des tâches
 * "périodiques" (100 ms, 300 ms, 1 s, trame RS232 reçue, émission CAN), dans cet ordre de
 * priorité. Les ISR ne font que lever des drapeaux ou assembler des octets.
 */
#include "mcc_generated_files/system.h"
#include "mcc_generated_files/tmr1.h"
#include "mcc_generated_files/tmr5.h"
#include "mcc_generated_files/adc1.h"
#include "mcc_generated_files/uart1.h"

#include "hal/board_io.h"
#include "hal/can_port.h"
#include "timebase.h"
#include "measures.h"
#include "battery.h"
#include "fcs.h"
#include "ems.h"
#include "inverter.h"
#include "hmi.h"
#include "cooling.h"
#include "maintenance.h"
#include "can_rx.h"
#include "can_tx.h"
#include "smb_state_machine.h"

static void tasks_100ms(void)
{
    Measures_Check100ms();
    Battery_Supervise100ms(smb.step, measures.bus_voltage);
    Inverter_Supervise100ms(battery.soc);
    Fcs_Update100ms(smb.step == SMB_FCS_START_2);
    Hmi_UpdateButtons100ms();
    Battery_Aggregate100ms();
    Ems_Update100ms(battery.output_current, fcs.output_current);
    Hmi_UpdateBuzzer100ms(smb.step);
    Inverter_UpdatePowerAverage100ms();
}

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
    SYSTEM_Initialize();

    TMR1_SetInterruptHandler(&Timebase_OnTick);
    ADC1_SetInterruptHandler(&Measures_OnConversionDone);
    UART1_SetRxInterruptHandler(&Inverter_OnRxInterrupt);

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
    Smb_Init();

    TMR1_Start();
    TMR5_Start();

    for (;;)
    {
        Smb_Run();

        if (CanPort_ReceivedCount() > 0)
            CanRx_Process();
        else if (measures.conversion_done)
            Measures_ProcessConversion();
        else if (measures.samples_ready)
            Measures_Convert();

        if (timebase.flag_100ms)
        {
            tasks_100ms();
            timebase.flag_100ms = false;
        }
        else if (timebase.flag_300ms)
        {
            Inverter_Poll300ms(smb.step);
            timebase.flag_300ms = false;
        }
        else if (timebase.flag_1s)
        {
            tasks_1s();
            timebase.flag_1s = false;
        }
        else if (inverter.frame_received)
            Inverter_ProcessFrame();
        else if (timebase.can_ready && (smb.step > SMB_INIT))
        {
            CanTx_SendNext();
            timebase.can_ready = false;
        }
    }

    return 0;
}
