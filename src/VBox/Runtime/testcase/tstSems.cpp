/* $Id$ */
/** @file
 * InnoTek Portable Runtime Testcase - Simple Semaphore Smoke Test.
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

