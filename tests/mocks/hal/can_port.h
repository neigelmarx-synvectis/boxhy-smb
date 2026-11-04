/**
 * @file tests/mocks/hal/can_port.h
 * @brief Double de test de hal/can_port.h : enregistre les trames émises, rejoue des trames injectées.
 */
#ifndef HAL_CAN_PORT_H
#define HAL_CAN_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/can_types.h"

#define MOCK_CAN_TX_MAX 64
#define MOCK_CAN_RX_MAX 16

typedef struct
{
    uint32_t id;
    uint8_t dlc;
    uint8_t idType;
    uint8_t frameType;
    uint8_t priority;
    uint8_t data[8];
} sMockCanMsg;

typedef struct
{
    sMockCanMsg tx[MOCK_CAN_TX_MAX];
    int tx_count;
    sMockCanMsg rx[MOCK_CAN_RX_MAX];
    int rx_count;
    int rx_head;
    int init_calls;
} sMockCan;

extern sMockCan mock_can;

void MockCan_Reset(void);
/** Place une trame dans la file de réception (data peut être NULL : octets à 0). */
void MockCan_PushRx(uint32_t id, uint8_t dlc, const uint8_t *data);
/** Dernière trame émise (NULL si aucune). */
const sMockCanMsg *MockCan_LastTx(void);
/** Trame émise d'indice i (NULL si hors limites). */
const sMockCanMsg *MockCan_Tx(int i);

void CanPort_Init(void);
void CanPort_Transmit(CAN_TX_PRIOIRTY priority, CAN_MSG_OBJ *msg);
bool CanPort_Receive(CAN_MSG_OBJ *msg);
uint8_t CanPort_ReceivedCount(void);

#endif /* HAL_CAN_PORT_H */
