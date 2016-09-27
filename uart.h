//
//  uart.h
//  MegaLEDController
//
//  Created by Michael Kwasnicki on 19.07.16.
//  Copyright © 2016 Michael Kwasnicki. All rights reserved.
//

#ifndef uart_h
#define uart_h


#include <stdint.h>


#ifndef BAUD
#define BAUD 9600
#endif

//#define UART_STDOUT


typedef void uart_callbackFN(const uint8_t in_BYTE);


void uart_init(uart_callbackFN *const in_UART_CALLBACK, const uint8_t in_OSCCAL);
void uart_receive();

void uart_putchar(char c);
uint8_t uart_getchar(void);


#endif /* uart_h */
