#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usitwislave.h"

static volatile uint8_t porta_count = 0, portb_count = 0;

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

static void twi_callback(uint8_t buffer_size, volatile uint8_t input_buffer_length, const volatile uint8_t *input_buffer,
						uint8_t volatile *output_buffer_length, volatile uint8_t *output_buffer)
{
	uint8_t	command;
	uint8_t	io;
	uint8_t ix;
	uint8_t	checksum;
	uint8_t	value;

	//PORTB |= 0x08;

	*output_buffer_length = 0;

	if(input_buffer_length < 1)
		goto return_parameter_error;

	command	= input_buffer[0] & 0xf8;
	io		= input_buffer[0] & 0x07;

	switch(command)
	{
		case(0x00):
		{
			if(io == 0x00)
			{
				static const uint8_t idstring[] = { 0x4a, 0xfb, 0x06, 0x00,
						'a', 't', 't', 'i', 'n', 'y', '8', '6', '1', 'a', 0x00 };

				for(ix = 0, checksum = 0; ix < 14; ix++)
				{
					output_buffer[ix] = idstring[ix];
					checksum += idstring[ix];
				}

				output_buffer[14] = checksum;
				*output_buffer_length = 15;

				return;
			}

			break;
		}

		case(0x60):
		{
			if(input_buffer_length < 2)
				return;

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
					goto return_parameter_error;
				}
			}

			break;
		}

		default:
		{
			goto return_command_error;
		}
	}

return_ok:
	output_buffer[0] = output_buffer[1] = 0xfd;
	output_buffer[2] = input_buffer[0];
	*output_buffer_length = 3;
	return;

return_parameter_error:
	output_buffer[0] = output_buffer[1] = 0xfb;
	output_buffer[2] = input_buffer[0];
	*output_buffer_length = 3;
	return;

return_command_error:
	output_buffer[0] = output_buffer[1] = 0xfc;
	output_buffer[2] = input_buffer[0];
	*output_buffer_length = 3;
}

int main(void)
{
	DDRA = _BV(DDA3) | _BV(DDA4);
	DDRB = _BV(DDB3) | _BV(DDB4);

	PCMSK0 = _BV(PCINT6);
	PCMSK1 = _BV(PCINT14);
	GIMSK  = _BV(PCIE1);

	PRR = 
		_BV(PRTIM1) |
		_BV(PRTIM0) |
		_BV(PRADC);

	//setup_wdt();

	usi_twi_slave(0x02, 0, twi_callback, 0);

	return(0);
}
