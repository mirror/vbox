/** @file
 *
 * Testcase for shared folder structure sizes.
 * Run this on Linux and Windows, then compare.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/shflsvc.h>
#include <iprt/stream.h>

#define STRUCT(t, size)   \
    do { \
        if (fPrintChecks) \
            RTPrintf("    STRUCT(" #t ", %d);\n", (int)sizeof(t)); \
        else if ((size) != sizeof(t)) \
        { \
            RTPrintf("%30s: %d expected %d!\n", #t, (int)sizeof(t), (size)); \
            cErrors++; \
        } \
        else \
            RTPrintf("%30s: %d\n", #t, (int)sizeof(t)); \
    } while (0)


int main(int argc, char **argv)
{
    unsigned cErrors = 0;

    /*
     * Prints the code below if any argument was giving.
     */
    bool fPrintChecks = argc != 1;

    /*
     * The checks.
     */
    STRUCT(SHFLROOT, 4);
    STRUCT(SHFLHANDLE, 8);
    STRUCT(SHFLSTRING, 6);
    STRUCT(SHFLCREATERESULT, 4);
    STRUCT(SHFLCREATEPARMS, 108);
    STRUCT(SHFLMAPPING, 8);
    STRUCT(SHFLDIRINFO, 128);
    STRUCT(SHFLVOLINFO, 40);
    STRUCT(VBoxSFQueryMappings, 52);
    STRUCT(VBoxSFQueryMapName, 52);
    STRUCT(VBoxSFMapFolder_Old, 52);
    STRUCT(VBoxSFMapFolder, 64);
    STRUCT(VBoxSFUnmapFolder, 28);
    STRUCT(VBoxSFCreate, 52);
    STRUCT(VBoxSFClose, 40);
    STRUCT(VBoxSFRead, 76);
    STRUCT(VBoxSFWrite, 76);
    STRUCT(VBoxSFLock, 76);
    STRUCT(VBoxSFFlush, 40);
    STRUCT(VBoxSFList, 112);
    STRUCT(VBoxSFInformation, 76);
    STRUCT(VBoxSFRemove, 52);
    STRUCT(VBoxSFRename, 64);
    STRUCT(VBoxSFAddMapping, 40);
    STRUCT(VBoxSFRemoveMapping, 28);

    /*
     * The summary.
     */
    if (!cErrors)
        RTPrintf("tstShflSizes: SUCCESS\n");
    else
        RTPrintf("tstShflSizes: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}

