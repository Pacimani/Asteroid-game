/*
 * score.c
 *
 * Written by Peter Sutton
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#define F_CPU 8000000UL	// 8MHz
#include <util/delay.h>
#include <stdint.h>
#include "game.h"
#include "score.h"
#include "terminalio.h"
#include "timer0.h"

volatile uint32_t score;

void init_score(void) {
	score = 0;
	clear_terminal();
	move_cursor(10,10);

	printf("Score : %" PRIu32 , get_score());
	move_cursor(10,12);
	printf("Lives : %d" , (int8_t)get_lives());
	
}


void add_to_score(uint16_t value) {
	
	if(value == 5){
		PORTD |= 0b01111100;
	}
	
	score += value;
	clear_terminal();
	move_cursor(10,10);
	if(value < 9){
		printf("Score : %" PRIu32, get_score());
		move_cursor(10,12);
		printf("Lives : %d" , (int8_t)get_lives());
		
	} else if(value > 10 && value < 100 ) {
		printf("Score: %" PRIu32 " ", get_score());
		move_cursor(10,12);
		printf("Lives : %d"  , (int8_t)get_lives());
		
	} else {
		printf("Score: %" PRIu32 "  ", get_score());
		move_cursor(10,12);
		printf("Lives : %d"  , (int8_t)get_lives());
	}
	
	
}

uint32_t get_score(void) {
	return score;
}



