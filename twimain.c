#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <usitwislave.h>

#include "ioports.h"
#include "adc.h"
#include "timer0.h"
#include "pwm_timer1.h"
#include "watchdog.h"

enum
{
	WATCHDOG_PRESCALER = WATCHDOG_PRESCALER_2K
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
static	const	pwmport_t		*pwmport;
static			pwm_meta_t		softpwm_meta[OUTPUT_PORTS];
static			pwm_meta_t		*softpwm_slot;
static			counter_meta_t	counters_meta[COUNTER_PORTS];

static	uint8_t			slot, dirty, duty, next_duty;
static	uint8_t			timer0_value, timer0_debug_1, timer0_debug_2;

static	void update_static_softpwm_ports(void);

ISR(WDT_vect)
{
	dirty = 0;

	softpwm_slot = &softpwm_meta[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		switch(softpwm_slot->pwm_mode)
		{
			case(pwm_mode_fade_in):
			case(pwm_mode_fade_in_out_cont):
			{
				if(softpwm_slot->duty < 20)
					softpwm_slot->duty += 2;
				else
				{
					if(softpwm_slot->duty < 245)
						softpwm_slot->duty += 10;
					else
					{
						softpwm_slot->duty = 255;

						if(softpwm_slot->pwm_mode == pwm_mode_fade_in)
							softpwm_slot->pwm_mode = pwm_mode_none;
						else
							softpwm_slot->pwm_mode = pwm_mode_fade_out_in_cont;
					}
				}

				dirty = 1;

				break;
			}

			case(pwm_mode_fade_out):
			case(pwm_mode_fade_out_in_cont):
			{
				if(softpwm_slot->duty > 20)
					softpwm_slot->duty -= 10;
				else
				{
					if(softpwm_slot->duty > 2)
						softpwm_slot->duty -= 2;
					else
					{
						softpwm_slot->duty = 0;

						if(softpwm_slot->pwm_mode == pwm_mode_fade_out)
							softpwm_slot->pwm_mode = pwm_mode_none;
						else
							softpwm_slot->pwm_mode = pwm_mode_fade_in_out_cont;
					}
				}

				dirty = 1;

				break;
			}
		}

		softpwm_slot++;
	}

	if(dirty)
	{
		update_static_softpwm_ports();
		timer0_start();
	}

	watchdog_setup(WATCHDOG_PRESCALER);
}

ISR(TIMER0_COMPA_vect) // timer 0 softpwm overflow
{
	softpwm_slot	= &softpwm_meta[0];
	ioport			= &output_ports[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		if(softpwm_slot->duty == 0)				// pwm duty == 0, port is off, set it off
			*ioport->port &= ~_BV(ioport->bit);
		else									// else set the port on
			*ioport->port |=  _BV(ioport->bit);

		softpwm_slot++;
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

	softpwm_slot	= &softpwm_meta[0];
	ioport			= &output_ports[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		if(softpwm_slot->duty <= timer0_value)
			*ioport->port &= ~_BV(ioport->bit);
		else
			if(softpwm_slot->duty < next_duty)
				next_duty = softpwm_slot->duty;

		softpwm_slot++;
		ioport++;
	}

	if(next_duty == 255)
	{
		next_duty = 255;

		softpwm_slot = &softpwm_meta[0];

		for(slot = OUTPUT_PORTS; slot > 0; slot--)
		{
			if((softpwm_slot->duty != 0) && (softpwm_slot->duty < next_duty))
				next_duty = softpwm_slot->duty;

			softpwm_slot++;
		}

		if(next_duty == 255)
			timer0_stop();
	}

	timer0_set_trigger(next_duty);
}

ISR(PCINT_vect)
{
	uint8_t port[2];

	port[0] = *counter_ports[0].port & _BV(counter_ports[0].bit);
	port[1] = *counter_ports[1].port & _BV(counter_ports[1].bit);

	if(!port[0] && !port[1])
	{
		counters_meta[0].counter = 0;
		counters_meta[1].counter = 0;
		return;
	}

	if(!port[0])
		counters_meta[0].counter++;

	if(!port[1])
		counters_meta[1].counter++;
}

static void update_static_softpwm_ports(void)
{
	softpwm_slot	= &softpwm_meta[0];
	ioport			= &output_ports[0];

	for(slot = OUTPUT_PORTS; slot > 0; slot--)
	{
		if(softpwm_slot->duty == 0)
			*ioport->port &= ~_BV(ioport->bit);
		else if(softpwm_slot->duty == 255)
			*ioport->port |= _BV(ioport->bit);

		softpwm_slot++;
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
						0x06, 0x00,
						't', '8', '6', '1', 'a'
					};

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
				}

				case(0x01):	// 0x02 read ADC
				{
					uint8_t value;

					value = ADCW;

					if(ADCSRA & _BV(ADSC))	// conversion not ready
						return(build_reply(output_buffer_length, output_buffer, input, 5, 0, 0));

					adc_stop();

					uint8_t replystring[2];

					replystring[0] = (value & 0xff00) >> 8;
					replystring[1] = (value & 0x00ff) >> 0;

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(replystring), replystring));
				}

				case(0x02): // 0x02 read timer0 counter
				{
					uint8_t value = timer0_get_counter();
					return(build_reply(output_buffer_length, output_buffer, input, 0, 1, &value));
				}

				case(0x03): // 0x03 read timer1 counter
				{
					uint16_t value = pwm_timer1_get_counter();
					uint8_t rv[2];

					rv[0] = (value & 0xff00) >> 8;
					rv[1] = (value & 0x00ff) >> 0;

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(rv), rv));
				}

				case(0x04): // 0x04 read timer1 max
				{
					uint16_t value = pwm_timer1_get_max();
					uint8_t rv[2];

					rv[0] = (value & 0xff00) >> 8;
					rv[1] = (value & 0x00ff) >> 0;

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(rv), rv));
				}

				case(0x05): // 0x05 read timer1 prescaler
				{
					uint8_t value = pwm_timer1_status();

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(value), &value));
				}

				case(0x06): // 0x06 read timer0 entry / exit counter values
				{
					uint8_t value[2];

					value[0] = timer0_debug_1;
					value[1] = timer0_debug_2;

					return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(value), value));
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
			if(io >= COUNTER_PORTS)
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

		case(0x80): // write pwm
		{
			if(input_buffer_length < 3)
				return(build_reply(output_buffer_length, output_buffer, input, 4, 0, 0));

			if(io >= PWM_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint16_t value;

			value = input_buffer[1];
			value <<= 8;
			value |= input_buffer[2];

			pwm_timer1_set_pwm(io, value);

			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
		}

		case(0x90): // read pwm
		{
			if(io >= PWM_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint16_t value = pwm_timer1_get_pwm(io);
			uint8_t reply[2];

			reply[0] = (value & 0xff00) >> 8;
			reply[1] = (value & 0x00ff) >> 0;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(reply), reply));
		}

		case(0xc0):	// start adc conversion
		{
			if(io > 1)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			adc_start(io);
			return(build_reply(output_buffer_length, output_buffer, input, 0, 0, 0));
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
	PCMSK0 = _BV(PCINT6);	//	PCINT on pa6
	PCMSK1 = _BV(PCINT14);	//	PCINT on pb6
	GIMSK  = _BV(PCIE1);	//	enable PCINT

	PRR =		(0 << 7)		|
				(0 << 6)		|	// reserved
				(0 << 5)		|
				(0 << 4)		|
				(0 << PRTIM1)	|	// timer1
				(0 << PRTIM0)	|	// timer0
				(0 << PRUSI)	|	// usi
				(0 << PRADC);		// adc / analog comperator

	uint8_t slot;

	DDRA = 0;
	DDRB = 0;

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
		*output_ports[slot].ddr |= _BV(output_ports[slot].bit);

	for(slot = 0; slot < PWM_PORTS; slot++)
		*pwm_ports[slot].ddr |= _BV(pwm_ports[slot].bit);

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		softpwm_meta[slot].duty		= 0;
		softpwm_meta[slot].pwm_mode	= pwm_mode_none;
	}

	for(slot = 0; slot < COUNTER_PORTS; slot++)
		counters_meta[slot].counter = 0;

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		ioport = &output_ports[slot];
		*ioport->port |= _BV(ioport->bit);
		_delay_ms(50);
	}

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		ioport = &output_ports[slot];
		*ioport->port &= ~_BV(ioport->bit);
		_delay_ms(50);
	}

	for(slot = 0; slot < PWM_PORTS; slot++)
	{
		pwmport = &pwm_ports[slot];
		*pwmport->port |= _BV(pwmport->bit);
		_delay_ms(50);
	}

	for(slot = 0; slot < PWM_PORTS; slot++)
	{
		pwmport = &pwm_ports[slot];
		*pwmport->port &= ~_BV(pwmport->bit);
		_delay_ms(50);
	}

	adc_init();

	// 8 mhz / 256 / 256 = 122 Hz
	timer0_init(TIMER0_PRESCALER_256);
	timer0_set_max(0xff);

	// 8 mhz / 32 / 1024 = 244 Hz
	pwm_timer1_init(PWM_TIMER1_PRESCALER_32);
	pwm_timer1_set_max(0x3ff);
	pwm_timer1_start();

	watchdog_setup(WATCHDOG_PRESCALER);
	watchdog_start();

	usi_twi_slave(0x02, 1, twi_callback, 0);

	return(0);
}
