/* $Id$ */
/** @file
 * innotek Portable Runtime - Setup Sanity Checks, C and C++.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/assert.h>

/*
 * Check that the IN_[RING3|RING0|GC] and [|R3_|R0_|GC_]ARCH_BITS
 * match up correctly.
 *
 * IPRT assumes r0 and r3 to has the same bit count.
 */

#if defined(IN_RING3) && ARCH_BITS != R3_ARCH_BITS
# error "defined(IN_RING3) && ARCH_BITS != R3_ARCH_BITS"
#endif
#if defined(IN_RING0) && ARCH_BITS != R0_ARCH_BITS
# error "defined(IN_RING0) && ARCH_BITS != R0_ARCH_BITS"
#endif
#if defined(IN_GC) && ARCH_BITS != GC_ARCH_BITS
# error "defined(IN_GC) && ARCH_BITS != GC_ARCH_BITS"
#endif
#if (defined(IN_RING0) || defined(IN_RING3)) && HC_ARCH_BITS != ARCH_BITS
# error "(defined(IN_RING0) || defined(IN_RING3)) && HC_ARCH_BITS != ARCH_BITS"
#endif
#if defined(IN_GC) && GC_ARCH_BITS != ARCH_BITS
# error "defined(IN_GC) && GC_ARCH_BITS != ARCH_BITS"
#endif


/*
 * Check basic host (hc/r0/r3) types.
 */
#if HC_ARCH_BITS == 64

AssertCompileSize(RTHCPTR, 8);
AssertCompileSize(RTHCINT, 4);
AssertCompileSize(RTHCUINT, 4);
AssertCompileSize(RTHCINTPTR, 8);
AssertCompileSize(RTHCUINTPTR, 8);
/*AssertCompileSize(RTHCINTREG, 8);*/
AssertCompileSize(RTHCUINTREG, 8);
AssertCompileSize(RTR0PTR, 8);
/*AssertCompileSize(RTR0INT, 4);*/
/*AssertCompileSize(RTR0UINT, 4);*/
AssertCompileSize(RTR0INTPTR, 8);
AssertCompileSize(RTR0UINTPTR, 8);
/*AssertCompileSize(RTR3PTR, 8);*/
/*AssertCompileSize(RTR3INT, 4);*/
/*AssertCompileSize(RTR3UINT, 4);*/
AssertCompileSize(RTR3INTPTR, 8);
AssertCompileSize(RTR3UINTPTR, 8);
AssertCompileSize(RTUINTPTR, 8);

# if defined(IN_RING3) || defined(IN_RING0)
/*AssertCompileSize(RTCCINTREG, 8);*/
AssertCompileSize(RTCCUINTREG, 8);
# endif

#else

AssertCompileSize(RTHCPTR, 4);
AssertCompileSize(RTHCINT, 4);
AssertCompileSize(RTHCUINT, 4);
/*AssertCompileSize(RTHCINTPTR, 4);*/
AssertCompileSize(RTHCUINTPTR, 4);
AssertCompileSize(RTR0PTR, 4);
/*AssertCompileSize(RTR0INT, 4);*/
/*AssertCompileSize(RTR0UINT, 4);*/
AssertCompileSize(RTR0INTPTR, 4);
AssertCompileSize(RTR0UINTPTR, 4);
/*AssertCompileSize(RTR3PTR, 4);*/
/*AssertCompileSize(RTR3INT, 4);*/
/*AssertCompileSize(RTR3UINT, 4);*/
AssertCompileSize(RTR3INTPTR, 4);
AssertCompileSize(RTR3UINTPTR, 4);
# if GC_ARCH_BITS == 64
AssertCompileSize(RTUINTPTR, 8);
# else
AssertCompileSize(RTUINTPTR, 4);
# endif

# if defined(IN_RING3) || defined(IN_RING0)
/*AssertCompileSize(RTCCINTREG, 4);*/
AssertCompileSize(RTCCUINTREG, 4);
# endif

#endif

AssertCompileSize(RTHCPHYS, 8);


/*
 * Check basic guest context types.
 */
#if GC_ARCH_BITS == 64

AssertCompileSize(RTGCINT, 4);
AssertCompileSize(RTGCUINT, 4);
AssertCompileSize(RTGCINTPTR, 8);
AssertCompileSize(RTGCUINTPTR, 8);
/*AssertCompileSize(RTGCINTREG, 8);*/
AssertCompileSize(RTGCUINTREG, 8);

# ifdef IN_GC
/*AssertCompileSize(RTCCINTREG, 8);*/
AssertCompileSize(RTCCUINTREG, 8);
# endif

#else

AssertCompileSize(RTGCINT, 4);
AssertCompileSize(RTGCUINT, 4);
AssertCompileSize(RTGCINTPTR, 4);
AssertCompileSize(RTGCUINTPTR, 4);
/*AssertCompileSize(RTGCINTREG, 4);*/
AssertCompileSize(RTGCUINTREG, 4);

# ifdef IN_GC
/*AssertCompileSize(RTCCINTREG, 4);*/
AssertCompileSize(RTCCUINTREG, 4);
# endif

#endif

AssertCompileSize(RTGCPHYS64, 8);
AssertCompileSize(RTGCPHYS32, 4);
AssertCompileSize(RTGCPHYS, 8);


/*
 * Check basic current context types.
 */
#if ARCH_BITS == 64

AssertCompileSize(void *, 8);

#else

AssertCompileSize(void *, 4);

#endif
