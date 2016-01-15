/* Name: modem.c
 *
 * Audio modem for Attiny85 & other AVR chips with modifications
 *
 * Author: Jari Tulilahti
 * Copyright: 2014 Rakettitiede Oy
 * License: LGPLv3, see COPYING, and COPYING.LESSER -files for more info
 */

#include <avr/io.h>
#include <stdlib.h>
#include "modem.h"

/* Ring buffer global variables */
static volatile uint8_t modem_buffer_head = 0, modem_buffer_tail = 0;
static volatile uint8_t modem_buffer[MODEM_BUFFER_SIZE];

Modem modem;

/*
 * Returns number of available bytes in ringbuffer or 0 if empty
 */
uint8_t Modem::buffer_available() {
	return modem_buffer_head - modem_buffer_tail;
}

/*
 * Store 1 byte in ringbuffer
 */
static inline void modem_buffer_put(const uint8_t c) {
	if (modem.buffer_available() != MODEM_BUFFER_SIZE) {
		modem_buffer[modem_buffer_head++ % MODEM_BUFFER_SIZE] = c;
	} 
}

/*
 * Fetch 1 byte from ringbuffer
 */
uint8_t Modem::buffer_get() {
	uint8_t b = 0;
	if (buffer_available() != 0) {
		b = modem_buffer[modem_buffer_tail++ % MODEM_BUFFER_SIZE];
	}
	return b;
}

/*
 * Pin Change Interrupt Vector. This is The Modem.
 */
ISR(PCINT3_vect) {
	/* Static variables instead of globals to keep scope inside ISR */
	static uint8_t modem_bit = 0;
	static uint8_t modem_bitlen = 0;
	static uint8_t modem_byte = 0;

	/* Read & Zero Timer/Counter 1 value */
	uint8_t modem_pulselen = MODEM_TIMER;
	MODEM_TIMER = 0;

	/*
	 * Check if we received Start/Sync -pulse.
	 * Calculate bit signal length middle point from pulse.
	 * Return from ISR immediately.
	 */
	if (modem_pulselen > MODEM_SYNC_LEN) {
		modem_bitlen = (modem_pulselen >> 2);
		modem_bit = 0;
		return;
	}

	/*
	 * Shift byte and set high bit according to the pulse length.
	 * Long pulse = 1, Short pulse = 0
	 */
	modem_byte = (modem_byte >> 1) | (modem_pulselen < modem_bitlen ? 0x00 : 0x80);

	/* Check if we received complete byte and store it in ring buffer */
	if (!(++modem_bit % 0x08)) {
		modem_buffer_put(modem_byte);
	}
}

/*
 * Start the modem by enabling Pin Change Interrupts & Timer
 */
void Modem::enable()  {
	/* Enable R1 */
	DDRA  |= _BV(PA3);
	PORTA |= _BV(PA3);

	/* Modem pin as input */
	MODEM_DDR &= ~_BV(MODEM_PIN);

	/* Enable Pin Change Interrupts and PCINT for MODEM_PIN */
	MODEM_PCMSK |= _BV(MODEM_PCINT);
	PCICR |= _BV(MODEM_PCIE);

	/* Timer: TCCR1: CS10 and CS11 bits: 8MHz clock with Prescaler 64 = 125kHz timer clock */
	TCCR1B = _BV(CS11) | _BV(CS10);
}

void Modem::disable()
{
	PORTA &= ~_BV(PA3);
	DDRA  &= ~_BV(PA3);
}
