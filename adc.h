#if !defined(_ADC_H_)
#define _ADC_H_

#include <stdint.h>
#include <avr/io.h>

void adc_init(void);
void adc_start(uint8_t source);	// source: 0 = adc6 = pa7, 1 = internal temperature sensor
void adc_stop(void);

#endif
