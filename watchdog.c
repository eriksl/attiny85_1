#include <stdint.h>
#include <avr/io.h>

#include "watchdog.h"

void watchdog_setup(uint8_t scaler)
{
	uint8_t wdp[4];

	if(scaler > 9)
		return;

	wdp[0] = (scaler & (1 << 0)) >> 0;
	wdp[1] = (scaler & (1 << 1)) >> 1;
	wdp[2] = (scaler & (1 << 2)) >> 2;
	wdp[3] = (scaler & (1 << 3)) >> 3;

	WDTCR =
		(1		<< WDIF)	|
		(1		<< WDIE)	|
		(1		<< WDCE)	|
		(1		<< WDE)		|
		(wdp[3] << WDP3)	|
		(wdp[2] << WDP2)	|
		(wdp[1] << WDP1)	|
		(wdp[0] << WDP0);
}

void watchdog_start(void)
{
	WDTCR |= _BV(WDIE) | _BV(WDCE) | _BV(WDE);
}

void watchdow_stop(void)
{
	WDTCR |= _BV(WDCE) | _BV(WDE);
	WDTCR |= _BV(WDCE);
}
