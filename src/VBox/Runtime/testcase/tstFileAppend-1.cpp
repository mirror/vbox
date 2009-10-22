/* $Id$ */
/** @file
 * IPRT Testcase - File Appending.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static int g_cErrors = 0;


static int MyFailure(const char *pszFormat, ...)
{
    va_list va;

    RTPrintf("tstFileAppend-1: FATAL: ");
    va_start(va, pszFormat);
    RTPrintfV(pszFormat, va);
    va_end(va);
    g_cErrors++;
    return 1;
}


void MyError(const char *pszFormat, ...)
{
    va_list va;

    RTPrintf("tstFileAppend-1: ERROR: ");
    va_start(va, pszFormat);
    RTPrintfV(pszFormat, va);
    va_end(va);
    g_cErrors++;
}

int main()
{
    int rc;
    RTFILE File;
    int64_t off;
    uint64_t offActual;
    size_t cb;
    char szBuf[256];


    RTPrintf("tstFileAppend-1: TESTING...\n");

    RTR3Init();

    /*
     * Open it write only and do some appending.
     * Checking that read fails and that the file position changes after the write.
     */
    RTFileDelete("tstFileAppend-1.tst");
    rc = RTFileOpen(&File,
                    "tstFileAppend-1.tst",
                      RTFILE_O_WRITE
                    | RTFILE_O_APPEND
                    | RTFILE_O_OPEN_CREATE
                    | RTFILE_O_DENY_NONE
                    | (0644 << RTFILE_O_CREATE_MODE_SHIFT)
                   );
    if (RT_FAILURE(rc))
        return MyFailure("1st RTFileOpen: %Rrc\n", rc);

    off = 0;
    rc = RTFileSeek(File, off, RTFILE_SEEK_CURRENT, &offActual);
    if (RT_FAILURE(rc))
        MyError("1st RTFileSeek failed: %Rrc\n", rc);
    else if (offActual != 0)
        MyError("unexpected position on 1st open: %llu - expected 0\n", offActual);

    rc = RTFileWrite(File, "0123456789", 10, &cb);
    if (RT_FAILURE(rc))
        MyError("1st write fail: %Rrc\n", rc);

    off = 0;
    rc = RTFileSeek(File, off, RTFILE_SEEK_CURRENT, &offActual);
    if (RT_FAILURE(rc))
        MyError("2nd RTFileSeek failed: %Rrc\n", rc);
    else if (offActual != 10)
        MyError("unexpected position after 1st write: %llu - expected 10\n", offActual);
    else
        RTPrintf("tstFileAppend-1: off=%llu after first write\n", offActual);

    rc = RTFileRead(File, szBuf, 1, &cb);
    if (RT_SUCCESS(rc))
        MyError("read didn't fail! cb=%#lx\n", (long)cb);

    off = 5;
    rc = RTFileSeek(File, off, RTFILE_SEEK_BEGIN, &offActual);
    if (RT_FAILURE(rc))
        MyError("3rd RTFileSeek failed: %Rrc\n", rc);
    else if (offActual != 5)
        MyError("unexpected position after 3rd set file pointer: %llu - expected 5\n", offActual);

    RTFileClose(File);


    /*
     * Open it write only and do some more appending.
     * Checking the initial position and that it changes after the write.
     */
    rc = RTFileOpen(&File,
                    "tstFileAppend-1.tst",
                      RTFILE_O_WRITE
                    | RTFILE_O_APPEND
                    | RTFILE_O_OPEN
                    | RTFILE_O_DENY_NONE
                   );
    if (RT_FAILURE(rc))
        return MyFailure("2nd RTFileOpen: %Rrc\n", rc);

    off = 0;
    rc = RTFileSeek(File, off, RTFILE_SEEK_CURRENT, &offActual);
    if (RT_FAILURE(rc))
        MyError("4th RTFileSeek failed: %Rrc\n", rc);
    else if (offActual != 0)
        MyError("unexpected position on 2nd open: %llu - expected 0\n", offActual);
    else
        RTPrintf("tstFileAppend-1: off=%llu on 2nd open\n", offActual);

    rc = RTFileWrite(File, "abcdefghij", 10, &cb);
    if (RT_FAILURE(rc))
        MyError("2nd write fail: %Rrc\n", rc);

    off = 0;
    rc = RTFileSeek(File, off, RTFILE_SEEK_CURRENT, &offActual);
    if (RT_FAILURE(rc))
        MyError("5th RTFileSeek failed: %Rrc\n", rc);
    else if (offActual != 20)
        MyError("unexpected position after 2nd write: %llu - expected 20\n", offActual);
    else
        RTPrintf("tstFileAppend-1: off=%llu after 2nd write\n", offActual);

    RTFileClose(File);

    /*
     * Open it read/write.
     * Check the initial position and read stuff. Then append some more and
     * check the new position and see that read returns 0/EOF. Finally,
     * do some seeking and read from a new position.
     */
    rc = RTFileOpen(&File,
                    "tstFileAppend-1.tst",
                      RTFILE_O_READWRITE
                    | RTFILE_O_APPEND
                    | RTFILE_O_OPEN
                    | RTFILE_O_DENY_NONE
                   );
    if (RT_FAILURE(rc))
        return MyFailure("3rd RTFileOpen: %Rrc\n", rc);

    off = 0;
    rc = RTFileSeek(File, off, RTFILE_SEEK_CURRENT, &offActual);
    if (RT_FAILURE(rc))
        MyError("6th RTFileSeek failed: %Rrc\n", rc);
    else if (offActual != 0)
        MyError("unexpected position on 3rd open: %llu - expected 0\n", offActual);
    else
        RTPrintf("tstFileAppend-1: off=%llu on 3rd open\n", offActual);

    rc = RTFileRead(File, szBuf, 10, &cb);
    if (RT_FAILURE(rc) || cb != 10)
        MyError("1st RTFileRead failed: %Rrc\n", rc);
    else if (memcmp(szBuf, "0123456789", 10))
        MyError("read the wrong stuff: %.10s - expected 0123456789\n", szBuf);

    off = 0;
    rc = RTFileSeek(File, off, RTFILE_SEEK_CURRENT, &offActual);
    if (RT_FAILURE(rc))
        MyError("7th RTFileSeek failed: %Rrc\n", rc);
    else if (offActual != 10)
        MyError("unexpected position after 1st read: %llu - expected 10\n", offActual);
    else
        RTPrintf("tstFileAppend-1: off=%llu on 1st open\n", offActual);

    rc = RTFileWrite(File, "klmnopqrst", 10, &cb);
    if (RT_FAILURE(rc))
        MyError("3rd write fail: %Rrc\n", rc);

    off = 0;
    rc = RTFileSeek(File, off, RTFILE_SEEK_CURRENT, &offActual);
    if (RT_FAILURE(rc))
        MyError("8th RTFileSeek failed: %Rrc\n", rc);
    else if (offActual != 30)
        MyError("unexpected position after 3rd write: %llu - expected 30\n", offActual);
    else
        RTPrintf("tstFileAppend-1: off=%llu after 3rd write\n", offActual);

    rc = RTFileRead(File, szBuf, 1, &cb);
    if (RT_SUCCESS(rc) && cb != 0)
        MyError("read after write didn't fail! cb=%#lx\n", (long)cb);

    off = 15;
    rc = RTFileSeek(File, off, RTFILE_SEEK_BEGIN, &offActual);
    if (RT_FAILURE(rc))
        MyError("9th RTFileSeek failed: %Rrc\n", rc);
    else if (offActual != 15)
        MyError("unexpected position after 9th seek: %llu - expected 15\n", offActual);
    else
    {
        rc = RTFileRead(File, szBuf, 10, &cb);
        if (RT_FAILURE(rc) || cb != 10)
            MyError("2nd RTFileRead failed: %Rrc\n", rc);
        else if (memcmp(szBuf, "fghijklmno", 10))
            MyError("read the wrong stuff: %.10s - expected fghijklmno\n", szBuf);

        off = 0;
        rc = RTFileSeek(File, off, RTFILE_SEEK_CURRENT, &offActual);
        if (RT_FAILURE(rc))
            MyError("10th RTFileSeek failed: %Rrc\n", rc);
        else if (offActual != 25)
            MyError("unexpected position after 2nd read: %llu - expected 25\n", offActual);
        else
            RTPrintf("tstFileAppend-1: off=%llu after 2nd read\n", offActual);
    }

    RTFileClose(File);

    /*
     * Open it read only + append and check that we cannot write to it.
     */
    rc = RTFileOpen(&File,
                    "tstFileAppend-1.tst",
                      RTFILE_O_READ
                    | RTFILE_O_APPEND
                    | RTFILE_O_OPEN
                    | RTFILE_O_DENY_NONE
                   );
    if (RT_FAILURE(rc))
        return MyFailure("4th RTFileOpen: %Rrc\n", rc);

    rc = RTFileWrite(File, "pqrstuvwx", 10, &cb);
    if (RT_SUCCESS(rc))
        MyError("write didn't fail on read-only+append open: cb=%#lx\n", (long)cb);

    RTFileClose(File);
    RTFileDelete("tstFileAppend-1.tst");

    if (g_cErrors)
        RTPrintf("tstFileAppend-1: FAILED\n");
    else
        RTPrintf("tstFileAppend-1: SUCCESS\n");
    return g_cErrors ? 1 : 0;
}

