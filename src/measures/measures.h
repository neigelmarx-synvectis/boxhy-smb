/**
 * @file measures.h
 * @brief Mesures analogiques : 4 voies ADC échantillonnées en rotation, moyennées sur
 *        ADC_SAMPLE_NB échantillons, converties en tensions et températures (sondes CTN).
 *
 * Chaîne de traitement :
 *  1. ISR ADC1 (Measures_OnConversionDone) : lève conversion_done à chaque fin de conversion ;
 *  2. boucle principale (Measures_ProcessConversion) : accumule le résultat et sélectionne la voie
 *     suivante (température 1, température 2, alimentation 24 V, bus DC) ;
 *  3. après 50 tours complets des 4 voies, samples_ready est levé et Measures_Convert() calcule
 *     tensions et températures ;
 *  4. toutes les 100 ms, Measures_Check100ms() surveille les seuils des deux sondes CTN.
 */
#ifndef MEASURES_H
#define MEASURES_H

#include <stdint.h>
#include <stdbool.h>
#include "src/measures/ctn_sensor.h"

/** Grandeurs mesurées, lues par la supervision, la régulation et la télémétrie. */
typedef struct
{
    uint16_t supply_voltage;     /**< alimentation 24 V (0.01 V) */
    uint16_t bus_voltage;        /**< bus DC (0.01 V) */
    uint16_t supply_voltage_raw; /**< moyenne ADC brute */
    uint16_t bus_voltage_raw;    /**< moyenne ADC brute */
    uint16_t temp_1_raw;         /**< moyenne ADC brute de la sonde 1 */
    uint16_t temp_2_raw;         /**< moyenne ADC brute de la sonde 2 */
    sCTN_SENSOR ac_ambiant_temp; /**< sonde 1 : ambiance partie AC */
    sCTN_SENSOR temp2;           /**< sonde 2 */

    volatile bool conversion_done; /**< levé par l'ISR ADC, acquitté par Measures_ProcessConversion */
    bool samples_ready;            /**< moyennes disponibles, acquitté par Measures_Convert */
} sMeasures;

/** Instance unique des mesures. */
extern sMeasures measures;

/** Initialise l'ADC, les accumulateurs et les seuils applicatifs des sondes CTN (smb_config.h). */
void Measures_Init(void);
/** Corps de l'interruption ADC1. */
void Measures_OnConversionDone(void);
/** Lit le résultat courant, l'accumule, passe à la voie suivante (boucle principale). */
void Measures_ProcessConversion(void);
/** Convertit les moyennes en grandeurs physiques (boucle principale, quand samples_ready). */
void Measures_Convert(void);
/** Surveillance des sondes CTN, toutes les 100 ms. */
void Measures_Check100ms(void);
/** Acquittement des erreurs sondes (sortie du mode défaut). */
void Measures_ResetSensorErrors(void);

#endif /* MEASURES_H */
