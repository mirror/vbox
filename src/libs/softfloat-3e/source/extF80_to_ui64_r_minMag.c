
/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of
California.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include <stdbool.h>
#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "specialize.h"
#include "softfloat.h"

uint_fast64_t extF80_to_ui64_r_minMag( extFloat80_t a, bool exact SOFTFLOAT_STATE_DECL_COMMA )
{
    union { struct extFloat80M s; extFloat80_t f; } uA;
    uint_fast16_t uiA64;
    int_fast32_t exp;
    uint_fast64_t sig;
    int_fast32_t shiftDist;
    bool sign;
    uint_fast64_t z;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uA.f = a;
    uiA64 = uA.s.signExp;
    exp = expExtF80UI64( uiA64 );
    sig = uA.s.signif;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    shiftDist = 0x403E - exp;
    if ( 64 <= shiftDist ) {
        if ( exact && (exp | sig) ) {
            softfloat_exceptionFlags |= softfloat_flag_inexact;
        }
        return 0;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    sign = signExtF80UI64( uiA64 );
    if ( sign || (shiftDist < 0) ) {
        softfloat_raiseFlags( softfloat_flag_invalid SOFTFLOAT_STATE_ARG_COMMA );
        return
            (exp == 0x7FFF) && (sig & UINT64_C( 0x7FFFFFFFFFFFFFFF ))
                ? ui64_fromNaN
                : sign ? ui64_fromNegOverflow : ui64_fromPosOverflow;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    z = sig>>shiftDist;
    if ( exact && (z<<shiftDist != sig) ) {
        softfloat_exceptionFlags |= softfloat_flag_inexact;
    }
    return z;

}

