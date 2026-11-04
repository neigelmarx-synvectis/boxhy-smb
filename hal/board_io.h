/**
 * @file hal/board_io.h
 * @brief Accès aux entrées/sorties de la carte (relais, buzzer, LEDs, boutons, PWM ventilateurs).
 *
 * Implémentation cible : fonctions inline sur les macros du pin manager MCC (coût nul).
 * Une implémentation de substitution existe dans tests/mocks/hal/ pour les tests sur PC.
 */
#ifndef HAL_BOARD_IO_H
#define HAL_BOARD_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/pin_manager.h"

/** Initialise le module PWM (ventilateurs) et coupe la référence de tension (DAC). */
void Board_Init(void);
/** Rapport cyclique ventilateurs AC en % (saturé à 100). */
void Board_SetFansDutyCycle(uint8_t duty_cycle);

/* ---- Relais ------------------------------------------------------------------- */
static inline void Board_SetDcacInputRelay(bool on)      { if (on) DCAC_INPUT_RELAY_SetHigh(); else DCAC_INPUT_RELAY_SetLow(); }
static inline void Board_ToggleDcacInputRelay(void)      { DCAC_INPUT_RELAY_Toggle(); }
static inline bool Board_GetDcacInputRelay(void)         { return DCAC_INPUT_RELAY_GetValue(); }

static inline void Board_SetDcacPrechargeRelay(bool on)  { if (on) DCAC_PRECHARGE_RELAY_SetHigh(); else DCAC_PRECHARGE_RELAY_SetLow(); }
static inline void Board_ToggleDcacPrechargeRelay(void)  { DCAC_PRECHARGE_RELAY_Toggle(); }
static inline bool Board_GetDcacPrechargeRelay(void)     { return DCAC_PRECHARGE_RELAY_GetValue(); }

static inline void Board_SetSupplyPrechargeRelay(bool on){ if (on) SUPPLY_PRECHARGE_RELAY_SetHigh(); else SUPPLY_PRECHARGE_RELAY_SetLow(); }
static inline void Board_ToggleSupplyPrechargeRelay(void){ SUPPLY_PRECHARGE_RELAY_Toggle(); }
static inline bool Board_GetSupplyPrechargeRelay(void)   { return SUPPLY_PRECHARGE_RELAY_GetValue(); }

static inline void Board_SetFcsOutputRelay(bool on)      { if (on) FCS_OUTPUT_RELAY_SetHigh(); else FCS_OUTPUT_RELAY_SetLow(); }
static inline void Board_ToggleFcsOutputRelay(void)      { FCS_OUTPUT_RELAY_Toggle(); }
static inline bool Board_GetFcsOutputRelay(void)         { return FCS_OUTPUT_RELAY_GetValue(); }

/* ---- Buzzer et LEDs ------------------------------------------------------------ */
static inline void Board_SetBuzzer(bool on)              { if (on) BUZZER_SetHigh(); else BUZZER_SetLow(); }
static inline void Board_ToggleBuzzer(void)              { BUZZER_Toggle(); }
static inline bool Board_GetBuzzer(void)                 { return BUZZER_GetValue(); }

static inline void Board_SetLedLeft(bool on)             { if (on) IHM_LEFT_BUTTON_LED_SetHigh(); else IHM_LEFT_BUTTON_LED_SetLow(); }
static inline void Board_ToggleLedLeft(void)             { IHM_LEFT_BUTTON_LED_Toggle(); }
static inline bool Board_GetLedLeft(void)                { return IHM_LEFT_BUTTON_LED_GetValue(); }

static inline void Board_SetLedMiddle(bool on)           { if (on) IHM_MIDDLE_BUTTON_LED_SetHigh(); else IHM_MIDDLE_BUTTON_LED_SetLow(); }
static inline void Board_ToggleLedMiddle(void)           { IHM_MIDDLE_BUTTON_LED_Toggle(); }
static inline bool Board_GetLedMiddle(void)              { return IHM_MIDDLE_BUTTON_LED_GetValue(); }

static inline void Board_SetLedRight(bool on)            { if (on) IHM_RIGHT_BUTTON_LED_SetHigh(); else IHM_RIGHT_BUTTON_LED_SetLow(); }
static inline void Board_ToggleLedRight(void)            { IHM_RIGHT_BUTTON_LED_Toggle(); }
static inline bool Board_GetLedRight(void)               { return IHM_RIGHT_BUTTON_LED_GetValue(); }

/* ---- Boutons (niveau brut) ------------------------------------------------------- */
static inline bool Board_GetButtonLeft(void)             { return IHM_LEFT_BUTTON_GetValue(); }
static inline bool Board_GetButtonMiddle(void)           { return IHM_MIDDLE_BUTTON_GetValue(); }
static inline bool Board_GetButtonRight(void)            { return IHM_RIGHT_BUTTON_GetValue(); }
static inline bool Board_GetButtonBatteryMode(void)      { return BATTERY_MODE_BUTTON_GetValue(); }
static inline bool Board_GetButtonHybridMode(void)       { return HYBRID_MODE_BUTTON_GetValue(); }

/** État de repli : alim 24 V précharge maintenue, tous les contacteurs de puissance ouverts. */
static inline void Board_RelaysSafeState(void)
{
    Board_SetSupplyPrechargeRelay(true);
    Board_SetDcacPrechargeRelay(false);
    Board_SetDcacInputRelay(false);
    Board_SetFcsOutputRelay(false);
}

#endif /* HAL_BOARD_IO_H */
