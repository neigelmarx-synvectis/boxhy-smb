/**
 * @file test_battery.c
 * @brief Tests du module batteries IBS (src/devices/battery.c).
 */
#include "test_framework.h"
#include "src/devices/battery.h"
#include "src/smb_config.h"
#include "hal/can_port.h"

static uint8_t rx_data[8];
static CAN_MSG_OBJ rx;

static void test_setup(void)
{
    MockCan_Reset();
    Battery_Init();
    memset(rx_data, 0, sizeof(rx_data));
    rx.data = rx_data;
    rx.field.dlc = 8;
}

static bool frame(uint32_t id, const uint8_t *d)
{
    rx.msgId = id;
    memcpy(rx_data, d, 8);
    return Battery_HandleCanFrame(&rx);
}

static void supervise_n(int n, uint8_t step, uint16_t bus)
{
    while (n--)
        Battery_Supervise100ms(step, bus);
}

/* ---- Décodage du courant IBS (flottant IEEE big-endian, signe inversé) ------------------- */
static int16_t current_of(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
    const uint8_t d[8] = {d0, d1, d2, d3, 0, 0, 0, 0};
    frame(0x18FF02D0, d);
    return battery.ibs[0].output_current;
}

TEST(decode_courant_ibs)
{
    ASSERT_EQ_INT(100, current_of(0xBF, 0x80, 0x00, 0x00));   /* -1.00 A IBS -> +1.00 A */
    ASSERT_EQ_INT(150, current_of(0xBF, 0xC0, 0x00, 0x00));   /* +1.50 A */
    ASSERT_EQ_INT(-200, current_of(0x40, 0x00, 0x00, 0x00));  /* -2.00 A */
    ASSERT_EQ_INT(1025, current_of(0xC1, 0x24, 0x00, 0x00));  /* +10.25 A */
    ASSERT_EQ_INT(-6400, current_of(0x42, 0x80, 0x00, 0x00)); /* -64.00 A : 2^6 */
    /* hors plage int16 : même troncature que l'origine */
    ASSERT_EQ_INT((int16_t)(int32_t)102450, current_of(0xC4, 0x80, 0x10, 0x00));
    ASSERT_EQ_INT(0, current_of(0x80, 0x00, 0x00, 0x00));     /* -0.0 */
}

TEST(decode_courant_ibs_mantisse_complete)
{
    /* L'origine ignorait les 12 bits de poids faible de la mantisse : 0xC2C7FFF3 = -99.9999 A.
     * Ancien décodage : 99.96 A (9996) ; décodage exact : 9999. */
    ASSERT_EQ_INT(9999, current_of(0xC2, 0xC7, 0xFF, 0xF3));
}

/* ---- Décodage des trames ---------------------------------------------------- */
TEST(trame_courant_et_sop_D0_D1)
{
    const uint8_t d0[8] = {0xBF, 0x80, 0, 0, 77, 0, 0, 0};
    const uint8_t d1[8] = {0x40, 0x00, 0, 0, 33, 0, 0, 0};
    battery.ibs[0].watchdog.cnt = 12;
    ASSERT_TRUE(frame(0x18FF02D0, d0));
    ASSERT_TRUE(frame(0x18FF02D1, d1));
    ASSERT_EQ_INT(100, battery.ibs[0].output_current);
    ASSERT_EQ_INT(77, battery.ibs[0].sop);
    ASSERT_EQ_INT(0, battery.ibs[0].watchdog.cnt);
    ASSERT_EQ_INT(-200, battery.ibs[1].output_current);
    ASSERT_EQ_INT(33, battery.ibs[1].sop);
}

TEST(trame_temperatures_cellules)
{
    /* d4..d5 : moyenne 25.6 °C = 256 (0x0100) ; d2..d3 : max = 0x0190 = 40.0 °C (bug 5 corrigé : x256) */
    const uint8_t d[8] = {0, 0, 0x01, 0x90, 0x01, 0x00, 0, 0};
    ASSERT_TRUE(frame(0x18FF03D0, d));
    ASSERT_EQ_INT(25, battery.ibs[0].avg_cell_temperature);
    ASSERT_EQ_INT(40, battery.ibs[0].max_cell_temperature);
}

TEST(trame_temperature_moyenne_negative)
{
    const uint8_t d[8] = {0, 0, 0, 0, 0xFF, 0x38, 0, 0}; /* -200 -> -20 °C */
    frame(0x18FF03D1, d);
    ASSERT_EQ_INT(-20, battery.ibs[1].avg_cell_temperature);
}

TEST(trame_temperature_carte)
{
    const uint8_t d[8] = {0, 0, 0x01, 0x2C, 0, 0, 0, 0}; /* 0x012C = 30.0 °C */
    ASSERT_TRUE(frame(0x18FF04D0, d));
    ASSERT_EQ_INT(30, battery.ibs[0].max_board_temperature);
}

TEST(trame_soc)
{
    const uint8_t d[8] = {0x27, 0x10, 0, 0, 0, 0, 0, 0}; /* 10000 -> 100 % */
    const uint8_t d2[8] = {0x13, 0x88, 0, 0, 0, 0, 0, 0}; /* 5000 -> 50 % */
    ASSERT_TRUE(frame(0x18FF05D0, d));
    ASSERT_TRUE(frame(0x18FF05D1, d2));
    ASSERT_EQ_INT(100, battery.ibs[0].soc);
    ASSERT_EQ_INT(50, battery.ibs[1].soc);
}

TEST(trame_infos_flags)
{
    const uint8_t ok[8] = {0, 0x03, 0x22, 0, 0, 0, 0, 0};   /* running, balancing ; b1 = 1 -> pas d'erreur ; b5 -> low soc */
    const uint8_t err[8] = {0, 0x00, 0x00, 0, 0, 0, 0, 0};  /* b1 = 0 -> erreur */
    ASSERT_TRUE(frame(0x18FF07D0, ok));
    ASSERT_TRUE(battery.ibs[0].system_running_flag);
    ASSERT_TRUE(battery.ibs[0].balancing_flag);
    ASSERT_FALSE(battery.ibs[0].error_flag);
    ASSERT_TRUE(battery.ibs[0].low_soc_warning_flag);
    ASSERT_TRUE(frame(0x18FF07D1, err));
    ASSERT_TRUE(battery.ibs[1].error_flag);
    ASSERT_FALSE(battery.ibs[1].low_soc_warning_flag);
}

TEST(trame_courants_max)
{
    const uint8_t d[8] = {0xBF, 0x80, 0, 0, 0x40, 0x00, 0, 0}; /* décharge +1.00 A ; charge -2.00 A */
    ASSERT_TRUE(frame(0x18FF0CD1, d));
    ASSERT_EQ_INT(100, battery.ibs[1].max_discharge_current);
    ASSERT_EQ_INT(-200, battery.ibs[1].max_charge_current);
}

TEST(ids_batterie_inconnus_consommes_autres_ignores)
{
    const uint8_t d[8] = {0};
    ASSERT_TRUE(frame(0x18FF3CD0, d));  /* sans effet mais ID batterie */
    ASSERT_TRUE(frame(0x18FF01D1, d));
    ASSERT_FALSE(frame(0x18FF02D2, d)); /* batterie 2 : hors périmètre */
    ASSERT_FALSE(frame(0x2FC0000, d));
}

/* ---- Agrégation -------------------------------------------------------------- */
TEST(agregation_soc_et_courant)
{
    battery.ibs[0].soc = 60;
    battery.ibs[1].soc = 71;
    battery.ibs[0].output_current = 1500;
    battery.ibs[1].output_current = -300;
    Battery_Aggregate100ms();
    ASSERT_EQ_INT(65, battery.soc); /* 131 / 2 */
    ASSERT_EQ_INT(1200, battery.output_current);
}

/* ---- Keep-alive et watchdogs ------------------------------------------------- */
TEST(keep_alive_alterne_les_ids)
{
    Battery_Update1s();
    ASSERT_EQ_HEX(0x180AD100, MockCan_Tx(0)->id);
    ASSERT_EQ_INT(1, MockCan_Tx(0)->dlc);
    ASSERT_EQ_INT(CAN_PRIORITY_HIGH, MockCan_Tx(0)->priority);
    Battery_Update1s();
    ASSERT_EQ_HEX(0x180AD000, MockCan_Tx(1)->id);
    Battery_Update1s();
    ASSERT_EQ_HEX(0x180AD100, MockCan_Tx(2)->id);
}

TEST(watchdog_par_batterie_30s)
{
    int i;
    const uint8_t d[8] = {0xBF, 0x80, 0, 0, 0, 0, 0, 0};
    for (i = 0; i < 30; i++)
    {
        Battery_Update1s();
        if (i == 15)
            frame(0x18FF02D1, d); /* la batterie 1 répond une fois */
    }
    ASSERT_FALSE(battery.ibs[0].watchdog.error);
    Battery_Update1s();
    ASSERT_TRUE(battery.ibs[0].watchdog.error);
    ASSERT_FALSE(battery.ibs[1].watchdog.error);
    ASSERT_TRUE(Battery_HasCriticalError());
}

/* ---- Supervisions ------------------------------------------------------------ */
TEST(soc_bas_avertissement_et_erreur_impulsionnelle)
{
    battery.soc = 20;
    Battery_Supervise100ms(30, 0);
    ASSERT_TRUE(battery.soc_low_level_warning_flag);
    ASSERT_FALSE(battery.soc_very_low_level_error_flag);

    battery.soc = 9;
    supervise_n(50, 30, 0);
    ASSERT_FALSE(battery.soc_very_low_level_error_flag);
    Battery_Supervise100ms(30, 0); /* 51e : cpt > 50 */
    ASSERT_TRUE(battery.soc_very_low_level_error_flag);
    ASSERT_TRUE(Battery_HasCriticalError());
    Battery_Supervise100ms(30, 0); /* le tick suivant, le flag retombe (origine) */
    ASSERT_FALSE(battery.soc_very_low_level_error_flag);

    battery.soc = 30;
    Battery_Supervise100ms(30, 0);
    ASSERT_FALSE(battery.soc_low_level_warning_flag);
}

TEST(soc_haut_current_behaviour_erreur_jamais_levee)
{
    /* TODO(bug 2) : avertissement seulement au-dessus de 95 %, et compteur bool */
    battery.soc = 90;
    Battery_Supervise100ms(30, 0);
    ASSERT_FALSE(battery.soc_high_level_warning_flag);
    battery.soc = 96;
    supervise_n(500, 30, 0);
    ASSERT_TRUE(battery.soc_high_level_warning_flag);
    ASSERT_FALSE(battery.soc_very_very_high_level_error_flag);
}

TEST(surintensite_avertissement_et_current_behaviour_erreur_jamais_levee)
{
    battery.ibs[0].max_charge_current = -4000;
    battery.ibs[1].max_charge_current = -4000;
    battery.ibs[0].output_current = 6901;
    Battery_Supervise100ms(30, 0);
    ASSERT_TRUE(battery.overcurrent_warning_flag);
    battery.ibs[0].output_current = 0;
    battery.ibs[1].output_current = -3401; /* < -4000 * 0.85 = -3400 */
    Battery_Supervise100ms(30, 0);
    ASSERT_TRUE(battery.overcurrent_warning_flag);
    battery.ibs[1].output_current = -3400;
    Battery_Supervise100ms(30, 0);
    ASSERT_FALSE(battery.overcurrent_warning_flag);
    ASSERT_EQ_INT(0, battery.overcurrent.cnt);

    /* TODO(bug 9) : compteur 8 bits, timeout 450 */
    battery.ibs[0].output_current = 7000;
    supervise_n(5000, 30, 0);
    ASSERT_FALSE(battery.overcurrent.error);
}

TEST(surchauffe_charge_et_decharge_selon_le_signe_du_courant)
{
    battery.ibs[0].max_cell_temperature = 46;
    battery.ibs[1].max_cell_temperature = 20;
    battery.output_current = -100; /* charge */
    supervise_n(50, 30, 0);
    ASSERT_EQ_INT(46, battery.max_temperature);
    ASSERT_TRUE(battery.overtemperature_charge_warning_flag);
    ASSERT_FALSE(battery.overtemperature_charge.error);
    ASSERT_FALSE(battery.overtemperature_discharge_warning_flag);
    Battery_Supervise100ms(30, 0);
    ASSERT_TRUE(battery.overtemperature_charge.error);

    Battery_Init();
    battery.ibs[1].max_cell_temperature = 56;
    battery.output_current = 100; /* décharge */
    supervise_n(51, 30, 0);
    ASSERT_TRUE(battery.overtemperature_discharge.error);
    ASSERT_FALSE(battery.overtemperature_charge_warning_flag);

    Battery_Init();
    battery.ibs[1].max_cell_temperature = 56;
    battery.output_current = 0; /* ni charge ni décharge : rien */
    supervise_n(100, 30, 0);
    ASSERT_FALSE(battery.overtemperature_discharge.error);
    ASSERT_FALSE(battery.overtemperature_charge.error);
}

TEST(surtension_bus_60V)
{
    supervise_n(50, 30, 6001);
    ASSERT_TRUE(battery.overvoltage_warning_flag);
    ASSERT_FALSE(battery.overvoltage.error);
    Battery_Supervise100ms(30, 6001);
    ASSERT_TRUE(battery.overvoltage.error);
    Battery_Supervise100ms(30, 6000);
    ASSERT_FALSE(battery.overvoltage_warning_flag);
    ASSERT_TRUE(battery.overvoltage.error); /* verrouillée */
}

TEST(sop_surveille_a_partir_du_standby)
{
    battery.ibs[0].sop = 19;
    battery.ibs[1].sop = 100;
    Battery_Supervise100ms(20, 0); /* autocheck : pas de surveillance */
    ASSERT_FALSE(battery.sop_warning_flag);
    ASSERT_FALSE(battery.sop_error_flag);
    Battery_Supervise100ms(30, 0);
    ASSERT_TRUE(battery.sop_warning_flag);
    ASSERT_TRUE(battery.sop_error_flag);
    battery.ibs[0].sop = 25;
    Battery_Supervise100ms(100, 0);
    ASSERT_TRUE(battery.sop_warning_flag);
    ASSERT_TRUE(battery.sop_error_flag); /* l'erreur reste tant que l'avertissement est présent */
    battery.ibs[0].sop = 30;
    Battery_Supervise100ms(100, 0);
    ASSERT_FALSE(battery.sop_warning_flag);
    ASSERT_FALSE(battery.sop_error_flag);
}

TEST(reset_errors_efface_tout_sauf_les_avertissements)
{
    battery.ibs[0].error_flag = 1;
    battery.ibs[1].watchdog.error = 1;
    battery.soc_very_low_level_error_flag = 1;
    battery.soc_very_very_high_level_error_flag = 1;
    battery.overtemperature_charge.error = 1;
    battery.overtemperature_discharge.error = 1;
    battery.overcurrent.error = 1;
    battery.overvoltage.error = 1;
    battery.sop_error_flag = 1;
    battery.sop_warning_flag = 1;
    ASSERT_TRUE(Battery_HasCriticalError());
    Battery_ResetErrors();
    ASSERT_FALSE(Battery_HasCriticalError());
    ASSERT_TRUE(battery.sop_warning_flag);
}

TEST_MAIN_BEGIN()
    RUN_TEST(decode_courant_ibs);
    RUN_TEST(decode_courant_ibs_mantisse_complete);
    RUN_TEST(trame_courant_et_sop_D0_D1);
    RUN_TEST(trame_temperatures_cellules);
    RUN_TEST(trame_temperature_moyenne_negative);
    RUN_TEST(trame_temperature_carte);
    RUN_TEST(trame_soc);
    RUN_TEST(trame_infos_flags);
    RUN_TEST(trame_courants_max);
    RUN_TEST(ids_batterie_inconnus_consommes_autres_ignores);
    RUN_TEST(agregation_soc_et_courant);
    RUN_TEST(keep_alive_alterne_les_ids);
    RUN_TEST(watchdog_par_batterie_30s);
    RUN_TEST(soc_bas_avertissement_et_erreur_impulsionnelle);
    RUN_TEST(soc_haut_current_behaviour_erreur_jamais_levee);
    RUN_TEST(surintensite_avertissement_et_current_behaviour_erreur_jamais_levee);
    RUN_TEST(surchauffe_charge_et_decharge_selon_le_signe_du_courant);
    RUN_TEST(surtension_bus_60V);
    RUN_TEST(sop_surveille_a_partir_du_standby);
    RUN_TEST(reset_errors_efface_tout_sauf_les_avertissements);
TEST_MAIN_END()
