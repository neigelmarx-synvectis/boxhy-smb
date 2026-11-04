/**
 * @file hal/adc_port.h
 * @brief Convertisseur analogique (ADC1 MCC) : canaux repérés par un index applicatif.
 */
#ifndef HAL_ADC_PORT_H
#define HAL_ADC_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/adc1.h"

/* Index applicatifs des voies mesurées (ordre d'échantillonnage) */
#define ADC_PORT_CH_TEMP_1          0
#define ADC_PORT_CH_TEMP_2          1
#define ADC_PORT_CH_SUPPLY_VOLTAGE  2
#define ADC_PORT_CH_BUS_VOLTAGE     3
#define ADC_PORT_CH_NB              4
/** Voie sélectionnée au démarrage par le code d'origine (ADC1_ChannelSelect(0) = AUX_TEMP_3, AN0 non câblé). */
#define ADC_PORT_CH_INITIAL         4

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

static inline void AdcPort_Init(void)
{
    ADC1_Enable();
    ADC1_ChannelSelect(AdcPort_ToMccChannel(ADC_PORT_CH_INITIAL));
    ADC1_InterruptEnable();
    ADC1_SoftwareTriggerEnable();
}

static inline void AdcPort_SelectChannel(uint8_t ch) { ADC1_ChannelSelect(AdcPort_ToMccChannel(ch)); }
static inline bool AdcPort_IsConversionComplete(uint8_t ch) { return ADC1_IsConversionComplete(AdcPort_ToMccChannel(ch)); }
static inline uint16_t AdcPort_Result(uint8_t ch) { return ADC1_ConversionResultGet(AdcPort_ToMccChannel(ch)); }

#endif /* HAL_ADC_PORT_H */
