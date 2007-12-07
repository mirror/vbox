/** @file
 * VBox OpenGL: Host service entry points.
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

#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <VBox/ssm.h>
#define LOG_GROUP LOG_GROUP_SHARED_OPENGL
#include <VBox/log.h>

#include "vboxgl.h"
#include "gldrv.h"


PVBOXHGCMSVCHELPERS g_pHelpers;


static DECLCALLBACK(int) svcUnload (void)
{
    int rc = VINF_SUCCESS;

    Log(("svcUnload\n"));

    vboxglGlobalUnload();
    return rc;
}

static DECLCALLBACK(int) svcConnect (uint32_t u32ClientID, void *pvClient)
{
    int rc = VINF_SUCCESS;

    NOREF(u32ClientID);
    NOREF(pvClient);

    Log(("svcConnect: u32ClientID = %d\n", u32ClientID));

    vboxglConnect((PVBOXOGLCTX)pvClient);
    return rc;
}

static DECLCALLBACK(int) svcDisconnect (uint32_t u32ClientID, void *pvClient)
{
    int rc = VINF_SUCCESS;
    VBOXOGLCTX *pClient = (VBOXOGLCTX *)pvClient;

    NOREF(pClient);

    Log(("svcDisconnect: u32ClientID = %d\n", u32ClientID));
    vboxglDisconnect((PVBOXOGLCTX)pvClient);
    return rc;
}

/**
 * We can't save the OpenGL state, so there's not much to do. Perhaps we should invalidate the client id? 
 */
static DECLCALLBACK(int) svcSaveState(uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    VBOXOGLCTX *pClient = (VBOXOGLCTX *)pvClient;

    NOREF(pClient);
    NOREF(pSSM);

    Log(("svcSaveState: u32ClientID = %d\n", u32ClientID));
    
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) svcLoadState(uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    VBOXOGLCTX *pClient = (VBOXOGLCTX *)pvClient;

    NOREF(pClient);
    NOREF(pSSM);

    Log(("svcLoadState: u32ClientID = %d\n", u32ClientID));

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) svcCall (VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    Log(("svcCall: u32ClientID = %d, fn = %d, cParms = %d, pparms = %d\n", u32ClientID, u32Function, cParms, paParms));

    VBOXOGLCTX *pClient = (VBOXOGLCTX *)pvClient;

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
        case VBOXOGL_FN_GLGETSTRING:
        {
            Log(("svcCall: VBOXOGL_FN_GLGETSTRING\n"));

            /* Verify parameter count and types. */
            if (cParms != VBOXOGL_CPARMS_GLGETSTRING)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else 
            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT     /* name */
                ||  paParms[1].type != VBOX_HGCM_SVC_PARM_PTR       /* string */
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint32_t name       = paParms[0].u.uint32;
                char    *pString    = (char *)paParms[1].u.pointer.addr;
                uint32_t cbString   = paParms[1].u.pointer.size;

                /* Verify parameters values. */
                if (   (cbString < 32)
                   )
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */
                    rc = vboxglGetString(pClient, name, pString, &cbString);

                    if (VBOX_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                        paParms[1].u.pointer.size = cbString;
                    }
                }
            }
            break;
        } 

        case VBOXOGL_FN_GLFLUSH:
        {
            Log(("svcCall: VBOXOGL_FN_GLFLUSH\n"));

            /* Verify parameter count and types. */
            if (cParms != VBOXOGL_CPARMS_GLFLUSH)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else 
            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_PTR       /* pCmdBuffer */
                ||  paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT     /* cCommands */
                ||  paParms[2].type != VBOX_HGCM_SVC_PARM_64BIT     /* retval */
                ||  paParms[3].type != VBOX_HGCM_SVC_PARM_32BIT     /* lasterror */
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint8_t *pCmdBuffer  = (uint8_t *)paParms[0].u.pointer.addr;
                uint32_t cbCmdBuffer = paParms[0].u.pointer.size;
                uint32_t cCommands   = paParms[1].u.uint32;
                GLenum   lasterror;
                uint64_t lastretval;

                /* Execute the function. */
                rc = vboxglFlushBuffer(pClient, pCmdBuffer, cbCmdBuffer, cCommands, &lasterror, &lastretval);

                if (VBOX_SUCCESS(rc))
                {
                    /* Update parameters.*/
                    paParms[2].u.uint64 = lastretval;
                    paParms[3].u.uint32 = lasterror;
                }
            }
            break;
        } 

        case VBOXOGL_FN_GLFLUSHPTR:
        {
            Log(("svcCall: VBOXOGL_FN_GLFLUSHPTR\n"));

            /* Verify parameter count and types. */
            if (cParms != VBOXOGL_CPARMS_GLFLUSHPTR)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else 
            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_PTR       /* pCmdBuffer */
                ||  paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT     /* cCommands */
                ||  (    paParms[2].type != VBOX_HGCM_SVC_PARM_PTR       /* pLastParam */
                     &&  paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT)    /* pLastParam if NULL */
                ||  paParms[3].type != VBOX_HGCM_SVC_PARM_64BIT     /* retval */
                ||  paParms[4].type != VBOX_HGCM_SVC_PARM_32BIT     /* lasterror */
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint8_t *pCmdBuffer  = (uint8_t *)paParms[0].u.pointer.addr;
                uint32_t cbCmdBuffer = paParms[0].u.pointer.size;
                uint32_t cCommands   = paParms[1].u.uint32;
                GLenum   lasterror;
                uint64_t lastretval;

                /* Save the last parameter of the last command in the client structure so the macro can pick it up there */
                if (paParms[2].type == VBOX_HGCM_SVC_PARM_32BIT)
                {
                    /* HGCM doesn't like NULL pointers. */
                    pClient->pLastParam  = NULL;
                    pClient->cbLastParam = 0;
                }
                else
                {
                    pClient->pLastParam  = (uint8_t *)paParms[2].u.pointer.addr;
                    pClient->cbLastParam = paParms[2].u.pointer.size;
                }

                /* Execute the function. */
                rc = vboxglFlushBuffer(pClient, pCmdBuffer, cbCmdBuffer, cCommands, &lasterror, &lastretval);

                /* Clear last parameter info again */
                pClient->pLastParam  = 0;
                pClient->cbLastParam = 0;

                if (VBOX_SUCCESS(rc))
                {
                    /* Update parameters.*/
                    paParms[3].u.uint64 = lastretval;
                    paParms[4].u.uint32 = lasterror;
                }
            }
            break;
        } 

        case VBOXOGL_FN_GLCHECKEXT:
        {
            Log(("svcCall: VBOXOGL_FN_GLCHECKEXT\n"));

            /* Verify parameter count and types. */
            if (cParms != VBOXOGL_CPARMS_GLCHECKEXT)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else 
            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_PTR       /* pszExtFnName */
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                char    *pszExtFnName = (char *)paParms[0].u.pointer.addr;
                uint32_t cbExtFnName  = paParms[0].u.pointer.size; /* size including null terminator */

                /* sanity checks */
                if (    cbExtFnName > 256
                    ||  pszExtFnName[cbExtFnName-1] != 0
                   )
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
#ifdef RT_OS_WINDOWS
                    /* Execute the function. */
                    if (vboxwglGetProcAddress(pszExtFnName))
                        rc = VINF_SUCCESS;
                    else
                        rc = VERR_FILE_NOT_FOUND;
#else
                        rc = VERR_FILE_NOT_FOUND;
#endif
                    if (VBOX_SUCCESS(rc))
                    {
                        /* Update parameters.*/
                    }
                }
            }
            break;
        } 

        default:
        {
            rc = VERR_NOT_IMPLEMENTED;
        }
    }

    LogFlow(("svcCall: rc = %Vrc\n", rc));

    g_pHelpers->pfnCallComplete (callHandle, rc);
}

/*
 * We differentiate between a function handler for the guest and one for the host. The guest is not allowed to add or remove mappings for obvious security reasons.
 */
static DECLCALLBACK(int) svcHostCall (uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
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
    default:
        rc = VERR_NOT_IMPLEMENTED;
        break;
    }

    LogFlow(("svcHostCall: rc = %Vrc\n", rc));
    return rc;
}

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad (VBOXHGCMSVCFNTABLE *ptable)
{
    int rc = VINF_SUCCESS;

    Log(("VBoxHGCMSvcLoad: ptable = %p\n", ptable));

    if (!ptable)
    {
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        Log(("VBoxHGCMSvcLoad: ptable->cbSize = %d, ptable->u32Version = 0x%08X\n", ptable->cbSize, ptable->u32Version));

        if (    ptable->cbSize != sizeof (VBOXHGCMSVCFNTABLE)
            ||  ptable->u32Version != VBOX_HGCM_SVC_VERSION)
        {
            rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            g_pHelpers = ptable->pHelpers;

            ptable->cbClient = sizeof (VBOXOGLCTX);

            ptable->pfnUnload     = svcUnload;
            ptable->pfnConnect    = svcConnect;
            ptable->pfnDisconnect = svcDisconnect;
            ptable->pfnCall       = svcCall;
            ptable->pfnHostCall   = svcHostCall;
            ptable->pfnSaveState  = svcSaveState;
            ptable->pfnLoadState  = svcLoadState;

            vboxglGlobalInit();
        }
    }

    return rc;
}
