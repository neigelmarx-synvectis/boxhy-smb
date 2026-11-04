/**
 * @file test_can_tx.c
 * @brief Tests de la télémétrie CAN (src/can_tx.c) : identifiants, DLC, octets, créneaux.
 */
#include "test_framework.h"
#include "can_tx.h"
#include "smb_config.h"
#include "smb_state_machine.h"
#include "battery.h"
#include "fcs.h"
#include "ems.h"
#include "inverter.h"
#include "measures.h"
#include "hmi.h"
#include "cooling.h"
#include "hal/board_io.h"
#include "hal/can_port.h"
#include "hal/adc_port.h"

static void test_setup(void)
{
    MockCan_Reset();
    MockBoard_Reset();
    MockAdc_Reset();
    Smb_Init();
    Battery_Init();
    Fcs_Init();
    Ems_Init();
    Inverter_Init();
    Measures_Init();
    Hmi_Init();
    Cooling_Init();
    CanTx_Init();
    smb.step = SMB_BATTERY_MODE;
}

/* Un cycle complet de 25 créneaux. */
static void cycle(void)
{
    int i;
    for (i = 0; i < 25; i++)
        CanTx_SendNext();
}

static const sMockCanMsg *find(uint32_t id)
{
    int i;
    for (i = 0; i < mock_can.tx_count; i++)
        if (mock_can.tx[i].id == id)
            return &mock_can.tx[i];
    return NULL;
}

static void check_header(const sMockCanMsg *m, uint8_t dlc)
{
    ASSERT_TRUE(m != NULL);
    if (!m) return;
    ASSERT_EQ_INT(dlc, m->dlc);
    ASSERT_EQ_INT(CAN_FRAME_EXT, m->idType);
    ASSERT_EQ_INT(CAN_FRAME_DATA, m->frameType);
    ASSERT_EQ_INT(CAN_PRIORITY_LOW, m->priority);
}

TEST(ordre_des_creneaux_et_modulo_25)
{
    const uint32_t expected[13] = {0x2BA0000, 0x2AC0000, 0x2AC0001, 0x2C00000, 0x2C00001, 0x2C00002,
                                   0x2C00003, 0x2C00004, 0x2BA0003, 0x2FC1000, 0xCFF0027, 0xCFF0127, 0x2BA0001};
    int i;
    cycle();
    ASSERT_EQ_INT(13, mock_can.tx_count);
    for (i = 0; i < 13; i++)
        ASSERT_EQ_HEX(expected[i], mock_can.tx[i].id);
    /* le créneau 0 est vide : la première trame part au 2e appel */
    MockCan_Reset();
    CanTx_SendNext();
    ASSERT_EQ_INT(0, mock_can.tx_count);
    CanTx_SendNext();
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_HEX(0x2BA0000, mock_can.tx[0].id);
}

TEST(trames_onduleur_conditionnees_a_l_etat)
{
    smb.step = SMB_STANDBY;
    cycle();
    ASSERT_EQ_INT(11, mock_can.tx_count);
    ASSERT_TRUE(find(0x2AC0000) == NULL);
    ASSERT_TRUE(find(0x2AC0001) == NULL);

    MockCan_Reset();
    smb.step = SMB_EMERGENCY;
    cycle();
    ASSERT_EQ_INT(13, mock_can.tx_count);

    /* après arrêt : une dernière émission grâce au drapeau, acquitté par la trame 0x2AC0001 */
    MockCan_Reset();
    smb.step = SMB_STANDBY;
    inverter.raz_data_after_stop_flag = 1;
    cycle();
    ASSERT_EQ_INT(13, mock_can.tx_count);
    ASSERT_FALSE(inverter.raz_data_after_stop_flag);
    MockCan_Reset();
    cycle();
    ASSERT_EQ_INT(11, mock_can.tx_count);
}

TEST(trame_0x2BA0000_etat_batteries)
{
    const uint8_t expected[8] = {65, 0xB7, 0x2C, 0x01, 0xEC, 30, 77, 88};
    battery.soc = 65;
    battery.ibs[0].system_running_flag = 1;
    battery.ibs[0].error_flag = 1;
    battery.ibs[0].low_soc_warning_flag = 1;
    battery.ibs[0].balancing_flag = 0;
    battery.ibs[1].system_running_flag = 1;
    battery.ibs[1].error_flag = 1;
    battery.ibs[1].low_soc_warning_flag = 0;
    battery.ibs[1].balancing_flag = 1;
    battery.output_current = 300; /* 0x012C */
    battery.ibs[0].avg_cell_temperature = -20;
    battery.ibs[1].avg_cell_temperature = 30;
    battery.ibs[0].sop = 77;
    battery.ibs[1].sop = 88;
    cycle();
    check_header(find(0x2BA0000), 8);
    ASSERT_EQ_MEM(expected, find(0x2BA0000)->data, 8);
}

TEST(trame_0x2AC0000_defauts_onduleur)
{
    const uint8_t expected[7] = {INVERTER_BATTERY_MODE, 0x3F, 0xFF, 0xFF, 0x07, 0xFF, 0x7F};
    const sMockCanMsg *m;
    inverter.mode = INVERTER_BATTERY_MODE;
    inverter.error_flag = 1; inverter.device_switched_on_flag = 1; inverter.load_status_flag = 1;
    inverter.charging_status_flag = 1; inverter.ac_charging_status_flag = 1; inverter.scc_charging_status_flag = 1;
    inverter.bus_overvoltage_error_flag = 1; inverter.bus_undervoltage_error_flag = 1; inverter.bus_soft_fail_error_flag = 1;
    inverter.voltage_too_low_error_flag = 1; inverter.voltage_too_high_error_flag = 1; inverter.overtemperature_error_flag = 1;
    inverter.fan_locked_error_flag = 1; inverter.battery_voltage_high_error_flag = 1;
    inverter.overcharge_error_flag = 1; inverter.overload_error_flag = 1; inverter.over_current_error_flag = 1;
    inverter.soft_start_error_flag = 1; inverter.self_test_error_flag = 1; inverter.ac_output_dc_voltage_too_high_error_flag = 1;
    inverter.battery_disconnected_error_flag = 1; inverter.current_sensor_error_flag = 1;
    inverter.battery_short_circuit_error_flag = 1; inverter.mppt_overload_error_flag = 1; inverter.apparent_power_very_high_error_flag = 1;
    inverter.line_fail_warning_flag = 1; inverter.opv_short_warning_flag = 1; inverter.overtemperature_warning_flag = 1;
    inverter.fan_locked_warning_flag = 1; inverter.battery_voltage_high_warning_flag = 1; inverter.battery_voltage_low_warning_flag = 1;
    inverter.battery_under_shutdown_voltage_warning_flag = 1;
    inverter.battery_derating_warning_flag = 1; inverter.overload_warning_flag = 1; inverter.eeprom_warning_flag = 1;
    inverter.power_limit_warning_flag = 1; inverter.pv_voltage_high_warning_flag = 1; inverter.mppt_overload_warning_flag = 1;
    inverter.battery_too_low_to_charge_warning_flag = 1;
    cycle();
    m = find(0x2AC0000);
    check_header(m, 7);
    ASSERT_EQ_MEM(expected, m->data, 7);
}

TEST(trame_0x2AC0000_bits_individuels)
{
    const sMockCanMsg *m;
    inverter.scc_charging_status_flag = 1;      /* d1 b5 */
    inverter.battery_voltage_high_error_flag = 1; /* d2 b7 */
    inverter.overload_error_flag = 1;           /* d3 b1 */
    inverter.apparent_power_very_high_error_flag = 1; /* d4 b2 */
    inverter.bus_soft_fail_error_flag = 1;      /* d2 b2 et d5 b2 */
    inverter.battery_too_low_to_charge_warning_flag = 1; /* d6 b6 */
    cycle();
    m = find(0x2AC0000);
    ASSERT_EQ_HEX(0x20, m->data[1]);
    ASSERT_EQ_HEX(0x84, m->data[2]);
    ASSERT_EQ_HEX(0x02, m->data[3]);
    ASSERT_EQ_HEX(0x04, m->data[4]);
    ASSERT_EQ_HEX(0x04, m->data[5]);
    ASSERT_EQ_HEX(0x40, m->data[6]);
}

TEST(trame_0x2AC0001_mesures_onduleur)
{
    const uint8_t expected[8] = {0x72, 0x14, 0x04, 0x10, 0xFD, 0x08, 45, 0x05};
    inverter.moy_ac_output_apparent_power = 5234; /* 0x1472 */
    inverter.ac_output_active_power = 4100;       /* 0x1004 */
    inverter.ac_output_voltage = 2301;            /* 0x08FD */
    inverter.dcac_temp = 45;
    inverter.toggle_qmod_flag = 1;
    inverter.toggle_qpiws_flag = 0;
    inverter.toggle_qpigs_flag = 1;
    cycle();
    check_header(find(0x2AC0001), 8);
    ASSERT_EQ_MEM(expected, find(0x2AC0001)->data, 8);
}

TEST(trame_0x2C00000_etat_smb)
{
    const uint8_t expected[8] = {SMB_HYBRID_MODE, 0x1F, 0xFF, 0x03, FCCB_OPERATING, 0x1F, 0x0F, 0x6F};
    const sMockCanMsg *m;
    smb.step = SMB_HYBRID_MODE;
    smb.critical_error_flag = 1; inverter.general_error_flag = 1; fcs.general_error_flag = 1;
    fcs.ppmh2_error_flag = 1; inverter.recieve_flag = 1;
    battery.ibs[0].watchdog.error = 1; battery.ibs[1].watchdog.error = 1; inverter.watchdog_error_flag = 1;
    fcs.watchdog.error = 1; fcs.startup_error_flag = 1; measures.ac_ambiant_temp.error_flag = 1;
    measures.temp2.error_flag = 1; smb.software_error_flag = 1;
    hmi.battery_mode_button.state = 1; hmi.hybrid_mode_button.state = 1;
    fcs.step = FCCB_OPERATING;
    battery.soc_high_level_warning_flag = 1; battery.soc_low_level_warning_flag = 1;
    inverter.apparent_power_high_warning_flag = 1; battery.sop_warning_flag = 1; battery.sop_error_flag = 1;
    battery.soc_very_very_high_level_error_flag = 1; battery.soc_very_low_level_error_flag = 1;
    battery.overvoltage_warning_flag = 1; battery.overvoltage.error = 1;
    battery.overcurrent_warning_flag = 1; battery.overcurrent.error = 1;
    battery.overtemperature_charge.error = 1; battery.overtemperature_discharge.error = 1;
    battery.overtemperature_charge_warning_flag = 1; battery.overtemperature_discharge_warning_flag = 1;
    cycle();
    m = find(0x2C00000);
    check_header(m, 8);
    ASSERT_EQ_MEM(expected, m->data, 8);
}

TEST(trame_0x2C00000_bits_individuels)
{
    const sMockCanMsg *m;
    smb.step = SMB_STANDBY;
    inverter.recieve_flag = 1;                    /* d1 b4 */
    measures.temp2.error_flag = 1;                /* d2 b6 */
    hmi.hybrid_mode_button.state = 1;             /* d3 b1 */
    battery.sop_error_flag = 1;                   /* d5 b4 */
    battery.overvoltage.error = 1;                /* d6 b3 */
    battery.overtemperature_discharge_warning_flag = 1; /* d7 b6 */
    cycle();
    m = find(0x2C00000);
    ASSERT_EQ_INT(SMB_STANDBY, m->data[0]);
    ASSERT_EQ_HEX(0x10, m->data[1]);
    ASSERT_EQ_HEX(0x40, m->data[2]);
    ASSERT_EQ_HEX(0x02, m->data[3]);
    ASSERT_EQ_HEX(0x00, m->data[4]);
    ASSERT_EQ_HEX(0x10, m->data[5]);
    ASSERT_EQ_HEX(0x08, m->data[6]);
    ASSERT_EQ_HEX(0x40, m->data[7]);
}

TEST(trame_0x2C00001_entrees_sorties_brutes)
{
    const uint8_t expected[8] = {0x15, 0xAA, 20, 30, 0xE8, 0x03, 0xC4, 0x09};
    mock_board.btn_left = 1; mock_board.btn_middle = 0; mock_board.btn_right = 1;
    mock_board.btn_battery_mode = 0; mock_board.btn_hybrid_mode = 1;
    mock_board.dcac_input_relay = 0; mock_board.dcac_precharge_relay = 1; mock_board.supply_precharge_relay = 0;
    mock_board.fcs_output_relay = 1; mock_board.buzzer = 0; mock_board.led_left = 1; mock_board.led_middle = 0;
    mock_board.led_right = 1;
    measures.temp_1_raw = 2050; /* /100 = 20 */
    measures.temp_2_raw = 3099; /* 30 */
    measures.supply_voltage_raw = 1000;
    measures.bus_voltage_raw = 2500;
    cycle();
    check_header(find(0x2C00001), 8);
    ASSERT_EQ_MEM(expected, find(0x2C00001)->data, 8);
}

TEST(trame_0x2C00002_mesures_converties)
{
    const uint8_t expected[8] = {25, 0xF6, 0, 0, 0xB1, 0x07, 0x29, 0x13};
    measures.ac_ambiant_temp.value_8 = 25;
    measures.temp2.value_8 = -10;
    measures.supply_voltage = 1969; /* 0x07B1 */
    measures.bus_voltage = 4905;    /* 0x1329 */
    cycle();
    check_header(find(0x2C00002), 8);
    ASSERT_EQ_MEM(expected, find(0x2C00002)->data, 8);
}

TEST(trame_0x2C00003_courants)
{
    const uint8_t expected[8] = {0xB8, 0x0B, 0x7C, 0x15, 0x18, 0xFC, 0xD0, 0x07};
    fcs.output_current = 3000;          /* 0x0BB8 */
    ems.requested_fcs_current = 5500;   /* 0x157C */
    battery.output_current = -1000;     /* 0xFC18 */
    ems.system_output_current = 2000;   /* 0x07D0 */
    cycle();
    check_header(find(0x2C00003), 8);
    ASSERT_EQ_MEM(expected, find(0x2C00003)->data, 8);
}

TEST(trame_0x2C00004_version_current_behaviour)
{
    /* TODO(bug 4) : l'octet 4 vaut toujours 0 (cast avant décalage) ; TODO(bug 6) : version 1.2.7 */
    const uint8_t expected[6] = {1, 2, 7, 0x2C, 0x00, 1};
    hmi.buzzer_5_minutes.cnt = 300; /* 0x012C */
    hmi.raz_erreur_ihm_flag = 1;
    cycle();
    check_header(find(0x2C00004), 6);
    ASSERT_EQ_MEM(expected, find(0x2C00004)->data, 6);
}

TEST(trame_0x2BA0001_detail_batteries)
{
    const uint8_t expected[8] = {0x64, 0x00, 0x38, 0xFF, 40, 41, 0xE2, 43};
    battery.ibs[0].output_current = 100;
    battery.ibs[1].output_current = -200;
    battery.ibs[0].max_cell_temperature = 40;
    battery.ibs[1].max_cell_temperature = 41;
    battery.ibs[0].max_board_temperature = -30;
    battery.ibs[1].max_board_temperature = 43;
    cycle();
    check_header(find(0x2BA0001), 8);
    ASSERT_EQ_MEM(expected, find(0x2BA0001)->data, 8);
}

TEST(trames_fixes_0x2BA0003_et_0x2FC1000)
{
    const uint8_t zero[2] = {0, 0};
    const uint8_t hb[2] = {0, 1};
    cycle();
    check_header(find(0x2BA0003), 2);
    ASSERT_EQ_MEM(zero, find(0x2BA0003)->data, 2);
    check_header(find(0x2FC1000), 2);
    ASSERT_EQ_MEM(hb, find(0x2FC1000)->data, 2);
}

TEST(trame_0xCFF0027_refroidissement)
{
    const uint8_t expected[4] = {11, 22, 33, 44};
    cooling.inverter_pwm = 11;
    cooling.ambiant_pwm = 22;
    cooling.dcdc_pwm = 33;
    cooling.fans_duty_cycle = 44;
    cycle();
    check_header(find(0xCFF0027), 4);
    ASSERT_EQ_MEM(expected, find(0xCFF0027)->data, 4);
}

TEST(trame_0xCFF0127_gestion_energie_current_behaviour)
{
    /* TODO(bug 4) : octet 2 = (uint8_t)5500 / 100 = 0x7C / 100 = 1 */
    const uint8_t expected[7] = {0, 0, 1, 0x30, 0xF8, 0x64, 0x00};
    ems.requested_fcs_current = 5500;
    battery.ibs[0].max_charge_current = -2000; /* 0xF830 */
    battery.ibs[0].max_discharge_current = 100;
    cycle();
    check_header(find(0xCFF0127), 7);
    ASSERT_EQ_MEM(expected, find(0xCFF0127)->data, 7);
}

TEST(buffer_remis_a_zero_entre_les_trames)
{
    /* la trame 0x2C00002 laisse les octets 2 et 3 à 0 même après une trame pleine */
    const sMockCanMsg *m;
    battery.output_current = 0x7FFF;
    measures.supply_voltage = 0xFFFF;
    cycle();
    m = find(0x2C00002);
    ASSERT_EQ_INT(0, m->data[2]);
    ASSERT_EQ_INT(0, m->data[3]);
    m = find(0x2BA0003);
    ASSERT_EQ_INT(0, m->data[0]);
    ASSERT_EQ_INT(0, m->data[1]);
}

TEST_MAIN_BEGIN()
    RUN_TEST(ordre_des_creneaux_et_modulo_25);
    RUN_TEST(trames_onduleur_conditionnees_a_l_etat);
    RUN_TEST(trame_0x2BA0000_etat_batteries);
    RUN_TEST(trame_0x2AC0000_defauts_onduleur);
    RUN_TEST(trame_0x2AC0000_bits_individuels);
    RUN_TEST(trame_0x2AC0001_mesures_onduleur);
    RUN_TEST(trame_0x2C00000_etat_smb);
    RUN_TEST(trame_0x2C00000_bits_individuels);
    RUN_TEST(trame_0x2C00001_entrees_sorties_brutes);
    RUN_TEST(trame_0x2C00002_mesures_converties);
    RUN_TEST(trame_0x2C00003_courants);
    RUN_TEST(trame_0x2C00004_version_current_behaviour);
    RUN_TEST(trame_0x2BA0001_detail_batteries);
    RUN_TEST(trames_fixes_0x2BA0003_et_0x2FC1000);
    RUN_TEST(trame_0xCFF0027_refroidissement);
    RUN_TEST(trame_0xCFF0127_gestion_energie_current_behaviour);
    RUN_TEST(buffer_remis_a_zero_entre_les_trames);
TEST_MAIN_END()
