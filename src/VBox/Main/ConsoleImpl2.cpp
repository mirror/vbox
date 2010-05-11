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
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "ConsoleImpl.h"
#include "DisplayImpl.h"
#ifdef VBOX_WITH_GUEST_CONTROL
# include "GuestImpl.h"
#endif
#include "VMMDev.h"

// generated header
#include "SchemaDefs.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/buildconfig.h>
#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/param.h>
#if 0 /* enable to play with lots of memory. */
# include <iprt/env.h>
#endif
#include <iprt/file.h>

#include <VBox/vmapi.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/pdmapi.h> /* For PDMR3DriverAttach/PDMR3DriverDetach */
#include <VBox/version.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#ifdef VBOX_WITH_CROGL
# include <VBox/HostServices/VBoxCrOpenGLSvc.h>
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

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>

#ifdef VBOX_WITH_NETFLT
# if defined(RT_OS_SOLARIS)
#  include <zone.h>
# elif defined(RT_OS_LINUX)
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <sys/socket.h>
#  include <linux/types.h>
#  include <linux/if.h>
#  include <linux/wireless.h>
# elif defined(RT_OS_FREEBSD)
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/ioctl.h>
#  include <sys/socket.h>
#  include <net/if.h>
#  include <net80211/ieee80211_ioctl.h>
# endif
# if defined(RT_OS_WINDOWS)
#  include <VBox/WinNetConfig.h>
#  include <Ntddndis.h>
#  include <devguid.h>
# else
#  include <HostNetworkInterfaceImpl.h>
#  include <netif.h>
#  include <stdlib.h>
# endif
#endif /* VBOX_WITH_NETFLT */

#include "DHCPServerRunner.h"

#if defined(RT_OS_DARWIN) && !defined(VBOX_OSE)

# include "IOKit/IOKitLib.h"

static int DarwinSmcKey(char *aKey, uint32_t iKeySize)
{
    /*
     * Method as described in Amit Singh's article:
     *   http://osxbook.com/book/bonus/chapter7/tpmdrmmyth/
     */
    typedef struct
    {
        uint32_t   key;
        uint8_t    pad0[22];
        uint32_t   datasize;
        uint8_t    pad1[10];
        uint8_t    cmd;
        uint32_t   pad2;
        uint8_t    data[32];
    } AppleSMCBuffer;

    AssertReturn(iKeySize >= 65, VERR_INTERNAL_ERROR);

    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault,
                                                       IOServiceMatching("AppleSMC"));
    if (!service)
        return VERR_INTERNAL_ERROR;

    io_connect_t    port = (io_connect_t)0;
    kern_return_t   kr   = IOServiceOpen(service, mach_task_self(), 0, &port);
    IOObjectRelease(service);

    if (kr != kIOReturnSuccess)
        return VERR_INTERNAL_ERROR;

    AppleSMCBuffer  inputStruct    = { 0, {0}, 32, {0}, 5, };
    AppleSMCBuffer  outputStruct;
    size_t          cbOutputStruct = sizeof(outputStruct);

    for (int i = 0; i < 2; i++)
    {
        inputStruct.key = (uint32_t)((i == 0) ? 'OSK0' : 'OSK1');
        kr = IOConnectCallStructMethod((mach_port_t)port,
                                       (uint32_t)2,
                                       (const void *)&inputStruct,
                                       sizeof(inputStruct),
                                       (void *)&outputStruct,
                                       &cbOutputStruct);
        if (kr != kIOReturnSuccess)
        {
            IOServiceClose(port);
            return VERR_INTERNAL_ERROR;
        }

        for (int j = 0; j < 32; j++)
            aKey[j + i*32] = outputStruct.data[j];
    }

    IOServiceClose(port);

    aKey[64] = 0;

    return VINF_SUCCESS;
}

#endif /* RT_OS_DARWIN */

/* Darwin compile cludge */
#undef PVM

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

/**
 * Simple class for storing network boot information.
 */
struct BootNic
{
    ULONG       mInstance;
    unsigned    mPciDev;
    unsigned    mPciFn;
    ULONG       mBootPrio;
    bool operator < (const BootNic &rhs) const
    {
        ULONG lval = mBootPrio     - 1; /* 0 will wrap around and get the lowest priority. */
        ULONG rval = rhs.mBootPrio - 1;
        return lval < rval; /* Zero compares as highest number (lowest prio). */
    }
};

/*
 * VC++ 8 / amd64 has some serious trouble with this function.
 * As a temporary measure, we'll drop global optimizations.
 */
#if defined(_MSC_VER) && defined(RT_ARCH_AMD64)
# pragma optimize("g", off)
#endif

static int findEfiRom(IVirtualBox* vbox, FirmwareType_T aFirmwareType, Utf8Str& aEfiRomFile)
{
    int rc;
    BOOL fPresent = FALSE;
    Bstr aFilePath, empty;

    rc = vbox->CheckFirmwarePresent(aFirmwareType, empty,
                                    empty.asOutParam(), aFilePath.asOutParam(), &fPresent);
    if (RT_FAILURE(rc))
        AssertComRCReturn(rc, VERR_FILE_NOT_FOUND);

    if (!fPresent)
        return VERR_FILE_NOT_FOUND;

    aEfiRomFile = Utf8Str(aFilePath);

    return S_OK;
}

static int getSmcDeviceKey(IMachine* pMachine, BSTR * aKey)
{
# if defined(RT_OS_DARWIN) && !defined(VBOX_OSE)
    int rc;
    char aKeyBuf[65];

    rc = DarwinSmcKey(aKeyBuf, sizeof aKeyBuf);
    if (SUCCEEDED(rc))
    {
        Bstr(aKeyBuf).detachTo(aKey);
        return rc;
    }
#endif

    return pMachine->GetExtraData(Bstr("VBoxInternal2/SmcDeviceKey"), aKey);
}

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
    BOOL fIs64BitGuest = false;

#if !defined(VBOX_WITH_XPCOM)
    {
        /* initialize COM */
        HRESULT hrc = CoInitializeEx(NULL,
                                     COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE |
                                     COINIT_SPEED_OVER_MEMORY);
        LogFlow(("Console::configConstructor(): CoInitializeEx()=%08X\n", hrc));
        AssertComRCReturn(hrc, VERR_GENERAL_FAILURE);
    }
#endif

    AssertReturn(pvConsole, VERR_GENERAL_FAILURE);
    ComObjPtr<Console> pConsole = static_cast<Console *>(pvConsole);

    AutoCaller autoCaller(pConsole);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    /* lock the console because we widely use internal fields and methods */
    AutoWriteLock alock(pConsole COMMA_LOCKVAL_SRC_POS);

    /* Save the VM pointer in the machine object */
    pConsole->mpVM = pVM;

    ComPtr<IMachine> pMachine = pConsole->machine();

    int             rc;
    HRESULT         hrc;
    Bstr            bstr;

#define RC_CHECK()  AssertMsgReturn(RT_SUCCESS(rc), ("rc=%Rrc\n", rc), rc)
#define H()         AssertMsgReturn(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), VERR_GENERAL_FAILURE)

    /*
     * Get necessary objects and frequently used parameters.
     */
    ComPtr<IVirtualBox> virtualBox;
    hrc = pMachine->COMGETTER(Parent)(virtualBox.asOutParam());                         H();

    ComPtr<IHost> host;
    hrc = virtualBox->COMGETTER(Host)(host.asOutParam());                               H();

    ComPtr<ISystemProperties> systemProperties;
    hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());       H();

    ComPtr<IBIOSSettings> biosSettings;
    hrc = pMachine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());                 H();

    hrc = pMachine->COMGETTER(HardwareUUID)(bstr.asOutParam());                         H();
    RTUUID HardwareUuid;
    rc = RTUuidFromUtf16(&HardwareUuid, bstr.raw());                                    RC_CHECK();

    ULONG cRamMBs;
    hrc = pMachine->COMGETTER(MemorySize)(&cRamMBs);                                    H();
#if 0 /* enable to play with lots of memory. */
    if (RTEnvExist("VBOX_RAM_SIZE"))
        cRamMBs = RTStrToUInt64(RTEnvGet("VBOX_RAM_SIZE"));
#endif
    uint64_t const cbRam = cRamMBs * (uint64_t)_1M;
    uint32_t const cbRamHole = MM_RAM_HOLE_SIZE_DEFAULT;

    ULONG cCpus = 1;
    hrc = pMachine->COMGETTER(CPUCount)(&cCpus);                                        H();

    Bstr osTypeId;
    hrc = pMachine->COMGETTER(OSTypeId)(osTypeId.asOutParam());                         H();

    BOOL fIOAPIC;
    hrc = biosSettings->COMGETTER(IOAPICEnabled)(&fIOAPIC);                             H();

    ComPtr<IGuestOSType> guestOSType;
    hrc = virtualBox->GetGuestOSType(osTypeId, guestOSType.asOutParam());               H();

    Bstr guestTypeFamilyId;
    hrc = guestOSType->COMGETTER(FamilyId)(guestTypeFamilyId.asOutParam());             H();
    BOOL fOsXGuest = guestTypeFamilyId == Bstr("MacOS");

    /*
     * Get root node first.
     * This is the only node in the tree.
     */
    PCFGMNODE pRoot = CFGMR3GetRoot(pVM);
    Assert(pRoot);

    /*
     * Set the root (and VMM) level values.
     */
    hrc = pMachine->COMGETTER(Name)(bstr.asOutParam());                                 H();
    rc = CFGMR3InsertStringW(pRoot, "Name",                 bstr.raw());                RC_CHECK();
    rc = CFGMR3InsertBytes(pRoot,   "UUID", &HardwareUuid, sizeof(HardwareUuid));       RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RamSize",              cbRam);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RamHoleSize",          cbRamHole);                 RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "NumCPUs",              cCpus);                     RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "TimerMillies",         10);                        RC_CHECK();
#ifdef VBOX_WITH_RAW_MODE
    rc = CFGMR3InsertInteger(pRoot, "RawR3Enabled",         1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RawR0Enabled",         1);     /* boolean */       RC_CHECK();
    /** @todo Config: RawR0, PATMEnabled and CSAMEnabled needs attention later. */
    rc = CFGMR3InsertInteger(pRoot, "PATMEnabled",          1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "CSAMEnabled",          1);     /* boolean */       RC_CHECK();
#endif

    /* cpuid leaf overrides. */
    static uint32_t const s_auCpuIdRanges[] =
    {
        UINT32_C(0x00000000), UINT32_C(0x0000000a),
        UINT32_C(0x80000000), UINT32_C(0x8000000a)
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_auCpuIdRanges); i += 2)
        for (uint32_t uLeaf = s_auCpuIdRanges[i]; uLeaf < s_auCpuIdRanges[i + 1]; uLeaf++)
        {
            ULONG ulEax, ulEbx, ulEcx, ulEdx;
            hrc = pMachine->GetCPUIDLeaf(uLeaf, &ulEax, &ulEbx, &ulEcx, &ulEdx);
            if (SUCCEEDED(hrc))
            {
                PCFGMNODE pLeaf;
                rc = CFGMR3InsertNodeF(pRoot, &pLeaf, "CPUM/HostCPUID/%RX32", uLeaf);   RC_CHECK();

                rc = CFGMR3InsertInteger(pLeaf, "eax", ulEax);                          RC_CHECK();
                rc = CFGMR3InsertInteger(pLeaf, "ebx", ulEbx);                          RC_CHECK();
                rc = CFGMR3InsertInteger(pLeaf, "ecx", ulEcx);                          RC_CHECK();
                rc = CFGMR3InsertInteger(pLeaf, "edx", ulEdx);                          RC_CHECK();
            }
            else if (hrc != E_INVALIDARG)                                               H();
        }

    if (osTypeId == "WindowsNT4")
    {
        /*
         * We must limit CPUID count for Windows NT 4, as otherwise it stops
         * with error 0x3e (MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED).
         */
        LogRel(("Limiting CPUID leaf count for NT4 guests\n"));
        PCFGMNODE pCPUM;
        rc = CFGMR3InsertNode(pRoot, "CPUM", &pCPUM);                                   RC_CHECK();
        rc = CFGMR3InsertInteger(pCPUM, "NT4LeafLimit", true);                          RC_CHECK();
    }

    if (fOsXGuest)
    {
        /*
         * Expose extended MWAIT features to Mac OS X guests.
         */
        LogRel(("Using MWAIT extensions\n"));
        PCFGMNODE pCPUM;
        rc = CFGMR3InsertNode(pRoot, "CPUM", &pCPUM);                                   RC_CHECK();
        rc = CFGMR3InsertInteger(pCPUM, "MWaitExtensions", true);                       RC_CHECK();
    }

    /* hardware virtualization extensions */
    BOOL fHWVirtExEnabled;
    BOOL fHwVirtExtForced;
#ifdef VBOX_WITH_RAW_MODE
    hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_Enabled, &fHWVirtExEnabled); H();
    if (cCpus > 1) /** @todo SMP: This isn't nice, but things won't work on mac otherwise. */
        fHWVirtExEnabled = TRUE;
# ifdef RT_OS_DARWIN
    fHwVirtExtForced = fHWVirtExEnabled;
# else
    /* - With more than 4GB PGM will use different RAMRANGE sizes for raw
         mode and hv mode to optimize lookup times.
       - With more than one virtual CPU, raw-mode isn't a fallback option. */
    fHwVirtExtForced = fHWVirtExEnabled
                    && (   cbRam > (_4G - cbRamHole)
                        || cCpus > 1);
# endif
#else  /* !VBOX_WITH_RAW_MODE */
    fHWVirtExEnabled = fHwVirtExtForced = TRUE;
#endif /* !VBOX_WITH_RAW_MODE */
    rc = CFGMR3InsertInteger(pRoot, "HwVirtExtForced",      fHwVirtExtForced);          RC_CHECK();

    PCFGMNODE pHWVirtExt;
    rc = CFGMR3InsertNode(pRoot, "HWVirtExt", &pHWVirtExt);                             RC_CHECK();
    if (fHWVirtExEnabled)
    {
        rc = CFGMR3InsertInteger(pHWVirtExt, "Enabled",     1);                         RC_CHECK();

        /* Indicate whether 64-bit guests are supported or not. */
        /** @todo This is currently only forced off on 32-bit hosts only because it
         *        makes a lof of difference there (REM and Solaris performance).
         */
        BOOL fSupportsLongMode = false;
        hrc = host->GetProcessorFeature(ProcessorFeature_LongMode,
                                        &fSupportsLongMode);                            H();
        hrc = guestOSType->COMGETTER(Is64Bit)(&fIs64BitGuest);                          H();

        if (fSupportsLongMode && fIs64BitGuest)
        {
            rc = CFGMR3InsertInteger(pHWVirtExt, "64bitEnabled", 1);                    RC_CHECK();
#if ARCH_BITS == 32 /* The recompiler must use VBoxREM64 (32-bit host only). */
            PCFGMNODE pREM;
            rc = CFGMR3InsertNode(pRoot, "REM", &pREM);                                 RC_CHECK();
            rc = CFGMR3InsertInteger(pREM, "64bitEnabled", 1);                          RC_CHECK();
#endif
        }
#if ARCH_BITS == 32 /* 32-bit guests only. */
        else
        {
            rc = CFGMR3InsertInteger(pHWVirtExt, "64bitEnabled", 0);                    RC_CHECK();
        }
#endif

        /** @todo Not exactly pretty to check strings; VBOXOSTYPE would be better, but that requires quite a bit of API change in Main. */
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
            rc = CFGMR3InsertInteger(pHWVirtExt, "TPRPatchingEnabled", 1);              RC_CHECK();
        }
    }

    /* HWVirtEx exclusive mode */
    BOOL fHWVirtExExclusive = true;
    hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_Exclusive, &fHWVirtExExclusive); H();
    rc = CFGMR3InsertInteger(pHWVirtExt, "Exclusive", fHWVirtExExclusive);              RC_CHECK();

    /* Nested paging (VT-x/AMD-V) */
    BOOL fEnableNestedPaging = false;
    hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_NestedPaging, &fEnableNestedPaging); H();
    rc = CFGMR3InsertInteger(pHWVirtExt, "EnableNestedPaging", fEnableNestedPaging);    RC_CHECK();

    /* Large pages; requires nested paging */
    BOOL fEnableLargePages = false;
    hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_LargePages, &fEnableLargePages); H();
    rc = CFGMR3InsertInteger(pHWVirtExt, "EnableLargePages", fEnableLargePages);        RC_CHECK();

    /* VPID (VT-x) */
    BOOL fEnableVPID = false;
    hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_VPID, &fEnableVPID);       H();
    rc = CFGMR3InsertInteger(pHWVirtExt, "EnableVPID", fEnableVPID);                    RC_CHECK();

    /* Physical Address Extension (PAE) */
    BOOL fEnablePAE = false;
    hrc = pMachine->GetCPUProperty(CPUPropertyType_PAE, &fEnablePAE);                   H();
    rc = CFGMR3InsertInteger(pRoot, "EnablePAE", fEnablePAE);                           RC_CHECK();

    /* Synthetic CPU */
    BOOL fSyntheticCpu = false;
    hrc = pMachine->GetCPUProperty(CPUPropertyType_Synthetic, &fSyntheticCpu);          H();
    rc = CFGMR3InsertInteger(pRoot, "SyntheticCpu", fSyntheticCpu);                     RC_CHECK();

    BOOL fPXEDebug;
    hrc = biosSettings->COMGETTER(PXEDebugEnabled)(&fPXEDebug);                         H();

    /*
     * PDM config.
     *  Load drivers in VBoxC.[so|dll]
     */
    PCFGMNODE pPDM;
    PCFGMNODE pDrivers;
    PCFGMNODE pMod;
    rc = CFGMR3InsertNode(pRoot,    "PDM", &pPDM);                                      RC_CHECK();
    rc = CFGMR3InsertNode(pPDM,     "Drivers", &pDrivers);                              RC_CHECK();
    rc = CFGMR3InsertNode(pDrivers, "VBoxC", &pMod);                                    RC_CHECK();
#ifdef VBOX_WITH_XPCOM
    // VBoxC is located in the components subdirectory
    char szPathVBoxC[RTPATH_MAX];
    rc = RTPathAppPrivateArch(szPathVBoxC, RTPATH_MAX - sizeof("/components/VBoxC"));   AssertRC(rc);
    strcat(szPathVBoxC, "/components/VBoxC");
    rc = CFGMR3InsertString(pMod,   "Path",  szPathVBoxC);                              RC_CHECK();
#else
    rc = CFGMR3InsertString(pMod,   "Path",  "VBoxC");                                  RC_CHECK();
#endif

    /*
     * I/O settings (cach, max bandwidth, ...).
     */
    PCFGMNODE pPDMAc;
    PCFGMNODE pPDMAcFile;
    rc = CFGMR3InsertNode(pPDM, "AsyncCompletion", &pPDMAc);                            RC_CHECK();
    rc = CFGMR3InsertNode(pPDMAc, "File", &pPDMAcFile);                                 RC_CHECK();

    /* Builtin I/O cache */
    BOOL fIoCache = true;
    hrc = pMachine->COMGETTER(IoCacheEnabled)(&fIoCache);                               H();
    rc = CFGMR3InsertInteger(pPDMAcFile, "CacheEnabled", fIoCache);                     RC_CHECK();

    /* I/O cache size */
    ULONG ioCacheSize = 5;
    hrc = pMachine->COMGETTER(IoCacheSize)(&ioCacheSize);                               H();
    rc = CFGMR3InsertInteger(pPDMAcFile, "CacheSize", ioCacheSize * _1M);               RC_CHECK();

    /* Maximum I/O bandwidth */
    ULONG ioBandwidthMax = 0;
    hrc = pMachine->COMGETTER(IoBandwidthMax)(&ioBandwidthMax);                         H();
    if (ioBandwidthMax != 0)
    {
        rc = CFGMR3InsertInteger(pPDMAcFile, "VMTransferPerSecMax", ioBandwidthMax * _1M); RC_CHECK();
    }

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
    PCFGMNODE pNetBootCfg = NULL;   /* /Devices/pcbios/0/Config/NetBoot/ */

    rc = CFGMR3InsertNode(pRoot, "Devices", &pDevices);                                 RC_CHECK();

    /*
     * PC Arch.
     */
    rc = CFGMR3InsertNode(pDevices, "pcarch", &pDev);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();

    /*
     * The time offset
     */
    LONG64 timeOffset;
    hrc = biosSettings->COMGETTER(TimeOffset)(&timeOffset);                             H();
    PCFGMNODE pTMNode;
    rc = CFGMR3InsertNode(pRoot, "TM", &pTMNode);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pTMNode, "UTCOffset", timeOffset * 1000000);               RC_CHECK();

    /*
     * DMA
     */
    rc = CFGMR3InsertNode(pDevices, "8237A", &pDev);                                    RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted", 1);                  /* boolean */       RC_CHECK();

    /*
     * PCI buses.
     */
    rc = CFGMR3InsertNode(pDevices, "pci", &pDev); /* piix3 */                          RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                                 RC_CHECK();

#if 0 /* enable this to test PCI bridging */
    rc = CFGMR3InsertNode(pDevices, "pcibridge", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",         14);                         RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                         RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIBusNo",             0);/* -> pci[0] */          RC_CHECK();

    rc = CFGMR3InsertNode(pDev,     "1", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          1);                         RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                         RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIBusNo",             1);/* ->pcibridge[0] */     RC_CHECK();

    rc = CFGMR3InsertNode(pDev,     "2", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          3);                         RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                         RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIBusNo",             1);/* ->pcibridge[0] */     RC_CHECK();
#endif

    /*
     * Enable 3 following devices: HPET, SMC, LPC on MacOS X guests
     */
    /*
     * High Precision Event Timer (HPET)
     */
    BOOL fHpetEnabled;
#ifdef VBOX_WITH_HPET
    /* Other guests may wish to use HPET too, but MacOS X not functional without it */
    hrc = pMachine->COMGETTER(HpetEnabled)(&fHpetEnabled);                              H();
    /* so always enable HPET in extended profile */
    fHpetEnabled |= fOsXGuest;
#else
    fHpetEnabled = false;
#endif
    if (fHpetEnabled)
    {
        rc = CFGMR3InsertNode(pDevices, "hpet", &pDev);                                 RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",   1);     /* boolean */              RC_CHECK();
    }

    /*
     * System Management Controller (SMC)
     */
    BOOL fSmcEnabled;
#ifdef VBOX_WITH_SMC
    fSmcEnabled = fOsXGuest;
#else
    fSmcEnabled = false;
#endif
    if (fSmcEnabled)
    {
        Bstr tmpStr2;
        rc = CFGMR3InsertNode(pDevices, "smc", &pDev);                                  RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",   1);     /* boolean */              RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
        rc = getSmcDeviceKey(pMachine,   tmpStr2.asOutParam());                         RC_CHECK();
        rc = CFGMR3InsertString(pCfg,   "DeviceKey", Utf8Str(tmpStr2).raw());           RC_CHECK();
    }

    /*
     * Low Pin Count (LPC) bus
     */
    BOOL fLpcEnabled;
    /** @todo: implement appropriate getter */
#ifdef VBOX_WITH_LPC
    fLpcEnabled = fOsXGuest;
#else
    fLpcEnabled = false;
#endif
    if (fLpcEnabled)
    {
        rc = CFGMR3InsertNode(pDevices, "lpc", &pDev);                                  RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",   1);     /* boolean */              RC_CHECK();
    }

    /*
     * PS/2 keyboard & mouse.
     */
    rc = CFGMR3InsertNode(pDevices, "pckbd", &pDev);                                    RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();

    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                                  RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "KeyboardQueue");           RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            64);                        RC_CHECK();

    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                         RC_CHECK();
    rc = CFGMR3InsertString(pLunL1, "Driver",               "MainKeyboard");            RC_CHECK();
    rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                                   RC_CHECK();
    Keyboard *pKeyboard = pConsole->mKeyboard;
    rc = CFGMR3InsertInteger(pCfg,  "Object",     (uintptr_t)pKeyboard);                RC_CHECK();

    rc = CFGMR3InsertNode(pInst,    "LUN#1", &pLunL0);                                  RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MouseQueue");              RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            128);                       RC_CHECK();

    rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                         RC_CHECK();
    rc = CFGMR3InsertString(pLunL1, "Driver",               "MainMouse");               RC_CHECK();
    rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                                   RC_CHECK();
    Mouse *pMouse = pConsole->mMouse;
    rc = CFGMR3InsertInteger(pCfg,  "Object",     (uintptr_t)pMouse);                   RC_CHECK();

    /*
     * i8254 Programmable Interval Timer And Dummy Speaker
     */
    rc = CFGMR3InsertNode(pDevices, "i8254", &pDev);                                    RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();
#ifdef DEBUG
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
#endif

    /*
     * i8259 Programmable Interrupt Controller.
     */
    rc = CFGMR3InsertNode(pDevices, "i8259", &pDev);                                    RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();

    /*
     * Advanced Programmable Interrupt Controller.
     * SMP: Each CPU has a LAPIC, but we have a single device representing all LAPICs states,
     *      thus only single insert
     */
    rc = CFGMR3InsertNode(pDevices, "apic", &pDev);                                     RC_CHECK();
    rc = CFGMR3InsertNode(pDev, "0", &pInst);                                           RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                                 RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "NumCPUs", cCpus);                                  RC_CHECK();

    if (fIOAPIC)
    {
        /*
         * I/O Advanced Programmable Interrupt Controller.
         */
        rc = CFGMR3InsertNode(pDevices, "ioapic", &pDev);                               RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",          1);     /* boolean */       RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    }

    /*
     * RTC MC146818.
     */
    rc = CFGMR3InsertNode(pDevices, "mc146818", &pDev);                                 RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();
    BOOL fRTCUseUTC;
    hrc = pMachine->COMGETTER(RTCUseUTC)(&fRTCUseUTC);                                  H();
    rc = CFGMR3InsertInteger(pCfg,  "UseUTC", fRTCUseUTC ? 1 : 0);                      RC_CHECK();

    /*
     * VGA.
     */
    rc = CFGMR3InsertNode(pDevices, "vga", &pDev);                                      RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          2);                         RC_CHECK();
    Assert(!afPciDeviceNo[2]);
    afPciDeviceNo[2] = true;
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                         RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();
    ULONG cVRamMBs;
    hrc = pMachine->COMGETTER(VRAMSize)(&cVRamMBs);                                     H();
    rc = CFGMR3InsertInteger(pCfg,  "VRamSize",             cVRamMBs * _1M);            RC_CHECK();
    ULONG cMonitorCount;
    hrc = pMachine->COMGETTER(MonitorCount)(&cMonitorCount);                            H();
    rc = CFGMR3InsertInteger(pCfg,  "MonitorCount",         cMonitorCount);             RC_CHECK();
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    rc = CFGMR3InsertInteger(pCfg,  "R0Enabled",            fHWVirtExEnabled);          RC_CHECK();
#endif

    /*
     * BIOS logo
     */
    BOOL fFadeIn;
    hrc = biosSettings->COMGETTER(LogoFadeIn)(&fFadeIn);                                H();
    rc = CFGMR3InsertInteger(pCfg,  "FadeIn",  fFadeIn ? 1 : 0);                        RC_CHECK();
    BOOL fFadeOut;
    hrc = biosSettings->COMGETTER(LogoFadeOut)(&fFadeOut);                              H();
    rc = CFGMR3InsertInteger(pCfg,  "FadeOut", fFadeOut ? 1: 0);                        RC_CHECK();
    ULONG logoDisplayTime;
    hrc = biosSettings->COMGETTER(LogoDisplayTime)(&logoDisplayTime);                   H();
    rc = CFGMR3InsertInteger(pCfg,  "LogoTime", logoDisplayTime);                       RC_CHECK();
    Bstr logoImagePath;
    hrc = biosSettings->COMGETTER(LogoImagePath)(logoImagePath.asOutParam());           H();
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
    rc = CFGMR3InsertInteger(pCfg, "ShowBootMenu", iShowBootMenu);                      RC_CHECK();

    /* Custom VESA mode list */
    unsigned cModes = 0;
    for (unsigned iMode = 1; iMode <= 16; ++iMode)
    {
        char szExtraDataKey[sizeof("CustomVideoModeXX")];
        RTStrPrintf(szExtraDataKey, sizeof(szExtraDataKey), "CustomVideoMode%u", iMode);
        hrc = pMachine->GetExtraData(Bstr(szExtraDataKey), bstr.asOutParam());          H();
        if (bstr.isEmpty())
            break;
        rc = CFGMR3InsertStringW(pCfg, szExtraDataKey, bstr.raw());                     RC_CHECK();
        ++cModes;
    }
    rc = CFGMR3InsertInteger(pCfg,  "CustomVideoModes", cModes);                        RC_CHECK();

    /* VESA height reduction */
    ULONG ulHeightReduction;
    IFramebuffer *pFramebuffer = pConsole->getDisplay()->getFramebuffer();
    if (pFramebuffer)
    {
        hrc = pFramebuffer->COMGETTER(HeightReduction)(&ulHeightReduction);             H();
    }
    else
    {
        /* If framebuffer is not available, there is no height reduction. */
        ulHeightReduction = 0;
    }
    rc = CFGMR3InsertInteger(pCfg,  "HeightReduction", ulHeightReduction);              RC_CHECK();

    /* Attach the display. */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                                  RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainDisplay");             RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                                   RC_CHECK();
    Display *pDisplay = pConsole->mDisplay;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pDisplay);                     RC_CHECK();


    /*
     * Firmware.
     */
    FirmwareType_T eFwType =  FirmwareType_BIOS;
    hrc = pMachine->COMGETTER(FirmwareType)(&eFwType);                                  H();

#ifdef VBOX_WITH_EFI
    BOOL fEfiEnabled = (eFwType >= FirmwareType_EFI) && (eFwType <= FirmwareType_EFIDUAL);
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
        rc = CFGMR3InsertBytes(pBiosCfg,    "UUID", &HardwareUuid,sizeof(HardwareUuid));RC_CHECK();
        rc = CFGMR3InsertNode(pBiosCfg,   "NetBoot", &pNetBootCfg);                     RC_CHECK();

        DeviceType_T bootDevice;
        if (SchemaDefs::MaxBootPosition > 9)
        {
            AssertMsgFailed(("Too many boot devices %d\n",
                             SchemaDefs::MaxBootPosition));
            return VERR_INVALID_PARAMETER;
        }

        for (ULONG pos = 1; pos <= SchemaDefs::MaxBootPosition; ++pos)
        {
            hrc = pMachine->GetBootOrder(pos, &bootDevice);                             H();

            char szParamName[] = "BootDeviceX";
            szParamName[sizeof(szParamName) - 2] = ((char (pos - 1)) + '0');

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
        Utf8Str efiRomFile;

        /* Autodetect firmware type, basing on guest type */
        if (eFwType == FirmwareType_EFI)
        {
            eFwType =
                    fIs64BitGuest ?
                    (FirmwareType_T)FirmwareType_EFI64
                    :
                    (FirmwareType_T)FirmwareType_EFI32;
        }
        bool f64BitEntry = eFwType == FirmwareType_EFI64;

        rc = findEfiRom(virtualBox, eFwType, efiRomFile);                                                                                                                          RC_CHECK();

        /* Get boot args */
        Bstr bootArgs;
        hrc = pMachine->GetExtraData(Bstr("VBoxInternal2/EfiBootArgs"), bootArgs.asOutParam()); H();

        /* Get device props */
        Bstr deviceProps;
        hrc = pMachine->GetExtraData(Bstr("VBoxInternal2/EfiDeviceProps"), deviceProps.asOutParam()); H();
        /* Get GOP mode settings */
        uint32_t u32GopMode = UINT32_MAX;
        hrc = pMachine->GetExtraData(Bstr("VBoxInternal2/EfiGopMode"), bstr.asOutParam()); H();
        if (!bstr.isEmpty())
            u32GopMode = Utf8Str(bstr).toUInt32();

        /* UGA mode settings */
        uint32_t u32UgaHorisontal = 0;
        hrc = pMachine->GetExtraData(Bstr("VBoxInternal2/EfiUgaHorizontalResolution"), bstr.asOutParam()); H();
        if (!bstr.isEmpty())
            u32UgaHorisontal = Utf8Str(bstr).toUInt32();

        uint32_t u32UgaVertical = 0;
        hrc = pMachine->GetExtraData(Bstr("VBoxInternal2/EfiUgaVerticalResolution"), bstr.asOutParam()); H();
        if (!bstr.isEmpty())
            u32UgaVertical = Utf8Str(bstr).toUInt32();

        /*
         * EFI subtree.
         */
        rc = CFGMR3InsertNode(pDevices, "efi", &pDev);                                  RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted", 1);              /* boolean */       RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamSize",          cbRam);                     RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamHoleSize",      cbRamHole);                 RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "NumCPUs",          cCpus);                     RC_CHECK();
        rc = CFGMR3InsertString(pCfg,   "EfiRom",           efiRomFile.raw());          RC_CHECK();
        rc = CFGMR3InsertString(pCfg,   "BootArgs",         Utf8Str(bootArgs).raw());   RC_CHECK();
        rc = CFGMR3InsertString(pCfg,   "DeviceProps",      Utf8Str(deviceProps).raw());RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "IOAPIC",           fIOAPIC);                   RC_CHECK();
        rc = CFGMR3InsertBytes(pCfg,    "UUID", &HardwareUuid,sizeof(HardwareUuid));    RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "64BitEntry", f64BitEntry); /* boolean */       RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "GopMode", u32GopMode);                         RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "UgaHorizontalResolution", u32UgaHorisontal);   RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "UgaVerticalResolution", u32UgaVertical);       RC_CHECK();

        /* For OS X guests we'll force passing host's DMI info to the guest */
        if (fOsXGuest)
        {
            rc = CFGMR3InsertInteger(pCfg,  "DmiUseHostInfo", 1);                       RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "DmiExposeMemoryTable", 1);                 RC_CHECK();
        }
    }

    /*
     * Storage controllers.
     */
    com::SafeIfaceArray<IStorageController> ctrls;
    PCFGMNODE aCtrlNodes[StorageControllerType_LsiLogicSas + 1] = {};
    hrc = pMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));       H();

    for (size_t i = 0; i < ctrls.size(); ++i)
    {
        DeviceType_T *paLedDevType = NULL;

        StorageControllerType_T enmCtrlType;
        rc = ctrls[i]->COMGETTER(ControllerType)(&enmCtrlType);                         H();
        AssertRelease((unsigned)enmCtrlType < RT_ELEMENTS(aCtrlNodes));

        StorageBus_T enmBus;
        rc = ctrls[i]->COMGETTER(Bus)(&enmBus);                                         H();

        Bstr controllerName;
        rc = ctrls[i]->COMGETTER(Name)(controllerName.asOutParam());                    H();

        ULONG ulInstance = 999;
        rc = ctrls[i]->COMGETTER(Instance)(&ulInstance);                                H();

        IoBackendType_T enmIoBackend;
        rc = ctrls[i]->COMGETTER(IoBackend)(&enmIoBackend);                             H();

        /* /Devices/<ctrldev>/ */
        const char *pszCtrlDev = pConsole->convertControllerTypeToDev(enmCtrlType);
        pDev = aCtrlNodes[enmCtrlType];
        if (!pDev)
        {
            rc = CFGMR3InsertNode(pDevices, pszCtrlDev, &pDev);                         RC_CHECK();
            aCtrlNodes[enmCtrlType] = pDev; /* IDE variants are handled in the switch */
        }

        /* /Devices/<ctrldev>/<instance>/ */
        PCFGMNODE pCtlInst = NULL;
        rc = CFGMR3InsertNodeF(pDev, &pCtlInst, "%u", ulInstance);                      RC_CHECK();

        /* Device config: /Devices/<ctrldev>/<instance>/<values> & /ditto/Config/<values> */
        rc = CFGMR3InsertInteger(pCtlInst, "Trusted",   1);                             RC_CHECK();
        rc = CFGMR3InsertNode(pCtlInst,    "Config",    &pCfg);                         RC_CHECK();

        switch (enmCtrlType)
        {
            case StorageControllerType_LsiLogic:
            {
                rc = CFGMR3InsertInteger(pCtlInst, "PCIDeviceNo",          20);         RC_CHECK();
                Assert(!afPciDeviceNo[20]);
                afPciDeviceNo[20] = true;
                rc = CFGMR3InsertInteger(pCtlInst, "PCIFunctionNo",        0);          RC_CHECK();

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst, "LUN#999", &pLunL0);                    RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");  RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapStorageLeds[iLedScsi]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                         RC_CHECK();
                Assert(cLedScsi >= 16);
                rc = CFGMR3InsertInteger(pCfg,  "Last",     15);                        RC_CHECK();
                paLedDevType = &pConsole->maStorageDevType[iLedScsi];
                break;
            }

            case StorageControllerType_BusLogic:
            {
                rc = CFGMR3InsertInteger(pCtlInst, "PCIDeviceNo",          21);         RC_CHECK();
                Assert(!afPciDeviceNo[21]);
                afPciDeviceNo[21] = true;
                rc = CFGMR3InsertInteger(pCtlInst, "PCIFunctionNo",        0);          RC_CHECK();

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst, "LUN#999", &pLunL0);                    RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");  RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapStorageLeds[iLedScsi]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                         RC_CHECK();
                Assert(cLedScsi >= 16);
                rc = CFGMR3InsertInteger(pCfg,  "Last",     15);                        RC_CHECK();
                paLedDevType = &pConsole->maStorageDevType[iLedScsi];
                break;
            }

            case StorageControllerType_IntelAhci:
            {
                rc = CFGMR3InsertInteger(pCtlInst, "PCIDeviceNo",          13);         RC_CHECK();
                Assert(!afPciDeviceNo[13]);
                afPciDeviceNo[13] = true;
                rc = CFGMR3InsertInteger(pCtlInst, "PCIFunctionNo",        0);          RC_CHECK();

                ULONG cPorts = 0;
                hrc = ctrls[i]->COMGETTER(PortCount)(&cPorts);                          H();
                rc = CFGMR3InsertInteger(pCfg, "PortCount", cPorts);                    RC_CHECK();

                /* Needed configuration values for the bios. */
                if (pBiosCfg)
                {
                    rc = CFGMR3InsertString(pBiosCfg, "SataHardDiskDevice", "ahci");    RC_CHECK();
                }

                for (uint32_t j = 0; j < 4; ++j)
                {
                    static const char * const s_apszConfig[4] =
                    { "PrimaryMaster", "PrimarySlave", "SecondaryMaster", "SecondarySlave" };
                    static const char * const s_apszBiosConfig[4] =
                    { "SataPrimaryMasterLUN", "SataPrimarySlaveLUN", "SataSecondaryMasterLUN", "SataSecondarySlaveLUN" };

                    LONG lPortNumber = -1;
                    hrc = ctrls[i]->GetIDEEmulationPort(j, &lPortNumber);               H();
                    rc = CFGMR3InsertInteger(pCfg, s_apszConfig[j], lPortNumber);       RC_CHECK();
                    if (pBiosCfg)
                    {
                        rc = CFGMR3InsertInteger(pBiosCfg, s_apszBiosConfig[j], lPortNumber); RC_CHECK();
                    }
                }

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst, "LUN#999", &pLunL0);                    RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");  RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                AssertRelease(cPorts <= cLedSata);
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapStorageLeds[iLedSata]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                         RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Last",     cPorts - 1);                RC_CHECK();
                paLedDevType = &pConsole->maStorageDevType[iLedSata];
                break;
            }

            case StorageControllerType_PIIX3:
            case StorageControllerType_PIIX4:
            case StorageControllerType_ICH6:
            {
                /*
                 * IDE (update this when the main interface changes)
                 */
                rc = CFGMR3InsertInteger(pCtlInst, "PCIDeviceNo",          1);          RC_CHECK();
                Assert(!afPciDeviceNo[1]);
                afPciDeviceNo[1] = true;
                rc = CFGMR3InsertInteger(pCtlInst, "PCIFunctionNo",        1);          RC_CHECK();
                rc = CFGMR3InsertString(pCfg,  "Type", controllerString(enmCtrlType));  RC_CHECK();

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst,    "LUN#999", &pLunL0);                 RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");  RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapStorageLeds[iLedIde]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                         RC_CHECK();
                Assert(cLedIde >= 4);
                rc = CFGMR3InsertInteger(pCfg,  "Last",     3);                         RC_CHECK();
                paLedDevType = &pConsole->maStorageDevType[iLedIde];

                /* IDE flavors */
                aCtrlNodes[StorageControllerType_PIIX3] = pDev;
                aCtrlNodes[StorageControllerType_PIIX4] = pDev;
                aCtrlNodes[StorageControllerType_ICH6]  = pDev;
                break;
            }

            case StorageControllerType_I82078:
            {
                /*
                 * i82078 Floppy drive controller
                 */
                fFdcEnabled = true;
                rc = CFGMR3InsertInteger(pCfg,  "IRQ",       6);                        RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "DMA",       2);                        RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "MemMapped", 0 );                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "IOBase",    0x3f0);                    RC_CHECK();

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst, "LUN#999", &pLunL0);                    RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");  RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapStorageLeds[iLedFloppy]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                         RC_CHECK();
                Assert(cLedFloppy >= 1);
                rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                         RC_CHECK();
                paLedDevType = &pConsole->maStorageDevType[iLedFloppy];
                break;
            }

            case StorageControllerType_LsiLogicSas:
            {
                rc = CFGMR3InsertInteger(pCtlInst, "PCIDeviceNo",          22);         RC_CHECK();
                Assert(!afPciDeviceNo[22]);
                afPciDeviceNo[22] = true;
                rc = CFGMR3InsertInteger(pCtlInst, "PCIFunctionNo",        0);          RC_CHECK();

                rc = CFGMR3InsertString(pCfg,  "ControllerType", "SAS1068");            RC_CHECK();

                /* Attach the status driver */
                rc = CFGMR3InsertNode(pCtlInst, "LUN#999", &pLunL0);                    RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");  RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapStorageLeds[iLedSas]); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "First",    0);                         RC_CHECK();
                Assert(cLedSas >= 8);
                rc = CFGMR3InsertInteger(pCfg,  "Last",     7);                         RC_CHECK();
                paLedDevType = &pConsole->maStorageDevType[iLedSas];
                break;
            }

            default:
                AssertMsgFailedReturn(("invalid storage controller type: %d\n", enmCtrlType), VERR_GENERAL_FAILURE);
        }

        /* Attach the media to the storage controllers. */
        com::SafeIfaceArray<IMediumAttachment> atts;
        hrc = pMachine->GetMediumAttachmentsOfController(controllerName,
                                                         ComSafeArrayAsOutParam(atts)); H();

        for (size_t j = 0; j < atts.size(); ++j)
        {
            rc = pConsole->configMediumAttachment(pCtlInst,
                                                  pszCtrlDev,
                                                  ulInstance,
                                                  enmBus,
                                                  enmIoBackend,
                                                  false /* fSetupMerge */,
                                                  0 /* uMergeSource */,
                                                  0 /* uMergeTarget */,
                                                  atts[j],
                                                  pConsole->mMachineState,
                                                  NULL /* phrc */,
                                                  false /* fAttachDetach */,
                                                  false /* fForceUnmount */,
                                                  NULL /* pVM */,
                                                  paLedDevType);                        RC_CHECK();
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
    rc = CFGMR3InsertNode(pDevices, "pcnet", &pDevPCNet);                               RC_CHECK();
#ifdef VBOX_WITH_E1000
    PCFGMNODE pDevE1000 = NULL;          /* E1000-type devices */
    rc = CFGMR3InsertNode(pDevices, "e1000", &pDevE1000);                               RC_CHECK();
#endif
#ifdef VBOX_WITH_VIRTIO
    PCFGMNODE pDevVirtioNet = NULL;          /* Virtio network devices */
    rc = CFGMR3InsertNode(pDevices, "virtio-net", &pDevVirtioNet);                      RC_CHECK();
#endif /* VBOX_WITH_VIRTIO */
    std::list<BootNic> llBootNics;
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::NetworkAdapterCount; ++ulInstance)
    {
        ComPtr<INetworkAdapter> networkAdapter;
        hrc = pMachine->GetNetworkAdapter(ulInstance, networkAdapter.asOutParam());     H();
        BOOL fEnabled = FALSE;
        hrc = networkAdapter->COMGETTER(Enabled)(&fEnabled);                            H();
        if (!fEnabled)
            continue;

        /*
         * The virtual hardware type. Create appropriate device first.
         */
        const char *pszAdapterName = "pcnet";
        NetworkAdapterType_T adapterType;
        hrc = networkAdapter->COMGETTER(AdapterType)(&adapterType);                     H();
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
                pszAdapterName = "virtio-net";
                break;
#endif /* VBOX_WITH_VIRTIO */
            default:
                AssertMsgFailed(("Invalid network adapter type '%d' for slot '%d'",
                                 adapterType, ulInstance));
                return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                  N_("Invalid network adapter type '%d' for slot '%d'"),
                                  adapterType, ulInstance);
        }

        rc = CFGMR3InsertNodeF(pDev, &pInst, "%u", ulInstance);                         RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",              1); /* boolean */       RC_CHECK();
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
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo", iPciDeviceNo);                   RC_CHECK();
        Assert(!afPciDeviceNo[iPciDeviceNo]);
        afPciDeviceNo[iPciDeviceNo] = true;
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                                  RC_CHECK();
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE /* not safe here yet. */
        if (pDev == pDevPCNet)
        {
            rc = CFGMR3InsertInteger(pCfg,  "R0Enabled",    false);                     RC_CHECK();
        }
#endif
        /*
         * Collect information needed for network booting and add it to the list.
         */
        BootNic     nic;

        nic.mInstance = ulInstance;
        nic.mPciDev   = iPciDeviceNo;
        nic.mPciFn    = 0;

        hrc = networkAdapter->COMGETTER(BootPriority)(&nic.mBootPrio);                  H();

        llBootNics.push_back(nic);

        /*
         * The virtual hardware type. PCNet supports two types.
         */
        switch (adapterType)
        {
            case NetworkAdapterType_Am79C970A:
                rc = CFGMR3InsertInteger(pCfg, "Am79C973", 0);                          RC_CHECK();
                break;
            case NetworkAdapterType_Am79C973:
                rc = CFGMR3InsertInteger(pCfg, "Am79C973", 1);                          RC_CHECK();
                break;
            case NetworkAdapterType_I82540EM:
                rc = CFGMR3InsertInteger(pCfg, "AdapterType", 0);                       RC_CHECK();
                break;
            case NetworkAdapterType_I82543GC:
                rc = CFGMR3InsertInteger(pCfg, "AdapterType", 1);                       RC_CHECK();
                break;
            case NetworkAdapterType_I82545EM:
                rc = CFGMR3InsertInteger(pCfg, "AdapterType", 2);                       RC_CHECK();
                break;
        }

        /*
         * Get the MAC address and convert it to binary representation
         */
        Bstr macAddr;
        hrc = networkAdapter->COMGETTER(MACAddress)(macAddr.asOutParam());              H();
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
        rc = CFGMR3InsertBytes(pCfg, "MAC", &Mac, sizeof(Mac));                         RC_CHECK();

        /*
         * Check if the cable is supposed to be unplugged
         */
        BOOL fCableConnected;
        hrc = networkAdapter->COMGETTER(CableConnected)(&fCableConnected);              H();
        rc = CFGMR3InsertInteger(pCfg, "CableConnected", fCableConnected ? 1 : 0);      RC_CHECK();

        /*
         * Line speed to report from custom drivers
         */
        ULONG ulLineSpeed;
        hrc = networkAdapter->COMGETTER(LineSpeed)(&ulLineSpeed);                       H();
        rc = CFGMR3InsertInteger(pCfg, "LineSpeed", ulLineSpeed);                       RC_CHECK();

        /*
         * Attach the status driver.
         */
        rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                            RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");          RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapNetworkLeds[ulInstance]); RC_CHECK();

        /*
         * Configure the network card now
         */
        rc = pConsole->configNetwork(pszAdapterName, ulInstance, 0,
                                     networkAdapter, pCfg, pLunL0, pInst,
                                     false /*fAttachDetach*/);                          RC_CHECK();
    }

    /*
     * Build network boot information and transfer it to the BIOS.
     */
    if (pNetBootCfg && !llBootNics.empty())  /* NetBoot node doesn't exist for EFI! */
    {
        llBootNics.sort();  /* Sort the list by boot priority. */

        char        achBootIdx[] = "0";
        unsigned    uBootIdx = 0;

        for (std::list<BootNic>::iterator it = llBootNics.begin(); it != llBootNics.end(); ++it)
        {
            /* A NIC with priority 0 is only used if it's first in the list. */
            if (it->mBootPrio == 0 && uBootIdx != 0)
                break;

            PCFGMNODE pNetBtDevCfg;
            achBootIdx[0] = '0' + uBootIdx++;   /* Boot device order. */
            rc = CFGMR3InsertNode(pNetBootCfg, achBootIdx, &pNetBtDevCfg);              RC_CHECK();
            rc = CFGMR3InsertInteger(pNetBtDevCfg, "NIC", it->mInstance);               RC_CHECK();
            rc = CFGMR3InsertInteger(pNetBtDevCfg, "PCIDeviceNo", it->mPciDev);         RC_CHECK();
            rc = CFGMR3InsertInteger(pNetBtDevCfg, "PCIFunctionNo", it->mPciFn);        RC_CHECK();
        }
    }

    /*
     * Serial (UART) Ports
     */
    rc = CFGMR3InsertNode(pDevices, "serial", &pDev);                                   RC_CHECK();
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::SerialPortCount; ++ulInstance)
    {
        ComPtr<ISerialPort> serialPort;
        hrc = pMachine->GetSerialPort(ulInstance, serialPort.asOutParam());             H();
        BOOL fEnabled = FALSE;
        if (serialPort)
            hrc = serialPort->COMGETTER(Enabled)(&fEnabled);                            H();
        if (!fEnabled)
            continue;

        rc = CFGMR3InsertNodeF(pDev, &pInst, "%u", ulInstance);                         RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                                  RC_CHECK();

        ULONG ulIRQ;
        hrc = serialPort->COMGETTER(IRQ)(&ulIRQ);                                       H();
        rc = CFGMR3InsertInteger(pCfg,   "IRQ", ulIRQ);                                 RC_CHECK();
        ULONG ulIOBase;
        hrc = serialPort->COMGETTER(IOBase)(&ulIOBase);                                 H();
        rc = CFGMR3InsertInteger(pCfg,   "IOBase", ulIOBase);                           RC_CHECK();
        BOOL  fServer;
        hrc = serialPort->COMGETTER(Server)(&fServer);                                  H();
        hrc = serialPort->COMGETTER(Path)(bstr.asOutParam());                           H();
        PortMode_T eHostMode;
        hrc = serialPort->COMGETTER(HostMode)(&eHostMode);                              H();
        if (eHostMode != PortMode_Disconnected)
        {
            rc = CFGMR3InsertNode(pInst,     "LUN#0", &pLunL0);                         RC_CHECK();
            if (eHostMode == PortMode_HostPipe)
            {
                rc = CFGMR3InsertString(pLunL0,  "Driver", "Char");                     RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,    "AttachedDriver", &pLunL1);            RC_CHECK();
                rc = CFGMR3InsertString(pLunL1,  "Driver", "NamedPipe");                RC_CHECK();
                rc = CFGMR3InsertNode(pLunL1,    "Config", &pLunL2);                    RC_CHECK();
                rc = CFGMR3InsertStringW(pLunL2, "Location", bstr.raw());               RC_CHECK();
                rc = CFGMR3InsertInteger(pLunL2, "IsServer", fServer);                  RC_CHECK();
            }
            else if (eHostMode == PortMode_HostDevice)
            {
                rc = CFGMR3InsertString(pLunL0,  "Driver", "Host Serial");              RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,    "Config", &pLunL1);                    RC_CHECK();
                rc = CFGMR3InsertStringW(pLunL1, "DevicePath", bstr.raw());             RC_CHECK();
            }
            else if (eHostMode == PortMode_RawFile)
            {
                rc = CFGMR3InsertString(pLunL0,  "Driver", "Char");                     RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,    "AttachedDriver", &pLunL1);            RC_CHECK();
                rc = CFGMR3InsertString(pLunL1,  "Driver", "RawFile");                  RC_CHECK();
                rc = CFGMR3InsertNode(pLunL1,    "Config", &pLunL2);                    RC_CHECK();
                rc = CFGMR3InsertStringW(pLunL2, "Location", bstr.raw());               RC_CHECK();
            }
        }
    }

    /*
     * Parallel (LPT) Ports
     */
    rc = CFGMR3InsertNode(pDevices, "parallel", &pDev);                                 RC_CHECK();
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::ParallelPortCount; ++ulInstance)
    {
        ComPtr<IParallelPort> parallelPort;
        hrc = pMachine->GetParallelPort(ulInstance, parallelPort.asOutParam());         H();
        BOOL fEnabled = FALSE;
        if (parallelPort)
        {
            hrc = parallelPort->COMGETTER(Enabled)(&fEnabled);                          H();
        }
        if (!fEnabled)
            continue;

        rc = CFGMR3InsertNodeF(pDev, &pInst, "%u", ulInstance);                         RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                                  RC_CHECK();

        ULONG ulIRQ;
        hrc = parallelPort->COMGETTER(IRQ)(&ulIRQ);                                     H();
        rc = CFGMR3InsertInteger(pCfg,   "IRQ", ulIRQ);                                 RC_CHECK();
        ULONG ulIOBase;
        hrc = parallelPort->COMGETTER(IOBase)(&ulIOBase);                               H();
        rc = CFGMR3InsertInteger(pCfg,   "IOBase", ulIOBase);                           RC_CHECK();
        rc = CFGMR3InsertNode(pInst,     "LUN#0", &pLunL0);                             RC_CHECK();
        rc = CFGMR3InsertString(pLunL0,  "Driver", "HostParallel");                     RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,    "AttachedDriver", &pLunL1);                    RC_CHECK();
        hrc = parallelPort->COMGETTER(Path)(bstr.asOutParam());                         H();
        rc = CFGMR3InsertStringW(pLunL1,  "DevicePath", bstr.raw());                    RC_CHECK();
    }

    /*
     * VMM Device
     */
    rc = CFGMR3InsertNode(pDevices, "VMMDev", &pDev);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */       RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          4);                         RC_CHECK();
    Assert(!afPciDeviceNo[4]);
    afPciDeviceNo[4] = true;
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                         RC_CHECK();
    Bstr hwVersion;
    hrc = pMachine->COMGETTER(HardwareVersion)(hwVersion.asOutParam());                 H();
    if (hwVersion.compare(Bstr("1")) == 0) /* <= 2.0.x */
    {
        CFGMR3InsertInteger(pCfg, "HeapEnabled", 0);                                    RC_CHECK();
    }

    /* the VMM device's Main driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                                  RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "HGCM");                    RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                                   RC_CHECK();
    VMMDev *pVMMDev = pConsole->mVMMDev;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pVMMDev);                      RC_CHECK();

    /*
     * Attach the status driver.
     */
    rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                                RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");              RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapSharedFolderLed); RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                     RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                                     RC_CHECK();

    /*
     * Audio Sniffer Device
     */
    rc = CFGMR3InsertNode(pDevices, "AudioSniffer", &pDev);                             RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                       RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                                   RC_CHECK();

    /* the Audio Sniffer device's Main driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                                  RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainAudioSniffer");        RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                                   RC_CHECK();
    AudioSniffer *pAudioSniffer = pConsole->mAudioSniffer;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pAudioSniffer);                RC_CHECK();

    /*
     * AC'97 ICH / SoundBlaster16 audio
     */
    BOOL enabled;
    ComPtr<IAudioAdapter> audioAdapter;
    hrc = pMachine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());                 H();
    if (audioAdapter)
        hrc = audioAdapter->COMGETTER(Enabled)(&enabled);                               H();

    if (enabled)
    {
        AudioControllerType_T audioController;
        hrc = audioAdapter->COMGETTER(AudioController)(&audioController);               H();
        switch (audioController)
        {
            case AudioControllerType_AC97:
            {
                /* default: ICH AC97 */
                rc = CFGMR3InsertNode(pDevices, "ichac97", &pDev);                      RC_CHECK();
                rc = CFGMR3InsertNode(pDev,     "0", &pInst);
                rc = CFGMR3InsertInteger(pInst, "Trusted",          1); /* bool */      RC_CHECK();
                rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",      5);                 RC_CHECK();
                Assert(!afPciDeviceNo[5]);
                afPciDeviceNo[5] = true;
                rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",    0);                 RC_CHECK();
                rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                       RC_CHECK();
                break;
            }
            case AudioControllerType_SB16:
            {
                /* legacy SoundBlaster16 */
                rc = CFGMR3InsertNode(pDevices, "sb16", &pDev);                         RC_CHECK();
                rc = CFGMR3InsertNode(pDev,     "0", &pInst);                           RC_CHECK();
                rc = CFGMR3InsertInteger(pInst, "Trusted",          1); /* bool */      RC_CHECK();
                rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "IRQ", 5);                              RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "DMA", 1);                              RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "DMA16", 5);                            RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Port", 0x220);                         RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Version", 0x0405);                     RC_CHECK();
                break;
            }
        }

        /* the Audio driver */
        rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "AUDIO");               RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();

        AudioDriverType_T audioDriver;
        hrc = audioAdapter->COMGETTER(AudioDriver)(&audioDriver);                       H();
        switch (audioDriver)
        {
            case AudioDriverType_Null:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "null");                   RC_CHECK();
                break;
            }
#ifdef RT_OS_WINDOWS
#ifdef VBOX_WITH_WINMM
            case AudioDriverType_WinMM:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "winmm");                  RC_CHECK();
                break;
            }
#endif
            case AudioDriverType_DirectSound:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "dsound");                 RC_CHECK();
                break;
            }
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_SOLARIS
            case AudioDriverType_SolAudio:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "solaudio");               RC_CHECK();
                break;
            }
#endif
#ifdef RT_OS_LINUX
# ifdef VBOX_WITH_ALSA
            case AudioDriverType_ALSA:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "alsa");                   RC_CHECK();
                break;
            }
# endif
# ifdef VBOX_WITH_PULSE
            case AudioDriverType_Pulse:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "pulse");                  RC_CHECK();
                break;
            }
# endif
#endif /* RT_OS_LINUX */
#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD) || defined(VBOX_WITH_SOLARIS_OSS)
            case AudioDriverType_OSS:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "oss");                    RC_CHECK();
                break;
            }
#endif
#ifdef RT_OS_FREEBSD
# ifdef VBOX_WITH_PULSE
            case AudioDriverType_Pulse:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "pulse");                  RC_CHECK();
                break;
            }
# endif
#endif
#ifdef RT_OS_DARWIN
            case AudioDriverType_CoreAudio:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "coreaudio");              RC_CHECK();
                break;
            }
#endif
        }
        hrc = pMachine->COMGETTER(Name)(bstr.asOutParam());                             H();
        rc = CFGMR3InsertStringW(pCfg, "StreamName", bstr.raw());                       RC_CHECK();
    }

    /*
     * The USB Controller.
     */
    ComPtr<IUSBController> USBCtlPtr;
    hrc = pMachine->COMGETTER(USBController)(USBCtlPtr.asOutParam());
    if (USBCtlPtr)
    {
        BOOL fOhciEnabled;
        hrc = USBCtlPtr->COMGETTER(Enabled)(&fOhciEnabled);                             H();
        if (fOhciEnabled)
        {
            rc = CFGMR3InsertNode(pDevices, "usb-ohci", &pDev);                         RC_CHECK();
            rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
            rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
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
            BOOL fEhciEnabled;
            hrc = USBCtlPtr->COMGETTER(EnabledEhci)(&fEhciEnabled);                     H();
            if (fEhciEnabled)
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
#endif

            /*
             * Virtual USB Devices.
             */
            PCFGMNODE pUsbDevices = NULL;
            rc = CFGMR3InsertNode(pRoot, "USB", &pUsbDevices);                          RC_CHECK();

#ifdef VBOX_WITH_USB
            {
                /*
                 * Global USB options, currently unused as we'll apply the 2.0 -> 1.1 morphing
                 * on a per device level now.
                 */
                rc = CFGMR3InsertNode(pUsbDevices, "USBProxy", &pCfg);                  RC_CHECK();
                rc = CFGMR3InsertNode(pCfg, "GlobalConfig", &pCfg);                     RC_CHECK();
                // This globally enables the 2.0 -> 1.1 device morphing of proxied devies to keep windows quiet.
                //rc = CFGMR3InsertInteger(pCfg, "Force11Device", true);                RC_CHECK();
                // The following breaks stuff, but it makes MSDs work in vista. (I include it here so
                // that it's documented somewhere.) Users needing it can use:
                //      VBoxManage setextradata "myvm" "VBoxInternal/USB/USBProxy/GlobalConfig/Force11PacketSize" 1
                //rc = CFGMR3InsertInteger(pCfg, "Force11PacketSize", true);            RC_CHECK();
            }
#endif

# if 0  /* Virtual MSD*/

            rc = CFGMR3InsertNode(pUsbDevices, "Msd", &pDev);                           RC_CHECK();
            rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
            rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
            rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                          RC_CHECK();

            rc = CFGMR3InsertString(pLunL0, "Driver", "SCSI");                          RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver", "Block");                         RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Type", "HardDisk");                        RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "Mountable", 0);                            RC_CHECK();

            rc = CFGMR3InsertNode(pLunL1,   "AttachedDriver", &pLunL2);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL2, "Driver", "VD");                            RC_CHECK();
            rc = CFGMR3InsertNode(pLunL2,   "Config", &pCfg);                           RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Path", "/Volumes/DataHFS/bird/VDIs/linux.vdi"); RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Format", "VDI");                           RC_CHECK();
# endif

            /* Virtual USB Mouse/Tablet */
            PointingHidType_T aPointingHid;
            hrc = pMachine->COMGETTER(PointingHidType)(&aPointingHid);                  H();
            if (aPointingHid == PointingHidType_USBMouse || aPointingHid == PointingHidType_USBTablet)
            {
                rc = CFGMR3InsertNode(pUsbDevices, "HidMouse", &pDev);                  RC_CHECK();
                rc = CFGMR3InsertNode(pDev,     "0", &pInst);                           RC_CHECK();
                rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                       RC_CHECK();

                if (aPointingHid == PointingHidType_USBTablet)
                {
                    rc = CFGMR3InsertInteger(pCfg, "Absolute", 1);                      RC_CHECK();
                }
                else
                {
                    rc = CFGMR3InsertInteger(pCfg, "Absolute", 0);                      RC_CHECK();
                }
                rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                      RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",        "MouseQueue");         RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            128);           RC_CHECK();

                rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);             RC_CHECK();
                rc = CFGMR3InsertString(pLunL1, "Driver",        "MainMouse");          RC_CHECK();
                rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                       RC_CHECK();
                pMouse = pConsole->mMouse;
                rc = CFGMR3InsertInteger(pCfg,  "Object",     (uintptr_t)pMouse);       RC_CHECK();
            }

            /* Virtual USB Keyboard */
            KeyboardHidType_T aKbdHid;
            hrc = pMachine->COMGETTER(KeyboardHidType)(&aKbdHid);                       H();
            if (aKbdHid == KeyboardHidType_USBKeyboard)
            {
                rc = CFGMR3InsertNode(pUsbDevices, "HidKeyboard", &pDev);               RC_CHECK();
                rc = CFGMR3InsertNode(pDev,     "0", &pInst);                           RC_CHECK();
                rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                       RC_CHECK();

                rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                      RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",               "KeyboardQueue"); RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "QueueSize",            64);            RC_CHECK();

                rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);             RC_CHECK();
                rc = CFGMR3InsertString(pLunL1, "Driver",               "MainKeyboard"); RC_CHECK();
                rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                       RC_CHECK();
                pKeyboard = pConsole->mKeyboard;
                rc = CFGMR3InsertInteger(pCfg,  "Object",     (uintptr_t)pKeyboard);    RC_CHECK();
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
            rc = pConsole->mVMMDev->hgcmLoadService("VBoxSharedClipboard", "VBoxSharedClipboard");

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

                pConsole->mVMMDev->hgcmHostCall("VBoxSharedClipboard", VBOX_SHARED_CLIPBOARD_HOST_FN_SET_MODE, 1, &parm);

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
            rc = pConsole->mVMMDev->hgcmLoadService("VBoxSharedCrOpenGL", "VBoxSharedCrOpenGL");
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

                parm.u.pointer.addr = (IConsole*) (Console*) pConsole;
                parm.u.pointer.size = sizeof(IConsole *);

                rc = pConsole->mVMMDev->hgcmHostCall("VBoxSharedCrOpenGL", SHCRGL_HOST_FN_SET_CONSOLE,
                                                     SHCRGL_CPARMS_SET_CONSOLE, &parm);
                if (!RT_SUCCESS(rc))
                    AssertMsgFailed(("SHCRGL_HOST_FN_SET_CONSOLE failed with %Rrc\n", rc));

                parm.u.pointer.addr = pVM;
                parm.u.pointer.size = sizeof(pVM);
                rc = pConsole->mVMMDev->hgcmHostCall("VBoxSharedCrOpenGL", SHCRGL_HOST_FN_SET_VM,
                                                     SHCRGL_CPARMS_SET_VM, &parm);
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

#ifdef VBOX_WITH_GUEST_CONTROL
    /*
     * Guest control service
     */

    rc = configGuestControl(pConsole);
#endif /* VBOX_WITH_GUEST_CONTROL defined */

    /*
     * ACPI
     */
    BOOL fACPI;
    hrc = biosSettings->COMGETTER(ACPIEnabled)(&fACPI);                                 H();
    if (fACPI)
    {
        BOOL fCpuHotPlug = false;
        BOOL fShowCpu = fOsXGuest;
        /* Always show the CPU leafs when we have multiple VCPUs or when the IO-APIC is enabled.
         * The Windows SMP kernel needs a CPU leaf or else its idle loop will burn cpu cycles; the
         * intelppm driver refuses to register an idle state handler.
         */
        if ((cCpus > 1) || fIOAPIC)
            fShowCpu = true;

        hrc = pMachine->COMGETTER(CPUHotPlugEnabled)(&fCpuHotPlug);                     H();

        rc = CFGMR3InsertNode(pDevices, "acpi", &pDev);                                 RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted", 1);              /* boolean */       RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamSize",          cbRam);                     RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamHoleSize",      cbRamHole);                 RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "NumCPUs",          cCpus);                     RC_CHECK();

        rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                             RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "FdcEnabled", fFdcEnabled);                     RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "HpetEnabled", fHpetEnabled);                   RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "SmcEnabled", fSmcEnabled);                     RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "ShowRtc",    fOsXGuest);                       RC_CHECK();
        if (fOsXGuest && !llBootNics.empty())
        {
            BootNic aNic = llBootNics.front();
            uint32_t u32NicPciAddr = (aNic.mPciDev << 16) | aNic.mPciFn;
            rc = CFGMR3InsertInteger(pCfg,  "NicPciAddress",    u32NicPciAddr);         RC_CHECK();
        }
        rc = CFGMR3InsertInteger(pCfg,  "ShowCpu", fShowCpu);                           RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "CpuHotPlug", fCpuHotPlug);                     RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          7);                     RC_CHECK();
        Assert(!afPciDeviceNo[7]);
        afPciDeviceNo[7] = true;
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();

        rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "ACPIHost");            RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();

        /* Attach the dummy CPU drivers */
        for (ULONG iCpuCurr = 1; iCpuCurr < cCpus; iCpuCurr++)
        {
            BOOL fCpuAttached = true;

            if (fCpuHotPlug)
            {
                hrc = pMachine->GetCPUStatus(iCpuCurr, &fCpuAttached);                  H();
            }

            if (fCpuAttached)
            {
                rc = CFGMR3InsertNodeF(pInst, &pLunL0, "LUN#%u", iCpuCurr);             RC_CHECK();
                rc = CFGMR3InsertString(pLunL0, "Driver",           "ACPICpu");         RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
            }
        }
    }


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

    size_t i2 = 0;
    for (std::list<Utf8Str>::const_iterator it = llExtraDataKeys.begin();
         it != llExtraDataKeys.end();
         ++it, ++i2)
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
        if (i2 < cGlobalValues)
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

#undef H
#undef RC_CHECK

    /* Register VM state change handler */
    int rc2 = VMR3AtStateRegister(pVM, Console::vmstateChangeCallback, pConsole);
    AssertRC(rc2);
    if (RT_SUCCESS(rc))
        rc = rc2;

    /* Register VM runtime error handler */
    rc2 = VMR3AtRuntimeErrorRegister(pVM, Console::setVMRuntimeErrorCallback, pConsole);
    AssertRC(rc2);
    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowFunc(("vrc = %Rrc\n", rc));
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

int Console::configMediumAttachment(PCFGMNODE pCtlInst,
                                    const char *pcszDevice,
                                    unsigned uInstance,
                                    StorageBus_T enmBus,
                                    IoBackendType_T enmIoBackend,
                                    bool fSetupMerge,
                                    unsigned uMergeSource,
                                    unsigned uMergeTarget,
                                    IMediumAttachment *pMediumAtt,
                                    MachineState_T aMachineState,
                                    HRESULT *phrc,
                                    bool fAttachDetach,
                                    bool fForceUnmount,
                                    PVM pVM,
                                    DeviceType_T *paLedDevType)
{
    int rc = VINF_SUCCESS;
    HRESULT hrc;
    Bstr    bstr;

#define RC_CHECK()  AssertMsgReturn(RT_SUCCESS(rc), ("rc=%Rrc\n", rc), rc)
#define H()         AssertMsgReturn(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), VERR_GENERAL_FAILURE)

    LONG lDev;
    hrc = pMediumAtt->COMGETTER(Device)(&lDev);                                         H();
    LONG lPort;
    hrc = pMediumAtt->COMGETTER(Port)(&lPort);                                          H();
    DeviceType_T lType;
    hrc = pMediumAtt->COMGETTER(Type)(&lType);                                          H();

    unsigned uLUN;
    PCFGMNODE pLunL0 = NULL;
    PCFGMNODE pCfg = NULL;
    hrc = Console::convertBusPortDeviceToLun(enmBus, lPort, lDev, uLUN);                H();

    /* First check if the LUN already exists. */
    pLunL0 = CFGMR3GetChildF(pCtlInst, "LUN#%u", uLUN);
    if (pLunL0)
    {
        if (fAttachDetach)
        {
            if (lType != DeviceType_HardDisk)
            {
                /* Unmount existing media only for floppy and DVD drives. */
                PPDMIBASE pBase;
                rc = PDMR3QueryLun(pVM, pcszDevice, uInstance, uLUN, &pBase);
                if (RT_FAILURE(rc))
                {
                    if (rc == VERR_PDM_LUN_NOT_FOUND || rc == VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN)
                        rc = VINF_SUCCESS;
                    AssertRC(rc);
                }
                else
                {
                    PPDMIMOUNT pIMount = PDMIBASE_QUERY_INTERFACE(pBase, PDMIMOUNT);
                    AssertReturn(pIMount, VERR_INVALID_POINTER);

                    /* Unmount the media. */
                    rc = pIMount->pfnUnmount(pIMount, fForceUnmount);
                    if (rc == VERR_PDM_MEDIA_NOT_MOUNTED)
                        rc = VINF_SUCCESS;
                }
            }

            rc = PDMR3DeviceDetach(pVM, pcszDevice, 0, uLUN, PDM_TACH_FLAGS_NOT_HOT_PLUG);
            if (rc == VERR_PDM_NO_DRIVER_ATTACHED_TO_LUN)
                rc = VINF_SUCCESS;
            RC_CHECK();

            CFGMR3RemoveNode(pLunL0);
        }
        else
            AssertFailedReturn(VERR_INTERNAL_ERROR);
    }

    rc = CFGMR3InsertNodeF(pCtlInst, &pLunL0, "LUN#%u", uLUN);                          RC_CHECK();

    /* SCSI has a another driver between device and block. */
    if (enmBus == StorageBus_SCSI || enmBus == StorageBus_SAS)
    {
        rc = CFGMR3InsertString(pLunL0, "Driver", "SCSI");                              RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                                 RC_CHECK();

        rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);                       RC_CHECK();
    }

    ComPtr<IMedium> pMedium;
    hrc = pMediumAtt->COMGETTER(Medium)(pMedium.asOutParam());                          H();
    BOOL fPassthrough;
    hrc = pMediumAtt->COMGETTER(Passthrough)(&fPassthrough);                            H();
    rc = configMedium(pLunL0,
                      !!fPassthrough,
                      lType,
                      enmIoBackend,
                      fSetupMerge,
                      uMergeSource,
                      uMergeTarget,
                      pMedium,
                      aMachineState,
                      phrc);                                                            RC_CHECK();

    if (fAttachDetach)
    {
        /* Attach the new driver. */
        rc = PDMR3DeviceAttach(pVM, pcszDevice, 0, uLUN,
                               PDM_TACH_FLAGS_NOT_HOT_PLUG, NULL /*ppBase*/);           RC_CHECK();

        /* There is no need to handle removable medium mounting, as we
         * unconditionally replace everthing including the block driver level.
         * This means the new medium will be picked up automatically. */
    }

    if (paLedDevType)
        paLedDevType[uLUN] = lType;

#undef H
#undef RC_CHECK

    return VINF_SUCCESS;;
}

int Console::configMedium(PCFGMNODE pLunL0,
                          bool fPassthrough,
                          DeviceType_T enmType,
                          IoBackendType_T enmIoBackend,
                          bool fSetupMerge,
                          unsigned uMergeSource,
                          unsigned uMergeTarget,
                          IMedium *pMedium,
                          MachineState_T aMachineState,
                          HRESULT *phrc)
{
    int rc = VINF_SUCCESS;
    HRESULT hrc;
    Bstr bstr;

#define RC_CHECK()  AssertMsgReturn(RT_SUCCESS(rc), ("rc=%Rrc\n", rc), rc)
#define H()         AssertMsgReturnStmt(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), if (phrc) *phrc = hrc, VERR_GENERAL_FAILURE)

    PCFGMNODE pLunL1 = NULL;
    PCFGMNODE pCfg = NULL;

    BOOL fHostDrive = FALSE;
    if (pMedium)
    {
        hrc = pMedium->COMGETTER(HostDrive)(&fHostDrive);                               H();
    }

    if (fHostDrive)
    {
        Assert(pMedium);
        if (enmType == DeviceType_DVD)
        {
            rc = CFGMR3InsertString(pLunL0, "Driver", "HostDVD");                       RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                             RC_CHECK();

            hrc = pMedium->COMGETTER(Location)(bstr.asOutParam());                      H();
            rc = CFGMR3InsertStringW(pCfg, "Path", bstr.raw());                         RC_CHECK();

            rc = CFGMR3InsertInteger(pCfg, "Passthrough", fPassthrough);                RC_CHECK();
        }
        else if (enmType == DeviceType_Floppy)
        {
            rc = CFGMR3InsertString(pLunL0, "Driver", "HostFloppy");                    RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                             RC_CHECK();

            hrc = pMedium->COMGETTER(Location)(bstr.asOutParam());                      H();
            rc = CFGMR3InsertStringW(pCfg, "Path", bstr.raw());                         RC_CHECK();
        }
    }
    else
    {
        rc = CFGMR3InsertString(pLunL0, "Driver", "Block");                             RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                                 RC_CHECK();
        switch (enmType)
        {
            case DeviceType_DVD:
                rc = CFGMR3InsertString(pCfg, "Type", "DVD");                           RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg, "Mountable", 1);                         RC_CHECK();
                break;
            case DeviceType_Floppy:
                rc = CFGMR3InsertString(pCfg, "Type", "Floppy 1.44");                   RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg, "Mountable", 1);                         RC_CHECK();
                break;
            case DeviceType_HardDisk:
            default:
                rc = CFGMR3InsertString(pCfg, "Type", "HardDisk");                      RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg, "Mountable", 0);                         RC_CHECK();
        }

        if (    pMedium
             && (    enmType == DeviceType_DVD
                  || enmType == DeviceType_Floppy
           ))
        {
            // if this medium represents an ISO image and this image is inaccessible,
            // the ignore it instead of causing a failure; this can happen when we
            // restore a VM state and the ISO has disappeared, e.g. because the Guest
            // Additions were mounted and the user upgraded VirtualBox. Previously
            // we failed on startup, but that's not good because the only way out then
            // would be to discard the VM state...
            MediumState_T mediumState;
            rc = pMedium->RefreshState(&mediumState);
            RC_CHECK();
            if (mediumState == MediumState_Inaccessible)
            {
                Bstr loc;
                rc = pMedium->COMGETTER(Location)(loc.asOutParam());
                if (FAILED(rc)) return rc;

                setVMRuntimeErrorCallbackF(mpVM,
                                           this,
                                           0,
                                           "DvdOrFloppyImageInaccessible",
                                           "The image file '%ls' is inaccessible and is being ignored. Please select a different image file for the virtual %s drive.",
                                           loc.raw(),
                                           (enmType == DeviceType_DVD) ? "DVD" : "floppy");
                pMedium = NULL;
            }
        }

        if (pMedium)
        {
            /* Start with length of parent chain, as the list is reversed */
            unsigned uImage = 0;
            IMedium *pTmp = pMedium;
            while (pTmp)
            {
                uImage++;
                hrc = pTmp->COMGETTER(Parent)(&pTmp);                                   H();
            }
            /* Index of last image */
            uImage--;

            rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL1);                   RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver", "VD");                            RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1, "Config", &pCfg);                             RC_CHECK();

            hrc = pMedium->COMGETTER(Location)(bstr.asOutParam());                      H();
            rc = CFGMR3InsertStringW(pCfg, "Path", bstr.raw());                         RC_CHECK();

            hrc = pMedium->COMGETTER(Format)(bstr.asOutParam());                        H();
            rc = CFGMR3InsertStringW(pCfg, "Format", bstr.raw());                       RC_CHECK();

            /* DVDs are always readonly */
            if (enmType == DeviceType_DVD)
            {
                rc = CFGMR3InsertInteger(pCfg, "ReadOnly", 1);                          RC_CHECK();
            }

            /* Start without exclusive write access to the images. */
            /** @todo Live Migration: I don't quite like this, we risk screwing up when
             *        we're resuming the VM if some 3rd dude have any of the VDIs open
             *        with write sharing denied.  However, if the two VMs are sharing a
             *        image it really is necessary....
             *
             *        So, on the "lock-media" command, the target teleporter should also
             *        make DrvVD undo TempReadOnly.  It gets interesting if we fail after
             *        that. Grumble. */
            else if (aMachineState == MachineState_TeleportingIn)
            {
                rc = CFGMR3InsertInteger(pCfg, "TempReadOnly", 1);                      RC_CHECK();
            }

            if (enmIoBackend == IoBackendType_Unbuffered)
            {
                rc = CFGMR3InsertInteger(pCfg, "UseNewIo", 1);                          RC_CHECK();
            }

            if (fSetupMerge)
            {
                rc = CFGMR3InsertInteger(pCfg, "SetupMerge", 1);                        RC_CHECK();
                if (uImage == uMergeSource)
                {
                    rc = CFGMR3InsertInteger(pCfg, "MergeSource", 1);                   RC_CHECK();
                }
                else if (uImage == uMergeTarget)
                {
                    rc = CFGMR3InsertInteger(pCfg, "MergeTarget", 1);                   RC_CHECK();
                }
            }

            /* Pass all custom parameters. */
            bool fHostIP = true;
            SafeArray<BSTR> names;
            SafeArray<BSTR> values;
            hrc = pMedium->GetProperties(NULL,
                                         ComSafeArrayAsOutParam(names),
                                         ComSafeArrayAsOutParam(values));               H();

            if (names.size() != 0)
            {
                PCFGMNODE pVDC;
                rc = CFGMR3InsertNode(pCfg, "VDConfig", &pVDC);                         RC_CHECK();
                for (size_t ii = 0; ii < names.size(); ++ii)
                {
                    if (values[ii] && *values[ii])
                    {
                        Utf8Str name = names[ii];
                        Utf8Str value = values[ii];
                        rc = CFGMR3InsertString(pVDC, name.c_str(), value.c_str());     RC_CHECK();
                        if (    name.compare("HostIPStack") == 0
                            &&  value.compare("0") == 0)
                            fHostIP = false;
                    }
                }
            }

            /* Create an inversed list of parents. */
            uImage--;
            IMedium *pParentMedium = pMedium;
            for (PCFGMNODE pParent = pCfg;; uImage--)
            {
                hrc = pParentMedium->COMGETTER(Parent)(&pMedium);                       H();
                if (!pMedium)
                    break;

                PCFGMNODE pCur;
                rc = CFGMR3InsertNode(pParent, "Parent", &pCur);                        RC_CHECK();
                hrc = pMedium->COMGETTER(Location)(bstr.asOutParam());                  H();
                rc = CFGMR3InsertStringW(pCur, "Path", bstr.raw());                     RC_CHECK();

                hrc = pMedium->COMGETTER(Format)(bstr.asOutParam());                    H();
                rc = CFGMR3InsertStringW(pCur, "Format", bstr.raw());                   RC_CHECK();

                if (fSetupMerge)
                {
                    if (uImage == uMergeSource)
                    {
                        rc = CFGMR3InsertInteger(pCur, "MergeSource", 1);               RC_CHECK();
                    }
                    else if (uImage == uMergeTarget)
                    {
                        rc = CFGMR3InsertInteger(pCur, "MergeTarget", 1);               RC_CHECK();
                    }
                }

                /* Pass all custom parameters. */
                SafeArray<BSTR> aNames;
                SafeArray<BSTR> aValues;
                hrc = pMedium->GetProperties(NULL,
                                             ComSafeArrayAsOutParam(aNames),
                                             ComSafeArrayAsOutParam(aValues));          H();

                if (aNames.size() != 0)
                {
                    PCFGMNODE pVDC;
                    rc = CFGMR3InsertNode(pCur, "VDConfig", &pVDC);                     RC_CHECK();
                    for (size_t ii = 0; ii < aNames.size(); ++ii)
                    {
                        if (aValues[ii] && *aValues[ii])
                        {
                            Utf8Str name = aNames[ii];
                            Utf8Str value = aValues[ii];
                            rc = CFGMR3InsertString(pVDC, name.c_str(), value.c_str()); RC_CHECK();
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
                    rc = CFGMR3InsertInteger(pCfg, "HostIPStack", 0);                   RC_CHECK();
                }

                /* next */
                pParent = pCur;
                pParentMedium = pMedium;
            }
        }
    }

#undef H
#undef RC_CHECK

    return VINF_SUCCESS;
}

/**
 *  Construct the Network configuration tree
 *
 *  @returns VBox status code.
 *
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
 *  @note Locks this object for writing.
 */
int Console::configNetwork(const char *pszDevice, unsigned uInstance,
                           unsigned uLun, INetworkAdapter *aNetworkAdapter,
                           PCFGMNODE pCfg, PCFGMNODE pLunL0, PCFGMNODE pInst,
                           bool fAttachDetach)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), VERR_ACCESS_DENIED);

    int rc = VINF_SUCCESS;
    HRESULT hrc;
    Bstr bstr;

#define RC_CHECK()  AssertMsgReturn(RT_SUCCESS(rc), ("rc=%Rrc\n", rc), rc)
#define H()         AssertMsgReturn(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), VERR_GENERAL_FAILURE)

    /*
     * Locking the object before doing VMR3* calls is quite safe here, since
     * we're on EMT. Write lock is necessary because we indirectly modify the
     * meAttachmentType member.
     */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    PVM pVM = mpVM;

    ComPtr<IMachine> pMachine = machine();

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
        if (meAttachmentType[uInstance] == NetworkAttachmentType_NAT)
            pszNetDriver = "NAT";
#if !defined(VBOX_WITH_NETFLT) && defined(RT_OS_LINUX)
        if (meAttachmentType[uInstance] == NetworkAttachmentType_Bridged)
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
            rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                             RC_CHECK();
            rc = CFGMR3InsertString(pLunL0, "Driver", "NetSniffer");                    RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                             RC_CHECK();
            hrc = aNetworkAdapter->COMGETTER(TraceFile)(bstr.asOutParam());             H();
            if (!bstr.isEmpty()) /* check convention for indicating default file. */
            {
                rc = CFGMR3InsertStringW(pCfg, "File", bstr.raw());                     RC_CHECK();
            }
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
        rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                                 RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver", "NetSniffer");                        RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                                 RC_CHECK();
        hrc = aNetworkAdapter->COMGETTER(TraceFile)(bstr.asOutParam());                 H();
        if (!bstr.isEmpty()) /* check convention for indicating default file. */
        {
            rc = CFGMR3InsertStringW(pCfg, "File", bstr.raw());                         RC_CHECK();
        }
    }

    Bstr networkName, trunkName, trunkType;
    NetworkAttachmentType_T eAttachmentType;
    hrc = aNetworkAdapter->COMGETTER(AttachmentType)(&eAttachmentType);                 H();
    switch (eAttachmentType)
    {
        case NetworkAttachmentType_Null:
            break;

        case NetworkAttachmentType_NAT:
        {
            ComPtr<INATEngine> natDriver;
            hrc = aNetworkAdapter->COMGETTER(NatDriver)(natDriver.asOutParam());        H();
            if (fSniffer)
            {
                rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);               RC_CHECK();
            }
            else
            {
                rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                         RC_CHECK();
            }
            rc = CFGMR3InsertString(pLunL0, "Driver", "NAT");                           RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                             RC_CHECK();

            /* Configure TFTP prefix and boot filename. */
            hrc = virtualBox->COMGETTER(HomeFolder)(bstr.asOutParam());                 H();
            if (!bstr.isEmpty())
            {
                rc = CFGMR3InsertStringF(pCfg, "TFTPPrefix", "%ls%c%s", bstr.raw(), RTPATH_DELIMITER, "TFTP"); RC_CHECK();
            }
            hrc = pMachine->COMGETTER(Name)(bstr.asOutParam());                         H();
            rc = CFGMR3InsertStringF(pCfg, "BootFile", "%ls.pxe", bstr.raw());          RC_CHECK();

            hrc = natDriver->COMGETTER(Network)(bstr.asOutParam());                     H();
            if (!bstr.isEmpty())
            {
                rc = CFGMR3InsertStringW(pCfg, "Network", bstr.raw());                  RC_CHECK();
            }
            else
            {
                ULONG uSlot;
                hrc = aNetworkAdapter->COMGETTER(Slot)(&uSlot);                         H();
                rc = CFGMR3InsertStringF(pCfg, "Network", "10.0.%d.0/24", uSlot+2);     RC_CHECK();
            }
            hrc = natDriver->COMGETTER(HostIP)(bstr.asOutParam());                      H();
            if (!bstr.isEmpty())
            {
                rc = CFGMR3InsertStringW(pCfg, "BindIP", bstr.raw());                   RC_CHECK();
            }
            ULONG mtu = 0;
            ULONG sockSnd = 0;
            ULONG sockRcv = 0;
            ULONG tcpSnd = 0;
            ULONG tcpRcv = 0;
            hrc = natDriver->GetNetworkSettings(&mtu, &sockSnd, &sockRcv, &tcpSnd, &tcpRcv); H();
            if (mtu)
            {
                rc = CFGMR3InsertInteger(pCfg, "SlirpMTU", mtu);                        RC_CHECK();
            }
            if (sockRcv)
            {
                rc = CFGMR3InsertInteger(pCfg, "SockRcv", sockRcv);                     RC_CHECK();
            }
            if (sockSnd)
            {
                rc = CFGMR3InsertInteger(pCfg, "SockSnd", sockSnd);                     RC_CHECK();
            }
            if (tcpRcv)
            {
                rc = CFGMR3InsertInteger(pCfg, "TcpRcv", tcpRcv);                       RC_CHECK();
            }
            if (tcpSnd)
            {
                rc = CFGMR3InsertInteger(pCfg, "TcpSnd", tcpSnd);                       RC_CHECK();
            }
            hrc = natDriver->COMGETTER(TftpPrefix)(bstr.asOutParam());                  H();
            if (!bstr.isEmpty())
            {
                rc = CFGMR3RemoveValue(pCfg, "TFTPPrefix");                             RC_CHECK();
                rc = CFGMR3InsertStringW(pCfg, "TFTPPrefix", bstr);                     RC_CHECK();
            }
            hrc = natDriver->COMGETTER(TftpBootFile)(bstr.asOutParam());                H();
            if (!bstr.isEmpty())
            {
                rc = CFGMR3RemoveValue(pCfg, "BootFile");                               RC_CHECK();
                rc = CFGMR3InsertStringW(pCfg, "BootFile", bstr);                       RC_CHECK();
            }
            hrc = natDriver->COMGETTER(TftpNextServer)(bstr.asOutParam());              H();
            if (!bstr.isEmpty())
            {
                rc = CFGMR3InsertStringW(pCfg, "NextServer", bstr);                     RC_CHECK();
            }
            BOOL fDnsFlag;
            hrc = natDriver->COMGETTER(DnsPassDomain)(&fDnsFlag);                       H();
            rc = CFGMR3InsertInteger(pCfg, "PassDomain", fDnsFlag);                     RC_CHECK();
            hrc = natDriver->COMGETTER(DnsProxy)(&fDnsFlag);                            H();
            rc = CFGMR3InsertInteger(pCfg, "DNSProxy", fDnsFlag);                       RC_CHECK();
            hrc = natDriver->COMGETTER(DnsUseHostResolver)(&fDnsFlag);                  H();
            rc = CFGMR3InsertInteger(pCfg, "UseHostResolver", fDnsFlag);                RC_CHECK();

            ULONG aliasMode;
            hrc = natDriver->COMGETTER(AliasMode)(&aliasMode);                          H();
            rc = CFGMR3InsertInteger(pCfg, "AliasMode", aliasMode);                     RC_CHECK();

            /* port-forwarding */
            SafeArray<BSTR> pfs;
            hrc = natDriver->COMGETTER(Redirects)(ComSafeArrayAsOutParam(pfs));         H();
            PCFGMNODE pPF = NULL;          /* /Devices/Dev/.../Config/PF#0/ */
            for (unsigned int i = 0; i < pfs.size(); ++i)
            {
                uint16_t port = 0;
                BSTR r = pfs[i];
                Utf8Str utf = Utf8Str(r);
                Utf8Str strName;
                Utf8Str strProto;
                Utf8Str strHostPort;
                Utf8Str strHostIP;
                Utf8Str strGuestPort;
                Utf8Str strGuestIP;
                size_t pos, ppos;
                pos = ppos = 0;
#define ITERATE_TO_NEXT_TERM(res, str, pos, ppos) \
    do { \
        pos = str.find(",", ppos); \
        if (pos == Utf8Str::npos) \
        { \
            Log(( #res " extracting from %s is failed\n", str.raw())); \
            continue; \
        } \
        res = str.substr(ppos, pos - ppos); \
        Log2((#res " %s pos:%d, ppos:%d\n", res.raw(), pos, ppos)); \
        ppos = pos + 1; \
    } while (0)
                ITERATE_TO_NEXT_TERM(strName, utf, pos, ppos);
                ITERATE_TO_NEXT_TERM(strProto, utf, pos, ppos);
                ITERATE_TO_NEXT_TERM(strHostIP, utf, pos, ppos);
                ITERATE_TO_NEXT_TERM(strHostPort, utf, pos, ppos);
                ITERATE_TO_NEXT_TERM(strGuestIP, utf, pos, ppos);
                strGuestPort = utf.substr(ppos, utf.length() - ppos);
#undef ITERATE_TO_NEXT_TERM

                uint32_t proto = strProto.toUInt32();
                bool fValid = true;
                switch (proto)
                {
                    case NATProtocol_UDP:
                        strProto = "UDP";
                        break;
                    case NATProtocol_TCP:
                        strProto = "TCP";
                        break;
                    default:
                        fValid = false;
                }
                /* continue with next rule if no valid proto was passed */
                if (!fValid)
                    continue;

                rc = CFGMR3InsertNode(pCfg, strName.raw(), &pPF);                       RC_CHECK();
                rc = CFGMR3InsertString(pPF, "Protocol", strProto.raw());               RC_CHECK();

                if (!strHostIP.isEmpty())
                {
                    rc = CFGMR3InsertString(pPF, "BindIP", strHostIP.raw());            RC_CHECK();
                }

                if (!strGuestIP.isEmpty())
                {
                    rc = CFGMR3InsertString(pPF, "GuestIP", strGuestIP.raw());          RC_CHECK();
                }

                port = RTStrToUInt16(strHostPort.raw());
                if (port)
                {
                    rc = CFGMR3InsertInteger(pPF, "HostPort", port);                    RC_CHECK();
                }

                port = RTStrToUInt16(strGuestPort.raw());
                if (port)
                {
                    rc = CFGMR3InsertInteger(pPF, "GuestPort", port);                   RC_CHECK();
                }
            }
            break;
        }

        case NetworkAttachmentType_Bridged:
        {
#if (defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)) && !defined(VBOX_WITH_NETFLT)
            hrc = attachToTapInterface(aNetworkAdapter);
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

            Assert((int)maTapFD[uInstance] >= 0);
            if ((int)maTapFD[uInstance] >= 0)
            {
                if (fSniffer)
                {
                    rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);           RC_CHECK();
                }
                else
                {
                    rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                     RC_CHECK();
                }
                rc = CFGMR3InsertString(pLunL0, "Driver", "HostInterface");             RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                         RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg, "FileHandle", maTapFD[uInstance]);       RC_CHECK();
            }

#elif defined(VBOX_WITH_NETFLT)
            /*
             * This is the new VBoxNetFlt+IntNet stuff.
             */
            if (fSniffer)
            {
                rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);               RC_CHECK();
            }
            else
            {
                rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                         RC_CHECK();
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
                /*
                 * Dynamic changing of attachment causes an attempt to configure
                 * network with invalid host adapter (as it is must be changed before
                 * the attachment), calling Detach here will cause a deadlock.
                 * See #4750.
                 * hrc = aNetworkAdapter->Detach();                        H();
                 */
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

            hrc = hostInterface->COMGETTER(Id)(bstr.asOutParam());
            if (FAILED(hrc))
            {
                LogRel(("NetworkAttachmentType_Bridged: COMGETTER(Id) failed, hrc (0x%x)", hrc));
                H();
            }
            Guid hostIFGuid(bstr);

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
#  if defined(RT_OS_FREEBSD)
            /*
             * If we bridge to a tap interface open it the `old' direct way.
             * This works and performs better than bridging a physical
             * interface via the current FreeBSD vboxnetflt implementation.
             */
            if (!strncmp(pszHifName, "tap", sizeof "tap" - 1)) {
                hrc = attachToTapInterface(aNetworkAdapter);
                if (FAILED(hrc))
                {
                    switch (hrc)
                    {
                        case VERR_ACCESS_DENIED:
                            return VMSetError(pVM, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,  N_(
                                              "Failed to open '/dev/%s' for read/write access.  Please check the "
                                              "permissions of that node, and that the net.link.tap.user_open "
                                              "sysctl is set.  Either run 'chmod 0666 /dev/%s' or "
                                              "change the group of that node to vboxusers and make yourself "
                                              "a member of that group.  Make sure that these changes are permanent."), pszHifName, pszHifName);
                        default:
                            AssertMsgFailed(("Could not attach to tap interface! Bad!\n"));
                            return VMSetError(pVM, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS, N_(
                                              "Failed to initialize Host Interface Networking"));
                    }
                }

                Assert((int)maTapFD[uInstance] >= 0);
                if ((int)maTapFD[uInstance] >= 0)
                {
                    rc = CFGMR3InsertString(pLunL0, "Driver", "HostInterface");         RC_CHECK();
                    rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                     RC_CHECK();
                    rc = CFGMR3InsertInteger(pCfg, "FileHandle", maTapFD[uInstance]);   RC_CHECK();
                }
                break;
            }
#  endif
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
                            setVMRuntimeErrorCallbackF(pVM, this, 0, "BridgedInterfaceDown", "Bridged interface %s is down. Guest will not be able to use this interface", pszHifName);
                        }

                    close(iSock);
                }
            }

# else
#  error "PORTME (VBOX_WITH_NETFLT)"
# endif

            rc = CFGMR3InsertString(pLunL0, "Driver", "IntNet");                        RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                             RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Trunk", pszTrunk);                           RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_NetFlt);
            RC_CHECK();
            char szNetwork[INTNET_MAX_NETWORK_NAME];
            RTStrPrintf(szNetwork, sizeof(szNetwork), "HostInterfaceNetworking-%s", pszHifName);
            rc = CFGMR3InsertString(pCfg, "Network", szNetwork);                        RC_CHECK();
            networkName = Bstr(szNetwork);
            trunkName = Bstr(pszTrunk);
            trunkType = Bstr(TRUNKTYPE_NETFLT);

# if defined(RT_OS_DARWIN)
            /** @todo Come up with a better deal here. Problem is that IHostNetworkInterface is completely useless here. */
            if (    strstr(pszHifName, "Wireless")
                ||  strstr(pszHifName, "AirPort" ))
            {
                rc = CFGMR3InsertInteger(pCfg, "SharedMacOnWire", true);                RC_CHECK();
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
# elif defined(RT_OS_FREEBSD)
            int iSock = socket(AF_INET, SOCK_DGRAM, 0);
            if (iSock >= 0)
            {
                struct ieee80211req WReq;
                uint8_t abData[32];

                memset(&WReq, 0, sizeof(WReq));
                strncpy(WReq.i_name, pszHifName, sizeof(WReq.i_name));
                WReq.i_type = IEEE80211_IOC_SSID;
                WReq.i_val = -1;
                WReq.i_data = abData;
                WReq.i_len = sizeof(abData);

                bool fSharedMacOnWire = ioctl(iSock, SIOCG80211, &WReq) >= 0;
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
                    rc = CFGMR3InsertInteger(pCfg, "SharedMacOnWire", true);            RC_CHECK();
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
                rc = CFGMR3InsertInteger(pCfg, "IgnoreAllPromisc", true);               RC_CHECK();
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
            hrc = aNetworkAdapter->COMGETTER(InternalNetwork)(bstr.asOutParam());       H();
            if (!bstr.isEmpty())
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
                rc = CFGMR3InsertString(pLunL0, "Driver", "IntNet");                    RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                         RC_CHECK();
                rc = CFGMR3InsertStringW(pCfg, "Network", bstr);                        RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_WhateverNone); RC_CHECK();
                networkName = bstr;
                trunkType = Bstr(TRUNKTYPE_WHATEVER);
            }
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

            rc = CFGMR3InsertString(pLunL0, "Driver", "IntNet");                        RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                             RC_CHECK();

            Bstr HifName;
            hrc = aNetworkAdapter->COMGETTER(HostInterface)(HifName.asOutParam());
            if (FAILED(hrc))
            {
                LogRel(("NetworkAttachmentType_HostOnly: COMGETTER(HostInterface) failed, hrc (0x%x)\n", hrc));
                H();
            }

            Utf8Str HifNameUtf8(HifName);
            const char *pszHifName = HifNameUtf8.raw();
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

            hrc = hostInterface->COMGETTER(Id)(bstr.asOutParam());
            if (FAILED(hrc))
            {
                LogRel(("NetworkAttachmentType_HostOnly: COMGETTER(Id) failed, hrc (0x%x)\n", hrc));
                H();
            }
            Guid hostIFGuid(bstr);

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

            rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_NetAdp);       RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Trunk", pszTrunk);                           RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Network", szNetwork);                        RC_CHECK();
            networkName = Bstr(szNetwork);
            trunkName   = Bstr(pszTrunk);
            trunkType   = TRUNKTYPE_NETADP;
# endif /* defined VBOX_WITH_NETFLT*/
#elif defined(RT_OS_DARWIN)
            rc = CFGMR3InsertString(pCfg, "Trunk", pszHifName);                         RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Network", szNetwork);                        RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_NetAdp);       RC_CHECK();
            networkName = Bstr(szNetwork);
            trunkName   = Bstr(pszHifName);
            trunkType   = TRUNKTYPE_NETADP;
#else
            rc = CFGMR3InsertString(pCfg, "Trunk", pszHifName);                         RC_CHECK();
            rc = CFGMR3InsertString(pCfg, "Network", szNetwork);                        RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg, "TrunkType", kIntNetTrunkType_NetFlt);       RC_CHECK();
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
            {
                /* Grab the IP number from the 'vboxnetX' instance number (see netif.h) */
                hrc = hostInterface->EnableStaticIpConfig(getDefaultIPv4Address(Bstr(pszHifName)),
                                                          Bstr(VBOXNET_IPV4MASK_DEFAULT));
            }

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

#if defined(VBOX_WITH_VDE)
        case NetworkAttachmentType_VDE:
        {
            hrc = aNetworkAdapter->COMGETTER(VDENetwork)(bstr.asOutParam());            H();
            rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                             RC_CHECK();
            rc = CFGMR3InsertString(pLunL0, "Driver", "VDE");                           RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                             RC_CHECK();
            if (!bstr.isEmpty())
            {
                rc = CFGMR3InsertStringW(pCfg, "Network", bstr);                        RC_CHECK();
                networkName = bstr;
            }
            break;
        }
#endif

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
#if defined(VBOX_WITH_VDE)
        case NetworkAttachmentType_VDE:
#endif
        {
            if (SUCCEEDED(hrc) && SUCCEEDED(rc))
            {
                if (fAttachDetach)
                {
                    rc = PDMR3DriverAttach(pVM, pszDevice, uInstance, uLun, 0 /*fFlags*/, NULL /* ppBase */);
                    //AssertRC(rc);
                }

                {
                    /** @todo pritesh: get the dhcp server name from the
                     * previous network configuration and then stop the server
                     * else it may conflict with the dhcp server running  with
                     * the current attachment type
                     */
                    /* Stop the hostonly DHCP Server */
                }

                if (!networkName.isEmpty())
                {
                    /*
                     * Until we implement service reference counters DHCP Server will be stopped
                     * by DHCPServerRunner destructor.
                     */
                    ComPtr<IDHCPServer> dhcpServer;
                    hrc = virtualBox->FindDHCPServerByNetworkName(networkName, dhcpServer.asOutParam());
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

    meAttachmentType[uInstance] = eAttachmentType;

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
    pVMMDev->hgcmHostCall("VBoxGuestPropSvc", guestProp::SET_PROP_HOST, 3,
                          &parms[0]);
}

/**
 * Set the global flags value by calling the service
 * @returns the status returned by the call to the service
 *
 * @param   pTable  the service instance handle
 * @param   eFlags  the flags to set
 */
int configSetGlobalPropertyFlags(VMMDev * const pVMMDev,
                                 guestProp::ePropFlags eFlags)
{
    VBOXHGCMSVCPARM paParm;
    paParm.setUInt32(eFlags);
    int rc = pVMMDev->hgcmHostCall("VBoxGuestPropSvc",
                                   guestProp::SET_GLOBAL_FLAGS_HOST, 1,
                                   &paParm);
    if (RT_FAILURE(rc))
    {
        char szFlags[guestProp::MAX_FLAGS_LEN];
        if (RT_FAILURE(writeFlags(eFlags, szFlags)))
            Log(("Failed to set the global flags.\n"));
        else
            Log(("Failed to set the global flags \"%s\".\n", szFlags));
    }
    return rc;
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
    ComObjPtr<Console> pConsole = static_cast<Console *>(pvConsole);

    /* Load the service */
    int rc = pConsole->mVMMDev->hgcmLoadService("VBoxGuestPropSvc", "VBoxGuestPropSvc");

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
        HRESULT hrc;
        hrc = pConsole->mControl->PullGuestProperties(ComSafeArrayAsOutParam(namesOut),
                                                      ComSafeArrayAsOutParam(valuesOut),
                                                      ComSafeArrayAsOutParam(timestampsOut),
                                                      ComSafeArrayAsOutParam(flagsOut));
        AssertMsgReturn(SUCCEEDED(hrc), ("hrc=%Rrc\n", hrc), VERR_GENERAL_FAILURE);
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

#ifdef VBOX_WITH_GUEST_PROPS_RDONLY_GUEST
        rc = configSetGlobalPropertyFlags(pConsole->mVMMDev,
                                          guestProp::RDONLYGUEST);
        AssertRCReturn(rc, rc);
#endif

        Log(("Set VBoxGuestPropSvc property store\n"));
    }
    return VINF_SUCCESS;
#else /* !VBOX_WITH_GUEST_PROPS */
    return VERR_NOT_SUPPORTED;
#endif /* !VBOX_WITH_GUEST_PROPS */
}

/**
 * Set up the Guest Control service.
 */
/* static */ int Console::configGuestControl(void *pvConsole)
{
#ifdef VBOX_WITH_GUEST_CONTROL
    AssertReturn(pvConsole, VERR_GENERAL_FAILURE);
    ComObjPtr<Console> pConsole = static_cast<Console *>(pvConsole);

    /* Load the service */
    int rc = pConsole->mVMMDev->hgcmLoadService("VBoxGuestControlSvc", "VBoxGuestControlSvc");

    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxGuestControlSvc is not available. rc = %Rrc\n", rc));
        /* That is not a fatal failure. */
        rc = VINF_SUCCESS;
    }
    else
    {
        HGCMSVCEXTHANDLE hDummy;
        rc = HGCMHostRegisterServiceExtension(&hDummy, "VBoxGuestControlSvc",
                                              &Guest::doGuestCtrlNotification,
                                              pConsole->getGuest());
        if (RT_FAILURE(rc))
            Log(("Cannot register VBoxGuestControlSvc extension!\n"));
        else
            Log(("VBoxGuestControlSvc loaded\n"));
    }

    return rc;
#else /* !VBOX_WITH_GUEST_CONTROL */
    return VERR_NOT_SUPPORTED;
#endif /* !VBOX_WITH_GUEST_CONTROL */
}
