/**
 * @file maintenance.h
 * @brief Mode maintenance : demande d'entrée/sortie et commandes manuelles des sorties par CAN.
 *
 * Depuis l'outil de télémétrie, un opérateur peut demander le mode maintenance (état SMB 40) puis
 * piloter individuellement relais, buzzer, LED et ventilateurs. Chaque commande doit porter la clé
 * 0x82 dans l'octet 0 et n'est exécutée qu'en mode maintenance.
 *
 * Appel : Maintenance_HandleCanFrame() depuis la réception CAN (can_rx.c).
 */
#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/can_types.h"

/** État du mode maintenance, lu par la machine d'états. */
typedef struct
{
    bool mode_request_flag; /**< mode maintenance demandé par la télémétrie */
} sMaintenance;

/** État du mode maintenance. */
extern sMaintenance maintenance;

/** Annule toute demande de maintenance. */
void Maintenance_Init(void);
/**
 * Traite 0x2C0100F (demande de mode) et 0x2C01D00..0x2C01D08 (commandes manuelles, actives
 * en SMB_MAINTENANCE seulement, octet 0 = 0x82).
 * @param msg      trame reçue
 * @param smb_step état SMB courant
 * @return true si l'ID a été traité
 */
bool Maintenance_HandleCanFrame(const CAN_MSG_OBJ *msg, uint8_t smb_step);

#endif /* MAINTENANCE_H */
