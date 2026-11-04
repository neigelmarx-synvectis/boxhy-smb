/**
 * @file test_ems.c
 * @brief Tests de la gestion énergétique (src/control/ems.c).
 */
#include "test_framework.h"
#include "src/control/ems.h"
#include "src/devices/fcs.h"
#include "hal/can_port.h"

static void test_setup(void)
{
    MockCan_Reset();
    Fcs_Init();
    Ems_Init();
}

TEST(moyennes_par_blocs_de_10)
{
    int i;
    for (i = 0; i < 9; i++)
        Ems_Update100ms(1000, 2000);
    ASSERT_EQ_INT(0, ems.system_output_current);
    Ems_Update100ms(1000, 2000);
    ASSERT_EQ_INT(1000, ems.battery_current_moy);
    ASSERT_EQ_INT(2000, ems.fcs_current_moy);
    ASSERT_EQ_INT(3000, ems.system_output_current);
    /* bloc suivant indépendant */
    for (i = 0; i < 10; i++)
        Ems_Update100ms(-500, 100);
    ASSERT_EQ_INT(-500, ems.battery_current_moy);
    ASSERT_EQ_INT(100, ems.fcs_current_moy);
    ASSERT_EQ_INT(-400, ems.system_output_current);
}

TEST(hors_mode_hybride_consigne_nulle_sans_commande)
{
    ems.system_output_current = 3000;
    Ems_Update1s(false, false, -3000, -3000);
    ASSERT_EQ_INT(0, ems.requested_fcs_current);
    ASSERT_EQ_INT(0, mock_can.tx_count);
    Ems_Update1s(true, true, -3000, -3000); /* court-circuit */
    ASSERT_EQ_INT(0, ems.requested_fcs_current);
    ASSERT_EQ_INT(0, mock_can.tx_count);
}

TEST(capacite_de_charge_globale)
{
    /* max(-3000, -1000) = -1000 ; * 0.7 * 2 * -1 = 1400 */
    Ems_Update1s(false, false, -3000, -1000);
    ASSERT_EQ_INT(1400, ems.battery_global_max_charge_current);
}

TEST(consigne_courant_systeme_plus_marge_si_capacite_suffisante)
{
    ems.system_output_current = 1000;
    Ems_Update1s(true, false, -2000, -2000); /* capacité 2800 > 2000 */
    ASSERT_EQ_INT(3000, ems.requested_fcs_current);
    ASSERT_EQ_INT(1, mock_can.tx_count);
    ASSERT_EQ_HEX(0x2FC1002, MockCan_LastTx()->id);
    ASSERT_EQ_INT(0xB8, MockCan_LastTx()->data[0]);
    ASSERT_EQ_INT(0x0B, MockCan_LastTx()->data[1]);
}

TEST(consigne_limitee_par_la_capacite_de_charge)
{
    ems.system_output_current = 1000;
    Ems_Update1s(true, false, -1000, -1000); /* capacité 1400 <= 2000 */
    ASSERT_EQ_INT(2400, ems.requested_fcs_current);
}

TEST(consigne_2000_si_courant_systeme_negatif)
{
    ems.system_output_current = -500;
    Ems_Update1s(true, false, -2000, -2000);
    ASSERT_EQ_INT(2000, ems.requested_fcs_current);
}

TEST(bornes_800_et_5500)
{
    ems.system_output_current = 0;
    Ems_Update1s(true, false, -100, -100); /* capacité 140 -> 0 + 140 = 140 < 800 */
    ASSERT_EQ_INT(800, ems.requested_fcs_current);
    ems.system_output_current = 6000;
    Ems_Update1s(true, false, -3000, -3000);
    ASSERT_EQ_INT(5500, ems.requested_fcs_current);
}

TEST(capacite_de_charge_positive_current_behaviour)
{
    /* Si l'IBS annonce un courant de charge max positif, la capacité devient négative et la
     * somme non signée déborde : saturation à 5500 (comportement d'origine). */
    ems.system_output_current = 100;
    Ems_Update1s(true, false, 1000, 1000); /* capacité -1400 ; 100 + (-1400) -> 64236 */
    ASSERT_EQ_INT(5500, ems.requested_fcs_current);
}

TEST_MAIN_BEGIN()
    RUN_TEST(moyennes_par_blocs_de_10);
    RUN_TEST(hors_mode_hybride_consigne_nulle_sans_commande);
    RUN_TEST(capacite_de_charge_globale);
    RUN_TEST(consigne_courant_systeme_plus_marge_si_capacite_suffisante);
    RUN_TEST(consigne_limitee_par_la_capacite_de_charge);
    RUN_TEST(consigne_2000_si_courant_systeme_negatif);
    RUN_TEST(bornes_800_et_5500);
    RUN_TEST(capacite_de_charge_positive_current_behaviour);
TEST_MAIN_END()
