/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Tests init and term of the support library
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>


int main(int argc, char **argv)
{
    int rc;
    RTR3Init(false);
    rc = SUPInit();
    RTPrintf("tstInit: SUPInit -> rc=%d\n", rc);
    if (!rc)
    {
        rc = SUPTerm();
        RTPrintf("tstInit: SUPTerm -> rc=%d\n", rc);
    }

    return rc;
}
