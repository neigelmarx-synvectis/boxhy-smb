/**
 * @file test_ctn_sensor.c
 * @brief Tests de la sonde CTN (src/measures/ctn_sensor.c).
 */
#include "test_framework.h"
#include "src/measures/ctn_sensor.h"

static sCTN_SENSOR s;

static void test_setup(void)
{
    ctnSensorInit(&s);
}

static void check_n(int n)
{
    while (n--)
        ctnSensorCheck(&s);
}

TEST(init_seuils_par_defaut)
{
    ASSERT_EQ_INT(50, s.warning_high_level);
    ASSERT_EQ_INT(70, s.high_level);
    ASSERT_EQ_INT(-5, s.low_level);
    ASSERT_EQ_INT(0, s.value);
    ASSERT_FALSE(s.error_flag);
}

TEST(conversion_hors_plage)
{
    ASSERT_EQ_HEX(0x7FFF, convertTemperatureUSP10976(0));
    ASSERT_EQ_HEX(0x7FFF, convertTemperatureUSP10976(69));
    ASSERT_EQ_HEX(0x7FFF, convertTemperatureUSP10976(4026));
    ASSERT_EQ_HEX(0x7FFF, convertTemperatureUSP10976(4095));
}

TEST(conversion_bornes_de_segments)
{
    /* valeurs calculées depuis la table d'origine */
    ASSERT_EQ_INT((int16_t)(70 * 16.52 - 6007.0), convertTemperatureUSP10976(70));
    ASSERT_EQ_INT((int16_t)(152 * 16.52 - 6007.0), convertTemperatureUSP10976(152));
    ASSERT_EQ_INT((int16_t)(153 * 7.7 - 4710.0), convertTemperatureUSP10976(153));
    ASSERT_EQ_INT((int16_t)(276 * 7.7 - 4710.0), convertTemperatureUSP10976(276));
    ASSERT_EQ_INT((int16_t)(470 * 4.94 - 3909.0), convertTemperatureUSP10976(470));
    ASSERT_EQ_INT((int16_t)(686 * 3.57 - 3245.0), convertTemperatureUSP10976(686));
    ASSERT_EQ_INT((int16_t)(960 * 2.88 - 2760.0), convertTemperatureUSP10976(960));
    ASSERT_EQ_INT((int16_t)(1370 * 2.44 - 2329.0), convertTemperatureUSP10976(1370));
    ASSERT_EQ_INT((int16_t)(2000 * 2.24 - 2068.0), convertTemperatureUSP10976(2000)); /* 2412 = 24.12 °C */
    ASSERT_EQ_INT(2412, convertTemperatureUSP10976(2000));
    ASSERT_EQ_INT((int16_t)(2477 * 2.24 - 2068.0), convertTemperatureUSP10976(2477));
    ASSERT_EQ_INT((int16_t)(2980 * 2.78 - 3411.0), convertTemperatureUSP10976(2980));
    ASSERT_EQ_INT((int16_t)(3347 * 3.90 - 6777.0), convertTemperatureUSP10976(3347));
    ASSERT_EQ_INT((int16_t)(3519 * 5.09 - 10732.0), convertTemperatureUSP10976(3519));
    ASSERT_EQ_INT((int16_t)(3664 * 6.80 - 16739.0), convertTemperatureUSP10976(3664));
    ASSERT_EQ_INT((int16_t)(3761 * 9.11 - 25191.0), convertTemperatureUSP10976(3761));
    ASSERT_EQ_INT((int16_t)(3842 * 12.21 - 36828.0), convertTemperatureUSP10976(3842));
    ASSERT_EQ_INT((int16_t)(3896 * 16.57 - 53582.0), convertTemperatureUSP10976(3896));
    ASSERT_EQ_INT((int16_t)(3941 * 22.02 - 74813.0), convertTemperatureUSP10976(3941));
    ASSERT_EQ_INT((int16_t)(3975 * 29.49 - 104250.0), convertTemperatureUSP10976(3975));
    ASSERT_EQ_INT((int16_t)(4001 * 39.20 - 142860.0), convertTemperatureUSP10976(4001));
    ASSERT_EQ_INT((int16_t)(4025 * 51.67 - 192760.0), convertTemperatureUSP10976(4025));
}

TEST(valeur_normale_pas_d_erreur)
{
    s.value = 2500; /* 25 °C */
    check_n(200);
    ASSERT_EQ_INT(25, s.value_8);
    ASSERT_FALSE(s.high_level_warning_flag);
    ASSERT_FALSE(s.error_flag);
}

TEST(avertissement_haut_immediat_et_reversible)
{
    s.value = 5100;
    ctnSensorCheck(&s);
    ASSERT_TRUE(s.high_level_warning_flag);
    ASSERT_FALSE(s.error_flag);
    s.value = 4900;
    ctnSensorCheck(&s);
    ASSERT_FALSE(s.high_level_warning_flag);
}

TEST(erreur_haute_apres_timeout)
{
    s.value = 7100;
    check_n(50);
    ASSERT_FALSE(s.high_level_error_flag);
    ctnSensorCheck(&s);
    ASSERT_TRUE(s.high_level_error_flag);
    ASSERT_TRUE(s.error_flag);
    /* verrouillée */
    s.value = 2500;
    check_n(10);
    ASSERT_TRUE(s.high_level_error_flag);
    ASSERT_TRUE(s.error_flag);
}

TEST(erreur_basse_apres_timeout)
{
    s.value = -600; /* -6 °C < -5 */
    check_n(50);
    ASSERT_FALSE(s.low_level_error_flag);
    ctnSensorCheck(&s);
    ASSERT_TRUE(s.low_level_error_flag);
    ASSERT_TRUE(s.error_flag);
}

TEST(compteur_haut_remis_a_zero_si_retour_normal)
{
    s.value = 7100;
    check_n(30);
    s.value = 2500;
    ctnSensorCheck(&s);
    ASSERT_EQ_INT(0, s.high_level_cnt);
    s.value = 7100;
    check_n(50);
    ASSERT_FALSE(s.high_level_error_flag);
}

TEST(capteur_perdu_apres_timeout)
{
    s.value = 0x7FFF;
    check_n(50);
    ASSERT_FALSE(s.lost_error_flag);
    ctnSensorCheck(&s);
    ASSERT_TRUE(s.lost_error_flag);
    ASSERT_TRUE(s.error_flag);
    /* value_8 n'est pas mis à jour quand le capteur est perdu */
    ASSERT_EQ_INT(0, s.value_8);
}

TEST(init_efface_les_erreurs)
{
    s.value = 0x7FFF;
    check_n(60);
    ASSERT_TRUE(s.error_flag);
    ctnSensorInit(&s);
    ASSERT_FALSE(s.error_flag);
    ASSERT_FALSE(s.lost_error_flag);
    ASSERT_EQ_INT(0, s.lost_cnt);
}

TEST_MAIN_BEGIN()
    RUN_TEST(init_seuils_par_defaut);
    RUN_TEST(conversion_hors_plage);
    RUN_TEST(conversion_bornes_de_segments);
    RUN_TEST(valeur_normale_pas_d_erreur);
    RUN_TEST(avertissement_haut_immediat_et_reversible);
    RUN_TEST(erreur_haute_apres_timeout);
    RUN_TEST(erreur_basse_apres_timeout);
    RUN_TEST(compteur_haut_remis_a_zero_si_retour_normal);
    RUN_TEST(capteur_perdu_apres_timeout);
    RUN_TEST(init_efface_les_erreurs);
TEST_MAIN_END()
