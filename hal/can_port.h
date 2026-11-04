/**
 * @file hal/can_port.h
 * @brief Bus CAN (CAN1 MCC) : émission / réception d'objets message.
 *
 * Fines enveloppes `static inline` des fonctions du driver MCC CAN1 (aucun coût). Les modules
 * passent par ici plutôt que par can1.h pour pouvoir être compilés et testés sur PC : le double de
 * test (tests/mocks/hal/can_port.h) enregistre les trames émises et rejoue des trames injectées.
 * Le type CAN_MSG_OBJ (mcc_generated_files/can_types.h) est commun aux deux implémentations.
 */
#ifndef HAL_CAN_PORT_H
#define HAL_CAN_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/can1.h"

/** Active l'émission et la réception et passe le contrôleur en mode normal CAN 2.0.
 *  Appelée une fois au démarrage (main.c), après SYSTEM_Initialize(). */
static inline void CanPort_Init(void)
{
    CAN1_TransmitEnable();
    CAN1_ReceiveEnable();
    CAN1_OperationModeSet(CAN_NORMAL_2_0_MODE);
}

/** Dépose une trame dans une boîte d'émission (le statut de retour est ignoré, comme à l'origine). */
static inline void CanPort_Transmit(CAN_TX_PRIOIRTY priority, CAN_MSG_OBJ *msg) { (void)CAN1_Transmit(priority, msg); }
/** Recopie le plus ancien message reçu dans `msg` (8 octets dans msg->data). false si aucun. */
static inline bool CanPort_Receive(CAN_MSG_OBJ *msg) { return CAN1_Receive(msg); }
/** Nombre de messages en attente dans les buffers de réception (testé à chaque tour de boucle). */
static inline uint8_t CanPort_ReceivedCount(void) { return CAN1_ReceivedMessageCountGet(); }

#endif /* HAL_CAN_PORT_H */
