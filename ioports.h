#if !defined(_IOPORTS_H_)
#define _IOPORTS_H_ 1
#include <inttypes.h>
#include <avr/io.h>

enum
{ 
	IO_PORTS  = 4,
	PWM_PORTS = 4
};

static const struct
{
	volatile uint8_t	*port;
	uint8_t				bit;
} io_ports[IO_PORTS]
=
{
	{ &PORTB, 3 },
	{ &PORTB, 4 },
	{ &PORTA, 3 },
	{ &PORTA, 4 }
};

static const struct
{
	volatile uint8_t	*port;
	uint8_t				bit;
} pwm_ports[PWM_PORTS]
=
{
	{ &PORTB, 3 },
	{ &PORTB, 4 },
	{ &PORTA, 3 },
	{ &PORTA, 4 }
};

#endif
