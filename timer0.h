#if !defined(_TIMER0_H_)
#define _TIMER0_H_

#include <stdint.h>
#include <avr/io.h>

void timer0_reset(void);
void timer0_init(uint8_t scaler, uint16_t counter);

#endif
