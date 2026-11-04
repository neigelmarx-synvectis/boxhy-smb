/**
 * @file test_inverter.c
 * @brief Tests de l'onduleur RS232 (src/inverter.c).
 */
#include "test_framework.h"
#include "inverter.h"
#include "smb_config.h"
#include "hal/uart_port.h"

static void test_setup(void)
{
    MockUart_Reset();
    Inverter_Init();
}

/* Envoie une requête (pour fixer frame_id) puis injecte une réponse "(...)\r" octet par octet. */
static void poll_until(uint8_t ind)
{
    uint8_t i;
    for (i = 0; i < ind; i++)
        Inverter_Poll300ms(100);
}

static void receive(const char *payload)
{
    const char *p;
    Inverter_OnRxByte('(');
    for (p = payload; *p; p++)
        Inverter_OnRxByte((uint8_t)*p);
    Inverter_OnRxByte('\r');
}

/* Réponse QPIGS de 106 caractères construite depuis un gabarit, chaque champ à une position fixe. */
static char qpigs[107];
static void qpigs_default(void)
{
    int i;
    for (i = 0; i < 106; i++)
        qpigs[i] = '0';
    qpigs[106] = 0;
    /* séparateurs aux positions de l'origine : 4, 9, 14, 16, 21, 26, 31, 35, 39, 45, 49, ... */
    qpigs[4] = ' '; qpigs[9] = ' '; qpigs[14] = '.'; qpigs[16] = ' '; qpigs[21] = ' ';
    qpigs[26] = ' '; qpigs[31] = ' ';
}

/* ---- Requêtes --------------------------------------------------------------- */
TEST(polling_300ms_sequence_et_octets_exacts)
{
    const uint8_t qmod[] = {'Q', 'M', 'O', 'D', 0x49, 0xC1, 0x0D};
    const uint8_t qpiws[] = {'Q', 'P', 'I', 'W', 'S', 0xB4, 0xDA, 0x0D};
    const uint8_t qpigs_req[] = {'Q', 'P', 'I', 'G', 'S', 0xB7, 0xA9, 0x0D};
    int i;

    Inverter_Poll300ms(100); /* ind 1 : QMOD */
    ASSERT_EQ_INT(7, mock_uart.tx_count);
    ASSERT_EQ_MEM(qmod, mock_uart.tx, 7);
    for (i = 2; i <= 5; i++)
        Inverter_Poll300ms(100);
    ASSERT_EQ_INT(7, mock_uart.tx_count);
    Inverter_Poll300ms(100); /* ind 6 : QPIWS */
    ASSERT_EQ_INT(15, mock_uart.tx_count);
    ASSERT_EQ_MEM(qpiws, mock_uart.tx + 7, 8);
    for (i = 7; i <= 12; i++)
        Inverter_Poll300ms(100);
    Inverter_Poll300ms(100); /* ind 13 : QPIGS */
    ASSERT_EQ_INT(23, mock_uart.tx_count);
    ASSERT_EQ_MEM(qpigs_req, mock_uart.tx + 15, 8);
    for (i = 14; i <= 25; i++)
        Inverter_Poll300ms(100); /* ind 25 > 24 : RAZ */
    ASSERT_EQ_INT(23, mock_uart.tx_count);
    Inverter_Poll300ms(100); /* nouveau cycle : QMOD */
    ASSERT_EQ_INT(30, mock_uart.tx_count);
}

TEST(polling_inactif_hors_31_199_et_efface_les_mesures)
{
    inverter.moy_ac_output_apparent_power = 1234;
    inverter.ac_output_active_power = 1000;
    inverter.ac_output_voltage = 2300;
    inverter.mode = INVERTER_BATTERY_MODE;
    Inverter_Poll300ms(30);
    ASSERT_EQ_INT(0, mock_uart.tx_count);
    ASSERT_EQ_INT(0, inverter.moy_ac_output_apparent_power);
    ASSERT_EQ_INT(0, inverter.ac_output_active_power);
    ASSERT_EQ_INT(0, inverter.ac_output_voltage);
    ASSERT_EQ_INT(INVERTER_BATTERY_MODE, inverter.mode); /* le mode n'est pas effacé ici */
    Inverter_Poll300ms(200);
    ASSERT_EQ_INT(0, mock_uart.tx_count);
    Inverter_Poll300ms(31);
    ASSERT_EQ_INT(7, mock_uart.tx_count);
}

/* ---- Réception --------------------------------------------------------------- */
TEST(reception_qmod_un_caractere)
{
    poll_until(1); /* QMOD */
    Inverter_OnRxByte('(');
    ASSERT_FALSE(inverter.frame_received);
    Inverter_OnRxByte('B');
    ASSERT_TRUE(inverter.frame_received);
    Inverter_ProcessFrame();
    ASSERT_EQ_INT(INVERTER_BATTERY_MODE, inverter.mode);
    ASSERT_FALSE(inverter.frame_received);
    ASSERT_TRUE(inverter.toggle_qmod_flag);
}

TEST(reception_sans_requete_en_cours_ignoree)
{
    receive("B");
    ASSERT_FALSE(inverter.frame_received);
    Inverter_ProcessFrame();
    ASSERT_EQ_INT(INVERTER_UNDEFINED_MODE, inverter.mode);
}

TEST(reception_nak_abandonne_la_trame)
{
    poll_until(6); /* QPIWS */
    Inverter_OnRxByte('(');
    Inverter_OnRxByte('N');
    Inverter_OnRxByte('A');
    Inverter_OnRxByte('K');
    ASSERT_FALSE(inverter.frame_received);
}

TEST(decode_qmod_toutes_les_lettres)
{
    const struct { char c; uint8_t mode; } cases[] = {
        {'B', INVERTER_BATTERY_MODE}, {'F', INVERTER_FAULT_MODE}, {'H', INVERTER_POWER_SAVING_MODE},
        {'L', INVERTER_AC_SOURCE_MODE}, {'S', INVERTER_STANDBY_MODE}, {'P', INVERTER_POWER_ON_MODE}};
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        Inverter_Init();
        poll_until(1);
        receive(&cases[i].c);
        Inverter_ProcessFrame();
        ASSERT_EQ_INT(cases[i].mode, inverter.mode);
    }
    /* lettre inconnue : mode inchangé */
    Inverter_Init();
    inverter.mode = INVERTER_STANDBY_MODE;
    poll_until(1);
    receive("X");
    Inverter_ProcessFrame();
    ASSERT_EQ_INT(INVERTER_STANDBY_MODE, inverter.mode);
}

TEST(qmod_remet_les_watchdogs_a_zero)
{
    int i;
    for (i = 0; i < 6; i++)
        Inverter_Update1s(100);
    ASSERT_TRUE(inverter.recieve_flag);
    poll_until(1);
    receive("B");
    Inverter_ProcessFrame();
    ASSERT_FALSE(inverter.recieve_flag);
    for (i = 0; i < 5; i++)
        Inverter_Update1s(100);
    ASSERT_FALSE(inverter.recieve_flag); /* compteur reparti de 0 */
}

TEST(decode_qpiws_bits_et_erreurs_derivees)
{
    char f[33];
    int i;
    for (i = 0; i < 32; i++)
        f[i] = '0';
    f[32] = 0;
    f[1] = '1';  /* inverter error */
    f[9] = '1';  /* overtemperature warning -> error car inverter error */
    f[10] = '1'; /* fan locked warning -> error */
    f[16] = '1'; /* overload warning -> error */
    f[29] = '1'; /* battery too low to charge */
    poll_until(6);
    receive(f);
    ASSERT_TRUE(inverter.frame_received);
    Inverter_ProcessFrame();
    ASSERT_TRUE(inverter.error_flag);
    ASSERT_TRUE(inverter.overtemperature_warning_flag);
    ASSERT_TRUE(inverter.overtemperature_error_flag);
    ASSERT_TRUE(inverter.fan_locked_error_flag);
    ASSERT_TRUE(inverter.overload_error_flag);
    ASSERT_TRUE(inverter.battery_too_low_to_charge_warning_flag);
    ASSERT_FALSE(inverter.bus_overvoltage_error_flag);
    ASSERT_TRUE(inverter.toggle_qpiws_flag);

    /* Sans erreur onduleur : les erreurs dérivées ne sont pas recalculées (restent à 1, origine) */
    f[1] = '0';
    f[9] = '0';
    Inverter_Init();
    inverter.overtemperature_error_flag = 1;
    poll_until(6);
    receive(f);
    Inverter_ProcessFrame();
    ASSERT_FALSE(inverter.error_flag);
    ASSERT_FALSE(inverter.overtemperature_warning_flag);
    ASSERT_TRUE(inverter.overtemperature_error_flag);
}

TEST(decode_qpiws_overload_error_si_surcharge_smb)
{
    char f[33];
    int i;
    for (i = 0; i < 32; i++)
        f[i] = '0';
    f[32] = 0;
    f[16] = '1';
    inverter.apparent_power_very_high_error_flag = 1;
    poll_until(6);
    receive(f);
    Inverter_ProcessFrame();
    ASSERT_FALSE(inverter.error_flag);
    ASSERT_TRUE(inverter.overload_error_flag);
}

TEST(decode_qpigs_champs)
{
    qpigs_default();
    /* tension sortie 230.1 V aux positions 11,12,13 . 15 */
    qpigs[11] = '2'; qpigs[12] = '3'; qpigs[13] = '0'; qpigs[15] = '1';
    /* puissance apparente 5234 VA : 22..25 */
    qpigs[22] = '5'; qpigs[23] = '2'; qpigs[24] = '3'; qpigs[25] = '4';
    /* puissance active 4100 W : 27..30 */
    qpigs[27] = '4'; qpigs[28] = '1'; qpigs[29] = '0'; qpigs[30] = '0';
    /* température 0045 : 54..57 */
    qpigs[54] = '0'; qpigs[55] = '0'; qpigs[56] = '4'; qpigs[57] = '5';
    qpigs[85] = '1'; qpigs[87] = '1'; qpigs[88] = '0'; qpigs[89] = '1'; qpigs[104] = '1'; qpigs[105] = '1';

    poll_until(13);
    receive(qpigs);
    ASSERT_TRUE(inverter.frame_received);
    Inverter_ProcessFrame();
    ASSERT_EQ_INT(2301, inverter.ac_output_voltage);
    ASSERT_EQ_INT(5234, inverter.ac_output_apparent_power);
    ASSERT_EQ_INT(4100, inverter.ac_output_active_power);
    ASSERT_EQ_INT(45, inverter.dcac_temp);
    ASSERT_TRUE(inverter.load_status_flag);
    ASSERT_TRUE(inverter.charging_status_flag);
    ASSERT_FALSE(inverter.scc_charging_status_flag);
    ASSERT_TRUE(inverter.ac_charging_status_flag);
    ASSERT_TRUE(inverter.charging_to_floating_mode_flag);
    ASSERT_TRUE(inverter.device_switched_on_flag);
    ASSERT_TRUE(inverter.toggle_qpigs_flag);
}

TEST(qpigs_incomplete_non_livree)
{
    qpigs_default();
    qpigs[105] = 0; /* 105 caractères seulement */
    poll_until(13);
    receive(qpigs);
    ASSERT_FALSE(inverter.frame_received);
}

TEST(reception_longue_sans_fin_ne_deborde_pas)
{
    int i;
    poll_until(13);
    Inverter_OnRxByte('(');
    for (i = 0; i < 300; i++)
        Inverter_OnRxByte('0');
    ASSERT_TRUE(inverter.frame_received);
}

/* ---- Moyenne et supervisions -------------------------------------------------- */
TEST(moyenne_puissance_sur_10_et_avertissement_5000)
{
    int i;
    inverter.ac_output_apparent_power = 5000;
    for (i = 0; i < 9; i++)
        Inverter_UpdatePowerAverage100ms();
    ASSERT_EQ_INT(0, inverter.moy_ac_output_apparent_power);
    ASSERT_FALSE(inverter.apparent_power_high_warning_flag);
    Inverter_UpdatePowerAverage100ms();
    ASSERT_EQ_INT(5000, inverter.moy_ac_output_apparent_power);
    ASSERT_TRUE(inverter.apparent_power_high_warning_flag);
    inverter.ac_output_apparent_power = 4990;
    for (i = 0; i < 10; i++)
        Inverter_UpdatePowerAverage100ms();
    ASSERT_EQ_INT(4990, inverter.moy_ac_output_apparent_power);
    ASSERT_FALSE(inverter.apparent_power_high_warning_flag);
}

TEST(moyenne_puissance_current_behaviour_somme_16_bits)
{
    /* TODO(bug 11) : 10 x 7000 = 70000 déborde 16 bits -> 4464 / 10 = 446 */
    int i;
    inverter.ac_output_apparent_power = 7000;
    for (i = 0; i < 10; i++)
        Inverter_UpdatePowerAverage100ms();
    ASSERT_EQ_INT(446, inverter.moy_ac_output_apparent_power);
}

TEST(surcharge_5200_pendant_90_ticks)
{
    int i;
    inverter.moy_ac_output_apparent_power = 5201;
    for (i = 0; i < 90; i++)
        Inverter_Supervise100ms(50);
    ASSERT_FALSE(inverter.apparent_power_very_high_error_flag);
    Inverter_Supervise100ms(50);
    ASSERT_TRUE(inverter.apparent_power_very_high_error_flag);
}

TEST(surcharge_critique_6000_meme_temporisation_que_5200)
{
    /* 6000 > 5200 mais pas > 7200 : c'est la temporisation 90 qui s'applique */
    int i;
    inverter.moy_ac_output_apparent_power = 6000;
    for (i = 0; i < 90; i++)
        Inverter_Supervise100ms(50);
    ASSERT_FALSE(inverter.apparent_power_very_high_error_flag);
    Inverter_Supervise100ms(50);
    ASSERT_TRUE(inverter.apparent_power_very_high_error_flag);
}

TEST(surcharge_compteur_non_remis_a_zero_sous_5000_current_behaviour)
{
    int i;
    inverter.moy_ac_output_apparent_power = 5201;
    for (i = 0; i < 60; i++)
        Inverter_Supervise100ms(50);
    inverter.moy_ac_output_apparent_power = 100; /* retour à une charge faible */
    for (i = 0; i < 100; i++)
        Inverter_Supervise100ms(50);
    inverter.moy_ac_output_apparent_power = 5201;
    for (i = 0; i < 31; i++)
        Inverter_Supervise100ms(50); /* 60 + 31 = 91 > 90 */
    ASSERT_TRUE(inverter.apparent_power_very_high_error_flag);
}

TEST(surcharge_compteur_raz_entre_5000_et_5200)
{
    int i;
    inverter.moy_ac_output_apparent_power = 5201;
    for (i = 0; i < 60; i++)
        Inverter_Supervise100ms(50);
    inverter.moy_ac_output_apparent_power = 5100;
    Inverter_Supervise100ms(50);
    inverter.moy_ac_output_apparent_power = 5201;
    for (i = 0; i < 90; i++)
        Inverter_Supervise100ms(50);
    ASSERT_FALSE(inverter.apparent_power_very_high_error_flag);
}

TEST(deconnexion_soc_bas_avec_charge)
{
    int i;
    inverter.moy_ac_output_apparent_power = 51;
    for (i = 0; i < 49; i++)
        Inverter_Supervise100ms(19);
    ASSERT_FALSE(inverter.soc_disconnect_error_flag);
    Inverter_Supervise100ms(19);
    ASSERT_TRUE(inverter.soc_disconnect_error_flag);
    /* maintenue tant que SOC < 25 même si la charge disparaît */
    inverter.moy_ac_output_apparent_power = 0;
    Inverter_Supervise100ms(22);
    ASSERT_TRUE(inverter.soc_disconnect_error_flag);
    /* levée quand le SOC remonte au niveau bas */
    Inverter_Supervise100ms(25);
    ASSERT_FALSE(inverter.soc_disconnect_error_flag);
}

TEST(deconnexion_soc_pas_sans_charge)
{
    int i;
    inverter.moy_ac_output_apparent_power = 50;
    for (i = 0; i < 100; i++)
        Inverter_Supervise100ms(10);
    ASSERT_FALSE(inverter.soc_disconnect_error_flag);
}

TEST(watchdogs_1s_actifs_en_mode_batterie_ou_hybride)
{
    int i;
    for (i = 0; i < 200; i++)
        Inverter_Update1s(30);
    ASSERT_FALSE(inverter.watchdog_error_flag);
    ASSERT_FALSE(inverter.recieve_flag);
    for (i = 0; i < 5; i++)
        Inverter_Update1s(150);
    ASSERT_FALSE(inverter.recieve_flag);
    Inverter_Update1s(150);
    ASSERT_TRUE(inverter.recieve_flag);
    for (i = 6; i < 180; i++)
        Inverter_Update1s(100);
    ASSERT_FALSE(inverter.watchdog_error_flag);
    Inverter_Update1s(100);
    ASSERT_TRUE(inverter.watchdog_error_flag);
}

TEST(erreur_generale_verrouillee)
{
    Inverter_UpdateGeneralError(false);
    ASSERT_FALSE(inverter.general_error_flag);
    Inverter_UpdateGeneralError(true);
    ASSERT_TRUE(inverter.general_error_flag);
    Inverter_UpdateGeneralError(false);
    ASSERT_TRUE(inverter.general_error_flag);
    Inverter_ResetErrors();
    ASSERT_FALSE(inverter.general_error_flag);
    inverter.error_flag = 1;
    Inverter_UpdateGeneralError(false);
    ASSERT_TRUE(inverter.general_error_flag);
}

TEST(clear_data_et_clear_data_after_stop)
{
    inverter.mode = INVERTER_BATTERY_MODE;
    inverter.moy_ac_output_apparent_power = 1;
    inverter.ac_output_active_power = 2;
    inverter.ac_output_voltage = 3;
    inverter.ac_output_apparent_power = 4;
    Inverter_ClearData();
    ASSERT_EQ_INT(0, inverter.mode);
    ASSERT_EQ_INT(0, inverter.moy_ac_output_apparent_power);
    ASSERT_EQ_INT(0, inverter.ac_output_active_power);
    ASSERT_EQ_INT(0, inverter.ac_output_voltage);
    ASSERT_EQ_INT(4, inverter.ac_output_apparent_power); /* non effacé (origine) */
    ASSERT_FALSE(inverter.raz_data_after_stop_flag);
    Inverter_ClearDataAfterStop();
    ASSERT_TRUE(inverter.raz_data_after_stop_flag);
}

TEST(reset_errors)
{
    inverter.watchdog_error_flag = 1;
    inverter.error_flag = 1;
    inverter.apparent_power_very_high_error_flag = 1;
    inverter.general_error_flag = 1;
    inverter.overload_error_flag = 1;
    Inverter_ResetErrors();
    ASSERT_FALSE(inverter.watchdog_error_flag);
    ASSERT_FALSE(inverter.error_flag);
    ASSERT_FALSE(inverter.apparent_power_very_high_error_flag);
    ASSERT_FALSE(inverter.general_error_flag);
    ASSERT_TRUE(inverter.overload_error_flag); /* détail QPIWS non concerné */
}

TEST_MAIN_BEGIN()
    RUN_TEST(polling_300ms_sequence_et_octets_exacts);
    RUN_TEST(polling_inactif_hors_31_199_et_efface_les_mesures);
    RUN_TEST(reception_qmod_un_caractere);
    RUN_TEST(reception_sans_requete_en_cours_ignoree);
    RUN_TEST(reception_nak_abandonne_la_trame);
    RUN_TEST(decode_qmod_toutes_les_lettres);
    RUN_TEST(qmod_remet_les_watchdogs_a_zero);
    RUN_TEST(decode_qpiws_bits_et_erreurs_derivees);
    RUN_TEST(decode_qpiws_overload_error_si_surcharge_smb);
    RUN_TEST(decode_qpigs_champs);
    RUN_TEST(qpigs_incomplete_non_livree);
    RUN_TEST(reception_longue_sans_fin_ne_deborde_pas);
    RUN_TEST(moyenne_puissance_sur_10_et_avertissement_5000);
    RUN_TEST(moyenne_puissance_current_behaviour_somme_16_bits);
    RUN_TEST(surcharge_5200_pendant_90_ticks);
    RUN_TEST(surcharge_critique_6000_meme_temporisation_que_5200);
    RUN_TEST(surcharge_compteur_non_remis_a_zero_sous_5000_current_behaviour);
    RUN_TEST(surcharge_compteur_raz_entre_5000_et_5200);
    RUN_TEST(deconnexion_soc_bas_avec_charge);
    RUN_TEST(deconnexion_soc_pas_sans_charge);
    RUN_TEST(watchdogs_1s_actifs_en_mode_batterie_ou_hybride);
    RUN_TEST(erreur_generale_verrouillee);
    RUN_TEST(clear_data_et_clear_data_after_stop);
    RUN_TEST(reset_errors);
TEST_MAIN_END()
