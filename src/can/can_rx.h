/**
 * @file can_rx.h
 * @brief Réception CAN : lecture d'un message et aiguillage vers le module concerné.
 */
#ifndef CAN_RX_H
#define CAN_RX_H

void CanRx_Init(void);
/** Lit un message reçu (s'il y en a) et le traite. */
void CanRx_Process(void);

#endif /* CAN_RX_H */
