/* $Id$ */

/** @file
 * VBox crOpenGL: Host service entry points.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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


#ifdef RT_OS_WINDOWS
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/stream.h>
#include <VBox/ssm.h>
#include <VBox/hgcmsvc.h>
#include <VBox/HostServices/VBoxCrOpenGLSvc.h>
#include "cr_server.h"
#define LOG_GROUP LOG_GROUP_SHARED_CROPENGL
#include <VBox/log.h>

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/EventQueue.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/com/assert.h>

#else
#include <VBox/com/VirtualBox.h>
#include <iprt/assert.h>
#include <VBox/ssm.h>
#include <VBox/hgcmsvc.h>
#include <VBox/HostServices/VBoxCrOpenGLSvc.h>

#include "cr_server.h"
#define LOG_GROUP LOG_GROUP_SHARED_CROPENGL
#include <VBox/log.h>
#endif

#include "render/renderspu.h"

PVBOXHGCMSVCHELPERS g_pHelpers;
static IFramebuffer* g_pFrameBuffer;
static ULONG64 g_winId = 0;

#ifndef RT_OS_WINDOWS
#define DWORD int
#define WINAPI
#endif

#define CR_USE_HGCM

static const char* gszVBoxOGLSSMMagic = "***OpenGL state data***";
#define SHCROGL_SSM_VERSION 3

typedef struct
{
    DWORD dwThreadID;

} VBOXOGLCTX, *PVBOXOGLCTX;

/*@todo remove this workaround for crstate "unshareable" data*/
static int crIsThreadWorking=0;

static DWORD WINAPI crServerProc(void* pv)
{
    uint64_t winId = *((uint64_t*)pv);
    renderspuSetWindowId((uint32_t)winId);
    CRServerMain(0, NULL);
    crIsThreadWorking = 0;
    return 0;
}


static DECLCALLBACK(int) svcUnload (void *)
{
    int rc = VINF_SUCCESS;

    Log(("SHARED_CROPENGL svcUnload\n"));

    //vboxglGlobalUnload();
    crVBoxServerTearDown();

    return rc;
}

static DECLCALLBACK(int) svcConnect (void *, uint32_t u32ClientID, void *pvClient)
{
    int rc = VINF_SUCCESS;

    NOREF(u32ClientID);
    VBOXOGLCTX *pClient = (VBOXOGLCTX *)pvClient;
    Assert(pClient);

    Log(("SHARED_CROPENGL svcConnect: u32ClientID = %d\n", u32ClientID));

#ifndef CR_USE_HGCM
    if (!crIsThreadWorking)
    {
        HANDLE h;
        Assert(g_pFrameBuffer);

        g_pFrameBuffer->COMGETTER(WinId)(&g_winId);
        //CHECK_ERROR_RET(g_piConsole, COMGETTER(Display)(display.asOutParam()), rc);

        //vboxglConnect((PVBOXOGLCTX)pvClient);
        crIsThreadWorking=1;
        h = CreateThread(NULL, 0, crServerProc, (void*)&g_winId, 0, &pClient->dwThreadID);
        if (!h) rc = VERR_MAX_THRDS_REACHED;
    }
    else
        rc = VERR_MAX_THRDS_REACHED;
#else
    g_pFrameBuffer->COMGETTER(WinId)(&g_winId);
    renderspuSetWindowId((uint32_t)g_winId);
    crVBoxServerAddClient(u32ClientID);
#endif

    return rc;
}

static DECLCALLBACK(int) svcDisconnect (void *, uint32_t u32ClientID, void *pvClient)
{
    int rc = VINF_SUCCESS;
    VBOXOGLCTX *pClient = (VBOXOGLCTX *)pvClient;
    Assert(pClient);

    Log(("SHARED_CROPENGL svcDisconnect: u32ClientID = %d\n", u32ClientID));

#ifndef CR_USE_HGCM
    if (crIsThreadWorking && pClient->dwThreadID)
        PostThreadMessage(pClient->dwThreadID, WM_QUIT, 0, 0);
#else
    crVBoxServerRemoveClient(u32ClientID);
#endif
    //vboxglDisconnect((PVBOXOGLCTX)pvClient);
    return rc;
}

static DECLCALLBACK(int) svcSaveState(void *, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    VBOXOGLCTX *pClient = (VBOXOGLCTX *)pvClient;

    NOREF(pClient);

    Log(("SHARED_CROPENGL svcSaveState: u32ClientID = %d\n", u32ClientID));

    int rc;

    /* Start*/
    rc = SSMR3PutStrZ(pSSM, gszVBoxOGLSSMMagic);
    AssertRCReturn(rc, rc);

    /* Version */
    rc = SSMR3PutU32(pSSM, (uint32_t) SHCROGL_SSM_VERSION);
    AssertRCReturn(rc, rc);

    /* The state itself */
    rc = crVBoxServerSaveState(pSSM);
    AssertRCReturn(rc, rc);

    /* End */
    rc = SSMR3PutStrZ(pSSM, gszVBoxOGLSSMMagic);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) svcLoadState(void *, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    VBOXOGLCTX *pClient = (VBOXOGLCTX *)pvClient;

    NOREF(pClient);
    NOREF(pSSM);

    Log(("SHARED_CROPENGL svcLoadState: u32ClientID = %d\n", u32ClientID));

    char psz[2000];
    int rc;
    uint32_t ui32;

    /* Start of data */
    rc = SSMR3GetStrZEx(pSSM, psz, 2000, NULL);
    AssertRCReturn(rc, rc);
    if (strcmp(gszVBoxOGLSSMMagic, psz))
        return VERR_SSM_UNEXPECTED_DATA;

    /* Version */
    rc = SSMR3GetU32(pSSM, &ui32);
    AssertRCReturn(rc, rc);
    if (SHCROGL_SSM_VERSION != ui32)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /* The state itself */
    rc = crVBoxServerLoadState(pSSM);
    AssertRCReturn(rc, rc);

    /* End of data */
    rc = SSMR3GetStrZEx(pSSM, psz, 2000, NULL);
    AssertRCReturn(rc, rc);
    if (strcmp(gszVBoxOGLSSMMagic, psz))
        return VERR_SSM_UNEXPECTED_DATA;

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) svcCall (void *, VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    Log(("SHARED_CROPENGL svcCall: u32ClientID = %d, fn = %d, cParms = %d, pparms = %d\n", u32ClientID, u32Function, cParms, paParms));

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
        case SHCRGL_GUEST_FN_WRITE:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_WRITE\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_WRITE)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_PTR     /* pBuffer */
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint8_t *pBuffer  = (uint8_t *)paParms[0].u.pointer.addr;
                uint32_t cbBuffer = paParms[0].u.pointer.size;

                /* Execute the function. */
                crVBoxServerClientWrite(u32ClientID, pBuffer, cbBuffer);
            }
            break;
        }

        case SHCRGL_GUEST_FN_READ:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_READ\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_READ)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_PTR     /* pBuffer */
                 || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT   /* cbBuffer */
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }

            /* Fetch parameters. */
            uint8_t *pBuffer  = (uint8_t *)paParms[0].u.pointer.addr;
            uint32_t cbBuffer = paParms[0].u.pointer.size;

            /* Execute the function. */
            rc = crVBoxServerClientRead(u32ClientID, pBuffer, &cbBuffer);

            if (RT_SUCCESS(rc))
            {
                /* Update parameters.*/
                paParms[0].u.pointer.size = cbBuffer; //@todo guest doesn't see this change somehow?
            }

            /* Return the required buffer size always */
            paParms[1].u.uint32 = cbBuffer;

            break;
        }

        case SHCRGL_GUEST_FN_WRITE_READ:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_WRITE_READ\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_WRITE_READ)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_PTR     /* pBuffer */
                 || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR     /* pWriteback */
                 || paParms[2].type != VBOX_HGCM_SVC_PARM_32BIT   /* cbWriteback */
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint8_t *pBuffer     = (uint8_t *)paParms[0].u.pointer.addr;
                uint32_t cbBuffer    = paParms[0].u.pointer.size;

                uint8_t *pWriteback  = (uint8_t *)paParms[1].u.pointer.addr;
                uint32_t cbWriteback = paParms[1].u.pointer.size;

                /* Execute the function. */
                crVBoxServerClientWrite(u32ClientID, pBuffer, cbBuffer);
                rc = crVBoxServerClientRead(u32ClientID, pWriteback, &cbWriteback);

                if (RT_SUCCESS(rc))
                {
                    /* Update parameters.*/
                    paParms[1].u.pointer.size = cbWriteback;
                }
                /* Return the required buffer size always */
                paParms[2].u.uint32 = cbWriteback;
            }
            break;
        }

        default:
        {
            rc = VERR_NOT_IMPLEMENTED;
        }
    }


    LogFlow(("svcCall: rc = %Rrc\n", rc));

    g_pHelpers->pfnCallComplete (callHandle, rc);
}

/*
 * We differentiate between a function handler for the guest and one for the host.
 */
static DECLCALLBACK(int) svcHostCall (void *, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    Log(("SHARED_CROPENGL svcHostCall: fn = %d, cParms = %d, pparms = %d\n", u32Function, cParms, paParms));

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
        case SHCRGL_HOST_FN_SET_FRAMEBUFFER:
        {
            Log(("svcCall: SHCRGL_HOST_FN_SET_FRAMEBUFFER\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_SET_FRAMEBUFFER)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (paParms[0].type != VBOX_HGCM_SVC_PARM_PTR)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                IFramebuffer* pFrameBuffer = (IFramebuffer*)paParms[0].u.pointer.addr;
                uint32_t  cbData = paParms[0].u.pointer.size;

                /* Verify parameters values. */
                if (cbData != sizeof (IFramebuffer*))
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */
                    g_pFrameBuffer = pFrameBuffer;
                    rc = VINF_SUCCESS;
                }
            }
            break;
        }
        case SHCRGL_HOST_FN_SET_VISIBLE_REGION:
        {
            Log(("svcCall: SHCRGL_HOST_FN_SET_VISIBLE_REGION\n"));

            if (cParms != SHCRGL_CPARMS_SET_VISIBLE_REGION)
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_PTR     /* pRects */
                 || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT   /* cRects */
               )
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            Assert(sizeof(RTRECT)==4*sizeof(GLint));

            renderspuSetRootVisibleRegion(paParms[1].u.uint32, (GLint*)paParms[0].u.pointer.addr);
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

    Log(("SHARED_CROPENGL VBoxHGCMSvcLoad: ptable = %p\n", ptable));

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
            ptable->pvService     = NULL;
#ifdef CR_USE_HGCM
            if (!crVBoxServerInit())
                return VERR_NOT_SUPPORTED;
#endif
        }
    }

    return rc;
}
