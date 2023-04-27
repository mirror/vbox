/* $Id$ */
/** @file
 * VBoxBFE - The basic VirtualBox frontend for running VMs without using Main/COM/XPCOM.
 * Mainly serves as a playground for the ARMv8 VMM bringup for now.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GUI

#include <VBox/log.h>
#include <VBox/version.h>
#include <VBox/vmm/vmmr3vtable.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/vmm/pdm.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/semaphore.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/getopt.h>
#include <iprt/env.h>
#include <iprt/errcore.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>

#include <SDL.h>

#include "Display.h"
#include "Framebuffer.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

#define LogError(m,rc) \
    do { \
        Log(("VBoxBFE: ERROR: " m " [rc=0x%08X]\n", rc)); \
        RTPrintf("%s\n", m); \
    } while (0)

#define DTB_ADDR 0x40000000


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** flag whether frontend should terminate */
static volatile bool    g_fTerminateFE      = false;
static RTLDRMOD         g_hModVMM           = NIL_RTLDRMOD;
static PCVMMR3VTABLE    g_pVMM              = NULL;
static PVM              g_pVM               = NULL;
static PUVM             g_pUVM              = NULL;
static uint32_t         g_u32MemorySizeMB   = 512;
static VMSTATE          g_enmVmState        = VMSTATE_CREATING;
static const char       *g_pszLoadMem       = NULL;
static const char       *g_pszLoadFlash     = NULL;
static const char       *g_pszLoadDtb       = NULL;
static const char       *g_pszSerialLog     = NULL;
static const char       *g_pszLoadKernel    = NULL;
static const char       *g_pszLoadInitrd    = NULL;
static const char       *g_pszCmdLine       = NULL;
static VMM2USERMETHODS  g_Vmm2UserMethods;
static Display          *g_pDisplay         = NULL;
static Framebuffer      *g_pFramebuffer     = NULL;
static bool gfIgnoreNextResize = false;
static SDL_TimerID gSdlResizeTimer = 0;

/** @todo currently this is only set but never read. */
static char szError[512];

extern DECL_HIDDEN_DATA(RTSEMEVENT) g_EventSemSDLEvents;
extern DECL_HIDDEN_DATA(volatile int32_t) g_cNotifyUpdateEventsPending;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Wait for the next SDL event. Don't use SDL_WaitEvent since this function
 * calls SDL_Delay(10) if the event queue is empty.
 */
static int WaitSDLEvent(SDL_Event *event)
{
    for (;;)
    {
        int rc = SDL_PollEvent(event);
        if (rc == 1)
            return 1;
        /* Immediately wake up if new SDL events are available. This does not
         * work for internal SDL events. Don't wait more than 10ms. */
        RTSemEventWait(g_EventSemSDLEvents, 10);
    }
}


/**
 * Timer callback function to check if resizing is finished
 */
static Uint32 ResizeTimer(Uint32 interval, void *param) RT_NOTHROW_DEF
{
    RT_NOREF(interval, param);

    /* post message so the window is actually resized */
    SDL_Event event = {0};
    event.type      = SDL_USEREVENT;
    event.user.type = SDL_USER_EVENT_WINDOW_RESIZE_DONE;
    PushSDLEventForSure(&event);
    /* one-shot */
    return 0;
}


/**
 * VM state callback function. Called by the VMM
 * using its state machine states.
 *
 * Primarily used to handle VM initiated power off, suspend and state saving,
 * but also for doing termination completed work (VMSTATE_TERMINATE).
 *
 * In general this function is called in the context of the EMT.
 *
 * @todo g_enmVmState is set to VMSTATE_RUNNING before all devices have received power on events
 *       this can prematurely allow the main thread to enter the event loop
 *
 * @param   pVM         The VM handle.
 * @param   enmState    The new state.
 * @param   enmOldState The old state.
 * @param   pvUser      The user argument.
 */
static DECLCALLBACK(void) vboxbfeVmStateChangeCallback(PUVM pUVM, PCVMMR3VTABLE pVMM, VMSTATE enmState, VMSTATE enmOldState, void *pvUser)
{
    RT_NOREF(pUVM, pVMM, enmOldState, pvUser);
    LogFlow(("vmstateChangeCallback: changing state from %d to %d\n", enmOldState, enmState));
    g_enmVmState = enmState;

    switch (enmState)
    {
        /*
         * The VM has terminated
         */
        case VMSTATE_OFF:
        {
            break;
        }

        /*
         * The VM has been completely destroyed.
         *
         * Note: This state change can happen at two points:
         *       1) At the end of VMR3Destroy() if it was not called from EMT.
         *       2) At the end of vmR3EmulationThread if VMR3Destroy() was called by EMT.
         */
        case VMSTATE_TERMINATED:
        {
            break;
        }

        default: /* shut up gcc */
            break;
    }
}


/**
 * VM error callback function. Called by the various VM components.
 *
 * @param   pVM         The VM handle.
 * @param   pvUser      The user argument.
 * @param   vrc         VBox status code.
 * @param   pszFormat   Error message format string.
 * @param   args        Error message arguments.
 * @thread EMT.
 */
DECLCALLBACK(void) vboxbfeSetVMErrorCallback(PUVM pUVM, void *pvUser, int vrc, RT_SRC_POS_DECL, const char *pszFormat, va_list args)
{
    RT_NOREF(pUVM, pvUser, pszFile, iLine, pszFunction);

    /** @todo accessing shared resource without any kind of synchronization */
    if (RT_SUCCESS(vrc))
        szError[0] = '\0';
    else
    {
        va_list va2;
        va_copy(va2, args); /* Have to make a copy here or GCC will break. */
        RTStrPrintf(szError, sizeof(szError),
                    "%N!\nVBox status code: %d (%Rrc)", pszFormat, &va2, vrc, vrc);
        RTPrintf("%s\n", szError);
        va_end(va2);
    }
}


/**
 * VM Runtime error callback function. Called by the various VM components.
 *
 * @param   pVM         The VM handle.
 * @param   pvUser      The user argument.
 * @param   fFlags      The action flags. See VMSETRTERR_FLAGS_*.
 * @param   pszErrorId  Error ID string.
 * @param   pszFormat   Error message format string.
 * @param   va          Error message arguments.
 * @thread EMT.
 */
DECLCALLBACK(void) vboxbfeSetVMRuntimeErrorCallback(PUVM pUVM, void *pvUser, uint32_t fFlags,
                                                    const char *pszErrorId, const char *pszFormat, va_list va)
{
    RT_NOREF(pUVM, pvUser);

    va_list va2;
    va_copy(va2, va); /* Have to make a copy here or GCC/AMD64 will break. */
    RTPrintf("%s: %s!\n%N!\n",
             fFlags & VMSETRTERR_FLAGS_FATAL ? "Error" : "Warning",
             pszErrorId, pszFormat, &va2);
    RTStrmFlush(g_pStdErr);
    va_end(va2);
}


/**
 * Register the main drivers.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
DECLCALLBACK(int) VBoxDriversRegister(PCPDMDRVREGCB pCallbacks, uint32_t u32Version)
{
    LogFlow(("VBoxDriversRegister: u32Version=%#x\n", u32Version));
    AssertReleaseMsg(u32Version == VBOX_VERSION, ("u32Version=%#x VBOX_VERSION=%#x\n", u32Version, VBOX_VERSION));

    int vrc = pCallbacks->pfnRegister(pCallbacks, &Display::DrvReg);
    if (RT_FAILURE(vrc))
        return vrc;

    return VINF_SUCCESS;
}


/**
 * Constructs the VMM configuration tree.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 */
static DECLCALLBACK(int) vboxbfeConfigConstructor(PUVM pUVM, PVM pVM, PCVMMR3VTABLE pVMM, void *pvConsole)
{
    int rcAll = VINF_SUCCESS;
    int rc;

    RT_NOREF(pvConsole);
    g_pUVM = pUVM;

#define UPDATE_RC() do { if (RT_FAILURE(rc) && RT_SUCCESS(rcAll)) rcAll = rc; } while (0)

    /*
     * Root values.
     */
    PCFGMNODE pRoot = pVMM->pfnCFGMR3GetRoot(pVM);
    rc = pVMM->pfnCFGMR3InsertString(pRoot,  "Name",           "Default VM");                   UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pRoot, "TimerMillies",   1000);                           UPDATE_RC();

    /*
     * Memory setup.
     */
    PCFGMNODE pMem = NULL;
    rc = pVMM->pfnCFGMR3InsertNode(pRoot, "MM", &pMem);                                         UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pMem, "MemRegions", &pMem);                                  UPDATE_RC();

    PCFGMNODE pMemRegion = NULL;
    rc = pVMM->pfnCFGMR3InsertNode(pMem, "Flash", &pMemRegion);                                 UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pMemRegion, "GCPhysStart",   0);                          UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pMemRegion, "Size", 64 * _1M);                            UPDATE_RC();

    rc = pVMM->pfnCFGMR3InsertNode(pMem, "Conventional", &pMemRegion);                          UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pMemRegion, "GCPhysStart",   DTB_ADDR);                   UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pMemRegion, "Size", (uint64_t)g_u32MemorySizeMB * _1M);   UPDATE_RC();


    /*
     * PDM.
     */
    rc = pVMM->pfnPDMR3DrvStaticRegistration(pVM, VBoxDriversRegister);                         UPDATE_RC();

    /*
     * Devices
     */
    PCFGMNODE pDevices = NULL;
    PCFGMNODE pDev = NULL;          /* /Devices/Dev/ */
    PCFGMNODE pInst = NULL;         /* /Devices/Dev/0/ */
    PCFGMNODE pCfg = NULL;          /* /Devices/Dev/.../Config/ */
    PCFGMNODE pLunL0 = NULL;        /* /Devices/Dev/0/LUN#0/ */
    PCFGMNODE pLunL1 = NULL;        /* /Devices/Dev/0/LUN#0/AttachedDriver/ */
    PCFGMNODE pLunL1Cfg = NULL;     /* /Devices/Dev/0/LUN#0/AttachedDriver/Config */

    rc = pVMM->pfnCFGMR3InsertNode(pRoot, "Devices", &pDevices);                             UPDATE_RC();

    rc = pVMM->pfnCFGMR3InsertNode(pDevices, "gic",          &pDev);                         UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pDev,     "0",            &pInst);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pInst, "Trusted",      1);                             UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pInst,    "Config",        &pCfg);                        UPDATE_RC();

    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "DistributorMmioBase",       0x08000000);       UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "RedistributorMmioBase",     0x080a0000);       UPDATE_RC();


    rc = pVMM->pfnCFGMR3InsertNode(pDevices, "qemu-fw-cfg",   &pDev);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pDev,     "0",            &pInst);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pInst,    "Config",        &pCfg);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "MmioSize",       4096);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "MmioBase", 0x09020000);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "DmaEnabled",        1);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "QemuRamfbSupport",  1);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pInst,    "LUN#0",           &pLunL0);                    UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertString(pLunL0, "Driver",          "MainDisplay");              UPDATE_RC();
    if (g_pszLoadKernel)
    {
        rc = pVMM->pfnCFGMR3InsertString(pCfg,  "KernelImage",  g_pszLoadKernel);            UPDATE_RC();
    }
    if (g_pszLoadInitrd)
    {
        rc = pVMM->pfnCFGMR3InsertString(pCfg,  "InitrdImage",  g_pszLoadInitrd);            UPDATE_RC();
    }
    if (g_pszCmdLine)
    {
        rc = pVMM->pfnCFGMR3InsertString(pCfg,  "CmdLine",  g_pszCmdLine);                   UPDATE_RC();
    }


    rc = pVMM->pfnCFGMR3InsertNode(pDevices, "flash-cfi",         &pDev);                    UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pDev,     "0",            &pInst);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pInst,    "Config",        &pCfg);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "BaseAddress", 64 * _1M);                       UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "Size",        64 * _1M);                       UPDATE_RC();
    if (g_pszLoadFlash)
    {
        rc = pVMM->pfnCFGMR3InsertString(pCfg, "FlashFile", g_pszLoadFlash);                 UPDATE_RC();
    }

    rc = pVMM->pfnCFGMR3InsertNode(pDevices, "arm-pl011",     &pDev);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pDev,     "0",            &pInst);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pInst,    "Config",        &pCfg);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "Irq",               1);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "MmioBase", 0x09000000);                        UPDATE_RC();

    if (g_pszSerialLog)
    {
        rc = pVMM->pfnCFGMR3InsertNode(pInst,    "LUN#0",           &pLunL0);                UPDATE_RC();
        rc = pVMM->pfnCFGMR3InsertString(pLunL0, "Driver",          "Char");                 UPDATE_RC();
        rc = pVMM->pfnCFGMR3InsertNode(pLunL0,   "AttachedDriver",  &pLunL1);                UPDATE_RC();
        rc = pVMM->pfnCFGMR3InsertString(pLunL1, "Driver",          "RawFile");              UPDATE_RC();
        rc = pVMM->pfnCFGMR3InsertNode(pLunL1,    "Config",         &pLunL1Cfg);             UPDATE_RC();
        rc = pVMM->pfnCFGMR3InsertString(pLunL1Cfg, "Location",     g_pszSerialLog);         UPDATE_RC();
    }

    rc = pVMM->pfnCFGMR3InsertNode(pDevices, "arm-pl031-rtc", &pDev);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pDev,     "0",            &pInst);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertNode(pInst,    "Config",        &pCfg);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "Irq",               2);                        UPDATE_RC();
    rc = pVMM->pfnCFGMR3InsertInteger(pCfg,  "MmioBase", 0x09010000);                        UPDATE_RC();

#undef UPDATE_RC
#undef UPDATE_RC

    pVMM->pfnVMR3AtRuntimeErrorRegister (pUVM, vboxbfeSetVMRuntimeErrorCallback, NULL);

    return rc;
}


/**
 * Loads the VMM if needed.
 *
 * @returns VBox status code.
 * @param   pszVmmMod       The VMM module to load.
 */
int vboxbfeLoadVMM(const char *pszVmmMod)
{
    Assert(!g_pVMM);

    RTERRINFOSTATIC ErrInfo;
    RTLDRMOD        hModVMM = NIL_RTLDRMOD;
    int vrc = SUPR3HardenedLdrLoadAppPriv(pszVmmMod, &hModVMM, RTLDRLOAD_FLAGS_LOCAL, RTErrInfoInitStatic(&ErrInfo));
    if (RT_SUCCESS(vrc))
    {
        PFNVMMGETVTABLE pfnGetVTable = NULL;
        vrc = RTLdrGetSymbol(hModVMM, VMMR3VTABLE_GETTER_NAME, (void **)&pfnGetVTable);
        if (pfnGetVTable)
        {
            PCVMMR3VTABLE pVMM = pfnGetVTable();
            if (pVMM)
            {
                if (VMMR3VTABLE_IS_COMPATIBLE(pVMM->uMagicVersion))
                {
                    if (pVMM->uMagicVersion == pVMM->uMagicVersionEnd)
                    {
                        g_hModVMM = hModVMM;
                        g_pVMM    = pVMM;
                        LogFunc(("mhLdrVMM=%p phVMM=%p uMagicVersion=%#RX64\n", hModVMM, pVMM, pVMM->uMagicVersion));
                        return VINF_SUCCESS;
                    }

                    LogRel(("Bogus VMM vtable: uMagicVersion=%#RX64 uMagicVersionEnd=%#RX64",
                            pVMM->uMagicVersion, pVMM->uMagicVersionEnd));
                }
                else
                    LogRel(("Incompatible of bogus VMM version magic: %#RX64", pVMM->uMagicVersion));
            }
            else
                LogRel(("pfnGetVTable return NULL!"));
        }
        else
            LogRel(("Failed to locate symbol '%s' in VBoxVMM: %Rrc", VMMR3VTABLE_GETTER_NAME, vrc));
        RTLdrClose(hModVMM);
    }
    else
        LogRel(("Failed to load VBoxVMM: %#RTeic", &ErrInfo.Core));

    return vrc;
}


/**
 * Loads the content of a given file into guest RAM starting at the given guest physical address.
 *
 * @returns VBox status code.
 * @param   pszFile         The file to load.
 * @param   GCPhysStart     The physical start address to load the file at.
 */
static int vboxbfeLoadFileAtGCPhys(const char *pszFile, RTGCPHYS GCPhysStart)
{
    RTFILE hFile = NIL_RTFILE;
    int rc = RTFileOpen(&hFile, pszFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
    {
        uint8_t abRead[GUEST_PAGE_SIZE];
        RTGCPHYS GCPhys = GCPhysStart;

        for (;;)
        {
            size_t cbThisRead = 0;
            rc = RTFileRead(hFile, &abRead[0], sizeof(abRead), &cbThisRead);
            if (RT_FAILURE(rc))
                break;

            rc = g_pVMM->pfnPGMPhysSimpleWriteGCPhys(g_pVM, GCPhys, &abRead[0], cbThisRead);
            if (RT_FAILURE(rc))
                break;

            GCPhys += cbThisRead;
            if (cbThisRead < sizeof(abRead))
                break;
        }

        RTFileClose(hFile);
    }
    else
        RTPrintf("Loading file %s failed -> %Rrc\n", pszFile, rc);

    return rc;
}


/**
 * @interface_method_impl{VMM2USERMETHODS,pfnQueryGenericObject}
 */
static DECLCALLBACK(void *) vboxbfeVmm2User_QueryGenericObject(PCVMM2USERMETHODS pThis, PUVM pUVM, PCRTUUID pUuid)
{
    RT_NOREF(pThis, pUVM);

    if (!RTUuidCompareStr(pUuid, DISPLAY_OID))
        return g_pDisplay;

    return NULL;
}


/** VM asynchronous operations thread */
DECLCALLBACK(int) vboxbfeVMPowerUpThread(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread, pvUser);

    int rc = VINF_SUCCESS;
    int rc2;

#if 0
    /*
     * Setup the release log instance in current directory.
     */
    if (g_fReleaseLog)
    {
        static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
        static char s_szError[RTPATH_MAX + 128] = "";
        PRTLOGGER pLogger;
        rc2 = RTLogCreateEx(&pLogger, RTLOGFLAGS_PREFIX_TIME_PROG, "all",
                            "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups, RTLOGDEST_FILE,
                            NULL /* pfnBeginEnd */, 0 /* cHistory */, 0 /* cbHistoryFileMax */, 0 /* uHistoryTimeMax */,
                            s_szError, sizeof(s_szError), "./VBoxBFE.log");
        if (RT_SUCCESS(rc2))
        {
            /* some introductory information */
            RTTIMESPEC TimeSpec;
            char szNowUct[64];
            RTTimeSpecToString(RTTimeNow(&TimeSpec), szNowUct, sizeof(szNowUct));
            RTLogRelLogger(pLogger, 0, ~0U,
                           "VBoxBFE %s (%s %s) release log\n"
                           "Log opened %s\n",
                           VBOX_VERSION_STRING, __DATE__, __TIME__,
                           szNowUct);

            /* register this logger as the release logger */
            RTLogRelSetDefaultInstance(pLogger);
        }
        else
            RTPrintf("Could not open release log (%s)\n", s_szError);
    }
#endif

    /*
     * Start VM (also from saved state) and track progress
     */
    LogFlow(("VMPowerUp\n"));

    g_Vmm2UserMethods.u32Magic                         = VMM2USERMETHODS_MAGIC;
    g_Vmm2UserMethods.u32Version                       = VMM2USERMETHODS_VERSION;
    g_Vmm2UserMethods.pfnSaveState                     = NULL;
    g_Vmm2UserMethods.pfnNotifyEmtInit                 = NULL;
    g_Vmm2UserMethods.pfnNotifyEmtTerm                 = NULL;
    g_Vmm2UserMethods.pfnNotifyPdmtInit                = NULL;
    g_Vmm2UserMethods.pfnNotifyPdmtTerm                = NULL;
    g_Vmm2UserMethods.pfnNotifyResetTurnedIntoPowerOff = NULL;
    g_Vmm2UserMethods.pfnQueryGenericObject            = vboxbfeVmm2User_QueryGenericObject;
    g_Vmm2UserMethods.u32EndMagic                      = VMM2USERMETHODS_MAGIC;

    /*
     * Create empty VM.
     */
    rc = g_pVMM->pfnVMR3Create(1, &g_Vmm2UserMethods, 0 /*fFlags*/, vboxbfeSetVMErrorCallback, NULL, vboxbfeConfigConstructor, NULL, &g_pVM, NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("Error: VM creation failed with %Rrc.\n", rc);
        goto failure;
    }


    /*
     * Register VM state change handler
     */
    rc = g_pVMM->pfnVMR3AtStateRegister(g_pUVM, vboxbfeVmStateChangeCallback, NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("Error: VMR3AtStateRegister failed with %Rrc.\n", rc);
        goto failure;
    }

    /*
     * Prepopulate the memory?
     */
    if (g_pszLoadMem)
    {
        rc = vboxbfeLoadFileAtGCPhys(g_pszLoadMem, 0 /*GCPhysStart*/);
        if (RT_FAILURE(rc))
            goto failure;
    }

    if (g_pszLoadDtb)
    {
        rc = vboxbfeLoadFileAtGCPhys(g_pszLoadDtb, DTB_ADDR);
        if (RT_FAILURE(rc))
            goto failure;
    }

    /*
     * Power on the VM (i.e. start executing).
     */
    if (RT_SUCCESS(rc))
    {
#if 0
        if (   g_fRestoreState
            && g_pszStateFile
            && *g_pszStateFile
            && RTPathExists(g_pszStateFile))
        {
            startProgressInfo("Restoring");
            rc = VMR3LoadFromFile(gpVM, g_pszStateFile, callProgressInfo, (uintptr_t)NULL);
            endProgressInfo();
            if (RT_SUCCESS(rc))
            {
                rc = VMR3Resume(gpVM);
                AssertRC(rc);
            }
            else
                AssertMsgFailed(("VMR3LoadFromFile failed, rc=%Rrc\n", rc));
        }
        else
#endif
        {
            rc = g_pVMM->pfnVMR3PowerOn(g_pUVM);
            if (RT_FAILURE(rc))
                AssertMsgFailed(("VMR3PowerOn failed, rc=%Rrc\n", rc));
        }
    }

    /*
     * On failure destroy the VM.
     */
    if (RT_FAILURE(rc))
        goto failure;

    return VINF_SUCCESS;

failure:
    if (g_pVM)
    {
        rc2 = g_pVMM->pfnVMR3Destroy(g_pUVM);
        AssertRC(rc2);
        g_pVM = NULL;
    }
    g_enmVmState = VMSTATE_TERMINATED;

    return VINF_SUCCESS;
}


static void show_usage()
{
    RTPrintf("Usage:\n"
             "   --start-paused     Start the VM in paused state\n"
             "\n");
}

/**
 *  Entry point.
 */
extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv, char **envp)
{
    RT_NOREF(envp);
    unsigned fPaused = 0;

    LogFlow(("VBoxBFE STARTED.\n"));
    RTPrintf(VBOX_PRODUCT " Basic Interface " VBOX_VERSION_STRING "\n"
             "Copyright (C) 2023-" VBOX_C_YEAR " " VBOX_VENDOR "\n\n");

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--start-paused",       'p', 0                   },
        { "--memory-size-mib",    'm', RTGETOPT_REQ_UINT32 },
        { "--load-file-into-ram", 'l', RTGETOPT_REQ_STRING },
        { "--load-flash",         'f', RTGETOPT_REQ_STRING },
        { "--load-dtb",           'd', RTGETOPT_REQ_STRING },
        { "--load-vmm",           'v', RTGETOPT_REQ_STRING },
        { "--load-kernel",        'k', RTGETOPT_REQ_STRING },
        { "--load-initrd",        'i', RTGETOPT_REQ_STRING },
        { "--cmd-line",           'c', RTGETOPT_REQ_STRING },
        { "--serial-log",         's', RTGETOPT_REQ_STRING },
    };

    const char *pszVmmMod = "VBoxVMM";

    /* Parse the config. */
    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0 /* fFlags */);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch(ch)
        {
            case 'p':
                fPaused = true;
                break;
            case 'm':
                g_u32MemorySizeMB = ValueUnion.u32;
                break;
            case 'l':
                g_pszLoadMem = ValueUnion.psz;
                break;
            case 'f':
                g_pszLoadFlash = ValueUnion.psz;
                break;
            case 'd':
                g_pszLoadDtb = ValueUnion.psz;
                break;
            case 'v':
                pszVmmMod = ValueUnion.psz;
                break;
            case 'k':
                g_pszLoadKernel = ValueUnion.psz;
                break;
            case 'i':
                g_pszLoadInitrd = ValueUnion.psz;
                break;
            case 'c':
                g_pszCmdLine = ValueUnion.psz;
                break;
            case 's':
                g_pszSerialLog = ValueUnion.psz;
                break;
            case 'h':
                show_usage();
                return 0;
            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;
            default:
                ch = RTGetOptPrintError(ch, &ValueUnion);
                show_usage();
                return ch;
        }
    }

    /* static initialization of the SDL stuff */
    if (!Framebuffer::init(true /*fShowSDLConfig*/))
        return RTEXITCODE_FAILURE;

    g_pDisplay = new Display();
    g_pFramebuffer = new Framebuffer(g_pDisplay, 0, false /*fFullscreen*/, false /*fResizable*/, true /*fShowSDLConfig*/, false,
                                     ~0, ~0, ~0, false /*fSeparate*/);
    g_pDisplay->SetFramebuffer(0, g_pFramebuffer);

    int vrc = vboxbfeLoadVMM(pszVmmMod);
    if (RT_FAILURE(vrc))
        return RTEXITCODE_FAILURE;

    /*
     * Start the VM execution thread. This has to be done
     * asynchronously as powering up can take some time
     * (accessing devices such as the host DVD drive). In
     * the meantime, we have to service the SDL event loop.
     */

    RTTHREAD thread;
    vrc = RTThreadCreate(&thread, vboxbfeVMPowerUpThread, 0, 0, RTTHREADTYPE_MAIN_WORKER, 0, "PowerUp");
    if (RT_FAILURE(vrc))
    {
        RTPrintf("Error: Thread creation failed with %d\n", vrc);
        return RTEXITCODE_FAILURE;
    }


    /* loop until the powerup processing is done */
    do
    {
        RTThreadSleep(1000);
    }
    while (   g_enmVmState == VMSTATE_CREATING
           || g_enmVmState == VMSTATE_LOADING);

    LogFlow(("VBoxSDL: Entering big event loop\n"));
    SDL_Event event;
    uint32_t uResizeWidth  = ~(uint32_t)0;
    uint32_t uResizeHeight = ~(uint32_t)0;

    while (WaitSDLEvent(&event))
    {
        switch (event.type)
        {
            /*
             * The screen needs to be repainted.
             */
            case SDL_WINDOWEVENT:
            {
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_EXPOSED:
                    {
                        g_pFramebuffer->repaint();
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                    {
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                    {
                        break;
                    }
                    case SDL_WINDOWEVENT_RESIZED:
                    {
                        if (g_pDisplay)
                        {
                            if (gfIgnoreNextResize)
                            {
                                gfIgnoreNextResize = FALSE;
                                break;
                            }
                            uResizeWidth  = event.window.data1;
                            uResizeHeight = event.window.data2;
                            if (gSdlResizeTimer)
                                SDL_RemoveTimer(gSdlResizeTimer);
                            gSdlResizeTimer = SDL_AddTimer(300, ResizeTimer, NULL);
                        }
                        break;
                    }
                    default:
                        break;
                }
                break;
            }

            /*
             * The window was closed.
             */
            case SDL_QUIT:
            {
                /** @todo */
                break;
            }

            /*
             * User specific update event.
             */
            /** @todo use a common user event handler so that SDL_PeepEvents() won't
             * possibly remove other events in the queue!
             */
            case SDL_USER_EVENT_UPDATERECT:
            {
                /*
                 * Decode event parameters.
                 */
                ASMAtomicDecS32(&g_cNotifyUpdateEventsPending);

                SDL_Rect *pUpdateRect = (SDL_Rect *)event.user.data1;
                AssertPtrBreak(pUpdateRect);

                int const x = pUpdateRect->x;
                int const y = pUpdateRect->y;
                int const w = pUpdateRect->w;
                int const h = pUpdateRect->h;

                RTMemFree(event.user.data1);

                Log3Func(("SDL_USER_EVENT_UPDATERECT: x=%d y=%d, w=%d, h=%d\n", x, y, w, h));

                Assert(g_pFramebuffer);
                g_pFramebuffer->update(x, y, w, h, true /* fGuestRelative */);
                break;
            }

            /*
             * User event: Window resize done
             */
            case SDL_USER_EVENT_WINDOW_RESIZE_DONE:
            {
                /* communicate the resize event to the guest */
                //g_pDisplay->SetVideoModeHint(0 /*=display*/, true /*=enabled*/, false /*=changeOrigin*/,
                //                             0 /*=originX*/, 0 /*=originY*/,
                //                             uResizeWidth, uResizeHeight, 0 /*=don't change bpp*/, true /*=notify*/);
                break;

            }

            /*
             * User specific framebuffer change event.
             */
            case SDL_USER_EVENT_NOTIFYCHANGE:
            {
                LogFlow(("SDL_USER_EVENT_NOTIFYCHANGE\n"));
                g_pFramebuffer->notifyChange(event.user.code);
                break;
            }

            /*
             * User specific termination event
             */
            case SDL_USER_EVENT_TERMINATE:
            {
                if (event.user.code != VBOXSDL_TERM_NORMAL)
                    RTPrintf("Error: VM terminated abnormally!\n");
                break;
            }

            default:
            {
                Log8(("unknown SDL event %d\n", event.type));
                break;
            }
        }
    }

    LogRel(("VBoxBFE: exiting\n"));
    return RT_SUCCESS(vrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


#ifndef VBOX_WITH_HARDENING
/**
 * Main entry point.
 */
int main(int argc, char **argv, char **envp)
{
    int rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_TRY_SUPLIB);
    if (RT_SUCCESS(rc))
        return TrustedMain(argc, argv, envp);
    RTPrintf("VBoxBFE: Runtime initialization failed: %Rrc - %Rrf\n", rc, rc);
    return RTEXITCODE_FAILURE;
}
#endif /* !VBOX_WITH_HARDENING */
