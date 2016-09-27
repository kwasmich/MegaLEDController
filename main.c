//
//  main.c
//  MegaLEDController
//
//  Created by Michael Kwasnicki on 12.07.16.
//  Copyright © 2016 Michael Kwasnicki. All rights reserved.
//


// UART : AVR306 http://www.atmel.com/devices/ATTINY4313.aspx?tab=documents
// IR : http://www.sbprojects.com/knowledge/ir/nec.php



#include "ir.h"
#include "uart.h"

#include "macros.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <ctype.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <avr/eeprom.h>
#include <avr/pgmspace.h>



#define R_PIN   PD6
#define G_PIN   PB3
#define B_PIN   PD5
#define W_PIN   PD3

#define R_VAL   OCR0A
#define G_VAL   OCR2A
#define B_VAL   OCR0B
#define W_VAL   OCR2B


// MARK: FUSES

//FUSES =
//{
//    .low = (FUSE_CKSEL0 & FUSE_CKSEL2 & FUSE_CKSEL3 & FUSE_SUT0 & FUSE_CKDIV8),
//    .high = HFUSE_DEFAULT,
//    .extended = (unsigned char)~0,
//};



// MARK: State

struct state_s {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
    uint8_t bri;
    uint8_t diy;
    bool on;
} g_state;


// look up tables for integer sequence encoded color palette
static const uint8_t PALETTE_INTENSITY[6] PROGMEM = { 0, 77, 111, 135, 204, 255 };

static const uint8_t PALETTE_F7[32] PROGMEM = {
    0xff, 0xcc, 0xba, 0xff, 0xb4, 0xd2, 0xc6, 0xff, 0xff, 0x95, 0x29, 0xff, 0x05, 0xb9, 0x71, 0xff,
    0xff, 0x0b, 0x21, 0xff, 0x1e, 0x11, 0x22, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd7, 0xff, 0xff, 0xff
};

static const uint8_t PALETTE_FF[32] PROGMEM = {
    0xff, 0xc6, 0xd2, 0xb4, 0xff, 0xba, 0xcc, 0xff, 0xff, 0xd4, 0xb9, 0xd7, 0xff, 0xd6, 0x95, 0xff,
    0xff, 0x22, 0x11, 0x1e, 0xff, 0x21, 0x0b, 0xff, 0xff, 0x29, 0x83, 0x05, 0xff, 0x71, 0x8d, 0xff
};


// some sort of gamma correction
static const uint8_t INTENSITY[32] PROGMEM = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 18, 22, 28, 33, 40, 47, 56, 65, 75, 86, 98, 111, 125, 140, 156, 173, 192, 212, 233, 255
};


static void updateFromState() {
    if (g_state.on) {
        uint8_t bri = (g_state.diy) ? 255 : pgm_read_byte(&INTENSITY[g_state.bri]);

        uint8_t r, g, b, w;
        r = (g_state.r * bri) >> 8;
        r += (g_state.r > 0) ? 1 : 0;
        g = (g_state.g * bri) >> 8;
        g += (g_state.g > 0) ? 1 : 0;
        b = (g_state.b * bri) >> 8;
        b += (g_state.b > 0) ? 1 : 0;
        w = (g_state.w * bri) >> 8;
        w += (g_state.w > 0) ? 1 : 0;
        R_VAL = r;
        G_VAL = g;
        B_VAL = b;
        W_VAL = w;
    } else {
        R_VAL = 0;
        G_VAL = 0;
        B_VAL = 0;
        W_VAL = 0;
    }
}




// MARK: Animation

typedef enum {
    ANIM_FLASH,
    ANIM_JUMP3,
    ANIM_SMOOTH,
    ANIM_JUMP7,

    ANIM_STROBE,
    ANIM_FADE,
    ANIM_FADE3,
    ANIM_FADE7
} AnimType_e;


static const uint8_t anim_flashSteps[] = {7, 0};
static const uint8_t anim_jump3Steps[] = {1, 2, 4};
static const uint8_t anim_smoothSteps[] = {1, 2, 4, 3, 5, 6, 7};
static const uint8_t anim_jump7Steps[] = {1, 2, 3, 4, 5, 6, 7};

static const uint8_t anim_strobeSteps[] = {1, 3, 2, 6, 4, 5};
static const uint8_t anim_fadeSteps[] = {1, 3, 2, 6, 4, 7, 3, 7, 6, 7, 5, 7};
static const uint8_t anim_fade3Steps[] = {0, 1, 0, 2, 0, 4};
static const uint8_t anim_fade7Steps[] = {5, 7, 1, 3, 2, 6, 4};

static bool anim_running = false;
static uint16_t anim_counter = 0;
static uint8_t anim_step = 0;
static uint8_t anim_subStep = 0;

static bool anim_smooth = true;
static uint16_t anim_stepDuration = 16;
static const uint8_t *anim_steps = NULL;
static uint8_t anim_numSteps = 0;

volatile bool anim_timeStep = false;


ISR(TIMER0_OVF_vect) {
    anim_timeStep = true;
}



static void anim_start() {
    anim_running = true;
    BIT_SET(TIMSK0, _BV(TOIE0));     // interrupt on timer0 overflow
}



static void anim_stop() {
    anim_running = false;
    BIT_CLR(TIMSK0, _BV(TOIE0));     // interrupt on timer0 overflow
}



static void anim_play(const AnimType_e in_ANIM) {
    switch (in_ANIM) {
        case ANIM_FLASH:
            anim_steps = anim_flashSteps;
            anim_numSteps = sizeof(anim_flashSteps);
            break;

        case ANIM_JUMP3:
            anim_steps = anim_jump3Steps;
            anim_numSteps = sizeof(anim_jump3Steps);
            break;

        case ANIM_SMOOTH:
            anim_steps = anim_smoothSteps;
            anim_numSteps = sizeof(anim_smoothSteps);
            break;

        case ANIM_JUMP7:
            anim_steps = anim_jump7Steps;
            anim_numSteps = sizeof(anim_jump7Steps);
            break;

        case ANIM_STROBE:
            anim_steps = anim_strobeSteps;
            anim_numSteps = sizeof(anim_strobeSteps);
            break;

        case ANIM_FADE:
            anim_steps = anim_fadeSteps;
            anim_numSteps = sizeof(anim_fadeSteps);
            break;

        case ANIM_FADE3:
            anim_steps = anim_fade3Steps;
            anim_numSteps = sizeof(anim_fade3Steps);
            break;

        case ANIM_FADE7:
            anim_steps = anim_fade7Steps;
            anim_numSteps = sizeof(anim_fade7Steps);
            break;
    }

    switch (in_ANIM) {
        case ANIM_FLASH:
        case ANIM_JUMP3:
        case ANIM_SMOOTH:
        case ANIM_JUMP7:
            anim_stepDuration = 1536;
            anim_smooth = false;
            break;

        case ANIM_STROBE:
        case ANIM_FADE:
        case ANIM_FADE3:
        case ANIM_FADE7:
            anim_stepDuration = 192;
            anim_smooth = true;
            break;
    }

    anim_counter = anim_stepDuration - 1;
    anim_step = 0;
    anim_subStep = 0;
    anim_start();
}



static void anim_tick() {
    if (anim_timeStep && anim_running) {
        anim_timeStep = false;
        anim_counter++;

        if (anim_counter >= anim_stepDuration) {
            anim_counter = 0;

            if (anim_smooth) {
                uint8_t rgbw0 = anim_steps[anim_step];
                uint8_t rgbw1 = anim_steps[(anim_step + 1) % anim_numSteps];

                uint8_t r0 = (rgbw0 & 0x1) ? 32 : 0;
                uint8_t g0 = (rgbw0 & 0x2) ? 32 : 0;
                uint8_t b0 = (rgbw0 & 0x4) ? 32 : 0;
                uint8_t w0 = (rgbw0 & 0x8) ? 32 : 0;

                uint8_t r1 = (rgbw1 & 0x1) ? 32 : 0;
                uint8_t g1 = (rgbw1 & 0x2) ? 32 : 0;
                uint8_t b1 = (rgbw1 & 0x4) ? 32 : 0;
                uint8_t w1 = (rgbw1 & 0x8) ? 32 : 0;

                r0 = (r0 * (31 - anim_subStep)) >> 5;
                g0 = (g0 * (31 - anim_subStep)) >> 5;
                b0 = (b0 * (31 - anim_subStep)) >> 5;
                w0 = (w0 * (31 - anim_subStep)) >> 5;

                r1 = (r1 * anim_subStep) >> 5;
                g1 = (g1 * anim_subStep) >> 5;
                b1 = (b1 * anim_subStep) >> 5;
                w1 = (w1 * anim_subStep) >> 5;

                g_state.r = pgm_read_byte(&INTENSITY[r0 + r1]);
                g_state.g = pgm_read_byte(&INTENSITY[g0 + g1]);
                g_state.b = pgm_read_byte(&INTENSITY[b0 + b1]);
                g_state.w = pgm_read_byte(&INTENSITY[w0 + w1]);

                anim_subStep++;

                if (anim_subStep == 32) {
                    anim_subStep = 0;
                    anim_step++;

                    if (anim_step == anim_numSteps) {
                        anim_step = 0;
                    }
                }
            } else {
                uint8_t rgbw = anim_steps[anim_step];
                uint8_t r = (rgbw & 0x1) ? 255 : 0;
                uint8_t g = (rgbw & 0x2) ? 255 : 0;
                uint8_t b = (rgbw & 0x4) ? 255 : 0;
                uint8_t w = (rgbw & 0x8) ? 255 : 0;

                g_state.r = r;
                g_state.g = g;
                g_state.b = b;
                g_state.w = w;

                anim_step++;

                if (anim_step == anim_numSteps) {
                    anim_step = 0;
                }
            }

            g_state.on = true;
            g_state.bri = 31;
            updateFromState();
        }
    }
}


static void executeCommand00F7(const uint8_t in_COMMAND) {
    anim_stop();

    switch (in_COMMAND) {
        case 0x00:  // Brightness +
            if (g_state.bri < 31) {
                g_state.bri++;
            }

            break;

        case 0x80:  // Brightness -
            if (g_state.bri > 1) {
                g_state.bri--;
            }

            break;

        case 0x40:  // Off
            g_state.on = false;
            break;

        case 0xc0:  // On
            g_state.on = true;
            break;

        case 0xd0:  // Flash
            anim_play(ANIM_JUMP3);
            break;

        case 0xf0:  // Strobe
            anim_play(ANIM_STROBE);
            break;

        case 0xc8:  // Fade
            anim_play(ANIM_FADE);
            break;

        case 0xe8:  // Smooth
            anim_play(ANIM_SMOOTH);
            break;

        default: {
                uint8_t index = in_COMMAND >> 3;
                uint8_t color = pgm_read_byte(&PALETTE_F7[index]);
                uint8_t r = color / 36;
                color -= r * 36;
                uint8_t g = color / 6;
                color -= g * 6;
                uint8_t b = color;
                g_state.r = pgm_read_byte(&PALETTE_INTENSITY[r]);
                g_state.g = pgm_read_byte(&PALETTE_INTENSITY[g]);
                g_state.b = pgm_read_byte(&PALETTE_INTENSITY[b]);
                g_state.w = 0;
            }
    }

    updateFromState();
}



static void diy_store() {
    if (g_state.diy) {
        uint8_t *base = NULL + g_state.diy * 4;
        eeprom_update_byte(base + 0, g_state.r);
        eeprom_update_byte(base + 1, g_state.g);
        eeprom_update_byte(base + 2, g_state.b);
        eeprom_update_byte(base + 3, g_state.w);
    }
}



static void diy_restore() {
    if (g_state.diy) {
        uint8_t *base = NULL + g_state.diy * 4;
        g_state.r = eeprom_read_byte(base + 0);
        g_state.g = eeprom_read_byte(base + 1);
        g_state.b = eeprom_read_byte(base + 2);
        g_state.w = eeprom_read_byte(base + 3);
    }
}



static void executeCommand00FF(const uint8_t in_COMMAND, const bool in_IS_REPEAT) {
    if ((in_COMMAND != 0x82) && (in_COMMAND != 0xe8) && (in_COMMAND != 0xc8)) {
        anim_stop();
    }

    switch (in_COMMAND) {
        case 0x3a:  // Brightness +
            if (g_state.bri < 31) {
                g_state.bri++;
            }

            break;

        case 0xba:  // Brightness -
            if (g_state.bri > 1) {
                g_state.bri--;
            }

            break;

        case 0x82:  // Play/Pause
            if (!in_IS_REPEAT) {
                if (anim_running) {
                    anim_stop();
                } else {
                    anim_start();
                }
            }

            break;

        case 0x02:  // On/Off
            if (!in_IS_REPEAT) {
                g_state.on = !g_state.on;
            }

            break;

        case 0x28: // R+
            if (g_state.diy) {
                if (in_IS_REPEAT && (g_state.r < 255 - 3)) {
                    g_state.r += 3;
                } else if (g_state.r < 255) {
                    g_state.r++;
                }
            }

            break;

        case 0x08: // R-
            if (g_state.diy) {
                if (in_IS_REPEAT && (g_state.r > 3)) {
                    g_state.r -= 3;
                } else if (g_state.r > 0) {
                    g_state.r--;
                }
            }

            break;

        case 0xa8: // G+
            if (g_state.diy) {
                if (in_IS_REPEAT && (g_state.g < 255 - 3)) {
                    g_state.g += 3;
                } else if (g_state.g < 255) {
                    g_state.g++;
                }
            }

            break;

        case 0x88: // G-
            if (g_state.diy) {
                if (in_IS_REPEAT && (g_state.g > 3)) {
                    g_state.g -= 3;
                } else if (g_state.g > 0) {
                    g_state.g--;
                }
            }

            break;

        case 0x68: // B+
            if (g_state.diy) {
                if (in_IS_REPEAT && (g_state.b < 255 - 3)) {
                    g_state.b += 3;
                } else if (g_state.b < 255) {
                    g_state.b++;
                }
            }

            break;

        case 0x48: // B-
            if (g_state.diy) {
                if (in_IS_REPEAT && (g_state.b > 3)) {
                    g_state.b -= 3;
                } else if (g_state.b > 0) {
                    g_state.b--;
                }
            }

            break;

        case 0x30: // DIY 1
            diy_store();
            g_state.diy = 1;
            diy_restore();
            break;

        case 0xb0: // DIY 2
            diy_store();
            g_state.diy = 2;
            diy_restore();
            break;

        case 0x70: // DIY 3
            diy_store();
            g_state.diy = 3;
            diy_restore();
            break;

        case 0x10: // DIY 4
            diy_store();
            g_state.diy = 4;
            diy_restore();
            break;

        case 0x90: // DIY 5
            diy_store();
            g_state.diy = 5;
            diy_restore();
            break;

        case 0x50: // DIY 6
            diy_store();
            g_state.diy = 6;
            diy_restore();
            break;

        case 0xe8:  // Speed +
            if (in_IS_REPEAT && (anim_stepDuration > 10)) {
                anim_stepDuration -= 10;
            } else if (anim_stepDuration > 2) {
                anim_stepDuration -= 2;
            }

            break;

        case 0xc8:  // Speed -
            if (in_IS_REPEAT && (anim_stepDuration < INT16_MAX)) {
                anim_stepDuration += 10;
            } else if (anim_stepDuration < INT16_MAX) {
                anim_stepDuration += 2;
            }

            break;

        //        case 0xf0:  // Auto
        //            diy_store();
        //            g_state.diy = 0;
        //            anim_play(ANIM_AUTO);
        //            break;

        case 0xd0:  // Flash
            diy_store();
            g_state.diy = 0;
            anim_play(ANIM_FLASH);
            break;

        case 0x20:  // Jump 3
            diy_store();
            g_state.diy = 0;
            anim_play(ANIM_JUMP3);
            break;

        case 0xa0:  // Jump 7
            diy_store();
            g_state.diy = 0;
            anim_play(ANIM_JUMP7);
            break;

        case 0x60:  // Fade 3
            diy_store();
            g_state.diy = 0;
            anim_play(ANIM_FADE3);
            break;

        case 0xe0:  // Fade 7
            diy_store();
            g_state.diy = 0;
            anim_play(ANIM_FADE7);
            break;

        default: {
                diy_store();
                g_state.diy = 0;

                uint8_t index = in_COMMAND;

                if ((index bitand 0xf) == 0x2) {
                    index += 0x38;
                }

                index -= 1;
                index = index >> 3;
                uint8_t color = pgm_read_byte(&PALETTE_FF[index]);
                uint8_t r = color / 36;
                color -= r * 36;
                uint8_t g = color / 6;
                color -= g * 6;
                uint8_t b = color;
                g_state.r = pgm_read_byte(&PALETTE_INTENSITY[r]);
                g_state.g = pgm_read_byte(&PALETTE_INTENSITY[g]);
                g_state.b = pgm_read_byte(&PALETTE_INTENSITY[b]);
                g_state.w = 0;
            }
    }

    updateFromState();
}


static void myIRCallback(const uint16_t in_ADDRESS, const uint16_t in_COMMAND, const bool in_IS_REPEAT) {
    const uint8_t cmdL = ~in_COMMAND;
    const uint8_t cmdH = in_COMMAND >> 8;

    //printf("%c 0x%x 0x%x\n", in_IS_REPEAT ? 'R' : '.', in_ADDRESS, in_COMMAND);

    if (cmdL == cmdH) {
        //puts("ok");
        if (in_ADDRESS == 0x00f7) {
            executeCommand00F7(cmdL);
        } else if (in_ADDRESS == 0x00ff) {
            executeCommand00FF(cmdL, in_IS_REPEAT);
        }
    } else {
        //puts("err");
    }
}


static uint8_t uart_sequence[10] = "#ff00cc00"; // #RRGGBBWW
static uint8_t uart_sequenceIndex = 9;


static uint8_t charToNibble(const uint8_t in_CHAR) {
    if (in_CHAR < 0x3A) {
        return in_CHAR - 0x30;
    }

    return (in_CHAR & 0xDF) - 0x37;
}


static void myUARTCallback(const uint8_t in_BYTE) {
    //putchar(in_BYTE);

    if (!isxdigit(in_BYTE) && (in_BYTE != '#')) {
        uart_sequenceIndex = 9;
    }

    if (in_BYTE == '#') {
        uart_sequenceIndex = 0;
    }

    if (uart_sequenceIndex < 9) {
        uart_sequence[uart_sequenceIndex] = in_BYTE;
        uart_sequenceIndex++;

        if (uart_sequenceIndex == 9) {
            //puts("");
            //printf("\nok - %s\n", uart_sequence);

            g_state.r = (charToNibble(uart_sequence[1]) << 4) + charToNibble(uart_sequence[2]);
            g_state.g = (charToNibble(uart_sequence[3]) << 4) + charToNibble(uart_sequence[4]);
            g_state.b = (charToNibble(uart_sequence[5]) << 4) + charToNibble(uart_sequence[6]);
            g_state.w = (charToNibble(uart_sequence[7]) << 4) + charToNibble(uart_sequence[8]);
            g_state.bri = 31;
            g_state.on = true;

            anim_stop();
            updateFromState();
        }
    }
}




// MARK: Setup

static void setup(void) {
    cli();

    // Set up output pins
    DDRB = _BV(W_PIN);
    DDRD = _BV3(R_PIN, G_PIN, B_PIN);

    // Configure timer 0 and 2 for phase correct (8-bit) PWM
    TCCR0A = _BV3(COM0A1, COM0B1, WGM00);
    TCCR0B = _BV(CS01);
    TCCR2A = _BV3(COM2A1, COM2B1, WGM20);
    TCCR2B = _BV(CS21);
    TCNT2 = 0;
    TCNT0 = 0;

    g_state.r = 255;
    g_state.g = 255;
    g_state.b = 255;
    g_state.w = 255;
    g_state.bri = 7;
    g_state.diy = 1;
    g_state.on = true;

    diy_restore();
    updateFromState();

    uart_init(myUARTCallback, 0x71);   //7.352.960 MHz //0x71 //0x53
    ir_init(myIRCallback);

    sei();

    //puts("\n\nready");
}


static void loop() {
    uart_receive();
    ir_receive();
    anim_tick();
}


int main(void) {
    setup();

    while (true) {
        loop();
    }
}
