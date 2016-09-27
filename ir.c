//
//  ir.c
//  MegaLEDController
//
//  Created by Michael Kwasnicki on 19.07.16.
//  Copyright © 2016 Michael Kwasnicki. All rights reserved.
//


#include "ir.h"

#include "macros.h"

#include <avr/interrupt.h>
#include <avr/io.h>



#define IR_RECV_SPACE 255
//#define IR_RECV_REPEAT  113 //116  // middle between normal and repeat = ((4500µs + 2250µs) / 2 + 560µs) * F_CPU / (PRESCALER * 1000000)
//#define IR_RECV_THRESHOLD 48      // middle between 0 and 1 = ((1125µs + 2250µs) / 2) * F_CPU / (PRESCALER * 1000000)
static const uint8_t IR_RECV_REPEAT = ((5060 + 2810) * (F_CPU >> 8)) / 2000000;
static const uint8_t IR_RECV_THRESHOLD = ((2250 + 1125) * (F_CPU >> 8)) / 2000000;

volatile static uint16_t ir_address = 0;
volatile static uint16_t ir_command = 0;
volatile static uint8_t ir_edge = 0;
volatile static bool ir_validStart = false;
volatile static bool ir_repeat = false;
volatile static bool ir_receiving = false;
volatile static bool ir_finished = false;

static ir_callbackFN *ir_callback;


ISR(PCINT1_vect) {
    if (bit_is_set(PINC, IR_PIN)) {
        TCCR1B = 0; // stop timer

        if (ir_edge == 0) {
            ir_validStart = (TCNT1L == IR_RECV_SPACE);
        } else if (ir_validStart) {
            if (ir_edge == 1) {
                ir_repeat = (TCNT1L < IR_RECV_REPEAT);

                if (!ir_repeat) {
                    ir_address = 0;
                    ir_command = 0;
                }
            } else if (!ir_repeat) {
                if (ir_edge < 18) {
                    ir_address <<= 1;

                    if (TCNT1L > IR_RECV_THRESHOLD) {
                        ir_address |= 0x01;
                    }
                } else if (ir_edge < 34) {
                    ir_command <<= 1;

                    if (TCNT1L > IR_RECV_THRESHOLD) {
                        ir_command |= 0x01;
                    }
                }
            }
        }

        ir_receiving = true;
        ir_edge++;
        TCNT1H = 0;
        TCNT1L = 1;         // start with 1 as starting with 0 immediately fires overflow
        TCCR1B = _BV(CS12); // start timer
    }
}


ISR(TIMER1_OVF_vect) {
    TCCR1B = 0; // stop timer
    TCNT1L = 255;

    if (ir_receiving) {
        ir_receiving = false;

        if (ir_repeat && (ir_edge == 2)) {
            ir_finished = true;
        } else if (!ir_repeat && (ir_edge == 34)) {
            ir_finished = true;
        }

        ir_edge = 0;
    }
}


void ir_receive() {
    if (ir_finished) {
        ir_finished = false;
        ir_callback(ir_address, ir_command, ir_repeat);
    }
}


void ir_init(ir_callbackFN *const in_IR_CALLBACK) {
    BIT_SET(PORTC, _BV(IR_PIN));    // interrupt pin hi
    BIT_SET(PCMSK1, _BV(IR_INT));   // set PC0 to trigger interrupt
    //BIT_SET(PCIFR, _BV(PCIF1));     // Clear interrupt signal
    BIT_SET(PCICR, _BV(PCIE1));     // Pin Change Interrupt Enabled

    TCCR1A = _BV2(WGM12, WGM10);    // fast 8-bit PWM
    TCCR1B = _BV(CS12);             // start timer clk/256
    //BIT_SET(TIFR1, _BV(TOV1));      // Clear timer overflow signal
    BIT_SET(TIMSK1, _BV(TOIE1));    // interrupt on timer0 overflow

    ir_callback = in_IR_CALLBACK;
}
