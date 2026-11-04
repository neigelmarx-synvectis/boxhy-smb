/**
 * @file tests/mocks/hal/mocks.c
 * @brief Implémentation des doubles de test de la couche HAL.
 */
#include <string.h>
#include "hal/board_io.h"
#include "hal/can_port.h"
#include "hal/uart_port.h"
#include "hal/adc_port.h"

/* ---- board_io ------------------------------------------------------------------ */
sMockBoard mock_board;

void MockBoard_Reset(void) { memset(&mock_board, 0, sizeof(mock_board)); }
void Board_Init(void) { mock_board.init_calls++; }
void Board_SetFansDutyCycle(uint8_t duty_cycle)
{
    if (duty_cycle > 100)
        duty_cycle = 100;
    mock_board.fans_duty_cycle = duty_cycle;
    mock_board.fans_set_calls++;
}

/* ---- can_port ------------------------------------------------------------------- */
sMockCan mock_can;

void MockCan_Reset(void) { memset(&mock_can, 0, sizeof(mock_can)); }

void MockCan_PushRx(uint32_t id, uint8_t dlc, const uint8_t *data)
{
    sMockCanMsg *m;
    if (mock_can.rx_count >= MOCK_CAN_RX_MAX)
        return;
    m = &mock_can.rx[mock_can.rx_count++];
    memset(m, 0, sizeof(*m));
    m->id = id;
    m->dlc = dlc;
    if (data != NULL)
        memcpy(m->data, data, dlc);
}

const sMockCanMsg *MockCan_LastTx(void)
{
    return mock_can.tx_count ? &mock_can.tx[mock_can.tx_count - 1] : NULL;
}

const sMockCanMsg *MockCan_Tx(int i)
{
    return (i >= 0 && i < mock_can.tx_count) ? &mock_can.tx[i] : NULL;
}

void CanPort_Init(void) { mock_can.init_calls++; }

void CanPort_Transmit(CAN_TX_PRIOIRTY priority, CAN_MSG_OBJ *msg)
{
    sMockCanMsg *m;
    if (mock_can.tx_count >= MOCK_CAN_TX_MAX)
        return;
    m = &mock_can.tx[mock_can.tx_count++];
    memset(m, 0, sizeof(*m));
    m->id = msg->msgId;
    m->dlc = msg->field.dlc;
    m->idType = msg->field.idType;
    m->frameType = msg->field.frameType;
    m->priority = (uint8_t)priority;
    memcpy(m->data, msg->data, m->dlc);
}

bool CanPort_Receive(CAN_MSG_OBJ *msg)
{
    const sMockCanMsg *m;
    if (mock_can.rx_head >= mock_can.rx_count)
        return false;
    m = &mock_can.rx[mock_can.rx_head++];
    msg->msgId = m->id;
    msg->field.dlc = m->dlc & 0x0F;
    msg->field.idType = CAN_FRAME_EXT;
    msg->field.frameType = CAN_FRAME_DATA;
    /* Comme le driver MCC, les 8 octets du buffer matériel sont recopiés (0 au-delà du dlc). */
    memcpy(msg->data, m->data, 8);
    return true;
}

uint8_t CanPort_ReceivedCount(void) { return (uint8_t)(mock_can.rx_count - mock_can.rx_head); }

/* ---- uart_port ------------------------------------------------------------------- */
sMockUart mock_uart;

void MockUart_Reset(void) { memset(&mock_uart, 0, sizeof(mock_uart)); }

void MockUart_PushRx(const uint8_t *bytes, int n)
{
    int i;
    for (i = 0; i < n && mock_uart.rx_count < MOCK_UART_RX_MAX; i++)
        mock_uart.rx[mock_uart.rx_count++] = bytes[i];
}

void UartPort_WriteBytes(const uint8_t *bytes, uint8_t n)
{
    uint8_t i;
    for (i = 0; i < n && mock_uart.tx_count < MOCK_UART_TX_MAX; i++)
        mock_uart.tx[mock_uart.tx_count++] = bytes[i];
}

bool UartPort_IsRxReady(void) { return mock_uart.rx_head < mock_uart.rx_count; }

uint8_t UartPort_ReadByte(void)
{
    if (mock_uart.rx_head < mock_uart.rx_count)
        return mock_uart.rx[mock_uart.rx_head++];
    return 0;
}

/* ---- adc_port -------------------------------------------------------------------- */
sMockAdc mock_adc;

void MockAdc_Reset(void) { memset(&mock_adc, 0, sizeof(mock_adc)); }

void AdcPort_Init(void)
{
    mock_adc.init_calls++;
    mock_adc.selected_channel = ADC_PORT_CH_INITIAL;
}

void AdcPort_SelectChannel(uint8_t ch)
{
    mock_adc.selected_channel = ch;
    mock_adc.select_calls++;
}

bool AdcPort_IsConversionComplete(uint8_t ch)
{
    (void)ch;
    return mock_adc.conversion_complete;
}

uint16_t AdcPort_Result(uint8_t ch) { return mock_adc.result[ch <= ADC_PORT_CH_NB ? ch : ADC_PORT_CH_NB]; }
