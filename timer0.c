/*
 * timer0.c
 *
 * Author: Peter Sutton
 *
 * We setup timer0 to generate an interrupt every 1ms
 * We update a global clock tick variable - whose value
 * can be retrieved using the get_clock_ticks() function.
 */

#define F_CPU 8000000UL	// 8MHz
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <stdint.h>


#include "timer0.h"
#include "score.h"
#include "game.h"
#include "terminalio.h"
#include "buttons.h"
#include "ledmatrix.h"
#include "scrolling_char_display.h"

/* Our internal clock tick count - incremented every 
 * millisecond. Will overflow every ~49 days. */
static volatile uint32_t clockTicks;

//make some of port c output to display score above 99;
int move;
volatile uint32_t lives = 4;

/* Set up timer 0 to generate an interrupt every 1ms. 
 * We will divide the clock by 64 and count up to 124.
 * We will therefore get an interrupt every 64 x 125
 * clock cycles, i.e. every 1 milliseconds with an 8MHz
 * clock. 
 * The counter will be reset to 0 when it reaches it's
 * output compare value.
 */



/* digits_displayed - 1 if digits are displayed on the seven
** segment display, 0 if not. No digits displayed initially.
*/
volatile uint8_t digits_displayed = 1;

/* Time value - we count hundredths of seconds,
** i.e. increment the count every 10ms.
*/
volatile uint16_t score;
volatile uint16_t value;
void joy_stick(void);


//prototyping the set and get lives methods
void set_lives(void);
int get_lives(void);
void init_lives(void);

//functions responsible for the sound
uint16_t freq_to_clock_period(uint16_t freq);
uint16_t duty_cycle_to_pulse_width(float dutycycle, uint16_t clockperiod);
void game_playing(void);
void game_start_tune(void);


/* Seven segment display digit being displayed.
** 0 = right digit; 1 = left digit.
*/
volatile uint8_t seven_seg_cc = 0;
/* Seven segment display segment values for 0 to 9 */
uint8_t seven_seg_data[10] = {63,6,91,79,102,109,125,7,127,111};
void init_timer0(void) {
	/* Reset clock tick count. L indicates a long (32 bit) 
	 * constant. 
	 */
	clockTicks = 0L;
	
	/* Make all bits of port A and the least significant
	** bit of port C be output bits to support the 7 segment and lives of the player
	*/
	DDRC = 0xFF;
	
	/* Clear the timer */
	TCNT0 = 0;

	/* Set the output compare value to be 124 */
	OCR0A = 124;
	
	/* Set the timer to clear on compare match (CTC mode)
	 * and to divide the clock by 64. This starts the timer
	 * running.
	 */
	TCCR0A = (1<<WGM01);
	TCCR0B = (1<<CS01)|(1<<CS00);

	/* Enable an interrupt on output compare match. 
	 * Note that interrupts have to be enabled globally
	 * before the interrupts will fire.
	 */
	TIMSK0 |= (1<<OCIE0A);
	
	/* Make sure the interrupt flag is cleared by writing a 
	 * 1 to it.
	 */
	TIFR0 &= (1<<OCF0A);
}

uint32_t get_current_time(void) {
	uint32_t returnValue;
	uint8_t interruptsOn = bit_is_set(SREG, SREG_I);
	cli();
	returnValue = clockTicks;
	if(interruptsOn) {
		sei();
	}
	return returnValue;
}

ISR(TIMER0_COMPA_vect) {
	/* Increment our clock tick count */
	score_display();
	
	clockTicks++;
}

void score_display(void){
	//flip the display select
	//check if the score reaches 99 so that we can switch on an LED to indicate
	//scores above it.
	seven_seg_cc = 1 ^ seven_seg_cc;
	PORTC = 0;
	
	if(get_score() >= 100){		score = get_score()%100;	} else {		score = get_score();	}	if(digits_displayed) {
		/* Display a digit */
		if(seven_seg_cc == 0) {
			PORTA &= ~(1 << PORTA2);
			/* Display rightmost digit - tenths of seconds */
			PORTC = seven_seg_data[(score)%10];
			
		} else {
			PORTA |= (1 << PORTA2);
			PORTC = seven_seg_data[(score/10)];
		}
	}
}

int get_lives(void){
	return lives;
}

void set_lives(void){
	
	lives --;
	
	if(lives == 3){
		PORTA &= 0X3C;
	} else if(lives == 2){
		PORTA &= 0X34;
	} else if(lives == 1) {
		PORTA &= 0X24;
	} else if(lives == 0) {
		add_to_score(0);
		PORTA &= 0X4;
		game_over(1);
		
		
	}
	//switch on Led to indicate that score has reach above 100 since the 7 seg can't display above 99
	if(get_score() == 2){
		PORTD |= (1 << 6);
		
	}
	if(get_score() == 5) {
		PORTD |= (1 << 7);
	}

	
}

void init_lives(void){
	lives = 4;
}
// For a given frequency (Hz), return the clock period (in terms of the
// number of clock cycles of a 1MHz clock)
uint16_t freq_to_clock_period(uint16_t freq) {
	return (1000000UL / freq);	// UL makes the constant an unsigned long (32 bits)
	// and ensures we do 32 bit arithmetic, not 16
}

// Return the width of a pulse (in clock cycles) given a duty cycle (%) and
// the period of the clock (measured in clock cycles)
uint16_t duty_cycle_to_pulse_width(float dutycycle, uint16_t clockperiod) {
	return (dutycycle * clockperiod) / 100;
}


void game_playing(void){
	uint16_t freq = 50;	// Hz
	float dutycycle = 2;	// %
	uint16_t clockperiod = freq_to_clock_period(freq);
	uint16_t pulsewidth = duty_cycle_to_pulse_width(dutycycle, clockperiod);
	
	
	
	// Set the maximum count value for timer/counter 1 to be one less than the clockperiod
	OCR1A = clockperiod - 1;
	
	// Set the count compare value based on the pulse width. The value will be 1 less
	// than the pulse width - unless the pulse width is 0.
	if(pulsewidth == 0) {
		OCR1B = 0;
		} else {
		OCR1B = pulsewidth - 1;
	}
	
	// Set up timer/counter 1 for Fast PWM, counting from 0 to the value in OCR1A
	// before reseting to 0. Count at 1MHz (CLK/8).
	// Configure output OC1B to be clear on compare match and set on timer/counter
	// overflow (non-inverting mode).
	TCCR1A = (1 << COM1B1) | (0 <<COM1B0) | (1 <<WGM11) | (1 << WGM10);
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (0 << CS12) | (1 << CS11) | (0 << CS10);

	// PWM output should now be happening - at the frequency and pulse width set above
	
	// Check the state of the buttons (on port C) every 100ms.
	if(button_pushed() == NO_BUTTON_PUSHED) {
		_delay_ms(100);
		
		if(PINB & 0x01) { // increase frequency by 5%, but highest frequency is 10000Hz
			freq = freq*105UL/100UL;	// Constants made 32 bit to ensure 32 bit arithmetic
			if(freq > 100) {
				freq = 100;
			}
		}
		if(PINB & 0x02) { // decrease frequency by 5%, but lowest frequency is 20Hz
			freq = freq*95UL/100UL;		// Constants made 32 bits to ensure 32 bit arithmetic
			if(freq < 10) {
				freq = 10;
			}
		}
		if(PINB & 0x04) { // increase duty cycle by 0.1 if less than 10% or 1 if 10% or higher
			if(dutycycle < 10) {
				dutycycle += 0.1;
				} else {
				dutycycle += 1.0;
				if(dutycycle > 10) {
					dutycycle = 10;
				}
			}
		}
		if(PINB & 0x08) { // decrease duty cycle by 0.1 if less than 10% or 1 if 10% or higher
			if(dutycycle < 10) {
				dutycycle -= 0.1;
				if(dutycycle < 0) {
					dutycycle = 0;
				}
				} else {
				dutycycle -= 1.0;
			}
		}
		
		// Work out the clock period and pulse width
		clockperiod = freq_to_clock_period(freq);
		pulsewidth = duty_cycle_to_pulse_width(dutycycle, clockperiod);
		
		// Update the PWM registers
		if(pulsewidth > 0) {
			// The compare value is one less than the number of clock cycles in the pulse width
			OCR1B = pulsewidth - 1;
			} else {
			OCR1B = 0;
		}
		// Note that a compare value of 0 results in special behaviour - see page 130 of the
		// datasheet (2018 version)
		
		// Set the maximum count value for timer/counter 1 to be one less than the clockperiod
		OCR1A = clockperiod - 1;
	}
	
}


void joy_stick(void){
	
	uint8_t x_or_y = 0;	/* 0 = x, 1 = y */
	
	for(x_or_y=0; x_or_y< 2; x_or_y++){
		// Set up ADC - AVCC reference, right adjust
	// Input selection doesn't matter yet - we'll swap this around in the while
	// loop below.
	ADMUX = (1<<REFS0);
	// Turn on the ADC (but don't start a conversion yet). Choose a clock
	// divider of 64. (The ADC clock must be somewhere
	// between 50kHz and 200kHz. We will divide our 8MHz clock by 64
	// to give us 125kHz.)
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1);
	/*  a welcome message
	*/
	// Set the ADC mux to choose ADC0 if x_or_y is 0, ADC1 if x_or_y is 1
	if(x_or_y == 0) {
		ADMUX &= ~1;
		} else {
		ADMUX |= 1;
	}
	// Start the ADC conversion
	ADCSRA |= (1<<ADSC);
	
	while(ADCSRA & (1<<ADSC)) {
		; /* Wait until conversion finished */
	}
	value = ADC; // read the value
	//temp = x_or_y ^ 1;
	if(x_or_y == 0) {
		if(value < 515){
			move_base(MOVE_LEFT);
		
		} else if(value > 530) {
			move_base(MOVE_RIGHT);
		}
		
	} else {
		if(value < 500  || value > 520){
			fire_projectile();
		}
	}
	
	}
}
