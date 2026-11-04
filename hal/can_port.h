/**
 * @file hal/can_port.h
 * @brief Bus CAN (CAN1 MCC) : émission / réception d'objets message.
 */
#ifndef HAL_CAN_PORT_H
#define HAL_CAN_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/can1.h"

static inline void CanPort_Init(void)
{
    CAN1_TransmitEnable();
    CAN1_ReceiveEnable();
    CAN1_OperationModeSet(CAN_NORMAL_2_0_MODE);
}

/** Dépose une trame dans une boîte d'émission (le statut de retour est ignoré, comme à l'origine). */
static inline void CanPort_Transmit(CAN_TX_PRIOIRTY priority, CAN_MSG_OBJ *msg) { (void)CAN1_Transmit(priority, msg); }
static inline bool CanPort_Receive(CAN_MSG_OBJ *msg) { return CAN1_Receive(msg); }
static inline uint8_t CanPort_ReceivedCount(void) { return CAN1_ReceivedMessageCountGet(); }

#endif /* HAL_CAN_PORT_H */
