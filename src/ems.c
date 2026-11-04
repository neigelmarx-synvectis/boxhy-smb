/**
 * @file ems.c
 * @brief Gestion énergétique (voir ems.h).
 */
#include <string.h>
#include "ems.h"
#include "generic.h"
#include "smb_config.h"
#include "fcs.h"

#define EMS_AVERAGE_SIZE 10

sEms ems;

static sBlockAverage battery_avg;
static sBlockAverage fcs_avg;

void Ems_Init(void)
{
    memset(&ems, 0, sizeof(ems));
    memset(&battery_avg, 0, sizeof(battery_avg));
    memset(&fcs_avg, 0, sizeof(fcs_avg));
}

void Ems_Update100ms(int16_t battery_current, int16_t fcs_current)
{
    bool ready;

    ready = BlockAverage_Push(&battery_avg, battery_current, EMS_AVERAGE_SIZE);
    BlockAverage_Push(&fcs_avg, fcs_current, EMS_AVERAGE_SIZE);
    if (ready)
    {
        ems.battery_current_moy = battery_avg.value;
        ems.fcs_current_moy = fcs_avg.value;
        ems.system_output_current = (int16_t)(ems.battery_current_moy + ems.fcs_current_moy);
    }
    /* XXX (origine) : ne pas calculer le courant pendant un court-circuit */
}

void Ems_Update1s(bool in_hybrid_mode, bool fcs_short_circuit_ongoing,
                  int16_t ibs0_max_charge_current, int16_t ibs1_max_charge_current)
{
    ems.battery_global_max_charge_current = Generic_MaxF(ibs0_max_charge_current, ibs1_max_charge_current);
    ems.battery_global_max_charge_current = (ems.battery_global_max_charge_current * EMS_BATTERY_CHARGE_RATIO * 2) * (-1);

    if (in_hybrid_mode && !fcs_short_circuit_ongoing)
    {
        if ((ems.system_output_current >= 0) && (ems.battery_global_max_charge_current > EMS_FCS_CURRENT_MARGIN))
            ems.requested_fcs_current = ems.system_output_current + EMS_FCS_CURRENT_MARGIN;
        else if ((ems.system_output_current >= 0) && (ems.battery_global_max_charge_current <= EMS_FCS_CURRENT_MARGIN))
            ems.requested_fcs_current = ems.system_output_current + ems.battery_global_max_charge_current;
        else
            ems.requested_fcs_current = EMS_FCS_CURRENT_MARGIN;

        if (ems.requested_fcs_current < EMS_FCS_CURRENT_MIN)
            ems.requested_fcs_current = EMS_FCS_CURRENT_MIN;
        else if (ems.requested_fcs_current > EMS_FCS_CURRENT_MAX)
            ems.requested_fcs_current = EMS_FCS_CURRENT_MAX;
        Fcs_SendCurrentLimit(ems.requested_fcs_current);
    }
    else
        ems.requested_fcs_current = 0;
}
