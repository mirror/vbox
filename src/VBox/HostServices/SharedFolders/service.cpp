/** @file
 * Shared Folders: Host service entry points.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <VBox/shflsvc.h>


#include "shfl.h"
#include "mappings.h"
#include "shflhandle.h"
#include "vbsf.h"
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <VBox/ssm.h>
#include <VBox/pdm.h>

#define SHFL_SSM_VERSION        2


/* Shared Folders Host Service.
 *
 * Shared Folders map a host file system to guest logical filesystem.
 * A mapping represents 'host name'<->'guest name' translation and a root
 * identifier to be used to access this mapping.
 * Examples: "C:\WINNT"<->"F:", "C:\WINNT\System32"<->"/mnt/host/system32".
 *
 * Therefore, host name and guest name are strings interpreted
 * only by host service and guest client respectively. Host name is
 * passed to guest only for informational purpose. Guest may for example
 * display the string or construct volume label out of the string.
 *
 * Root identifiers are unique for whole guest life,
 * that is until next guest reset/fresh start.
 * 32 bit value incremented for each new mapping is used.
 *
 * Mapping strings are taken from VM XML configuration on VM startup.
 * The service DLL takes mappings during initialization. There is
 * also API for changing mappings at runtime.
 *
 * Current mappings and root identifiers are saved when VM is saved.
 *
 * Guest may use any of these mappings. Full path information
 * about an object on a mapping consists of the root indentifier and
 * a full path of object.
 *
 * Guest IFS connects to the service and calls SHFL_FN_QUERY_MAP
 * function which returns current mappings. For guest convenience,
 * removed mappings also returned with REMOVED flag and new mappings
 * are marked with NEW flag.
 *
 * To access host file system guest just forwards file system calls
 * to the service, and specifies full paths or handles for objects.
 *
 *
 */


PVBOXHGCMSVCHELPERS g_pHelpers;
static PPDMLED      pStatusLed = NULL;

static DECLCALLBACK(int) svcUnload (void *)
{
    int rc = VINF_SUCCESS;

    Log(("svcUnload\n"));

    return rc;
}

static DECLCALLBACK(int) svcConnect (void *, uint32_t u32ClientID, void *pvClient)
{
    int rc = VINF_SUCCESS;

    NOREF(u32ClientID);
    NOREF(pvClient);

    LogRel(("SharedFolders host service: connected, u32ClientID = %d\n", u32ClientID));

    return rc;
}

static DECLCALLBACK(int) svcDisconnect (void *, uint32_t u32ClientID, void *pvClient)
{
    int rc = VINF_SUCCESS;
    SHFLCLIENTDATA *pClient = (SHFLCLIENTDATA *)pvClient;

    LogRel(("SharedFolders host service: disconnected, u32ClientID = %d\n", u32ClientID));

    vbsfDisconnect(pClient);
    return rc;
}

/** @note We only save as much state as required to access the shared folder again after restore.
 *        All I/O requests pending at the time of saving will never be completed or result in errors.
 *        (file handles no longer valid etc)
 *        This works as designed at the moment. A full state save would be difficult and not always possible
 *        as the contents of a shared folder might change in between save and restore.
 */
static DECLCALLBACK(int) svcSaveState(void *, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    SHFLCLIENTDATA *pClient = (SHFLCLIENTDATA *)pvClient;

    LogRel(("SharedFolders host service: saving state, u32ClientID = %d\n", u32ClientID));

    int rc = SSMR3PutU32(pSSM, SHFL_SSM_VERSION);
    AssertRCReturn(rc, rc);

    rc = SSMR3PutU32(pSSM, SHFL_MAX_MAPPINGS);
    AssertRCReturn(rc, rc);

    /* Save client structure length & contents */
    rc = SSMR3PutU32(pSSM, sizeof(*pClient));
    AssertRCReturn(rc, rc);

    rc = SSMR3PutMem(pSSM, pClient, sizeof(*pClient));
    AssertRCReturn(rc, rc);

    /* Save all the active mappings. */
    for (int i=0;i<SHFL_MAX_MAPPINGS;i++)
    {
        /* Mapping are saved in the order of increasing root handle values. */
        MAPPING *pFolderMapping = vbsfMappingGetByRoot(i);

        rc = SSMR3PutU32(pSSM, pFolderMapping? pFolderMapping->cMappings: 0);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutBool(pSSM, pFolderMapping? pFolderMapping->fValid: false);
        AssertRCReturn(rc, rc);

        if (pFolderMapping && pFolderMapping->fValid)
        {
            uint32_t len;

            len = ShflStringSizeOfBuffer(pFolderMapping->pFolderName);
            rc = SSMR3PutU32(pSSM, len);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutMem(pSSM, pFolderMapping->pFolderName, len);
            AssertRCReturn(rc, rc);

            len = ShflStringSizeOfBuffer(pFolderMapping->pMapName);
            rc = SSMR3PutU32(pSSM, len);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutMem(pSSM, pFolderMapping->pMapName, len);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutBool(pSSM, pFolderMapping->fHostCaseSensitive);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutBool(pSSM, pFolderMapping->fGuestCaseSensitive);
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) svcLoadState(void *, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    uint32_t        nrMappings;
    SHFLCLIENTDATA *pClient = (SHFLCLIENTDATA *)pvClient;
    uint32_t        len, version;

    LogRel(("SharedFolders host service: loading state, u32ClientID = %d\n", u32ClientID));

    int rc = SSMR3GetU32(pSSM, &version);
    AssertRCReturn(rc, rc);

    if (version != SHFL_SSM_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    rc = SSMR3GetU32(pSSM, &nrMappings);
    AssertRCReturn(rc, rc);
    if (nrMappings != SHFL_MAX_MAPPINGS)
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;

    /* Restore the client data (flags + path delimiter at the moment) */
    rc = SSMR3GetU32(pSSM, &len);
    AssertRCReturn(rc, rc);

    if (len != sizeof(*pClient))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;

    rc = SSMR3GetMem(pSSM, pClient, sizeof(*pClient));
    AssertRCReturn(rc, rc);

    /* We don't actually (fully) restore the state; we simply check if the current state is as we it expect it to be. */
    for (int i=0;i<SHFL_MAX_MAPPINGS;i++)
    {
        /* Load the saved mapping description and try to find it in the mappings. */
        MAPPING mapping;
        memset (&mapping, 0, sizeof (mapping));

        /* restore the folder mapping counter. */
        rc = SSMR3GetU32(pSSM, &mapping.cMappings);
        AssertRCReturn(rc, rc);

        rc = SSMR3GetBool(pSSM, &mapping.fValid);
        AssertRCReturn(rc, rc);

        if (mapping.fValid)
        {
            uint32_t cbFolderName;
            PSHFLSTRING pFolderName;

            uint32_t cbMapName;
            PSHFLSTRING pMapName;

            /* Load the host path name. */
            rc = SSMR3GetU32(pSSM, &cbFolderName);
            AssertRCReturn(rc, rc);

            pFolderName = (PSHFLSTRING)RTMemAlloc(cbFolderName);
            AssertReturn(pFolderName != NULL, VERR_NO_MEMORY);

            rc = SSMR3GetMem(pSSM, pFolderName, cbFolderName);
            AssertRCReturn(rc, rc);

            /* Load the map name. */
            rc = SSMR3GetU32(pSSM, &cbMapName);
            AssertRCReturn(rc, rc);

            pMapName = (PSHFLSTRING)RTMemAlloc(cbMapName);
            AssertReturn(pMapName != NULL, VERR_NO_MEMORY);

            rc = SSMR3GetMem(pSSM, pMapName, cbMapName);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetBool(pSSM, &mapping.fHostCaseSensitive);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetBool(pSSM, &mapping.fGuestCaseSensitive);
            AssertRCReturn(rc, rc);

            mapping.pFolderName = pFolderName;
            mapping.pMapName = pMapName;

            /* 'i' is the root handle of the saved mapping. */
            rc = vbsfMappingLoaded (&mapping, i);

            RTMemFree(pMapName);
            RTMemFree(pFolderName);

            AssertRCReturn(rc, rc);
        }
    }
    LogRel(("SharedFolders host service: success\n"));
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) svcCall (void *, VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    Log(("svcCall: u32ClientID = %d, fn = %d, cParms = %d, pparms = %d\n", u32ClientID, u32Function, cParms, paParms));

    SHFLCLIENTDATA *pClient = (SHFLCLIENTDATA *)pvClient;

    bool fAsynchronousProcessing = false;

#ifdef DEBUG
    uint32_t i;

    for (i = 0; i < cParms; i++)
    {
        /** @todo parameters other than 32 bit */
        Log(("    pparms[%d]: type %d value %d\n", i, paParms[i].type, paParms[i].u.uint32));
    }
#endif

    switch (u32Function)
    {
        case SHFL_FN_QUERY_MAPPINGS:
        {
            Log(("svcCall: SHFL_FN_QUERY_MAPPINGS\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_QUERY_MAPPINGS)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* flags */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT   /* numberOfMappings */
                     || paParms[2].type != VBOX_HGCM_SVC_PARM_PTR     /* mappings */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint32_t fu32Flags     = paParms[0].u.uint32;
                uint32_t cMappings     = paParms[1].u.uint32;
                SHFLMAPPING *pMappings = (SHFLMAPPING *)paParms[2].u.pointer.addr;
                uint32_t cbMappings    = paParms[2].u.pointer.size;

                /* Verify parameters values. */
                if (   (fu32Flags & ~SHFL_MF_UTF8) != 0
                    || cbMappings / sizeof (SHFLMAPPING) < cMappings
                   )
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */
                    if (fu32Flags & SHFL_MF_UTF8)
                    {
                        pClient->fu32Flags |= SHFL_CF_UTF8;
                    }

                    rc = vbsfMappingsQuery (pClient, pMappings, &cMappings);

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        paParms[1].u.uint32 = cMappings;
                    }
                }
            }


        } break;

        case SHFL_FN_QUERY_MAP_NAME:
        {
            Log(("svcCall: SHFL_FN_QUERY_MAP_NAME\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_QUERY_MAP_NAME)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* root */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR     /* name */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT  root         = (SHFLROOT)paParms[0].u.uint32;
                SHFLSTRING *pString    = (SHFLSTRING *)paParms[1].u.pointer.addr;

                /* Verify parameters values. */
                if (!ShflStringIsValid(pString, paParms[1].u.pointer.size))
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */
                    rc = vbsfMappingsQueryName (pClient, root, pString);

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        ; /* none */
                    }
                }
            }

        } break;

        case SHFL_FN_CREATE:
        {
            Log(("svcCall: SHFL_FN_CREATE\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_CREATE)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* root */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR   /* path */
                     || paParms[2].type != VBOX_HGCM_SVC_PARM_PTR   /* parms */
                    )
            {
                Log(("Invalid parameters types\n"));
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT  root          = (SHFLROOT)paParms[0].u.uint32;
                SHFLSTRING *pPath       = (SHFLSTRING *)paParms[1].u.pointer.addr;
                uint32_t cbPath         = paParms[1].u.pointer.size;
                SHFLCREATEPARMS *pParms = (SHFLCREATEPARMS *)paParms[2].u.pointer.addr;
                uint32_t cbParms        = paParms[2].u.pointer.size;

                /* Verify parameters values. */
                if (   !ShflStringIsValid(pPath, cbPath)
                    || (cbParms != sizeof (SHFLCREATEPARMS))
                   )
                {
                    AssertMsgFailed (("Invalid parameters cbPath or cbParms (%x, %x - expected >=%x, %x)\n",
                                      cbPath, cbParms, sizeof(SHFLSTRING), sizeof (SHFLCREATEPARMS)));
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */

                    rc = vbsfCreate (pClient, root, pPath, cbPath, pParms);

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        ; /* none */
                    }
                }
            }
            break;
        }

        case SHFL_FN_CLOSE:
        {
            Log(("svcCall: SHFL_FN_CLOSE\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_CLOSE)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_64BIT   /* handle */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT   root   = (SHFLROOT)paParms[0].u.uint32;
                SHFLHANDLE Handle = paParms[1].u.uint64;

                /* Verify parameters values. */
                if (Handle == SHFL_HANDLE_ROOT)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                if (Handle == SHFL_HANDLE_NIL)
                {
                    AssertMsgFailed(("Invalid handle!!!!\n"));
                    rc = VERR_INVALID_HANDLE;
                }
                else
                {
                    /* Execute the function. */

                    rc = vbsfClose (pClient, root, Handle);

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        ; /* none */
                    }
                }
            }
            break;

        }

        /** Read object content. */
        case SHFL_FN_READ:
            Log(("svcCall: SHFL_FN_READ\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_READ)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                || paParms[1].type != VBOX_HGCM_SVC_PARM_64BIT   /* handle */
                || paParms[2].type != VBOX_HGCM_SVC_PARM_64BIT   /* offset */
                || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT   /* count */
                || paParms[4].type != VBOX_HGCM_SVC_PARM_PTR     /* buffer */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT   root    = (SHFLROOT)paParms[0].u.uint32;
                SHFLHANDLE Handle  = paParms[1].u.uint64;
                uint64_t   offset  = paParms[2].u.uint64;
                uint32_t   count   = paParms[3].u.uint32;
                uint8_t   *pBuffer = (uint8_t *)paParms[4].u.pointer.addr;

                /* Verify parameters values. */
                if (   Handle == SHFL_HANDLE_ROOT
                    || count > paParms[4].u.pointer.size
                   )
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                if (Handle == SHFL_HANDLE_NIL)
                {
                    AssertMsgFailed(("Invalid handle!!!!\n"));
                    rc = VERR_INVALID_HANDLE;
                }
                else
                {
                    /* Execute the function. */
                    if (pStatusLed)
                    {
                        Assert(pStatusLed->u32Magic == PDMLED_MAGIC);
                        pStatusLed->Asserted.s.fReading = pStatusLed->Actual.s.fReading = 1;
                    }

                    rc = vbsfRead (pClient, root, Handle, offset, &count, pBuffer);
                    if (pStatusLed)
                        pStatusLed->Actual.s.fReading = 0;

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        paParms[3].u.uint32 = count;
                    }
                    else
                    {
                        paParms[3].u.uint32 = 0;   /* nothing read */
                    }
                }
            }
            break;

        /** Write new object content. */
        case SHFL_FN_WRITE:
            Log(("svcCall: SHFL_FN_WRITE\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_WRITE)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                || paParms[1].type != VBOX_HGCM_SVC_PARM_64BIT   /* handle */
                || paParms[2].type != VBOX_HGCM_SVC_PARM_64BIT   /* offset */
                || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT   /* count */
                || paParms[4].type != VBOX_HGCM_SVC_PARM_PTR     /* buffer */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT   root    = (SHFLROOT)paParms[0].u.uint32;
                SHFLHANDLE Handle  = paParms[1].u.uint64;
                uint64_t   offset  = paParms[2].u.uint64;
                uint32_t   count   = paParms[3].u.uint32;
                uint8_t   *pBuffer = (uint8_t *)paParms[4].u.pointer.addr;

                /* Verify parameters values. */
                if (   Handle == SHFL_HANDLE_ROOT
                    || count > paParms[4].u.pointer.size
                   )
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                if (Handle == SHFL_HANDLE_NIL)
                {
                    AssertMsgFailed(("Invalid handle!!!!\n"));
                    rc = VERR_INVALID_HANDLE;
                }
                else
                {
                    /* Execute the function. */
                    if (pStatusLed)
                    {
                        Assert(pStatusLed->u32Magic == PDMLED_MAGIC);
                        pStatusLed->Asserted.s.fWriting = pStatusLed->Actual.s.fWriting = 1;
                    }

                    rc = vbsfWrite (pClient, root, Handle, offset, &count, pBuffer);
                    if (pStatusLed)
                        pStatusLed->Actual.s.fWriting = 0;

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        paParms[3].u.uint32 = count;
                    }
                    else
                    {
                        paParms[3].u.uint32 = 0;   /* nothing read */
                    }
                }
            }
            break;

        /** Lock/unlock a range in the object. */
        case SHFL_FN_LOCK:
            Log(("svcCall: SHFL_FN_LOCK\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_LOCK)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                || paParms[1].type != VBOX_HGCM_SVC_PARM_64BIT   /* handle */
                || paParms[2].type != VBOX_HGCM_SVC_PARM_64BIT   /* offset */
                || paParms[3].type != VBOX_HGCM_SVC_PARM_64BIT   /* length */
                || paParms[4].type != VBOX_HGCM_SVC_PARM_32BIT   /* flags */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT  root     = (SHFLROOT)paParms[0].u.uint32;
                SHFLHANDLE Handle  = paParms[1].u.uint64;
                uint64_t   offset  = paParms[2].u.uint64;
                uint64_t   length  = paParms[3].u.uint64;
                uint32_t   flags   = paParms[4].u.uint32;

                /* Verify parameters values. */
                if (Handle == SHFL_HANDLE_ROOT)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                if (Handle == SHFL_HANDLE_NIL)
                {
                    AssertMsgFailed(("Invalid handle!!!!\n"));
                    rc = VERR_INVALID_HANDLE;
                }
                else if (flags & SHFL_LOCK_WAIT)
                {
                    /* @todo This should be properly implemented by the shared folders service.
                     *       The service thread must never block. If an operation requires
                     *       blocking, it must be processed by another thread and when it is
                     *       completed, the another thread must call
                     *
                     *           g_pHelpers->pfnCallComplete (callHandle, rc);
                     *
                     * The operation is async.
                     * fAsynchronousProcessing = true;
                     */

                    /* Here the operation must be posted to another thread. At the moment it is not implemented.
                     * Until it is implemented, try to perform the operation without waiting.
                     */
                    flags &= ~SHFL_LOCK_WAIT;

                    /* Execute the function. */
                    if ((flags & SHFL_LOCK_MODE_MASK) == SHFL_LOCK_CANCEL)
                        rc = vbsfUnlock(pClient, root, Handle, offset, length, flags);
                    else
                        rc = vbsfLock(pClient, root, Handle, offset, length, flags);

                    if (VBOX_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        /* none */
                    }
                }
                else
                {
                    /* Execute the function. */
                    if ((flags & SHFL_LOCK_MODE_MASK) == SHFL_LOCK_CANCEL)
                        rc = vbsfUnlock(pClient, root, Handle, offset, length, flags);
                    else
                        rc = vbsfLock(pClient, root, Handle, offset, length, flags);

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        /* none */
                    }
                }
            }
            break;

        /** List object content. */
        case SHFL_FN_LIST:
        {
            Log(("svcCall: SHFL_FN_LIST\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_LIST)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                || paParms[1].type != VBOX_HGCM_SVC_PARM_64BIT   /* handle */
                || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT   /* flags */
                || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT   /* cb */
                || paParms[4].type != VBOX_HGCM_SVC_PARM_PTR     /* pPath */
                || paParms[5].type != VBOX_HGCM_SVC_PARM_PTR     /* buffer */
                || paParms[6].type != VBOX_HGCM_SVC_PARM_32BIT   /* resumePoint */
                || paParms[7].type != VBOX_HGCM_SVC_PARM_32BIT   /* cFiles (out) */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT  root     = (SHFLROOT)paParms[0].u.uint32;
                SHFLHANDLE Handle  = paParms[1].u.uint64;
                uint32_t   flags   = paParms[2].u.uint32;
                uint32_t   length  = paParms[3].u.uint32;
                SHFLSTRING *pPath  = (paParms[4].u.pointer.size == 0) ? 0 : (SHFLSTRING *)paParms[4].u.pointer.addr;
                uint8_t   *pBuffer = (uint8_t *)paParms[5].u.pointer.addr;
                uint32_t   resumePoint = paParms[6].u.uint32;
                uint32_t   cFiles = 0;

                /* Verify parameters values. */
                if (   (length < sizeof (SHFLDIRINFO))
                    ||  length > paParms[5].u.pointer.size
                    ||  !ShflStringIsValidOrNull(pPath, paParms[4].u.pointer.size)
                   )
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    if (pStatusLed)
                    {
                        Assert(pStatusLed->u32Magic == PDMLED_MAGIC);
                        pStatusLed->Asserted.s.fReading = pStatusLed->Actual.s.fReading = 1;
                    }

                    /* Execute the function. */
                    rc = vbsfDirList (pClient, root, Handle, pPath, flags, &length, pBuffer, &resumePoint, &cFiles);

                    if (pStatusLed)
                        pStatusLed->Actual.s.fReading = 0;

                    if (rc == VERR_NO_MORE_FILES && cFiles != 0)
                        rc = VINF_SUCCESS; /* Successfully return these files. */

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        paParms[3].u.uint32 = length;
                        paParms[6].u.uint32 = resumePoint;
                        paParms[7].u.uint32 = cFiles;
                    }
                    else
                    {
                        paParms[3].u.uint32 = 0;  /* nothing read */
                        paParms[6].u.uint32 = 0;
                        paParms[7].u.uint32 = cFiles;
                    }
                }
            }
            break;
        }

        /* Legacy interface */
        case SHFL_FN_MAP_FOLDER_OLD:
        {
            Log(("svcCall: SHFL_FN_MAP_FOLDER_OLD\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_MAP_FOLDER_OLD)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_PTR     /* path */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                     || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT   /* delimiter */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                PSHFLSTRING pszMapName = (PSHFLSTRING)paParms[0].u.pointer.addr;
                SHFLROOT    root       = (SHFLROOT)paParms[1].u.uint32;
                RTUTF16     delimiter  = (RTUTF16)paParms[2].u.uint32;

                /* Verify parameters values. */
                if (!ShflStringIsValid(pszMapName, paParms[0].u.pointer.size))
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */
                    rc = vbsfMapFolder (pClient, pszMapName, delimiter, false,  &root);

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        paParms[1].u.uint32 = root;
                    }
                }
            }
            break;
        }

        case SHFL_FN_MAP_FOLDER:
        {
            Log(("svcCall: SHFL_FN_MAP_FOLDER\n"));
            if (BIT_FLAG(pClient->fu32Flags, SHFL_CF_UTF8))
                LogRel(("SharedFolders host service: request to map folder %S\n",
                        ((PSHFLSTRING)paParms[0].u.pointer.addr)->String.utf8));
            else
                LogRel(("SharedFolders host service: request to map folder %lS\n",
                        ((PSHFLSTRING)paParms[0].u.pointer.addr)->String.ucs2));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_MAP_FOLDER)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_PTR     /* path */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                     || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT   /* delimiter */
                     || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT   /* fCaseSensitive */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                PSHFLSTRING pszMapName = (PSHFLSTRING)paParms[0].u.pointer.addr;
                SHFLROOT    root       = (SHFLROOT)paParms[1].u.uint32;
                RTUTF16     delimiter  = (RTUTF16)paParms[2].u.uint32;
                bool        fCaseSensitive = !!paParms[3].u.uint32;

                /* Verify parameters values. */
                if (!ShflStringIsValid(pszMapName, paParms[0].u.pointer.size))
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {

                    /* Execute the function. */
                    rc = vbsfMapFolder (pClient, pszMapName, delimiter, fCaseSensitive, &root);

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        paParms[1].u.uint32 = root;
                    }
                }
            }
            LogRel(("SharedFolders host service: map operation result %Rrc.\n", rc));
            if (RT_SUCCESS(rc))
                LogRel(("    Mapped to handle %d.\n", paParms[1].u.uint32));
            break;
        }

        case SHFL_FN_UNMAP_FOLDER:
        {
            Log(("svcCall: SHFL_FN_UNMAP_FOLDER\n"));
            LogRel(("SharedFolders host service: request to unmap folder handle %d\n",
                    paParms[0].u.uint32));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_UNMAP_FOLDER)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if ( paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT    root       = (SHFLROOT)paParms[0].u.uint32;

                /* Execute the function. */
                rc = vbsfUnmapFolder (pClient, root);

                if (RT_SUCCESS(rc))
                {
                    /* Update parameters.*/
                    /* nothing */
                }
            }
            LogRel(("SharedFolders host service: unmap operation result %Rrc.\n", rc));
            break;
        }

        /** Query/set object information. */
        case SHFL_FN_INFORMATION:
        {
            Log(("svcCall: SHFL_FN_INFORMATION\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_INFORMATION)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                || paParms[1].type != VBOX_HGCM_SVC_PARM_64BIT   /* handle */
                || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT   /* flags */
                || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT   /* cb */
                || paParms[4].type != VBOX_HGCM_SVC_PARM_PTR     /* buffer */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT  root     = (SHFLROOT)paParms[0].u.uint32;
                SHFLHANDLE Handle  = paParms[1].u.uint64;
                uint32_t   flags   = paParms[2].u.uint32;
                uint32_t   length  = paParms[3].u.uint32;
                uint8_t   *pBuffer = (uint8_t *)paParms[4].u.pointer.addr;

                /* Verify parameters values. */
                if (length > paParms[4].u.pointer.size)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */
                    if (flags & SHFL_INFO_SET)
                        rc = vbsfSetFSInfo (pClient, root, Handle, flags, &length, pBuffer);
                    else /* SHFL_INFO_GET */
                        rc = vbsfQueryFSInfo (pClient, root, Handle, flags, &length, pBuffer);

                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        paParms[3].u.uint32 = length;
                    }
                    else
                    {
                        paParms[3].u.uint32 = 0;  /* nothing read */
                    }
                }
            }
            break;
        }

        /** Remove or rename object */
        case SHFL_FN_REMOVE:
        {
            Log(("svcCall: SHFL_FN_REMOVE\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_REMOVE)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR   /* path */
                     || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT /* flags */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT  root          = (SHFLROOT)paParms[0].u.uint32;
                SHFLSTRING *pPath       = (SHFLSTRING *)paParms[1].u.pointer.addr;
                uint32_t cbPath         = paParms[1].u.pointer.size;
                uint32_t flags          = paParms[2].u.uint32;

                /* Verify parameters values. */
                if (!ShflStringIsValid(pPath, cbPath))
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */

                    rc = vbsfRemove (pClient, root, pPath, cbPath, flags);
                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        ; /* none */
                    }
                }
            }
            break;
        }

        case SHFL_FN_RENAME:
        {
            Log(("svcCall: SHFL_FN_RENAME\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_RENAME)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                     || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR   /* src */
                     || paParms[2].type != VBOX_HGCM_SVC_PARM_PTR   /* dest */
                     || paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT /* flags */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT    root        = (SHFLROOT)paParms[0].u.uint32;
                SHFLSTRING *pSrc        = (SHFLSTRING *)paParms[1].u.pointer.addr;
                SHFLSTRING *pDest       = (SHFLSTRING *)paParms[2].u.pointer.addr;
                uint32_t    flags       = paParms[3].u.uint32;

                /* Verify parameters values. */
                if (    !ShflStringIsValid(pSrc, paParms[1].u.pointer.size)
                    ||  !ShflStringIsValid(pDest, paParms[2].u.pointer.size)
                   )
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */
                    rc = vbsfRename (pClient, root, pSrc, pDest, flags);
                    if (RT_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        ; /* none */
                    }
                }
            }
            break;
        }

        case SHFL_FN_FLUSH:
        {
            Log(("svcCall: SHFL_FN_FLUSH\n"));

            /* Verify parameter count and types. */
            if (cParms != SHFL_CPARMS_FLUSH)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (   paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT   /* root */
                || paParms[1].type != VBOX_HGCM_SVC_PARM_64BIT   /* handle */
                    )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                SHFLROOT   root    = (SHFLROOT)paParms[0].u.uint32;
                SHFLHANDLE Handle  = paParms[1].u.uint64;

                /* Verify parameters values. */
                if (Handle == SHFL_HANDLE_ROOT)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                if (Handle == SHFL_HANDLE_NIL)
                {
                    AssertMsgFailed(("Invalid handle!!!!\n"));
                    rc = VERR_INVALID_HANDLE;
                }
                else
                {
                    /* Execute the function. */

                    rc = vbsfFlush (pClient, root, Handle);

                    if (RT_SUCCESS(rc))
                    {
                        /* Nothing to do */
                    }
                }
            }
        } break;

        case SHFL_FN_SET_UTF8:
        {
            pClient->fu32Flags |= SHFL_CF_UTF8;
            rc = VINF_SUCCESS;
            break;
        }

        default:
        {
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }
    }

    LogFlow(("svcCall: rc = %Rrc\n", rc));

    if (   !fAsynchronousProcessing
        || RT_FAILURE (rc))
    {
        /* Complete the operation if it was unsuccessful or
         * it was processed synchronously.
         */
        g_pHelpers->pfnCallComplete (callHandle, rc);
    }

    LogFlow(("\n"));        /* Add a new line to differentiate between calls more easily. */
}

/*
 * We differentiate between a function handler for the guest and one for the host. The guest is not allowed to add or remove mappings for obvious security reasons.
 */
static DECLCALLBACK(int) svcHostCall (void *, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    Log(("svcHostCall: fn = %d, cParms = %d, pparms = %d\n", u32Function, cParms, paParms));

#ifdef DEBUG
    uint32_t i;

    for (i = 0; i < cParms; i++)
    {
        /** @todo parameters other than 32 bit */
        Log(("    pparms[%d]: type %d value %d\n", i, paParms[i].type, paParms[i].u.uint32));
    }
#endif

    switch (u32Function)
    {
    case SHFL_FN_ADD_MAPPING:
    {
        Log(("svcCall: SHFL_FN_ADD_MAPPING\n"));
        LogRel(("SharedFolders host service: adding host mapping.\n"));
        LogRel(("    Host path %lS, map name %lS, writable %d\n",
                ((SHFLSTRING *)paParms[0].u.pointer.addr)->String.ucs2,
                ((SHFLSTRING *)paParms[1].u.pointer.addr)->String.ucs2,
                paParms[2].u.uint32));

        /* Verify parameter count and types. */
        if (cParms != SHFL_CPARMS_ADD_MAPPING)
        {
            rc = VERR_INVALID_PARAMETER;
        }
        else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_PTR     /* host folder name */
                 || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR     /* guest map name */
                 || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT   /* fWritable */
                )
        {
            rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            /* Fetch parameters. */
            SHFLSTRING *pFolderName = (SHFLSTRING *)paParms[0].u.pointer.addr;
            SHFLSTRING *pMapName    = (SHFLSTRING *)paParms[1].u.pointer.addr;
            uint32_t fWritable      = paParms[2].u.uint32;

            /* Verify parameters values. */
            if (    !ShflStringIsValid(pFolderName, paParms[0].u.pointer.size)
                ||  !ShflStringIsValid(pMapName, paParms[1].u.pointer.size)
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Execute the function. */
                rc = vbsfMappingsAdd (pFolderName, pMapName, fWritable);

                if (RT_SUCCESS(rc))
                {
                    /* Update parameters.*/
                    ; /* none */
                }
            }
        }
        LogRel(("SharedFolders host service: add mapping result %Rrc\n", rc));
        break;
    }

    case SHFL_FN_REMOVE_MAPPING:
    {
        Log(("svcCall: SHFL_FN_REMOVE_MAPPING\n"));
        LogRel(("SharedFolders host service: removing host mapping %lS\n",
                ((SHFLSTRING *)paParms[0].u.pointer.addr)->String.ucs2));

        /* Verify parameter count and types. */
        if (cParms != SHFL_CPARMS_REMOVE_MAPPING)
        {
            rc = VERR_INVALID_PARAMETER;
        }
        else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_PTR     /* folder name */
                )
        {
            rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            /* Fetch parameters. */
            SHFLSTRING *pString = (SHFLSTRING *)paParms[0].u.pointer.addr;

            /* Verify parameters values. */
            if (!ShflStringIsValid(pString, paParms[0].u.pointer.size))
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Execute the function. */
                rc = vbsfMappingsRemove (pString);

                if (RT_SUCCESS(rc))
                {
                    /* Update parameters.*/
                    ; /* none */
                }
            }
        }
        LogRel(("SharedFolders host service: remove mapping result %Rrc\n", rc));
        break;
    }

    case SHFL_FN_SET_STATUS_LED:
    {
        Log(("svcCall: SHFL_FN_SET_STATUS_LED\n"));

        /* Verify parameter count and types. */
        if (cParms != SHFL_CPARMS_SET_STATUS_LED)
        {
            rc = VERR_INVALID_PARAMETER;
        }
        else if (   paParms[0].type != VBOX_HGCM_SVC_PARM_PTR     /* folder name */
                )
        {
            rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            /* Fetch parameters. */
            PPDMLED  pLed     = (PPDMLED)paParms[0].u.pointer.addr;
            uint32_t cbLed    = paParms[0].u.pointer.size;

            /* Verify parameters values. */
            if (   (cbLed != sizeof (PDMLED))
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Execute the function. */
                pStatusLed = pLed;
                rc = VINF_SUCCESS;
            }
        }
        break;
    }

    default:
        rc = VERR_NOT_IMPLEMENTED;
        break;
    }

    LogFlow(("svcHostCall: rc = %Rrc\n", rc));
    return rc;
}

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad (VBOXHGCMSVCFNTABLE *ptable)
{
    int rc = VINF_SUCCESS;

    Log(("VBoxHGCMSvcLoad: ptable = %p\n", ptable));

    if (!VALID_PTR(ptable))
    {
        LogRelFunc(("Bad value of ptable (%p) in shared folders service\n", ptable));
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        Log(("VBoxHGCMSvcLoad: ptable->cbSize = %d, ptable->u32Version = 0x%08X\n", ptable->cbSize, ptable->u32Version));

        if (    ptable->cbSize != sizeof (VBOXHGCMSVCFNTABLE)
            ||  ptable->u32Version != VBOX_HGCM_SVC_VERSION)
        {
            LogRelFunc(("version mismatch loading shared folders service: ptable->cbSize = %d, should be %d, ptable->u32Version = 0x%08X, should be 0x%08X\n", ptable->cbSize, sizeof (VBOXHGCMSVCFNTABLE), ptable->u32Version, VBOX_HGCM_SVC_VERSION));
            rc = VERR_VERSION_MISMATCH;
        }
        else
        {
            g_pHelpers = ptable->pHelpers;

            ptable->cbClient = sizeof (SHFLCLIENTDATA);

            ptable->pfnUnload     = svcUnload;
            ptable->pfnConnect    = svcConnect;
            ptable->pfnDisconnect = svcDisconnect;
            ptable->pfnCall       = svcCall;
            ptable->pfnHostCall   = svcHostCall;
            ptable->pfnSaveState  = svcSaveState;
            ptable->pfnLoadState  = svcLoadState;
            ptable->pvService     = NULL;
        }

        /* Init handle table */
        rc = vbsfInitHandleTable();
        AssertRC(rc);

        vbsfMappingInit();
    }

    return rc;
}
