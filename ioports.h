#if !defined(_IOPORTS_H_)
#define _IOPORTS_H_ 1
#include <stdint.h>
#include <avr/io.h>

typedef struct
{
	volatile	uint8_t	*port;
				uint8_t	bit;
} port_t;

enum
{
	INPUT_PORTS		= 2,
	OUTPUT_PORTS	= 4,
	PWM_PORTS		= 4
};

static const port_t input_ports[INPUT_PORTS] =
{
	{ &PINB, 6 },
	{ &PINA, 6 }
};

static const port_t output_ports[OUTPUT_PORTS] =
{
	{ &PORTB, 3 },
	{ &PORTB, 4 },
	{ &PORTA, 3 },
	{ &PORTA, 4 }
};

static const port_t pwm_ports[PWM_PORTS] = 
{
	{ &PORTB, 3 },
	{ &PORTB, 4 },
	{ &PORTA, 3 },
	{ &PORTA, 4 }
};

#endif
