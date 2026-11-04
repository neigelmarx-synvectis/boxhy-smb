/**
 * @file test_maintenance.c
 * @brief Tests du mode maintenance (src/maintenance.c).
 */
#include "test_framework.h"
#include "maintenance.h"
#include "hal/board_io.h"

static uint8_t rx_data[8];
static CAN_MSG_OBJ rx;

static void test_setup(void)
{
    MockBoard_Reset();
    Maintenance_Init();
    memset(rx_data, 0, sizeof(rx_data));
    rx.data = rx_data;
    rx.field.dlc = 8;
}

static bool frame(uint32_t id, uint8_t d0, uint8_t d1, uint8_t d7, uint8_t step)
{
    rx.msgId = id;
    rx_data[0] = d0;
    rx_data[1] = d1;
    rx_data[7] = d7;
    return Maintenance_HandleCanFrame(&rx, step);
}

TEST(demande_de_mode_maintenance)
{
    ASSERT_TRUE(frame(0x2C0100F, 0x82, 0, 0xFF, 30));
    ASSERT_TRUE(maintenance.mode_request_flag);
    /* sortie : en maintenance avec la clé */
    ASSERT_TRUE(frame(0x2C0100F, 0x82, 0, 0, 40));
    ASSERT_FALSE(maintenance.mode_request_flag);
}

TEST(demande_refusee_sans_cle_ou_hors_standby)
{
    frame(0x2C0100F, 0x00, 0, 0xFF, 30);
    ASSERT_FALSE(maintenance.mode_request_flag);
    frame(0x2C0100F, 0x82, 0, 0x00, 30);
    ASSERT_FALSE(maintenance.mode_request_flag);
    frame(0x2C0100F, 0x82, 0, 0xFF, 100);
    ASSERT_FALSE(maintenance.mode_request_flag);
    /* une demande de sortie hors maintenance ne change rien */
    maintenance.mode_request_flag = 1;
    frame(0x2C0100F, 0x82, 0, 0, 30);
    ASSERT_TRUE(maintenance.mode_request_flag);
}

TEST(commandes_manuelles_actives_en_maintenance_seulement)
{
    ASSERT_TRUE(frame(0x2C01D01, 0x82, 1, 0, 30)); /* ID traité mais sans effet */
    ASSERT_FALSE(mock_board.dcac_input_relay);
    ASSERT_TRUE(frame(0x2C01D01, 0x00, 1, 0, 40)); /* mauvaise clé */
    ASSERT_FALSE(mock_board.dcac_input_relay);
    ASSERT_TRUE(frame(0x2C01D01, 0x82, 1, 0, 40));
    ASSERT_TRUE(mock_board.dcac_input_relay);
}

TEST(toggle_et_forcage_a_zero)
{
    frame(0x2C01D02, 0x82, 1, 0, 40);
    ASSERT_TRUE(mock_board.dcac_precharge_relay);
    frame(0x2C01D02, 0x82, 1, 0, 40);
    ASSERT_FALSE(mock_board.dcac_precharge_relay);
    frame(0x2C01D02, 0x82, 1, 0, 40);
    ASSERT_TRUE(mock_board.dcac_precharge_relay);
    frame(0x2C01D02, 0x82, 0, 0, 40);
    ASSERT_FALSE(mock_board.dcac_precharge_relay);
    frame(0x2C01D02, 0x82, 5, 0, 40); /* valeur autre que 1 : forçage à 0 */
    ASSERT_FALSE(mock_board.dcac_precharge_relay);
}

TEST(chaque_sortie_est_adressee)
{
    frame(0x2C01D03, 0x82, 1, 0, 40); ASSERT_TRUE(mock_board.supply_precharge_relay);
    frame(0x2C01D04, 0x82, 1, 0, 40); ASSERT_TRUE(mock_board.fcs_output_relay);
    frame(0x2C01D05, 0x82, 1, 0, 40); ASSERT_TRUE(mock_board.buzzer);
    frame(0x2C01D06, 0x82, 1, 0, 40); ASSERT_TRUE(mock_board.led_left);
    frame(0x2C01D07, 0x82, 1, 0, 40); ASSERT_TRUE(mock_board.led_middle);
    frame(0x2C01D08, 0x82, 1, 0, 40); ASSERT_TRUE(mock_board.led_right);
    frame(0x2C01D00, 0x82, 73, 0, 40); ASSERT_EQ_INT(73, mock_board.fans_duty_cycle);
    frame(0x2C01D00, 0x82, 150, 0, 40); ASSERT_EQ_INT(100, mock_board.fans_duty_cycle);
}

TEST(ids_inconnus)
{
    ASSERT_FALSE(frame(0x2C01D09, 0x82, 1, 0, 40));
    ASSERT_FALSE(frame(0x2C01009, 0x82, 1, 0, 40));
    ASSERT_FALSE(frame(0x18FF02D0, 0x82, 1, 0, 40));
}

TEST_MAIN_BEGIN()
    RUN_TEST(demande_de_mode_maintenance);
    RUN_TEST(demande_refusee_sans_cle_ou_hors_standby);
    RUN_TEST(commandes_manuelles_actives_en_maintenance_seulement);
    RUN_TEST(toggle_et_forcage_a_zero);
    RUN_TEST(chaque_sortie_est_adressee);
    RUN_TEST(ids_inconnus);
TEST_MAIN_END()
