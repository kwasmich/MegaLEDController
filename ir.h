//
//  ir.h
//  MegaLEDController
//
//  Created by Michael Kwasnicki on 19.07.16.
//  Copyright © 2016 Michael Kwasnicki. All rights reserved.
//

#ifndef ir_h
#define ir_h


#include <stdbool.h>
#include <stdint.h>


#define IR_INT  PCINT8
#define IR_PIN  PC0

typedef void ir_callbackFN(const uint16_t in_ADDRESS, const uint16_t in_COMMAND, const bool in_IS_REPEAT);

void ir_init(ir_callbackFN *const in_IR_CALLBACK);
void ir_receive();


#endif /* ir_h */
