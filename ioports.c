#include <stdint.h>
#include <avr/io.h>

#include "ioports.h"

const ioport_t input_ports[INPUT_PORTS] =
{
	{ &PINB, &DDRB, 3 },
};

const ioport_t output_ports[OUTPUT_PORTS] =
{
	{ &PORTB, &DDRB, 4 },
	{ &PORTB, &DDRB, 1 },
};
