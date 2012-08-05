#if !defined(_WATCHDOG_H_)
#define _WATCHDOG_H_ 1

#include <stdint.h>
#include <avr/io.h>

void watchdog_setup(uint8_t scaler);
void watchdog_start(void);
void watchdow_stop(void);

#endif
