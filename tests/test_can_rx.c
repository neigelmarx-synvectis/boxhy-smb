/**
 * @file test_can_rx.c
 * @brief Tests de l'aiguillage des trames CAN reçues (src/can/can_rx.c).
 */
#include "test_framework.h"
#include "src/can/can_rx.h"
#include "src/devices/battery.h"
#include "src/devices/fcs.h"
#include "src/hmi/maintenance.h"
#include "src/hmi/hmi.h"
#include "src/system/smb_state_machine.h"
#include "hal/can_port.h"
#include "hal/board_io.h"

static void test_setup(void)
{
    MockCan_Reset();
    MockBoard_Reset();
    Battery_Init();
    Fcs_Init();
    Maintenance_Init();
    Hmi_Init();
    Smb_Init();
    CanRx_Init();
}

TEST(rien_a_lire)
{
    CanRx_Process();
    ASSERT_EQ_INT(0, battery.ibs[0].soc);
}

TEST(trame_batterie_aiguillee)
{
    const uint8_t d[8] = {0x27, 0x10, 0, 0, 0, 0, 0, 0};
    MockCan_PushRx(0x18FF05D0, 8, d);
    ASSERT_EQ_INT(1, CanPort_ReceivedCount());
    CanRx_Process();
    ASSERT_EQ_INT(100, battery.ibs[0].soc);
    ASSERT_EQ_INT(0, CanPort_ReceivedCount());
}

TEST(trame_fccb_aiguillee)
{
    const uint8_t d[8] = {FCCB_OPERATING, 0, 0, 0, 0, 0, 0, 0};
    MockCan_PushRx(0x2FC0000, 8, d);
    CanRx_Process();
    ASSERT_EQ_INT(FCCB_OPERATING, fcs.step);
}

TEST(trame_maintenance_aiguillee_avec_l_etat_courant)
{
    const uint8_t d[8] = {0x82, 1, 0, 0, 0, 0, 0, 0};
    smb.step = SMB_MAINTENANCE;
    MockCan_PushRx(0x2C01D05, 8, d);
    CanRx_Process();
    ASSERT_TRUE(mock_board.buzzer);
    smb.step = SMB_STANDBY;
    MockCan_PushRx(0x2C01D05, 8, d);
    CanRx_Process();
    ASSERT_TRUE(mock_board.buzzer); /* inchangé hors maintenance */
}

TEST(ordre_de_reset_en_mode_defaut_seulement)
{
    const uint8_t ok[8] = {0x82, 0, 0, 0, 0, 0, 0, 0};
    smb.step = SMB_STANDBY;
    MockCan_PushRx(0x2C01009, 8, ok);
    CanRx_Process();
    ASSERT_FALSE(smb.reset_order_flag);
    smb.step = SMB_EMERGENCY;
    MockCan_PushRx(0x2C01009, 8, ok);
    CanRx_Process();
    ASSERT_TRUE(smb.reset_order_flag);
    /* toute autre trame 0x2C01009 annule l'ordre */
    const uint8_t ko[8] = {0x00, 0, 0, 0, 0, 0, 0, 0};
    MockCan_PushRx(0x2C01009, 8, ko);
    CanRx_Process();
    ASSERT_FALSE(smb.reset_order_flag);
}

TEST(raz_erreur_ihm)
{
    const uint8_t one[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    hmi.raz_erreur_ihm_flag = 1;
    MockCan_PushRx(0x25C1000, 8, zero);
    CanRx_Process();
    ASSERT_TRUE(hmi.raz_erreur_ihm_flag);
    MockCan_PushRx(0x25C1000, 8, one);
    CanRx_Process();
    ASSERT_FALSE(hmi.raz_erreur_ihm_flag);
}

TEST(current_behaviour_0x2C01009_avec_octet_1_efface_aussi_raz_ihm)
{
    /* TODO(bug 3) : fall-through 0x2C01009 -> 0x25C1000 */
    const uint8_t one[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    hmi.raz_erreur_ihm_flag = 1;
    smb.step = SMB_EMERGENCY;
    MockCan_PushRx(0x2C01009, 8, one);
    CanRx_Process();
    ASSERT_FALSE(smb.reset_order_flag);
    ASSERT_FALSE(hmi.raz_erreur_ihm_flag);
}

TEST(id_inconnu_ignore)
{
    const uint8_t d[8] = {0x82, 1, 1, 1, 1, 1, 1, 0xFF};
    MockCan_PushRx(0x12345678, 8, d);
    CanRx_Process();
    ASSERT_EQ_INT(0, CanPort_ReceivedCount());
    ASSERT_FALSE(maintenance.mode_request_flag);
}

TEST(une_trame_par_appel)
{
    const uint8_t d[8] = {0x27, 0x10, 0, 0, 0, 0, 0, 0};
    MockCan_PushRx(0x18FF05D0, 8, d);
    MockCan_PushRx(0x18FF05D1, 8, d);
    CanRx_Process();
    ASSERT_EQ_INT(100, battery.ibs[0].soc);
    ASSERT_EQ_INT(0, battery.ibs[1].soc);
    CanRx_Process();
    ASSERT_EQ_INT(100, battery.ibs[1].soc);
}

TEST_MAIN_BEGIN()
    RUN_TEST(rien_a_lire);
    RUN_TEST(trame_batterie_aiguillee);
    RUN_TEST(trame_fccb_aiguillee);
    RUN_TEST(trame_maintenance_aiguillee_avec_l_etat_courant);
    RUN_TEST(ordre_de_reset_en_mode_defaut_seulement);
    RUN_TEST(raz_erreur_ihm);
    RUN_TEST(current_behaviour_0x2C01009_avec_octet_1_efface_aussi_raz_ihm);
    RUN_TEST(id_inconnu_ignore);
    RUN_TEST(une_trame_par_appel);
TEST_MAIN_END()
