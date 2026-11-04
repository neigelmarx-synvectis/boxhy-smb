/**
 * @file can_tx.h
 * @brief Émission CAN : télémétrie SMB, une trame par créneau TMR1 (cycle de 25 créneaux).
 */
#ifndef CAN_TX_H
#define CAN_TX_H

void CanTx_Init(void);
/** Émet la trame du créneau courant (s'il y en a une) et passe au créneau suivant. */
void CanTx_SendNext(void);

#endif /* CAN_TX_H */
