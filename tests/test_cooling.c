/**
 * @file test_cooling.c
 * @brief Tests du refroidissement AC (src/control/cooling.c).
 */
#include "test_framework.h"
#include "src/control/cooling.h"
#include "hal/board_io.h"

static void test_setup(void)
{
    MockBoard_Reset();
    Cooling_Init();
}

TEST(temperatures_basses_duty_minimal_15)
{
    Cooling_Update1s(20, 30, 30);
    ASSERT_EQ_INT(0, cooling.ambiant_pwm);
    ASSERT_EQ_INT(0, cooling.inverter_pwm);
    ASSERT_EQ_INT(0, cooling.dcdc_pwm);
    ASSERT_EQ_INT(15, cooling.fans_duty_cycle);
    ASSERT_EQ_INT(15, mock_board.fans_duty_cycle);
    ASSERT_EQ_INT(1, mock_board.fans_set_calls);
}

TEST(cibles_40_55_45)
{
    /* +10 °C au-dessus de chaque cible : P = 10, I = 1 -> 11 % */
    Cooling_Update1s(50, 65, 55);
    ASSERT_EQ_INT(11, cooling.ambiant_pwm);
    ASSERT_EQ_INT(11, cooling.inverter_pwm);
    ASSERT_EQ_INT(11, cooling.dcdc_pwm);
    ASSERT_EQ_INT(15, cooling.fans_duty_cycle); /* 11 < minimum 15 */
}

TEST(la_demande_la_plus_forte_pilote_les_ventilateurs)
{
    Cooling_Update1s(20, 85, 30); /* onduleur +30 : P = 30, I = 3 -> 33 */
    ASSERT_EQ_INT(33, cooling.inverter_pwm);
    ASSERT_EQ_INT(33, cooling.fans_duty_cycle);
    Cooling_Update1s(80, 30, 30); /* ambiance +40 : P = 40, I = -2 + 4 = 2 -> 42 ; onduleur retombe */
    ASSERT_EQ_INT(42, cooling.ambiant_pwm);
    ASSERT_EQ_INT(42, cooling.fans_duty_cycle);
}

TEST(saturation_a_100_et_integrale_bornee)
{
    int i;
    for (i = 0; i < 2000; i++)
        Cooling_Update1s(20, 30, 100); /* DC/DC +55 */
    ASSERT_EQ_INT(100, cooling.dcdc_pwm);
    ASSERT_EQ_INT(100, cooling.fans_duty_cycle);
    ASSERT_EQ_INT(100, mock_board.fans_duty_cycle);
    /* retour à froid : l'intégrale (bornée à 100/Ki) redescend, la sortie finit à 0 */
    for (i = 0; i < 2000; i++)
        Cooling_Update1s(20, 30, 30);
    ASSERT_EQ_INT(0, cooling.dcdc_pwm);
    ASSERT_EQ_INT(15, cooling.fans_duty_cycle);
}

TEST(temperature_ambiante_signee)
{
    Cooling_Update1s(-20, 30, 30);
    ASSERT_EQ_INT(0, cooling.ambiant_pwm);
    ASSERT_EQ_INT(15, cooling.fans_duty_cycle);
}

TEST_MAIN_BEGIN()
    RUN_TEST(temperatures_basses_duty_minimal_15);
    RUN_TEST(cibles_40_55_45);
    RUN_TEST(la_demande_la_plus_forte_pilote_les_ventilateurs);
    RUN_TEST(saturation_a_100_et_integrale_bornee);
    RUN_TEST(temperature_ambiante_signee);
TEST_MAIN_END()
