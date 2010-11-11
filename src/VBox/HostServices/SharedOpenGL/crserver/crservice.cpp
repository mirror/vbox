/* $Id$ */

/** @file
 * VBox crOpenGL: Host service entry points.
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define __STDC_CONSTANT_MACROS  /* needed for a definition in iprt/string.h */

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
#include <VBox/com/ErrorInfo.h>
#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_CRHGSMI
#include <VBox/VBoxVideo.h>
#endif

#include <VBox/com/errorprint.h>

PVBOXHGCMSVCHELPERS g_pHelpers;
static IConsole* g_pConsole = NULL;
static PVM g_pVM = NULL;
#ifdef VBOX_WITH_CRHGSMI
static uint8_t* g_pvVRamBase;
#endif

#ifndef RT_OS_WINDOWS
#define DWORD int
#define WINAPI
#endif

static const char* gszVBoxOGLSSMMagic = "***OpenGL state data***";

static DECLCALLBACK(int) svcUnload (void *)
{
    int rc = VINF_SUCCESS;

    Log(("SHARED_CROPENGL svcUnload\n"));

    crVBoxServerTearDown();

    return rc;
}

static DECLCALLBACK(int) svcConnect (void *, uint32_t u32ClientID, void *pvClient)
{
    int rc = VINF_SUCCESS;

    NOREF(pvClient);

    Log(("SHARED_CROPENGL svcConnect: u32ClientID = %d\n", u32ClientID));

    rc = crVBoxServerAddClient(u32ClientID);

    return rc;
}

static DECLCALLBACK(int) svcDisconnect (void *, uint32_t u32ClientID, void *pvClient)
{
    int rc = VINF_SUCCESS;

    NOREF(pvClient);

    Log(("SHARED_CROPENGL svcDisconnect: u32ClientID = %d\n", u32ClientID));

    crVBoxServerRemoveClient(u32ClientID);

    return rc;
}

static DECLCALLBACK(int) svcSaveState(void *, uint32_t u32ClientID, void *pvClient, PSSMHANDLE pSSM)
{
    int rc = VINF_SUCCESS;

    NOREF(pvClient);

    Log(("SHARED_CROPENGL svcSaveState: u32ClientID = %d\n", u32ClientID));

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
    int rc = VINF_SUCCESS;

    NOREF(pvClient);

    Log(("SHARED_CROPENGL svcLoadState: u32ClientID = %d\n", u32ClientID));

    char psz[2000];
    uint32_t ui32;

    /* Start of data */
    rc = SSMR3GetStrZEx(pSSM, psz, 2000, NULL);
    AssertRCReturn(rc, rc);
    if (strcmp(gszVBoxOGLSSMMagic, psz))
        return VERR_SSM_UNEXPECTED_DATA;

    /* Version */
    rc = SSMR3GetU32(pSSM, &ui32);
    AssertRCReturn(rc, rc);

    /* The state itself */
    rc = crVBoxServerLoadState(pSSM, ui32);

    if (rc==VERR_SSM_DATA_UNIT_FORMAT_CHANGED && ui32!=SHCROGL_SSM_VERSION)
    {
        LogRel(("SHARED_CROPENGL svcLoadState: unsupported save state version %d\n", ui32));

        /*@todo ugly hack, as we don't know size of stored opengl data try to read untill end of opengl data marker*/
        /*VboxSharedCrOpenGL isn't last hgcm service now, so can't use SSMR3SkipToEndOfUnit*/
        {
            const char *pMatch = &gszVBoxOGLSSMMagic[0];
            char current;

            while (*pMatch)
            {
                rc = SSMR3GetS8(pSSM, (int8_t*)&current);
                AssertRCReturn(rc, rc);

                if (current==*pMatch)
                {
                    pMatch++;
                }
                else
                {
                    pMatch = &gszVBoxOGLSSMMagic[0];
                }
            }
        }

        return VINF_SUCCESS;
    }
    AssertRCReturn(rc, rc);

    /* End of data */
    rc = SSMR3GetStrZEx(pSSM, psz, 2000, NULL);
    AssertRCReturn(rc, rc);
    if (strcmp(gszVBoxOGLSSMMagic, psz))
        return VERR_SSM_UNEXPECTED_DATA;

    return VINF_SUCCESS;
}

static void svcClientVersionUnsupported(uint32_t minor, uint32_t major)
{
    LogRel(("SHARED_CROPENGL: unsupported client version %d.%d\n", minor, major));

    /*MS's opengl32 tries to load our ICD around 30 times on failure...this is to prevent unnecessary spam*/
    static int shown = 0;

    if (g_pVM && !shown)
    {
        VMSetRuntimeError(g_pVM, VMSETRTERR_FLAGS_NO_WAIT, "3DSupportIncompatibleAdditions",
        "An attempt by the virtual machine to use hardware 3D acceleration failed. "
        "The version of the Guest Additions installed in the virtual machine does not match the "
        "version of VirtualBox on the host. Please install appropriate Guest Additions to fix this issue");
        shown = 1;
    }
}

static DECLCALLBACK(void) svcPresentFBO(void *data, int32_t screenId, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
#if 0
    ComPtr<IDisplay> pDisplay;
    BYTE *data;
    int i,j;
    CHECK_ERROR(g_pConsole, COMGETTER(Display)(pDisplay.asOutParam()));

    data = (BYTE*) RTMemTmpAllocZ(100*100*4);

    for (i=0; i<100; i+=2)
    {
        for (j=0; j<100; ++j)
        {
            *(data+i*100*4+j*4+0) = 0xFF;
            *(data+i*100*4+j*4+1) = 0xFF;
            *(data+i*100*4+j*4+2) = 0xFF;
            *(data+i*100*4+j*4+3) = 0xFF;
        }
    }

    CHECK_ERROR(pDisplay, DrawToScreen(screenId, data, 0, 0, 100, 100));

    RTMemTmpFree(data);
#endif
    HRESULT rc;
    ComPtr<IDisplay> pDisplay;

    CHECK_ERROR(g_pConsole, COMGETTER(Display)(pDisplay.asOutParam()));
    CHECK_ERROR(pDisplay, DrawToScreen(screenId, (BYTE*)data, x, y, w, h));
}

static DECLCALLBACK(void) svcCall (void *, VBOXHGCMCALLHANDLE callHandle, uint32_t u32ClientID, void *pvClient, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    int rc = VINF_SUCCESS;

    NOREF(pvClient);

    Log(("SHARED_CROPENGL svcCall: u32ClientID = %d, fn = %d, cParms = %d, pparms = %d\n", u32ClientID, u32Function, cParms, paParms));

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
                rc = crVBoxServerClientWrite(u32ClientID, pBuffer, cbBuffer);
                if (!RT_SUCCESS(rc))
                {
                    Assert(VERR_NOT_SUPPORTED==rc);
                    svcClientVersionUnsupported(0, 0);
                }

            }
            break;
        }

        case SHCRGL_GUEST_FN_INJECT:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_INJECT\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_INJECT)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT /* u32ClientID */
                 || paParms[1].type != VBOX_HGCM_SVC_PARM_PTR   /* pBuffer */
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint32_t u32InjectClientID = paParms[0].u.uint32;
                uint8_t *pBuffer  = (uint8_t *)paParms[1].u.pointer.addr;
                uint32_t cbBuffer = paParms[1].u.pointer.size;

                /* Execute the function. */
                rc = crVBoxServerClientWrite(u32InjectClientID, pBuffer, cbBuffer);
                if (!RT_SUCCESS(rc))
                {
                    if (VERR_NOT_SUPPORTED==rc)
                    {
                        svcClientVersionUnsupported(0, 0);
                    }
                    else
                    {
                        crWarning("SHCRGL_GUEST_FN_INJECT failed to inject for %i from %i", u32InjectClientID, u32ClientID);
                    }
                }
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
            } else if (VERR_NOT_SUPPORTED==rc)
            {
                svcClientVersionUnsupported(0, 0);
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
                rc = crVBoxServerClientWrite(u32ClientID, pBuffer, cbBuffer);
                if (!RT_SUCCESS(rc))
                {
                    Assert(VERR_NOT_SUPPORTED==rc);
                    svcClientVersionUnsupported(0, 0);
                }

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

        case SHCRGL_GUEST_FN_SET_VERSION:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_SET_VERSION\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_SET_VERSION)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (    paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT     /* vMajor */
                 || paParms[1].type != VBOX_HGCM_SVC_PARM_32BIT     /* vMinor */
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint32_t vMajor    = paParms[0].u.uint32;
                uint32_t vMinor    = paParms[1].u.uint32;

                /* Execute the function. */
                rc = crVBoxServerClientSetVersion(u32ClientID, vMajor, vMinor);

                if (!RT_SUCCESS(rc))
                {
                    svcClientVersionUnsupported(vMajor, vMinor);
                }
            }

            break;
        }

        case SHCRGL_GUEST_FN_SET_PID:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_SET_PID\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_SET_PID)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            if (paParms[0].type != VBOX_HGCM_SVC_PARM_64BIT)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint64_t pid    = paParms[0].u.uint64;

                /* Execute the function. */
                rc = crVBoxServerClientSetPID(u32ClientID, pid);
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

#ifdef VBOX_WITH_CRHGSMI
static int vboxCrHgsmiCtl(PVBOXVDMACMD_CHROMIUM_CTL pCtl)
{
    int rc;

    switch (pCtl->enmType)
    {
        case VBOXVDMACMD_CHROMIUM_CTL_TYPE_CRHGSMI_SETUP:
        {
            PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP pSetup = (PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP)pCtl;
            g_pvVRamBase = (uint8_t*)pSetup->pvRamBase;
            rc = VINF_SUCCESS;
        } break;
        default:
            Assert(0);
            rc = VERR_INVALID_PARAMETER;
    }

    return rc;
}

#define VBOXCRHGSMI_PTR(_off, _t) ((_t*)(g_pvVRamBase + (_off)))
static int vboxCrHgsmiCmd(PVBOXVDMACMD_CHROMIUM_CMD pCmd)
{
    int rc;
    uint32_t cBuffers = pCmd->cBuffers;
    uint32_t cParams;

    if (!g_pvVRamBase)
    {
        Assert(0);
        return VERR_INVALID_STATE;
    }

    if (!cBuffers)
    {
        Assert(0);
        return VERR_INVALID_PARAMETER;
    }

    cParams = cBuffers-1;

    CRVBOXHGSMIHDR *pHdr = VBOXCRHGSMI_PTR(pCmd->aBuffers[0].offBuffer, CRVBOXHGSMIHDR);
    uint32_t u32Function = pHdr->u32Function;
    uint32_t u32ClientID = pHdr->u32ClientID;
    /* now we compile HGCM params out of HGSMI
     * @todo: can we avoid this ? */
    switch (u32Function)
    {

        case SHCRGL_GUEST_FN_WRITE:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_WRITE\n"));

            CRVBOXHGSMIWRITE* pFnCmd = (CRVBOXHGSMIWRITE*)pHdr;

            /* @todo: Verify  */
            if (cParams == 1)
            {
                VBOXVDMACMD_CHROMIUM_BUFFER *pBuf = &pCmd->aBuffers[1];
                /* Fetch parameters. */
                uint8_t *pBuffer  = VBOXCRHGSMI_PTR(pBuf->offBuffer, uint8_t);
                uint32_t cbBuffer = pBuf->cbBuffer;

                /* Execute the function. */
                rc = crVBoxServerClientWrite(u32ClientID, pBuffer, cbBuffer);
                if (!RT_SUCCESS(rc))
                {
                    Assert(VERR_NOT_SUPPORTED==rc);
                    svcClientVersionUnsupported(0, 0);
                }
            }
            else
            {
                Assert(0);
                rc = VERR_INVALID_PARAMETER;
            }
            break;
        }

        case SHCRGL_GUEST_FN_INJECT:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_INJECT\n"));

            CRVBOXHGSMIINJECT *pFnCmd = (CRVBOXHGSMIINJECT*)pHdr;

            /* @todo: Verify  */
            if (cParams == 1)
            {
                /* Fetch parameters. */
                uint32_t u32InjectClientID = pFnCmd->u32ClientID;
                VBOXVDMACMD_CHROMIUM_BUFFER *pBuf = &pCmd->aBuffers[1];
                uint8_t *pBuffer  = VBOXCRHGSMI_PTR(pBuf->offBuffer, uint8_t);
                uint32_t cbBuffer = pBuf->cbBuffer;

                /* Execute the function. */
                rc = crVBoxServerClientWrite(u32InjectClientID, pBuffer, cbBuffer);
                if (!RT_SUCCESS(rc))
                {
                    if (VERR_NOT_SUPPORTED==rc)
                    {
                        svcClientVersionUnsupported(0, 0);
                    }
                    else
                    {
                        crWarning("SHCRGL_GUEST_FN_INJECT failed to inject for %i from %i", u32InjectClientID, u32ClientID);
                    }
                }
            }
            else
            {
                Assert(0);
                rc = VERR_INVALID_PARAMETER;
            }
            break;
        }

        case SHCRGL_GUEST_FN_READ:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_READ\n"));

            /* @todo: Verify  */
            if (cParams == 1)
            {
                CRVBOXHGSMIREAD *pFnCmd = (CRVBOXHGSMIREAD*)pHdr;
                VBOXVDMACMD_CHROMIUM_BUFFER *pBuf = &pCmd->aBuffers[1];
                /* Fetch parameters. */
                uint8_t *pBuffer  = VBOXCRHGSMI_PTR(pBuf->offBuffer, uint8_t);
                uint32_t cbBuffer = pBuf->cbBuffer;

                /* Execute the function. */
                rc = crVBoxServerClientRead(u32ClientID, pBuffer, &cbBuffer);

                if (RT_SUCCESS(rc))
                {
                    /* Update parameters.*/
//                    paParms[0].u.pointer.size = cbBuffer; //@todo guest doesn't see this change somehow?
                } else if (VERR_NOT_SUPPORTED==rc)
                {
                    svcClientVersionUnsupported(0, 0);
                }

                /* Return the required buffer size always */
                pFnCmd->cbBuffer = cbBuffer;
            }
            else
            {
                Assert(0);
                rc = VERR_INVALID_PARAMETER;
            }

            break;
        }

        case SHCRGL_GUEST_FN_WRITE_READ:
        {
            Log(("svcCall: SHCRGL_GUEST_FN_WRITE_READ\n"));

            /* @todo: Verify  */
            if (cParams == 2)
            {
                CRVBOXHGSMIWRITEREAD *pFnCmd = (CRVBOXHGSMIWRITEREAD*)pHdr;
                VBOXVDMACMD_CHROMIUM_BUFFER *pBuf = &pCmd->aBuffers[1];
                VBOXVDMACMD_CHROMIUM_BUFFER *pWbBuf = &pCmd->aBuffers[2];

                /* Fetch parameters. */
                uint8_t *pBuffer  = VBOXCRHGSMI_PTR(pBuf->offBuffer, uint8_t);
                uint32_t cbBuffer = pBuf->cbBuffer;

                uint8_t *pWriteback  = VBOXCRHGSMI_PTR(pWbBuf->offBuffer, uint8_t);
                uint32_t cbWriteback = pWbBuf->cbBuffer;

                /* Execute the function. */
                rc = crVBoxServerClientWrite(u32ClientID, pBuffer, cbBuffer);
                if (!RT_SUCCESS(rc))
                {
                    Assert(VERR_NOT_SUPPORTED==rc);
                    svcClientVersionUnsupported(0, 0);
                }

                rc = crVBoxServerClientRead(u32ClientID, pWriteback, &cbWriteback);

//                if (RT_SUCCESS(rc))
//                {
//                    /* Update parameters.*/
//                    paParms[1].u.pointer.size = cbWriteback;
//                }
                /* Return the required buffer size always */
                pFnCmd->cbWriteback = cbWriteback;
            }
            else
            {
                Assert(0);
                rc = VERR_INVALID_PARAMETER;
            }

            break;
        }

        case SHCRGL_GUEST_FN_SET_VERSION:
        {
            Assert(0);
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }

        case SHCRGL_GUEST_FN_SET_PID:
        {
            Assert(0);
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }

        default:
        {
            Assert(0);
            rc = VERR_NOT_IMPLEMENTED;
        }

    }

    pHdr->result = rc;

    return VINF_SUCCESS;
}
#endif

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
#ifdef VBOX_WITH_CRHGSMI
        case SHCRGL_HOST_FN_CRHGSMI_CMD:
        {
            Assert(cParms == 1 && paParms[0].type == VBOX_HGCM_SVC_PARM_PTR);
            if (cParms == 1 && paParms[0].type == VBOX_HGCM_SVC_PARM_PTR)
                rc = vboxCrHgsmiCmd((PVBOXVDMACMD_CHROMIUM_CMD)paParms[0].u.pointer.addr);
            else
                rc = VERR_INVALID_PARAMETER;
        } break;
        case SHCRGL_HOST_FN_CRHGSMI_CTL:
        {
            Assert(cParms == 1 && paParms[0].type == VBOX_HGCM_SVC_PARM_PTR);
            if (cParms == 1 && paParms[0].type == VBOX_HGCM_SVC_PARM_PTR)
                rc = vboxCrHgsmiCtl((PVBOXVDMACMD_CHROMIUM_CTL)paParms[0].u.pointer.addr);
            else
                rc = VERR_INVALID_PARAMETER;
        } break;
#endif
        case SHCRGL_HOST_FN_SET_CONSOLE:
        {
            Log(("svcCall: SHCRGL_HOST_FN_SET_DISPLAY\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_SET_CONSOLE)
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
                IConsole* pConsole = (IConsole*)paParms[0].u.pointer.addr;
                uint32_t  cbData = paParms[0].u.pointer.size;

                /* Verify parameters values. */
                if (cbData != sizeof (IConsole*))
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else if (!pConsole)
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else /* Execute the function. */
                {
                    ComPtr<IMachine> pMachine;
                    ComPtr<IDisplay> pDisplay;
                    ComPtr<IFramebuffer> pFramebuffer;
                    LONG xo, yo;
                    LONG64 winId = 0;
                    ULONG monitorCount, i, w, h;

                    CHECK_ERROR_BREAK(pConsole, COMGETTER(Machine)(pMachine.asOutParam()));
                    CHECK_ERROR_BREAK(pMachine, COMGETTER(MonitorCount)(&monitorCount));
                    CHECK_ERROR_BREAK(pConsole, COMGETTER(Display)(pDisplay.asOutParam()));

                    rc = crVBoxServerSetScreenCount(monitorCount);
                    AssertRCReturn(rc, rc);

                    for (i=0; i<monitorCount; ++i)
                    {
                        CHECK_ERROR_RET(pDisplay, GetFramebuffer(i, pFramebuffer.asOutParam(), &xo, &yo), rc);

                        if (!pDisplay)
                        {
                            rc = crVBoxServerUnmapScreen(i);
                            AssertRCReturn(rc, rc);
                        }
                        else
                        {
                            CHECK_ERROR_RET(pFramebuffer, COMGETTER(WinId)(&winId), rc);
                            CHECK_ERROR_RET(pFramebuffer, COMGETTER(Width)(&w), rc);
                            CHECK_ERROR_RET(pFramebuffer, COMGETTER(Height)(&h), rc);

                            rc = crVBoxServerMapScreen(i, xo, yo, w, h, winId);
                            AssertRCReturn(rc, rc);
                        }
                    }

                    g_pConsole = pConsole;
                    rc = VINF_SUCCESS;
                }
            }
            break;
        }
        case SHCRGL_HOST_FN_SET_VM:
        {
            Log(("svcCall: SHCRGL_HOST_FN_SET_VM\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_SET_VM)
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
                PVM pVM = (PVM)paParms[0].u.pointer.addr;
                uint32_t  cbData = paParms[0].u.pointer.size;

                /* Verify parameters values. */
                if (cbData != sizeof (PVM))
                {
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* Execute the function. */
                    g_pVM = pVM;
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

            rc = crVBoxServerSetRootVisibleRegion(paParms[1].u.uint32, (GLint*)paParms[0].u.pointer.addr);
            break;
        }
        case SHCRGL_HOST_FN_SCREEN_CHANGED:
        {
            Log(("svcCall: SHCRGL_HOST_FN_SCREEN_CHANGED\n"));

            /* Verify parameter count and types. */
            if (cParms != SHCRGL_CPARMS_SCREEN_CHANGED)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else if (paParms[0].type != VBOX_HGCM_SVC_PARM_32BIT)
            {
                rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                /* Fetch parameters. */
                uint32_t screenId = paParms[0].u.uint32;

                /* Execute the function. */
                ComPtr<IDisplay> pDisplay;
                ComPtr<IFramebuffer> pFramebuffer;
                LONG xo, yo;
                LONG64 winId = 0;
                ULONG w, h;

                Assert(g_pConsole);
                CHECK_ERROR_RET(g_pConsole, COMGETTER(Display)(pDisplay.asOutParam()), rc);
                CHECK_ERROR_RET(pDisplay, GetFramebuffer(screenId, pFramebuffer.asOutParam(), &xo, &yo), rc);

                if (!pFramebuffer)
                {
                    rc = crVBoxServerUnmapScreen(screenId);
                    AssertRCReturn(rc, rc);
                }
                else
                {
                    CHECK_ERROR_RET(pFramebuffer, COMGETTER(WinId)(&winId), rc);
                    CHECK_ERROR_RET(pFramebuffer, COMGETTER(Width)(&w), rc);
                    CHECK_ERROR_RET(pFramebuffer, COMGETTER(Height)(&h), rc);

                    rc = crVBoxServerMapScreen(screenId, xo, yo, w, h, winId);
                    AssertRCReturn(rc, rc);
                }

                rc = VINF_SUCCESS;
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

            ptable->cbClient = sizeof (void*);

            ptable->pfnUnload     = svcUnload;
            ptable->pfnConnect    = svcConnect;
            ptable->pfnDisconnect = svcDisconnect;
            ptable->pfnCall       = svcCall;
            ptable->pfnHostCall   = svcHostCall;
            ptable->pfnSaveState  = svcSaveState;
            ptable->pfnLoadState  = svcLoadState;
            ptable->pvService     = NULL;

            if (!crVBoxServerInit())
                return VERR_NOT_SUPPORTED;

            crVBoxServerSetPresentFBOCB(svcPresentFBO);
        }
    }

    return rc;
}

