/**
 * @file hal/uart_port.h
 * @brief Liaison RS232 vers l'onduleur (UART1 MCC).
 *
 * Enveloppes `static inline` du driver MCC UART1, qui gère des files d'émission et de réception
 * alimentées par interruption. Utilisé par devices/inverter.c : émission des requêtes QMOD / QPIWS /
 * QPIGS toutes les 300 ms et lecture octet par octet dans l'interruption de réception.
 * Double de test : tests/mocks/hal/uart_port.h (capture des octets émis, injection des octets reçus).
 */
#ifndef HAL_UART_PORT_H
#define HAL_UART_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/uart1.h"

/** Émet n octets (bloquant sur la file d'émission MCC, comme l'ancien printf). */
static inline void UartPort_WriteBytes(const uint8_t *bytes, uint8_t n)
{
    uint8_t i;
    for (i = 0; i < n; i++)
        UART1_Write(bytes[i]);
}

/** Au moins un octet reçu attend dans la file de réception. */
static inline bool UartPort_IsRxReady(void) { return UART1_IsRxReady(); }
/** Retire un octet de la file de réception (bloquant si elle est vide : tester IsRxReady avant). */
static inline uint8_t UartPort_ReadByte(void) { return UART1_Read(); }

#endif /* HAL_UART_PORT_H */
