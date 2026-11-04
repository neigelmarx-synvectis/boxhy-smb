/**
 * @file measures.c
 * @brief Mesures analogiques (voir measures.h).
 *
 * Les quatre voies sont converties à tour de rôle, une conversion par tour de boucle principale
 * (la tâche est prioritaire sur les tâches périodiques). Chaque voie est moyennée sur ADC_SAMPLE_NB
 * conversions pour filtrer le bruit ; les grandeurs physiques sont recalculées à chaque bloc complet.
 */
#include "src/measures/measures.h"
#include "src/smb_config.h"
#include "hal/adc_port.h"

sMeasures measures;

/* Accumulateurs indexés par voie applicative (ADC_PORT_CH_TEMP_1 ... ADC_PORT_CH_BUS_VOLTAGE) */
static uint32_t adc_sum_tab[ADC_PORT_CH_NB]; /* sommes pour moyennage */
static uint16_t adc_moy_tab[ADC_PORT_CH_NB]; /* moyennes */
static uint8_t adc_samples_cnt;              /* tours complets des 4 voies dans le bloc en cours */
static uint8_t current_index;   /* case du tableau en cours d'acquisition */
static uint8_t current_channel; /* voie physiquement sélectionnée */

/* Démarre l'ADC et applique les seuils des sondes. Appelée une fois par main(). */
void Measures_Init(void)
{
    uint8_t i;

    AdcPort_Init();
    for (i = 0; i < ADC_PORT_CH_NB; i++)
    {
        adc_moy_tab[i] = 0;
        adc_sum_tab[i] = 0;
    }
    adc_samples_cnt = 0;
    current_index = ADC_PORT_CH_TEMP_1;
    /* Au démarrage, la voie sélectionnée n'est pas celle de la case 0 (code d'origine) :
     * le premier échantillon de la sonde 1 provient de la voie "initiale". */
    current_channel = ADC_PORT_CH_INITIAL;

    measures.supply_voltage = 0;
    measures.bus_voltage = 0;
    measures.supply_voltage_raw = 0;
    measures.bus_voltage_raw = 0;
    measures.temp_1_raw = 0;
    measures.temp_2_raw = 0;
    measures.conversion_done = false;
    measures.samples_ready = false;

    /* Seuils applicatifs ; les autres champs sont remis à zéro. */
    measures.ac_ambiant_temp = (sCTN_SENSOR){
        .high_level = CTN_AC_AMBIANT_HIGH_LEVEL,
        .low_level = CTN_AC_AMBIANT_LOW_LEVEL,
        .warning_high_level = CTN_AC_AMBIANT_WARNING_HIGH_LEVEL,
    };
    measures.temp2 = (sCTN_SENSOR){
        .high_level = CTN_TEMP2_HIGH_LEVEL,
        .low_level = CTN_TEMP2_LOW_LEVEL,
        .warning_high_level = CTN_TEMP2_WARNING_HIGH_LEVEL,
    };
}

/* Interruption ADC1 : signale seulement la fin de conversion ; la lecture se fait dans la boucle. */
void Measures_OnConversionDone(void)
{
    if (AdcPort_IsConversionComplete(current_channel))
        measures.conversion_done = true;
}

/* Boucle principale : accumule la conversion terminée et passe à la voie suivante. Tous les
 * ADC_SAMPLE_NB tours complets, calcule les moyennes et lève samples_ready. */
void Measures_ProcessConversion(void)
{
    uint8_t i;

    adc_sum_tab[current_index] += AdcPort_Result(current_channel);

    current_index++;
    if (current_index >= ADC_PORT_CH_NB) /* les 4 voies ont été lues : un tour complet */
    {
        adc_samples_cnt++;
        if (adc_samples_cnt >= ADC_SAMPLE_NB)
        {
            for (i = 0; i < ADC_PORT_CH_NB; i++)
            {
                adc_moy_tab[i] = (uint16_t)(adc_sum_tab[i] / ADC_SAMPLE_NB);
                adc_sum_tab[i] = 0;
            }
            measures.samples_ready = true;
            adc_samples_cnt = 0;
        }
        current_index = 0;
    }

    /* La voie physique suit désormais la case du tableau (voir hal/adc_port.h pour la correspondance) */
    current_channel = current_index;
    AdcPort_SelectChannel(current_channel);

    measures.conversion_done = false;
}

/* Conversion des moyennes en grandeurs physiques : tensions en 0,01 V, températures en 0,01 °C. */
void Measures_Convert(void)
{
    /* Moyennes brutes, publiées telles quelles dans la télémétrie (trame SMB_TX_RAW1) */
    measures.temp_1_raw = adc_moy_tab[ADC_PORT_CH_TEMP_1];
    measures.temp_2_raw = adc_moy_tab[ADC_PORT_CH_TEMP_2];
    measures.bus_voltage_raw = adc_moy_tab[ADC_PORT_CH_BUS_VOLTAGE];
    measures.supply_voltage_raw = adc_moy_tab[ADC_PORT_CH_SUPPLY_VOLTAGE];

    /* Gain des ponts diviseurs (calcul flottant, résultat tronqué) */
    measures.supply_voltage = adc_moy_tab[ADC_PORT_CH_SUPPLY_VOLTAGE] * SUPPLY_VOLTAGE_GAIN;
    measures.bus_voltage = adc_moy_tab[ADC_PORT_CH_BUS_VOLTAGE] * BUS_VOLTAGE_GAIN;

    /* Linéarisation par segments des sondes CTN ; 0x7FFF si hors plage
     * (sonde absente ou en court-circuit) */
    measures.ac_ambiant_temp.value = convertTemperatureUSP10976(adc_moy_tab[ADC_PORT_CH_TEMP_1]);
    measures.temp2.value = convertTemperatureUSP10976(adc_moy_tab[ADC_PORT_CH_TEMP_2]);

    measures.samples_ready = false;
}

/* Toutes les 100 ms : seuils haut / bas et perte des deux sondes (temporisations dans ctn_sensor.c). */
void Measures_Check100ms(void)
{
    ctnSensorCheck(&measures.ac_ambiant_temp);
    ctnSensorCheck(&measures.temp2);
}

/* Appelée par la machine d'états à la sortie du mode défaut. */
void Measures_ResetSensorErrors(void)
{
    /* TODO(bug 12) : ctnSensorInit() réapplique les seuils par défaut du module (50/70/-5 °C)
     * à la place des seuils applicatifs ; comportement d'origine conservé. */
    measures.ac_ambiant_temp.error_flag = 0;
    ctnSensorInit(&measures.temp2);
    ctnSensorInit(&measures.ac_ambiant_temp);
}
