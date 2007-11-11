/* $Id$ */
/** @file
 * DBGC Testcase - Command Parser.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-kStuff-spam@anduin.net>
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/dbg.h>
#include "../DBGCInternal.h"

#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/initterm.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Global error counter. */
static unsigned g_cErrors = 0;



int main()
{
    /*
     * Init.
     */
    RTR3Init();
    RTPrintf("tstDBGCParser: TESTING...\n");

    /*
     *
     */


    /*
     * Summary
     */
    if (!g_cErrors)
        RTPrintf("tstDBGCParser: SUCCESS\n");
    else
        RTPrintf("tstDBGCParser: FAILURE - %d errors\n", g_cErrors);
    return g_cErrors != 0;
}
