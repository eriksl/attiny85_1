#include <avr/io.h>

#include "ioports.h"
#include "adc.h"

void adc_init(void)
{
	PRR &= ~_BV(PRADC);

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
}

void adc_start(uint8_t source)	// source: 0 = adc6 = pa7, 1 = internal temperature sensor
{
	const adcport_t * p;

	if(source >= ADC_PORTS)
		return;

	p = &adc_ports[source];

	ADMUX	=	(p->refs[1]	<< REFS1)	|	// Vref
				(p->refs[0]	<< REFS0)	|
				(0			<< ADLAR)	|	// right adjust result
				(p->mux[4]	<< MUX4)	|
				(p->mux[3]	<< MUX3)	|
				(p->mux[2]	<< MUX2)	|	// input
				(p->mux[1]	<< MUX1)	|
				(p->mux[0]	<< MUX0);

	ADCSRB	=	(0			<< BIN)		|	// unipolair input
				(0			<< GSEL)	|	// gain select (n/a)
				(0			<< 5)		|	// reserved
				(p->refs[2]	<< REFS2)	|
				(p->mux[5]	<< MUX5)	|
				(0			<< ADTS2)	|
				(0			<< ADTS1)	|	// auto trigger source (n/a)
				(0			<< ADTS0);

	ADCSRA =	(1			<< ADEN)	|	// enable ADC
				(1			<< ADSC)	|	// start conversion
				(0			<< ADATE)	|	// auto trigger enable
				(1			<< ADIF)	|	// clear interrupt flag
				(0			<< ADIE)	|	// enable interrupt
				(1			<< ADPS2)	|
				(1			<< ADPS1)	|
				(0			<< ADPS0);		// select clock scaler 110 = 64
};

void adc_stop(void)
{
	ADCSRA =	(0 << ADEN)		|	// enable ADC
				(0 << ADSC)		|	// start conversion
				(0 << ADATE)	|	// auto trigger enable
				(1 << ADIF)		|	// clear interrupt flag
				(0 << ADIE)		|	// enable interrupt
				(1 << ADPS2)	|
				(1 << ADPS1)	|
				(0 << ADPS0);		// select clock scaler 110 = 64
};
