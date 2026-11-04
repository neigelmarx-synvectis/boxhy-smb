/**
 * @file ems.c
 * @brief Gestion énergétique (voir ems.h).
 *
 * Principe de la consigne pile (Ems_Update1s), seulement en mode hybride et hors court-circuit :
 *  - courant système = courant batteries + courant pile, moyennés sur 1 s ; c'est le courant
 *    réellement consommé par la charge ;
 *  - la pile doit couvrir ce courant et y ajouter une marge de recharge des batteries
 *    (EMS_FCS_CURRENT_MARGIN = 20 A), limitée par la capacité de charge annoncée par les IBS ;
 *  - la consigne est bornée entre EMS_FCS_CURRENT_MIN (8 A) et EMS_FCS_CURRENT_MAX (55 A).
 * Hors mode hybride, aucune consigne n'est envoyée et la valeur publiée vaut 0.
 */
#include <string.h>
#include "src/control/ems.h"
#include "src/utils/generic.h"
#include "src/smb_config.h"
#include "src/devices/fcs.h"

#define EMS_AVERAGE_SIZE 10 /* 10 échantillons de 100 ms : moyenne sur 1 s */

/* État public (déclaré extern dans ems.h). */
sEms ems;

/* Accumulateurs des moyennes par blocs (voir BlockAverage_Push dans generic.c). */
static sBlockAverage battery_avg;
static sBlockAverage fcs_avg;

/* Remet à zéro les moyennes, le courant système et la consigne. */
void Ems_Init(void)
{
    memset(&ems, 0, sizeof(ems));
    memset(&battery_avg, 0, sizeof(battery_avg));
    memset(&fcs_avg, 0, sizeof(fcs_avg));
}

/*
 * Accumule un échantillon de courant batterie et de courant pile. Toutes les 10 périodes (1 s),
 * les deux moyennes sont publiées et le courant système est recalculé.
 */
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

/*
 * Calcul et envoi de la consigne de courant pile (toutes les secondes).
 * Capacité de charge retenue : la moins restrictive des deux limites IBS (valeurs négatives,
 * d'où le maximum), x EMS_BATTERY_CHARGE_RATIO (0,7) x 2 batteries, rendue positive.
 */
void Ems_Update1s(bool in_hybrid_mode, bool fcs_short_circuit_ongoing,
                  int16_t ibs0_max_charge_current, int16_t ibs1_max_charge_current)
{
    ems.battery_global_max_charge_current = Generic_MaxF(ibs0_max_charge_current, ibs1_max_charge_current);
    ems.battery_global_max_charge_current = (ems.battery_global_max_charge_current * EMS_BATTERY_CHARGE_RATIO * 2) * (-1);

    if (in_hybrid_mode && !fcs_short_circuit_ongoing)
    {
        /* Le système consomme : la pile couvre la consommation + la recharge, celle-ci étant
         * limitée à la marge ou à la capacité de charge des batteries si elle est plus faible.
         * Le système recharge déjà les batteries (courant négatif) : consigne égale à la marge. */
        if ((ems.system_output_current >= 0) && (ems.battery_global_max_charge_current > EMS_FCS_CURRENT_MARGIN))
            ems.requested_fcs_current = ems.system_output_current + EMS_FCS_CURRENT_MARGIN;
        else if ((ems.system_output_current >= 0) && (ems.battery_global_max_charge_current <= EMS_FCS_CURRENT_MARGIN))
            ems.requested_fcs_current = ems.system_output_current + ems.battery_global_max_charge_current;
        else
            ems.requested_fcs_current = EMS_FCS_CURRENT_MARGIN;

        /* Bornes de fonctionnement de la pile */
        if (ems.requested_fcs_current < EMS_FCS_CURRENT_MIN)
            ems.requested_fcs_current = EMS_FCS_CURRENT_MIN;
        else if (ems.requested_fcs_current > EMS_FCS_CURRENT_MAX)
            ems.requested_fcs_current = EMS_FCS_CURRENT_MAX;
        Fcs_SendCurrentLimit(ems.requested_fcs_current);
    }
    else
        ems.requested_fcs_current = 0; /* pas de consigne hors mode hybride ou pendant un court-circuit */
}
