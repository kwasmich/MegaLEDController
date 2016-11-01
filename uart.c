//
//  uart.c
//  MegaLEDController
//
//  Created by Michael Kwasnicki on 19.07.16.
//  Copyright © 2016 Michael Kwasnicki. All rights reserved.
//

#include "uart.h"

#include "macros.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdio.h>
#include <util/delay.h>
#include <util/setbaud.h>


volatile static uint8_t uart_received_data;
volatile static bool uart_received_flag;

static uart_callbackFN *uart_callback;


ISR(USART_RX_vect) {
    uart_received_data = UDR0;
    uart_received_flag = true;
}


void uart_putchar(char c) {
    loop_until_bit_is_set(UCSR0A, UDRE0);   // wait for transmit buffer to be ready
    UDR0 = c;
}


#ifdef UART_STDOUT
static void uart_putc(char c, FILE *stream) {
    if (c == '\n') {
        uart_putc('\r', stream);
    }

    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = c;
}
#endif


uint8_t uart_getchar(void) {
    loop_until_bit_is_set(UCSR0A, RXC0);    // wait for data to be received
    return UDR0;
}


void uart_receive(void) {
    if (uart_received_flag) {
        uart_received_flag = false;
        uart_callback(uart_received_data);
    }
}


void uart_init(uart_callbackFN *const in_UART_CALLBACK, const uint8_t in_OSCCAL) {
    // calibrate clock to match baud rate = 115.2kHz * 16 * x to match the system Clock ~7.3728MHz
    //OSCCAL = 0x54; //7.352.960

    while (OSCCAL > in_OSCCAL) {
        OSCCAL--;
        _delay_ms(10);
    }

    while (OSCCAL < in_OSCCAL) {
        OSCCAL++;
        _delay_ms(10);
    }

    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;

#if USE_2X
    UCSR0A = _BV(U2X0);
#else
    UCSR0A = 0;
#endif

    UCSR0B = _BV3(RXCIE0, RXEN0, TXEN0);
    UCSR0C = _BV2(UCSZ01, UCSZ00); // 8N1

#ifdef UART_STDOUT
    static FILE uart_stdout = FDEV_SETUP_STREAM(uart_putc, NULL, _FDEV_SETUP_WRITE);
    stdout = &uart_stdout;
#endif

    //printf("%ld %ld\n", UBRRH_VALUE, UBRRL_VALUE);

    uart_callback = in_UART_CALLBACK;
}
