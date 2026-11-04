/**
 * @file tests/mocks/hal/uart_port.h
 * @brief Double de test de hal/uart_port.h : capture les octets émis, fournit des octets injectés.
 */
#ifndef HAL_UART_PORT_H
#define HAL_UART_PORT_H

#include <stdint.h>
#include <stdbool.h>

#define MOCK_UART_TX_MAX 256
#define MOCK_UART_RX_MAX 256

typedef struct
{
    uint8_t tx[MOCK_UART_TX_MAX];
    int tx_count;
    uint8_t rx[MOCK_UART_RX_MAX];
    int rx_count;
    int rx_head;
} sMockUart;

extern sMockUart mock_uart;

void MockUart_Reset(void);
void MockUart_PushRx(const uint8_t *bytes, int n);

void UartPort_WriteBytes(const uint8_t *bytes, uint8_t n);
bool UartPort_IsRxReady(void);
uint8_t UartPort_ReadByte(void);

#endif /* HAL_UART_PORT_H */
