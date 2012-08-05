#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <usitwislave.h>

#include "ioports.h"

static volatile uint32_t porta_count = 0, portb_count = 0;

static volatile uint8_t pwm_value[PWM_PORTS] = { 128, 128, 128, 128 };

//static void setup_wdt(void)
//{
//	WDTCR =
//		(1 << WDIF)	|
//		(1 << WDIE)	|
//		(1 << WDCE)	|
//		(1 << WDE)	|
//		(0 << WDP3)	|
//		(0 << WDP2)	|
//		(1 << WDP1)	|
//		(1 << WDP0);
//}

//ISR(WDT_vect)
//{
//	setup_wdt();
//
//	PORTA &= ~0x18;
//	PORTB &= ~0x18;
//}

static void reset_adc(void)
{
	ACSRA =		(1 << ACD)		|	// disable comperator
				(0 << ACBG)		|	// bandgap select (n/a)
				(0 << ACO)		|	// enable analog comperator output
				(1 << ACI)		|	// clear comperator interrupt flag
				(0 << ACIE)		|	// enable analog comperator interrupt
				(0 << ACME)		|	// use adc multiplexer
				(0 << ACIS1)	|	// interrupt mode select (n/a)
				(0 << ACIS0);

	ACSRB =		(0 << HSEL)		|	// hysteresis select (n/a)
				(0 << HLEV)		|	// hysteresis level (n/a)
				(0 << 5)		|
				(0 << 4)		|
				(0 << 3)		|
				(0 << ACM0)		|	// analog comperator multiplexer (n/a)
				(0 << ACM1)		|	// analog comperator multiplexer (n/a)
				(0 << ACM2);		// analog comperator multiplexer (n/a)

	ADCSRA =	(0 << ADEN)		|	// enable ADC
				(0 << ADSC)		|	// start conversion
				(0 << ADATE)	|	// auto trigger enable
				(1 << ADIF)		|	// clear interrupt flag
				(0 << ADIE)		|	// enable interrupt
				(1 << ADPS2)	|
				(1 << ADPS1)	|
				(0 << ADPS0);		// select clock scaler 110 = 64

	ADMUX	=	(0 << REFS1)	|	// select Vcc as Vref
				(0 << REFS0)	|
				(0 << ADLAR)	|	// right adjust result
				(0 << MUX4)		|
				(0 << MUX3)		|
				(1 << MUX2)		|	// select 000110 = adc6 (pa7)
				(1 << MUX1)		|
				(0 << MUX0);

	ADCSRB	=	(0 << BIN)		|	// unipolair input
				(0 << GSEL)		|	// gain select (n/a)
				(0 << 5)		|	// reserved
				(0 << REFS2)	|	// select Vcc as Vref (n/a)
				(0 << MUX5)		|	// select adc6 (pa7)
				(0 << ADTS2)	|
				(0 << ADTS1)	|	// auto trigger source (n/a)
				(0 << ADTS0);

	DIDR0 = 	(1 << ADC6D)	|	// disable digital input adc6
				(0 << ADC5D)	|
				(0 << ADC4D)	|
				(0 << ADC3D)	|
				(0 << AREFD)	|
				(0 << ADC2D)	|
				(0 << ADC1D)	|
				(0 << ADC0D);

	DIDR1 =		(0 << ADC10D)	|
				(0 << ADC9D)	|
				(0 << ADC8D)	|
				(0 << ADC7D)	|
				(0 << 3)		|	// reserved
				(0 << 2)		|
				(0 << 1)		|
				(0 << 0);
};

static void reset_timer0(void)
{
	TCNT0H =	0;
	TCNT0L =	0;
}

ISR(PCINT_vect)
{
	if(!(PINA & _BV(PA6)) && !(PINB & _BV(PB6)))
	{
		porta_count = 0;
		portb_count = 0;

		return;
	}

	uint8_t pv = pwm_value[0];

	if(!(PINA & _BV(PA6)))
	{
		if(pv < 248)
			pv += 8;
		else
			pv = 255;

		porta_count++;
	}

	if(!(PINB & _BV(PB6)))
	{
		if(pv > 8)
			pv -= 8;
		else
			pv = 0;

		portb_count++;
	}

	pwm_value[0] = pv;
}

ISR(TIMER0_COMPA_vect)
{
	static	uint8_t current;
			uint8_t ix;

	if(current == 0)
		for(ix = 0; ix < PWM_PORTS; ix++)
			*pwm_ports[ix].port |= (1 << pwm_ports[ix].bit);

	for(ix = 0; ix < PWM_PORTS; ix++)
		if(current == pwm_value[ix])
			*pwm_ports[ix].port &= ~(1 << pwm_ports[ix].bit);

	current++;

	reset_timer0();
}

static void init_timer0(void)
{
	static const int compareval = 16;	// 8 mhz / (prescaler = 8) / (counter = 16) = 62500 hz
										// / (steps = 256) = 244 hz

	TCCR0A =	(1 << TCW0)		|	// enable 16 bit mode
				(0 << ICEN0)	|	// enable capture mode
				(0 << ICNC0)	|	// enable capture mode noise canceller
				(0 << ICES0)	|	// enable capture mode edge select
				(0 << ACIC0)	|	// enable capture mode by analog compare
				(0 << 2)		|
				(0 << 1)		|	// reserved
				(0 << WGM00);		// waveform generation mode => normal (n/a)

	OCR0B =		(compareval & 0xff00) >> 8;	// output compare 0 a high byte
	OCR0A =		(compareval & 0x00ff) >> 0;	// output compare 0 a low byte

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

	TCCR0B =	(0 << 7)		|
				(0 << 6)		|
				(0 << 5)		|	// reserved
				(0 << TSM)		|	// enable synchronisation mode
				(0 << PSR0)		|	// reset prescaler
				(0 << CS02)		|	// clock source + prescaler
				(1 << CS01)		|	//		001 = 1, 010 = 8, 011 = 64
				(0 << CS00);		//		100 = 256, 101 = 1024

	reset_timer0();
}

static void build_reply(uint8_t volatile *output_buffer_length, volatile uint8_t *output_buffer,
		uint8_t command, uint8_t error_code, uint8_t reply_length, const uint8_t *reply_string)
{
	uint8_t checksum;
	uint8_t ix;

	output_buffer[0] = 5 + reply_length;
	output_buffer[1] = command;
	output_buffer[2] = command;
	output_buffer[3] = error_code;

	for(ix = 0; ix < reply_length; ix++)
		output_buffer[4 + ix] = reply_string[ix];

	for(ix = 0, checksum = 0; ix < (4 + reply_length); ix++)
		checksum += output_buffer[ix];

	output_buffer[4 + reply_length] = checksum;
	*output_buffer_length = 4 + reply_length + 1;
}

static void twi_callback(uint8_t buffer_size, volatile uint8_t input_buffer_length, const volatile uint8_t *input_buffer,
						uint8_t volatile *output_buffer_length, volatile uint8_t *output_buffer)
{
	uint8_t input;
	uint8_t	command;
	uint8_t	io;
	uint8_t	value;

	//PORTB |= 0x08;
	
	if(input_buffer_length < 1)
		return(build_reply(output_buffer_length, output_buffer, 0, 1, 0, 0));

	input	= input_buffer[0];
	command	= input & 0xf8;
	io		= input & 0x07;

	switch(command)
	{
		case(0x00):	// special
		{
			if(io == 0x00)	//	identify
			{
				static const uint8_t replystring[] =
				{
					0x4a, 0xfb,
					0x06, 0x00,
					't', '8', '6', '1', 'a'
				};

				return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
			}

			break;
		}

		case(0x10):	// 0x10 = read counter, 0x40 = read/reset counter
		case(0x40):
		{
			uint8_t clear = (command == 0x40);

			uint32_t counter;

			switch(io)
			{
				case(0x00):
				{
					counter = portb_count;
					if(clear)
						portb_count = 0;
					break;
				}

				case(0x01):
				{
					counter = porta_count;
					if(clear)
						porta_count = 0;
					break;
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));
				}
			}

			uint8_t replystring[4];

			replystring[0] = (counter & 0xff000000) >> 24;
			replystring[1] = (counter & 0x00ff0000) >> 16;
			replystring[2] = (counter & 0x0000ff00) >> 8;
			replystring[3] = (counter & 0x000000ff) >> 0;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
		}

		case(0x20):	// write counter
		{
			uint32_t counter;

			if(input_buffer_length < 5)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			counter = input_buffer[1];
			counter <<= 8;
			counter |= input_buffer[2];
			counter <<= 8;
			counter |= input_buffer[3];
			counter <<= 8;
			counter |= input_buffer[4];

			switch(io)
			{
				case(0x00):
				{
					portb_count = counter;
					break;
				}

				case(0x01):
				{
					porta_count = counter;
					break;
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));
				}
			}

			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0x30):	// reset counter
		{
			switch(io)
			{
				case(0x00):
				{
					portb_count = 0;
					break;
				}

				case(0x01):
				{
					porta_count = 0;
					break;
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));
				}
			}

			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0x50):	//	read i/o
		{
			uint8_t value;

			switch(io)
			{
				case(0x00):
				{
					value = !!(PINB & _BV(PB6));
					break;
				}

				case(0x01):
				{
					value = !!(PINA & _BV(PA6));
					break;
				}

				case(0x02):
				{
					value = !!(PINB & _BV(PB3));
					break;
				}

				case(0x03):
				{
					value = !!(PINB & _BV(PB4));
					break;
				}

				case(0x04):
				{
					value = !!(PINA & _BV(PA3));
					break;
				}

				case(0x05):
				{
					value = !!(PINA & _BV(PA4));
					break;
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));
				}
			}

			return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &value));
		}

		case(0x60):	//	set i/o
		{
			if(input_buffer_length < 2)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			value = input_buffer[1];

			switch(io)
			{
				case(0x00):
				{
					if(value)
						PORTB |= 0x08;
					else
						PORTB &= ~0x08;

					break;
				}

				case(0x01):
				{
					if(value)
						PORTB |= 0x10;
					else
						PORTB &= ~0x10;

					break;
				}

				case(0x02):
				{
					if(value)
						PORTA |= 0x08;
					else
						PORTA &= ~0x08;

					break;
				}

				case(0x03):
				{
					if(value)
						PORTA |= 0x10;
					else
						PORTA &= ~0x10;

					break;
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));
				}
			}

			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0xb0):	// start adc conversion
		{
			switch(io)
			{
				case(0x00):	//	pa7 / adc6
				{
					ADMUX	=	(0 << REFS1)	|	// select Vcc as Vref
								(0 << REFS0)	|
								(0 << ADLAR)	|	// right adjust result
								(0 << MUX4)		|
								(0 << MUX3)		|
								(1 << MUX2)		|	// select 000110 = adc6 (pa7)
								(1 << MUX1)		|
								(0 << MUX0);

					ADCSRB	=	(0 << BIN)		|	// unipolair input
								(0 << GSEL)		|	// gain select (n/a)
								(0 << 5)		|	// reserved
								(0 << REFS2)	|	// select Vcc as Vref (n/a)
								(0 << MUX5)		|	// select adc6 (pa7)
								(0 << ADTS2)	|
								(0 << ADTS1)	|	// auto trigger source (n/a)
								(0 << ADTS0);

					ADCSRA =	(1 << ADEN)		|	// enable ADC
								(1 << ADSC)		|	// start conversion
								(0 << ADATE)	|	// auto trigger enable
								(1 << ADIF)		|	// clear interrupt flag
								(0 << ADIE)		|	// enable interrupt
								(1 << ADPS2)	|
								(1 << ADPS1)	|
								(0 << ADPS0);		// select clock scaler 110 = 64
					break;
				}

				case(0x01):	// internal temperature sensor
				{
					ADMUX	=	(1 << REFS1)	|	// 010 select internal 1.1 V ref as Vref
								(0 << REFS0)	|
								(0 << ADLAR)	|	// right adjust result
								(1 << MUX4)		|
								(1 << MUX3)		|
								(1 << MUX2)		|	// select 111110 = adc11 (temperature sensor)
								(1 << MUX1)		|
								(1 << MUX0);

					ADCSRB	=	(0 << BIN)		|	// unipolair input
								(0 << GSEL)		|	// gain select (n/a)
								(0 << 5)		|	// reserved
								(0 << REFS2)	|	// 010 select internal 1.1 V ref as Vref
								(1 << MUX5)		|	// select adc11 (temperature sensor)
								(0 << ADTS2)	|
								(0 << ADTS1)	|	// auto trigger source (n/a)
								(0 << ADTS0);

					ADCSRA =	(1 << ADEN)		|	// enable ADC
								(1 << ADSC)		|	// start conversion
								(0 << ADATE)	|	// auto trigger enable
								(1 << ADIF)		|	// clear interrupt flag
								(0 << ADIE)		|	// enable interrupt
								(1 << ADPS2)	|
								(1 << ADPS1)	|
								(0 << ADPS0);		// select clock scaler 110 = 64
					break;
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));
				}
			}

			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0xc0):	// read adc
		{
			uint16_t value;

			switch(io)
			{
				case(0x00):
				{
					value = ADCW;
					break;
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 2, 0, 0));
				}
			}

			if(ADCSRA & _BV(ADSC))	// conversion not ready
				return(build_reply(output_buffer_length, output_buffer, input, 5, 0, 0));

			reset_adc();

			uint8_t replystring[2];

			replystring[0] = (value & 0xff00) >> 8;
			replystring[1] = (value & 0x00ff) >> 0;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
		}

		case(0xd0):	// twi stats
		{
			uint8_t		replystring[2];
			uint16_t	stats;

			switch(io)
			{
				case(0x00):	//	disable
				{
					usi_twi_enable_stats(0);
					return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
				}

				case(0x01):	//	enable
				{
					usi_twi_enable_stats(1);
					return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
				}

				case(0x02):	//	read start conditions
				{
					stats = usi_twi_stats_start_conditions();
					break;
				}

				case(0x03):	//	read stop conditions
				{
					stats = usi_twi_stats_stop_conditions();
					break;
				}

				case(0x04):	//	read error conditions
				{
					stats = usi_twi_stats_error_conditions();
					break;
				}

				case(0x05):	//	read overflow conditions
				{
					stats = usi_twi_stats_overflow_conditions();
					break;
				}

				case(0x06):	//	read local frames
				{
					stats = usi_twi_stats_local_frames();
					break;
				}

				case(0x07):	//	read idle calls
				{
					stats = usi_twi_stats_idle_calls();
					break;
				}
			}

			replystring[0] = (stats & 0xff00) >> 8;
			replystring[1] = (stats & 0x00ff) >> 0;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
		}

		default:
		{
			return(build_reply(output_buffer_length, output_buffer, input, 2, 0, 0));
		}
	}

	return(build_reply(output_buffer_length, output_buffer, input, 5, 0, 0));
}

int main(void)
{
	DDRA = _BV(DDA3) | _BV(DDA4);
	DDRB = _BV(DDB3) | _BV(DDB4);

	PCMSK0 = _BV(PCINT6);
	PCMSK1 = _BV(PCINT14);
	GIMSK  = _BV(PCIE1);

	PRR =		(0 << 7)		|
				(0 << 6)		|	// reserved
				(0 << 5)		|
				(0 << 4)		|
				(1 << PRTIM1)	|	// timer1
				(0 << PRTIM0)	|	// timer0
				(0 << PRUSI)	|	// usi
				(0 << PRADC);		// adc / analog comperator

	init_timer0();

	//setup_wdt();

	usi_twi_slave(0x02, 1, twi_callback, 0);

	return(0);
}
