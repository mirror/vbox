/* $Id$ */
/** @file
 * 16-bit DOS sleep program.
 *
 * Build: wcl -I%WATCOM%\h\win -l=dos -k4096 -fm -W4 DosSleep.c
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>



int main(int argc, char **argv)
{
    int iExit;

    if (argc == 2)
    {
        int cSeconds = atoi(argv[1]);
        sleep(cSeconds);
        iExit = 0;
    }
    else
    {
        fprintf(stderr, "syntax error: only expected a number of seconds\n");
        iExit = 4;
    }

    return iExit;
}

