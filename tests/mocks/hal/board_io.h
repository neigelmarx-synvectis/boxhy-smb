/**
 * @file tests/mocks/hal/board_io.h
 * @brief Double de test de hal/board_io.h : mémorise l'état des sorties, lit des entrées injectées.
 */
#ifndef HAL_BOARD_IO_H
#define HAL_BOARD_IO_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool dcac_input_relay;
    bool dcac_precharge_relay;
    bool supply_precharge_relay;
    bool fcs_output_relay;
    bool buzzer;
    bool led_left, led_middle, led_right;
    bool btn_left, btn_middle, btn_right, btn_battery_mode, btn_hybrid_mode;
    uint8_t fans_duty_cycle;
    int fans_set_calls;
    int init_calls;
} sMockBoard;

extern sMockBoard mock_board;

void MockBoard_Reset(void);
void Board_Init(void);
void Board_SetFansDutyCycle(uint8_t duty_cycle);

static inline void Board_SetDcacInputRelay(bool on)      { mock_board.dcac_input_relay = on; }
static inline void Board_ToggleDcacInputRelay(void)      { mock_board.dcac_input_relay = !mock_board.dcac_input_relay; }
static inline bool Board_GetDcacInputRelay(void)         { return mock_board.dcac_input_relay; }

static inline void Board_SetDcacPrechargeRelay(bool on)  { mock_board.dcac_precharge_relay = on; }
static inline void Board_ToggleDcacPrechargeRelay(void)  { mock_board.dcac_precharge_relay = !mock_board.dcac_precharge_relay; }
static inline bool Board_GetDcacPrechargeRelay(void)     { return mock_board.dcac_precharge_relay; }

static inline void Board_SetSupplyPrechargeRelay(bool on){ mock_board.supply_precharge_relay = on; }
static inline void Board_ToggleSupplyPrechargeRelay(void){ mock_board.supply_precharge_relay = !mock_board.supply_precharge_relay; }
static inline bool Board_GetSupplyPrechargeRelay(void)   { return mock_board.supply_precharge_relay; }

static inline void Board_SetFcsOutputRelay(bool on)      { mock_board.fcs_output_relay = on; }
static inline void Board_ToggleFcsOutputRelay(void)      { mock_board.fcs_output_relay = !mock_board.fcs_output_relay; }
static inline bool Board_GetFcsOutputRelay(void)         { return mock_board.fcs_output_relay; }

static inline void Board_SetBuzzer(bool on)              { mock_board.buzzer = on; }
static inline void Board_ToggleBuzzer(void)              { mock_board.buzzer = !mock_board.buzzer; }
static inline bool Board_GetBuzzer(void)                 { return mock_board.buzzer; }

static inline void Board_SetLedLeft(bool on)             { mock_board.led_left = on; }
static inline void Board_ToggleLedLeft(void)             { mock_board.led_left = !mock_board.led_left; }
static inline bool Board_GetLedLeft(void)                { return mock_board.led_left; }

static inline void Board_SetLedMiddle(bool on)           { mock_board.led_middle = on; }
static inline void Board_ToggleLedMiddle(void)           { mock_board.led_middle = !mock_board.led_middle; }
static inline bool Board_GetLedMiddle(void)              { return mock_board.led_middle; }

static inline void Board_SetLedRight(bool on)            { mock_board.led_right = on; }
static inline void Board_ToggleLedRight(void)            { mock_board.led_right = !mock_board.led_right; }
static inline bool Board_GetLedRight(void)               { return mock_board.led_right; }

static inline bool Board_GetButtonLeft(void)             { return mock_board.btn_left; }
static inline bool Board_GetButtonMiddle(void)           { return mock_board.btn_middle; }
static inline bool Board_GetButtonRight(void)            { return mock_board.btn_right; }
static inline bool Board_GetButtonBatteryMode(void)      { return mock_board.btn_battery_mode; }
static inline bool Board_GetButtonHybridMode(void)       { return mock_board.btn_hybrid_mode; }

static inline void Board_RelaysSafeState(void)
{
    Board_SetSupplyPrechargeRelay(true);
    Board_SetDcacPrechargeRelay(false);
    Board_SetDcacInputRelay(false);
    Board_SetFcsOutputRelay(false);
}

#endif /* HAL_BOARD_IO_H */
