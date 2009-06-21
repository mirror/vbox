/* $Id$ */
/** @file
 * IPRT - Debug Misc.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/dbg.h>
#include <iprt/mem.h>



/**
 * Allocate a new symbol structure.
 *
 * @returns Pointer to a new structure on success, NULL on failure.
 */
RTDECL(PRTDBGSYMBOL) RTDbgSymbolAlloc(void)
{
    return (PRTDBGSYMBOL)RTMemAllocZ(sizeof(RTDBGSYMBOL));
}


/**
 * Duplicates a symbol structure.
 *
 * @returns Pointer to duplicate on success, NULL on failure.
 *
 * @param   pSymbol         The symbol to duplicate.
 */
RTDECL(PRTDBGSYMBOL) RTDbgSymbolDup(PCRTDBGSYMBOL pSymbol)
{
    return (PRTDBGSYMBOL)RTMemDup(pSymbol, sizeof(*pSymbol));
}


/**
 * Free a symbol structure previously allocated by a RTDbg method.
 *
 * @param   pSymbol         The symbol to free. NULL is ignored.
 */
RTDECL(void) RTDbgSymbolFree(PRTDBGSYMBOL pSymbol)
{
    RTMemFree(pSymbol);
}

