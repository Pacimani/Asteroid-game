/*
 * project.c
 *
 * Main file
 *
 * Author: Peter Sutton. Modified by <YOUR NAME HERE>
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <avr/eeprom.h>



#include "ledmatrix.h"
#include "scrolling_char_display.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "score.h"
#include "timer0.h"
#include "game.h"


#define F_CPU 8000000L
#include <util/delay.h>

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void splash_screen(void);
void new_game(void);
void play_game(void);
void handle_game_over(void);

// ASCII code for Escape character
#define ESCAPE_CHAR 27

volatile int speed = 500;
volatile uint32_t  asteroid_speed = 1000;
volatile int saveScore;
volatile char playerName[12];
volatile int8_t paused = 0; // 1 = paused 
uint32_t timePaused;
volatile uint32_t current_time, last_move_time, pause_time, asteroid_move_time;
//a function which outputs the direction of joystic
void serial_check_pause(void);

/////////////////////////////// main //////////////////////////////////
int main(void) {
	// Setup hardware and call backs. This will turn on
	// interrupts.
	asteroid_speed = 1200;
	DDRA = 0b01111100;
	DDRD = 0b11111000;
	// Make pin OC1B be an output (port D, pin 4)
	
	initialise_hardware();
	//eeprom_read_word(0);
	// Show the splash screen message. Returns when display
	// is complete
	splash_screen();
	while(1) {
		new_game();
		play_game();
		handle_game_over();
		
		}
	}
		


void initialise_hardware(void) {
	ledmatrix_setup();
	init_button_interrupts();
	// Setup serial port for 19200 baud communication with no echo
	// of incoming characters
	init_serial_stdio(19200,0);
	
	init_timer0();
	
	// Turn on global interrupts
	
	sei();
}

void splash_screen(void) {
	// Clear terminal screen and output a message
	clear_terminal();
	move_cursor(10,10);
	printf_P(PSTR("Asteroids"));
	move_cursor(10,12);
	printf_P(PSTR("CSSE2010/7201 project by Pacifique Rukikza; S4521717"));
	
	// Output the scrolling message to the LED matrix
	// and wait for a push button to be pushed.
	ledmatrix_clear();
	while(1) {
		set_scrolling_display_text("45217171", COLOUR_GREEN);
		// Scroll the message until it has scrolled off the 
		// display or a button is pushed
		while(scroll_display()) {
			_delay_ms(150);
			if(button_pushed() != NO_BUTTON_PUSHED) {
				return;
			}
		}
	}
}

void new_game(void) {
	// Initialise the game and display
	initialise_game();

	// Clear the serial terminal
	clear_terminal();
	
	// Initialise the score
	init_score();
	// Clear a button push or serial input if any are waiting
	// (The cast to void means the return value is ignored.)
	(void)button_pushed();
	clear_serial_input_buffer();
}

void play_game(void) {
	
	int8_t button;
	char serial_input, escape_sequence_char;
	uint8_t characters_into_escape_sequence = 0;
	
	
	// Get the current time and remember this as the last time the projectiles
	// were moved.
	current_time = get_current_time();
	last_move_time = current_time;
	asteroid_move_time = current_time;
	
	if(is_game_over()){
		speed = 500;
		asteroid_speed = 1000;
		//eeprom_write_word(0, ("Pacifique d%", get_score()));
		//ledmatrix_clear();
		
		
		if(button_pushed()){
			
			PORTC |= 0X7C;
			init_score();
			init_lives();
			paused = 0;
			DDRD = 0;
			game_over(0);
			PORTD &=  ~( 1<<6 |1 << 7);
			
		}
	}
	
	// We play the game until it's over
	while(!is_game_over()) {
		DDRD = (1<<4 | 1<<5 | 1<<6);
		// Check for input - which could be a button push or serial input.
		// Serial input may be part of an escape sequence, e.g. ESC [ D
		// is a left cursor key press. At most one of the following three
		// variables will be set to a value other than -1 if input is available.
		// (We don't initalise button to -1 since button_pushed() will return -1
		// if no button pushes are waiting to be returned.)
		// Button pushes take priority serial input. If there are both then
		// we'll retrieve the serial input the next time through this loop
		serial_input = -1;
		escape_sequence_char = -1;
		button = button_pushed();
		current_time = get_current_time();
		
		if(button == NO_BUTTON_PUSHED) {
			// No push button was pushed, see if there is any serial input
			if(serial_input_available()) {
				// Serial data was available - read the data from standard input
				serial_input = fgetc(stdin);
				// Check if the character is part of an escape sequence
				if(characters_into_escape_sequence == 0 && serial_input == ESCAPE_CHAR) {
					// We've hit the first character in an escape sequence (escape)
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
					} else if(characters_into_escape_sequence == 1 && serial_input == '[') {
					// We've hit the second character in an escape sequence
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
					} else if(characters_into_escape_sequence == 2) {
					// Third (and last) character in the escape sequence
					escape_sequence_char = serial_input;
					serial_input = -1;  // Don't further process this character - we
					// deal with it as part of the escape sequence
					characters_into_escape_sequence = 0;
					} else {
					// Character was not part of an escape sequence (or we received
					// an invalid second character in the sequence). We'll process
					// the data in the serial_input variable.
					characters_into_escape_sequence = 0;
				}
			}
		}
		
		// Process the input.
		if(button==3 || escape_sequence_char=='D' || serial_input=='L' || serial_input=='l') {
			// Button 3 pressed OR left cursor key escape sequence completed OR
			// letter L (lowercase or uppercase) pressed - attempt to move left
			move_base(MOVE_LEFT);
			} else if(button==2 || escape_sequence_char=='A' || serial_input==' ') {
			// Button 2 pressed or up cursor key escape sequence completed OR
			// space bar pressed - attempt to fire projectile
			fire_projectile();
			} else if(button==1 || escape_sequence_char=='B') {
			// Button 1 pressed OR down cursor key escape sequence completed
			// Ignore at present
			} else if(button==0 || escape_sequence_char=='C' || serial_input=='R' || serial_input=='r' ) {
			// Button 0 pressed OR right cursor key escape sequence completed OR
			// letter R (lowercase or uppercase) pressed - attempt to move right
			move_base(MOVE_RIGHT);
			} else if(serial_input == 'p' || serial_input == 'P' ) {
			// Unimplemented feature - pause/unpause the game until 'p' or 'P' is
			// pressed again
			if(!paused){
				// Pausing
				paused = 1;
				pause_time = get_current_time();
				DDRD = ~(1 << 4);
				
			}
		}
		// else - invalid input or we're part way through an escape sequence -
		// do nothing
		while(paused){
			serial_input = fgetc(stdin);
			if(serial_input == 'p' || serial_input == 'P' || button==1 || escape_sequence_char=='B'){
				timePaused = get_current_time() - pause_time;
				current_time = get_current_time() + timePaused;
				paused = 0;
				DDRD = (1 << 4);
				
			}
			
		}
		
		if(!is_game_over() && current_time >= last_move_time + ((uint32_t) speed)) {
			// 500ms (0.5 second) has passed since the last time we moved
			// the projectiles - move them - and keep track of the time we
			// moved them
			advance_projectiles();
	
			game_playing();
			joy_stick();
			
			//crease the speed of the game as the score increases
			if(get_score() >= 10) {
				speed =  500 - get_score();
			}
			
			last_move_time = current_time;
		} 
		
		if(!is_game_over() && current_time >= asteroid_move_time + asteroid_speed) {
		// 500ms (0.5 second) has passed since the last time we moved
		// the projectiles - move them - and keep track of the time we
		// moved them
		//we descend the asteroids from top to bottom
		advance_asteroids();
		game_playing();
		joy_stick();
		
		//crease the speed of the game as the score increases
		if(get_score() >= 10) {
			asteroid_speed =  1000 - 2*(get_score());
		}
		
		asteroid_move_time = current_time;
		
		
	}
		
	}

	// We get here if the game is over.
}



void handle_game_over() {
	move_cursor(10,14);
	printf_P(PSTR("GAME OVER"));
	move_cursor(10,15);
	printf_P(PSTR("Press a button to start again"));
	game_visual();
	
	while(button_pushed() == NO_BUTTON_PUSHED) {
		set_scrolling_display_text("GAME OVER", COLOUR_RED);
		// Scroll the message until it has scrolled off the
		// display or a button is pushed
		while(scroll_display()) {
			_delay_ms(150);
			if(button_pushed() != NO_BUTTON_PUSHED) {
				return;
			}
		}
	}

}
