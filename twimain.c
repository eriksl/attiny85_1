#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include <usitwislave.h>

#include "ioports.h"
#include "adc.h"
#include "timer0.h"
#include "watchdog.h"

//ISR(WDT_vect)
//{
//	setup_wdt();
//
//	PORTA &= ~0x18;
//	PORTB &= ~0x18;
//}

ISR(PCINT_vect)
{
	uint8_t port[2];

	port[0] = (*counter_ports[0].port) & _BV(counter_ports[0].bit);
	port[1] = (*counter_ports[1].port) & _BV(counter_ports[1].bit);

	if(!port[0] && !port[1])
	{
		counter_ports[0].counter = 0;
		counter_ports[1].counter = 0;
		return;
	}

	if(!port[0])
		counter_ports[0].counter++;

	if(!port[1])
		counter_ports[1].counter++;
}

ISR(TIMER0_COMPA_vect)
{
	static	uint8_t current;
			uint8_t ix, nonzero_pwm = 0;

	for(ix = 0; ix < PWM_PORTS; ix++)
	{
		if(pwm_ports[ix].pwm > 0)
			nonzero_pwm++;

		if(current == 0)
		{
			if(pwm_ports[ix].pwm > 0)
				*pwm_ports[ix].port |= (1 << pwm_ports[ix].bit);
			else
				*pwm_ports[ix].port &= ~(1 << pwm_ports[ix].bit);
		}
		else
		{
			if(pwm_ports[ix].pwm == current)
				*pwm_ports[ix].port &= ~(1 << pwm_ports[ix].bit);
		}
	}

	if(nonzero_pwm > 0)
		current++;
	else
	{
		current = 0;
		timer0_stop();
	}

	timer0_reset();
}

static void build_reply(uint8_t volatile *output_buffer_length, volatile uint8_t *output_buffer,
		uint8_t command, uint8_t error_code, uint8_t reply_length, const uint8_t *reply_string)
{
	uint8_t checksum;
	uint8_t ix;

	output_buffer[0] = 4 + reply_length;
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

		case(0x10):	// 0x10 read counter
		case(0x30):	// 0x30 reset counter
		case(0x40): // 0x40 read / reset counter
		{
			if(io >= COUNTER_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint32_t counter = counter_ports[io].counter;

			if(command != 0x10)
				counter_ports[io].counter = 0;

			if(command == 0x30)
				return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));

			uint8_t replystring[4];

			replystring[0] = (counter & 0xff000000) >> 24;
			replystring[1] = (counter & 0x00ff0000) >> 16;
			replystring[2] = (counter & 0x0000ff00) >> 8;
			replystring[3] = (counter & 0x000000ff) >> 0;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
		}

		case(0x20):	// write counter
		{
			if(input_buffer_length < 5)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= COUNTER_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint32_t counter;

			counter = input_buffer[1];
			counter <<= 8;
			counter |= input_buffer[2];
			counter <<= 8;
			counter |= input_buffer[3];
			counter <<= 8;
			counter |= input_buffer[4];

			counter_ports[io].counter = counter;

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

		case(0x70):	//	read pwm
		{
			if(io >= PWM_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t pwm = pwm_ports[io].pwm;

			return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &pwm));
		}

		case(0x80):	//	write pwm
		{
			if(input_buffer_length < 2)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= PWM_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t pwm = input_buffer[1];

			pwm_ports[io].pwm = pwm;

			if(pwm > 0)
			{
				if(timer0_status() == 0)
					timer0_start();
			}
			else
				*pwm_ports[io].port &= ~(1 << pwm_ports[io].bit);

			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0xb0):	// start adc conversion
		{
			if(io > 1)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			adc_start(io);
			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0xc0):	// misc
		{
			uint16_t value;

			switch(io)
			{
				case(0x00):	// 0xc0 read ADC
				{
					value = ADCW;

					if(ADCSRA & _BV(ADSC))	// conversion not ready
						return(build_reply(output_buffer_length, output_buffer, input, 5, 0, 0));

					adc_stop();

					uint8_t replystring[2];

					replystring[0] = (value & 0xff00) >> 8;
					replystring[1] = (value & 0x00ff) >> 0;

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
				}

				case(0x01): // 0xc1 read timer0 status
				{
					uint8_t value = timer0_status();
					return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &value));
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 2, 0, 0));
				}
			}

			return(build_reply(output_buffer_length, output_buffer, input, 2, 0, 0));
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

	PCMSK0 = _BV(PCINT6);	//	PCINT on pa6
	PCMSK1 = _BV(PCINT14);	//	PCINT on pb6
	GIMSK  = _BV(PCIE1);	//	enabe PCINT

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

	// 8 mhz / (prescaler = 64) / (counter = 4) = 31250 hz
	// (steps = 256) = 122 Hz
	timer0_init(TIMER0_PRESCALER_64, 4); 

	usi_twi_slave(0x02, 1, twi_callback, 0);

	return(0);
}
