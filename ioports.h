#if !defined(_IOPORTS_H_)
#define _IOPORTS_H_ 1
#include <stdint.h>
#include <avr/io.h>

typedef struct
{
	uint8_t	refs[3];
	uint8_t	mux[6];
} adcport_t;

typedef struct
{
	volatile	uint8_t	*port;
				uint8_t	bit;
} ioport_t;

typedef struct
{
	volatile	uint8_t		*port;
				uint8_t		bit;
				uint32_t	counter;
} counterport_t;

enum
{
	ADC_PORTS		= 2,
	INPUT_PORTS		= 2,
	OUTPUT_PORTS	= 4,
	PWM_PORTS		= 4,
	COUNTER_PORTS	= 2
};

extern const	adcport_t		adc_ports[];
extern const	ioport_t		input_ports[];
extern const	ioport_t		output_ports[];
extern const	ioport_t		pwm_ports[];
extern			counterport_t	counter_ports[];

#endif
