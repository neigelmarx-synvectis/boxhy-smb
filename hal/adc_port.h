/**
 * @file hal/adc_port.h
 * @brief Convertisseur analogique (ADC1 MCC) : canaux repérés par un index applicatif.
 *
 * measures.c échantillonne les voies à tour de rôle et range chaque résultat dans un tableau indexé
 * par ADC_PORT_CH_*. Cet index applicatif (0..3, contigu) est traduit ici vers l'énumération MCC
 * ADC1_CHANNEL, dont les valeurs suivent l'ordre de configuration MCC et non les entrées physiques :
 *
 *     index applicatif              voie MCC          entrée analogique
 *     ADC_PORT_CH_TEMP_1         -> AUX_TEMP_1        AN10  (sonde CTN ambiance AC)
 *     ADC_PORT_CH_TEMP_2         -> AUX_TEMP_2        AN9   (sonde CTN 2)
 *     ADC_PORT_CH_SUPPLY_VOLTAGE -> SUPPLY_VOLTAGE    AN4   (alimentation 24 V)
 *     ADC_PORT_CH_BUS_VOLTAGE    -> BUS_VOLTAGE       AN5   (bus DC)
 *     autre (ADC_PORT_CH_INITIAL)-> AUX_TEMP_3        AN0   (non câblé, voir ci-dessous)
 *
 * `static inline` : aucun coût par rapport aux appels MCC directs. Double de test :
 * tests/mocks/hal/adc_port.h (résultats injectés par voie).
 */
#ifndef HAL_ADC_PORT_H
#define HAL_ADC_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/adc1.h"

/* Index applicatifs des voies mesurées (ordre d'échantillonnage, et index du tableau des moyennes) */
#define ADC_PORT_CH_TEMP_1          0
#define ADC_PORT_CH_TEMP_2          1
#define ADC_PORT_CH_SUPPLY_VOLTAGE  2
#define ADC_PORT_CH_BUS_VOLTAGE     3
#define ADC_PORT_CH_NB              4
/** Voie sélectionnée au démarrage par le code d'origine (ADC1_ChannelSelect(0) = AUX_TEMP_3,
 *  AN0 non câblé) : le premier échantillon de la sonde 1 est donc lu sur cette voie. */
#define ADC_PORT_CH_INITIAL         4

/** Traduit un index applicatif en voie MCC (voir le tableau en tête de fichier). */
static inline ADC1_CHANNEL AdcPort_ToMccChannel(uint8_t ch)
{
    switch (ch)
    {
    case ADC_PORT_CH_TEMP_1:         return AUX_TEMP_1;
    case ADC_PORT_CH_TEMP_2:         return AUX_TEMP_2;
    case ADC_PORT_CH_SUPPLY_VOLTAGE: return SUPPLY_VOLTAGE;
    case ADC_PORT_CH_BUS_VOLTAGE:    return BUS_VOLTAGE;
    default:                         return AUX_TEMP_3;
    }
}

/** Active l'ADC, sélectionne la voie de démarrage, autorise l'interruption de fin de conversion
 *  et démarre l'échantillonnage (bit SAMP). Ensuite, selon la configuration MCC, l'échantillonnage
 *  est automatique (ASAM) et chaque conversion est déclenchée par TMR5 ; l'interruption ADC signale
 *  chaque fin de conversion à measures.c. Appelée par Measures_Init(). */
static inline void AdcPort_Init(void)
{
    ADC1_Enable();
    ADC1_ChannelSelect(AdcPort_ToMccChannel(ADC_PORT_CH_INITIAL));
    ADC1_InterruptEnable();
    ADC1_SoftwareTriggerEnable();
}

/** Sélectionne la voie de la prochaine conversion. */
static inline void AdcPort_SelectChannel(uint8_t ch) { ADC1_ChannelSelect(AdcPort_ToMccChannel(ch)); }
/** Conversion terminée sur la voie (testé dans l'interruption ADC). */
static inline bool AdcPort_IsConversionComplete(uint8_t ch) { return ADC1_IsConversionComplete(AdcPort_ToMccChannel(ch)); }
/** Résultat 12 bits (0..4095) de la dernière conversion de la voie. */
static inline uint16_t AdcPort_Result(uint8_t ch) { return ADC1_ConversionResultGet(AdcPort_ToMccChannel(ch)); }

#endif /* HAL_ADC_PORT_H */
