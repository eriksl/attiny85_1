#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include <usitwislave.h>

#include "ioports.h"
#include "adc.h"
#include "timer0.h"

volatile static struct
{
	uint8_t	ioport;
	uint8_t	duty;
} soft_pwm_slots[OUTPUT_PORTS];

volatile static uint32_t	counters[COUNTER_PORTS];
volatile static uint8_t		current_slot;

ISR(TIMER0_COMPA_vect)		// timer 0 softpwm overflow
{
	uint8_t slot, slot_port, slot_duty, active = 0;

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		slot_port	= soft_pwm_slots[slot].ioport;
		slot_duty	= soft_pwm_slots[slot].duty;

		if((slot_duty != 0) && (slot_duty != 0xff))
			active++;

		if(slot_duty == 0)					// pwm duty == 0, port is off, set it off
			*output_ports[slot_port].port &= ~_BV(output_ports[slot_port].bit);
		else								// else set the port on
			*output_ports[slot_port].port |=  _BV(output_ports[slot_port].bit);
	}

	if(active == 0)
		timer0_stop();
}

ISR(TIMER0_COMPB_vect)		// timer 0 softpwm trigger
{
	uint8_t		slot_port;
	uint8_t		slot_duty, next_duty;

	slot_duty = soft_pwm_slots[current_slot].duty;

	if((slot_duty != 0) && (slot_duty != 0xff))			// only process active slots
	{
		for(; current_slot < OUTPUT_PORTS; current_slot++)	// process all slots with this duty value
		{
			if(soft_pwm_slots[current_slot].duty != slot_duty)
				break;

			slot_port = soft_pwm_slots[current_slot].ioport;
			*output_ports[slot_port].port &= ~_BV(output_ports[slot_port].bit);
		}
	}

	if(current_slot >= OUTPUT_PORTS)
		current_slot = 0;

	next_duty = 0;

	for(; current_slot < OUTPUT_PORTS; current_slot++)		// skip slots with duty 0 (off) and duty 0xff (always on)
	{
		next_duty = soft_pwm_slots[current_slot].duty;

		if((next_duty != 0xff) && (next_duty != 0))
			break;
	}

	if(current_slot >= OUTPUT_PORTS)
	{
		current_slot = 0;
		next_duty = soft_pwm_slots[current_slot].duty;
	}

	timer0_set_trigger(next_duty);
}

ISR(PCINT_vect)
{
	uint8_t port[2];

	port[0] = (*counter_ports[0].port) & _BV(counter_ports[0].bit);
	port[1] = (*counter_ports[1].port) & _BV(counter_ports[1].bit);

	if(!port[0] && !port[1])
	{
		counters[0] = 0;
		counters[1] = 0;
		return;
	}

	if(!port[0])
		counters[0]++;

	if(!port[1])
		counters[1]++;
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

	//PORTB |= 0x08;

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

				case(0x02): // 0x03 read timer0 status
				{
					uint8_t value = timer0_status();
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
			if(io >= COUNTER_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint32_t counter = counters[io];

			if(command == 0x20)
				counters[io] = 0;

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

			uint8_t slot, port, new_duty;

			new_duty = input_buffer[1];

			timer0_stop();

			// find the slot that is using this io port
			// then assign i/o port and duty cycle

			for(slot = 0; slot < OUTPUT_PORTS; slot++)
				if(soft_pwm_slots[slot].ioport == io)
					break;

			if(slot >= OUTPUT_PORTS) // "unknown error"
				return(build_reply(output_buffer_length, output_buffer, input, 6, 0, 0));

			soft_pwm_slots[slot].duty = new_duty;

			// If duty = 0 or duty = 0xff, turn this ports off/on immediately.
			// The timer may not get restarted when all ports are inactive,
			// so do it here.

			port = soft_pwm_slots[slot].ioport;

			if(new_duty == 0)
				*output_ports[port].port &= ~_BV(output_ports[port].bit);
			else
				if(new_duty == 0xff)
					*output_ports[port].port |= _BV(output_ports[port].bit);

			// Now (re-)sort the slots.
			// Using bubble sort is okay because there are so little entries and it uses very little memory.
			// Bubble higher duty values and unused elements to the end.

			uint8_t duty[2], ioport[2], dirty;

			do
			{
				dirty = 0;

				for(slot = 0; slot < (OUTPUT_PORTS - 1); slot++)
				{
					duty[0]		= soft_pwm_slots[slot + 0].duty;
					duty[1]		= soft_pwm_slots[slot + 1].duty;

					ioport[0]	= soft_pwm_slots[slot + 0].ioport;
					ioport[1]	= soft_pwm_slots[slot + 1].ioport;

					if(duty[0] > duty[1])
					{
						soft_pwm_slots[slot + 0].duty	= duty[1];
						soft_pwm_slots[slot + 1].duty	= duty[0];

						soft_pwm_slots[slot + 0].ioport	= ioport[1];
						soft_pwm_slots[slot + 1].ioport	= ioport[0];

						dirty = 1;
					}
				}
			} while(dirty);

			// Find first active duty slot (duty != 0 && duty != 0xff)

			for(current_slot = 0; current_slot < OUTPUT_PORTS; current_slot++)
			{
				new_duty = soft_pwm_slots[current_slot].duty;

				if((new_duty != 0) && (new_duty != 0xff))
					break;
			}

			if(current_slot < OUTPUT_PORTS)
			{
				timer0_reset_counter();
				timer0_set_trigger(new_duty);
				timer0_start();
			}

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(soft_pwm_slots), (uint8_t *)soft_pwm_slots));
		}

		case(0x50):	//	read output / softpwm
		{
			if(io >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t slot;

			for(slot = 0; slot < OUTPUT_PORTS; slot++)
				if(soft_pwm_slots[slot].ioport == io)
					break;

			if(slot >= OUTPUT_PORTS)
				return(build_reply(output_buffer_length, output_buffer, input, 3, 0, 0));

			uint8_t duty;

			duty = soft_pwm_slots[slot].duty;

			return(build_reply(output_buffer_length, output_buffer, input, 0, sizeof(duty), &duty));
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
				(1 << PRTIM1)	|	// timer1
				(0 << PRTIM0)	|	// timer0
				(0 << PRUSI)	|	// usi
				(0 << PRADC);		// adc / analog comperator

	uint8_t slot;

	DDRA = 0;
	DDRB = 0;

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
		*output_ports[slot].ddr |= _BV(output_ports[slot].bit);

	for(slot = 0; slot < OUTPUT_PORTS; slot++)
	{
		soft_pwm_slots[slot].duty     = 0;
		soft_pwm_slots[slot].ioport	= slot;
	}

	for(slot = 0; slot < COUNTER_PORTS; slot++)
		counters[slot] = 0;

	adc_init();

	// 8 mhz / 256 / 256 = 122

	timer0_init(TIMER0_PRESCALER_256);
	timer0_set_max(0xff);

	usi_twi_slave(0x02, 1, twi_callback, 0);

	return(0);
}
