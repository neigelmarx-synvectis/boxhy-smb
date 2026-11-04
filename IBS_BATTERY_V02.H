/* EXPLICATIONS / DETAILS :
 * Initialiser la batterie avec 'ibs_battery_init'.
 * Parametrer 'IBS_BATTERY_NUMBER' en fonction du nombre de batterie
 * Toutes les secondes, appeler 'ibs_battery_keep_alive' pour envoyer le keep-alive aux batteries
 * Lors de la reception CAN, utiliser 'read_ibs_battery_info' en fonction de la batterie concernee et de l'ID de la batterie (voir exemple)
 */

/* HISTORIQUE DES VERSIONS :
 * V01 : JV - 2021/08/23 - Version initiale
 * V02 : JV - 2022/02/07 - Version modifiée pour XC16

*/

#include <math.h>

//typedef union 
//{
//    struct
//    {
//        bool b0:1, b1:1, b2:1, b3:1, b4:1, b5:1, b6:1, b7:1;
//    } sFLAGSBITS;
//    uint8_t byte;
//} uFLAGSBYTE;


// <editor-fold defaultstate="collapsed" desc="STRUCTURE">
typedef struct
{
    uint16_t voltage;				//Tension batterie (0.01V)
    uint16_t min_cell_voltage;	//Tension de cellule minimum (mV)
    uint16_t max_cell_voltage;	//Tension de cellule maximum (mV)
    
    int16_t output_current;					//Courant batterie (0.01A)
    int8_t max_discharge_current;	//Courant de dÃ©charge maximum (1A)
    int8_t max_charge_current;		//Courant de charge maximum (1A)
    
    int8_t min_cell_temperature;		//Temperature de cellule minimum (1°C)
    int8_t max_cell_temperature;		//Temperature de cellule maximum (1°C)
    int8_t avg_cell_temperature;		//Temperature moyenne des cellules (1°C)
    int8_t min_board_temperature;     //Temperature de la carte minimum (1°C)
    int8_t max_board_temperature;     //Temperature de la carte maximum (1°C)
    int8_t avg_board_temperature;     //Temperature moyenne de la carte (1°C)
    
    uint8_t soc;	//SOC de la batterie (%)
    uint8_t soh;	//SOH de la batterie (%)
    
    bool system_running_flag;		//0 : Batterie en standby / 1 : Batterie en fonctionnement
    bool balancing_flag;			//1 : Batterie en balancing
    bool low_soc_warning_flag;		//1 : Flag d'avertissement SOC bas
    bool error_flag;				//0 : Pas d'erreur / 1 : Au moins une erreur detectee
    bool power_output_1_state_flag;
    bool power_output_2_state_flag;
    
	//Flags d'erreur
    bool overvoltage_error_flag;	
    bool undervoltage_error_flag;
    bool overtemperature_error_flag;
    bool undertemperature_error_flag;
    bool overload_error_flag;
    bool shortcircuit_error_flag;
    bool param_timeout_error_flag;
    bool board_values_uptodate_error_flag;
    bool current_uptodate_error_flag;
    bool freesb_shutdown_error_flag;
    bool open_connection_error_flag;
    bool pdu_failure_error_flag;
    bool pdu_overtemperature_error_flag;
    bool encrypt_key_error_flag;
    bool pdu_lock_error_flag;
    bool init_timestamp_error_flag;
    bool data_corrupted_error_flag;
    bool precharge_error_flag;
    bool interlock_error_flag;
    bool var_type_error_flag;
    bool imd_error_flag;
    bool ov_safety_fuse_error_flag;
    bool association_error_flag;
    
    uint8_t watchdog_cnt;
    bool watchdog_error_flag;
}sIBS;
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="PROTOTYPES">
void ibsReadInfo(sIBS *ibs_battery);
void ibsInit(sIBS *ibs_battery);
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="VARIABLES">
uint8_t ibs_info_index = 0;
uint8_t ibs_keep_alive_index = 0;
uint8_t ibs_info_data[8] = {0,0,0,0,0,0,0,0};
float mant_sum;
float expo_pow;

uFLAGSBYTE ibs_flagsbyte;
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="EXEMPLE">
/* METTRE LES DIFFERENTS MORCEAUX AUX BON ENDROITS DANS LE SOFT
  
    // <editor-fold defaultstate="collapsed" desc="VARIABLES">
    struct ibs_battery_struct ibs_battery_0;
    struct ibs_battery_struct ibs_battery_1;
    struct ibs_battery_struct ibs_battery_2;
    struct ibs_battery_struct ibs_battery_3;
    struct ibs_battery_struct ibs_battery_4;
    struct ibs_battery_struct ibs_battery_5;
    struct ibs_battery_struct ibs_battery_6;
    struct ibs_battery_struct ibs_battery_7;
    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="INIT MAIN">
    ibs_battery_init(ibs_battery_0);
    ibs_battery_init(ibs_battery_1);
    ibs_battery_init(ibs_battery_2);
    ibs_battery_init(ibs_battery_3);
    ibs_battery_init(ibs_battery_4);
    ibs_battery_init(ibs_battery_5);
    ibs_battery_init(ibs_battery_6);
    ibs_battery_init(ibs_battery_7);
    // </editor-fold>
 
    // <editor-fold defaultstate="collapsed" desc="RECEPTION CAN">
    if(can_getd(rx_id,&input_buffer[0], rx_len, rxstat))
    { 
        if((rx_id & 0x18FF00D0) == 0x18FF00D0)
        {
            ibs_battery_info_index = (unsigned int8)((rx_id & 0x0000FF00) >> 8);

            if((rx_id & 0x0000000F) == 0) read_ibs_battery_info(ibs_battery_0);
            else if((rx_id & 0x0000000F) == 1) read_ibs_battery_info(ibs_battery_1);
            else if((rx_id & 0x0000000F) == 2) read_ibs_battery_info(ibs_battery_2);
            else if((rx_id & 0x0000000F) == 3) read_ibs_battery_info(ibs_battery_3);
            else if((rx_id & 0x0000000F) == 4) read_ibs_battery_info(ibs_battery_4);
            else if((rx_id & 0x0000000F) == 5) read_ibs_battery_info(ibs_battery_5);
            else if((rx_id & 0x0000000F) == 6) read_ibs_battery_info(ibs_battery_6);
            else if((rx_id & 0x0000000F) == 7) read_ibs_battery_info(ibs_battery_7);
        }
    }
    // </editor-fold>
  
    // <editor-fold defaultstate="collapsed" desc="TOUTES LES 1s">
    else if(tmr0_tick_1s_flag == 1)
    {
        ibs_battery_keep_alive();

        tmr0_tick_1s_flag = 0;
    }
    // </editor-fold>   
  
    // <editor-fold defaultstate="collapsed" desc="ENVOI CAN">
    if(can_frame_sending_flag == 1) //Si autorisé à envoyer une trame
    {
        for(int i = 0;i < 8;i++) output_buffer[i] = 0;  //RAZ output_buffer

        switch(can_frame_ind)
        {   
            case 0: //GENERAL 
                output_buffer[0] = (int8)ibs_battery_0.soc;
                output_buffer[1] = (int8)ibs_battery_0.soh;
                output_buffer[2] = (int8)(ibs_battery_0.avg_cell_temperature / 100);
                output_buffer[3] = (int8)(0 | ibs_battery_0.system_running_flag | (ibs_battery_0.error_flag << 1) 
                                            | (ibs_battery_0.low_soc_warning_flag << 2) | (ibs_battery_0.balancing_flag << 3));
                output_buffer[4] = (int8)ibs_battery_0.voltage;
                output_buffer[5] = (int8)(ibs_battery_0.voltage >> 8);
                output_buffer[6] = (int8)ibs_battery_0.current;
                output_buffer[7] = (int8)(ibs_battery_0.current >> 8);
                can_putd(0x150, output_buffer, 8, 0, FALSE, FALSE); 
                break;
            case 1: //TEMPERATURES
                output_buffer[0] = (int8)(ibs_battery_0.min_cell_temperature / 100);
                output_buffer[1] = (int8)(ibs_battery_0.max_cell_temperature / 100);
                output_buffer[2] = (int8)(ibs_battery_0.avg_cell_temperature / 100);
                output_buffer[3] = (int8)(ibs_battery_0.min_board_temperature / 100);
                output_buffer[4] = (int8)(ibs_battery_0.max_board_temperature / 100);
                output_buffer[5] = (int8)(ibs_battery_0.avg_board_temperature / 100);
                can_putd(0x151, output_buffer, 6, 0, FALSE, FALSE); 
                break;    
            case 2: //ELEC
                output_buffer[0] = (int8)ibs_battery_0.max_cell_voltage;
                output_buffer[1] = (int8)(ibs_battery_0.max_cell_voltage >> 8);
                output_buffer[2] = (int8)ibs_battery_0.min_cell_voltage;
                output_buffer[3] = (int8)(ibs_battery_0.min_cell_voltage >> 8);
                output_buffer[4] = (int8)ibs_battery_0.max_discharge_current;
                output_buffer[5] = (int8)(ibs_battery_0.max_discharge_current >> 8);
                output_buffer[6] = (int8)ibs_battery_0.max_charge_current;
                output_buffer[7] = (int8)(ibs_battery_0.max_charge_current >> 8);
                can_putd(0x152, output_buffer, 8, 0, FALSE, FALSE); 
                break;    
            case 3: //ERREURS
                output_buffer[0] = (int8)(0 | ibs_battery_0.overvoltage_error_flag | (ibs_battery_0.undervoltage_error_flag << 1) 
                                            | (ibs_battery_0.overtemperature_error_flag << 2) | (ibs_battery_0.undertemperature_error_flag << 3)
                                            | (ibs_battery_0.overload_error_flag << 4) | (ibs_battery_0.shortcircuit_error_flag << 5) 
                                            | (ibs_battery_0.param_timeout_error_flag << 6) | (ibs_battery_0.board_values_uptodate_error_flag << 7));   
                output_buffer[1] = (int8)(0 | ibs_battery_0.current_uptodate_error_flag | (ibs_battery_0.freesb_shutdown_error_flag << 1) 
                                            | (ibs_battery_0.open_connection_error_flag << 2) | (ibs_battery_0.pdu_failure_error_flag << 3)
                                            | (ibs_battery_0.pdu_overtemperature_error_flag << 4) | (ibs_battery_0.encrypt_key_error_flag << 5) 
                                            | (ibs_battery_0.pdu_lock_error_flag << 6) | (ibs_battery_0.init_timestamp_error_flag << 7));   
                output_buffer[2] = (int8)(0 | ibs_battery_0.data_corrupted_error_flag | (ibs_battery_0.precharge_error_flag << 1) 
                                            | (ibs_battery_0.interlock_error_flag << 2) | (ibs_battery_0.var_type_error_flag << 3)
                                            | (ibs_battery_0.imd_error_flag << 4) | (ibs_battery_0.ov_safety_fuse_error_flag << 5) 
                                            | (ibs_battery_0.association_error_flag << 6));   
                can_putd(0x153, output_buffer, 3, 0, FALSE, FALSE); 
                break;    
            default:
                break;
        }
        if(can_frame_ind > 23) can_frame_ind = 0;  //RAZ 
        else can_frame_ind++; //Passage à l'indice suivant

        can_frame_sending_flag = 0; //RAZ
    }
// </editor-fold>
*/

