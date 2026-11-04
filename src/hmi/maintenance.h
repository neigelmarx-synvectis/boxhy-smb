/**
 * @file maintenance.h
 * @brief Mode maintenance : demande d'entrée/sortie et commandes manuelles des sorties par CAN.
 */
#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/can_types.h"

typedef struct
{
    bool mode_request_flag; /**< mode maintenance demandé par la télémétrie */
} sMaintenance;

extern sMaintenance maintenance;

void Maintenance_Init(void);
/**
 * Traite 0x2C0100F (demande de mode) et 0x2C01D00..0x2C01D08 (commandes manuelles, actives
 * en SMB_MAINTENANCE seulement, octet 0 = 0x82).
 * @return true si l'ID a été traité
 */
bool Maintenance_HandleCanFrame(const CAN_MSG_OBJ *msg, uint8_t smb_step);

#endif /* MAINTENANCE_H */
