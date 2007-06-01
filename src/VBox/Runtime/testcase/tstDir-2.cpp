/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - Directory listing & filtering .
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include <iprt/dir.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/runtime.h>

int main(int argc, char **argv)
{
    int rcRet = 0;
    RTR3Init();

    /*
     * Iterate arguments.
     */
    for (int i = 1; i < argc; i++)
    {
        /* open */
        PRTDIR pDir;
        int rc = RTDirOpenFiltered(&pDir, argv[i], RTDIRFILTER_WINNT);
        if (RT_SUCCESS(rc))
        {
            for (;;)
            {
                RTDIRENTRY DirEntry;
                rc = RTDirRead(pDir, &DirEntry, NULL);
                if (RT_FAILURE(rc))
                    break;
                switch (DirEntry.enmType)
                {
                    case RTDIRENTRYTYPE_UNKNOWN:     RTPrintf("u"); break;
                    case RTDIRENTRYTYPE_FIFO:        RTPrintf("f"); break;
                    case RTDIRENTRYTYPE_DEV_CHAR:    RTPrintf("c"); break;
                    case RTDIRENTRYTYPE_DIRECTORY:   RTPrintf("d"); break;
                    case RTDIRENTRYTYPE_DEV_BLOCK:   RTPrintf("b"); break;
                    case RTDIRENTRYTYPE_FILE:        RTPrintf("-"); break;
                    case RTDIRENTRYTYPE_SYMLINK:     RTPrintf("l"); break;
                    case RTDIRENTRYTYPE_SOCKET:      RTPrintf("s"); break;
                    case RTDIRENTRYTYPE_WHITEOUT:    RTPrintf("w"); break;
                    default:
                        rcRet = 1;
                        RTPrintf("?");
                        break;
                }
                RTPrintf(" %#18llx  %3d %s\n", (uint64_t)DirEntry.INodeId,
                         DirEntry.cbName, DirEntry.szName);
            }

            if (rc != VERR_NO_MORE_FILES)
            {
                RTPrintf("tstDir: Enumeration failed! rc=%Rrc\n", rc);
                rcRet = 1;
            }

            /* close up */
            rc = RTDirClose(pDir);
            if (RT_FAILURE(rc))
            {
                RTPrintf("tstDir: Failed to close dir! rc=%Rrc\n", rc);
                rcRet = 1;
            }
        }
        else
        {
            RTPrintf("tstDir: Failed to open '%s', rc=%Rrc\n", argv[i], rc);
            rcRet = 1;
        }
    }

    return rcRet;
}

