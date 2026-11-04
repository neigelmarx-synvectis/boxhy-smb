/**
 * @file ctn_sensor.h
 * @brief Sonde de température CTN : conversion ADC -> température et surveillance de seuils.
 *
 * Module hérité de CTN_SENSOR_V03 (JV) :
 *  - V01 : 2021/08/23 - Version initiale
 *  - V02 : 2021/12/09 - Corrections
 *  - V03 : 2022/01/24 - Refonte et simplification
 *  - 2026 : recompilé en unité de compilation séparée (plus de #include du .c)
 *
 * Utilisation : initialiser avec ctnSensorInit() puis régler les seuils ; affecter
 * `value` (0.01 °C) après chaque conversion ADC (convertTemperatureUSP10976) ;
 * appeler ctnSensorCheck() toutes les 100 ms.
 *  - `high_level_warning_flag` : sans temporisation ;
 *  - `low_level_error_flag` / `high_level_error_flag` : après `CTN_SENSOR_LEVEL_TIMEOUT` ;
 *  - `lost_error_flag` : valeur 0x7FFF (hors plage) pendant `CTN_SENSOR_LOST_TIMEOUT`.
 * Les drapeaux d'erreur sont verrouillés : seul ctnSensorInit() les efface (sortie du mode défaut).
 *
 * Dans ce firmware, les deux sondes sont gérées par src/measures/measures.c, qui fournit `value`
 * après chaque bloc de 50 conversions et appelle ctnSensorCheck() depuis la tâche 100 ms.
 */
#ifndef CTN_SENSOR_H
#define CTN_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#define CTN_SENSOR_LOST_VALUE     0x7FFF /**< valeur hors plage de mesure */
#define CTN_SENSOR_LOST_TIMEOUT   50     /**< ticks 100 ms (~5 s hors plage avant l'erreur) */
#define CTN_SENSOR_LEVEL_TIMEOUT  50     /**< ticks 100 ms (~5 s au-delà d'un seuil avant l'erreur) */

/** État d'une sonde : seuils (réglés par l'application), mesure, drapeaux et compteurs. */
typedef struct
{
    /* A régler en début de soft */
    int8_t warning_high_level; /**< Température (1°C) d'avertissement haute */
    int8_t high_level;         /**< Température (1°C) d'erreur haute après level_timeout */
    int8_t low_level;          /**< Température (1°C) d'erreur basse après level_timeout */

    /* Lecture seulement */
    int16_t value;   /**< Température (0.01°C) */
    int8_t value_8;  /**< Température (1°C) */

    bool high_level_warning_flag; /**< au-dessus de warning_high_level (immédiat, non verrouillé) */
    bool high_level_error_flag;   /**< au-dessus de high_level trop longtemps (verrouillé) */
    bool low_level_error_flag;    /**< sous low_level trop longtemps (verrouillé) */
    bool lost_error_flag;         /**< valeur hors plage trop longtemps : sonde absente ou en court-circuit */
    bool error_flag; /**< erreur générale (haute, basse ou capteur perdu) */

    /* Interne */
    uint8_t high_level_cnt; /**< ticks consécutifs au-dessus de high_level */
    uint8_t low_level_cnt;  /**< ticks consécutifs sous low_level */
    uint8_t lost_cnt;       /**< ticks consécutifs hors plage */
} sCTN_SENSOR;

/** Remet le capteur à zéro et applique les seuils par défaut (50 / 70 / -5 °C). */
void ctnSensorInit(sCTN_SENSOR *ctn_sensor);
/** Surveillance des seuils, à appeler toutes les 100 ms. */
void ctnSensorCheck(sCTN_SENSOR *ctn_sensor);
/** Conversion du résultat ADC 12 bits d'une sonde USP10976 en 0.01 °C (0x7FFF hors plage). */
int16_t convertTemperatureUSP10976(uint16_t adc_result);

#endif /* CTN_SENSOR_H */
