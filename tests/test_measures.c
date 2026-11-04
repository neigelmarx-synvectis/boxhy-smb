/**
 * @file test_measures.c
 * @brief Tests des mesures analogiques (src/measures.c) avec le double ADC.
 */
#include "test_framework.h"
#include "measures.h"
#include "smb_config.h"
#include "hal/adc_port.h"

static void test_setup(void)
{
    MockAdc_Reset();
    Measures_Init();
}

/* Simule un cycle complet : 4 voies x ADC_SAMPLE_NB conversions. */
static void acquire_full_cycle(void)
{
    int i;
    for (i = 0; i < ADC_PORT_CH_NB * ADC_SAMPLE_NB; i++)
    {
        mock_adc.conversion_complete = true;
        Measures_OnConversionDone();
        ASSERT_TRUE(measures.conversion_done);
        Measures_ProcessConversion();
    }
}

TEST(init_configure_l_adc_et_les_seuils)
{
    ASSERT_EQ_INT(1, mock_adc.init_calls);
    ASSERT_EQ_INT(ADC_PORT_CH_INITIAL, mock_adc.selected_channel);
    ASSERT_EQ_INT(75, measures.ac_ambiant_temp.high_level);
    ASSERT_EQ_INT(0, measures.ac_ambiant_temp.low_level);
    ASSERT_EQ_INT(50, measures.ac_ambiant_temp.warning_high_level);
    ASSERT_EQ_INT(90, measures.temp2.high_level);
    ASSERT_EQ_INT(0, measures.temp2.low_level);
    ASSERT_EQ_INT(50, measures.temp2.warning_high_level);
    ASSERT_FALSE(measures.conversion_done);
    ASSERT_FALSE(measures.samples_ready);
}

TEST(isr_ignore_une_conversion_incomplete)
{
    mock_adc.conversion_complete = false;
    Measures_OnConversionDone();
    ASSERT_FALSE(measures.conversion_done);
    mock_adc.conversion_complete = true;
    Measures_OnConversionDone();
    ASSERT_TRUE(measures.conversion_done);
}

TEST(rotation_des_voies)
{
    mock_adc.conversion_complete = true;
    Measures_ProcessConversion();
    ASSERT_EQ_INT(ADC_PORT_CH_TEMP_2, mock_adc.selected_channel);
    ASSERT_FALSE(measures.conversion_done);
    Measures_ProcessConversion();
    ASSERT_EQ_INT(ADC_PORT_CH_SUPPLY_VOLTAGE, mock_adc.selected_channel);
    Measures_ProcessConversion();
    ASSERT_EQ_INT(ADC_PORT_CH_BUS_VOLTAGE, mock_adc.selected_channel);
    Measures_ProcessConversion();
    ASSERT_EQ_INT(ADC_PORT_CH_TEMP_1, mock_adc.selected_channel);
}

TEST(moyenne_sur_50_echantillons_et_conversion)
{
    mock_adc.result[ADC_PORT_CH_TEMP_1] = 2000;
    mock_adc.result[ADC_PORT_CH_TEMP_2] = 3000;
    mock_adc.result[ADC_PORT_CH_SUPPLY_VOLTAGE] = 1000;
    mock_adc.result[ADC_PORT_CH_BUS_VOLTAGE] = 2500;
    mock_adc.result[ADC_PORT_CH_INITIAL] = 2000; /* première lecture de la case 0 (voie initiale) */

    acquire_full_cycle();
    ASSERT_TRUE(measures.samples_ready);

    Measures_Convert();
    ASSERT_FALSE(measures.samples_ready);
    ASSERT_EQ_INT(2000, measures.temp_1_raw);
    ASSERT_EQ_INT(3000, measures.temp_2_raw);
    ASSERT_EQ_INT(1000, measures.supply_voltage_raw);
    ASSERT_EQ_INT(2500, measures.bus_voltage_raw);
    ASSERT_EQ_INT((uint16_t)(1000 * 1.969), measures.supply_voltage); /* 1969 */
    ASSERT_EQ_INT((uint16_t)(2500 * 1.962), measures.bus_voltage);    /* 4905 */
    ASSERT_EQ_INT(convertTemperatureUSP10976(2000), measures.ac_ambiant_temp.value);
    ASSERT_EQ_INT(convertTemperatureUSP10976(3000), measures.temp2.value);
}

TEST(pas_de_resultat_avant_50_echantillons)
{
    int i;
    for (i = 0; i < ADC_PORT_CH_NB * (ADC_SAMPLE_NB - 1); i++)
        Measures_ProcessConversion();
    ASSERT_FALSE(measures.samples_ready);
    for (i = 0; i < ADC_PORT_CH_NB; i++)
        Measures_ProcessConversion();
    ASSERT_TRUE(measures.samples_ready);
}

TEST(premier_echantillon_case_0_provient_de_la_voie_initiale)
{
    /* Comportement d'origine : la voie sélectionnée au démarrage (index 0 de l'énumération MCC)
     * n'est pas la sonde 1 ; un échantillon sur 50 du premier cycle est donc pris sur cette voie. */
    mock_adc.result[ADC_PORT_CH_TEMP_1] = 100;
    mock_adc.result[ADC_PORT_CH_INITIAL] = 5100; /* 100 + 5000 -> moyenne décalée de +100 */
    acquire_full_cycle();
    Measures_Convert();
    ASSERT_EQ_INT(200, measures.temp_1_raw);

    /* second cycle : plus de voie initiale */
    acquire_full_cycle();
    Measures_Convert();
    ASSERT_EQ_INT(100, measures.temp_1_raw);
}

TEST(check_100ms_surveille_les_deux_sondes)
{
    int i;
    measures.ac_ambiant_temp.value = 7600; /* > 75 °C */
    measures.temp2.value = 9100;           /* > 90 °C */
    for (i = 0; i < 51; i++)
        Measures_Check100ms();
    ASSERT_TRUE(measures.ac_ambiant_temp.error_flag);
    ASSERT_TRUE(measures.temp2.error_flag);
}

TEST(reset_sensor_errors_current_behaviour_reapplique_les_seuils_module)
{
    int i;
    measures.ac_ambiant_temp.value = 7600;
    for (i = 0; i < 51; i++)
        Measures_Check100ms();
    ASSERT_TRUE(measures.ac_ambiant_temp.error_flag);

    Measures_ResetSensorErrors();
    ASSERT_FALSE(measures.ac_ambiant_temp.error_flag);
    ASSERT_FALSE(measures.temp2.error_flag);
    /* TODO(bug 12) : seuils remplacés par ceux du module CTN */
    ASSERT_EQ_INT(70, measures.ac_ambiant_temp.high_level);
    ASSERT_EQ_INT(-5, measures.ac_ambiant_temp.low_level);
    ASSERT_EQ_INT(70, measures.temp2.high_level);
}

TEST_MAIN_BEGIN()
    RUN_TEST(init_configure_l_adc_et_les_seuils);
    RUN_TEST(isr_ignore_une_conversion_incomplete);
    RUN_TEST(rotation_des_voies);
    RUN_TEST(moyenne_sur_50_echantillons_et_conversion);
    RUN_TEST(pas_de_resultat_avant_50_echantillons);
    RUN_TEST(premier_echantillon_case_0_provient_de_la_voie_initiale);
    RUN_TEST(check_100ms_surveille_les_deux_sondes);
    RUN_TEST(reset_sensor_errors_current_behaviour_reapplique_les_seuils_module);
TEST_MAIN_END()
