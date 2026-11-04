/**
 * @file ctn_sensor.c
 * @brief Sonde de température CTN (voir ctn_sensor.h).
 */
#include "src/measures/ctn_sensor.h"

void ctnSensorCheck(sCTN_SENSOR *ctn_sensor)
{
    if (ctn_sensor->value == CTN_SENSOR_LOST_VALUE)
    {
        ctn_sensor->lost_cnt++;
        if (ctn_sensor->lost_cnt > CTN_SENSOR_LOST_TIMEOUT)
            ctn_sensor->lost_error_flag = 1;
    }
    else
    {
        ctn_sensor->lost_cnt = 0;
        ctn_sensor->value_8 = ctn_sensor->value / 100;

        if (ctn_sensor->value_8 < ctn_sensor->low_level)
        {
            ctn_sensor->low_level_cnt++;
            if (ctn_sensor->low_level_cnt > CTN_SENSOR_LEVEL_TIMEOUT)
                ctn_sensor->low_level_error_flag = 1;
        }
        else
        {
            ctn_sensor->low_level_cnt = 0;

            if (ctn_sensor->value_8 > ctn_sensor->warning_high_level)
                ctn_sensor->high_level_warning_flag = 1;
            else
                ctn_sensor->high_level_warning_flag = 0;

            if (ctn_sensor->value_8 > ctn_sensor->high_level)
            {
                ctn_sensor->high_level_cnt++;
                if (ctn_sensor->high_level_cnt > CTN_SENSOR_LEVEL_TIMEOUT)
                    ctn_sensor->high_level_error_flag = 1;
            }
            else
                ctn_sensor->high_level_cnt = 0;
        }
    }
    if ((ctn_sensor->low_level_error_flag) || (ctn_sensor->high_level_error_flag) || (ctn_sensor->lost_error_flag))
        ctn_sensor->error_flag = 1;
}

void ctnSensorInit(sCTN_SENSOR *ctn_sensor)
{
    ctn_sensor->value = 0;
    ctn_sensor->value_8 = 0;

    ctn_sensor->warning_high_level = 50;
    ctn_sensor->high_level = 70;
    ctn_sensor->low_level = -5;

    ctn_sensor->high_level_cnt = 0;
    ctn_sensor->low_level_cnt = 0;
    ctn_sensor->lost_cnt = 0;

    ctn_sensor->high_level_warning_flag = 0;
    ctn_sensor->high_level_error_flag = 0;
    ctn_sensor->low_level_error_flag = 0;
    ctn_sensor->lost_error_flag = 0;
    ctn_sensor->error_flag = 0;
}

int16_t convertTemperatureUSP10976(uint16_t adc_result)
{
    int16_t temp;

    if (adc_result < 70)         temp = CTN_SENSOR_LOST_VALUE;
    else if (adc_result <= 152)  temp = (int16_t)(adc_result * 16.52 - 6007.0);
    else if (adc_result <= 276)  temp = (int16_t)(adc_result * 7.7 - 4710.0);
    else if (adc_result <= 470)  temp = (int16_t)(adc_result * 4.94 - 3909.0);
    else if (adc_result <= 686)  temp = (int16_t)(adc_result * 3.57 - 3245.0);
    else if (adc_result <= 960)  temp = (int16_t)(adc_result * 2.88 - 2760.0);
    else if (adc_result <= 1370) temp = (int16_t)(adc_result * 2.44 - 2329.0);
    else if (adc_result <= 2477) temp = (int16_t)(adc_result * 2.24 - 2068.0);
    else if (adc_result <= 2980) temp = (int16_t)(adc_result * 2.78 - 3411.0);
    else if (adc_result <= 3347) temp = (int16_t)(adc_result * 3.90 - 6777.0);
    else if (adc_result <= 3519) temp = (int16_t)(adc_result * 5.09 - 10732.0);
    else if (adc_result <= 3664) temp = (int16_t)(adc_result * 6.80 - 16739.0);
    else if (adc_result <= 3761) temp = (int16_t)(adc_result * 9.11 - 25191.0);
    else if (adc_result <= 3842) temp = (int16_t)(adc_result * 12.21 - 36828.0);
    else if (adc_result <= 3896) temp = (int16_t)(adc_result * 16.57 - 53582.0);
    else if (adc_result <= 3941) temp = (int16_t)(adc_result * 22.02 - 74813.0);
    else if (adc_result <= 3975) temp = (int16_t)(adc_result * 29.49 - 104250.0);
    else if (adc_result <= 4001) temp = (int16_t)(adc_result * 39.20 - 142860.0);
    else if (adc_result <= 4025) temp = (int16_t)(adc_result * 51.67 - 192760.0);
    else                         temp = CTN_SENSOR_LOST_VALUE;

    return temp;
}
