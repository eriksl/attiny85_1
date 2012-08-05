#include <stdint.h>
#include <avr/io.h>

#include "timer0.h"

static uint8_t	cs0[3]	= { 0, 0, 0 };
static uint8_t	ocr0a	= 0xff;
static uint8_t	ocr0b	= 0xff;

void timer0_init(uint8_t scaler, uint16_t counter)
{
	if(scaler > 5)
		return;

	cs0[0] = (scaler & (1 << 0)) >> 0;
	cs0[1] = (scaler & (1 << 1)) >> 1;
	cs0[2] = (scaler & (1 << 2)) >> 2;

	ocr0a = (counter & 0x00ff) >> 0;	// output compare 0 a high byte
	ocr0b = (counter & 0xff00) >> 8;	// output compare 0 a low byte

	PRR &= ~_BV(PRTIM0);

	timer0_stop();

	TCCR0A =	(1 << TCW0)		|	// enable 16 bit mode
				(0 << ICEN0)	|	// enable capture mode
				(0 << ICNC0)	|	// enable capture mode noise canceller
				(0 << ICES0)	|	// enable capture mode edge select
				(0 << ACIC0)	|	// enable capture mode by analog compare
				(0 << 2)		|	// reserved
				(0 << 1)		|	// reserved
				(0 << WGM00);		// waveform generation mode => normal (n/a)

	TIMSK =		(0 << OCIE1D)	|	// enable output compare match 1 d interrupt
				(0 << OCIE1A)	|	// enable output compare match 1 a interrupt
				(0 << OCIE1B)	|	// enable output compare match 1 b interrupt
				(1 << OCIE0A)	|	// enable output compare match 0 a interrupt
				(0 << OCIE0B)	|	// enable output compare match 0 b interrupt
				(0 << TOIE1)	|	// enable timer 1 overflow interrupt
				(0 << TOIE0)	|	// enable timer 0 overflow interrupt
				(0 << TICIE0);		// enable timer 0 capture interrupt

	TIFR =		(0 << OCF1D)	|	// clear output compare flag 1 d
				(0 << OCF1A)	|	// clear output compare flag 1 a
				(0 << OCF1B)	|	// clear output compare flag 1 b
				(1 << OCF0A)	|	// clear output compare flag 0 a
				(0 << OCF0B)	|	// clear output compare flag 0 b
				(0 << TOV1)		|	// clear timer 1 overflow flag
				(1 << TOV0)		|	// clear timer 0 overflow flag
				(0 << ICF0);		// clear timer 0 input capture flag

}

void timer0_reset(void)
{
	OCR0B =	ocr0b;
	OCR0A =	ocr0a;

	TCNT0H =	0;
	TCNT0L =	0;
}

void timer0_start(void)
{
	timer0_stop();

	timer0_reset();

	TCCR0B =	(0 << 7)			|	// reserved
				(0 << 6)			|	// reserved
				(0 << 5)			|	// reserved
				(0 << TSM)			|	// timer synchronisation mode
				(0 << PSR0)			|	// prescaler reset
				(cs0[2] << CS02)	|	// scaler
				(cs0[1] << CS01)	|	// 	start timer
				(cs0[0] << CS00);		//
}

void timer0_stop(void)
{
	TCCR0B =	(0 << 7)		|	// reserved
				(0 << 6)		|	// reserved
				(0 << 5)		|	// reserved
				(0 << TSM)		|	// timer synchronisation mode
				(0 << PSR0)		|	// prescaler reset
				(0 << CS02)		|	// scaler
				(0 << CS01)		|	// 	000 = stop timer
				(0 << CS00);		//
}

uint8_t timer0_status(void)
{
	return((TCCR0B & (_BV(CS02) | _BV(CS01) | _BV(CS00))) >> CS00);
}
