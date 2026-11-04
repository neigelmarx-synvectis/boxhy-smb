/**
 * @file test_fcs.c
 * @brief Tests du module pile à combustible (src/fcs.c).
 */
#include "test_framework.h"
#include "fcs.h"
#include "smb_config.h"
#include "hal/can_port.h"

static uint8_t rx_data[8];
static CAN_MSG_OBJ rx;

static void test_setup(void)
{
    MockCan_Reset();
    Fcs_Init();
    memset(rx_data, 0, sizeof(rx_data));
    rx.data = rx_data;
    rx.field.dlc = 8;
}

static void frame(uint32_t id, const uint8_t *d)
{
    rx.msgId = id;
    memcpy(rx_data, d, 8);
}

TEST(trame_etat_0x2FC0000)
{
    const uint8_t d[8] = {FCCB_OPERATING, 0x05, 0x06, 0, 0, 0x01, 0x34, 0x12};
    fcs.watchdog.cnt = 7;
    fcs.watchdog.error = 1;
    frame(0x2FC0000, d);
    ASSERT_TRUE(Fcs_HandleCanFrame(&rx));
    ASSERT_EQ_INT(FCCB_OPERATING, fcs.step);
    ASSERT_TRUE(fcs.critical_error_flag);   /* d1 b0 */
    ASSERT_FALSE(fcs.minor_error_flag);     /* d1 b1 */
    ASSERT_TRUE(fcs.ppmh2_error_flag);      /* d1 b2 */
    ASSERT_TRUE(fcs.low_current_warning_flag);        /* d2 b1 */
    ASSERT_TRUE(fcs.dcdc_temperature_warning_flag);   /* d2 b2 */
    ASSERT_TRUE(fcs.short_circuit_ongoing_flag);      /* d5 b0 */
    ASSERT_EQ_INT(0x1234, fcs.output_current);
    ASSERT_EQ_INT(0, fcs.watchdog.cnt);
    ASSERT_FALSE(fcs.watchdog.error);
}

TEST(courant_pile_16_bits_signe)
{
    const uint8_t d[8] = {0, 0, 0, 0, 0, 0, 0x18, 0xFC}; /* 0xFC18 = -1000 */
    frame(0x2FC0000, d);
    Fcs_HandleCanFrame(&rx);
    ASSERT_EQ_INT(-1000, fcs.output_current);
}

TEST(trames_0x2FC0003_et_0x2FC0004)
{
    const uint8_t d3[8] = {0, 0, 0, 47, 0, 0, 0, 0};
    const uint8_t d4[8] = {0, 0, 0, 0, 0, 0, 9, 0};
    frame(0x2FC0003, d3);
    ASSERT_TRUE(Fcs_HandleCanFrame(&rx));
    ASSERT_EQ_INT(47, fcs.dcdc_temperature);
    frame(0x2FC0004, d4);
    ASSERT_TRUE(Fcs_HandleCanFrame(&rx));
    ASSERT_EQ_INT(9, fcs.short_circuit_interval_cnt);
}

TEST(id_inconnu_non_traite)
{
    const uint8_t d[8] = {0};
    frame(0x2FC0001, d);
    ASSERT_FALSE(Fcs_HandleCanFrame(&rx));
    frame(0x18FF02D0, d);
    ASSERT_FALSE(Fcs_HandleCanFrame(&rx));
}

TEST(watchdog_30s)
{
    int i;
    for (i = 0; i < 30; i++)
        Fcs_Update1s();
    ASSERT_FALSE(fcs.watchdog.error);
    Fcs_Update1s();
    ASSERT_TRUE(fcs.watchdog.error);
    ASSERT_EQ_INT(0, fcs.watchdog.cnt);
}

TEST(erreur_generale_agregee)
{
    Fcs_UpdateGeneralError();
    ASSERT_FALSE(fcs.general_error_flag);
    fcs.minor_error_flag = 1;
    Fcs_UpdateGeneralError();
    ASSERT_TRUE(fcs.general_error_flag);
    fcs.minor_error_flag = 0;
    fcs.watchdog.error = 1;
    Fcs_UpdateGeneralError();
    ASSERT_TRUE(fcs.general_error_flag);
    fcs.watchdog.error = 0;
    fcs.startup_error_flag = 1;
    Fcs_UpdateGeneralError();
    ASSERT_TRUE(fcs.general_error_flag);
    fcs.startup_error_flag = 0;
    fcs.ppmh2_error_flag = 1; /* n'entre pas dans l'erreur générale */
    Fcs_UpdateGeneralError();
    ASSERT_FALSE(fcs.general_error_flag);
}

TEST(timeout_courant_bas_current_behaviour_jamais_atteint)
{
    /* TODO(bug 10) : compteur 8 bits, timeout 600 */
    int i;
    fcs.output_current = 500;
    for (i = 0; i < 5000; i++)
        Fcs_Update100ms(false);
    ASSERT_FALSE(fcs.low_current_timeout.error);
}

TEST(timeout_courant_bas_compteur_raz_si_courant_ok)
{
    fcs.output_current = 500;
    Fcs_Update100ms(false);
    Fcs_Update100ms(false);
    ASSERT_EQ_INT(2, fcs.low_current_timeout.cnt);
    fcs.output_current = 1000; /* pas < 1000 */
    Fcs_Update100ms(false);
    ASSERT_EQ_INT(0, fcs.low_current_timeout.cnt);
}

TEST(timeout_start_2_apres_600_ticks)
{
    int i;
    for (i = 0; i < 600; i++)
        Fcs_Update100ms(true);
    ASSERT_FALSE(fcs.start_2_timeout.error);
    Fcs_Update100ms(true);
    ASSERT_TRUE(fcs.start_2_timeout.error);
    ASSERT_EQ_INT(0, fcs.start_2_timeout.cnt);
    /* hors de l'état : compteur remis à zéro */
    fcs.start_2_timeout.cnt = 100;
    Fcs_Update100ms(false);
    ASSERT_EQ_INT(0, fcs.start_2_timeout.cnt);
}

static void check_cmd(uint32_t id, uint8_t dlc, const uint8_t *expected)
{
    const sMockCanMsg *m = MockCan_LastTx();
    ASSERT_TRUE(m != NULL);
    if (!m) return;
    ASSERT_EQ_HEX(id, m->id);
    ASSERT_EQ_INT(dlc, m->dlc);
    ASSERT_EQ_INT(CAN_PRIORITY_HIGH, m->priority);
    ASSERT_EQ_INT(CAN_FRAME_EXT, m->idType);
    ASSERT_EQ_INT(CAN_FRAME_DATA, m->frameType);
    ASSERT_EQ_MEM(expected, m->data, dlc);
}

TEST(commandes)
{
    const uint8_t start[3] = {0, 0, 1};
    const uint8_t stop[3] = {0, 1, 0};
    const uint8_t critical[3] = {1, 0, 0};
    const uint8_t maint[8] = {0, 0, 0x82, 0, 0, 0, 0, 0xFF};
    const uint8_t reset[8] = {0, 0, 0, 0x01, 0, 0, 0, 0xFF};
    const uint8_t limit[2] = {0x7C, 0x15}; /* 5500 */

    Fcs_SendStart();          check_cmd(0x2FC1001, 3, start);
    Fcs_SendStop();           check_cmd(0x2FC1001, 3, stop);
    Fcs_SendCriticalStop();   check_cmd(0x2FC1001, 3, critical);
    Fcs_SendMaintenanceMode();check_cmd(0x2FC1001, 8, maint);
    Fcs_SendResetErrors();    check_cmd(0x2FC1001, 8, reset);
    Fcs_SendCurrentLimit(5500); check_cmd(0x2FC1002, 2, limit);
    ASSERT_EQ_INT(6, mock_can.tx_count);
}

TEST(reset_errors_envoie_la_commande_et_efface_les_flags)
{
    fcs.startup_error_flag = 1;
    fcs.critical_error_flag = 1;
    fcs.minor_error_flag = 1;
    fcs.watchdog.error = 1;
    fcs.general_error_flag = 1;
    fcs.ppmh2_error_flag = 1;
    fcs.startup_retry_flag = 1;
    fcs.start_2_timeout.error = 1;

    Fcs_ResetErrors();
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_HEX(0x2FC1001, MockCan_LastTx()->id);
    ASSERT_EQ_INT(0x01, MockCan_LastTx()->data[3]);
    ASSERT_FALSE(fcs.startup_error_flag);
    ASSERT_FALSE(fcs.critical_error_flag);
    ASSERT_FALSE(fcs.minor_error_flag);
    ASSERT_FALSE(fcs.watchdog.error);
    ASSERT_FALSE(fcs.general_error_flag);
    /* non concernés par Fcs_ResetErrors */
    ASSERT_TRUE(fcs.ppmh2_error_flag);
    ASSERT_TRUE(fcs.startup_retry_flag);
    ASSERT_TRUE(fcs.start_2_timeout.error);

    Fcs_ResetAllErrors();
    ASSERT_EQ_INT(2, mock_can.tx_count);
    ASSERT_FALSE(fcs.ppmh2_error_flag);
    ASSERT_FALSE(fcs.startup_retry_flag);
    ASSERT_FALSE(fcs.start_2_timeout.error);
}

TEST_MAIN_BEGIN()
    RUN_TEST(trame_etat_0x2FC0000);
    RUN_TEST(courant_pile_16_bits_signe);
    RUN_TEST(trames_0x2FC0003_et_0x2FC0004);
    RUN_TEST(id_inconnu_non_traite);
    RUN_TEST(watchdog_30s);
    RUN_TEST(erreur_generale_agregee);
    RUN_TEST(timeout_courant_bas_current_behaviour_jamais_atteint);
    RUN_TEST(timeout_courant_bas_compteur_raz_si_courant_ok);
    RUN_TEST(timeout_start_2_apres_600_ticks);
    RUN_TEST(commandes);
    RUN_TEST(reset_errors_envoie_la_commande_et_efface_les_flags);
TEST_MAIN_END()
