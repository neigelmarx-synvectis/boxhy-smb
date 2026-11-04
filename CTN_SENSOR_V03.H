/* EXPLICATIONS / DETAILS :
 * Initialiser le capteur avec 'ctn_temperature_init'.
 * Définir 'level_timeout' & 'lost_timeout' (durée en 0.1s) et les différents niveaux d'erreur/avertissements.
 * Appeller 'ctn_temperature_sensor_read' à chaque fin de conversion ADC avec comme paramètre l'index de la sonde utilisée :
 *  - 0x01 : Sonde Ballard
 *  - 0x02 : Sonde NJ28NA0103FCC
 *  - 0x03 : Sonde USP10976
 *  - ...
 * Si la valeur est hors des limites programmées ('raw_value_low_limit' & 'raw_value_high_limit') pendant la durée définie 'lost_timeout', 'lost_flag' est mis à 1.
 * Appeller 'ctn_temperature_sensor_check_error' toutes les 100ms.
 * Si la valeur est hors des valeurs d'avertissements ('warning_low_level' & 'warning_high_level'), les flags 'low_level_warning_flag' & 'high_level_warning_flag' sont mis à 1 sans timeout.
 * Si la valeur est hors des valeurs haute/basse ('low_level' & 'high_level') pendant la durée 'level_timeout', les flags 'low_level_error_flag' & 'high_level_error_flag' sont mis à 1.
 * Si la valeur est hors des valeurs d'erreur critique ('very_low_level' & 'very_high_level') pendant la durée ('level_timeout' / 10), les flags 'low_level_error_flag' & 'high_level_error_flag' sont mis à 1.
 */

/* HISTORIQUE DES VERSIONS :
 * V01 : JV - 2021/08/23 - Version initiale
 * V02 : JV - 2021/12/09 - Corrections
 * V03 : JV - 2022/01/24 - Refonte et simplification
 * 
*/

#ifndef CTN_SENSOR_H
#define	CTN_SENSOR_H

//#include "CTN_SENSOR_V03.C"

// <editor-fold defaultstate="collapsed" desc="DEFINES">
//#define CTN_BALLARD         1
//#define CTN_NJ28NA0103FCC   2
#define CTN_USP10976        3
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="STRUCTURE">
typedef struct
{
    //A REGLER EN DEBUT DE SOFT---------------------------------------------------------------------------------------------------------------------------------------
    int8_t warning_high_level;   //Temperature (1�C) à laquelle un avertissement de temperature haute est declenche
    int8_t high_level;           //Temperature (1�C) à laquelle une erreur de temperature haute est declenchee apres 'level_timeout'
    int8_t low_level;            //Temperature (1�C) à laquelle une erreur de temperature basse est declench�ee apres 'level_timeout'
    
    //--------------------------------------------------------------------------------------------------------------------------------------------------------------
    
    
    //LECTURE SEULEMENT----------------------------------------------------------------------------------------------------------------------------------------------
    int16_t value;                //Temperature (0.01�C)
    int8_t value_8;                //Temperature (1�C)
    
    bool high_level_warning_flag;  //Flag d'avertissement temperature haute
    bool high_level_error_flag;    //Flag d'erreur temperature haute
    bool low_level_error_flag;     //Flag d'erreur temperature haute
    bool lost_error_flag;                //Flag d'erreur capteur perdu
    bool error_flag;               //Flag d'erreur general de temperature (haute ou basse)
    //--------------------------------------------------------------------------------------------------------------------------------------------------------------
    
    //AUTRES--------------------------------------------------------------------------------------------------------------------------------------------------------
    
    uint8_t high_level_cnt;   //Compteur pour delai avant declenchement de l'erreur temperature haute
    uint8_t low_level_cnt;    //Compteur pour delai avant declenchement de l'erreur temperature basse
    uint8_t lost_cnt;         //Compteur pour delai avant declenchement de l'erreur perte de capteur    
    
    //--------------------------------------------------------------------------------------------------------------------------------------------------------------
}sCTN_SENSOR;
// </editor-fold>    

// <editor-fold defaultstate="collapsed" desc="PROTOTYPES">
void inline ctnSensorCheck(sCTN_SENSOR *ctn_sensor);                      //Fonction a utiliser toutes les 100ms
int16_t inline convertTemperatureUSP10976(uint16_t adc_result);
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="VARIABLES">
const uint8_t ctn_sensor_lost_timeout = 50;     
const uint8_t ctn_sensor_level_timeout = 50;    
int16_t ctn_sensor_temp_value = 0;
// </editor-fold>

#endif	