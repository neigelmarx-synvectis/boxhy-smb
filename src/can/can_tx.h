/**
 * @file can_tx.h
 * @brief Émission CAN : télémétrie SMB, une trame par créneau TMR1 (cycle de 25 créneaux).
 *
 * La boucle principale appelle CanTx_SendNext() au plus une fois par période TMR1 (10 ms),
 * quand aucune tâche plus prioritaire n'est en attente et que la SMB a quitté l'état INIT.
 * Un cycle complet dure donc au moins 250 ms ; chaque trame est émise une fois par cycle.
 */
#ifndef CAN_TX_H
#define CAN_TX_H

/** Repart au créneau 0. À appeler une fois au démarrage. */
void CanTx_Init(void);
/** Émet la trame du créneau courant (s'il y en a une) et passe au créneau suivant. */
void CanTx_SendNext(void);

#endif /* CAN_TX_H */
