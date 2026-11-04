/**
 * @file test_smb_state_machine.c
 * @brief Tests de la machine d'états SMB (src/smb_state_machine.c) : transitions, relais, buzzer.
 */
#include "test_framework.h"
#include "smb_state_machine.h"
#include "smb_config.h"
#include "timebase.h"
#include "battery.h"
#include "fcs.h"
#include "inverter.h"
#include "measures.h"
#include "hmi.h"
#include "maintenance.h"
#include "hal/board_io.h"
#include "hal/can_port.h"
#include "hal/adc_port.h"

static void test_setup(void)
{
    MockBoard_Reset();
    MockCan_Reset();
    MockAdc_Reset();
    Timebase_Init();
    Battery_Init();
    Fcs_Init();
    Inverter_Init();
    Measures_Init();
    Hmi_Init();
    Maintenance_Init();
    Smb_Init();
}

static void run(int n)
{
    while (n--)
        Smb_Run();
}

/* Place la machine dans un état donné avec un contexte sain. */
static void enter(uint8_t step)
{
    smb.step = step;
    timebase.ticks_100ms = 0;
}

static void assert_safe_relays(void)
{
    ASSERT_TRUE(mock_board.supply_precharge_relay);
    ASSERT_FALSE(mock_board.dcac_precharge_relay);
    ASSERT_FALSE(mock_board.dcac_input_relay);
    ASSERT_FALSE(mock_board.fcs_output_relay);
}

/* ---- Démarrage ----------------------------------------------------------------- */
TEST(init_puis_autocheck_puis_standby)
{
    ASSERT_EQ_INT(SMB_INIT, smb.step);
    run(1);
    assert_safe_relays();
    timebase.ticks_100ms = 19;
    run(1);
    ASSERT_EQ_INT(SMB_INIT, smb.step);
    timebase.ticks_100ms = 20;
    run(1);
    ASSERT_EQ_INT(SMB_AUTOCHECK, smb.step);
    ASSERT_EQ_INT(0, timebase.ticks_100ms);
    timebase.ticks_100ms = 30;
    run(1);
    ASSERT_EQ_INT(SMB_STANDBY, smb.step);
}

TEST(autocheck_vers_urgence_sur_erreur)
{
    enter(SMB_AUTOCHECK);
    inverter.general_error_flag = 1;
    run(1);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
}

TEST(erreur_critique_agregee_et_verrouillee)
{
    enter(SMB_STANDBY);
    battery.sop_error_flag = 1;
    run(1);
    ASSERT_TRUE(smb.critical_error_flag);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
    battery.sop_error_flag = 0;
    run(1);
    ASSERT_TRUE(smb.critical_error_flag); /* verrouillée */

    Smb_Init();
    enter(SMB_STANDBY);
    fcs.ppmh2_error_flag = 1;
    run(1);
    ASSERT_TRUE(smb.critical_error_flag);
}

TEST(standby_vers_demarrage_onduleur_sur_bouton_batterie)
{
    enter(SMB_STANDBY);
    timebase.ticks_100ms = 7;
    run(1);
    ASSERT_EQ_INT(SMB_STANDBY, smb.step);
    ASSERT_EQ_INT(0, timebase.ticks_100ms); /* chronomètre maintenu à 0 */
    hmi.battery_mode_button.state = 1;
    run(1);
    ASSERT_EQ_INT(SMB_DCAC_START_1, smb.step);
}

TEST(standby_vers_maintenance_envoie_la_commande_fccb)
{
    enter(SMB_STANDBY);
    maintenance.mode_request_flag = 1;
    run(1);
    ASSERT_EQ_INT(SMB_MAINTENANCE, smb.step);
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_HEX(0x2FC1001, MockCan_LastTx()->id);
    ASSERT_EQ_INT(0x82, MockCan_LastTx()->data[2]);
    /* sortie de maintenance : arrêt */
    maintenance.mode_request_flag = 0;
    run(1);
    ASSERT_EQ_INT(SMB_STOPPING, smb.step);
}

TEST(sequence_demarrage_onduleur)
{
    enter(SMB_DCAC_START_1);
    run(1);
    ASSERT_TRUE(mock_board.dcac_precharge_relay);
    ASSERT_EQ_INT(SMB_DCAC_START_1, smb.step);
    timebase.ticks_100ms = 30;
    run(1);
    ASSERT_EQ_INT(SMB_DCAC_START_2, smb.step);
    ASSERT_TRUE(mock_board.dcac_precharge_relay); /* relais encore commandé dans ce tour */
    timebase.ticks_100ms = 4;
    run(1);
    ASSERT_EQ_INT(SMB_DCAC_START_2, smb.step);
    timebase.ticks_100ms = 5;
    run(1);
    ASSERT_EQ_INT(SMB_DCAC_START_3, smb.step);
    ASSERT_FALSE(mock_board.dcac_precharge_relay);
    ASSERT_TRUE(mock_board.dcac_input_relay);
    /* attente du mode onduleur */
    inverter.mode = INVERTER_STANDBY_MODE;
    run(1);
    ASSERT_EQ_INT(SMB_DCAC_START_3, smb.step);
    inverter.mode = INVERTER_BATTERY_MODE;
    run(1);
    ASSERT_EQ_INT(SMB_BATTERY_MODE, smb.step);
}

TEST(demarrage_onduleur_timeout_15s_ou_defaut)
{
    enter(SMB_DCAC_START_3);
    timebase.ticks_100ms = 150;
    run(1);
    ASSERT_EQ_INT(SMB_BATTERY_MODE, smb.step);
    enter(SMB_DCAC_START_3);
    inverter.mode = INVERTER_FAULT_MODE;
    run(1);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
}

/* ---- Mode batterie ------------------------------------------------------------- */
TEST(mode_batterie_relais_et_deconnexion_soc)
{
    enter(SMB_BATTERY_MODE);
    hmi.battery_mode_button.state = 1;
    mock_board.fcs_output_relay = true;
    run(1);
    ASSERT_TRUE(mock_board.dcac_input_relay);
    ASSERT_FALSE(mock_board.fcs_output_relay);
    ASSERT_EQ_INT(SMB_BATTERY_MODE, smb.step);
    inverter.soc_disconnect_error_flag = 1;
    run(1);
    ASSERT_FALSE(mock_board.dcac_input_relay);
    inverter.soc_disconnect_error_flag = 0;
    run(1);
    ASSERT_TRUE(mock_board.dcac_input_relay);
}

TEST(mode_batterie_arret_quand_aucun_bouton)
{
    enter(SMB_BATTERY_MODE);
    run(1);
    ASSERT_EQ_INT(SMB_STOPPING, smb.step);
}

TEST(mode_batterie_vers_hybride_selon_soc_et_erreur_pile)
{
    enter(SMB_BATTERY_MODE);
    hmi.hybrid_mode_button.state = 1;
    battery.soc = 71;
    run(1);
    ASSERT_EQ_INT(SMB_BATTERY_MODE, smb.step); /* SOC trop haut : on reste */
    battery.soc = 70;
    run(1);
    ASSERT_EQ_INT(SMB_FCS_START_1, smb.step);

    enter(SMB_BATTERY_MODE);
    fcs.minor_error_flag = 1; /* -> general error */
    run(1);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
}

TEST(mode_batterie_urgence_sur_surcharge)
{
    enter(SMB_BATTERY_MODE);
    hmi.battery_mode_button.state = 1;
    inverter.apparent_power_very_high_error_flag = 1;
    run(1);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
}

/* ---- Démarrage pile --------------------------------------------------------------- */
TEST(demarrage_pile_quand_fccb_en_standby)
{
    enter(SMB_FCS_START_1);
    fcs.step = FCCB_INIT;
    run(1);
    ASSERT_EQ_INT(SMB_FCS_START_1, smb.step);
    fcs.step = FCCB_STANDBY;
    run(1);
    ASSERT_EQ_INT(SMB_FCS_START_2, smb.step);
    ASSERT_TRUE(mock_board.fcs_output_relay);
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_INT(1, MockCan_LastTx()->data[2]); /* Start */
    fcs.step = FCCB_OPERATING;
    run(1);
    ASSERT_EQ_INT(SMB_HYBRID_MODE, smb.step);
}

TEST(demarrage_pile_echec_attente_standby)
{
    enter(SMB_FCS_START_1);
    fcs.step = FCCB_INIT;
    timebase.ticks_100ms = 30;
    run(1);
    ASSERT_EQ_INT(SMB_FC_STOP_1, smb.step);
    ASSERT_TRUE(fcs.startup_error_flag);
    ASSERT_FALSE(mock_board.fcs_output_relay);
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_INT(1, MockCan_LastTx()->data[1]); /* Stop */
}

TEST(demarrage_pile_etape_2_timeout_puis_retry_puis_urgence)
{
    enter(SMB_FCS_START_2);
    fcs.step = FCCB_RAMP_UP;
    run(1);
    ASSERT_EQ_INT(SMB_FCS_START_2, smb.step);
    fcs.start_2_timeout.error = 1;
    run(1);
    ASSERT_EQ_INT(SMB_FCS_START_1, smb.step);
    ASSERT_TRUE(fcs.startup_retry_flag);
    ASSERT_FALSE(fcs.start_2_timeout.error);
    /* second passage : FCCB standby -> start 2 ; sans OPERATING immédiat -> urgence */
    fcs.step = FCCB_STANDBY;
    run(1);
    ASSERT_EQ_INT(SMB_FCS_START_2, smb.step);
    fcs.step = FCCB_RAMP_UP;
    run(1);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
}

TEST(demarrage_pile_etape_2_erreur_fccb)
{
    enter(SMB_FCS_START_2);
    mock_board.fcs_output_relay = true;
    fcs.step = FCCB_EMERGENCY;
    run(1);
    ASSERT_EQ_INT(SMB_FC_STOP_1, smb.step);
    ASSERT_TRUE(fcs.startup_error_flag);
    ASSERT_FALSE(mock_board.fcs_output_relay);
}

TEST(etape_2_current_behaviour_deconnexion_soc_masque_les_transitions)
{
    /* TODO(bug 7) */
    enter(SMB_FCS_START_2);
    inverter.soc_disconnect_error_flag = 1;
    battery.sop_error_flag = 1; /* -> erreur critique */
    run(1);
    ASSERT_TRUE(smb.critical_error_flag);
    ASSERT_EQ_INT(SMB_FCS_START_2, smb.step);
    ASSERT_FALSE(mock_board.dcac_input_relay);
    inverter.soc_disconnect_error_flag = 0;
    run(1);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
}

/* ---- Mode hybride ----------------------------------------------------------------- */
TEST(mode_hybride_conditions_de_sortie)
{
    enter(SMB_HYBRID_MODE);
    hmi.hybrid_mode_button.state = 1;
    fcs.step = FCCB_OPERATING;
    battery.soc = 50;
    run(1);
    ASSERT_EQ_INT(SMB_HYBRID_MODE, smb.step);
    ASSERT_TRUE(mock_board.dcac_input_relay);

    battery.soc = 85;
    run(1);
    ASSERT_EQ_INT(SMB_FC_STOP_1, smb.step);

    enter(SMB_HYBRID_MODE);
    battery.soc = 50;
    fcs.low_current_timeout.error = 1;
    run(1);
    ASSERT_EQ_INT(SMB_FC_STOP_1, smb.step);
    ASSERT_FALSE(fcs.low_current_timeout.error);

    enter(SMB_HYBRID_MODE);
    hmi.hybrid_mode_button.state = 0;
    run(1);
    ASSERT_EQ_INT(SMB_FC_STOP_1, smb.step);

    enter(SMB_HYBRID_MODE);
    hmi.hybrid_mode_button.state = 1;
    fcs.step = FCCB_SHUTDOWN;
    run(1);
    ASSERT_EQ_INT(SMB_FC_STOP_1, smb.step);
}

/* ---- Arrêt pile ------------------------------------------------------------------- */
TEST(arret_pile_envoie_stop_apres_200ms_puis_attend_standby)
{
    enter(SMB_FC_STOP_1);
    timebase.ticks_100ms = 1;
    run(1);
    ASSERT_EQ_INT(SMB_FC_STOP_1, smb.step);
    timebase.ticks_100ms = 2;
    run(1);
    ASSERT_EQ_INT(SMB_FC_STOP_2, smb.step);
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_INT(1, MockCan_LastTx()->data[1]); /* Stop */

    mock_board.fcs_output_relay = true;
    fcs.step = FCCB_SHUTDOWN;
    run(1);
    ASSERT_FALSE(mock_board.fcs_output_relay);
    ASSERT_EQ_INT(SMB_FC_STOP_2, smb.step);
    fcs.step = FCCB_STANDBY;
    run(1);
    ASSERT_EQ_INT(SMB_BATTERY_MODE, smb.step);
}

TEST(arret_pile_double_bip_pendant_shutdown)
{
    enter(SMB_FC_STOP_2);
    fcs.step = FCCB_SHUTDOWN;
    timebase.ticks_100ms = 25; run(1); ASSERT_TRUE(mock_board.buzzer);
    timebase.ticks_100ms = 27; run(1); ASSERT_FALSE(mock_board.buzzer);
    timebase.ticks_100ms = 35; run(1); ASSERT_TRUE(mock_board.buzzer);
    timebase.ticks_100ms = 40; run(1); ASSERT_FALSE(mock_board.buzzer);
    fcs.step = FCCB_RAMP_UP;
    timebase.ticks_100ms = 25; run(1); ASSERT_FALSE(mock_board.buzzer);
}

TEST(arret_pile_urgence_fccb)
{
    enter(SMB_FC_STOP_2);
    fcs.step = FCCB_EMERGENCY;
    run(1);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
}

/* ---- Arrêt onduleur / arrêt général ----------------------------------------------- */
TEST(arret_onduleur_apres_1_5s)
{
    enter(SMB_DCAC_STOP);
    mock_board.dcac_input_relay = true;
    inverter.mode = INVERTER_BATTERY_MODE;
    run(1);
    ASSERT_FALSE(mock_board.dcac_input_relay);
    timebase.ticks_100ms = 15;
    run(1);
    ASSERT_EQ_INT(SMB_STANDBY, smb.step);
    ASSERT_TRUE(inverter.raz_data_after_stop_flag);
    ASSERT_EQ_INT(0, inverter.mode);
}

TEST(arret_general_sequence_current_behaviour)
{
    int i;
    enter(SMB_STOPPING);
    mock_board.dcac_input_relay = true;
    mock_board.fcs_output_relay = true;
    mock_board.dcac_precharge_relay = true;
    fcs.step = FCCB_STANDBY;

    run(1); /* ticks == 0 : rien */
    ASSERT_TRUE(mock_board.dcac_precharge_relay);

    timebase.ticks_100ms = 1; run(1); /* cnt 1 : stop pile */
    ASSERT_FALSE(mock_board.dcac_precharge_relay);
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_INT(1, MockCan_LastTx()->data[1]);
    ASSERT_EQ_INT(FCCB_OPERATING, fcs.step); /* TODO(bug 1) : état FCCB écrasé */
    ASSERT_EQ_INT(0, timebase.ticks_100ms);

    timebase.ticks_100ms = 1; run(1); /* cnt 2 */
    ASSERT_FALSE(mock_board.fcs_output_relay);
    ASSERT_TRUE(mock_board.dcac_input_relay);
    for (i = 3; i <= 14; i++) { timebase.ticks_100ms = 1; run(1); }
    ASSERT_TRUE(mock_board.dcac_input_relay);
    timebase.ticks_100ms = 1; run(1); /* cnt 15 */
    ASSERT_FALSE(mock_board.dcac_input_relay);
    for (i = 16; i <= 49; i++) { timebase.ticks_100ms = 1; run(1); }
    ASSERT_EQ_INT(SMB_STOPPING, smb.step);
    inverter.mode = INVERTER_BATTERY_MODE;
    timebase.ticks_100ms = 1; run(1); /* cnt 50 */
    ASSERT_EQ_INT(SMB_STANDBY, smb.step);
    ASSERT_TRUE(inverter.raz_data_after_stop_flag);
    ASSERT_EQ_INT(0, inverter.mode);
    ASSERT_EQ_INT(1, mock_can.tx_count); /* un seul stop */
}

TEST(arret_general_urgence_envoie_critical_stop)
{
    enter(SMB_STOPPING);
    battery.overvoltage.error = 1;
    run(1);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_INT(1, MockCan_LastTx()->data[0]); /* CriticalStop */
}

/* ---- Mode défaut --------------------------------------------------------------------- */
TEST(urgence_relais_securite_et_buzzer_alterne)
{
    enter(SMB_EMERGENCY);
    inverter.mode = INVERTER_BATTERY_MODE;
    inverter.moy_ac_output_apparent_power = 100;
    run(1);
    assert_safe_relays();
    ASSERT_EQ_INT(0, inverter.mode);
    ASSERT_EQ_INT(0, inverter.moy_ac_output_apparent_power);
    ASSERT_FALSE(mock_board.buzzer);
    timebase.ticks_100ms = 3;
    run(1);
    ASSERT_TRUE(mock_board.buzzer);
    ASSERT_EQ_INT(0, timebase.ticks_100ms);
    timebase.ticks_100ms = 3;
    run(1);
    ASSERT_FALSE(mock_board.buzzer);
}

TEST(urgence_buzzer_continu_sur_fuite_h2_et_coupe_apres_5_min)
{
    enter(SMB_EMERGENCY);
    fcs.ppmh2_error_flag = 1;
    run(1);
    ASSERT_TRUE(mock_board.buzzer);
    timebase.ticks_100ms = 5;
    run(1);
    ASSERT_TRUE(mock_board.buzzer);
    hmi.buzzer_5_minutes.error = 1;
    run(1);
    ASSERT_FALSE(mock_board.buzzer);
}

TEST(urgence_chronometre_borne_a_12_sans_transition)
{
    enter(SMB_EMERGENCY);
    fcs.ppmh2_error_flag = 1;
    timebase.ticks_100ms = 13;
    run(1);
    ASSERT_EQ_INT(0, timebase.ticks_100ms);
}

TEST(sortie_urgence_par_bouton_central_acquitte_tout)
{
    enter(SMB_EMERGENCY);
    smb.critical_error_flag = 1;
    smb.software_error_flag = 1;
    battery.overvoltage.error = 1;
    battery.ibs[0].error_flag = 1;
    fcs.ppmh2_error_flag = 1;
    fcs.startup_error_flag = 1;
    fcs.startup_retry_flag = 1;
    inverter.watchdog_error_flag = 1;
    inverter.general_error_flag = 1;
    measures.ac_ambiant_temp.error_flag = 1;
    hmi.buzzer_5_minutes.error = 1;
    hmi.middle_button.state = 1;

    run(1);
    ASSERT_EQ_INT(SMB_AUTOCHECK, smb.step);
    ASSERT_FALSE(hmi.middle_button.state);
    ASSERT_FALSE(smb.critical_error_flag);
    ASSERT_FALSE(smb.software_error_flag);
    ASSERT_FALSE(battery.overvoltage.error);
    ASSERT_FALSE(battery.ibs[0].error_flag);
    ASSERT_FALSE(fcs.ppmh2_error_flag);
    ASSERT_FALSE(fcs.startup_error_flag);
    ASSERT_FALSE(fcs.startup_retry_flag);
    ASSERT_FALSE(fcs.general_error_flag);
    ASSERT_FALSE(inverter.watchdog_error_flag);
    ASSERT_FALSE(inverter.general_error_flag);
    ASSERT_FALSE(measures.ac_ambiant_temp.error_flag);
    ASSERT_FALSE(hmi.buzzer_5_minutes.error);
    ASSERT_TRUE(hmi.raz_erreur_ihm_flag);
    ASSERT_EQ_INT(1, mock_can.tx_count); /* commande RAZ erreurs FCCB */
    ASSERT_EQ_INT(0x01, MockCan_LastTx()->data[3]);
    ASSERT_EQ_INT(0, timebase.ticks_100ms);
}

TEST(sortie_urgence_par_ordre_can)
{
    enter(SMB_EMERGENCY);
    Smb_SetResetOrder(true);
    run(1);
    ASSERT_EQ_INT(SMB_AUTOCHECK, smb.step);
    ASSERT_FALSE(smb.reset_order_flag);
}

TEST(etat_inconnu_erreur_logicielle_et_urgence)
{
    enter(77);
    mock_board.dcac_input_relay = true;
    run(1);
    ASSERT_TRUE(smb.software_error_flag);
    ASSERT_EQ_INT(SMB_EMERGENCY, smb.step);
    assert_safe_relays();
    run(1);
    ASSERT_TRUE(smb.critical_error_flag);
}

TEST_MAIN_BEGIN()
    RUN_TEST(init_puis_autocheck_puis_standby);
    RUN_TEST(autocheck_vers_urgence_sur_erreur);
    RUN_TEST(erreur_critique_agregee_et_verrouillee);
    RUN_TEST(standby_vers_demarrage_onduleur_sur_bouton_batterie);
    RUN_TEST(standby_vers_maintenance_envoie_la_commande_fccb);
    RUN_TEST(sequence_demarrage_onduleur);
    RUN_TEST(demarrage_onduleur_timeout_15s_ou_defaut);
    RUN_TEST(mode_batterie_relais_et_deconnexion_soc);
    RUN_TEST(mode_batterie_arret_quand_aucun_bouton);
    RUN_TEST(mode_batterie_vers_hybride_selon_soc_et_erreur_pile);
    RUN_TEST(mode_batterie_urgence_sur_surcharge);
    RUN_TEST(demarrage_pile_quand_fccb_en_standby);
    RUN_TEST(demarrage_pile_echec_attente_standby);
    RUN_TEST(demarrage_pile_etape_2_timeout_puis_retry_puis_urgence);
    RUN_TEST(demarrage_pile_etape_2_erreur_fccb);
    RUN_TEST(etape_2_current_behaviour_deconnexion_soc_masque_les_transitions);
    RUN_TEST(mode_hybride_conditions_de_sortie);
    RUN_TEST(arret_pile_envoie_stop_apres_200ms_puis_attend_standby);
    RUN_TEST(arret_pile_double_bip_pendant_shutdown);
    RUN_TEST(arret_pile_urgence_fccb);
    RUN_TEST(arret_onduleur_apres_1_5s);
    RUN_TEST(arret_general_sequence_current_behaviour);
    RUN_TEST(arret_general_urgence_envoie_critical_stop);
    RUN_TEST(urgence_relais_securite_et_buzzer_alterne);
    RUN_TEST(urgence_buzzer_continu_sur_fuite_h2_et_coupe_apres_5_min);
    RUN_TEST(urgence_chronometre_borne_a_12_sans_transition);
    RUN_TEST(sortie_urgence_par_bouton_central_acquitte_tout);
    RUN_TEST(sortie_urgence_par_ordre_can);
    RUN_TEST(etat_inconnu_erreur_logicielle_et_urgence);
TEST_MAIN_END()
