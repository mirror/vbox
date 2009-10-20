/* $Id$ */
/** @file
 * VBox Console COM Class implementation
 *
 * @remark  We've split out the code that the 64-bit VC++ v8 compiler finds
 *          problematic to optimize so we can disable optimizations and later,
 *          perhaps, find a real solution for it (like rewriting the code and
 *          to stop resemble a tonne of spaghetti).
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "ConsoleImpl.h"
#include "DisplayImpl.h"
#include "VMMDev.h"

// generated header
#include "SchemaDefs.h"

#include "Logging.h"

#include <iprt/buildconfig.h>
#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/param.h>
#if 0 /* enable to play with lots of memory. */
# include <iprt/env.h>
#endif

#include <VBox/vmapi.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#ifdef VBOX_WITH_CROGL
#include <VBox/HostServices/VBoxCrOpenGLSvc.h>
#endif
#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>
# include <VBox/com/defs.h>
# include <VBox/com/array.h>
# include <hgcm/HGCM.h> /** @todo it should be possible to register a service
                          * extension using a VMMDev callback. */
# include <vector>
#endif /* VBOX_WITH_GUEST_PROPS */
#include <VBox/intnet.h>

#include <VBox/com/string.h>
#include <VBox/com/array.h>

#if defined(RT_OS_SOLARIS) && defined(VBOX_WITH_NETFLT)
# include <zone.h>
#endif

#if defined(RT_OS_LINUX) && defined(VBOX_WITH_NETFLT)
# include <unistd.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <linux/types.h>
# include <linux/if.h>
# include <linux/wireless.h>
#endif

#if defined(RT_OS_FREEBSD) && defined(VBOX_WITH_NETFLT)
# include <unistd.h>
# include <sys/types.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <net/if.h>
#endif

#if defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)
# include <VBox/WinNetConfig.h>
# include <Ntddndis.h>
# include <devguid.h>
#endif

#if !defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)
# include <HostNetworkInterfaceImpl.h>
# include <netif.h>
#endif

#include "DHCPServerRunner.h"

#include <VBox/param.h>

/* Comment out the following line to remove VMWare compatibility hack. */
#define VMWARE_NET_IN_SLOT_11

/**
 * Translate IDE StorageControllerType_T to string representation.
 */
const char* controllerString(StorageControllerType_T enmType)
{
    switch (enmType)
    {
        case StorageControllerType_PIIX3:
            return "PIIX3";
        case StorageControllerType_PIIX4:
            return "PIIX4";
        case StorageControllerType_ICH6:
            return "ICH6";
        default:
            return "Unknown";
    }
}

/*
 * VC++ 8 / amd64 has some serious trouble with this function.
 * As a temporary measure, we'll drop global optimizations.
 */
#if defined(_MSC_VER) && defined(RT_ARCH_AMD64)
# pragma optimize("g", off)
#endif

/**
 *  Construct the VM configuration tree (CFGM).
 *
 *  This is a callback for VMR3Create() call. It is called from CFGMR3Init()
 *  in the emulation thread (EMT). Any per thread COM/XPCOM initialization
 *  is done here.
 *
 *  @param   pVM                 VM handle.
 *  @param   pvConsole           Pointer to the VMPowerUpTask object.
 *  @return  VBox status code.
 *
 *  @note Locks the Console object for writing.
 */
DECLCALLBACK(int) Console::configConstructor(PVM pVM, void *pvConsole)
{
    LogFlowFuncEnter();
    /* Note: hardcoded assumption about number of slots; see rom bios */
    bool afPciDeviceNo[32] = {false};
    bool fFdcEnabled = false;

#if !defined (VBOX_WITH_XPCOM)
    {
        /* initialize COM */
        HRESULT hrc = CoInitializeEx(NULL,
                                     COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE |
                                     COINIT_SPEED_OVER_MEMORY);
        LogFlow (("Console::configConstructor(): CoInitializeEx()=%08X\n", hrc));
        AssertComRCReturn (hrc, VERR_GENERAL_FAILURE);
    }
#endif

    AssertReturn(pvConsole, VERR_GENERAL_FAILURE);
    ComObjPtr<Console> pConsole = static_cast <Console *> (pvConsole);

    AutoCaller autoCaller(pConsole);
    AssertComRCReturn (autoCaller.rc(), VERR_ACCESS_DENIED);

    /* lock the console because we widely use internal fields and methods */
    AutoWriteLock alock(pConsole);

    /* Save the VM pointer in the machine object */
    pConsole->mpVM = pVM;

    ComPtr<IMachine> pMachine = pConsole->machine();

    int             rc;
    HRESULT         hrc;
    BSTR            str = NULL;

#define STR_FREE()  do { if (str) { SysFreeString(str); str = NULL; } } while (0)
#define RC_CHECK()  do { if (RT_FAILURE(rc)) { AssertMsgFailed(("rc=%Rrc\n", rc));  STR_FREE(); return rc;                   } } while (0)
#define H()         do { if (FAILED(hrc))    { AssertMsgFailed(("hrc=%#x\n", hrc)); STR_FREE(); return VERR_GENERAL_FAILURE; } } while (0)

    /*
     * Get necessary objects and frequently used parameters.
     */
    ComPtr<IVirtualBox> virtualBox;
    hrc = pMachine->COMGETTER(Parent)(virtualBox.asOutParam());                     H();

    ComPtr<IHost> host;
    hrc = virtualBox->COMGETTER(Host)(host.asOutParam());                           H();

    ComPtr<ISystemProperties> systemProperties;
    hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());   H();

    ComPtr<IBIOSSettings> biosSettings;
    hrc = pMachine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());             H();

    hrc = pMachine->COMGETTER(Id)(&str);                                            H();
    Guid MachineUuid(str);
    PCRTUUID pUuid = MachineUuid.raw();
    STR_FREE();

    ULONG cRamMBs;
    hrc = pMachine->COMGETTER(MemorySize)(&cRamMBs);                                H();
#if 0 /* enable to play with lots of memory. */
    if (RTEnvExist("VBOX_RAM_SIZE"))
        cRamMBs = RTStrToUInt64(RTEnvGet("VBOX_RAM_SIZE"));
#endif
    uint64_t const cbRam = cRamMBs * (uint64_t)_1M;
    uint32_t const cbRamHole = MM_RAM_HOLE_SIZE_DEFAULT;

    ULONG cCpus = 1;
    hrc = pMachine->COMGETTER(CPUCount)(&cCpus);                                    H();

    Bstr osTypeId;
    hrc = pMachine->COMGETTER(OSTypeId)(osTypeId.asOutParam());                     H();

    BOOL fIOAPIC;
    hrc = biosSettings->COMGETTER(IOAPICEnabled)(&fIOAPIC);                          H();

    /*
     * Get root node first.
     * This is the only node in the tree.
     */
    PCFGMNODE pRoot = CFGMR3GetRoot(pVM);
    Assert(pRoot);

    /*
     * Set the root (and VMM) level values.
     */
    hrc = pMachine->COMGETTER(Name)(&str);                                          H();
    rc = CFGMR3InsertStringW(pRoot, "Name",                 str);                   RC_CHECK();
    rc = CFGMR3InsertBytes(pRoot,   "UUID", pUuid, sizeof(*pUuid));                 RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RamSize",              cbRam);                 RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RamHoleSize",          cbRamHole);             RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "NumCPUs",              cCpus);                 RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "TimerMillies",         10);                    RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RawR3Enabled",         1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RawR0Enabled",         1);     /* boolean */   RC_CHECK();
    /** @todo Config: RawR0, PATMEnabled and CASMEnabled needs attention later. */
    rc = CFGMR3InsertInteger(pRoot, "PATMEnabled",          1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "CSAMEnabled",          1);     /* boolean */   RC_CHECK();

    if (osTypeId == "WindowsNT4")
    {
        /*
         * We must limit CPUID count for Windows NT 4, as otherwise it stops
         * with error 0x3e (MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED).
         */
        LogRel(("Limiting CPUID leaf count for NT4 guests\n"));
        PCFGMNODE pCPUM;
        rc = CFGMR3InsertNode(pRoot, "CPUM", &pCPUM);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pCPUM, "NT4LeafLimit", true);                      RC_CHECK();
    }

    /* hardware virtualization extensions */
    BOOL fHWVirtExEnabled;
    hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_Enabled, &fHWVirtExEnabled);                  H();
    if (cCpus > 1) /** @todo SMP: This isn't nice, but things won't work on mac otherwise. */
        fHWVirtExEnabled = TRUE;

#ifdef RT_OS_DARWIN
    rc = CFGMR3InsertInteger(pRoot, "HwVirtExtForced",      fHWVirtExEnabled);      RC_CHECK();
#else
    /* - With more than 4GB PGM will use different RAMRANGE sizes for raw
         mode and hv mode to optimize lookup times.
       - With more than one virtual CPU, raw-mode isn't a fallback option. */
    BOOL fHwVirtExtForced = fHWVirtExEnabled
                         && (   cbRam > (_4G - cbRamHole)
                             || cCpus > 1);
    rc = CFGMR3InsertInteger(pRoot, "HwVirtExtForced",      fHwVirtExtForced);      RC_CHECK();
#endif

    PCFGMNODE pHWVirtExt;
    rc = CFGMR3InsertNode(pRoot, "HWVirtExt", &pHWVirtExt);                         RC_CHECK();
    if (fHWVirtExEnabled)
    {
        rc = CFGMR3InsertInteger(pHWVirtExt, "Enabled",     1);                     RC_CHECK();

        /* Indicate whether 64-bit guests are supported or not. */
        /** @todo This is currently only forced off on 32-bit hosts only because it
         *        makes a lof of difference there (REM and Solaris performance).
         */

        ComPtr<IGuestOSType> guestOSType;
        hrc = virtualBox->GetGuestOSType(osTypeId, guestOSType.asOutParam());       H();

        BOOL fSupportsLongMode = false;
        hrc = host->GetProcessorFeature(ProcessorFeature_LongMode,
                                        &fSupportsLongMode);                        H();
        BOOL fIs64BitGuest = false;
        hrc = guestOSType->COMGETTER(Is64Bit)(&fIs64BitGuest);                      H();

        if (fSupportsLongMode && fIs64BitGuest)
        {
            rc = CFGMR3InsertInteger(pHWVirtExt, "64bitEnabled", 1);                RC_CHECK();
#if ARCH_BITS == 32 /* The recompiler must use VBoxREM64 (32-bit host only). */
            PCFGMNODE pREM;
            rc = CFGMR3InsertNode(pRoot, "REM", &pREM);                             RC_CHECK();
            rc = CFGMR3InsertInteger(pREM, "64bitEnabled", 1);                      RC_CHECK();
#endif
        }
#if ARCH_BITS == 32 /* 32-bit guests only. */
        else
        {
            rc = CFGMR3InsertInteger(pHWVirtExt, "64bitEnabled", 0);                RC_CHECK();
        }
#endif

        /* @todo Not exactly pretty to check strings; VBOXOSTYPE would be better, but that requires quite a bit of API change in Main. */
        if (    !fIs64BitGuest
            &&  fIOAPIC
            &&  (   osTypeId == "WindowsNT4"
                 || osTypeId == "Windows2000"
                 || osTypeId == "WindowsXP"
                 || osTypeId == "Windows2003"))
        {
            /* Only allow TPR patching for NT, Win2k, XP and Windows Server 2003. (32 bits mode)
             * We may want to consider adding more guest OSes (Solaris) later on.
             */
            rc = CFGMR3InsertInteger(pHWVirtExt, "TPRPatchingEnabled", 1);          RC_CHECK();
        }
    }

    /* HWVirtEx exclusive mode */
    BOOL fHWVirtExExclusive = true;
    hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_Exclusive, &fHWVirtExExclusive);  H();
    rc = CFGMR3InsertInteger(pHWVirtExt, "Exclusive", fHWVirtExExclusive);                     RC_CHECK();

    /* Nested paging (VT-x/AMD-V) */
    BOOL fEnableNestedPaging = false;
    hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_NestedPaging, &fEnableNestedPaging);   H();
    rc = CFGMR3InsertInteger(pHWVirtExt, "EnableNestedPaging", fEnableNestedPaging);     RC_CHECK();

    /* VPID (VT-x) */
    BOOL fEnableVPID = false;
    hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_VPID, &fEnableVPID);        H();
    rc = CFGMR3InsertInteger(pHWVirtExt, "EnableVPID", fEnableVPID);                     RC_CHECK();

    /* Physical Address Extension (PAE) */
    BOOL fEnablePAE = false;
    hrc = pMachine->GetCpuProperty(CpuPropertyType_PAE, &fEnablePAE);               H();
    rc = CFGMR3InsertInteger(pRoot, "EnablePAE", fEnablePAE);                       RC_CHECK();

    /* Synthetic CPU */
    BOOL fSyntheticCpu = false;
    hrc = pMachine->GetCpuProperty(CpuPropertyType_Synthetic, &fSyntheticCpu);      H();
    rc = CFGMR3InsertInteger(pRoot, "SyntheticCpu", fSyntheticCpu);                 RC_CHECK();

    BOOL fPXEDebug;
    hrc = biosSettings->COMGETTER(PXEDebugEnabled)(&fPXEDebug);                      H();

    /*
     * PDM config.
     *  Load drivers in VBoxC.[so|dll]
     */
    PCFGMNODE pPDM;
    PCFGMNODE pDrivers;
    PCFGMNODE pMod;
    rc = CFGMR3InsertNode(pRoot,    "PDM", &pPDM);                                     RC_CHECK();
    rc = CFGMR3InsertNode(pPDM,     "Drivers", &pDrivers);                             RC_CHECK();
    rc = CFGMR3InsertNode(pDrivers, "VBoxC", &pMod);                                   RC_CHECK();
#ifdef VBOX_WITH_XPCOM
    // VBoxC is located in the components subdirectory
    char szPathVBoxC[RTPATH_MAX];
    rc = RTPathAppPrivateArch(szPathVBoxC, RTPATH_MAX - sizeof("/components/VBoxC")); AssertRC(rc);
    strcat(szPathVBoxC, "/components/VBoxC");
    rc = CFGMR3InsertString(pMod,   "Path",  szPathVBoxC);                             RC_CHECK();
#else
    rc = CFGMR3InsertString(pMod,   "Path",  "VBoxC");                                 RC_CHECK();
#endif

    /*
     * Devices
     */
    PCFGMNODE pDevices = NULL;      /* /Devices */
    PCFGMNODE pDev = NULL;          /* /Devices/Dev/ */
    PCFGMNODE pInst = NULL;         /* /Devices/Dev/0/ */
    PCFGMNODE pCfg = NULL;          /* /Devices/Dev/.../Config/ */
    PCFGMNODE pLunL0 = NULL;        /* /Devices/Dev/0/LUN#0/ */
    PCFGMNODE pLunL1 = NULL;        /* /Devices/Dev/0/LUN#0/AttachedDriver/ */
    PCFGMNODE pLunL2 = NULL;        /* /Devices/Dev/0/LUN#0/AttachedDriver/Config/ */
    PCFGMNODE pBiosCfg = NULL;      /* /Devices/pcbios/0/Config/ */

    rc = CFGMR3InsertNode(pRoot, "Devices", &pDevices);                             RC_CHECK();

    /*
     * PC Arch.
     */
    rc = CFGMR3InsertNode(pDevices, "pcarch", &pDev);                               RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    /*
     * The time offset
     */
    LONG64 timeOffset;
    hrc = biosSettings->COMGETTER(TimeOffset)(&timeOffset);                         H();
    PCFGMNODE pTMNode;
    rc = CFGMR3InsertNode(pRoot, "TM", &pTMNode);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pTMNode, "UTCOffset", timeOffset * 1000000);           RC_CHECK();

    /*
     * DMA
     */
    rc = CFGMR3InsertNode(pDevices, "8237A", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted", 1);                  /* boolean */   RC_CHECK();

    /*
     * PCI buses.
     */
    rc = CFGMR3InsertNode(pDevices, "pci", &pDev); /* piix3 */                      RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                             RC_CHECK();

#if 0 /* enable this to test PCI bridging */
    rc = CFGMR3InsertNode(pDevices, "pcibridge", &pDev);                            RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",         14);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIBusNo",             0);/* -> pci[0] */      RC_CHECK();

    rc = CFGMR3InsertNode(pDev,     "1", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          1);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIBusNo",             1);/* ->pcibridge[0] */ RC_CHECK();

    rc = CFGMR3InsertNode(pDev,     "2", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          3);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIBusNo",             1);/* ->pcibridge[0] */ RC_CHECK();
#endif

    /*
     * Temporary hack for enabling the next three devices and various ACPI features.
     */
    Bstr tmpStr2;
    hrc = pMachine->GetExtraData(Bstr("VBoxInternal2/SupportExtHwProfile"), tmpStr2.asOutParam()); H();
    BOOL fExtProfile = tmpStr2 == Bstr("on");

    /*
     * High Precision Event Timer (HPET)
     */
    BOOL fHpetEnabled;
#ifdef VBOX_WITH_HPET
    fHpetEnabled = fExtProfile;
#else
    fHpetEnabled = false;
#endif
    if (fHpetEnabled)
    {
        rc = CFGMR3InsertNode(pDevices, "hpet", &pDev);                      RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                        RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",   1);     /* boolean */   RC_CHECK();
    }

    /*
     * System Management Controller (SMC)
     */
    BOOL fSmcEnabled;
#ifdef VBOX_WITH_SMC
    fSmcEnabled = fExtProfile;
#else
    fSmcEnabled = false;
#endif
    if (fSmcEnabled)
    {
        rc = CFGMR3InsertNode(pDevices, "smc", &pDev);                       RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                        RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",   1);     /* boolean */   RC_CHECK();
    }

    /*
     * Low Pin Count (LPC) bus
     */
    BOOL fLpcEnabled;
    /** @todo: implement appropriate getter */
#ifdef VBOX_WITH_LPC
    fLpcEnabled = fExtProfile;
#else
    fLpcEnabled = false;
#endif
    if (fLpcEnabled)
    {
        rc = CFGMR3InsertNode(pDevices, "lpc", &pDev);                       RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                        RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",   1);     /* boolean */   RC_CHECK();
    }

    /*
     * PS/2 keyboard & mouse.
     */
    rc = CFGMR3InsertNode(pDevices, "pckbd", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "KeyboardQueue");       RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            64);                    RC_CHECK();

    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                     RC_CHECK();
    rc = CFGMR3InsertString(pLunL1, "Driver",               "MainKeyboard");        RC_CHECK();
    rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                               RC_CHECK();
    Keyboard *pKeyboard = pConsole->mKeyboard;
    rc = CFGMR3InsertInteger(pCfg,  "Object",     (uintptr_t)pKeyboard);            RC_CHECK();

    rc = CFGMR3InsertNode(pInst,    "LUN#1", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MouseQueue");          RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            128);                   RC_CHECK();

    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                     RC_CHECK();
    rc = CFGMR3InsertString(pLunL1, "Driver",               "MainMouse");           RC_CHECK();
    rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                               RC_CHECK();
    Mouse *pMouse = pConsole->mMouse;
    rc = CFGMR3InsertInteger(pCfg,  "Object",     (uintptr_t)pMouse);               RC_CHECK();

    /*
     * i8254 Programmable Interval Timer And Dummy Speaker
     */
    rc = CFGMR3InsertNode(pDevices, "i8254", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
#ifdef DEBUG
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
#endif

    /*
     * i8259 Programmable Interrupt Controller.
     */
    rc = CFGMR3InsertNode(pDevices, "i8259", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    /*
     * Advanced Programmable Interrupt Controller.
     * SMP: Each CPU has a LAPIC, but we have a single device representing all LAPICs states,
     *      thus only single insert
     */
    rc = CFGMR3InsertNode(pDevices, "apic", &pDev);                                 RC_CHECK();
    rc = CFGMR3InsertNode(pDev, "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                             RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "NumCPUs", cCpus);                              RC_CHECK();

    if (fIOAPIC)
    {
        /*
         * I/O Advanced Programmable Interrupt Controller.
         */
        rc = CFGMR3InsertNode(pDevices, "ioapic", &pDev);                           RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",          1);     /* boolean */   RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
    }

    /*
     * RTC MC146818.
     */
    rc = CFGMR3InsertNode(pDevices, "mc146818", &pDev);                             RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    /*
     * VGA.
     */
    rc = CFGMR3InsertNode(pDevices, "vga", &pDev);                                  RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          2);                     RC_CHECK();
    Assert(!afPciDeviceNo[2]);
    afPciDeviceNo[2] = true;
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    ULONG cVRamMBs;
    hrc = pMachine->COMGETTER(VRAMSize)(&cVRamMBs);                                 H();
    rc = CFGMR3InsertInteger(pCfg,  "VRamSize",             cVRamMBs * _1M);        RC_CHECK();
    ULONG cMonitorCount;
    hrc = pMachine->COMGETTER(MonitorCount)(&cMonitorCount);                        H();
    rc = CFGMR3InsertInteger(pCfg,  "MonitorCount",         cMonitorCount);         RC_CHECK();
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE /* not safe here yet. */ /** @todo this needs fixing !!! No wonder VGA is slooooooooow on 32-bit darwin! */
    rc = CFGMR3InsertInteger(pCfg,  "R0Enabled",            fHWVirtExEnabled);      RC_CHECK();
#endif

    /*
     * BIOS logo
     */
    BOOL fFadeIn;
    hrc = biosSettings->COMGETTER(LogoFadeIn)(&fFadeIn);                            H();
    rc = CFGMR3InsertInteger(pCfg,  "FadeIn",  fFadeIn ? 1 : 0);                    RC_CHECK();
    BOOL fFadeOut;
    hrc = biosSettings->COMGETTER(LogoFadeOut)(&fFadeOut);                          H();
    rc = CFGMR3InsertInteger(pCfg,  "FadeOut", fFadeOut ? 1: 0);                    RC_CHECK();
    ULONG logoDisplayTime;
    hrc = biosSettings->COMGETTER(LogoDisplayTime)(&logoDisplayTime);               H();
    rc = CFGMR3InsertInteger(pCfg,  "LogoTime", logoDisplayTime);                   RC_CHECK();
    Bstr logoImagePath;
    hrc = biosSettings->COMGETTER(LogoImagePath)(logoImagePath.asOutParam());       H();
    rc = CFGMR3InsertString(pCfg,   "LogoFile", logoImagePath ? Utf8Str(logoImagePath).c_str() : ""); RC_CHECK();

    /*
     * Boot menu
     */
    BIOSBootMenuMode_T eBootMenuMode;
    int iShowBootMenu;
    biosSettings->COMGETTER(BootMenuMode)(&eBootMenuMode);
    switch (eBootMenuMode)
    {
        case BIOSBootMenuMode_Disabled: iShowBootMenu = 0;  break;
        case BIOSBootMenuMode_MenuOnly: iShowBootMenu = 1;  break;
        default:                        iShowBootMenu = 2;  break;
    }
    rc = CFGMR3InsertInteger(pCfg, "ShowBootMenu", iShowBootMenu);                  RC_CHECK();

    /* Custom VESA mode list */
    unsigned cModes = 0;
    for (unsigned iMode = 1; iMode <= 16; ++iMode)
    {
        char szExtraDataKey[sizeof("CustomVideoModeXX")];
        RTStrPrintf(szExtraDataKey, sizeof(szExtraDataKey), "CustomVideoMode%u", iMode);
        hrc = pMachine->GetExtraData(Bstr(szExtraDataKey), &str);                   H();
        if (!str || !*str)
            break;
        rc = CFGMR3InsertStringW(pCfg, szExtraDataKey, str);                        RC_CHECK();
        STR_FREE();
        ++cModes;
    }
    STR_FREE();
    rc = CFGMR3InsertInteger(pCfg,  "CustomVideoModes", cModes);

    /* VESA height reduction */
    ULONG ulHeightReduction;
    IFramebuffer *pFramebuffer = pConsole->getDisplay()->getFramebuffer();
    if (pFramebuffer)
    {
        hrc = pFramebuffer->COMGETTER(HeightReduction)(&ulHeightReduction);         H();
    }
    else
    {
        /* If framebuffer is not available, there is no height reduction. */
        ulHeightReduction = 0;
    }
    rc = CFGMR3InsertInteger(pCfg,  "HeightReduction", ulHeightReduction);          RC_CHECK();

    /* Attach the display. */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainDisplay");         RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    Display *pDisplay = pConsole->mDisplay;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pDisplay);                 RC_CHECK();


    /*
     * Firmware.
     */
#ifdef VBOX_WITH_EFI
    Bstr tmpStr1;
    hrc = pMachine->GetExtraData(Bstr("VBoxInternal2/UseEFI"), tmpStr1.asOutParam());    H();
    BOOL fEfiEnabled = !tmpStr1.isEmpty();

    /**
     * @todo: VBoxInternal2/UseEFI extradata will go away soon, and we'll
     *        just use this code
     */
    if (!fEfiEnabled)
    {
      FirmwareType_T eType =  FirmwareType_BIOS;
      hrc = pMachine->COMGETTER(FirmwareType)(&eType);                                H();
      fEfiEnabled = (eType == FirmwareType_EFI);
    }
#else
    BOOL fEfiEnabled = false;
#endif
    if (!fEfiEnabled)
    {
        /*
         * PC Bios.
         */
        rc = CFGMR3InsertNode(pDevices, "pcbios", &pDev);                               RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pBiosCfg);                           RC_CHECK();
        rc = CFGMR3InsertInteger(pBiosCfg,  "RamSize",              cbRam);             RC_CHECK();
        rc = CFGMR3InsertInteger(pBiosCfg,  "RamHoleSize",          cbRamHole);         RC_CHECK();
        rc = CFGMR3InsertInteger(pBiosCfg,  "NumCPUs",              cCpus);             RC_CHECK();
        rc = CFGMR3InsertString(pBiosCfg,   "HardDiskDevice",       "piix3ide");        RC_CHECK();
        rc = CFGMR3InsertString(pBiosCfg,   "FloppyDevice",         "i82078");          RC_CHECK();
        rc = CFGMR3InsertInteger(pBiosCfg,  "IOAPIC",               fIOAPIC);           RC_CHECK();
        rc = CFGMR3InsertInteger(pBiosCfg,  "PXEDebug",             fPXEDebug);         RC_CHECK();
        rc = CFGMR3InsertBytes(pBiosCfg,    "UUID", pUuid, sizeof(*pUuid));             RC_CHECK();

        DeviceType_T bootDevice;
        if (SchemaDefs::MaxBootPosition > 9)
        {
            AssertMsgFailed (("Too many boot devices %d\n",
                              SchemaDefs::MaxBootPosition));
            return VERR_INVALID_PARAMETER;
        }

        for (ULONG pos = 1; pos <= SchemaDefs::MaxBootPosition; ++pos)
        {
            hrc = pMachine->GetBootOrder(pos, &bootDevice);                             H();

            char szParamName[] = "BootDeviceX";
            szParamName[sizeof (szParamName) - 2] = ((char (pos - 1)) + '0');

            const char *pszBootDevice;
            switch (bootDevice)
            {
                case DeviceType_Null:
                    pszBootDevice = "NONE";
                    break;
                case DeviceType_HardDisk:
                    pszBootDevice = "IDE";
                    break;
                case DeviceType_DVD:
                    pszBootDevice = "DVD";
                    break;
                case DeviceType_Floppy:
                    pszBootDevice = "FLOPPY";
                    break;
                case DeviceType_Network:
                    pszBootDevice = "LAN";
                    break;
                default:
                    AssertMsgFailed(("Invalid bootDevice=%d\n", bootDevice));
                    return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                      N_("Invalid boot device '%d'"), bootDevice);
            }
            rc = CFGMR3InsertString(pBiosCfg, szParamName, pszBootDevice);              RC_CHECK();
        }
    }
    else
    {
        /*
         * EFI.
         */
        rc = CFGMR3InsertNode(pDevices, "efi", &pDev);                              RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted", 1);              /* boolean */   RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamSize",          cbRam);                 RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamHoleSize",      cbRamHole);             RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "NumCPUs",          cCpus);                 RC_CHECK();
    }

    /*
     * Storage controllers.
     */
    com::SafeIfaceArray<IStorageController> ctrls;
    hrc = pMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));   H();

    for (size_t i = 0; i < ctrls.size(); ++ i)
    {
        PCFGMNODE               pCtlInst = NULL;    /* /Devices/<name>/0/ */
        StorageControllerType_T enmCtrlType;
        StorageBus_T            enmBus;
        bool                    fSCSI = false;
        Bstr                    controllerName;

        rc = ctrls[i]->COMGETTER(ControllerType)(&enmCtrlType);                     H();
        rc = ctrls[i]->COMGETTER(Bus)(&enmBus);                                     H();
        rc = ctrls[i]->COMGETTER(Name)(controllerName.asOutParam());                H();

        const char *pszCtrlDev = pConsole->controllerTypeToDev(enmCtrlType);

        rc = CFGMR3InsertNode(pDevices, pszCtrlDev, &pDev);                         RC_CHECK();
        /** @todo support multiple instances of a controller */
        rc = CFGMR3InsertNode(pDev,     "0", &pCtlInst);                            RC_CHECK();

        switch (enmCtrlType)
        {
            case StorageControllerType_LsiLogic:
            {
                rc = CFGMR3InsertInteger(pCtlInst, "Trusted",              1);         RC_CHECK();
                rc = CFGMR3InsertInteger(pCtlInst, "PCIDeviceNo",          20);        RC_CHECK();
                Assert(!afPciDeviceNo[20]);
                afPciDeviceNo[20] = true;
                rc = CFGMR3InsertInteger(pCtlInst, "PCIFunctionNo",        0);         RC_CHECK();
                rc = CFGMR3InsertNode(pCtlInst,    "Config", &pCfg);                   RC_CHECK();
                fSCSI = true;

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst, "LUN#999", &pLunL0);                              RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");            RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                                 RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapSCSILeds[0]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                   RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Last",     15);                                  RC_CHECK();
                break;
            }

            case StorageControllerType_BusLogic:
            {
                rc = CFGMR3InsertInteger(pCtlInst, "Trusted",              1);         RC_CHECK();
                rc = CFGMR3InsertInteger(pCtlInst, "PCIDeviceNo",          21);        RC_CHECK();
                Assert(!afPciDeviceNo[21]);
                afPciDeviceNo[21] = true;
                rc = CFGMR3InsertInteger(pCtlInst, "PCIFunctionNo",        0);         RC_CHECK();
                rc = CFGMR3InsertNode(pCtlInst,    "Config", &pCfg);                   RC_CHECK();
                fSCSI = true;

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst, "LUN#999", &pLunL0);                              RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");            RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                                 RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapSCSILeds[0]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                   RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Last",     15);                                  RC_CHECK();
                break;
            }

            case StorageControllerType_IntelAhci:
            {
                rc = CFGMR3InsertInteger(pCtlInst, "Trusted",              1);         RC_CHECK();
                rc = CFGMR3InsertInteger(pCtlInst, "PCIDeviceNo",          13);        RC_CHECK();
                Assert(!afPciDeviceNo[13]);
                afPciDeviceNo[13] = true;
                rc = CFGMR3InsertInteger(pCtlInst, "PCIFunctionNo",        0);         RC_CHECK();
                rc = CFGMR3InsertNode(pCtlInst,    "Config", &pCfg);                   RC_CHECK();

                ULONG cPorts = 0;
                hrc = ctrls[i]->COMGETTER(PortCount)(&cPorts);                          H();
                rc = CFGMR3InsertInteger(pCfg, "PortCount", cPorts);                    RC_CHECK();

                /* Needed configuration values for the bios. */
                if (pBiosCfg)
                {
                    rc = CFGMR3InsertString(pBiosCfg, "SataHardDiskDevice", "ahci");        RC_CHECK();
                }

                for (uint32_t j = 0; j < 4; ++j)
                {
                    static const char *s_apszConfig[4] =
                    { "PrimaryMaster", "PrimarySlave", "SecondaryMaster", "SecondarySlave" };
                    static const char *s_apszBiosConfig[4] =
                    { "SataPrimaryMasterLUN", "SataPrimarySlaveLUN", "SataSecondaryMasterLUN", "SataSecondarySlaveLUN" };

                    LONG lPortNumber = -1;
                    hrc = ctrls[i]->GetIDEEmulationPort(j, &lPortNumber);                   H();
                    rc = CFGMR3InsertInteger(pCfg, s_apszConfig[j], lPortNumber);           RC_CHECK();
                    if (pBiosCfg)
                    {
                        rc = CFGMR3InsertInteger(pBiosCfg, s_apszBiosConfig[j], lPortNumber);   RC_CHECK();
                    }
                }

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst, "LUN#999", &pLunL0);                              RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");            RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                                 RC_CHECK();
                AssertRelease(cPorts <= RT_ELEMENTS(pConsole->mapSATALeds));
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapSATALeds[0]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                   RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Last",     cPorts - 1);                          RC_CHECK();
                break;
            }

            case StorageControllerType_PIIX3:
            case StorageControllerType_PIIX4:
            case StorageControllerType_ICH6:
            {
                /*
                 * IDE (update this when the main interface changes)
                 */
                rc = CFGMR3InsertInteger(pCtlInst, "Trusted",              1);  /* boolean */   RC_CHECK();
                rc = CFGMR3InsertInteger(pCtlInst, "PCIDeviceNo",          1);                  RC_CHECK();
                Assert(!afPciDeviceNo[1]);
                afPciDeviceNo[1] = true;
                rc = CFGMR3InsertInteger(pCtlInst, "PCIFunctionNo",        1);                  RC_CHECK();
                rc = CFGMR3InsertNode(pCtlInst,    "Config", &pCfg);                            RC_CHECK();
                rc = CFGMR3InsertString(pCfg,  "Type", controllerString(enmCtrlType));          RC_CHECK();

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst,    "LUN#999", &pLunL0);                         RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");          RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapIDELeds[0]);RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                 RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Last",     3);                                 RC_CHECK();
                break;
            }
            case StorageControllerType_I82078:
            {
                /*
                 * i82078 Floppy drive controller
                 */
                fFdcEnabled = true;
                rc = CFGMR3InsertInteger(pCtlInst, "Trusted",   1);                         RC_CHECK();
                rc = CFGMR3InsertNode(pCtlInst,    "Config",    &pCfg);                     RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "IRQ",       6);                            RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "DMA",       2);                            RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "MemMapped", 0 );                           RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "IOBase",    0x3f0);                        RC_CHECK();

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst, "LUN#999", &pLunL0);                            RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");          RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapFDLeds[0]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                 RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                                 RC_CHECK();
                break;
            }

            default:
                AssertMsgFailedReturn(("invalid storage controller type: %d\n", enmCtrlType), VERR_GENERAL_FAILURE);
        }

        /* At the moment we only support one controller per type. So the instance id is always 0. */
        rc = ctrls[i]->COMSETTER(Instance)(0);                                  H();

        /* Attach the hard disks. */
        com::SafeIfaceArray<IMediumAttachment> atts;
        hrc = pMachine->GetMediumAttachmentsOfController(controllerName,
                                                         ComSafeArrayAsOutParam(atts)); H();

        for (size_t j = 0; j < atts.size(); ++ j)
        {
            BOOL fHostDrive = FALSE;

            ComPtr<IMedium> medium;
            hrc = atts [j]->COMGETTER(Medium)(medium.asOutParam());             H();
            LONG lDev;
            hrc = atts[j]->COMGETTER(Device)(&lDev);                            H();
            LONG lPort;
            hrc = atts[j]->COMGETTER(Port)(&lPort);                             H();
            DeviceType_T lType;
            hrc = atts[j]->COMGETTER(Type)(&lType);                             H();

            unsigned uLUN = 0;
            hrc = pConsole->convertBusPortDeviceToLun(enmBus, lPort, lDev, uLUN);   H();
            rc = CFGMR3InsertNodeF(pCtlInst, &pLunL0, "LUN#%u", uLUN);          RC_CHECK();

            /* SCSI has a another driver between device and block. */
            if (fSCSI)
            {
                rc = CFGMR3InsertString(pLunL0, "Driver", "SCSI");              RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                 RC_CHECK();

                rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);       RC_CHECK();
            }

            if (!medium.isNull())
            {
                hrc = medium->COMGETTER(HostDrive)(&fHostDrive);                H();
            }

            if (fHostDrive)
            {
                Assert(!medium.isNull());
                if (lType == DeviceType_DVD)
                {
                    rc = CFGMR3InsertString(pLunL0, "Driver", "HostDVD");           RC_CHECK();
                    rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                 RC_CHECK();

                    hrc = medium->COMGETTER(Location)(&str);                        H();
                    rc = CFGMR3InsertStringW(pCfg, "Path", str);                    RC_CHECK();
                    STR_FREE();

                    BOOL fPassthrough = false;
                    hrc = atts[j]->COMGETTER(Passthrough)(&fPassthrough);           H();
                    rc = CFGMR3InsertInteger(pCfg, "Passthrough", !!fPassthrough);  RC_CHECK();
                }
                else if (lType == DeviceType_Floppy)
                {
                    rc = CFGMR3InsertString(pLunL0, "Driver", "HostFloppy");        RC_CHECK();
                    rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                 RC_CHECK();

                    hrc = medium->COMGETTER(Location)(&str);                        H();
                    rc = CFGMR3InsertStringW(pCfg, "Path", str);                    RC_CHECK();
                    STR_FREE();
                }
            }
            else
            {
                rc = CFGMR3InsertString(pLunL0, "Driver", "Block");                 RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                     RC_CHECK();
                switch (lType)
                {
                    case DeviceType_DVD:
                        rc = CFGMR3InsertString(pCfg, "Type", "DVD");               RC_CHECK();
                        rc = CFGMR3InsertInteger(pCfg, "Mountable", 1);             RC_CHECK();
                        break;
                    case DeviceType_Floppy:
                        rc = CFGMR3InsertString(pCfg, "Type", "Floppy 1.44");       RC_CHECK();
                        rc = CFGMR3InsertInteger(pCfg, "Mountable", 1);             RC_CHECK();
                        break;
                    case DeviceType_HardDisk:
                    default:
                        rc = CFGMR3InsertString(pCfg, "Type", "HardDisk");          RC_CHECK();
                        rc = CFGMR3InsertInteger(pCfg, "Mountable", 0);             RC_CHECK();
                }

                if (!medium.isNull())
                {
                    rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL1);           RC_CHECK();
                    rc = CFGMR3InsertString(pLunL1, "Driver", "VD");                    RC_CHECK();
                    rc = CFGMR3InsertNode(pLunL1, "Config", &pCfg);                     RC_CHECK();

                    hrc = medium->COMGETTER(Location)(&str);                            H();
                    rc = CFGMR3InsertStringW(pCfg, "Path", str);                        RC_CHECK();
                    STR_FREE();

                    hrc = medium->COMGETTER(Format)(&str);                              H();
                    rc = CFGMR3InsertStringW(pCfg, "Format", str);                      RC_CHECK();
                    STR_FREE();

                    /* DVDs are always readonly */
                    if (lType == DeviceType_DVD)
                    {
                        rc = CFGMR3InsertInteger(pCfg, "ReadOnly", 1);                  RC_CHECK();
                    }

                    /* Pass all custom parameters. */
                    bool fHostIP = true;
                    SafeArray<BSTR> names;
                    SafeArray<BSTR> values;
                    hrc = medium->GetProperties(NULL,
                                                ComSafeArrayAsOutParam(names),
                                                ComSafeArrayAsOutParam(values));        H();

                    if (names.size() != 0)
                    {
                        PCFGMNODE pVDC;
                        rc = CFGMR3InsertNode(pCfg, "VDConfig", &pVDC);                 RC_CHECK();
                        for (size_t ii = 0; ii < names.size(); ++ii)
                        {
                            if (values[ii] && *values[ii])
                            {
                                Utf8Str name = names[ii];
                                Utf8Str value = values[ii];
                                rc = CFGMR3InsertString(pVDC, name.c_str(), value.c_str());
                                if (    name.compare("HostIPStack") == 0
                                    &&  value.compare("0") == 0)
                                    fHostIP = false;
                            }
                        }
                    }

                    /* Create an inversed tree of parents. */
                    ComPtr<IMedium> parentMedium = medium;
                    for (PCFGMNODE pParent = pCfg;;)
                    {
                        hrc = parentMedium->COMGETTER(Parent)(medium.asOutParam());     H();
                        if (medium.isNull())
                            break;

                        PCFGMNODE pCur;
                        rc = CFGMR3InsertNode(pParent, "Parent", &pCur);                RC_CHECK();
                        hrc = medium->COMGETTER(Location)(&str);                        H();
                        rc = CFGMR3InsertStringW(pCur, "Path", str);                    RC_CHECK();
                        STR_FREE();

                        hrc = medium->COMGETTER(Format)(&str);                          H();
                        rc = CFGMR3InsertStringW(pCur, "Format", str);                  RC_CHECK();
                        STR_FREE();

                        /* Pass all custom parameters. */
                        SafeArray<BSTR> names;
                        SafeArray<BSTR> values;
                        hrc = medium->GetProperties(NULL,
                                                    ComSafeArrayAsOutParam(names),
                                                    ComSafeArrayAsOutParam(values));    H();

                        if (names.size() != 0)
                        {
                            PCFGMNODE pVDC;
                            rc = CFGMR3InsertNode(pCur, "VDConfig", &pVDC);             RC_CHECK();
                            for (size_t ii = 0; ii < names.size(); ++ii)
                            {
                                if (values[ii])
                                {
                                    Utf8Str name = names[ii];
                                    Utf8Str value = values[ii];
                                    rc = CFGMR3InsertString(pVDC, name.c_str(), value.c_str());
                                    if (    name.compare("HostIPStack") == 0
                                        &&  value.compare("0") == 0)
                                        fHostIP = false;
                                }
                            }
                        }

                        /* Custom code: put marker to not use host IP stack to driver
                         * configuration node. Simplifies life of DrvVD a bit. */
                        if (!fHostIP)
                        {
                            rc = CFGMR3InsertInteger(pCfg, "HostIPStack", 0);           RC_CHECK();
                        }

                        /* next */
                        pParent = pCur;
                        parentMedium = medium;
                    }
                }
            }
        }
        H();
    }
    H();

    /*
     * Network adapters
     */
#ifdef VMWARE_NET_IN_SLOT_11
    bool fSwapSlots3and11 = false;
#endif
    PCFGMNODE pDevPCNet = NULL;          /* PCNet-type devices */
    rc = CFGMR3InsertNode(pDevices, "pcnet", &pDevPCNet);                           RC_CHECK();
#ifdef VBOX_WITH_E1000
    PCFGMNODE pDevE1000 = NULL;          /* E1000-type devices */
    rc = CFGMR3InsertNode(pDevices, "e1000", &pDevE1000);                           RC_CHECK();
#endif
#ifdef VBOX_WITH_VIRTIO
    PCFGMNODE pDevVirtioNet = NULL;          /* Virtio network devices */
    rc = CFGMR3InsertNode(pDevices, "virtio-net", &pDevVirtioNet);                  RC_CHECK();
#endif /* VBOX_WITH_VIRTIO */
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::NetworkAdapterCount; ++ulInstance)
    {
        ComPtr<INetworkAdapter> networkAdapter;
        hrc = pMachine->GetNetworkAdapter(ulInstance, networkAdapter.asOutParam()); H();
        BOOL fEnabled = FALSE;
        hrc = networkAdapter->COMGETTER(Enabled)(&fEnabled);                        H();
        if (!fEnabled)
            continue;

        /*
         * The virtual hardware type. Create appropriate device first.
         */
        const char *pszAdapterName = "pcnet";
        NetworkAdapterType_T adapterType;
        hrc = networkAdapter->COMGETTER(AdapterType)(&adapterType);                 H();
        switch (adapterType)
        {
            case NetworkAdapterType_Am79C970A:
            case NetworkAdapterType_Am79C973:
                pDev = pDevPCNet;
                break;
#ifdef VBOX_WITH_E1000
            case NetworkAdapterType_I82540EM:
            case NetworkAdapterType_I82543GC:
            case NetworkAdapterType_I82545EM:
                pDev = pDevE1000;
                pszAdapterName = "e1000";
                break;
#endif
#ifdef VBOX_WITH_VIRTIO
            case NetworkAdapterType_Virtio:
                pDev = pDevVirtioNet;
                pszAdapterName = "virtio";
                break;
#endif /* VBOX_WITH_VIRTIO */
            default:
                AssertMsgFailed(("Invalid network adapter type '%d' for slot '%d'",
                                 adapterType, ulInstance));
                return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                  N_("Invalid network adapter type '%d' for slot '%d'"),
                                  adapterType, ulInstance);
        }

        rc = CFGMR3InsertNodeF(pDev, &pInst, "%u", ulInstance);                     RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",              1); /* boolean */   RC_CHECK();
        /* the first network card gets the PCI ID 3, the next 3 gets 8..10,
         * next 4 get 16..19. */
        unsigned iPciDeviceNo = 3;
        if (ulInstance)
        {
            if (ulInstance < 4)
                iPciDeviceNo = ulInstance - 1 + 8;
            else
                iPciDeviceNo = ulInstance - 4 + 16;
        }
#ifdef VMWARE_NET_IN_SLOT_11
        /*
         * Dirty hack for PCI slot compatibility with VMWare,
         * it assigns slot 11 to the first network controller.
         */
        if (iPciDeviceNo == 3 && adapterType == NetworkAdapterType_I82545EM)
        {
            iPciDeviceNo = 0x11;
            fSwapSlots3and11 = true;
        }
        else if (iPciDeviceNo == 0x11 && fSwapSlots3and11)
            iPciDeviceNo = 3;
#endif
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo", iPciDeviceNo);               RC_CHECK();
        Assert(!afPciDeviceNo[iPciDeviceNo]);
        afPciDeviceNo[iPciDeviceNo] = true;
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                 RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                              RC_CHECK();
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE /* not safe here yet. */
        if (pDev == pDevPCNet)
        {
            rc = CFGMR3InsertInteger(pCfg,  "R0Enabled",    false);                 RC_CHECK();
        }
#endif

        /*
         * The virtual hardware type. PCNet supports two types.
         */
        switch (adapterType)
        {
            case NetworkAdapterType_Am79C970A:
                rc = CFGMR3InsertInteger(pCfg, "Am79C973", 0);                      RC_CHECK();
                break;
            case NetworkAdapterType_Am79C973:
                rc = CFGMR3InsertInteger(pCfg, "Am79C973", 1);                      RC_CHECK();
                break;
            case NetworkAdapterType_I82540EM:
                rc = CFGMR3InsertInteger(pCfg, "AdapterType", 0);                   RC_CHECK();
                break;
            case NetworkAdapterType_I82543GC:
                rc = CFGMR3InsertInteger(pCfg, "AdapterType", 1);                   RC_CHECK();
                break;
            case NetworkAdapterType_I82545EM:
                rc = CFGMR3InsertInteger(pCfg, "AdapterType", 2);                   RC_CHECK();
                break;
        }

        /*
         * Get the MAC address and convert it to binary representation
         */
        Bstr macAddr;
        hrc = networkAdapter->COMGETTER(MACAddress)(macAddr.asOutParam());          H();
        Assert(macAddr);
        Utf8Str macAddrUtf8 = macAddr;
        char *macStr = (char*)macAddrUtf8.raw();
        Assert(strlen(macStr) == 12);
        RTMAC Mac;
        memset(&Mac, 0, sizeof(Mac));
        char *pMac = (char*)&Mac;
        for (uint32_t i = 0; i < 6; ++i)
        {
            char c1 = *macStr++ - '0';
            if (c1 > 9)
                c1 -= 7;
            char c2 = *macStr++ - '0';
            if (c2 > 9)
                c2 -= 7;
            *pMac++ = ((c1 & 0x0f) << 4) | (c2 & 0x0f);
        }
        rc = CFGMR3InsertBytes(pCfg, "MAC", &Mac, sizeof(Mac));                     RC_CHECK();

        /*
         * Check if the cable is supposed to be unplugged
         */
        BOOL fCableConnected;
        hrc = networkAdapter->COMGETTER(CableConnected)(&fCableConnected);          H();
        rc = CFGMR3InsertInteger(pCfg, "CableConnected", fCableConnected ? 1 : 0);  RC_CHECK();

        /*
         * Line speed to report from custom drivers
         */
        ULONG ulLineSpeed;
        hrc = networkAdapter->COMGETTER(LineSpeed)(&ulLineSpeed);                   H();
        rc = CFGMR3InsertInteger(pCfg, "LineSpeed", ulLineSpeed);                   RC_CHECK();

        /*
         * Attach the status driver.
         */
        rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                        RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");      RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapNetworkLeds[ulInstance]); RC_CHECK();

        /*
         * Configure the network card now
         */
        rc = configNetwork(pConsole, pszAdapterName, ulInstance, 0, networkAdapter,
                           pCfg, pLunL0, pInst, false /*fAttachDetach*/);           RC_CHECK();
    }

    /*
     * Serial (UART) Ports
     */
    rc = CFGMR3InsertNode(pDevices, "serial", &pDev);                               RC_CHECK();
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::SerialPortCount; ++ulInstance)
    {
        ComPtr<ISerialPort> serialPort;
        hrc = pMachine->GetSerialPort (ulInstance, serialPort.asOutParam());        H();
        BOOL fEnabled = FALSE;
        if (serialPort)
            hrc = serialPort->COMGETTER(Enabled)(&fEnabled);                        H();
        if (!fEnabled)
            continue;

        rc = CFGMR3InsertNodeF(pDev, &pInst, "%u", ulInstance);                     RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                              RC_CHECK();

        ULONG ulIRQ;
        hrc = serialPort->COMGETTER(IRQ)(&ulIRQ);                                   H();
        rc = CFGMR3InsertInteger(pCfg,   "IRQ", ulIRQ);                             RC_CHECK();
        ULONG ulIOBase;
        hrc = serialPort->COMGETTER(IOBase)(&ulIOBase);                             H();
        rc = CFGMR3InsertInteger(pCfg,   "IOBase", ulIOBase);                       RC_CHECK();
        BOOL  fServer;
        hrc = serialPort->COMGETTER(Server)(&fServer);                              H();
        hrc = serialPort->COMGETTER(Path)(&str);                                    H();
        PortMode_T eHostMode;
        hrc = serialPort->COMGETTER(HostMode)(&eHostMode);                          H();
        if (eHostMode != PortMode_Disconnected)
        {
            rc = CFGMR3InsertNode(pInst,     "LUN#0", &pLunL0);                     RC_CHECK();
            if (eHostMode == PortMode_HostPipe)
            {
                rc = CFGMR3InsertString(pLunL0,  "Driver", "Char");                 RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,    "AttachedDriver", &pLunL1);        RC_CHECK();
                rc = CFGMR3InsertString(pLunL1,  "Driver", "NamedPipe");            RC_CHECK();
                rc = CFGMR3InsertNode(pLunL1,    "Config", &pLunL2);                RC_CHECK();
                rc = CFGMR3InsertStringW(pLunL2, "Location", str);                  RC_CHECK();
                rc = CFGMR3InsertInteger(pLunL2, "IsServer", fServer);              RC_CHECK();
            }
            else if (eHostMode == PortMode_HostDevice)
            {
                rc = CFGMR3InsertString(pLunL0,  "Driver", "Host Serial");          RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,    "Config", &pLunL1);                RC_CHECK();
                rc = CFGMR3InsertStringW(pLunL1, "DevicePath", str);                RC_CHECK();
            }
            else if (eHostMode == PortMode_RawFile)
            {
                rc = CFGMR3InsertString(pLunL0,  "Driver", "Char");                 RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,    "AttachedDriver", &pLunL1);        RC_CHECK();
                rc = CFGMR3InsertString(pLunL1,  "Driver", "RawFile");              RC_CHECK();
                rc = CFGMR3InsertNode(pLunL1,    "Config", &pLunL2);                RC_CHECK();
                rc = CFGMR3InsertStringW(pLunL2, "Location", str);                  RC_CHECK();
            }
        }
        STR_FREE();
    }

    /*
     * Parallel (LPT) Ports
     */
    rc = CFGMR3InsertNode(pDevices, "parallel", &pDev);                             RC_CHECK();
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::ParallelPortCount; ++ulInstance)
    {
        ComPtr<IParallelPort> parallelPort;
        hrc = pMachine->GetParallelPort(ulInstance, parallelPort.asOutParam());     H();
        BOOL fEnabled = FALSE;
        if (parallelPort)
        {
            hrc = parallelPort->COMGETTER(Enabled)(&fEnabled);                      H();
        }
        if (!fEnabled)
            continue;

        rc = CFGMR3InsertNodeF(pDev, &pInst, "%u", ulInstance);                     RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                              RC_CHECK();

        ULONG ulIRQ;
        hrc = parallelPort->COMGETTER(IRQ)(&ulIRQ);                                 H();
        rc = CFGMR3InsertInteger(pCfg,   "IRQ", ulIRQ);                             RC_CHECK();
        ULONG ulIOBase;
        hrc = parallelPort->COMGETTER(IOBase)(&ulIOBase);                           H();
        rc = CFGMR3InsertInteger(pCfg,   "IOBase", ulIOBase);                       RC_CHECK();
        rc = CFGMR3InsertNode(pInst,     "LUN#0", &pLunL0);                         RC_CHECK();
        rc = CFGMR3InsertString(pLunL0,  "Driver", "HostParallel");                 RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,    "AttachedDriver", &pLunL1);                RC_CHECK();
        hrc = parallelPort->COMGETTER(Path)(&str);                                  H();
        rc = CFGMR3InsertStringW(pLunL1,  "DevicePath", str);                       RC_CHECK();
        STR_FREE();
    }

    /*
     * VMM Device
     */
    rc = CFGMR3InsertNode(pDevices, "VMMDev", &pDev);                               RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          4);                     RC_CHECK();
    Assert(!afPciDeviceNo[4]);
    afPciDeviceNo[4] = true;
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
    Bstr hwVersion;
    hrc = pMachine->COMGETTER(HardwareVersion)(hwVersion.asOutParam());             H();
    if (hwVersion.compare(Bstr("1")) == 0) /* <= 2.0.x */
    {
        CFGMR3InsertInteger(pCfg, "HeapEnabled", 0);                                RC_CHECK();
    }

    /* the VMM device's Main driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "HGCM");                RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    VMMDev *pVMMDev = pConsole->mVMMDev;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pVMMDev);                  RC_CHECK();

    /*
     * Attach the status driver.
     */
    rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                            RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");          RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapSharedFolderLed); RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                 RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                                 RC_CHECK();

    /*
     * Audio Sniffer Device
     */
    rc = CFGMR3InsertNode(pDevices, "AudioSniffer", &pDev);                         RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    /* the Audio Sniffer device's Main driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainAudioSniffer");    RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    AudioSniffer *pAudioSniffer = pConsole->mAudioSniffer;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pAudioSniffer);            RC_CHECK();

    /*
     * AC'97 ICH / SoundBlaster16 audio
     */
    BOOL enabled;
    ComPtr<IAudioAdapter> audioAdapter;
    hrc = pMachine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());             H();
    if (audioAdapter)
        hrc = audioAdapter->COMGETTER(Enabled)(&enabled);                           H();

    if (enabled)
    {
        AudioControllerType_T audioController;
        hrc = audioAdapter->COMGETTER(AudioController)(&audioController);           H();
        switch (audioController)
        {
            case AudioControllerType_AC97:
            {
                /* default: ICH AC97 */
                rc = CFGMR3InsertNode(pDevices, "ichac97", &pDev);                  RC_CHECK();
                rc = CFGMR3InsertNode(pDev,     "0", &pInst);
                rc = CFGMR3InsertInteger(pInst, "Trusted",          1); /* bool */  RC_CHECK();
                rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",      5);             RC_CHECK();
                Assert(!afPciDeviceNo[5]);
                afPciDeviceNo[5] = true;
                rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",    0);             RC_CHECK();
                rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                   RC_CHECK();
                break;
            }
            case AudioControllerType_SB16:
            {
                /* legacy SoundBlaster16 */
                rc = CFGMR3InsertNode(pDevices, "sb16", &pDev);                     RC_CHECK();
                rc = CFGMR3InsertNode(pDev,     "0", &pInst);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pInst, "Trusted",          1); /* bool */  RC_CHECK();
                rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                   RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "IRQ", 5);                          RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "DMA", 1);                          RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "DMA16", 5);                        RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Port", 0x220);                     RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Version", 0x0405);                 RC_CHECK();
                break;
            }
        }

        /* the Audio driver */
        rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                          RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "AUDIO");           RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();

        AudioDriverType_T audioDriver;
        hrc = audioAdapter->COMGETTER(AudioDriver)(&audioDriver);                   H();
        switch (audioDriver)
        {
            case AudioDriverType_Null:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "null");               RC_CHECK();
                break;
            }
#ifdef RT_OS_WINDOWS
#ifdef VBOX_WITH_WINMM
            case AudioDriverType_WinMM:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "winmm");              RC_CHECK();
                break;
            }
#endif
            case AudioDriverType_DirectSound:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "dsound");             RC_CHECK();
                break;
            }
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_SOLARIS
            case AudioDriverType_SolAudio:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "solaudio");           RC_CHECK();
                break;
            }
#endif
#ifdef RT_OS_LINUX
# ifdef VBOX_WITH_ALSA
            case AudioDriverType_ALSA:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "alsa");               RC_CHECK();
                break;
            }
# endif
# ifdef VBOX_WITH_PULSE
            case AudioDriverType_Pulse:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "pulse");              RC_CHECK();
                break;
            }
# endif
#endif /* RT_OS_LINUX */
#if defined (RT_OS_LINUX) || defined (RT_OS_FREEBSD) || defined(VBOX_WITH_SOLARIS_OSS)
            case AudioDriverType_OSS:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "oss");                RC_CHECK();
                break;
            }
#endif
#ifdef RT_OS_DARWIN
            case AudioDriverType_CoreAudio:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "coreaudio");          RC_CHECK();
                break;
            }
#endif
        }
        hrc = pMachine->COMGETTER(Name)(&str);                                      H();
        rc = CFGMR3InsertStringW(pCfg, "StreamName", str);                          RC_CHECK();
        STR_FREE();
    }

    /*
     * The USB Controller.
     */
    ComPtr<IUSBController> USBCtlPtr;
    hrc = pMachine->COMGETTER(USBController)(USBCtlPtr.asOutParam());
    if (USBCtlPtr)
    {
        BOOL fEnabled;
        hrc = USBCtlPtr->COMGETTER(Enabled)(&fEnabled);                             H();
        if (fEnabled)
        {
            rc = CFGMR3InsertNode(pDevices, "usb-ohci", &pDev);                     RC_CHECK();
            rc = CFGMR3InsertNode(pDev,     "0", &pInst);                           RC_CHECK();
            rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                       RC_CHECK();
            rc = CFGMR3InsertInteger(pInst, "Trusted",              1); /* boolean */   RC_CHECK();
            rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          6);                 RC_CHECK();
            Assert(!afPciDeviceNo[6]);
            afPciDeviceNo[6] = true;
            rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                 RC_CHECK();

            rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                          RC_CHECK();
            rc = CFGMR3InsertString(pLunL0, "Driver",               "VUSBRootHub");     RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();

            /*
             * Attach the status driver.
             */
            rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                        RC_CHECK();
            rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");      RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapUSBLed[0]);RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "First",    0);                             RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                             RC_CHECK();

#ifdef VBOX_WITH_EHCI
            hrc = USBCtlPtr->COMGETTER(EnabledEhci)(&fEnabled);                         H();
            if (fEnabled)
            {
                rc = CFGMR3InsertNode(pDevices, "usb-ehci", &pDev);                     RC_CHECK();
                rc = CFGMR3InsertNode(pDev,     "0", &pInst);                           RC_CHECK();
                rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pInst, "Trusted",              1); /* bool */  RC_CHECK();
                rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          11);            RC_CHECK();
                Assert(!afPciDeviceNo[11]);
                afPciDeviceNo[11] = true;
                rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);             RC_CHECK();

                rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                      RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "VUSBRootHub"); RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();

                /*
                 * Attach the status driver.
                 */
                rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                    RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");  RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapUSBLed[1]);RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                         RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                         RC_CHECK();
            }
            else
#endif
            {
                /*
                 * Global USB options, currently unused as we'll apply the 2.0 -> 1.1 morphing
                 * on a per device level now.
                 */
                rc = CFGMR3InsertNode(pRoot, "USB", &pCfg);                             RC_CHECK();
                rc = CFGMR3InsertNode(pCfg, "USBProxy", &pCfg);                         RC_CHECK();
                rc = CFGMR3InsertNode(pCfg, "GlobalConfig", &pCfg);                     RC_CHECK();
                // This globally enables the 2.0 -> 1.1 device morphing of proxied devies to keep windows quiet.
                //rc = CFGMR3InsertInteger(pCfg, "Force11Device", true);                RC_CHECK();
                // The following breaks stuff, but it makes MSDs work in vista. (I include it here so
                // that it's documented somewhere.) Users needing it can use:
                //      VBoxManage setextradata "myvm" "VBoxInternal/USB/USBProxy/GlobalConfig/Force11PacketSize" 1
                //rc = CFGMR3InsertInteger(pCfg, "Force11PacketSize", true);            RC_CHECK();
            }
        }
    }

    /*
     * Clipboard
     */
    {
        ClipboardMode_T mode = ClipboardMode_Disabled;
        hrc = pMachine->COMGETTER(ClipboardMode)(&mode);                                H();

        if (mode != ClipboardMode_Disabled)
        {
            /* Load the service */
            rc = pConsole->mVMMDev->hgcmLoadService ("VBoxSharedClipboard", "VBoxSharedClipboard");

            if (RT_FAILURE(rc))
            {
                LogRel(("VBoxSharedClipboard is not available. rc = %Rrc\n", rc));
                /* That is not a fatal failure. */
                rc = VINF_SUCCESS;
            }
            else
            {
                /* Setup the service. */
                VBOXHGCMSVCPARM parm;

                parm.type = VBOX_HGCM_SVC_PARM_32BIT;

                switch (mode)
                {
                    default:
                    case ClipboardMode_Disabled:
                    {
                        LogRel(("VBoxSharedClipboard mode: Off\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_OFF;
                        break;
                    }
                    case ClipboardMode_GuestToHost:
                    {
                        LogRel(("VBoxSharedClipboard mode: Guest to Host\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST;
                        break;
                    }
                    case ClipboardMode_HostToGuest:
                    {
                        LogRel(("VBoxSharedClipboard mode: Host to Guest\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST;
                        break;
                    }
                    case ClipboardMode_Bidirectional:
                    {
                        LogRel(("VBoxSharedClipboard mode: Bidirectional\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL;
                        break;
                    }
                }

                pConsole->mVMMDev->hgcmHostCall ("VBoxSharedClipboard", VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE, 1, &parm);

                Log(("Set VBoxSharedClipboard mode\n"));
            }
        }
    }

#ifdef VBOX_WITH_CROGL
    /*
     * crOpenGL
     */
    {
        BOOL fEnabled = false;
        hrc = pMachine->COMGETTER(Accelerate3DEnabled)(&fEnabled); H();

        if (fEnabled)
        {
            /* Load the service */
            rc = pConsole->mVMMDev->hgcmLoadService ("VBoxSharedCrOpenGL", "VBoxSharedCrOpenGL");
            if (RT_FAILURE(rc))
            {
                LogRel(("Failed to load Shared OpenGL service %Rrc\n", rc));
                /* That is not a fatal failure. */
                rc = VINF_SUCCESS;
            }
            else
            {
                LogRel(("Shared crOpenGL service loaded.\n"));

                /* Setup the service. */
                VBOXHGCMSVCPARM parm;
                parm.type = VBOX_HGCM_SVC_PARM_PTR;

                parm.u.pointer.addr = pConsole->getDisplay()->getFramebuffer();
                parm.u.pointer.size = sizeof(IFramebuffer *);

                rc = pConsole->mVMMDev->hgcmHostCall("VBoxSharedCrOpenGL", SHCRGL_HOST_FN_SET_FRAMEBUFFER, 1, &parm);
                if (!RT_SUCCESS(rc))
                    AssertMsgFailed(("SHCRGL_HOST_FN_SET_FRAMEBUFFER failed with %Rrc\n", rc));

                parm.u.pointer.addr = pVM;
                parm.u.pointer.size = sizeof(pVM);
                rc = pConsole->mVMMDev->hgcmHostCall("VBoxSharedCrOpenGL", SHCRGL_HOST_FN_SET_VM, 1, &parm);
                if (!RT_SUCCESS(rc))
                    AssertMsgFailed(("SHCRGL_HOST_FN_SET_VM failed with %Rrc\n", rc));
            }

        }
    }
#endif

#ifdef VBOX_WITH_GUEST_PROPS
    /*
     * Guest property service
     */

    rc = configGuestProperties(pConsole);
#endif /* VBOX_WITH_GUEST_PROPS defined */

    /*
     * CFGM overlay handling.
     *
     * Here we check the extra data entries for CFGM values
     * and create the nodes and insert the values on the fly. Existing
     * values will be removed and reinserted. CFGM is typed, so by default
     * we will guess whether it's a string or an integer (byte arrays are
     * not currently supported). It's possible to override this autodetection
     * by adding "string:", "integer:" or "bytes:" (future).
     *
     * We first perform a run on global extra data, then on the machine
     * extra data to support global settings with local overrides.
     *
     */
    /** @todo add support for removing nodes and byte blobs. */
    SafeArray<BSTR> aGlobalExtraDataKeys;
    SafeArray<BSTR> aMachineExtraDataKeys;
    /*
        * Get the next key
        */
    if (FAILED(hrc = virtualBox->GetExtraDataKeys(ComSafeArrayAsOutParam(aGlobalExtraDataKeys))))
        AssertMsgFailed(("VirtualBox::GetExtraDataKeys failed with %Rrc\n", hrc));

    // remember the no. of global values so we can call the correct method below
    size_t cGlobalValues = aGlobalExtraDataKeys.size();

    if (FAILED(hrc = pMachine->GetExtraDataKeys(ComSafeArrayAsOutParam(aMachineExtraDataKeys))))
        AssertMsgFailed(("IMachine::GetExtraDataKeys failed with %Rrc\n", hrc));

    // build a combined list from global keys...
    std::list<Utf8Str> llExtraDataKeys;
    for (size_t i = 0; i < aGlobalExtraDataKeys.size(); ++i)
        llExtraDataKeys.push_back(Utf8Str(aGlobalExtraDataKeys[i]));
    // ... and machine keys
    for (size_t i = 0; i < aMachineExtraDataKeys.size(); ++i)
        llExtraDataKeys.push_back(Utf8Str(aMachineExtraDataKeys[i]));

    size_t i = 0;
    for (std::list<Utf8Str>::const_iterator it = llExtraDataKeys.begin();
         it != llExtraDataKeys.end();
         ++it, ++i)
    {
        const Utf8Str &strKey = *it;

        /*
         * We only care about keys starting with "VBoxInternal/" (skip "G:" or "M:")
         */
        if (!strKey.startsWith("VBoxInternal/"))
            continue;

        const char *pszExtraDataKey = strKey.raw() + sizeof("VBoxInternal/") - 1;

        // get the value
        Bstr strExtraDataValue;
        if (i < cGlobalValues)
            // this is still one of the global values:
            hrc = virtualBox->GetExtraData(Bstr(strKey), strExtraDataValue.asOutParam());
        else
            hrc = pMachine->GetExtraData(Bstr(strKey), strExtraDataValue.asOutParam());
        if (FAILED(hrc))
            LogRel(("Warning: Cannot get extra data key %s, rc = %Rrc\n", strKey.raw(), hrc));

        /*
         * The key will be in the format "Node1/Node2/Value" or simply "Value".
         * Split the two and get the node, delete the value and create the node
         * if necessary.
         */
        PCFGMNODE pNode;
        const char *pszCFGMValueName = strrchr(pszExtraDataKey, '/');
        if (pszCFGMValueName)
        {
            /* terminate the node and advance to the value (Utf8Str might not
               offically like this but wtf) */
            *(char*)pszCFGMValueName = '\0';
            ++pszCFGMValueName;

            /* does the node already exist? */
            pNode = CFGMR3GetChild(pRoot, pszExtraDataKey);
            if (pNode)
                CFGMR3RemoveValue(pNode, pszCFGMValueName);
            else
            {
                /* create the node */
                rc = CFGMR3InsertNode(pRoot, pszExtraDataKey, &pNode);
                if (RT_FAILURE(rc))
                {
                    AssertLogRelMsgRC(rc, ("failed to insert node '%s'\n", pszExtraDataKey));
                    continue;
                }
                Assert(pNode);
            }
        }
        else
        {
            /* root value (no node path). */
            pNode = pRoot;
            pszCFGMValueName = pszExtraDataKey;
            pszExtraDataKey--;
            CFGMR3RemoveValue(pNode, pszCFGMValueName);
        }

        /*
         * Now let's have a look at the value.
         * Empty strings means that we should remove the value, which we've
         * already done above.
         */
        Utf8Str strCFGMValueUtf8(strExtraDataValue);
        const char *pszCFGMValue = strCFGMValueUtf8.raw();
        if (    pszCFGMValue
            && *pszCFGMValue)
        {
            uint64_t u64Value;

            /* check for type prefix first. */
            if (!strncmp(pszCFGMValue, "string:", sizeof("string:") - 1))
                rc = CFGMR3InsertString(pNode, pszCFGMValueName, pszCFGMValue + sizeof("string:") - 1);
            else if (!strncmp(pszCFGMValue, "integer:", sizeof("integer:") - 1))
            {
                rc = RTStrToUInt64Full(pszCFGMValue + sizeof("integer:") - 1, 0, &u64Value);
                if (RT_SUCCESS(rc))
                    rc = CFGMR3InsertInteger(pNode, pszCFGMValueName, u64Value);
            }
            else if (!strncmp(pszCFGMValue, "bytes:", sizeof("bytes:") - 1))
                rc = VERR_NOT_IMPLEMENTED;
            /* auto detect type. */
            else if (RT_SUCCESS(RTStrToUInt64Full(pszCFGMValue, 0, &u64Value)))
                rc = CFGMR3InsertInteger(pNode, pszCFGMValueName, u64Value);
            else
                rc = CFGMR3InsertString(pNode, pszCFGMValueName, pszCFGMValue);
            AssertLogRelMsgRC(rc, ("failed to insert CFGM value '%s' to key '%s'\n", pszCFGMValue, pszExtraDataKey));
        }
    }

    /*
     * ACPI
     */
    BOOL fACPI;
    hrc = biosSettings->COMGETTER(ACPIEnabled)(&fACPI);                             H();
    if (fACPI)
    {
        BOOL fShowCpu = fExtProfile;
        /* Always show the CPU leafs when we have multiple VCPUs or when the IO-APIC is enabled.
         * The Windows SMP kernel needs a CPU leaf or else its idle loop will burn cpu cycles; the
         * intelppm driver refuses to register an idle state handler.
         */
        if ((cCpus > 1) ||  fIOAPIC)
            fShowCpu = true;

        rc = CFGMR3InsertNode(pDevices, "acpi", &pDev);                             RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted", 1);              /* boolean */   RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamSize",          cbRam);                 RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamHoleSize",      cbRamHole);             RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "NumCPUs",          cCpus);                 RC_CHECK();

        rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                         RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "FdcEnabled", fFdcEnabled);                 RC_CHECK();
#ifdef VBOX_WITH_HPET
        rc = CFGMR3InsertInteger(pCfg,  "HpetEnabled", fHpetEnabled);               RC_CHECK();
#endif
#ifdef VBOX_WITH_SMC
        rc = CFGMR3InsertInteger(pCfg,  "SmcEnabled", fSmcEnabled);                 RC_CHECK();
#endif
        rc = CFGMR3InsertInteger(pCfg,  "ShowRtc", fExtProfile);                    RC_CHECK();

        rc = CFGMR3InsertInteger(pCfg,  "ShowCpu", fShowCpu);                       RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          7);                 RC_CHECK();
        Assert(!afPciDeviceNo[7]);
        afPciDeviceNo[7] = true;
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                 RC_CHECK();

        rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                          RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "ACPIHost");        RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
    }

#undef STR_FREE
#undef H
#undef RC_CHECK

    /* Register VM state change handler */
    int rc2 = VMR3AtStateRegister (pVM, Console::vmstateChangeCallback, pConsole);
    AssertRC (rc2);
    if (RT_SUCCESS(rc))
        rc = rc2;

    /* Register VM runtime error handler */
    rc2 = VMR3AtRuntimeErrorRegister (pVM, Console::setVMRuntimeErrorCallback, pConsole);
    AssertRC (rc2);
    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowFunc (("vrc = %Rrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

/**
 * Ellipsis to va_list wrapper for calling setVMRuntimeErrorCallback.
 */
/*static*/ void Console::setVMRuntimeErrorCallbackF(PVM pVM, void *pvConsole, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    setVMRuntimeErrorCallback(pVM, pvConsole, fFlags, pszErrorId, pszFormat, va);
    va_end(va);
}

/**
 *  Construct the Network configuration tree
 *
 *  @returns VBox status code.
 *
 *  @param   pThis               Pointer to the Console object.
 *  @param   pszDevice           The PDM device name.
 *  @param   uInstance           The PDM device instance.
 *  @param   uLun                The PDM LUN number of the drive.
 *  @param   aNetworkAdapter     The network adapter whose attachment needs to be changed
 *  @param   pCfg                Configuration node for the device
 *  @param   pLunL0              To store the pointer to the LUN#0.
 *  @param   pInst               The instance CFGM node
 *  @param   fAttachDetach       To determine if the network attachment should
 *                               be attached/detached after/before
 *                               configuration.
 *
 *  @note Locks the Console object for writing.
 */
/*static*/ int Console::configNetwork(Console *pThis, const char *pszDevice,
                                      unsigned uInstance, unsigned uLun,
                                      INetworkAdapter *aNetworkAdapter,
                                      PCFGMNODE pCfg, PCFGMNODE pLunL0,
                                      PCFGMNODE pInst, bool fAttachDetach)
{
    int rc = VINF_SUCCESS;

    AutoCaller autoCaller(pThis);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    /*
     * Locking the object before doing VMR3* calls is quite safe here, since
     * we're on EMT. Write lock is necessary because we indirectly modify the
     * meAttachmentType member.
     */
    AutoWriteLock alock(pThis);

    PVM     pVM = pThis->mpVM;
    BSTR    str = NULL;

#define STR_FREE()  do { if (str) { SysFreeString(str); str = NULL; } } while (0)
#define RC_CHECK()  do { if (RT_FAILURE(rc)) { AssertMsgFailed(("rc=%Rrc\n", rc));  STR_FREE(); return rc;                   } } while (0)
#define H()         do { if (FAILED(hrc))    { AssertMsgFailed(("hrc=%#x\n", hrc)); STR_FREE(); return VERR_GENERAL_FAILURE; } } while (0)

    HRESULT hrc;
    ComPtr<IMachine> pMachine = pThis->machine();

    ComPtr<IVirtualBox> virtualBox;
    hrc = pMachine->COMGETTER(Parent)(virtualBox.asOutParam());
    H();

    ComPtr<IHost> host;
    hrc = virtualBox->COMGETTER(Host)(host.asOutParam());
    H();

    BOOL fSniffer;
    hrc = aNetworkAdapter->COMGETTER(TraceEnabled)(&fSniffer);
    H();

    if (fAttachDetach && fSniffer)
    {
        const char *pszNetDriver = "IntNet";
        if (pThis->meAttachmentType[uInstance] == NetworkAttachmentType_NAT)
            pszNetDriver = "NAT";
#if !defined(VBOX_WITH_NETFLT) && defined(RT_OS_LINUX)
        if (pThis->meAttachmentType[uInstance] == NetworkAttachmentType_Bridged)
            pszNetDriver = "HostInterface";
#endif

        rc = PDMR3DriverDetach(pVM, pszDevice, uInstance, uLun, pszNetDriver, 0, 0 /*fFlags*/);
        if (rc == VINF_PDM_NO_DRIVER_ATTACHED_TO_LUN)
            rc = VINF_SUCCESS;
        AssertLogRelRCReturn(rc, rc);

        pLunL0 = CFGMR3GetChildF(pInst, "LUN#%u", uLun);
        PCFGMNODE pLunAD = CFGMR3GetChildF(pLunL0, "AttachedDriver");
        if (pLunAD)
        {
            CFGMR3RemoveNode(pLunAD);
        }
        else
        {
            CFGMR3RemoveNode(pLunL0);
            rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL0, "Driver", "NetSniffer");        RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                 RC_CHECK();
            hrc = aNetworkAdapter->COMGETTER(TraceFile)(&str);              H();
            if (str) /* check convention for indicating default file. */
            {
                rc = CFGMR3InsertStringW(pCfg, "File", str);                RC_CHECK();
            }
            STR_FREE();
        }
    }
    else if (fAttachDetach && !fSniffer)
    {
        rc = PDMR3DeviceDetach(pVM, pszDevice, uInstance, uLun, 0 /*fFlags*/);
        if (rc == VINF_PDM_NO_DRIVER_ATTACHED_TO_LUN)
            rc = VINF_SUCCESS;
        AssertLogRelRCReturn(rc, rc);

        /* nuke anything which might have been left behind. */
        CFGMR3RemoveNode(CFGMR3GetChildF(pInst, "LUN#%u", uLun));
    }
    else if (!fAttachDetach && fSniffer)
    {
        /* insert the sniffer filter driver. */
        rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                 RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver", "NetSniffer");        RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                 RC_CHECK();
        hrc = aNetworkAdapter->COMGETTER(TraceFile)(&str);              H();
        if (str) /* check convention for indicating default file. */
        {
            rc = CFGMR3InsertStringW(pCfg, "File", str);                RC_CHECK();
        }
        STR_FREE();
    }

    Bstr networkName, trunkName, trunkType;
    NetworkAttachmentType_T eAttachmentType;
    hrc = aNetworkAdapter->COMGETTER(AttachmentType)(&eAttachmentType); H();
    switch (eAttachmentType)
    {
        case NetworkAttachmentType_Null:
            break;

        case NetworkAttachmentType_NAT:
        {
            if (fSniffer)
            {
                rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0); RC_CHECK();
            }
            else
            {
                rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);         RC_CHECK();
            }
            rc = CFGMR3InsertString(pLunL0, "Driver", "NAT");           RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);             RC_CHECK();

            /* Configure TFTP prefix and boot filename. */
            hrc = virtualBox->COMGETTER(HomeFolder)(&str);              H();
            if (str && *str)
            {
                rc = CFGMR3InsertStringF(pCfg, "TFTPPrefix", "%ls%c%s", str, RTPATH_DELIMITER, "TFTP"); RC_CHECK();
            }
            STR_FREE();
            hrc = pMachine->COMGETTER(Name)(&str);                      H();
            rc = CFGMR3InsertStringF(pCfg, "BootFile", "%ls.pxe", str); RC_CHECK();
            STR_FREE();

            hrc = aNetworkAdapter->COMGETTER(NATNetwork)(&str);         H();
            if (str && *str)
            {
                rc = CFGMR3InsertStringW(pCfg, "Network", str);         RC_CHECK();
                /* NAT uses its own DHCP implementation */
                //networkName = Bstr(psz);
            }
            STR_FREE();
            break;
        }

        case NetworkAttachmentType_Bridged:
        {
#if (defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)) && !defined(VBOX_WITH_NETFLT)
            hrc = pThis->attachToTapInterface(aNetworkAdapter);
            if (FAILED(hrc))
            {
                switch (hrc)
                {
                    case VERR_ACCESS_DENIED:
                        return VMSetError(pVM, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,  N_(
                                         "Failed to open '/dev/net/tun' for read/write access. Please check the "
                                         "permissions of that node. Either run 'chmod 0666 /dev/net/tun' or "
                                         "change the group of that node and make yourself a member of that group. Make "
                                         "sure that these changes are permanent, especially if you are "
                                         "using udev"));
                    default:
                        AssertMsgFailed(("Could not attach to host interface! Bad!\n"));
                        return VMSetError(pVM, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS, N_(
                                         "Failed to initialize Host Interface Networking"));
                }
            }

            Assert ((int)pThis->maTapFD[uInstance] >= 0);
            if ((int)pThis->maTapFD[uInstance] >= 0)
            {
                if (fSniffer)
                {
                    rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);               RC_CHECK();
                }
                else
                {
                    rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                         RC_CHECK();
                }
                rc = CFGMR3InsertString(pLunL0, "Driver", "HostInterface");                 RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                             RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg, "FileHandle", pThis->maTapFD[uInstance]);    RC_CHECK();
            }

#elif defined(VBOX_WITH_NETFLT)
            /*
             * This is the new VBoxNetFlt+IntNet stuff.
             */
            if (fSniffer)
            {
                rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);                   RC_CHECK();
            }
            else
            {
                rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                             RC_CHECK();
            }

            Bstr HifName;
            hrc = aNetworkAdapter->COMGETTER(HostInterface)(HifName.asOutParam());
            if (FAILED(hrc))
            {
                LogRel(("NetworkAttachmentType_Bridged: COMGETTER(HostInterface) failed, hrc (0x%x)", hrc));
                H();
            }

            Utf8Str HifNameUtf8(HifName);
            const char *pszHifName = HifNameUtf8.raw();

# if defined(RT_OS_DARWIN)
            /* The name is on the form 'ifX: long name', chop it off at the colon. */
            char szTrunk[8];
            strncpy(szTrunk, pszHifName, sizeof(szTrunk));
            char *pszColon = (char *)memchr(szTrunk, ':', sizeof(szTrunk));
            if (!pszColon)
            {
                hrc = aNetworkAdapter->Detach();                        H();
                return VMSetError(pVM, VERR_INTERNAL_ERROR, RT_SRC_POS,
                                  N_("Malformed host interface networking name '%ls'"),
                                  HifName.raw());
            }
            *pszColon = '\0';
            const char *pszTrunk = szTrunk;

# elif defined(RT_OS_SOLARIS)
            /* The name is on the form format 'ifX[:1] - long name, chop it off at space. */
            char szTrunk[256];
            strlcpy(szTrunk, pszHifName, sizeof(szTrunk));
            char *pszSpace = (char *)memchr(szTrunk, ' ', sizeof(szTrunk));

            /*
             * Currently don't bother about malformed names here for the sake of people using
             * VBoxManage and setting only the NIC name from there. If there is a space we
             * chop it off and proceed, otherwise just use whatever we've got.
             */
            if (pszSpace)
                *pszSpace = '\0';

            /* Chop it off at the colon (zone naming eg: e1000g:1 we need only the e1000g) */
            char *pszColon = (char *)memchr(szTrunk, ':', sizeof(szTrunk));
            if (pszColon)
                *pszColon = '\0';

            const char *pszTrunk = szTrunk;

# elif defined(RT_OS_WINDOWS)
            ComPtr<IHostNetworkInterface> hostInterface;
            hrc = host->FindHostNetworkInterfaceByName(HifName, hostInterface.asOutParam());
            if (!SUCCEEDED(hrc))
            {
                AssertLogRelMsgFailed(("NetworkAttachmentType_Bridged: FindByName failed, rc=%Rhrc (0x%x)", hrc, hrc));
                return VMSetError(pVM, VERR_INTERNAL_ERROR, RT_SRC_POS,
                                  N_("Inexistent host networking interface, name '%ls'"),
                                  HifName.raw());
            }

            HostNetworkInterfaceType_T eIfType;
            hrc = hostInterface->COMGETTER(InterfaceType)(&eIfType);
            if (FAILED(hrc))
            {
                LogRel(("NetworkAttachmentType_Bridged: COMGETTER(InterfaceType) failed, hrc (0x%x)", hrc));
                H();
            }

            if (eIfType != HostNetworkInterfaceType_Bridged)
            {
                return VMSetError(pVM, VERR_INTERNAL_ERROR, RT_SRC_POS,
                                                      N_("Interface ('%ls') is not a Bridged Adapter interface"),
                                                      HifName.raw());
            }

            hrc = hostInterface->COMGETTER(Id)(&str);
            if (FAILED(hrc))
            {
                LogRel(("NetworkAttachmentType_Bridged: COMGETTER(Id) failed, hrc (0x%x)", hrc));
                H();
            }
            Guid hostIFGuid(str);
            STR_FREE();

            INetCfg              *pNc;
            ComPtr<INetCfgComponent> pAdaptorComponent;
            LPWSTR                pszApp;
            int rc = VERR_INTNET_FLT_IF_NOT_FOUND;

            hrc = VBoxNetCfgWinQueryINetCfg(FALSE /*fGetWriteLock*/,
                                            L"VirtualBox",
                                            &pNc,
                                            &pszApp);
            Assert(hrc == S_OK);
            if (hrc == S_OK)
            {
                /* get the adapter's INetCfgComponent*/
                hrc = VBoxNetCfgWinGetComponentByGuid(pNc, &GUID_DEVCLASS_NET, (GUID*)hostIFGuid.ptr(), pAdaptorComponent.asOutParam());
                if (hrc != S_OK)
                {
                    VBoxNetCfgWinReleaseINetCfg(pNc, FALSE /*fHasWriteLock*/);
                    LogRel(("NetworkAttachmentType_Bridged: VBoxNetCfgWinGetComponentByGuid failed, hrc (0x%x)", hrc));
                    H();
                }
            }
#define VBOX_WIN_BINDNAME_PREFIX "\\DEVICE\\"
            char szTrunkName[INTNET_MAX_TRUNK_NAME];
            char *pszTrunkName = szTrunkName;
            wchar_t * pswzBindName;
            hrc = pAdaptorComponent->GetBindName(&pswzBindName);
            Assert(hrc == S_OK);
            if (hrc == S_OK)
            {
                int cwBindName = (int)wcslen(pswzBindName) + 1;
                int cbFullBindNamePrefix = sizeof(VBOX_WIN_BINDNAME_PREFIX);
                if (sizeof(szTrunkName) > cbFullBindNamePrefix + cwBindName)
                {
                    strcpy(szTrunkName, VBOX_WIN_BINDNAME_PREFIX);
                    pszTrunkName += cbFullBindNamePrefix-1;
                    if (!WideCharToMultiByte(CP_ACP, 0, pswzBindName, cwBindName, pszTrunkName,
                                             sizeof(szTrunkName) - cbFullBindNamePrefix + 1, NULL, NULL))
                    {
                        DWORD err = GetLastError();
                        hrc = HRESULT_FROM_WIN32(err);
                        AssertMsgFailed(("%hrc=%Rhrc %#x\n", hrc, hrc));
                        AssertLogRelMsgFailed(("NetworkAttachmentType_Bridged: WideCharToMultiByte failed, hr=%Rhrc (0x%x) err=%u\n", hrc, hrc, err));
                    }
                }
                else
                {
                    AssertLogRelMsgFailed(("NetworkAttachmentType_Bridged: insufficient szTrunkName buffer space\n"));
                    /** @todo set appropriate error code */
                    hrc = E_FAIL;
                }

                if (hrc != S_OK)
                {
                    AssertFailed();
                    CoTaskMemFree(pswzBindName);
                    VBoxNetCfgWinReleaseINetCfg(pNc, FALSE /*fHasWriteLock*/);
                    H();
                }

                /* we're not freeing the bind name since we'll use it later for detecting wireless*/
            }
            else
            {
                VBoxNetCfgWinReleaseINetCfg(pNc, FALSE /*fHasWriteLock*/);
                AssertLogRelMsgFailed(("NetworkAttachmentType_Bridged: VBoxNetCfgWinGetComponentByGuid failed, hrc (0x%x)", hrc));
                H();
            }
            const char *pszTrunk = szTrunkName;
            /* we're not releasing the INetCfg stuff here since we use it later to figure out whether it is wireless */

# elif defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
            /** @todo Check for malformed names. */
            const char *pszTrunk = pszHifName;

            /* Issue a warning if the interface is down */
            {
                int iSock = socket(AF_INET, SOCK_DGRAM, 0);
                if (iSock >= 0)
                {
                    struct ifreq Req;

                    memset(&Req, 0, sizeof(Req));
                    strncpy(Req.ifr_name, pszHifName, sizeof(Req.ifr_name) - 1);
                    if (ioctl(iSock, SIOCGIFFLAGS, &Req) >= 0)
                        if ((Req.ifr_flags & IFF_UP) == 0)
                        {
                            setVMRuntimeErrorCallbackF(pVM, pThis, 0, "BridgedInterfaceDown", "Bridged interface %s is down. Guest will not be able to use this interface", pszHifName);
                        }

                    close(iSock);
                }
            }

# else
#  error "PORTME (VBOX_WITH_NETFLT)"
# endif

            rc = CFGMR3InsertString(pLunL0, "Driver", "IntNet");                    RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                         RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Trunk", pszTrunk);                       RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_NetFlt);
            RC_CHECK();
            char szNetwork[INTNET_MAX_NETWORK_NAME];
            RTStrPrintf(szNetwork, sizeof(szNetwork), "HostInterfaceNetworking-%s", pszHifName);
            rc = CFGMR3InsertString(pCfg, "Network", szNetwork);                    RC_CHECK();
            networkName = Bstr(szNetwork);
            trunkName = Bstr(pszTrunk);
            trunkType = Bstr(TRUNKTYPE_NETFLT);

# if defined(RT_OS_DARWIN)
            /** @todo Come up with a better deal here. Problem is that IHostNetworkInterface is completely useless here. */
            if (    strstr(pszHifName, "Wireless")
                ||  strstr(pszHifName, "AirPort" ))
            {
                rc = CFGMR3InsertInteger(pCfg, "SharedMacOnWire", true);            RC_CHECK();
            }
# elif defined(RT_OS_LINUX)
            int iSock = socket(AF_INET, SOCK_DGRAM, 0);
            if (iSock >= 0)
            {
                struct iwreq WRq;

                memset(&WRq, 0, sizeof(WRq));
                strncpy(WRq.ifr_name, pszHifName, IFNAMSIZ);
                bool fSharedMacOnWire = ioctl(iSock, SIOCGIWNAME, &WRq) >= 0;
                close(iSock);
                if (fSharedMacOnWire)
                {
                    rc = CFGMR3InsertInteger(pCfg, "SharedMacOnWire", true);
                    RC_CHECK();
                    Log(("Set SharedMacOnWire\n"));
                }
                else
                    Log(("Failed to get wireless name\n"));
            }
            else
                Log(("Failed to open wireless socket\n"));
# elif defined(RT_OS_WINDOWS)
#  define DEVNAME_PREFIX L"\\\\.\\"
            /* we are getting the medium type via IOCTL_NDIS_QUERY_GLOBAL_STATS Io Control
             * there is a pretty long way till there though since we need to obtain the symbolic link name
             * for the adapter device we are going to query given the device Guid */


            /* prepend the "\\\\.\\" to the bind name to obtain the link name */

            wchar_t FileName[MAX_PATH];
            wcscpy(FileName, DEVNAME_PREFIX);
            wcscpy((wchar_t*)(((char*)FileName) + sizeof(DEVNAME_PREFIX) - sizeof(FileName[0])), pswzBindName);

            /* open the device */
            HANDLE hDevice = CreateFile(FileName,
                                        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL,
                                        NULL);

            if (hDevice != INVALID_HANDLE_VALUE)
            {
                bool fSharedMacOnWire = false;

                /* now issue the OID_GEN_PHYSICAL_MEDIUM query */
                DWORD Oid = OID_GEN_PHYSICAL_MEDIUM;
                NDIS_PHYSICAL_MEDIUM PhMedium;
                DWORD cbResult;
                if (DeviceIoControl(hDevice,
                                    IOCTL_NDIS_QUERY_GLOBAL_STATS,
                                    &Oid,
                                    sizeof(Oid),
                                    &PhMedium,
                                    sizeof(PhMedium),
                                    &cbResult,
                                    NULL))
                {
                    /* that was simple, now examine PhMedium */
                    if (   PhMedium == NdisPhysicalMediumWirelessWan
                        || PhMedium == NdisPhysicalMediumWirelessLan
                        || PhMedium == NdisPhysicalMediumNative802_11
                        || PhMedium == NdisPhysicalMediumBluetooth)
                        fSharedMacOnWire = true;
                }
                else
                {
                    int winEr = GetLastError();
                    LogRel(("Console::configConstructor: DeviceIoControl failed, err (0x%x), ignoring\n", winEr));
                    Assert(winEr == ERROR_INVALID_PARAMETER || winEr == ERROR_NOT_SUPPORTED || winEr == ERROR_BAD_COMMAND);
                }
                CloseHandle(hDevice);

                if (fSharedMacOnWire)
                {
                    Log(("this is a wireless adapter"));
                    rc = CFGMR3InsertInteger(pCfg, "SharedMacOnWire", true); RC_CHECK();
                    Log(("Set SharedMacOnWire\n"));
                }
                else
                    Log(("this is NOT a wireless adapter"));
            }
            else
            {
                int winEr = GetLastError();
                AssertLogRelMsgFailed(("Console::configConstructor: CreateFile failed, err (0x%x), ignoring\n", winEr));
            }

            CoTaskMemFree(pswzBindName);

            pAdaptorComponent.setNull();
            /* release the pNc finally */
            VBoxNetCfgWinReleaseINetCfg(pNc, FALSE /*fHasWriteLock*/);
# else
            /** @todo PORTME: wireless detection */
# endif

# if defined(RT_OS_SOLARIS)
#  if 0 /* bird: this is a bit questionable and might cause more trouble than its worth.  */
            /* Zone access restriction, don't allow snopping the global zone. */
            zoneid_t ZoneId = getzoneid();
            if (ZoneId != GLOBAL_ZONEID)
            {
                rc = CFGMR3InsertInteger(pCfg, "IgnoreAllPromisc", true);   RC_CHECK();
            }
#  endif
# endif

#elif defined(RT_OS_WINDOWS) /* not defined NetFlt */
            /* NOTHING TO DO HERE */
#elif defined(RT_OS_LINUX)
/// @todo aleksey: is there anything to be done here?
#elif defined(RT_OS_FREEBSD)
/** @todo FreeBSD: Check out this later (HIF networking). */
#else
# error "Port me"
#endif
            break;
        }

        case NetworkAttachmentType_Internal:
        {
            hrc = aNetworkAdapter->COMGETTER(InternalNetwork)(&str);    H();
            if (str && *str)
            {
                if (fSniffer)
                {
                    rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);
                    RC_CHECK();
                }
                else
                {
                    rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);
                    RC_CHECK();
                }
                rc = CFGMR3InsertString(pLunL0, "Driver", "IntNet");    RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);         RC_CHECK();
                rc = CFGMR3InsertStringW(pCfg, "Network", str);         RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_WhateverNone); RC_CHECK();
                networkName = str;
                trunkType = Bstr(TRUNKTYPE_WHATEVER);
            }
            STR_FREE();
            break;
        }

        case NetworkAttachmentType_HostOnly:
        {
            if (fSniffer)
            {
                rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);
                RC_CHECK();
            }
            else
            {
                rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);
                RC_CHECK();
            }

            rc = CFGMR3InsertString(pLunL0, "Driver", "IntNet");            RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                 RC_CHECK();

            Bstr HifName;
            hrc = aNetworkAdapter->COMGETTER(HostInterface)(HifName.asOutParam());
            if (FAILED(hrc))
            {
                LogRel(("NetworkAttachmentType_HostOnly: COMGETTER(HostInterface) failed, hrc (0x%x)\n", hrc));
                H();
            }

            Utf8Str HifNameUtf8(HifName);
            const char *pszHifName = HifNameUtf8.raw();
            LogRel(("NetworkAttachmentType_HostOnly: COMGETTER(HostInterface): %s\n", pszHifName));
            ComPtr<IHostNetworkInterface> hostInterface;
            rc = host->FindHostNetworkInterfaceByName(HifName, hostInterface.asOutParam());
            if (!SUCCEEDED(rc))
            {
                LogRel(("NetworkAttachmentType_HostOnly: FindByName failed, rc (0x%x)\n", rc));
                return VMSetError(pVM, VERR_INTERNAL_ERROR, RT_SRC_POS,
                                  N_("Inexistent host networking interface, name '%ls'"),
                                  HifName.raw());
            }

            char szNetwork[INTNET_MAX_NETWORK_NAME];
            RTStrPrintf(szNetwork, sizeof(szNetwork), "HostInterfaceNetworking-%s", pszHifName);

#if defined(RT_OS_WINDOWS)
# ifndef VBOX_WITH_NETFLT
            hrc = E_NOTIMPL;
            LogRel(("NetworkAttachmentType_HostOnly: Not Implemented\n"));
            H();
# else  /* defined VBOX_WITH_NETFLT*/
            /** @todo r=bird: Put this in a function. */

            HostNetworkInterfaceType_T eIfType;
            hrc = hostInterface->COMGETTER(InterfaceType)(&eIfType);
            if (FAILED(hrc))
            {
                LogRel(("NetworkAttachmentType_HostOnly: COMGETTER(InterfaceType) failed, hrc (0x%x)\n", hrc));
                H();
            }

            if (eIfType != HostNetworkInterfaceType_HostOnly)
                return VMSetError(pVM, VERR_INTERNAL_ERROR, RT_SRC_POS,
                                  N_("Interface ('%ls') is not a Host-Only Adapter interface"),
                                  HifName.raw());

            hrc = hostInterface->COMGETTER(Id)(&str);
            if (FAILED(hrc))
            {
                LogRel(("NetworkAttachmentType_HostOnly: COMGETTER(Id) failed, hrc (0x%x)\n", hrc));
                H();
            }
            Guid hostIFGuid(str);
            STR_FREE();

            INetCfg *pNc;
            ComPtr<INetCfgComponent> pAdaptorComponent;
            LPWSTR pszApp;
            rc = VERR_INTNET_FLT_IF_NOT_FOUND;

            hrc = VBoxNetCfgWinQueryINetCfg(FALSE,
                                            L"VirtualBox",
                                            &pNc,
                                            &pszApp);
            Assert(hrc == S_OK);
            if (hrc == S_OK)
            {
                /* get the adapter's INetCfgComponent*/
                hrc = VBoxNetCfgWinGetComponentByGuid(pNc, &GUID_DEVCLASS_NET, (GUID*)hostIFGuid.ptr(), pAdaptorComponent.asOutParam());
                if (hrc != S_OK)
                {
                    VBoxNetCfgWinReleaseINetCfg(pNc, FALSE /*fHasWriteLock*/);
                    LogRel(("NetworkAttachmentType_HostOnly: VBoxNetCfgWinGetComponentByGuid failed, hrc=%Rhrc (0x%x)\n", hrc, hrc));
                    H();
                }
            }
#define VBOX_WIN_BINDNAME_PREFIX "\\DEVICE\\"
            char szTrunkName[INTNET_MAX_TRUNK_NAME];
            char *pszTrunkName = szTrunkName;
            wchar_t * pswzBindName;
            hrc = pAdaptorComponent->GetBindName(&pswzBindName);
            Assert(hrc == S_OK);
            if (hrc == S_OK)
            {
                int cwBindName = (int)wcslen(pswzBindName) + 1;
                int cbFullBindNamePrefix = sizeof(VBOX_WIN_BINDNAME_PREFIX);
                if (sizeof(szTrunkName) > cbFullBindNamePrefix + cwBindName)
                {
                    strcpy(szTrunkName, VBOX_WIN_BINDNAME_PREFIX);
                    pszTrunkName += cbFullBindNamePrefix-1;
                    if (!WideCharToMultiByte(CP_ACP, 0, pswzBindName, cwBindName, pszTrunkName,
                                             sizeof(szTrunkName) - cbFullBindNamePrefix + 1, NULL, NULL))
                    {
                        DWORD err = GetLastError();
                        hrc = HRESULT_FROM_WIN32(err);
                        AssertLogRelMsgFailed(("NetworkAttachmentType_HostOnly: WideCharToMultiByte failed, hr=%Rhrc (0x%x) err=%u\n", hrc, hrc, err));
                    }
                }
                else
                {
                    AssertLogRelMsgFailed(("NetworkAttachmentType_HostOnly: insufficient szTrunkName buffer space\n"));
                    /** @todo set appropriate error code */
                    hrc = E_FAIL;
                }

                if (hrc != S_OK)
                {
                    AssertFailed();
                    CoTaskMemFree(pswzBindName);
                    VBoxNetCfgWinReleaseINetCfg(pNc, FALSE /*fHasWriteLock*/);
                    H();
                }
            }
            else
            {
                VBoxNetCfgWinReleaseINetCfg(pNc, FALSE /*fHasWriteLock*/);
                AssertLogRelMsgFailed(("NetworkAttachmentType_HostOnly: VBoxNetCfgWinGetComponentByGuid failed, hrc=%Rhrc (0x%x)\n", hrc, hrc));
                H();
            }


            CoTaskMemFree(pswzBindName);

            pAdaptorComponent.setNull();
            /* release the pNc finally */
            VBoxNetCfgWinReleaseINetCfg(pNc, FALSE /*fHasWriteLock*/);

            const char *pszTrunk = szTrunkName;

            rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_NetAdp);   RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Trunk", pszTrunk);                       RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Network", szNetwork);                    RC_CHECK();
            networkName = Bstr(szNetwork);
            trunkName   = Bstr(pszTrunk);
            trunkType   = TRUNKTYPE_NETADP;
# endif /* defined VBOX_WITH_NETFLT*/
#elif defined(RT_OS_DARWIN)
            rc = CFGMR3InsertString(pCfg, "Trunk", pszHifName);                     RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Network", szNetwork);                    RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_NetAdp);   RC_CHECK();
            networkName = Bstr(szNetwork);
            trunkName   = Bstr(pszHifName);
            trunkType   = TRUNKTYPE_NETADP;
#else
            rc = CFGMR3InsertString(pCfg, "Trunk", pszHifName);                     RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Network", szNetwork);                    RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_NetFlt);   RC_CHECK();
            networkName = Bstr(szNetwork);
            trunkName   = Bstr(pszHifName);
            trunkType   = TRUNKTYPE_NETFLT;
#endif
#if !defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)

            Bstr tmpAddr, tmpMask;

            hrc = virtualBox->GetExtraData(BstrFmt("HostOnly/%s/IPAddress", pszHifName), tmpAddr.asOutParam());
            if (SUCCEEDED(hrc) && !tmpAddr.isEmpty())
            {
                hrc = virtualBox->GetExtraData(BstrFmt("HostOnly/%s/IPNetMask", pszHifName), tmpMask.asOutParam());
                if (SUCCEEDED(hrc) && !tmpMask.isEmpty())
                    hrc = hostInterface->EnableStaticIpConfig(tmpAddr, tmpMask);
                else
                    hrc = hostInterface->EnableStaticIpConfig(tmpAddr,
                                                              Bstr(VBOXNET_IPV4MASK_DEFAULT));
            }
            else
                hrc = hostInterface->EnableStaticIpConfig(Bstr(VBOXNET_IPV4ADDR_DEFAULT),
                                                          Bstr(VBOXNET_IPV4MASK_DEFAULT));
            ComAssertComRC(hrc); /** @todo r=bird: Why this isn't fatal? (H()) */

            hrc = virtualBox->GetExtraData(BstrFmt("HostOnly/%s/IPV6Address", pszHifName), tmpAddr.asOutParam());
            if (SUCCEEDED(hrc))
                hrc = virtualBox->GetExtraData(BstrFmt("HostOnly/%s/IPV6NetMask", pszHifName), tmpMask.asOutParam());
            if (SUCCEEDED(hrc) && !tmpAddr.isEmpty() && !tmpMask.isEmpty())
            {
                hrc = hostInterface->EnableStaticIpConfigV6(tmpAddr, Utf8Str(tmpMask).toUInt32());
                ComAssertComRC(hrc); /** @todo r=bird: Why this isn't fatal? (H()) */
            }
#endif
            break;
        }

        default:
            AssertMsgFailed(("should not get here!\n"));
            break;
    }

    /*
     * Attempt to attach the driver.
     */
    switch (eAttachmentType)
    {
        case NetworkAttachmentType_Null:
            break;

        case NetworkAttachmentType_Bridged:
        case NetworkAttachmentType_Internal:
        case NetworkAttachmentType_HostOnly:
        case NetworkAttachmentType_NAT:
        {
            if (SUCCEEDED(hrc) && SUCCEEDED(rc))
            {
                if (fAttachDetach)
                {
                    rc = PDMR3DriverAttach(pVM, pszDevice, uInstance, uLun, 0 /*fFlags*/, NULL /* ppBase */);
                    AssertRC(rc);
                }

                {
                    /** @todo pritesh: get the dhcp server name from the
                     * previous network configuration and then stop the server
                     * else it may conflict with the dhcp server running  with
                     * the current attachment type
                     */
                    /* Stop the hostonly DHCP Server */
                }

                if (!networkName.isNull())
                {
                    /*
                     * Until we implement service reference counters DHCP Server will be stopped
                     * by DHCPServerRunner destructor.
                     */
                    ComPtr<IDHCPServer> dhcpServer;
                    hrc = virtualBox->FindDHCPServerByNetworkName(networkName.mutableRaw(), dhcpServer.asOutParam());
                    if (SUCCEEDED(hrc))
                    {
                        /* there is a DHCP server available for this network */
                        BOOL fEnabled;
                        hrc = dhcpServer->COMGETTER(Enabled)(&fEnabled);
                        if (FAILED(hrc))
                        {
                            LogRel(("DHCP svr: COMGETTER(Enabled) failed, hrc (%Rhrc)", hrc));
                            H();
                        }

                        if (fEnabled)
                            hrc = dhcpServer->Start(networkName, trunkName, trunkType);
                    }
                    else
                        hrc = S_OK;
                }
            }

            break;
        }

        default:
            AssertMsgFailed(("should not get here!\n"));
            break;
    }

    pThis->meAttachmentType[uInstance] = eAttachmentType;

#undef STR_FREE
#undef H
#undef RC_CHECK

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_GUEST_PROPS
/**
 * Set an array of guest properties
 */
static void configSetProperties(VMMDev * const pVMMDev, void *names,
                                void *values, void *timestamps, void *flags)
{
    VBOXHGCMSVCPARM parms[4];

    parms[0].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[0].u.pointer.addr = names;
    parms[0].u.pointer.size = 0;  /* We don't actually care. */
    parms[1].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[1].u.pointer.addr = values;
    parms[1].u.pointer.size = 0;  /* We don't actually care. */
    parms[2].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[2].u.pointer.addr = timestamps;
    parms[2].u.pointer.size = 0;  /* We don't actually care. */
    parms[3].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[3].u.pointer.addr = flags;
    parms[3].u.pointer.size = 0;  /* We don't actually care. */

    pVMMDev->hgcmHostCall ("VBoxGuestPropSvc", guestProp::SET_PROPS_HOST, 4,
                           &parms[0]);
}

/**
 * Set a single guest property
 */
static void configSetProperty(VMMDev * const pVMMDev, const char *pszName,
                              const char *pszValue, const char *pszFlags)
{
    VBOXHGCMSVCPARM parms[4];

    AssertPtrReturnVoid(pszName);
    AssertPtrReturnVoid(pszValue);
    AssertPtrReturnVoid(pszFlags);
    parms[0].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[0].u.pointer.addr = (void *)pszName;
    parms[0].u.pointer.size = strlen(pszName) + 1;
    parms[1].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[1].u.pointer.addr = (void *)pszValue;
    parms[1].u.pointer.size = strlen(pszValue) + 1;
    parms[2].type = VBOX_HGCM_SVC_PARM_PTR;
    parms[2].u.pointer.addr = (void *)pszFlags;
    parms[2].u.pointer.size = strlen(pszFlags) + 1;
    pVMMDev->hgcmHostCall ("VBoxGuestPropSvc", guestProp::SET_PROP_HOST, 3,
                           &parms[0]);
}
#endif /* VBOX_WITH_GUEST_PROPS */

/**
 * Set up the Guest Property service, populate it with properties read from
 * the machine XML and set a couple of initial properties.
 */
/* static */ int Console::configGuestProperties(void *pvConsole)
{
#ifdef VBOX_WITH_GUEST_PROPS
    AssertReturn(pvConsole, VERR_GENERAL_FAILURE);
    ComObjPtr<Console> pConsole = static_cast <Console *> (pvConsole);

    /* Load the service */
    int rc = pConsole->mVMMDev->hgcmLoadService ("VBoxGuestPropSvc", "VBoxGuestPropSvc");

    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxGuestPropSvc is not available. rc = %Rrc\n", rc));
        /* That is not a fatal failure. */
        rc = VINF_SUCCESS;
    }
    else
    {
        /*
         * Initialize built-in properties that can be changed and saved.
         *
         * These are typically transient properties that the guest cannot
         * change.
         */

        /* Sysprep execution by VBoxService. */
        configSetProperty(pConsole->mVMMDev,
                          "/VirtualBox/HostGuest/SysprepExec", "",
                          "TRANSIENT, RDONLYGUEST");
        configSetProperty(pConsole->mVMMDev,
                          "/VirtualBox/HostGuest/SysprepArgs", "",
                          "TRANSIENT, RDONLYGUEST");

        /*
         * Pull over the properties from the server.
         */
        SafeArray<BSTR> namesOut;
        SafeArray<BSTR> valuesOut;
        SafeArray<ULONG64> timestampsOut;
        SafeArray<BSTR> flagsOut;
        HRESULT hrc = pConsole->mControl->PullGuestProperties
                                      (ComSafeArrayAsOutParam(namesOut),
                                       ComSafeArrayAsOutParam(valuesOut),
                                       ComSafeArrayAsOutParam(timestampsOut),
                                       ComSafeArrayAsOutParam(flagsOut));
        AssertMsgReturn(SUCCEEDED(hrc), ("hrc=%#x\n", hrc),
                        VERR_GENERAL_FAILURE);
        size_t cProps = namesOut.size();
        size_t cAlloc = cProps + 1;
        if (   valuesOut.size() != cProps
            || timestampsOut.size() != cProps
            || flagsOut.size() != cProps
           )
            AssertFailedReturn(VERR_INVALID_PARAMETER);

        char **papszNames, **papszValues, **papszFlags;
        char szEmpty[] = "";
        ULONG64 *pau64Timestamps;
        papszNames = (char **)RTMemTmpAllocZ(sizeof(void *) * cAlloc);
        papszValues = (char **)RTMemTmpAllocZ(sizeof(void *) * cAlloc);
        pau64Timestamps = (ULONG64 *)RTMemTmpAllocZ(sizeof(ULONG64) * cAlloc);
        papszFlags = (char **)RTMemTmpAllocZ(sizeof(void *) * cAlloc);
        if (papszNames && papszValues && pau64Timestamps && papszFlags)
        {
            for (unsigned i = 0; RT_SUCCESS(rc) && i < cProps; ++i)
            {
                AssertPtrReturn(namesOut[i], VERR_INVALID_PARAMETER);
                rc = RTUtf16ToUtf8(namesOut[i], &papszNames[i]);
                if (RT_FAILURE(rc))
                    break;
                if (valuesOut[i])
                    rc = RTUtf16ToUtf8(valuesOut[i], &papszValues[i]);
                else
                    papszValues[i] = szEmpty;
                if (RT_FAILURE(rc))
                    break;
                pau64Timestamps[i] = timestampsOut[i];
                if (flagsOut[i])
                    rc = RTUtf16ToUtf8(flagsOut[i], &papszFlags[i]);
                else
                    papszFlags[i] = szEmpty;
            }
            if (RT_SUCCESS(rc))
                configSetProperties(pConsole->mVMMDev,
                                    (void *)papszNames,
                                    (void *)papszValues,
                                    (void *)pau64Timestamps,
                                    (void *)papszFlags);
            for (unsigned i = 0; i < cProps; ++i)
            {
                RTStrFree(papszNames[i]);
                if (valuesOut[i])
                    RTStrFree(papszValues[i]);
                if (flagsOut[i])
                    RTStrFree(papszFlags[i]);
            }
        }
        else
            rc = VERR_NO_MEMORY;
        RTMemTmpFree(papszNames);
        RTMemTmpFree(papszValues);
        RTMemTmpFree(pau64Timestamps);
        RTMemTmpFree(papszFlags);
        AssertRCReturn(rc, rc);

        /*
         * These properties have to be set before pulling over the properties
         * from the machine XML, to ensure that properties saved in the XML
         * will override them.
         */
        /* Set the VBox version string as a guest property */
        configSetProperty(pConsole->mVMMDev, "/VirtualBox/HostInfo/VBoxVer",
                          VBOX_VERSION_STRING, "TRANSIENT, RDONLYGUEST");
        /* Set the VBox SVN revision as a guest property */
        configSetProperty(pConsole->mVMMDev, "/VirtualBox/HostInfo/VBoxRev",
                          RTBldCfgRevisionStr(), "TRANSIENT, RDONLYGUEST");

        /*
         * Register the host notification callback
         */
        HGCMSVCEXTHANDLE hDummy;
        HGCMHostRegisterServiceExtension(&hDummy, "VBoxGuestPropSvc",
                                         Console::doGuestPropNotification,
                                         pvConsole);

        Log(("Set VBoxGuestPropSvc property store\n"));
    }
    return VINF_SUCCESS;
#else /* !VBOX_WITH_GUEST_PROPS */
    return VERR_NOT_SUPPORTED;
#endif /* !VBOX_WITH_GUEST_PROPS */
}
