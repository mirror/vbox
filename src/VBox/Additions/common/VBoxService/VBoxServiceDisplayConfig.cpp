/** @file
 * VBoxService - Guest displays handling.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <iprt/win/windows.h>
#include <wtsapi32.h>

#include <iprt/errcore.h>
#include <iprt/system.h>

#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"

#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_LONGHORN
#include <iprt/win/d3dkmthk.h>

#define VBOX_WITH_WDDM
#include <VBoxDisplay.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

static PFND3DKMT_ENUMADAPTERS g_pfnD3DKMTEnumAdapters;
static PFND3DKMT_OPENADAPTERFROMLUID g_pfnD3DKMTOpenAdapterFromLuid;
static PFND3DKMT_CLOSEADAPTER g_pfnD3DKMTCloseAdapter;
static PFND3DKMT_ESCAPE g_pfnD3DKMTEscape;

static decltype(WTSFreeMemory)                 *g_pfnWTSFreeMemory = NULL;
static decltype(WTSQuerySessionInformationA)   *g_pfnWTSQuerySessionInformationA = NULL;
static decltype(WTSEnumerateSessionsA)         *g_pfnWTSEnumerateSessionsA = NULL;

/**
 * @interface_method_impl{VBOXSERVICE,pfnInit}
 */
static DECLCALLBACK(int) vgsvcDisplayConfigInit(void)
{
    if (RTSystemGetNtVersion() < RTSYSTEM_MAKE_NT_VERSION(6, 0, 0))
    {
        VGSvcVerbose(1, "displayconfig requires Windows Vista or later\n");
        return VERR_SERVICE_DISABLED;
    }

    RTLDRMOD hLdrMod;
    int rc = RTLdrLoadSystem("gdi32.dll", true /*fNoUnload*/, &hLdrMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hLdrMod, "D3DKMTEnumAdapters", (void **)&g_pfnD3DKMTEnumAdapters);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "D3DKMTOpenAdapterFromLuid", (void **)&g_pfnD3DKMTOpenAdapterFromLuid);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "D3DKMTCloseAdapter", (void **)&g_pfnD3DKMTCloseAdapter);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "D3DKMTEscape", (void **)&g_pfnD3DKMTEscape);
        AssertRC(rc);
        RTLdrClose(hLdrMod);
    }

    if (RT_FAILURE(rc))
    {
        VGSvcVerbose(1, "d3dkmthk API is not available (%Rrc)\n", rc);
        g_pfnD3DKMTEnumAdapters = NULL;
        g_pfnD3DKMTOpenAdapterFromLuid = NULL;
        g_pfnD3DKMTCloseAdapter = NULL;
        g_pfnD3DKMTEscape = NULL;
        return VERR_SERVICE_DISABLED;
    }

    rc = RTLdrLoadSystem("wtsapi32.dll", true /*fNoUnload*/, &hLdrMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hLdrMod, "WTSFreeMemory", (void **)&g_pfnWTSFreeMemory);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "WTSQuerySessionInformationA", (void **)&g_pfnWTSQuerySessionInformationA);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hLdrMod, "WTSEnumerateSessionsA", (void **)&g_pfnWTSEnumerateSessionsA);
        AssertRC(rc);
        RTLdrClose(hLdrMod);
    }

    if (RT_FAILURE(rc))
    {
        VGSvcVerbose(1, "WtsApi32.dll API is not available (%Rrc)\n", rc);
        g_pfnWTSFreeMemory = NULL;
        g_pfnWTSQuerySessionInformationA = NULL;
        g_pfnWTSEnumerateSessionsA = NULL;
        return VERR_SERVICE_DISABLED;
    }

    return VINF_SUCCESS;
}

void ReconnectDisplays(uint32_t cDisplays, VMMDevDisplayDef *paDisplays)
{
    D3DKMT_HANDLE hAdapter;
    D3DKMT_ENUMADAPTERS EnumAdapters = {0};
    NTSTATUS rcNt;
    uint32_t u32Mask = 0;

    for(uint32_t i = 0; i < cDisplays; i++)
    {
        u32Mask |= (paDisplays[i].fDisplayFlags & VMMDEV_DISPLAY_DISABLED) ? 0 : RT_BIT(i);
    }

    VGSvcVerbose(3, "ReconnectDisplays u32Mask 0x%x\n", u32Mask);

    EnumAdapters.NumAdapters = RT_ELEMENTS(EnumAdapters.Adapters);
    rcNt = g_pfnD3DKMTEnumAdapters(&EnumAdapters);

    VGSvcVerbose(3, "D3DKMTEnumAdapters  rcNt=%#x NumAdapters=%d\n", rcNt, EnumAdapters.NumAdapters);

    for(ULONG id = 0; id < EnumAdapters.NumAdapters; id++)
    {
        D3DKMT_ADAPTERINFO *pAdapterInfo = &EnumAdapters.Adapters[id];
        VGSvcVerbose(3, "#%d: NumOfSources=%d hAdapter=0x%p Luid(%u, %u)\n", id,
            pAdapterInfo->NumOfSources, pAdapterInfo->hAdapter, pAdapterInfo->AdapterLuid.HighPart, pAdapterInfo->AdapterLuid.LowPart);
    }

    D3DKMT_OPENADAPTERFROMLUID OpenAdapterData;
    RT_ZERO(OpenAdapterData);
    OpenAdapterData.AdapterLuid = EnumAdapters.Adapters[0].AdapterLuid;
    rcNt = g_pfnD3DKMTOpenAdapterFromLuid(&OpenAdapterData);
    VGSvcVerbose(3, "D3DKMTOpenAdapterFromLuid  rcNt=%#x hAdapter=0x%p\n", rcNt, OpenAdapterData.hAdapter);

    hAdapter = OpenAdapterData.hAdapter;

    if (hAdapter)
    {
        VBOXDISPIFESCAPE EscapeHdr = {0};
        EscapeHdr.escapeCode = VBOXESC_RECONNECT_TARGETS;
        EscapeHdr.u32CmdSpecific = u32Mask;

        D3DKMT_ESCAPE EscapeData = {0};
        EscapeData.hAdapter = hAdapter;
        EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
        EscapeData.Flags.HardwareAccess = 1;
        EscapeData.pPrivateDriverData = &EscapeHdr;
        EscapeData.PrivateDriverDataSize = sizeof (EscapeHdr);

        rcNt = g_pfnD3DKMTEscape(&EscapeData);
        VGSvcVerbose(3, "D3DKMTEscape rcNt=%#x\n", rcNt);

        D3DKMT_CLOSEADAPTER CloseAdapter;
        CloseAdapter.hAdapter = hAdapter;

        rcNt = g_pfnD3DKMTCloseAdapter(&CloseAdapter);
        VGSvcVerbose(3, "D3DKMTCloseAdapter rcNt=%#x\n", rcNt);
    }
}

static char* WTSSessionState2Str(WTS_CONNECTSTATE_CLASS State)
{
    switch(State)
    {
        RT_CASE_RET_STR(WTSActive);
        RT_CASE_RET_STR(WTSConnected);
        RT_CASE_RET_STR(WTSConnectQuery);
        RT_CASE_RET_STR(WTSShadow);
        RT_CASE_RET_STR(WTSDisconnected);
        RT_CASE_RET_STR(WTSIdle);
        RT_CASE_RET_STR(WTSListen);
        RT_CASE_RET_STR(WTSReset);
        RT_CASE_RET_STR(WTSDown);
        RT_CASE_RET_STR(WTSInit);
        default:
            return "Unknown";
    }
}

bool HasActiveLocalUser(void)
{
    WTS_SESSION_INFO *paSessionInfos = NULL;
    DWORD cSessionInfos = 0;
    bool fRet = false;

    if (g_pfnWTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &paSessionInfos, &cSessionInfos))
    {
        VGSvcVerbose(3, "WTSEnumerateSessionsA got %u sessions\n", cSessionInfos);

        for(DWORD i = 0; i < cSessionInfos; i++)
        {
            VGSvcVerbose(3, "WTS session[%u] SessionId (%u) pWinStationName (%s) State (%s %u)\n", i,
                paSessionInfos[i].SessionId, paSessionInfos[i].pWinStationName,
                WTSSessionState2Str(paSessionInfos[i].State), paSessionInfos[i].State);

            if (paSessionInfos[i].State == WTSActive && RTStrNICmpAscii(paSessionInfos[i].pWinStationName, RT_STR_TUPLE("Console")) == 0)
            {
                VGSvcVerbose(2, "Found active WTS session %u connected to Console\n", paSessionInfos[i].SessionId);
                fRet = true;
            }
        }
    }
    else
    {
        VGSvcError("WTSEnumerateSessionsA failed %#x\n", GetLastError());
    }

    if (paSessionInfos)
        g_pfnWTSFreeMemory(paSessionInfos);

    return fRet;
}

/**
 * @interface_method_impl{VBOXSERVICE,pfnWorker}
 */
DECLCALLBACK(int) vgsvcDisplayConfigWorker(bool volatile *pfShutdown)
{
    int rc = VINF_SUCCESS;

    /*
     * Tell the control thread that it can continue spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());
    /*
     * The Work Loop.
     */

    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    VGSvcVerbose(3, "VbglR3CtlFilterMask set rc=%Rrc\n", rc);

    for (;;)
    {
        uint32_t fEvents = 0;

        if (HasActiveLocalUser())
        {
            RTThreadSleep(1000);
        }
        else
        {
            rc = VbglR3AcquireGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0, false);
            VGSvcVerbose(3, "VbglR3AcquireGuestCaps acquire VMMDEV_GUEST_SUPPORTS_GRAPHICS rc=%Rrc\n", rc);

            rc = VbglR3WaitEvent(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 2000 /*ms*/, &fEvents);
            VGSvcVerbose(3, "VbglR3WaitEvent rc=%Rrc\n", rc);

            if (RT_SUCCESS(rc))
            {
                VMMDevDisplayDef aDisplays[64];
                uint32_t cDisplays = RT_ELEMENTS(aDisplays);

                rc = VbglR3GetDisplayChangeRequestMulti(cDisplays, &cDisplays, &aDisplays[0], true);
                VGSvcVerbose(3, "VbglR3GetDisplayChangeRequestMulti rc=%Rrc cDisplays=%d\n", rc, cDisplays);
                if (cDisplays > 0)
                {
                    for(uint32_t i = 0; i < cDisplays; i++)
                    {
                        VGSvcVerbose(2, "Display[%i] flags=%#x (%dx%d)\n", i,
                            aDisplays[i].fDisplayFlags,
                            aDisplays[i].cx, aDisplays[i].cy);
                    }

                    ReconnectDisplays(cDisplays, &aDisplays[0]);
                }
            }
            else
            {
                /* To prevent CPU throttle in case of multiple failures */
                RTThreadSleep(200);
            }

            rc = VbglR3AcquireGuestCaps(0, VMMDEV_GUEST_SUPPORTS_GRAPHICS, false);
            VGSvcVerbose(3, "VbglR3AcquireGuestCaps release VMMDEV_GUEST_SUPPORTS_GRAPHICS rc=%Rrc\n", rc);
        }

        if (*pfShutdown)
            break;
    }

    rc = VbglR3CtlFilterMask(0, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    VGSvcVerbose(3, "VbglR3CtlFilterMask cleared rc=%Rrc\n", rc);

    return rc;
}

/**
 * @interface_method_impl{VBOXSERVICE,pfnStop}
 */
static DECLCALLBACK(void) vgsvcDisplayConfigStop(void)
{
}

/**
 * @interface_method_impl{VBOXSERVICE,pfnTerm}
 */
static DECLCALLBACK(void) vgsvcDisplayConfigTerm(void)
{
}

/**
 * The 'displayconfig' service description.
 */
VBOXSERVICE g_DisplayConfig =
{
    /* pszName. */
    "displayconfig",
    /* pszDescription. */
    "Display configuration",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VGSvcDefaultPreInit,
    VGSvcDefaultOption,
    vgsvcDisplayConfigInit,
    vgsvcDisplayConfigWorker,
    vgsvcDisplayConfigStop,
    vgsvcDisplayConfigTerm
};
