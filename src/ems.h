/**
 * @file ems.h
 * @brief Gestion énergétique : courants moyens, courant système et consigne de courant pile.
 */
#ifndef EMS_H
#define EMS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    int16_t battery_current_moy;      /**< moyenne 1 s du courant batterie (0.01 A) */
    int16_t fcs_current_moy;          /**< moyenne 1 s du courant pile (0.01 A) */
    int16_t system_output_current;    /**< batterie + pile (0.01 A) */
    int16_t battery_global_max_charge_current; /**< capacité de charge retenue (0.01 A, positif) */
    uint16_t requested_fcs_current;   /**< consigne envoyée à la FCCB (0.01 A), 0 hors mode hybride */
} sEms;

extern sEms ems;

void Ems_Init(void);
/** Moyennes glissantes par blocs de 10 (toutes les 100 ms). */
void Ems_Update100ms(int16_t battery_current, int16_t fcs_current);
/**
 * Consigne de courant pile, toutes les 1 s.
 * @param in_hybrid_mode            machine d'états en SMB_HYBRID_MODE
 * @param fcs_short_circuit_ongoing court-circuit pile en cours (pas de consigne)
 * @param ibs0_max_charge_current   courant de charge max batterie 0 (0.01 A, négatif)
 * @param ibs1_max_charge_current   idem batterie 1
 */
void Ems_Update1s(bool in_hybrid_mode, bool fcs_short_circuit_ongoing,
                  int16_t ibs0_max_charge_current, int16_t ibs1_max_charge_current);

#endif /* EMS_H */
