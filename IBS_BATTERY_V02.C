//EXPLICATIONS & HISTORIQUE DES VERSIONS DANS LE FICHIER .H :
#include "IBS_BATTERY_V02.H"

void ibsReadInfo(sIBS *ibs_battery)
{
    switch(ibs_info_index)
    {
        case 0x01: //VOLTAGE
            ibs_battery->voltage = (uint16_t)((0 | (int32_t)ibs_info_data[3] | ((int32_t)ibs_info_data[2] << 8) | ((int32_t)ibs_info_data[1] << 16) | ((int32_t)ibs_info_data[0] << 24)) / 10); //0.01V //10.0); //0.01V
//            ibs_battery->max_cell_voltage = (int16_t)(ibs_info_data[5] | ((int16_t)ibs_info_data[4] << 8)); //1mV
//            ibs_battery->min_cell_voltage = (int16_t)(ibs_info_data[7] | ((int16_t)ibs_info_data[6] << 8)); //1mV
            break;
        case 0x02: //CURRENT
            expo_pow = pow(2.0, (((ibs_info_data[0] & 0x7F) << 1 | ((ibs_info_data[1] & 0x80) >> 7)) - 127.0));

            mant_sum = 1.0;
            ibs_flagsbyte.byte = ibs_info_data[1];
            if(ibs_flagsbyte.sFLAGSBITS.b6) mant_sum += 0.50000000000000;
            if(ibs_flagsbyte.sFLAGSBITS.b5) mant_sum += 0.25000000000000;
            if(ibs_flagsbyte.sFLAGSBITS.b4) mant_sum += 0.12500000000000;
            if(ibs_flagsbyte.sFLAGSBITS.b3) mant_sum += 0.06250000000000;
            if(ibs_flagsbyte.sFLAGSBITS.b2) mant_sum += 0.03125000000000;
            if(ibs_flagsbyte.sFLAGSBITS.b1) mant_sum += 0.01562500000000;
            if(ibs_flagsbyte.sFLAGSBITS.b0) mant_sum += 0.00781250000000;
            ibs_flagsbyte.byte = ibs_info_data[2];
            if(ibs_flagsbyte.sFLAGSBITS.b7) mant_sum += 0.00390625000000;
            if(ibs_flagsbyte.sFLAGSBITS.b6) mant_sum += 0.00195312500000;
            if(ibs_flagsbyte.sFLAGSBITS.b5) mant_sum += 0.00097656250000;
            if(ibs_flagsbyte.sFLAGSBITS.b4) mant_sum += 0.00048828125000;
//            if(ibs_flagsbyte.sFLAGSBITS.b3) mant_sum += 0.00024414062500;
//            if(ibs_flagsbyte.sFLAGSBITS.b2) mant_sum += 0.00012207031250;
//            if(ibs_flagsbyte.sFLAGSBITS.b1) mant_sum += 0.00006103515625;
//            if(ibs_flagsbyte.sFLAGSBITS.b0) mant_sum += 0.00003051757813;
            
            ibs_flagsbyte.byte = ibs_info_data[0];
            if(ibs_flagsbyte.sFLAGSBITS.b7) ibs_battery->output_current = (expo_pow * mant_sum * 100);
            else ibs_battery->output_current = (expo_pow * mant_sum * (-100));    
            break;
        case 0x03: //CELL TEMPERATURE
//            ibs_battery->min_cell_temperature = (int8_t)((ibs_info_data[1] | ((int16_t)ibs_info_data[0] << 8)) / 10); //1°C
//            ibs_battery->max_cell_temperature = (int8_t)((ibs_info_data[3] | ((int16_t)ibs_info_data[2] << 8)) / 10); //1°C
            ibs_battery->avg_cell_temperature = (int8_t)((ibs_info_data[5] | ((int16_t)ibs_info_data[4] << 8)) / 10); //1°C
            break;
//        case 0x04: //BOARD TEMPERATURE
//            ibs_battery->min_board_temperature = (int8_t)((ibs_info_data[1] | ((int16_t)ibs_info_data[0] << 8)) / 10); //1°C
//            ibs_battery->max_board_temperature = (int8_t)((ibs_info_data[3] | ((int16_t)ibs_info_data[2] << 8)) / 10); //1°C
//            ibs_battery->avg_board_temperature = (int8_t)((ibs_info_data[5] | ((int16_t)ibs_info_data[4] << 8)) / 10); //1°C
//            break;
        case 0x05: //STATE
            ibs_battery->soc = (uint8_t)((ibs_info_data[1] | ((int16_t)ibs_info_data[0] << 8)) / 100); //1%
//            ibs_battery->soh = (uint8_t)((ibs_info_data[3] | ((int16_t)ibs_info_data[2] << 8)) / 100); //1%
            break;
//        case 0x0C: //CHARGE/DISCHARGE CURRENT
//            ibs_battery->max_discharge_current = (int8_t)((0 | (int32_t)ibs_info_data[3] | ((int32_t)ibs_info_data[2] << 8) | ((int32_t)ibs_info_data[1] << 16) | ((int32_t)ibs_info_data[0] << 24)) / 1000.0); //1A
//            ibs_battery->max_charge_current = (int8_t)((0 | (int32_t)ibs_info_data[7] | ((int32_t)ibs_info_data[6] << 8) | ((int32_t)ibs_info_data[5] << 16) | ((int32_t)ibs_info_data[4] << 24)) / 1000.0); //1A
//            break;
        case 0x07: //INFOS
            ibs_flagsbyte.byte = ibs_info_data[1];
//            if(ibs_flagsbyte.sFLAGSBITS.b0) ibs_battery->system_running_flag = 1;
//            else ibs_battery->system_running_flag = 0;
//            if(ibs_flagsbyte.sFLAGSBITS.b1) ibs_battery->balancing_flag = 1;
//            else ibs_battery->balancing_flag = 0;
            ibs_battery->system_running_flag = ibs_flagsbyte.sFLAGSBITS.b0;
            ibs_battery->balancing_flag = ibs_flagsbyte.sFLAGSBITS.b1;
            
            ibs_flagsbyte.byte = ibs_info_data[2];
//            if(ibs_flagsbyte.sFLAGSBITS.b1) ibs_battery->error_flag = 0;
//            else ibs_battery->error_flag = 1;
//            if(ibs_flagsbyte.sFLAGSBITS.b5) ibs_battery->low_soc_warning_flag = 1;
//            else ibs_battery->low_soc_warning_flag = 0;
            ibs_battery->error_flag = !ibs_flagsbyte.sFLAGSBITS.b1;
            ibs_battery->low_soc_warning_flag = ibs_flagsbyte.sFLAGSBITS.b5;
            
            break;
//        case 0x10: //OVERVOLTAGE ERROR
//            if(ibs_info_data[0] > 0) ibs_battery->overvoltage_error_flag = 1;
//            else ibs_battery->overvoltage_error_flag = 0;
//            break;
//        case 0x11: //UNDERVOLTAGE ERROR
//            if(ibs_info_data[0] > 0) ibs_battery->undervoltage_error_flag = 1;
//            else ibs_battery->undervoltage_error_flag = 0;
//            break;
//        case 0x12: //OVERTEMPERATURE ERROR
//            if(ibs_info_data[0] > 0) ibs_battery->overtemperature_error_flag = 1;
//            else ibs_battery->overtemperature_error_flag = 0;
//            break;
//        case 0x13: //UNDERTEMPERATURE ERROR
//            if(ibs_info_data[0] > 0) ibs_battery->undertemperature_error_flag = 1;
//            else ibs_battery->undertemperature_error_flag = 0;
//            break;
//        case 0x14: //OVERLOAD ERROR
//            if(ibs_info_data[0] > 0) ibs_battery->overload_error_flag = 1;
//            else ibs_battery->overload_error_flag = 0;
//            break;
//        case 0x15: //SHORTCIRCUIT ERROR
//            if(ibs_info_data[0] > 0) ibs_battery->shortcircuit_error_flag = 1;
//            else ibs_battery->shortcircuit_error_flag = 0;
//            break;
//        case 0x16: //BATTERY PARAM TIMEOUT
//            if(ibs_info_data[0] > 0) ibs_battery->param_timeout_error_flag = 1;
//            else ibs_battery->param_timeout_error_flag = 0;
//            break;
//        case 0x17: //BOARD VALUES UPTODATE
//            if(ibs_info_data[0] > 0) ibs_battery->board_values_uptodate_error_flag = 1;
//            else ibs_battery->board_values_uptodate_error_flag = 0;
//            break;
//        case 0x18: //CURRENT UPTODATE
//            if(ibs_info_data[0] > 0) ibs_battery->current_uptodate_error_flag = 1;
//            else ibs_battery->current_uptodate_error_flag = 0;
//            break;
//        case 0x19: //FREESB SHUTDOWN
//            if(ibs_info_data[0] > 0) ibs_battery->freesb_shutdown_error_flag = 1;
//            else ibs_battery->freesb_shutdown_error_flag = 0;
//            break;
//        case 0x1A: //OPEN CONNECTION
//            if(ibs_info_data[0] > 0) ibs_battery->open_connection_error_flag = 1;
//            else ibs_battery->open_connection_error_flag = 0;
//            break;
//        case 0x1B: //PDU FAILURE
//            if(ibs_info_data[0] > 0) ibs_battery->pdu_failure_error_flag = 1;
//            else ibs_battery->pdu_failure_error_flag = 0;
//            break;
//        case 0x1C: //PDU OVERTEMPERATURE
//            if(ibs_info_data[0] > 0) ibs_battery->pdu_overtemperature_error_flag = 1;
//            else ibs_battery->pdu_overtemperature_error_flag = 0;
//            break;
//        case 0x1D: //ENCRYPTION KEY NOT VALID
//            if(ibs_info_data[0] > 0) ibs_battery->encrypt_key_error_flag = 1;
//            else ibs_battery->encrypt_key_error_flag = 0;
//            break;
//        case 0x1E: //PDU LOCK
//            if(ibs_info_data[0] > 0) ibs_battery->pdu_lock_error_flag = 1;
//            else ibs_battery->pdu_lock_error_flag = 0;
//            break;
//        case 0x1F: //WAITING INIT TIMESTAMP
//            if(ibs_info_data[0] > 0) ibs_battery->init_timestamp_error_flag = 1;
//            else ibs_battery->init_timestamp_error_flag = 0;
//            break;
//        case 0x20: //DATA CORRUPTED
//            if(ibs_info_data[0] > 0) ibs_battery->data_corrupted_error_flag = 1;
//            else ibs_battery->data_corrupted_error_flag = 0;
//            break;
//        case 0x21: //PRECHARGE ERROR
//            if(ibs_info_data[0] > 0) ibs_battery->precharge_error_flag = 1;
//            else ibs_battery->precharge_error_flag = 0;
//            break;
//        case 0x22: //INTERLOCK ERROR
//            if(ibs_info_data[0] > 0) ibs_battery->interlock_error_flag = 1;
//            else ibs_battery->interlock_error_flag = 0;
//            break;
//        case 0x23: //VAR TYPE MISMATCH
//            if(ibs_info_data[0] > 0) ibs_battery->var_type_error_flag = 1;
//            else ibs_battery->var_type_error_flag = 0;
//            break;
//        case 0x24: //IMD ERROR
//            if(ibs_info_data[0] > 0) ibs_battery->imd_error_flag = 1;
//            else ibs_battery->imd_error_flag = 0;
//            break;
//        case 0x25: //OV SAFETY FUSE
//            if(ibs_info_data[0] > 0) ibs_battery->ov_safety_fuse_error_flag = 1;
//            else ibs_battery->ov_safety_fuse_error_flag = 0;
//            break;
//        case 0x26: //ASSOCIATION
//            if(ibs_info_data[0] > 0) ibs_battery->association_error_flag = 1;
//            else ibs_battery->association_error_flag = 0;
//            break;
        default:
            break;
    }
    
    ibs_info_index = 0;
}

void ibsBatteryInit(sIBS *ibs_battery)
{
    ibs_battery->voltage = 0;
    ibs_battery->max_cell_voltage = 0;
    ibs_battery->min_cell_voltage = 0;
    
    ibs_battery->output_current = 0;
    ibs_battery->max_discharge_current = 0;
    ibs_battery->max_charge_current = 0;
    
    ibs_battery->min_cell_temperature = 0;
    ibs_battery->max_cell_temperature = 0;
    ibs_battery->avg_cell_temperature = 0;
    ibs_battery->min_board_temperature = 0;
    ibs_battery->max_board_temperature = 0;
    ibs_battery->avg_board_temperature = 0;
    
    ibs_battery->soc = 0;
    ibs_battery->soh = 0;
    
    ibs_battery->system_running_flag = 0;
    ibs_battery->balancing_flag = 0;
    ibs_battery->low_soc_warning_flag = 0;
    ibs_battery->error_flag = 0;
    ibs_battery->power_output_1_state_flag = 0;
    ibs_battery->power_output_2_state_flag = 0;
    
    ibs_battery->overvoltage_error_flag = 0;
    ibs_battery->undervoltage_error_flag = 0;
    ibs_battery->overtemperature_error_flag = 0;
    ibs_battery->undertemperature_error_flag = 0;
    ibs_battery->overload_error_flag = 0;
    ibs_battery->shortcircuit_error_flag = 0;
    ibs_battery->param_timeout_error_flag = 0;
    ibs_battery->board_values_uptodate_error_flag = 0;
    ibs_battery->current_uptodate_error_flag = 0;
    ibs_battery->freesb_shutdown_error_flag = 0;
    ibs_battery->open_connection_error_flag = 0;
    ibs_battery->pdu_failure_error_flag = 0;
    ibs_battery-> pdu_overtemperature_error_flag = 0;
    ibs_battery->encrypt_key_error_flag = 0;
    ibs_battery->pdu_lock_error_flag = 0;
    ibs_battery->init_timestamp_error_flag = 0;
    ibs_battery->data_corrupted_error_flag = 0;
    ibs_battery->precharge_error_flag = 0;
    ibs_battery->interlock_error_flag = 0;
    ibs_battery-> var_type_error_flag = 0;
    ibs_battery->imd_error_flag = 0;
    ibs_battery->ov_safety_fuse_error_flag = 0;
    ibs_battery->association_error_flag = 0;
    
    ibs_battery->watchdog_cnt = 0;
    ibs_battery->watchdog_error_flag = 0;
    
}

