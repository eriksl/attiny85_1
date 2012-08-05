#include <stdint.h>
#include <avr/io.h>

#include "ioports.h"

const adcport_t adc_ports[ADC_PORTS] = 
{
	{							// pa7
		{ 0, 0, 0 },			// Vref = Vcc
		{ 0, 1, 1, 0, 0, 0 },	// mux = adc6
	},
	{							// internal temp sensor
		{ 0, 1, 0, },			// Vref = internal 1.1V reference
		{ 1, 1, 1, 1, 1, 1 }	// mux = adc11
	}
};

const ioport_t input_ports[INPUT_PORTS] =
{
	{ &PINB, 6 },
	{ &PINA, 6 }
};

const ioport_t output_ports[OUTPUT_PORTS] =
{
	{ &PORTB, 3 },
	{ &PORTB, 4 },
	{ &PORTA, 3 },
	{ &PORTA, 4 }
};

const ioport_t pwm_ports[PWM_PORTS] = 
{
	{ &PORTB, 3 },
	{ &PORTB, 4 },
	{ &PORTA, 3 },
	{ &PORTA, 4 }
};

counterport_t counter_ports[COUNTER_PORTS] =
{
	{ &PINB, 6, 0},
	{ &PINA, 6, 0 }
};
