/** @file
 *
 * vboxvfs -- VirtualBox Guest Additions for Linux:
 * Module logging support
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#if 0
  #include <linux/version.h>
  #include <linux/kernel.h>
#endif

#include "the-linux-kernel.h"

#include <iprt/cdefs.h>
#include "vfsmod.h"

/** @todo r=bird: Unless you want to prefix the messages with a module name or
 * something, use the runtime implementation of these assert workers. If there is
 * no runtime implementation add it. (Yes, I know this has been the normal way
 * to do it, but it's not so anylonger.) */

/* Runtime assert implementation for Linux ring 0 */
RTDECL(void) AssertMsg1(
    const char *pszExpr,
    unsigned uLine,
    const char *pszFile,
    const char *pszFunction
    )
{
    printk (KERN_ALERT "!!Assertion Failed!!\n"
            "Expression: %s\n"
            "Location  : %s(%d) %s\n",
            pszExpr, pszFile, uLine, pszFunction);
}

/* Runtime assert implementation for Linux ring 0 */
RTDECL(void) AssertMsg2(const char *pszFormat, ...)
{
    va_list ap;
    char    msg[256];

    va_start(ap, pszFormat);
    vsnprintf(msg, sizeof(msg) - 1, pszFormat, ap);
    msg[sizeof(msg) - 1] = '\0';
    printk ("%s", msg);
    va_end(ap);
}

RTDECL(size_t) RTLogBackdoorPrintf (const char *pszFormat, ...)
{
    va_list ap;
    char    msg[256];
    size_t n;

    va_start(ap, pszFormat);
    n = vsnprintf(msg, sizeof(msg) - 1, pszFormat, ap);
    msg[sizeof(msg) - 1] = '\0';
    printk ("%s", msg);
    va_end(ap);
    return n;
}
