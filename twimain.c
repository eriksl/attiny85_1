#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include <usitwislave.h>

#include "ioports.h"
#include "adc.h"
#include "timer0.h"
#include "watchdog.h"

static volatile uint32_t porta_count = 0, portb_count = 0;

static volatile uint8_t pwm_value[PWM_PORTS] = { 128, 128, 128, 128 };

//ISR(WDT_vect)
//{
//	setup_wdt();
//
//	PORTA &= ~0x18;
//	PORTB &= ~0x18;
//}

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

	timer0_reset();
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

		case(0x50):	//	read input
		{
			uint8_t value;

			if(io >= INPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			value = !!(*input_ports[io].port & (1 << input_ports[io].bit));

			return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &value));
		}

		case(0x60):	//	write output
		{
			if(input_buffer_length < 2)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			if(input_buffer[1])
				*output_ports[io].port |= (1 << output_ports[io].bit);
			else
				*output_ports[io].port &= ~(1 << output_ports[io].bit);

			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0xb0):	// start adc conversion
		{
			if(io > 1)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			adc_start(io);
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

			adc_stop();

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

		case(0xe0):	//	read output
		{
			uint8_t value;

			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			value = !!(*output_ports[io].port & (1 << output_ports[io].bit));

			return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &value));
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

	// watchdog_setup();
	// watchdog_start();
	
	adc_init();

	// 8 mhz / (prescaler = 8) / (counter = 16) = 62500 hz
	// (steps = 256) = 244 hz
	// timer0_init(8, 16); 

	usi_twi_slave(0x02, 1, twi_callback, 0);

	return(0);
}
