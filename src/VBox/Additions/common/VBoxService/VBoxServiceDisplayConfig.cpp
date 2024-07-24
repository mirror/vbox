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

#include <iprt/errcore.h>

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

/**
 * @interface_method_impl{VBOXSERVICE,pfnInit}
 */
static DECLCALLBACK(int) vgsvcDisplayConfigInit(void)
{
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

    VGSvcVerbose(2, "DXGK d3dkmthk callbacks are %s\n", RT_SUCCESS(rc) ? "Ok" : "Fail");

    return VINF_SUCCESS;
}

void ReplugDisplays(uint32_t cDisplays, VMMDevDisplayDef *paDisplays)
{
    RT_NOREF2(cDisplays, paDisplays);
    D3DKMT_HANDLE hAdapter;
    D3DKMT_ENUMADAPTERS EnumAdapters = {0};
    NTSTATUS rcNt;
    ULONG id = 0;

    EnumAdapters.NumAdapters = RT_ELEMENTS(EnumAdapters.Adapters);
    rcNt = g_pfnD3DKMTEnumAdapters(&EnumAdapters);

    VGSvcVerbose(2, "D3DKMTEnumAdapters  rcNt=%#x NumAdapters=%d\n", rcNt, EnumAdapters.NumAdapters);

    for(id = 0; id < EnumAdapters.NumAdapters; id++)
    {
        D3DKMT_ADAPTERINFO *pAdapterInfo = &EnumAdapters.Adapters[id];
        VGSvcVerbose(2, "#%d: NumOfSources=%d hAdapter=0x%p Luid(%u, %u)\n", id,
            pAdapterInfo->NumOfSources, pAdapterInfo->hAdapter, pAdapterInfo->AdapterLuid.HighPart, pAdapterInfo->AdapterLuid.LowPart);
    }

    D3DKMT_OPENADAPTERFROMLUID OpenAdapterData;
    RT_ZERO(OpenAdapterData);
    OpenAdapterData.AdapterLuid = EnumAdapters.Adapters[0].AdapterLuid;
    rcNt = g_pfnD3DKMTOpenAdapterFromLuid(&OpenAdapterData);
    VGSvcVerbose(2, "D3DKMTOpenAdapterFromLuid  rcNt=%#x hAdapter=0x%p\n", rcNt, OpenAdapterData.hAdapter);

    hAdapter = OpenAdapterData.hAdapter;

    if (hAdapter)
    {
        VBOXDISPIFESCAPE EscapeHdr = {0};
        EscapeHdr.escapeCode = VBOXESC_CONFIGURETARGETS;
        EscapeHdr.u32CmdSpecific = 0;

        D3DKMT_ESCAPE EscapeData = {0};
        EscapeData.hAdapter = hAdapter;
        EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
        EscapeData.Flags.HardwareAccess = 1;
        EscapeData.pPrivateDriverData = &EscapeHdr;
        EscapeData.PrivateDriverDataSize = sizeof (EscapeHdr);

        rcNt = g_pfnD3DKMTEscape(&EscapeData);
        VGSvcVerbose(2, "D3DKMTEscape rcNt=%#x\n", rcNt);

        D3DKMT_CLOSEADAPTER CloseAdapter;
        CloseAdapter.hAdapter = hAdapter;

        rcNt = g_pfnD3DKMTCloseAdapter(&CloseAdapter);
        VGSvcVerbose(2, "D3DKMTCloseAdapter rcNt=%#x\n", rcNt);
    }
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
    VGSvcVerbose(2, "VbglR3CtlFilterMask set rc=%Rrc\n", rc);

    for (;;)
    {
        uint32_t fEvents = 0;

        rc = VbglR3AcquireGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0, false);
        VGSvcVerbose(2, "VbglR3AcquireGuestCaps acquire VMMDEV_GUEST_SUPPORTS_GRAPHICS rc=%Rrc\n", rc);

        rc = VbglR3WaitEvent(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 1000 /*ms*/, &fEvents);
        VGSvcVerbose(2, "VbglR3WaitEvent rc=%Rrc\n", rc);

        if (RT_SUCCESS(rc))
        {
            VMMDevDisplayDef aDisplays[64];
            uint32_t cDisplays = RT_ELEMENTS(aDisplays);

            rc = VbglR3GetDisplayChangeRequestMulti(cDisplays, &cDisplays, &aDisplays[0], true);
            VGSvcVerbose(2, "VbglR3GetDisplayChangeRequestMulti rc=%Rrc cDisplays=%d\n", rc, cDisplays);
            if (cDisplays > 0)
            {
                ReplugDisplays(cDisplays, &aDisplays[0]);
                VGSvcVerbose(2, "Display[0] (%dx%d)\n", aDisplays[0].cx, aDisplays[0].cy);
            }
        }

        rc = VbglR3AcquireGuestCaps(0, VMMDEV_GUEST_SUPPORTS_GRAPHICS, false);
        VGSvcVerbose(2, "VbglR3AcquireGuestCaps release VMMDEV_GUEST_SUPPORTS_GRAPHICS rc=%Rrc\n", rc);

        RTThreadSleep(1000);

        if (*pfShutdown)
            break;
    }

    rc = VbglR3CtlFilterMask(0, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    VGSvcVerbose(2, "VbglR3CtlFilterMask cleared rc=%Rrc\n", rc);

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
