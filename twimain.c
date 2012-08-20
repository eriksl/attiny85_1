#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <usitwislave.h>

#include "ioports.h"
#include "timer0.h"
#include "watchdog.h"

enum
{
	PWM_MODE_WATCHDOG_SCALER = WATCHDOG_PRESCALER_2K,
};

typedef enum
{
	pwm_mode_none				= 0,
	pwm_mode_fade_in			= 1,
	pwm_mode_fade_out			= 2,
	pwm_mode_fade_in_out_cont	= 3,
	pwm_mode_fade_out_in_cont	= 4
} pwm_mode_t;

typedef struct
{
	uint8_t		duty;
	pwm_mode_t	pwm_mode:8;
} pwm_meta_t;

typedef struct
{
	uint32_t	counter;
} counter_meta_t;

static	const	ioport_t		*ioport;
static			pwm_meta_t		softpwm_meta[OUTPUT_PORTS];
static			pwm_meta_t		*pwm_slot;
static			counter_meta_t	counters_meta[INPUT_PORTS];

static	uint8_t		slot, dirty, duty, next_duty, diff;
static	uint8_t		timer0_value;

static	void update_static_softpwm_ports(void);

ISR(WDT_vect)
{
	dirty = 0;

	pwm_slot = &softpwm_meta[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		duty	= pwm_slot->duty;
		diff	= duty / 10;

		if(diff < 3)
			diff = 3;

		switch(pwm_slot->pwm_mode)
		{
			case(pwm_mode_fade_in):
			case(pwm_mode_fade_in_out_cont):
			{
				if(duty < (255 - diff))
					duty += diff;
				else
				{
					duty = 255;

					if(pwm_slot->pwm_mode == pwm_mode_fade_in)
						pwm_slot->pwm_mode = pwm_mode_none;
					else
						pwm_slot->pwm_mode = pwm_mode_fade_out_in_cont;
				}

				pwm_slot->duty = duty;

				dirty = 1;

				break;
			}

			case(pwm_mode_fade_out):
			case(pwm_mode_fade_out_in_cont):
			{
				if(duty > diff)
					duty -= diff;
				else
				{
					duty = 0;

					if(pwm_slot->pwm_mode == pwm_mode_fade_out)
						pwm_slot->pwm_mode = pwm_mode_none;
					else
						pwm_slot->pwm_mode = pwm_mode_fade_in_out_cont;
				}

				pwm_slot->duty = duty;

				dirty = 1;

				break;
			}
		}

		pwm_slot++;
	}

	if(dirty)
	{
		update_static_softpwm_ports();
		timer0_start();
	}

	watchdog_setup(PWM_MODE_WATCHDOG_SCALER);
}

ISR(TIMER0_COMPA_vect) // timer 0 softpwm overflow
{
	pwm_slot	= &softpwm_meta[0];
	ioport		= &output_ports[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		if(pwm_slot->duty == 0)				// pwm duty == 0, port is off, set it off
			*ioport->port &= ~_BV(ioport->bit);
		else									// else set the port on
			*ioport->port |=  _BV(ioport->bit);

		pwm_slot++;
		ioport++;
	}
}

ISR(TIMER0_COMPB_vect) // timer 0 softpwm trigger
{
	timer0_value = timer0_get_counter();

	if(timer0_value < 253)
		timer0_value += 1;
	else
		timer0_value = 255;

	next_duty = 255;

	pwm_slot	= &softpwm_meta[0];
	ioport		= &output_ports[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		if(pwm_slot->duty <= timer0_value)
			*ioport->port &= ~_BV(ioport->bit);
		else
			if(pwm_slot->duty < next_duty)
				next_duty = pwm_slot->duty;

		pwm_slot++;
		ioport++;
	}

	if(next_duty == 255)
	{
		next_duty = 255;

		pwm_slot = &softpwm_meta[0];

		for(slot = OUTPUT_PORTS; slot > 0; slot--)
		{
			if((pwm_slot->duty != 0) && (pwm_slot->duty < next_duty))
				next_duty = pwm_slot->duty;

			pwm_slot++;
		}

		if(next_duty == 255)
			timer0_stop();
	}

	timer0_set_trigger(next_duty);
}

ISR(PCINT0_vect)
{
	uint8_t port;

	port = *input_ports[0].port & _BV(input_ports[0].bit);

	if(!port)
		counters_meta[0].counter++;
}

static void update_static_softpwm_ports(void)
{
	pwm_slot	= &softpwm_meta[0];
	ioport		= &output_ports[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		if(pwm_slot->duty == 0)
			*ioport->port &= ~_BV(ioport->bit);
		else if(pwm_slot->duty == 255)
			*ioport->port |= _BV(ioport->bit);

		pwm_slot++;
		ioport++;
	}
}

static void build_reply(uint8_t volatile *output_buffer_length, volatile uint8_t *output_buffer,
		uint8_t command, uint8_t error_code, uint8_t reply_length, const uint8_t *reply_string)
{
	uint8_t checksum;
	uint8_t ix;

	output_buffer[0] = 3 + reply_length;
	output_buffer[1] = error_code;
	output_buffer[2] = command;

	for(ix = 0; ix < reply_length; ix++)
		output_buffer[3 + ix] = reply_string[ix];

	for(ix = 1, checksum = 0; ix < (3 + reply_length); ix++)
		checksum += output_buffer[ix];

	output_buffer[3 + reply_length] = checksum;
	*output_buffer_length = 3 + reply_length + 1;
}

static void twi_callback(uint8_t buffer_size, volatile uint8_t input_buffer_length, const volatile uint8_t *input_buffer,
						uint8_t volatile *output_buffer_length, volatile uint8_t *output_buffer)
{
	uint8_t input;
	uint8_t	command;
	uint8_t	io;

	if(input_buffer_length < 1)
		return(build_reply(output_buffer_length, output_buffer, 0, 1, 0, 0));

	input	= input_buffer[0];
	command	= input & 0xf8;
	io		= input & 0x07;

	switch(command)
	{
		case(0x00):	// short / no-io
		{
			switch(io)
			{
				case(0x00):	// identify
				{
					static const uint8_t replystring[] =
					{
						0x4a, 0xfb,
						0x05, 0x01, 0x00,
						't', '8', '5'
					};

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
				}

				case(0x02): // 0x02 DEBUG read timer0 counter
				{
					uint8_t value = timer0_get_counter();
					return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &value));
				}

				case(0x07): // extended command
				{
					return(build_reply(output_buffer_length, output_buffer, input, 7, 0, 0));
				}

				default:
				{
					return(build_reply(output_buffer_length, output_buffer, input, 7, 0, 0));
				}
			}

			break;
		}

		case(0x10):	// 0x10 read counter
		case(0x20): // 0x20 read / reset counter
		{
			if(io >= INPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint32_t counter = counters_meta[io].counter;

			if(command == 0x20)
				counters_meta[io].counter = 0;

			uint8_t replystring[4];

			replystring[0] = (counter & 0xff000000) >> 24;
			replystring[1] = (counter & 0x00ff0000) >> 16;
			replystring[2] = (counter & 0x0000ff00) >> 8;
			replystring[3] = (counter & 0x000000ff) >> 0;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
		}

		case(0x30):	//	read input
		{
			uint8_t value;

			if(io >= INPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			value = !!(*input_ports[io].port & (1 << input_ports[io].bit));

			return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &value));
		}

		case(0x40):	//	write output / softpwm
		{
			if(input_buffer_length < 2)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			softpwm_meta[io].duty = input_buffer[1];
			update_static_softpwm_ports();
			timer0_start();

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(softpwm_meta), (uint8_t *)softpwm_meta));
		}

		case(0x50):	// read output / softpwm
		{
			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			duty = softpwm_meta[io].duty;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(duty), &duty));
		}

		case(0x60): // write softpwm mode
		{
			if(input_buffer_length < 2)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t mode = input_buffer[1];

			if(mode > 3)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			softpwm_meta[io].pwm_mode = input_buffer[1];

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(softpwm_meta), (uint8_t *)softpwm_meta));
		}

		case(0x70):	// read softpwm mode
		{
			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t mode;

			mode = softpwm_meta[io].pwm_mode;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(mode), &mode));
		}

		case(0xf0):	// twi stats
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

	return(build_reply(output_buffer_length, output_buffer, input, 2, 0, 0));
}

int main(void)
{
	PCMSK	= _BV(PCINT3);	//	PCINT on pb3
	GIMSK	= _BV(PCIE);	//	enable PCINT

	PRR =		(0 << 7)		|
				(0 << 6)		|	// reserved
				(0 << 5)		|
				(0 << 4)		|
				(1 << PRTIM1)	|	// timer1
				(0 << PRTIM0)	|	// timer0
				(0 << PRUSI)	|	// usi
				(1 << PRADC);		// adc / analog comperator

	DDRB = 0;

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
		*output_ports[slot].ddr |= _BV(output_ports[slot].bit);

	for(slot = 0; slot < INPUT_PORTS; slot++)
	{
		*input_ports[slot].port |= _BV(input_ports[slot].bit);	// enable pullup
		counters_meta[slot].counter = 0;
	}

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		softpwm_meta[slot].duty		= 0;
		softpwm_meta[slot].pwm_mode	= pwm_mode_none;
	}

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		ioport = &output_ports[slot];
		*ioport->port |= _BV(ioport->bit);
		_delay_ms(250);
	}

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		ioport = &output_ports[slot];
		*ioport->port &= ~_BV(ioport->bit);
		_delay_ms(250);
	}

	// 8 mhz / 256 / 256 = 122 Hz
	timer0_init(TIMER0_PRESCALER_256);
	timer0_set_max(0xff);

	watchdog_setup(PWM_MODE_WATCHDOG_SCALER);
	watchdog_start();

	usi_twi_slave(0x02, 1, twi_callback, 0);

	return(0);
}
