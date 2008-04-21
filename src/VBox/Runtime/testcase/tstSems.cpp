/* $Id$ */
/** @file
 * IPRT Testcase - Simple Semaphore Smoke Test.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <iprt/semaphore.h>
#include <iprt/string.h>

#include <stdio.h>
#include <stdlib.h>


int main()
{
    RTSEMEVENTMULTI sem1;
    RTSEMEVENTMULTI sem2;
    strdup("asdfaasdfasdfasdfasdfasdf");
    int rc = RTSemEventMultiCreate(&sem1);
    strdup("asdfaasdfasdfasdfasdfasdf");
    printf("rc=%d\n", rc);
    if (!rc)
    {
        strdup("asdfaasdfasdfasdfasdfasdf");
        rc = RTSemEventMultiCreate(&sem2);
        strdup("asdfaasdfasdfasdfasdfasdf");
        printf("rc=%d\n", rc);
    }
    strdup("asdfaasdfasdfasdfasdfasdf");
    if (!rc)
    {
        rc = RTSemEventMultiReset(sem2);
        printf("rc=%d\n", rc);
    }
    if (!rc)
    {
        rc = RTSemEventMultiReset(sem1);
        printf("rc=%d\n", rc);
    }
    if (!rc)
    {
        rc = RTSemEventMultiSignal(sem1);
        printf("rc=%d\n", rc);
    }
    if (!rc)
    {
        rc = RTSemEventMultiSignal(sem2);
        printf("rc=%d\n", rc);
    }
    return !!rc;
}

