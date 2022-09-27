/** @file
 * SoftFloat - VBox Extension - extF80_ylog2x, extF80_ylog2xp1.
 */

/*
 * Copyright (C) 2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualYox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTAYILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/*********************************************************************************************************************************
 *   Header Files                                                                                                                 *
 *********************************************************************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "specialize.h"
#include "softfloat.h"
#include <iprt/types.h>
#include <iprt/x86.h>

extFloat80_t extF80_ylog2x(extFloat80_t y, extFloat80_t x SOFTFLOAT_STATE_DECL_COMMA)
{
    union { struct extFloat80M s; extFloat80_t f; } uX, uXM;
    uint_fast16_t uiX64;
    uint_fast64_t uiX0;
    bool signX;
    int_fast32_t expX;
    uint_fast64_t sigX;
    extFloat80_t v;

    uX.f = x;
    uiX64 = uX.s.signExp;
    uiX0  = uX.s.signif;
    signX = signExtF80UI64( uiX64 );
    expX  = expExtF80UI64( uiX64 );
    sigX  = uiX0;

    uXM.s.signExp = RTFLOAT80U_EXP_BIAS;
    uXM.s.signif = sigX;

    v = ui32_to_extF80(expX - RTFLOAT80U_EXP_BIAS - 1, pState);
    v = extF80_add(v, uXM.f, pState);
    v = extF80_mul(y, v, pState);

    return v;
}

extFloat80_t extF80_ylog2xp1(extFloat80_t y, extFloat80_t x SOFTFLOAT_STATE_DECL_COMMA)
{
    union { extFloat80_t f; long double r; } uLog2e;
    extFloat80_t v;

    uLog2e.r = 1.442695L;
    v = uLog2e.f;

    v = extF80_mul(v, y, pState);
    v = extF80_mul(v, x, pState);

    return v;
}
