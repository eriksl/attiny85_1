#include <stdint.h>
#include <avr/io.h>

void timer0_reset(void)
{
	TCNT0H =	0;
	TCNT0L =	0;
}

void timer0_init(uint8_t scaler, uint16_t counter)
{
	uint8_t	cs0[3];

	if(scaler > 5)
		return;

	PRR &= ~_BV(PRTIM0);

	// clock source + prescaler
	//		001 = 1, 010 = 8, 011 = 64
	//		100 = 256, 101 = 1024

	cs0[0] = (scaler & (1 << 0)) >> 0;
	cs0[1] = (scaler & (1 << 1)) >> 1;
	cs0[2] = (scaler & (1 << 2)) >> 2;

	TCCR0A =	(1 << TCW0)		|	// enable 16 bit mode
				(0 << ICEN0)	|	// enable capture mode
				(0 << ICNC0)	|	// enable capture mode noise canceller
				(0 << ICES0)	|	// enable capture mode edge select
				(0 << ACIC0)	|	// enable capture mode by analog compare
				(0 << 2)		|
				(0 << 1)		|	// reserved
				(0 << WGM00);		// waveform generation mode => normal (n/a)

	OCR0B =		(counter & 0xff00) >> 8;	// output compare 0 a high byte
	OCR0A =		(counter & 0x00ff) >> 0;	// output compare 0 a low byte

	TIMSK =		(0 << OCIE1D)	|	// enable output compare match 1 d interrupt
				(0 << OCIE1A)	|	// enable output compare match 1 a interrupt
				(0 << OCIE1B)	|	// enable output compare match 1 b interrupt
				(1 << OCIE0A)	|	// enable output compare match 0 a interrupt
				(0 << OCIE0B)	|	// enable output compare match 0 b interrupt
				(0 << TOIE1)	|	// enable timer 1 overflow interrupt
				(0 << TOIE0)	|	// enable timer 0 overflow interrupt
				(0 << TICIE0);		// enable timer 0 capture interrupt

	TIFR =		(0 << OCF1D)	|	// output compare flag 1 d
				(0 << OCF1A)	|	// output compare flag 1 a
				(0 << OCF1B)	|	// output compare flag 1 b
				(1 << OCF0A)	|	// output compare flag 0 a
				(0 << OCF0B)	|	// output compare flag 0 b
				(0 << TOV1)		|	// timer 1 overflow flag
				(0 << TOV0)		|	// timer 0 overflow flag
				(0 << ICF0);		// timer 0 input capture flag

	TCCR0B =	(0		<< 7)		|
				(0		<< 6)		|
				(0		<< 5)		|	// reserved
				(0		<< TSM)		|	// enable synchronisation mode
				(0		<< PSR0)	|	// reset prescaler
				(cs0[2] << CS02)	|	// clock source + prescaler
				(cs0[1] << CS01)	|	
				(cs0[0] << CS00);

	timer0_reset();
}
