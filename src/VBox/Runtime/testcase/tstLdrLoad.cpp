/* $Id$ */
/** @file
 * InnoTek Portable Runtime Testcase - Native Loader.
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


#include <iprt/ldr.h>
#include <iprt/stream.h>
#include <iprt/runtime.h>
#include <iprt/err.h>

int main(int argc, const char * const *argv)
{
    int rcRet = 0;
    RTR3Init();

    /*
     * If no args, display usage.
     */
    if (argc <= 1)
    {
        RTPrintf("Syntax: %s [so/dll [so/dll [..]]\n", argv[0]);
        return 1;
    }

    /*
     * Iterate the arguments and treat all of them as so/dll paths.
     */
    for (int i = 1; i < argc; i++)
    {
        RTLDRMOD hLdrMod = (RTLDRMOD)0xbaadffaa;
        int rc = RTLdrLoad(argv[i], &hLdrMod);
        if (RT_SUCCESS(rc))
        {
            RTPrintf("tstLdrLoad: %d - %s\n", i, argv[i]);
            rc = RTLdrClose(hLdrMod);
            if (RT_FAILURE(rc))
            {
                RTPrintf("tstLdrLoad: rc=%Rrc RTLdrClose()\n", rc);
                rcRet++;
            }
        }
        else
        {
            RTPrintf("tstLdrLoad: rc=%Rrc RTLdrOpen('%s')\n", rc, argv[i]);
            rcRet++;
        }
    }

    /*
     * Summary.
     */
    if (!rcRet)
        RTPrintf("tstLdrLoad: SUCCESS\n");
    else
        RTPrintf("tstLdrLoad: FAILURE - %d errors\n", rcRet);

    return !!rcRet;
}
