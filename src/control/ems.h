/**
 * @file ems.h
 * @brief Gestion énergétique : courants moyens, courant système et consigne de courant pile.
 *
 * En mode hybride, la pile à combustible fournit le courant consommé par le système et recharge
 * les batteries. L'EMS (Energy Management System) calcule chaque seconde la consigne de courant
 * envoyée à la FCCB à partir du courant moyen débité et de la capacité de charge des batteries.
 *
 * Appels (main.c) : Ems_Update100ms() dans la tâche 100 ms, Ems_Update1s() dans la tâche 1 s.
 * Unités : courants en 0,01 A ; courant batterie positif en décharge.
 */
#ifndef EMS_H
#define EMS_H

#include <stdint.h>
#include <stdbool.h>

/** Grandeurs calculées par la gestion énergétique. */
typedef struct
{
    int16_t battery_current_moy;      /**< moyenne 1 s du courant batterie (0.01 A) */
    int16_t fcs_current_moy;          /**< moyenne 1 s du courant pile (0.01 A) */
    int16_t system_output_current;    /**< batterie + pile (0.01 A) */
    int16_t battery_global_max_charge_current; /**< capacité de charge retenue (0.01 A, positif) */
    uint16_t requested_fcs_current;   /**< consigne envoyée à la FCCB (0.01 A), 0 hors mode hybride */
} sEms;

/** État de la gestion énergétique, lu par la télémétrie (can_tx.c). */
extern sEms ems;

/** Remet à zéro les moyennes et la consigne. */
void Ems_Init(void);
/**
 * Moyennes par blocs de 10 échantillons (toutes les 100 ms, nouvelle moyenne chaque seconde).
 * @param battery_current courant total batteries (0,01 A, positif en décharge)
 * @param fcs_current     courant de sortie pile (0,01 A)
 */
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
