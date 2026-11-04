/**
 * @file can_rx.h
 * @brief Réception CAN : lecture d'un message et aiguillage vers le module concerné.
 *
 * Le décodage des signaux est fait par chaque module destinataire, à partir des tables de liaison
 * construites sur can_db.h (base CAN can database/BOXHY.dbc).
 */
#ifndef CAN_RX_H
#define CAN_RX_H

/** Initialise l'objet de réception. À appeler une fois au démarrage. */
void CanRx_Init(void);
/** Lit un message reçu (s'il y en a) et le traite. */
void CanRx_Process(void);

#endif /* CAN_RX_H */
