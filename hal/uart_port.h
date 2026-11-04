/**
 * @file hal/uart_port.h
 * @brief Liaison RS232 vers l'onduleur (UART1 MCC).
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

static inline bool UartPort_IsRxReady(void) { return UART1_IsRxReady(); }
static inline uint8_t UartPort_ReadByte(void) { return UART1_Read(); }

#endif /* HAL_UART_PORT_H */
