/**
 * @file test_generic.c
 * @brief Tests des fonctions génériques (src/generic.c).
 */
#include "test_framework.h"
#include "generic.h"

static void test_setup(void) {}

/* ---- Arithmétique ---------------------------------------------------------- */
TEST(min_max)
{
    ASSERT_FLOAT_WITHIN(0.0, 1.0, Generic_MinF(1.0f, 2.0f));
    ASSERT_FLOAT_WITHIN(0.0, 1.0, Generic_MinF(2.0f, 1.0f));
    ASSERT_FLOAT_WITHIN(0.0, 2.0, Generic_MaxF(1.0f, 2.0f));
    ASSERT_FLOAT_WITHIN(0.0, 2.0, Generic_MaxF(2.0f, 1.0f));
    ASSERT_FLOAT_WITHIN(0.0, -3.0, Generic_MaxF(-3.0f, -3.0f));
}

TEST(saturation_bornes)
{
    ASSERT_FLOAT_WITHIN(0.0, 15.0, Generic_Saturation(15.0f, 3.0f, 100.0f));
    ASSERT_FLOAT_WITHIN(0.0, 100.0, Generic_Saturation(15.0f, 250.0f, 100.0f));
    ASSERT_FLOAT_WITHIN(0.0, 42.0, Generic_Saturation(15.0f, 42.0f, 100.0f));
    ASSERT_FLOAT_WITHIN(0.0, 15.0, Generic_Saturation(15.0f, 15.0f, 100.0f));
    ASSERT_FLOAT_WITHIN(0.0, 100.0, Generic_Saturation(15.0f, 100.0f, 100.0f));
}

TEST(dynamics_limiter_rampe)
{
    ASSERT_FLOAT_WITHIN(1e-6, 110.0, Generic_DynamicsLimiter(100.0f, 500.0f, 10.0f));
    ASSERT_FLOAT_WITHIN(1e-6, 90.0, Generic_DynamicsLimiter(100.0f, 0.0f, 10.0f));
    ASSERT_FLOAT_WITHIN(1e-6, 103.0, Generic_DynamicsLimiter(100.0f, 103.0f, 10.0f));
}

TEST(pid_sortie_nulle_sous_la_cible)
{
    float int_part = 0.0f;
    uint8_t out = Generic_PidController(1.0f, 0.1f, 0.0f, &int_part, 40.0f, 20.0f);
    ASSERT_EQ_INT(0, out);
    ASSERT_FLOAT_WITHIN(1e-5, -2.0, int_part); /* 0.1 * (20 - 40) */
}

TEST(pid_proportionnel_et_integrale)
{
    float int_part = 0.0f;
    /* erreur +10 : P = 10, I = 1 -> 11 */
    ASSERT_EQ_INT(11, Generic_PidController(1.0f, 0.1f, 0.0f, &int_part, 40.0f, 50.0f));
    /* deuxième appel : I = 2 -> 12 */
    ASSERT_EQ_INT(12, Generic_PidController(1.0f, 0.1f, 0.0f, &int_part, 40.0f, 50.0f));
}

TEST(pid_saturation_integrale_et_sortie)
{
    float int_part = 0.0f;
    int i;
    for (i = 0; i < 20000; i++)
        Generic_PidController(1.0f, 0.1f, 0.0f, &int_part, 40.0f, 60.0f);
    ASSERT_FLOAT_WITHIN(1e-3, 1000.0, int_part); /* 100 / Ki */
    ASSERT_EQ_INT(100, Generic_PidController(1.0f, 0.1f, 0.0f, &int_part, 40.0f, 60.0f));
    for (i = 0; i < 20000; i++)
        Generic_PidController(1.0f, 0.1f, 0.0f, &int_part, 40.0f, 0.0f);
    ASSERT_FLOAT_WITHIN(1e-3, -200.0, int_part); /* -20 / Ki */
    ASSERT_EQ_INT(0, Generic_PidController(1.0f, 0.1f, 0.0f, &int_part, 40.0f, 0.0f));
}

TEST(char_to_dec)
{
    ASSERT_EQ_INT(0, Generic_CharToDec('0'));
    ASSERT_EQ_INT(9, Generic_CharToDec('9'));
    ASSERT_EQ_INT(5, Generic_CharToDec('5'));
    ASSERT_EQ_INT(0x0F, Generic_CharToDec('/'));
    ASSERT_EQ_INT(0x0F, Generic_CharToDec(':'));
    ASSERT_EQ_INT(0x0F, Generic_CharToDec(' '));
    ASSERT_EQ_INT(0x0F, Generic_CharToDec('A'));
}

/* ---- Anti-rebond ------------------------------------------------------------ */
TEST(debounce_bascule_apres_threshold_plus_un)
{
    sDebounce d = {false, 0};
    int i;
    for (i = 0; i < 4; i++)
    {
        ASSERT_FALSE(Debounce_Update(&d, true, 4));
        ASSERT_FALSE(d.state);
    }
    ASSERT_TRUE(Debounce_Update(&d, true, 4)); /* 5e échantillon : cnt = 5 > 4 */
    ASSERT_TRUE(d.state);
    ASSERT_EQ_INT(0, d.cnt);
}

TEST(debounce_filtre_les_rebonds)
{
    sDebounce d = {false, 0};
    Debounce_Update(&d, true, 4);
    Debounce_Update(&d, true, 4);
    Debounce_Update(&d, false, 4); /* RAZ du compteur */
    ASSERT_EQ_INT(0, d.cnt);
    Debounce_Update(&d, true, 4);
    Debounce_Update(&d, true, 4);
    Debounce_Update(&d, true, 4);
    Debounce_Update(&d, true, 4);
    ASSERT_FALSE(d.state);
    Debounce_Update(&d, true, 4);
    ASSERT_TRUE(d.state);
}

TEST(debounce_relachement_symetrique)
{
    sDebounce d = {true, 0};
    int i;
    for (i = 0; i < 4; i++)
        Debounce_Update(&d, false, 4);
    ASSERT_TRUE(d.state);
    ASSERT_TRUE(Debounce_Update(&d, false, 4));
    ASSERT_FALSE(d.state);
    /* entrée stable égale à l'état : compteur maintenu à 0 */
    Debounce_Update(&d, false, 4);
    ASSERT_EQ_INT(0, d.cnt);
}

TEST(hold_detect_arme_sans_desarmer)
{
    sDebounce d = {false, 0};
    int i;
    for (i = 0; i < 50; i++)
        ASSERT_FALSE(HoldDetect_Update(&d, true, 50));
    ASSERT_FALSE(d.state);
    ASSERT_TRUE(HoldDetect_Update(&d, true, 50));
    ASSERT_TRUE(d.state);
    ASSERT_EQ_INT(0, d.cnt);
    HoldDetect_Update(&d, false, 50);
    ASSERT_TRUE(d.state); /* l'acquittement est externe */
    ASSERT_EQ_INT(0, d.cnt);
}

/* ---- Drapeaux temporisés ----------------------------------------------------- */
TEST(timed_flag8_declenche_au_dela_du_timeout)
{
    sTimedFlag8 tf = {0, false};
    int i;
    for (i = 0; i < 50; i++)
        ASSERT_FALSE(TimedFlag8_Update(&tf, true, 50));
    ASSERT_EQ_INT(50, tf.cnt);
    ASSERT_TRUE(TimedFlag8_Update(&tf, true, 50));
    ASSERT_EQ_INT(0, tf.cnt);
}

TEST(timed_flag8_verrouille_et_raz_compteur)
{
    sTimedFlag8 tf = {0, false};
    int i;
    for (i = 0; i < 20; i++)
        TimedFlag8_Update(&tf, true, 5);
    ASSERT_TRUE(tf.error);
    ASSERT_FALSE(TimedFlag8_Update(&tf, false, 5) == false); /* reste verrouillé */
    ASSERT_EQ_INT(0, tf.cnt);
    tf.cnt = 3;
    TimedFlag8_Update(&tf, false, 5);
    ASSERT_EQ_INT(0, tf.cnt);
}

TEST(timed_flag8_compteur_8_bits_ne_depasse_jamais_450)
{
    /* Comportement d'origine conservé (TODO bug 9/10) : compteur uint8_t, timeout > 255 */
    sTimedFlag8 tf = {0, false};
    int i;
    for (i = 0; i < 5000; i++)
        TimedFlag8_Update(&tf, true, 450);
    ASSERT_FALSE(tf.error);
}

TEST(timed_flag16_compteur_16_bits)
{
    sTimedFlag16 tf = {0, false};
    int i;
    for (i = 0; i < 600; i++)
        ASSERT_FALSE(TimedFlag16_Update(&tf, true, 600));
    ASSERT_TRUE(TimedFlag16_Update(&tf, true, 600));
    ASSERT_EQ_INT(0, tf.cnt);
}

/* ---- Moyenne par blocs ------------------------------------------------------- */
TEST(block_average_sur_10)
{
    sBlockAverage a = {0, 0, 0};
    int i;
    for (i = 1; i <= 9; i++)
        ASSERT_FALSE(BlockAverage_Push(&a, (int16_t)(i * 10), 10));
    ASSERT_TRUE(BlockAverage_Push(&a, 100, 10));
    ASSERT_EQ_INT(55, a.value);
    ASSERT_EQ_INT(0, a.sum);
    ASSERT_EQ_INT(0, a.ind);
}

TEST(block_average_negatif_tronque_vers_zero)
{
    sBlockAverage a = {0, 0, 0};
    int i;
    for (i = 0; i < 9; i++)
        BlockAverage_Push(&a, -1, 10);
    BlockAverage_Push(&a, -6, 10); /* somme -15 / 10 = -1 (troncature C) */
    ASSERT_EQ_INT(-1, a.value);
}

/* ---- Motif sonore ------------------------------------------------------------ */
TEST(beep_pattern_sequence_10_12_14_16)
{
    uint8_t cpt = 0;
    int i;
    for (i = 1; i <= 9; i++)
        ASSERT_EQ_INT(BEEP_UNCHANGED, BeepPattern_Update(&cpt, 30));
    ASSERT_EQ_INT(BEEP_ON, BeepPattern_Update(&cpt, 30));        /* 10 */
    ASSERT_EQ_INT(BEEP_UNCHANGED, BeepPattern_Update(&cpt, 30)); /* 11 */
    ASSERT_EQ_INT(BEEP_OFF, BeepPattern_Update(&cpt, 30));       /* 12 */
    ASSERT_EQ_INT(BEEP_UNCHANGED, BeepPattern_Update(&cpt, 30)); /* 13 */
    ASSERT_EQ_INT(BEEP_ON, BeepPattern_Update(&cpt, 30));        /* 14 */
    ASSERT_EQ_INT(BEEP_UNCHANGED, BeepPattern_Update(&cpt, 30)); /* 15 */
    ASSERT_EQ_INT(BEEP_OFF, BeepPattern_Update(&cpt, 30));       /* 16 */
    for (i = 17; i < 30; i++)
    {
        ASSERT_EQ_INT(BEEP_UNCHANGED, BeepPattern_Update(&cpt, 30));
        ASSERT_EQ_INT(i, cpt);
    }
    BeepPattern_Update(&cpt, 30); /* 30 >= period : RAZ */
    ASSERT_EQ_INT(0, cpt);
}

TEST(beep_pattern_periode_superieure_a_255_boucle_sur_256)
{
    uint8_t cpt = 0;
    int i;
    for (i = 0; i < 256; i++)
        BeepPattern_Update(&cpt, 920);
    ASSERT_EQ_INT(0, cpt); /* débordement 8 bits, pas de RAZ explicite */
}

TEST_MAIN_BEGIN()
    RUN_TEST(min_max);
    RUN_TEST(saturation_bornes);
    RUN_TEST(dynamics_limiter_rampe);
    RUN_TEST(pid_sortie_nulle_sous_la_cible);
    RUN_TEST(pid_proportionnel_et_integrale);
    RUN_TEST(pid_saturation_integrale_et_sortie);
    RUN_TEST(char_to_dec);
    RUN_TEST(debounce_bascule_apres_threshold_plus_un);
    RUN_TEST(debounce_filtre_les_rebonds);
    RUN_TEST(debounce_relachement_symetrique);
    RUN_TEST(hold_detect_arme_sans_desarmer);
    RUN_TEST(timed_flag8_declenche_au_dela_du_timeout);
    RUN_TEST(timed_flag8_verrouille_et_raz_compteur);
    RUN_TEST(timed_flag8_compteur_8_bits_ne_depasse_jamais_450);
    RUN_TEST(timed_flag16_compteur_16_bits);
    RUN_TEST(block_average_sur_10);
    RUN_TEST(block_average_negatif_tronque_vers_zero);
    RUN_TEST(beep_pattern_sequence_10_12_14_16);
    RUN_TEST(beep_pattern_periode_superieure_a_255_boucle_sur_256);
TEST_MAIN_END()
