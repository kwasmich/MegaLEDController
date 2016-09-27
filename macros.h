//
//  macros.h
//  MegaLEDController
//
//  Created by Michael Kwasnicki on 19.07.16.
//  Copyright © 2016 Michael Kwasnicki. All rights reserved.
//

#ifndef macros_h
#define macros_h

#include <iso646.h>


#define _BV2( A, B ) ( _BV( A ) bitor _BV( B ) )
#define _BV3( A, B, C ) ( _BV2( A, B ) bitor _BV( C ) )
#define _BV4( A, B, C, D ) ( _BV2( A, B ) bitor _BV2( C, D ) )
#define _BV5( A, B, C, D, E ) ( _BV2( A, B ) bitor _BV3( C, D, E ) )
#define BIT_SET( PORT, BIT_FIELD ) PORT |=  BIT_FIELD
#define BIT_CLR( PORT, BIT_FIELD ) PORT &= ~BIT_FIELD
#define BIT_TGL( PORT, BIT_FIELD ) PORT ^=  BIT_FIELD


#endif /* macros_h */
