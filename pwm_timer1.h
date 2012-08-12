#if !defined(_PWM_TIMER1_H_)
#define _PWM_TIMER1_H_

#include <stdint.h>
#include <avr/io.h>

#define always_inline __attribute__((always_inline)) __attribute__((used))

enum
{
	PWM_TIMER1_PRESCALER_OFF	= 0,
	PWM_TIMER1_PRESCALER_1		= 1,
	PWM_TIMER1_PRESCALER_2		= 2,
	PWM_TIMER1_PRESCALER_4		= 3,
	PWM_TIMER1_PRESCALER_8		= 4,
	PWM_TIMER1_PRESCALER_16		= 5,
	PWM_TIMER1_PRESCALER_32		= 6,
	PWM_TIMER1_PRESCALER_64		= 7,
	PWM_TIMER1_PRESCALER_128	= 8,
	PWM_TIMER1_PRESCALER_256	= 9,
	PWM_TIMER1_PRESCALER_512	= 10,
	PWM_TIMER1_PRESCALER_1024	= 11,
	PWM_TIMER1_PRESCALER_2048	= 12,
	PWM_TIMER1_PRESCALER_4096	= 13,
	PWM_TIMER1_PRESCALER_8192	= 14,
	PWM_TIMER1_PRESCALER_16384	= 15
};

		void		pwm_timer1_init(uint8_t prescaler);
static	void		pwm_timer1_reset_counter(void);
		uint16_t	pwm_timer1_get_counter(void);
		void		pwm_timer1_set_max(uint16_t);
		uint16_t	pwm_timer1_get_max(void);
		void		pwm_timer1_set_pwm(uint8_t port, uint16_t value);
		uint16_t	pwm_timer1_get_pwm(uint8_t port);
		void		pwm_timer1_start(void);
		void		pwm_timer1_stop(void);
		uint8_t		pwm_timer1_status(void);

static always_inline void pwm_timer1_reset_counter(void)
{
	TC1H	= 0;
	TCNT1	= 0;
}

#endif
