#if !defined(_TIMER0_H_)
#define _TIMER0_H_

#include <stdint.h>
#include <avr/io.h>

enum
{
	TIMER0_PRESCALER_OFF	= 0,
	TIMER0_PRESCALER_1		= 1,
	TIMER0_PRESCALER_8		= 2,
	TIMER0_PRESCALER_64	= 3,
	TIMER0_PRESCALER_256	= 4,
	TIMER0_PRESCALER_1024	= 5
};

void	timer0_init(uint8_t scaler, uint16_t counter);
void	timer0_reset(void);
void	timer0_start(void);
void	timer0_stop(void);
uint8_t	timer0_status(void);

#endif
