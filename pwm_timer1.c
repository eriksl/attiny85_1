#include <stdint.h>
#include <avr/io.h>

#include "ioports.h"
#include "pwm_timer1.h"

static uint8_t cs1[4];

void pwm_timer1_init(uint8_t prescaler)
{
	uint8_t temp, mask;

	if(prescaler > 15)
		return;

	cs1[0] = (prescaler & (1 << 0)) >> 0;
	cs1[1] = (prescaler & (1 << 1)) >> 1;
	cs1[2] = (prescaler & (1 << 2)) >> 2;
	cs1[3] = (prescaler & (1 << 3)) >> 3;

	pwm_timer1_stop();

	TCCR1A =	(0 << COM1A1)	|	// compare 1a output mode
				(0 << COM1A0)	|	// compare 1a output mode
				(0 << COM1B1)	|	// compare 1b output mode
				(0 << COM1B0)	|	// compare 1b output mode
				(0 << FOC1A)	|	// force compare 1a output
				(0 << FOC1B)	|	// force compare 1b output
				(0 << PWM1A)	|	// enable pwm compare 1a
				(0 << PWM1B);		// enable pwm compare 1b

	TCCR1B =	(0 << PWM1X)	|	// enable pwm inversion mode
				(0 << PSR1)		|	// reset prescaler
				(0 << DTPS11)	|	// dead time prescaler
				(0 << DTPS10)	|	// dead time prescaler
				(0 << CS13)		|	// clock select bit 3
				(0 << CS12)		|	// clock select bit 2
				(0 << CS11)		|	// clock select bit 1
				(0 << CS10);		// clock select bit 0

	TCCR1C =	(0 << COM1A1S)	|	// shadow compare 1a output mode
				(0 << COM1A0S)	|	// shadow compare 1a output mode
				(0 << COM1B1S)	|	// shadow compare 1b output mode
				(0 << COM1B0S)	|	// shadow compare 1b output mode
				(0 << COM1D1)	|	// compare 1d output mode
				(0 << COM1D0)	|	// compare 1d output mode
				(0 << FOC1D)	|	// force compare d output
				(0 << PWM1D);		// enable pwm compare 1d

	TCCR1D =	(0 << FPIE1)	|	// enable fault protection interrupt
				(0 << FPEN1)	|	// enable fault protection mode
				(0 << FPNC1)	|	// enable fault protection noise canceler
				(0 << FPES1)	|	// fault protection edge select
				(0 << FPAC1)	|	// enable fault protection analog comparator
				(0 << FPF1)		|	// fault protection interrupt flag
				(0 << WGM11)	|	// wave mode, 00 = normal / fast pwm, 01 = correct pwm, 10/11 = pwm6
				(1 << WGM10);		//		top = ocr1c, overflow at bottom

	TCCR1E	=	0;					// output compare pwm6 override bits

	PLLCSR =	(0 << LSM)		|	// enable low speed mode (pll / 2)
				(0 << 6)		|	// reserved
				(0 << 5)		|	// reserved
				(0 << 4)		|	// reserved
				(0 << 3)		|	// reserved
				(0 << PCKE)		|	// high speed mode (32/64 Mhz)
				(0 << PLLE)		|	// pll enable
				(0 << PLOCK);		// pll lock detector

	mask =		(1 << OCIE1D)	|	// enable interrupt output compare 1d
				(1 << OCIE1A)	|	// enable interrupt output compare 1a
				(1 << OCIE1B)	|	// enable interrupt output compare 1b
				(0 << OCIE0A)	|	// enable interrupt output compare 0a
				(0 << OCIE0B)	|	// enable interrupt output compare 0b
				(1 << TOIE1)	|	// enable interrupt output overflow timer 1
				(0 << TOIE0)	|	// enable interrupt output overflow timer 0
				(0 << TICIE0);		// enable interrupt input capture timer0

	temp = TIMSK & ~mask;

	temp |=		(0 << OCIE1D)	|	// enable interrupt output compare 1d
				(0 << OCIE1A)	|	// enable interrupt output compare 1a
				(0 << OCIE1B)	|	// enable interrupt output compare 1b
				(0 << OCIE0A)	|	// enable interrupt output compare 0a
				(0 << OCIE0B)	|	// enable interrupt output compare 0b
				(0 << TOIE1)	|	// enable interrupt output overflow timer 1
				(0 << TOIE0)	|	// enable interrupt output overflow timer 0
				(0 << TICIE0);		// enable interrupt input capture timer0

	TIMSK = temp;

	TIFR =		(1 << OCF1D)	|	// output compare flag 1d
				(1 << OCF1A)	|	// output compare flag 1a
				(1 << OCF1B)	|	// output compare flag 1b
				(0 << OCF0A)	|	// output compare flag 0a
				(0 << OCF0B)	|	// output compare flag 0b
				(1 << TOV1)		|	// overflow flag timer1
				(0 << TOV0)		|	// overflow flag timer0
				(0 << ICF0);		// input capture flag timer0
}

uint16_t pwm_timer1_get_counter(void)
{
	uint16_t rv;

	rv = TCNT1;
	rv |= TC1H << 8;

	return(rv);
}

void pwm_timer1_set_max(uint16_t max_value)
{
	TC1H	= (max_value & 0xff00) >> 8;
	OCR1C	= (max_value & 0x00ff) >> 0;
}

uint16_t pwm_timer1_get_max(void)
{
	uint16_t rv;

	rv = OCR1C;
	rv |= TC1H << 8;

	return(rv);
}

void pwm_timer1_set_pwm(uint8_t port, uint16_t pwm_value)
{
	if(port >= PWM_PORTS)
		return;

	const pwmport_t *slot = &pwm_ports[port];
	
	if((pwm_value == 0) || (pwm_value >= 0x3ff))
	{
		*slot->compare_reg_high	= 0;
		*slot->compare_reg_low	= 0;
		*slot->control_reg		&= ~(_BV(slot->com0_bit) | _BV(slot->com1_bit) | _BV(slot->pwm_bit));	// disconnnected

		if(pwm_value == 0)
			*slot->port &= ~_BV(slot->bit);
		else
			*slot->port |= _BV(slot->bit);
	}
	else
	{
		uint8_t control_reg_temp;

		*slot->compare_reg_high	= pwm_value >> 8;
		*slot->compare_reg_low	= pwm_value & 0xff;

		control_reg_temp	= *slot->control_reg;
		control_reg_temp	&= ~_BV(slot->com0_bit);	//	clear on match when counting up, set on match when counting down
		control_reg_temp	|= _BV(slot->com1_bit) | _BV(slot->pwm_bit) | _BV(slot->foc_bit);	//	force immediate effect
		*slot->control_reg	= control_reg_temp;
	}
}

uint16_t pwm_timer1_get_pwm(uint8_t port)
{
	uint16_t rv;

	if(port >= PWM_PORTS)
		return(0);

	const pwmport_t *slot = &pwm_ports[port];

	rv = *slot->compare_reg_low;
	rv |= *slot->compare_reg_high << 8;

	return(rv);
}

void pwm_timer1_start(void)
{
	pwm_timer1_stop();
	pwm_timer1_reset_counter();

	TCCR1B =	(0		<< PWM1X)	|	// enable pwm inversion mode
				(1		<< PSR1)	|	// reset prescaler
				(0		<< DTPS11)	|	// dead time prescaler
				(0		<< DTPS10)	|	// dead time prescaler
				(cs1[3]	<< CS13)	|	// clock select bit 3
				(cs1[2]	<< CS12)	|	// clock select bit 2
				(cs1[1]	<< CS11)	|	// clock select bit 1
				(cs1[0]	<< CS10);		// clock select bit 0
}

void pwm_timer1_stop(void)
{
	TCCR1B =	(0		<< PWM1X)	|	// enable pwm inversion mode
				(1		<< PSR1)	|	// reset prescaler
				(0		<< DTPS11)	|	// dead time prescaler
				(0		<< DTPS10)	|	// dead time prescaler
				(0		<< CS13)	|	// clock select bit 3
				(0		<< CS12)	|	// clock select bit 2
				(0		<< CS11)	|	// clock select bit 1
				(0		<< CS10);		// clock select bit 0
}

uint8_t pwm_timer1_status(void)
{
	return((TCCR1B & (_BV(CS13) | _BV(CS12) | _BV(CS11) | _BV(CS10))) >> CS10);
}

