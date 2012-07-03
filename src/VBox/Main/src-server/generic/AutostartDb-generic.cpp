/* $Id$ */
/** @file
 * VirtualBox Main - Autostart implementation.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/process.h>
#include <iprt/path.h>
#include <iprt/mem.h>
#include <iprt/file.h>
#include <iprt/string.h>

#include "AutostartDb.h"

/** @todo: Make configurable through kmk/installer. */
#define AUTOSTART_DATABASE "/etc/vbox/autostart.d"

#if defined(RT_OS_LINUX)
/**
 * Return the username of the current process.
 *
 * @returns Pointer to the string containing the username of
 *          NULL in case of an error. Free with RTMemFree().
 */
static int autostartGetProcessUser(char **ppszUser)
{
    int rc = VINF_SUCCESS;
    size_t cbUser = 128;
    char *pszUser = (char *)RTMemAllocZ(cbUser);

    if (pszUser)
    {
        rc = RTProcQueryUsername(RTProcSelf(), pszUser, cbUser, &cbUser);
        if (rc == VERR_BUFFER_OVERFLOW)
        {
            char *pszTmp = (char *)RTMemRealloc(pszUser, cbUser);
            if (pszTmp)
            {
                pszUser = pszTmp;
                rc = RTProcQueryUsername(RTProcSelf(), pszUser, cbUser, &cbUser);
                Assert(rc != VERR_BUFFER_OVERFLOW);
            }
        }

        if (RT_FAILURE(rc))
        {
            RTMemFree(pszUser);
            pszUser = NULL;
        }
        else
            *ppszUser = pszUser;
    }

    return rc;
}

/**
 * Modifies the autostart database.
 *
 * @returns VBox status code.
 * @param   fAutostart    Flag whether the autostart or autostop database is modified.
 * @param   fAddVM        Flag whether a VM is added or removed from the database.
 */
static int autostartModifyDb(bool fAutostart, bool fAddVM)
{
    int rc = VINF_SUCCESS;
    char *pszUser = NULL;

    rc = autostartGetProcessUser(&pszUser);
    if (   RT_SUCCESS(rc)
        && pszUser)
    {
        char *pszFile;
        uint64_t fOpen = RTFILE_O_DENY_ALL | RTFILE_O_READWRITE;
        RTFILE hAutostartFile;

        if (fAddVM)
            fOpen |= RTFILE_O_OPEN_CREATE;

        rc = RTStrAPrintf(&pszFile, "%s/%s.%s",
                          AUTOSTART_DATABASE, pszUser, fAutostart ? "start" : "stop");
        if (RT_SUCCESS(rc))
        {
            rc = RTFileOpen(&hAutostartFile, pszFile, fOpen);
            if (RT_SUCCESS(rc))
            {
                uint64_t cbFile;

                /*
                 * Files with more than 16 bytes are rejected because they just contain
                 * a number of the amount of VMs with autostart configured, so they
                 * should be really really small. Anything else is bogus.
                 */
                rc = RTFileGetSize(hAutostartFile, &cbFile);
                if (   RT_SUCCESS(rc)
                    && cbFile <= 16)
                {
                    char abBuf[16 + 1]; /* trailing \0 */
                    uint32_t cAutostartVms = 0;

                    memset(abBuf, 0, sizeof(abBuf));

                    /* Check if the file was just created. */
                    if (cbFile)
                    {
                        rc = RTFileRead(hAutostartFile, abBuf, cbFile, NULL);
                        if (RT_SUCCESS(rc))
                        {
                            rc = RTStrToUInt32Ex(abBuf, NULL, 10 /* uBase */, &cAutostartVms);
                            if (   rc == VWRN_TRAILING_CHARS
                                || rc == VWRN_TRAILING_SPACES)
                                rc = VINF_SUCCESS;
                        }
                    }

                    if (RT_SUCCESS(rc))
                    {
                        size_t cbBuf;

                        /* Modify VM counter and write back. */
                        if (fAddVM)
                            cAutostartVms++;
                        else
                            cAutostartVms--;

                        if (cAutostartVms > 0)
                        {
                            cbBuf = RTStrPrintf(abBuf, sizeof(abBuf), "%u", cAutostartVms);
                            rc = RTFileSetSize(hAutostartFile, cbBuf);
                            if (RT_SUCCESS(rc))
                                rc = RTFileWriteAt(hAutostartFile, 0, abBuf, cbBuf, NULL);
                        }
                        else
                        {
                            /* Just delete the file if there are no VMs left. */
                            RTFileClose(hAutostartFile);
                            RTFileDelete(pszFile);
                            hAutostartFile = NIL_RTFILE;
                        }
                    }
                }
                else if (RT_SUCCESS(rc))
                    rc = VERR_FILE_TOO_BIG;

                if (hAutostartFile != NIL_RTFILE)
                    RTFileClose(hAutostartFile);
            }
            RTStrFree(pszFile);
        }
    }
    else if (pszUser)
        rc = VERR_NOT_SUPPORTED;

    return rc;
}

#endif

AutostartDb::AutostartDb()
{
#ifdef RT_OS_LINUX
    int rc = RTCritSectInit(&this->CritSect);
    NOREF(rc);
#endif
}

AutostartDb::~AutostartDb()
{
#ifdef RT_OS_LINUX
    RTCritSectDelete(&this->CritSect);
#endif
}

int AutostartDb::addAutostartVM(const char *pszVMId)
{
    int rc = VERR_NOT_SUPPORTED;

#if defined(RT_OS_LINUX)
    NOREF(pszVMId); /* Not needed */

    RTCritSectEnter(&this->CritSect);
    rc = autostartModifyDb(true /* fAutostart */, true /* fAddVM */);
    RTCritSectLeave(&this->CritSect);
#elif defined(RT_OS_DARWIN)
    NOREF(pszVMId); /* Not needed */
    rc = VINF_SUCCESS;
#else
    NOREF(pszVMId);
    rc = VERR_NOT_SUPPORTED;
#endif

    return rc;
}

int AutostartDb::removeAutostartVM(const char *pszVMId)
{
    int rc = VINF_SUCCESS;

#if defined(RT_OS_LINUX)
    NOREF(pszVMId); /* Not needed */
    RTCritSectEnter(&this->CritSect);
    rc = autostartModifyDb(true /* fAutostart */, false /* fAddVM */);
    RTCritSectLeave(&this->CritSect);
#elif defined(RT_OS_DARWIN)
    NOREF(pszVMId); /* Not needed */
    rc = VINF_SUCCESS;
#else
    NOREF(pszVMId);
    rc = VERR_NOT_SUPPORTED;
#endif

    return rc;
}

int AutostartDb::addAutostopVM(const char *pszVMId)
{
    int rc = VINF_SUCCESS;

#if defined(RT_OS_LINUX)
    NOREF(pszVMId); /* Not needed */
    RTCritSectEnter(&this->CritSect);
    rc = autostartModifyDb(false /* fAutostart */, true /* fAddVM */);
    RTCritSectLeave(&this->CritSect);
#elif defined(RT_OS_DARWIN)
    NOREF(pszVMId); /* Not needed */
    rc = VINF_SUCCESS;
#else
    NOREF(pszVMId);
    rc = VERR_NOT_SUPPORTED;
#endif

    return rc;
}

int AutostartDb::removeAutostopVM(const char *pszVMId)
{
    int rc = VINF_SUCCESS;

#if defined(RT_OS_LINUX)
    NOREF(pszVMId); /* Not needed */
    RTCritSectEnter(&this->CritSect);
    rc = autostartModifyDb(false /* fAutostart */, false /* fAddVM */);
    RTCritSectLeave(&this->CritSect);
#elif defined(RT_OS_DARWIN)
    NOREF(pszVMId); /* Not needed */
    rc = VINF_SUCCESS;
#else
    NOREF(pszVMId);
    rc = VERR_NOT_SUPPORTED;
#endif

    return rc;
}

