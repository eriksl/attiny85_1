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
	{ &PINB, &DDRB, 6 },
	{ &PINA, &DDRA, 6 }
};

const ioport_t output_ports[OUTPUT_PORTS] =
{
	{ &PORTB, &DDRB, 2 },
	{ &PORTB, &DDRB, 4 },
	{ &PORTA, &DDRA, 3 },
	{ &PORTA, &DDRA, 4 }
};

const ioport_t counter_ports[COUNTER_PORTS] =
{
	{ &PINB, &DDRB, 6 },
	{ &PINA, &DDRA, 6 }
};
