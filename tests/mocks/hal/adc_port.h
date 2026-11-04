/**
 * @file tests/mocks/hal/adc_port.h
 * @brief Double de test de hal/adc_port.h : mémorise la voie sélectionnée, renvoie des valeurs injectées.
 */
#ifndef HAL_ADC_PORT_H
#define HAL_ADC_PORT_H

#include <stdint.h>
#include <stdbool.h>

#define ADC_PORT_CH_TEMP_1          0
#define ADC_PORT_CH_TEMP_2          1
#define ADC_PORT_CH_SUPPLY_VOLTAGE  2
#define ADC_PORT_CH_BUS_VOLTAGE     3
#define ADC_PORT_CH_NB              4
#define ADC_PORT_CH_INITIAL         4

typedef struct
{
    uint8_t selected_channel;
    uint16_t result[ADC_PORT_CH_NB + 1]; /**< valeur renvoyée par voie (index ADC_PORT_CH_*) */
    bool conversion_complete;
    int init_calls;
    int select_calls;
} sMockAdc;

extern sMockAdc mock_adc;

void MockAdc_Reset(void);

void AdcPort_Init(void);
void AdcPort_SelectChannel(uint8_t ch);
bool AdcPort_IsConversionComplete(uint8_t ch);
uint16_t AdcPort_Result(uint8_t ch);

#endif /* HAL_ADC_PORT_H */
