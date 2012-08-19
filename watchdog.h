#if !defined(_WATCHDOG_H_)
#define _WATCHDOG_H_ 1

#include <stdint.h>
#include <avr/io.h>

enum
{
	WATCHDOG_PRESCALER_2K		=	0,
	WATCHDOG_PRESCALER_4K		=	1,
	WATCHDOG_PRESCALER_8K		=	2,
	WATCHDOG_PRESCALER_16K		=	3,
	WATCHDOG_PRESCALER_32K		=	4,
	WATCHDOG_PRESCALER_64K		=	5,
	WATCHDOG_PRESCALER_128K		=	6,
	WATCHDOG_PRESCALER_256K		=	7,
	WATCHDOG_PRESCALER_512K		=	8,
	WATCHDOG_PRESCALER_1024K	=	9
};

void watchdog_setup(uint8_t scaler);
void watchdog_start(void);
void watchdow_stop(void);

#endif
