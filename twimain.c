#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usitwislave.h"

static volatile uint32_t porta_count = 0, portb_count = 0;

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

ISR(PCINT_vect)
{
	if(!(PINA & _BV(PA6)) && !(PINB & _BV(PB6)))
	{
		porta_count = 0;
		portb_count = 0;

		return;
	}

	if(!(PINA & _BV(PA6)))
		porta_count++;

	if(!(PINB & _BV(PB6)))
		portb_count++;
}

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
	{
		build_reply(output_buffer_length, output_buffer, 0, 1, 0, 0);
		return;
	}

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

				build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring);
				return;
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
					build_reply(output_buffer_length, output_buffer, input, 3, 0, 0);
					return;
				}
			}

			uint8_t replystring[4];

			replystring[0] = (counter & 0xff000000) >> 24;
			replystring[1] = (counter & 0x00ff0000) >> 16;
			replystring[2] = (counter & 0x0000ff00) >> 8;
			replystring[3] = (counter & 0x000000ff) >> 0;

			build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring);
			return;
		}

		case(0x20):	// write counter
		{
			uint32_t counter;

			if(input_buffer_length < 5)
			{
				build_reply(output_buffer_length, output_buffer, input, 4, 0, 0);
				return;
			}

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
					build_reply(output_buffer_length, output_buffer, input, 3, 0, 0);
					return;
				}
			}

			build_reply(output_buffer_length, output_buffer, input, 0, 0, 0);
			return;
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
					build_reply(output_buffer_length, output_buffer, input, 3, 0, 0);
					return;
				}
			}

			build_reply(output_buffer_length, output_buffer, input, 0, 0, 0);
			return;
		}

		case(0x60):
		{
			if(input_buffer_length < 2)
			{
				build_reply(output_buffer_length, output_buffer, input, 4, 0, 0);
				return;
			}

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
					build_reply(output_buffer_length, output_buffer, input, 3, 0, 0);
					return;
				}
			}

			build_reply(output_buffer_length, output_buffer, input, 0, 0, 0);
			return;
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
					build_reply(output_buffer_length, output_buffer, input, 3, 0, 0);
					return;
				}
			}

			build_reply(output_buffer_length, output_buffer, input, 0, 0, 0);
			return;
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
					build_reply(output_buffer_length, output_buffer, input, 2, 0, 0);
					return;
				}
			}

			if(ADCSRA & _BV(ADSC))	// conversion not ready
			{
				build_reply(output_buffer_length, output_buffer, input, 5, 0, 0);
				return;
			}

			reset_adc();

			uint8_t replystring[2];

			replystring[0] = (value & 0xff00) >> 8;
			replystring[1] = (value & 0x00ff) >> 0;

			build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring);
			return;
		}

		default:
		{
			build_reply(output_buffer_length, output_buffer, input, 2, 0, 0);
			return;
		}
	}

	build_reply(output_buffer_length, output_buffer, input, 5, 0, 0);
}

static void twi_idle()
{
	porta_count++;
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
				(1 << PRTIM0)	|	// timer0
				(0 << PRUSI)	|	// usi
				(0 << PRADC);		// adc / analog comperator

	//setup_wdt();

	usi_twi_slave(0x02, 1, twi_callback, twi_idle);

	return(0);
}
