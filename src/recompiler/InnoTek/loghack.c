/** @file
 *
 * Hack for fprintf(logfile, ...) and printf to tee that onto the debugger.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP   LOG_GROUP_REM_PRINTF
#include <VBox/log.h>

#include <stdio.h>
#include <stdarg.h>


/**
 * Wrapper for fprintf().
 */
int hacked_fprintf(FILE *pFile, const char *pszFormat, ...)
{
    va_list     args;
    int         rc;
    int         rc2;
    static char szBuffer[16384];

    va_start(args, pszFormat);
    rc = vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, args);
    va_end(args);

    if (rc >= sizeof(szBuffer))
        rc = sizeof(szBuffer) - 1;
    if (rc <= 0 || (szBuffer[rc - 1] != '\r' && szBuffer[rc - 1] != '\n'))
        Log(("VBoxREM: %s\n", szBuffer));
    else
        Log(("VBoxREM: %s", szBuffer));

    rc2 = rc;
#ifndef DEBUG_sandervl
    if (pFile && pFile != stderr && pFile != stdout && pFile != stdin)
    {
        va_start(args, pszFormat);
        rc2 = vfprintf(pFile, pszFormat, args);
        va_end(args);
    }
#endif
    return rc2;
}

/**
 * Wrapper for printf().
 */
int hacked_printf(const char *pszFormat, ...)
{
    va_list     args;
    int         rc;
    static char szBuffer[16384];

    va_start(args, pszFormat);
    rc = vsnprintf(szBuffer, sizeof(szBuffer), pszFormat, args);
    va_end(args);

    if (rc >= sizeof(szBuffer))
        rc = sizeof(szBuffer) - 1;
    if (rc <= 0 || (szBuffer[rc - 1] != '\r' && szBuffer[rc - 1] != '\n'))
        Log(("VBoxREM: %s\n", szBuffer));
    else
        Log(("VBoxREM: %s", szBuffer));

    return rc;
}

