/**
 * @file test_hmi.c
 * @brief Tests de l'IHM (src/hmi/hmi.c).
 */
#include "test_framework.h"
#include "src/hmi/hmi.h"
#include "src/devices/battery.h"
#include "src/devices/inverter.h"
#include "src/devices/fcs.h"
#include "hal/board_io.h"
#include "hal/can_port.h"

static void test_setup(void)
{
    MockBoard_Reset();
    MockCan_Reset();
    Battery_Init();
    Inverter_Init();
    Fcs_Init();
    Hmi_Init();
}

static void buttons_n(int n)
{
    while (n--)
        Hmi_UpdateButtons100ms();
}

static void buzzer_n(int n, uint8_t step)
{
    while (n--)
        Hmi_UpdateBuzzer100ms(step);
}

TEST(bouton_batterie_seuil_4)
{
    mock_board.btn_battery_mode = true;
    buttons_n(4);
    ASSERT_FALSE(hmi.battery_mode_button.state);
    buttons_n(1);
    ASSERT_TRUE(hmi.battery_mode_button.state);
    mock_board.btn_battery_mode = false;
    buttons_n(4);
    ASSERT_TRUE(hmi.battery_mode_button.state);
    buttons_n(1);
    ASSERT_FALSE(hmi.battery_mode_button.state);
}

TEST(bouton_hybride_acquitte_les_erreurs_pile)
{
    fcs.general_error_flag = 1;
    fcs.minor_error_flag = 1;
    mock_board.btn_hybrid_mode = true;
    buttons_n(4);
    ASSERT_EQ_INT(0, mock_can.tx_count);
    buttons_n(1);
    ASSERT_TRUE(hmi.hybrid_mode_button.state);
    ASSERT_EQ_INT(1, mock_can.tx_count); /* Fcs_ResetErrors -> commande 0x2FC1001 */
    ASSERT_FALSE(fcs.general_error_flag);
    ASSERT_FALSE(fcs.minor_error_flag);
    /* relâchement : pas de nouvelle commande */
    mock_board.btn_hybrid_mode = false;
    buttons_n(5);
    ASSERT_FALSE(hmi.hybrid_mode_button.state);
    ASSERT_EQ_INT(1, mock_can.tx_count);
}

TEST(bouton_hybride_sans_erreur_pile_pas_de_commande)
{
    mock_board.btn_hybrid_mode = true;
    buttons_n(5);
    ASSERT_TRUE(hmi.hybrid_mode_button.state);
    ASSERT_EQ_INT(0, mock_can.tx_count);
}

TEST(bouton_central_appui_long_50)
{
    mock_board.btn_middle = true;
    buttons_n(50);
    ASSERT_FALSE(hmi.middle_button.state);
    buttons_n(1);
    ASSERT_TRUE(hmi.middle_button.state);
    mock_board.btn_middle = false;
    buttons_n(10);
    ASSERT_TRUE(hmi.middle_button.state); /* acquitté par la machine d'états */
    hmi.middle_button.state = false;
    buttons_n(10);
    ASSERT_FALSE(hmi.middle_button.state);
}

TEST(buzzer_inactif_hors_modes_batterie_hybride)
{
    battery.soc_low_level_warning_flag = 1;
    battery.output_current = 100;
    battery.soc = 20;
    mock_board.buzzer = true;
    buzzer_n(20, 30);
    ASSERT_TRUE(mock_board.buzzer); /* non touché */
    buzzer_n(20, 250);
    ASSERT_TRUE(mock_board.buzzer);
}

TEST(buzzer_coupe_sans_avertissement)
{
    mock_board.buzzer = true;
    Hmi_UpdateBuzzer100ms(100);
    ASSERT_FALSE(mock_board.buzzer);
}

TEST(buzzer_soc_bas_double_bip_periode_selon_soc)
{
    /* SOC 20 : période (20 - 10) * 10 + 20 = 120 ticks */
    battery.soc_low_level_warning_flag = 1;
    battery.output_current = 100;
    battery.soc = 20;
    buzzer_n(9, 100);
    ASSERT_FALSE(mock_board.buzzer);
    buzzer_n(1, 100); ASSERT_TRUE(mock_board.buzzer);  /* 10 */
    buzzer_n(2, 100); ASSERT_FALSE(mock_board.buzzer); /* 12 */
    buzzer_n(2, 100); ASSERT_TRUE(mock_board.buzzer);  /* 14 */
    buzzer_n(2, 100); ASSERT_FALSE(mock_board.buzzer); /* 16 */
    buzzer_n(120 - 16, 100); /* -> 120 : RAZ */
    buzzer_n(10, 100);
    ASSERT_TRUE(mock_board.buzzer); /* nouveau cycle, pas 10 */
}

TEST(buzzer_soc_bas_seulement_en_decharge)
{
    battery.soc_low_level_warning_flag = 1;
    battery.output_current = -100; /* charge */
    battery.soc = 20;
    mock_board.buzzer = true;
    buzzer_n(10, 100);
    ASSERT_FALSE(mock_board.buzzer); /* branche "else" : coupé */
}

TEST(buzzer_soc_haut_periode_selon_soc)
{
    /* SOC 94 : période (95 - 94) * 30 + 20 = 50 */
    battery.soc_high_level_warning_flag = 1;
    battery.soc = 94;
    buzzer_n(10, 150);
    ASSERT_TRUE(mock_board.buzzer);
    buzzer_n(6, 150);
    ASSERT_FALSE(mock_board.buzzer);
    buzzer_n(50 - 16, 150); /* RAZ à 50 */
    buzzer_n(10, 150);
    ASSERT_TRUE(mock_board.buzzer);
}

TEST(buzzer_puissance_ou_sop_periode_19)
{
    inverter.apparent_power_high_warning_flag = 1;
    buzzer_n(10, 100);
    ASSERT_TRUE(mock_board.buzzer);
    buzzer_n(6, 100);
    ASSERT_FALSE(mock_board.buzzer);
    buzzer_n(3, 100); /* 19 : RAZ */
    buzzer_n(10, 100);
    ASSERT_TRUE(mock_board.buzzer);

    Hmi_Init();
    MockBoard_Reset();
    inverter.apparent_power_high_warning_flag = 0;
    battery.sop_warning_flag = 1;
    buzzer_n(10, 100);
    ASSERT_TRUE(mock_board.buzzer);
}

TEST(buzzer_priorite_soc_bas_puis_soc_haut)
{
    /* soc bas (en décharge) prime sur soc haut qui prime sur puissance */
    battery.soc_low_level_warning_flag = 1;
    battery.soc_high_level_warning_flag = 1;
    battery.output_current = 100;
    battery.soc = 5; /* < 10 : pas de RAZ explicite */
    buzzer_n(16, 100);
    ASSERT_FALSE(mock_board.buzzer);
    buzzer_n(240, 100); /* 256 : débordement 8 bits */
    buzzer_n(10, 100);
    ASSERT_TRUE(mock_board.buzzer);
}

TEST(limite_5_minutes_en_mode_defaut)
{
    int i;
    for (i = 0; i < 300; i++)
        Hmi_Update1s(250);
    ASSERT_FALSE(hmi.buzzer_5_minutes.error);
    Hmi_Update1s(250);
    ASSERT_TRUE(hmi.buzzer_5_minutes.error);
    ASSERT_EQ_INT(0, hmi.buzzer_5_minutes.cnt);
    /* hors mode défaut : compteur à zéro, drapeau conservé (acquitté par la machine d'états) */
    for (i = 0; i < 100; i++)
        Hmi_Update1s(250);
    Hmi_Update1s(30);
    ASSERT_EQ_INT(0, hmi.buzzer_5_minutes.cnt);
    ASSERT_TRUE(hmi.buzzer_5_minutes.error);
}

TEST_MAIN_BEGIN()
    RUN_TEST(bouton_batterie_seuil_4);
    RUN_TEST(bouton_hybride_acquitte_les_erreurs_pile);
    RUN_TEST(bouton_hybride_sans_erreur_pile_pas_de_commande);
    RUN_TEST(bouton_central_appui_long_50);
    RUN_TEST(buzzer_inactif_hors_modes_batterie_hybride);
    RUN_TEST(buzzer_coupe_sans_avertissement);
    RUN_TEST(buzzer_soc_bas_double_bip_periode_selon_soc);
    RUN_TEST(buzzer_soc_bas_seulement_en_decharge);
    RUN_TEST(buzzer_soc_haut_periode_selon_soc);
    RUN_TEST(buzzer_puissance_ou_sop_periode_19);
    RUN_TEST(buzzer_priorite_soc_bas_puis_soc_haut);
    RUN_TEST(limite_5_minutes_en_mode_defaut);
TEST_MAIN_END()
