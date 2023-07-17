/* $Id$ */
/** @file
 * VBox Console COM Class implementation - VM Configuration Bits.
 *
 * @remark  We've split out the code that the 64-bit VC++ v8 compiler finds
 *          problematic to optimize so we can disable optimizations and later,
 *          perhaps, find a real solution for it (like rewriting the code and
 *          to stop resemble a tonne of spaghetti).
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN_CONSOLE
#include "LoggingNew.h"

// VBoxNetCfg-win.h needs winsock2.h and thus MUST be included before any other
// header file includes Windows.h.
#if defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)
# include <VBox/VBoxNetCfg-win.h>
#endif

#include "ConsoleImpl.h"
#include "DisplayImpl.h"
#include "NvramStoreImpl.h"
#ifdef VBOX_WITH_SHARED_CLIPBOARD
# include "GuestShClPrivate.h"
#endif
#ifdef VBOX_WITH_DRAG_AND_DROP
# include "GuestImpl.h"
# include "GuestDnDPrivate.h"
#endif
#include "VMMDev.h"
#include "Global.h"
#ifdef VBOX_WITH_PCI_PASSTHROUGH
# include "PCIRawDevImpl.h"
#endif

// generated header
#include "SchemaDefs.h"

#include "AutoCaller.h"

#include <iprt/base64.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/system.h>
#if 0 /* enable to play with lots of memory. */
# include <iprt/env.h>
#endif
#include <iprt/stream.h>

#include <iprt/http.h>
#include <iprt/socket.h>
#include <iprt/uri.h>

#include <VBox/vmm/vmmr3vtable.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/settings.h> /* For MachineConfigFile::getHostDefaultAudioDriver(). */
#include <VBox/vmm/pdmapi.h> /* For PDMR3DriverAttach/PDMR3DriverDetach. */
#include <VBox/vmm/pdmusb.h> /* For PDMR3UsbCreateEmulatedDevice. */
#include <VBox/vmm/pdmdev.h> /* For PDMAPICMODE enum. */
#include <VBox/vmm/pdmstorageifs.h>
#include <VBox/vmm/gcm.h>
#include <VBox/version.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD
# include <VBox/HostServices/VBoxClipboardSvc.h>
#endif
#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>
# include <VBox/com/defs.h>
# include <VBox/com/array.h>
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
# elif defined(RT_OS_FREEBSD)
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/ioctl.h>
#  include <sys/socket.h>
#  include <net/if.h>
#  include <net80211/ieee80211_ioctl.h>
# endif
# if defined(RT_OS_WINDOWS)
#  include <iprt/win/ntddndis.h>
#  include <devguid.h>
# else
#  include <HostNetworkInterfaceImpl.h>
#  include <netif.h>
#  include <stdlib.h>
# endif
#endif /* VBOX_WITH_NETFLT */

#ifdef VBOX_WITH_AUDIO_VRDE
# include "DrvAudioVRDE.h"
#endif
#ifdef VBOX_WITH_AUDIO_RECORDING
# include "DrvAudioRec.h"
#endif
#include "NetworkServiceRunner.h"
#include "BusAssignmentManager.h"
#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/* Darwin compile kludge */
#undef PVM

/* Comment out the following line to remove VMWare compatibility hack. */
#define VMWARE_NET_IN_SLOT_11

/**
 * Translate IDE StorageControllerType_T to string representation.
 */
static const char* controllerString(StorageControllerType_T enmType)
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
    ULONG          mInstance;
    PCIBusAddress  mPCIAddress;

    ULONG          mBootPrio;
    bool operator < (const BootNic &rhs) const
    {
        ULONG lval = mBootPrio     - 1; /* 0 will wrap around and get the lowest priority. */
        ULONG rval = rhs.mBootPrio - 1;
        return lval < rval; /* Zero compares as highest number (lowest prio). */
    }
};

#ifndef VBOX_WITH_EFI_IN_DD2
static int findEfiRom(IVirtualBox* vbox, FirmwareType_T aFirmwareType, Utf8Str *pEfiRomFile)
{
    Bstr aFilePath, empty;
    BOOL fPresent = FALSE;
    HRESULT hrc = vbox->CheckFirmwarePresent(aFirmwareType, empty.raw(),
                                             empty.asOutParam(), aFilePath.asOutParam(), &fPresent);
    AssertComRCReturn(hrc, Global::vboxStatusCodeFromCOM(hrc));

    if (!fPresent)
    {
        LogRel(("Failed to find an EFI ROM file.\n"));
        return VERR_FILE_NOT_FOUND;
    }

    *pEfiRomFile = Utf8Str(aFilePath);

    return VINF_SUCCESS;
}
#endif

/**
 * @throws HRESULT on extra data retrival error.
 */
static int getSmcDeviceKey(IVirtualBox *pVirtualBox, IMachine *pMachine, Utf8Str *pStrKey, bool *pfGetKeyFromRealSMC)
{
    *pfGetKeyFromRealSMC = false;

    /*
     * The extra data takes precedence (if non-zero).
     */
    GetExtraDataBoth(pVirtualBox, pMachine, "VBoxInternal2/SmcDeviceKey", pStrKey);
    if (pStrKey->isNotEmpty())
        return VINF_SUCCESS;

#ifdef RT_OS_DARWIN

    /*
     * Work done in EFI/DevSmc
     */
    *pfGetKeyFromRealSMC = true;
    int vrc = VINF_SUCCESS;

#else
    /*
     * Is it apple hardware in bootcamp?
     */
    /** @todo implement + test RTSYSDMISTR_MANUFACTURER on all hosts.
     *        Currently falling back on the product name. */
    char szManufacturer[256];
    szManufacturer[0] = '\0';
    RTSystemQueryDmiString(RTSYSDMISTR_MANUFACTURER, szManufacturer, sizeof(szManufacturer));
    if (szManufacturer[0] != '\0')
    {
        if (   !strcmp(szManufacturer, "Apple Computer, Inc.")
            || !strcmp(szManufacturer, "Apple Inc.")
            )
            *pfGetKeyFromRealSMC = true;
    }
    else
    {
        char szProdName[256];
        szProdName[0] = '\0';
        RTSystemQueryDmiString(RTSYSDMISTR_PRODUCT_NAME, szProdName, sizeof(szProdName));
        if (   (   !strncmp(szProdName, RT_STR_TUPLE("Mac"))
                || !strncmp(szProdName, RT_STR_TUPLE("iMac"))
                || !strncmp(szProdName, RT_STR_TUPLE("Xserve"))
               )
            && !strchr(szProdName, ' ')                             /* no spaces */
            && RT_C_IS_DIGIT(szProdName[strlen(szProdName) - 1])    /* version number */
           )
            *pfGetKeyFromRealSMC = true;
    }

    int vrc = VINF_SUCCESS;
#endif

    return vrc;
}


/*
 * VC++ 8 / amd64 has some serious trouble with the next functions.
 * As a temporary measure, we'll drop global optimizations.
 */
#if defined(_MSC_VER) && defined(RT_ARCH_AMD64)
# if _MSC_VER >= RT_MSC_VER_VC80 && _MSC_VER < RT_MSC_VER_VC100
#  pragma optimize("g", off)
# endif
#endif

/** Helper that finds out the next HBA port used
 */
static LONG GetNextUsedPort(LONG aPortUsed[30], LONG lBaseVal, uint32_t u32Size)
{
    LONG lNextPortUsed = 30;
    for (size_t j = 0; j < u32Size; ++j)
    {
        if (   aPortUsed[j] >  lBaseVal
            && aPortUsed[j] <= lNextPortUsed)
           lNextPortUsed = aPortUsed[j];
    }
    return lNextPortUsed;
}

#define MAX_BIOS_LUN_COUNT   4

int Console::SetBiosDiskInfo(ComPtr<IMachine> pMachine, PCFGMNODE pCfg, PCFGMNODE pBiosCfg,
                             Bstr controllerName, const char * const s_apszBiosConfig[4])
{
    RT_NOREF(pCfg);
    HRESULT             hrc;
#define MAX_DEVICES     30
#define H()     AssertLogRelMsgReturn(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), VERR_MAIN_CONFIG_CONSTRUCTOR_COM_ERROR)

    LONG lPortLUN[MAX_BIOS_LUN_COUNT];
    LONG lPortUsed[MAX_DEVICES];
    uint32_t u32HDCount = 0;

    /* init to max value */
    lPortLUN[0] = MAX_DEVICES;

    com::SafeIfaceArray<IMediumAttachment> atts;
    hrc = pMachine->GetMediumAttachmentsOfController(controllerName.raw(),
                                        ComSafeArrayAsOutParam(atts));  H();
    size_t uNumAttachments = atts.size();
    if (uNumAttachments > MAX_DEVICES)
    {
        LogRel(("Number of Attachments > Max=%d.\n", uNumAttachments));
        uNumAttachments = MAX_DEVICES;
    }

    /* Find the relevant ports/IDs, i.e the ones to which a HD is attached. */
    for (size_t j = 0; j < uNumAttachments; ++j)
    {
        IMediumAttachment *pMediumAtt = atts[j];
        LONG lPortNum = 0;
        hrc = pMediumAtt->COMGETTER(Port)(&lPortNum);                   H();
        if (SUCCEEDED(hrc))
        {
            DeviceType_T lType;
            hrc = pMediumAtt->COMGETTER(Type)(&lType);                  H();
            if (SUCCEEDED(hrc) && lType == DeviceType_HardDisk)
            {
                /* find min port number used for HD */
                if (lPortNum < lPortLUN[0])
                    lPortLUN[0] = lPortNum;
                lPortUsed[u32HDCount++] = lPortNum;
                LogFlowFunc(("HD port Count=%d\n", u32HDCount));
            }
        }
    }


    /* Pick only the top 4 used HD Ports as CMOS doesn't have space
     * to save details for all 30 ports
     */
    uint32_t u32MaxPortCount = MAX_BIOS_LUN_COUNT;
    if (u32HDCount < MAX_BIOS_LUN_COUNT)
        u32MaxPortCount = u32HDCount;
    for (size_t j = 1; j < u32MaxPortCount; j++)
        lPortLUN[j] = GetNextUsedPort(lPortUsed, lPortLUN[j-1], u32HDCount);
    if (pBiosCfg)
    {
        for (size_t j = 0; j < u32MaxPortCount; j++)
        {
            InsertConfigInteger(pBiosCfg, s_apszBiosConfig[j], lPortLUN[j]);
            LogFlowFunc(("Top %d HBA ports = %s, %d\n", j, s_apszBiosConfig[j], lPortLUN[j]));
        }
    }
    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_PCI_PASSTHROUGH
HRESULT Console::i_attachRawPCIDevices(PUVM pUVM, BusAssignmentManager *pBusMgr, PCFGMNODE pDevices)
{
# ifndef VBOX_WITH_EXTPACK
    RT_NOREF(pUVM);
# endif
    HRESULT hrc = S_OK;
    PCFGMNODE pInst, pCfg, pLunL0, pLunL1;

    SafeIfaceArray<IPCIDeviceAttachment> assignments;
    ComPtr<IMachine> aMachine = i_machine();

    hrc = aMachine->COMGETTER(PCIDeviceAssignments)(ComSafeArrayAsOutParam(assignments));
    if (   hrc != S_OK
        || assignments.size() < 1)
        return hrc;

    /*
     * PCI passthrough is only available if the proper ExtPack is installed.
     *
     * Note. Configuring PCI passthrough here and providing messages about
     * the missing extpack isn't exactly clean, but it is a necessary evil
     * to patch over legacy compatability issues introduced by the new
     * distribution model.
     */
# ifdef VBOX_WITH_EXTPACK
    static const char *s_pszPCIRawExtPackName = "Oracle VM VirtualBox Extension Pack";
    if (!mptrExtPackManager->i_isExtPackUsable(s_pszPCIRawExtPackName))
        /* Always fatal! */
        return pVMM->pfnVMR3SetError(pUVM, VERR_NOT_FOUND, RT_SRC_POS,
                                     N_("Implementation of the PCI passthrough framework not found!\n"
                                        "The VM cannot be started. To fix this problem, either "
                                        "install the '%s' or disable PCI passthrough via VBoxManage"),
                                     s_pszPCIRawExtPackName);
# endif

    /* Now actually add devices */
    PCFGMNODE pPCIDevs = NULL;

    if (assignments.size() > 0)
    {
        InsertConfigNode(pDevices, "pciraw",  &pPCIDevs);

        PCFGMNODE pRoot = CFGMR3GetParent(pDevices); Assert(pRoot);

        /* Tell PGM to tell GPCIRaw about guest mappings. */
        CFGMR3InsertNode(pRoot, "PGM", NULL);
        InsertConfigInteger(CFGMR3GetChild(pRoot, "PGM"), "PciPassThrough", 1);

        /*
         * Currently, using IOMMU needed for PCI passthrough
         * requires RAM preallocation.
         */
        /** @todo check if we can lift this requirement */
        CFGMR3RemoveValue(pRoot, "RamPreAlloc");
        InsertConfigInteger(pRoot, "RamPreAlloc", 1);
    }

    for (size_t iDev = 0; iDev < assignments.size(); iDev++)
    {
        ComPtr<IPCIDeviceAttachment> const assignment = assignments[iDev];

        LONG host;
        hrc = assignment->COMGETTER(HostAddress)(&host);            H();
        LONG guest;
        hrc = assignment->COMGETTER(GuestAddress)(&guest);          H();
        Bstr bstrDevName;
        hrc = assignment->COMGETTER(Name)(bstrDevName.asOutParam());   H();

        InsertConfigNodeF(pPCIDevs, &pInst, "%d", iDev);
        InsertConfigInteger(pInst, "Trusted", 1);

        PCIBusAddress HostPCIAddress(host);
        Assert(HostPCIAddress.valid());
        InsertConfigNode(pInst,        "Config",  &pCfg);
        InsertConfigString(pCfg,       "DeviceName",  bstrDevName);

        InsertConfigInteger(pCfg,      "DetachHostDriver",  1);
        InsertConfigInteger(pCfg,      "HostPCIBusNo",      HostPCIAddress.miBus);
        InsertConfigInteger(pCfg,      "HostPCIDeviceNo",   HostPCIAddress.miDevice);
        InsertConfigInteger(pCfg,      "HostPCIFunctionNo", HostPCIAddress.miFn);

        PCIBusAddress GuestPCIAddress(guest);
        Assert(GuestPCIAddress.valid());
        hrc = pBusMgr->assignHostPCIDevice("pciraw", pInst, HostPCIAddress, GuestPCIAddress, true);
        if (hrc != S_OK)
            return hrc;

        InsertConfigInteger(pCfg,      "GuestPCIBusNo",      GuestPCIAddress.miBus);
        InsertConfigInteger(pCfg,      "GuestPCIDeviceNo",   GuestPCIAddress.miDevice);
        InsertConfigInteger(pCfg,      "GuestPCIFunctionNo", GuestPCIAddress.miFn);

        /* the driver */
        InsertConfigNode(pInst,        "LUN#0",   &pLunL0);
        InsertConfigString(pLunL0,     "Driver", "pciraw");
        InsertConfigNode(pLunL0,       "AttachedDriver", &pLunL1);

        /* the Main driver */
        InsertConfigString(pLunL1,     "Driver", "MainPciRaw");
        InsertConfigNode(pLunL1,       "Config", &pCfg);
        PCIRawDev *pMainDev = new PCIRawDev(this);
# error This is not allowed any more
        InsertConfigInteger(pCfg,      "Object", (uintptr_t)pMainDev);
    }

    return hrc;
}
#endif


/**
 * Worker for configConstructor.
 *
 * @return  VBox status code.
 * @param   pUVM        The user mode VM handle.
 * @param   pVM         The cross context VM handle.
 * @param   pVMM        The VMM vtable.
 * @param   pAlock      The automatic lock instance.  This is for when we have
 *                      to leave it in order to avoid deadlocks (ext packs and
 *                      more).
 */
int Console::i_configConstructorX86(PUVM pUVM, PVM pVM, PCVMMR3VTABLE pVMM, AutoWriteLock *pAlock)
{
    RT_NOREF(pVM /* when everything is disabled */);
    VMMDev         *pVMMDev   = m_pVMMDev; Assert(pVMMDev);
    ComPtr<IMachine> pMachine = i_machine();

    int             vrc;
    HRESULT         hrc;
    Utf8Str         strTmp;
    Bstr            bstr;

#define H()         AssertLogRelMsgReturn(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), VERR_MAIN_CONFIG_CONSTRUCTOR_COM_ERROR)

    /*
     * Get necessary objects and frequently used parameters.
     */
    ComPtr<IVirtualBox> virtualBox;
    hrc = pMachine->COMGETTER(Parent)(virtualBox.asOutParam());                             H();

    ComPtr<IHost> host;
    hrc = virtualBox->COMGETTER(Host)(host.asOutParam());                                   H();

    ComPtr<ISystemProperties> systemProperties;
    hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());           H();

    ComPtr<IBIOSSettings> biosSettings;
    hrc = pMachine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());                     H();

    ComPtr<INvramStore> nvramStore;
    hrc = pMachine->COMGETTER(NonVolatileStore)(nvramStore.asOutParam());                   H();

    hrc = pMachine->COMGETTER(HardwareUUID)(bstr.asOutParam());                             H();
    RTUUID HardwareUuid;
    vrc = RTUuidFromUtf16(&HardwareUuid, bstr.raw());
    AssertRCReturn(vrc, vrc);

    ULONG cRamMBs;
    hrc = pMachine->COMGETTER(MemorySize)(&cRamMBs);                                        H();
#if 0 /* enable to play with lots of memory. */
    if (RTEnvExist("VBOX_RAM_SIZE"))
        cRamMBs = RTStrToUInt64(RTEnvGet("VBOX_RAM_SIZE"));
#endif
    uint64_t const cbRam   = cRamMBs * (uint64_t)_1M;
    uint32_t cbRamHole     = MM_RAM_HOLE_SIZE_DEFAULT;
    uint64_t uMcfgBase     = 0;
    uint32_t cbMcfgLength  = 0;

    ParavirtProvider_T enmParavirtProvider;
    hrc = pMachine->GetEffectiveParavirtProvider(&enmParavirtProvider);                     H();

    Bstr strParavirtDebug;
    hrc = pMachine->COMGETTER(ParavirtDebug)(strParavirtDebug.asOutParam());                H();

    BOOL fIOAPIC;
    uint32_t uIoApicPciAddress = NIL_PCIBDF;
    hrc = biosSettings->COMGETTER(IOAPICEnabled)(&fIOAPIC);                                 H();

    ChipsetType_T chipsetType;
    hrc = pMachine->COMGETTER(ChipsetType)(&chipsetType);                                   H();
    if (chipsetType == ChipsetType_ICH9)
    {
        /* We'd better have 0x10000000 region, to cover 256 buses but this put
         * too much load on hypervisor heap. Linux 4.8 currently complains with
         * ``acpi PNP0A03:00: [Firmware Info]: MMCONFIG for domain 0000 [bus 00-3f]
         *   only partially covers this bridge'' */
        cbMcfgLength = 0x4000000; //0x10000000;
        cbRamHole += cbMcfgLength;
        uMcfgBase = _4G - cbRamHole;
    }

    /* Get the CPU profile name. */
    Bstr bstrCpuProfile;
    hrc = pMachine->COMGETTER(CPUProfile)(bstrCpuProfile.asOutParam());                     H();

    /* Check if long mode is enabled. */
    BOOL fIsGuest64Bit;
    hrc = pMachine->GetCPUProperty(CPUPropertyType_LongMode, &fIsGuest64Bit);               H();

    /*
     * Figure out the IOMMU config.
     */
#if defined(VBOX_WITH_IOMMU_AMD) || defined(VBOX_WITH_IOMMU_INTEL)
    IommuType_T enmIommuType;
    hrc = pMachine->COMGETTER(IommuType)(&enmIommuType);                                    H();

    /* Resolve 'automatic' type to an Intel or AMD IOMMU based on the host CPU. */
    if (enmIommuType == IommuType_Automatic)
    {
        if (   bstrCpuProfile.startsWith("AMD")
            || bstrCpuProfile.startsWith("Quad-Core AMD")
            || bstrCpuProfile.startsWith("Hygon"))
            enmIommuType = IommuType_AMD;
        else if (bstrCpuProfile.startsWith("Intel"))
        {
            if (   bstrCpuProfile.equals("Intel 8086")
                || bstrCpuProfile.equals("Intel 80186")
                || bstrCpuProfile.equals("Intel 80286")
                || bstrCpuProfile.equals("Intel 80386")
                || bstrCpuProfile.equals("Intel 80486"))
                enmIommuType = IommuType_None;
            else
                enmIommuType = IommuType_Intel;
        }
# if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
        else if (ASMIsAmdCpu())
            enmIommuType = IommuType_AMD;
        else if (ASMIsIntelCpu())
            enmIommuType = IommuType_Intel;
# endif
        else
        {
            /** @todo Should we handle other CPUs like Shanghai, VIA etc. here? */
            LogRel(("WARNING! Unrecognized CPU type, IOMMU disabled.\n"));
            enmIommuType = IommuType_None;
        }
    }

    if (enmIommuType == IommuType_AMD)
    {
# ifdef VBOX_WITH_IOMMU_AMD
        /*
         * Reserve the specific PCI address of the "SB I/O APIC" when using
         * an AMD IOMMU. Required by Linux guests, see @bugref{9654#c23}.
         */
        uIoApicPciAddress = VBOX_PCI_BDF_SB_IOAPIC;
# else
        LogRel(("WARNING! AMD IOMMU not supported, IOMMU disabled.\n"));
        enmIommuType = IommuType_None;
# endif
    }

    if (enmIommuType == IommuType_Intel)
    {
# ifdef VBOX_WITH_IOMMU_INTEL
        /*
         * Reserve a unique PCI address for the I/O APIC when using
         * an Intel IOMMU. For convenience we use the same address as
         * we do on AMD, see @bugref{9967#c13}.
         */
        uIoApicPciAddress = VBOX_PCI_BDF_SB_IOAPIC;
# else
        LogRel(("WARNING! Intel IOMMU not supported, IOMMU disabled.\n"));
        enmIommuType = IommuType_None;
# endif
    }

    if (   enmIommuType == IommuType_AMD
        || enmIommuType == IommuType_Intel)
    {
        if (chipsetType != ChipsetType_ICH9)
            return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                         N_("IOMMU uses MSIs which requires the ICH9 chipset implementation."));
        if (!fIOAPIC)
            return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                         N_("IOMMU requires an I/O APIC for remapping interrupts."));
    }
#else
    IommuType_T const enmIommuType = IommuType_None;
#endif

    /* Instantiate the bus assignment manager. */
    Assert(enmIommuType != IommuType_Automatic);
    BusAssignmentManager *pBusMgr = mBusMgr = BusAssignmentManager::createInstance(pVMM, chipsetType, enmIommuType);

    ULONG cCpus = 1;
    hrc = pMachine->COMGETTER(CPUCount)(&cCpus);                                            H();

    ULONG ulCpuExecutionCap = 100;
    hrc = pMachine->COMGETTER(CPUExecutionCap)(&ulCpuExecutionCap);                         H();

    Bstr osTypeId;
    hrc = pMachine->COMGETTER(OSTypeId)(osTypeId.asOutParam());                             H();
    LogRel(("Guest OS type: '%s'\n", Utf8Str(osTypeId).c_str()));

    APICMode_T apicMode;
    hrc = biosSettings->COMGETTER(APICMode)(&apicMode);                                     H();
    uint32_t uFwAPIC;
    switch (apicMode)
    {
        case APICMode_Disabled:
            uFwAPIC = 0;
            break;
        case APICMode_APIC:
            uFwAPIC = 1;
            break;
        case APICMode_X2APIC:
            uFwAPIC = 2;
            break;
        default:
            AssertMsgFailed(("Invalid APICMode=%d\n", apicMode));
            uFwAPIC = 1;
            break;
    }

    ComPtr<IGuestOSType> pGuestOSType;
    virtualBox->GetGuestOSType(osTypeId.raw(), pGuestOSType.asOutParam());

    BOOL fOsXGuest = FALSE;
    BOOL fWinGuest = FALSE;
    BOOL fOs2Guest = FALSE;
    BOOL fW9xGuest = FALSE;
    BOOL fDosGuest = FALSE;
    if (!pGuestOSType.isNull())
    {
        Bstr guestTypeFamilyId;
        hrc = pGuestOSType->COMGETTER(FamilyId)(guestTypeFamilyId.asOutParam());            H();
        fOsXGuest = guestTypeFamilyId == Bstr("MacOS");
        fWinGuest = guestTypeFamilyId == Bstr("Windows");
        fOs2Guest = osTypeId.startsWith("OS2");
        fW9xGuest = osTypeId.startsWith("Windows9");    /* Does not include Windows Me. */
        fDosGuest = osTypeId.startsWith("DOS") || osTypeId.startsWith("Windows31");
    }

    ULONG maxNetworkAdapters;
    hrc = systemProperties->GetMaxNetworkAdapters(chipsetType, &maxNetworkAdapters);        H();

    /*
     * Get root node first.
     * This is the only node in the tree.
     */
    PCFGMNODE pRoot = pVMM->pfnCFGMR3GetRootU(pUVM);
    Assert(pRoot);

    // catching throws from InsertConfigString and friends.
    try
    {

        /*
         * Set the root (and VMM) level values.
         */
        hrc = pMachine->COMGETTER(Name)(bstr.asOutParam());                                 H();
        InsertConfigString(pRoot,  "Name",                 bstr);
        InsertConfigBytes(pRoot,   "UUID", &HardwareUuid, sizeof(HardwareUuid));
        InsertConfigInteger(pRoot, "RamSize",              cbRam);
        InsertConfigInteger(pRoot, "RamHoleSize",          cbRamHole);
        InsertConfigInteger(pRoot, "NumCPUs",              cCpus);
        InsertConfigInteger(pRoot, "CpuExecutionCap",      ulCpuExecutionCap);
        InsertConfigInteger(pRoot, "TimerMillies",         10);

        BOOL fPageFusion = FALSE;
        hrc = pMachine->COMGETTER(PageFusionEnabled)(&fPageFusion);                         H();
        InsertConfigInteger(pRoot, "PageFusionAllowed",    fPageFusion); /* boolean */

        /* Not necessary, but makes sure this setting ends up in the release log. */
        ULONG ulBalloonSize = 0;
        hrc = pMachine->COMGETTER(MemoryBalloonSize)(&ulBalloonSize);                       H();
        InsertConfigInteger(pRoot, "MemBalloonSize",       ulBalloonSize);

        /*
         * EM values (before CPUM as it may need to set IemExecutesAll).
         */
        PCFGMNODE pEM;
        InsertConfigNode(pRoot, "EM", &pEM);

        /* Triple fault behavior. */
        BOOL fTripleFaultReset = false;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_TripleFaultReset, &fTripleFaultReset); H();
        InsertConfigInteger(pEM, "TripleFaultReset", fTripleFaultReset);

        /*
         * CPUM values.
         */
        PCFGMNODE pCPUM;
        InsertConfigNode(pRoot, "CPUM", &pCPUM);
        PCFGMNODE pIsaExts;
        InsertConfigNode(pCPUM, "IsaExts", &pIsaExts);

        /* Host CPUID leaf overrides. */
        for (uint32_t iOrdinal = 0; iOrdinal < _4K; iOrdinal++)
        {
            ULONG uLeaf, uSubLeaf, uEax, uEbx, uEcx, uEdx;
            hrc = pMachine->GetCPUIDLeafByOrdinal(iOrdinal, &uLeaf, &uSubLeaf, &uEax, &uEbx, &uEcx, &uEdx);
            if (hrc == E_INVALIDARG)
                break;
            H();
            PCFGMNODE pLeaf;
            InsertConfigNode(pCPUM, Utf8StrFmt("HostCPUID/%RX32", uLeaf).c_str(), &pLeaf);
            /** @todo Figure out how to tell the VMM about uSubLeaf   */
            InsertConfigInteger(pLeaf, "eax", uEax);
            InsertConfigInteger(pLeaf, "ebx", uEbx);
            InsertConfigInteger(pLeaf, "ecx", uEcx);
            InsertConfigInteger(pLeaf, "edx", uEdx);
        }

        /* We must limit CPUID count for Windows NT 4, as otherwise it stops
        with error 0x3e (MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED). */
        if (osTypeId == "WindowsNT4")
        {
            LogRel(("Limiting CPUID leaf count for NT4 guests\n"));
            InsertConfigInteger(pCPUM, "NT4LeafLimit", true);
        }

        if (fOsXGuest)
        {
            /* Expose extended MWAIT features to Mac OS X guests. */
            LogRel(("Using MWAIT extensions\n"));
            InsertConfigInteger(pIsaExts, "MWaitExtensions", true);

            /* Fake the CPU family/model so the guest works.  This is partly
               because older mac releases really doesn't work on newer cpus,
               and partly because mac os x expects more from systems with newer
               cpus (MSRs, power features, whatever). */
            uint32_t uMaxIntelFamilyModelStep = UINT32_MAX;
            if (   osTypeId == "MacOS"
                || osTypeId == "MacOS_64")
                uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 0); /* Penryn / X5482. */
            else if (   osTypeId == "MacOS106"
                     || osTypeId == "MacOS106_64")
                uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 0); /* Penryn / X5482 */
            else if (   osTypeId == "MacOS107"
                     || osTypeId == "MacOS107_64")
                uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 0); /* Penryn / X5482 */ /** @todo figure out
                                                                                what is required here. */
            else if (   osTypeId == "MacOS108"
                     || osTypeId == "MacOS108_64")
                uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 0); /* Penryn / X5482 */ /** @todo figure out
                                                                                what is required here. */
            else if (   osTypeId == "MacOS109"
                     || osTypeId == "MacOS109_64")
                uMaxIntelFamilyModelStep = RT_MAKE_U32_FROM_U8(1, 23, 6, 0); /* Penryn / X5482 */ /** @todo figure
                                                                                out what is required here. */
            if (uMaxIntelFamilyModelStep != UINT32_MAX)
                InsertConfigInteger(pCPUM, "MaxIntelFamilyModelStep", uMaxIntelFamilyModelStep);
        }

        /* CPU Portability level, */
        ULONG uCpuIdPortabilityLevel = 0;
        hrc = pMachine->COMGETTER(CPUIDPortabilityLevel)(&uCpuIdPortabilityLevel);          H();
        InsertConfigInteger(pCPUM, "PortableCpuIdLevel", uCpuIdPortabilityLevel);

        /* Physical Address Extension (PAE) */
        BOOL fEnablePAE = false;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_PAE, &fEnablePAE);                   H();
        fEnablePAE |= fIsGuest64Bit;
        InsertConfigInteger(pRoot, "EnablePAE", fEnablePAE);

        /* 64-bit guests (long mode) */
        InsertConfigInteger(pCPUM, "Enable64bit", fIsGuest64Bit);

        /* APIC/X2APIC configuration */
        BOOL fEnableAPIC = true;
        BOOL fEnableX2APIC = true;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_APIC, &fEnableAPIC);                 H();
        hrc = pMachine->GetCPUProperty(CPUPropertyType_X2APIC, &fEnableX2APIC);             H();
        if (fEnableX2APIC)
            Assert(fEnableAPIC);

        /* CPUM profile name. */
        InsertConfigString(pCPUM, "GuestCpuName", bstrCpuProfile);

        /*
         * Temporary(?) hack to make sure we emulate the ancient 16-bit CPUs
         * correctly.   There are way too many #UDs we'll miss using VT-x,
         * raw-mode or qemu for the 186 and 286, while we'll get undefined opcodes
         * dead wrong on 8086 (see http://www.os2museum.com/wp/undocumented-8086-opcodes/).
         */
        if (   bstrCpuProfile.equals("Intel 80386") /* just for now */
            || bstrCpuProfile.equals("Intel 80286")
            || bstrCpuProfile.equals("Intel 80186")
            || bstrCpuProfile.equals("Nec V20")
            || bstrCpuProfile.equals("Intel 8086") )
        {
            InsertConfigInteger(pEM, "IemExecutesAll", true);
            if (!bstrCpuProfile.equals("Intel 80386"))
            {
                fEnableAPIC = false;
                fIOAPIC     = false;
            }
            fEnableX2APIC = false;
        }

        /* Adjust firmware APIC handling to stay within the VCPU limits. */
        if (uFwAPIC == 2 && !fEnableX2APIC)
        {
            if (fEnableAPIC)
                uFwAPIC = 1;
            else
                uFwAPIC = 0;
            LogRel(("Limiting the firmware APIC level from x2APIC to %s\n", fEnableAPIC ? "APIC" : "Disabled"));
        }
        else if (uFwAPIC == 1 && !fEnableAPIC)
        {
            uFwAPIC = 0;
            LogRel(("Limiting the firmware APIC level from APIC to Disabled\n"));
        }

        /* Speculation Control. */
        BOOL fSpecCtrl = FALSE;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_SpecCtrl, &fSpecCtrl);      H();
        InsertConfigInteger(pCPUM, "SpecCtrl", fSpecCtrl);

        /* Nested VT-x / AMD-V. */
        BOOL fNestedHWVirt = FALSE;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_HWVirt, &fNestedHWVirt);      H();
        InsertConfigInteger(pCPUM, "NestedHWVirt", fNestedHWVirt ? true : false);

        /*
         * Hardware virtualization extensions.
         */
        /* Sanitize valid/useful APIC combinations, see @bugref{8868}. */
        if (!fEnableAPIC)
        {
            if (fIsGuest64Bit)
                return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                             N_("Cannot disable the APIC for a 64-bit guest."));
            if (cCpus > 1)
                return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                             N_("Cannot disable the APIC for an SMP guest."));
            if (fIOAPIC)
                return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                             N_("Cannot disable the APIC when the I/O APIC is present."));
        }

        BOOL fHMEnabled;
        hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_Enabled, &fHMEnabled);     H();
        if (cCpus > 1 && !fHMEnabled)
        {
            LogRel(("Forced fHMEnabled to TRUE by SMP guest.\n"));
            fHMEnabled = TRUE;
        }

        BOOL fHMForced;
        fHMEnabled = fHMForced = TRUE;
        LogRel(("fHMForced=true - No raw-mode support in this build!\n"));
        if (!fHMForced) /* No need to query if already forced above. */
        {
            hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_Force, &fHMForced); H();
            if (fHMForced)
                LogRel(("fHMForced=true - HWVirtExPropertyType_Force\n"));
        }
        InsertConfigInteger(pRoot, "HMEnabled", fHMEnabled);

        /* /HM/xyz */
        PCFGMNODE pHM;
        InsertConfigNode(pRoot, "HM", &pHM);
        InsertConfigInteger(pHM, "HMForced", fHMForced);
        if (fHMEnabled)
        {
            /* Indicate whether 64-bit guests are supported or not. */
            InsertConfigInteger(pHM, "64bitEnabled", fIsGuest64Bit);

            /** @todo Not exactly pretty to check strings; VBOXOSTYPE would be better,
                but that requires quite a bit of API change in Main. */
            if (    fIOAPIC
                &&  (   osTypeId == "WindowsNT4"
                     || osTypeId == "Windows2000"
                     || osTypeId == "WindowsXP"
                     || osTypeId == "Windows2003"))
            {
                /* Only allow TPR patching for NT, Win2k, XP and Windows Server 2003. (32 bits mode)
                 * We may want to consider adding more guest OSes (Solaris) later on.
                 */
                InsertConfigInteger(pHM, "TPRPatchingEnabled", 1);
            }
        }

        /* HWVirtEx exclusive mode */
        BOOL fHMExclusive = true;
        hrc = systemProperties->COMGETTER(ExclusiveHwVirt)(&fHMExclusive);                  H();
        InsertConfigInteger(pHM, "Exclusive", fHMExclusive);

        /* Nested paging (VT-x/AMD-V) */
        BOOL fEnableNestedPaging = false;
        hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_NestedPaging, &fEnableNestedPaging); H();
        InsertConfigInteger(pHM, "EnableNestedPaging", fEnableNestedPaging);

        /* Large pages; requires nested paging */
        BOOL fEnableLargePages = false;
        hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_LargePages, &fEnableLargePages); H();
        InsertConfigInteger(pHM, "EnableLargePages", fEnableLargePages);

        /* VPID (VT-x) */
        BOOL fEnableVPID = false;
        hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_VPID, &fEnableVPID);       H();
        InsertConfigInteger(pHM, "EnableVPID", fEnableVPID);

        /* Unrestricted execution aka UX (VT-x) */
        BOOL fEnableUX = false;
        hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_UnrestrictedExecution, &fEnableUX); H();
        InsertConfigInteger(pHM, "EnableUX", fEnableUX);

        /* Virtualized VMSAVE/VMLOAD (AMD-V) */
        BOOL fVirtVmsaveVmload = true;
        hrc = host->GetProcessorFeature(ProcessorFeature_VirtVmsaveVmload, &fVirtVmsaveVmload);     H();
        InsertConfigInteger(pHM, "SvmVirtVmsaveVmload", fVirtVmsaveVmload);

        /* Indirect branch prediction boundraries. */
        BOOL fIBPBOnVMExit = false;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_IBPBOnVMExit, &fIBPBOnVMExit); H();
        InsertConfigInteger(pHM, "IBPBOnVMExit", fIBPBOnVMExit);

        BOOL fIBPBOnVMEntry = false;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_IBPBOnVMEntry, &fIBPBOnVMEntry); H();
        InsertConfigInteger(pHM, "IBPBOnVMEntry", fIBPBOnVMEntry);

        BOOL fSpecCtrlByHost = false;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_SpecCtrlByHost, &fSpecCtrlByHost); H();
        InsertConfigInteger(pHM, "SpecCtrlByHost", fSpecCtrlByHost);

        BOOL fL1DFlushOnSched = true;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_L1DFlushOnEMTScheduling, &fL1DFlushOnSched); H();
        InsertConfigInteger(pHM, "L1DFlushOnSched", fL1DFlushOnSched);

        BOOL fL1DFlushOnVMEntry = false;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_L1DFlushOnVMEntry, &fL1DFlushOnVMEntry); H();
        InsertConfigInteger(pHM, "L1DFlushOnVMEntry", fL1DFlushOnVMEntry);

        BOOL fMDSClearOnSched = true;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_MDSClearOnEMTScheduling, &fMDSClearOnSched); H();
        InsertConfigInteger(pHM, "MDSClearOnSched", fMDSClearOnSched);

        BOOL fMDSClearOnVMEntry = false;
        hrc = pMachine->GetCPUProperty(CPUPropertyType_MDSClearOnVMEntry, &fMDSClearOnVMEntry); H();
        InsertConfigInteger(pHM, "MDSClearOnVMEntry", fMDSClearOnVMEntry);

        /* Reset overwrite. */
        mfTurnResetIntoPowerOff = GetExtraDataBoth(virtualBox, pMachine,
                                                   "VBoxInternal2/TurnResetIntoPowerOff", &strTmp)->equals("1");
        if (mfTurnResetIntoPowerOff)
            InsertConfigInteger(pRoot, "PowerOffInsteadOfReset", 1);

        /* Use NEM rather than HM. */
        BOOL fUseNativeApi = false;
        hrc = pMachine->GetHWVirtExProperty(HWVirtExPropertyType_UseNativeApi, &fUseNativeApi); H();
        InsertConfigInteger(pHM, "UseNEMInstead", fUseNativeApi);

        /* Enable workaround for missing TLB flush for OS/2 guests, see ticketref:20625. */
        if (osTypeId.startsWith("OS2"))
            InsertConfigInteger(pHM, "MissingOS2TlbFlushWorkaround", 1);

        /*
         * NEM
         */
        PCFGMNODE pNEM;
        InsertConfigNode(pRoot, "NEM", &pNEM);
        InsertConfigInteger(pNEM, "Allow64BitGuests", fIsGuest64Bit);

        /*
         * Paravirt. provider.
         */
        PCFGMNODE pParavirtNode;
        InsertConfigNode(pRoot, "GIM", &pParavirtNode);
        const char *pcszParavirtProvider;
        bool fGimDeviceNeeded = true;
        switch (enmParavirtProvider)
        {
            case ParavirtProvider_None:
                pcszParavirtProvider = "None";
                fGimDeviceNeeded = false;
                break;

            case ParavirtProvider_Minimal:
                pcszParavirtProvider = "Minimal";
                break;

            case ParavirtProvider_HyperV:
                pcszParavirtProvider = "HyperV";
                break;

            case ParavirtProvider_KVM:
                pcszParavirtProvider = "KVM";
                break;

            default:
                AssertMsgFailed(("Invalid enmParavirtProvider=%d\n", enmParavirtProvider));
                return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS, N_("Invalid paravirt. provider '%d'"),
                                             enmParavirtProvider);
        }
        InsertConfigString(pParavirtNode, "Provider", pcszParavirtProvider);

        /*
         * Parse paravirt. debug options.
         */
        bool         fGimDebug          = false;
        com::Utf8Str strGimDebugAddress = "127.0.0.1";
        uint32_t     uGimDebugPort      = 50000;
        if (strParavirtDebug.isNotEmpty())
        {
            /* Hyper-V debug options. */
            if (enmParavirtProvider == ParavirtProvider_HyperV)
            {
                bool         fGimHvDebug = false;
                com::Utf8Str strGimHvVendor;
                bool         fGimHvVsIf = false;
                bool         fGimHvHypercallIf = false;

                size_t       uPos = 0;
                com::Utf8Str strDebugOptions = strParavirtDebug;
                com::Utf8Str strKey;
                com::Utf8Str strVal;
                while ((uPos = strDebugOptions.parseKeyValue(strKey, strVal, uPos)) != com::Utf8Str::npos)
                {
                    if (strKey == "enabled")
                    {
                        if (strVal.toUInt32() == 1)
                        {
                            /* Apply defaults.
                               The defaults are documented in the user manual,
                               changes need to be reflected accordingly. */
                            fGimHvDebug       = true;
                            strGimHvVendor    = "Microsoft Hv";
                            fGimHvVsIf        = true;
                            fGimHvHypercallIf = false;
                        }
                        /* else: ignore, i.e. don't assert below with 'enabled=0'. */
                    }
                    else if (strKey == "address")
                        strGimDebugAddress = strVal;
                    else if (strKey == "port")
                        uGimDebugPort = strVal.toUInt32();
                    else if (strKey == "vendor")
                        strGimHvVendor = strVal;
                    else if (strKey == "vsinterface")
                        fGimHvVsIf = RT_BOOL(strVal.toUInt32());
                    else if (strKey == "hypercallinterface")
                        fGimHvHypercallIf = RT_BOOL(strVal.toUInt32());
                    else
                    {
                        AssertMsgFailed(("Unrecognized Hyper-V debug option '%s'\n", strKey.c_str()));
                        return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                                     N_("Unrecognized Hyper-V debug option '%s' in '%s'"), strKey.c_str(),
                                                     strDebugOptions.c_str());
                    }
                }

                /* Update HyperV CFGM node with active debug options. */
                if (fGimHvDebug)
                {
                    PCFGMNODE pHvNode;
                    InsertConfigNode(pParavirtNode, "HyperV", &pHvNode);
                    InsertConfigString(pHvNode,  "VendorID", strGimHvVendor);
                    InsertConfigInteger(pHvNode, "VSInterface", fGimHvVsIf ? 1 : 0);
                    InsertConfigInteger(pHvNode, "HypercallDebugInterface", fGimHvHypercallIf ? 1 : 0);
                    fGimDebug = true;
                }
            }
        }

        /*
         * Guest Compatibility Manager.
         */
        PCFGMNODE   pGcmNode;
        uint32_t    u32FixerSet = 0;
        InsertConfigNode(pRoot, "GCM", &pGcmNode);
        /* OS/2 and Win9x guests can run DOS apps so they get
         * the DOS specific fixes as well.
         */
        if (fOs2Guest)
            u32FixerSet = GCMFIXER_DBZ_DOS | GCMFIXER_DBZ_OS2;
        else if (fW9xGuest)
            u32FixerSet = GCMFIXER_DBZ_DOS | GCMFIXER_DBZ_WIN9X;
        else if (fDosGuest)
            u32FixerSet = GCMFIXER_DBZ_DOS;
        InsertConfigInteger(pGcmNode, "FixerSet", u32FixerSet);


        /*
         * MM values.
         */
        PCFGMNODE pMM;
        InsertConfigNode(pRoot, "MM", &pMM);
        InsertConfigInteger(pMM, "CanUseLargerHeap", chipsetType == ChipsetType_ICH9);

        /*
         * PDM config.
         *  Load drivers in VBoxC.[so|dll]
         */
        PCFGMNODE pPDM;
        PCFGMNODE pNode;
        PCFGMNODE pMod;
        InsertConfigNode(pRoot,    "PDM", &pPDM);
        InsertConfigNode(pPDM,     "Devices", &pNode);
        InsertConfigNode(pPDM,     "Drivers", &pNode);
        InsertConfigNode(pNode,    "VBoxC", &pMod);
#ifdef VBOX_WITH_XPCOM
        // VBoxC is located in the components subdirectory
        char szPathVBoxC[RTPATH_MAX];
        vrc = RTPathAppPrivateArch(szPathVBoxC, RTPATH_MAX - sizeof("/components/VBoxC"));  AssertRC(vrc);
        strcat(szPathVBoxC, "/components/VBoxC");
        InsertConfigString(pMod,   "Path",  szPathVBoxC);
#else
        InsertConfigString(pMod,   "Path",  "VBoxC");
#endif


        /*
         * Block cache settings.
         */
        PCFGMNODE pPDMBlkCache;
        InsertConfigNode(pPDM, "BlkCache", &pPDMBlkCache);

        /* I/O cache size */
        ULONG ioCacheSize = 5;
        hrc = pMachine->COMGETTER(IOCacheSize)(&ioCacheSize);                               H();
        InsertConfigInteger(pPDMBlkCache, "CacheSize", ioCacheSize * _1M);

        /*
         * Bandwidth groups.
         */
        ComPtr<IBandwidthControl> bwCtrl;

        hrc = pMachine->COMGETTER(BandwidthControl)(bwCtrl.asOutParam());                   H();

        com::SafeIfaceArray<IBandwidthGroup> bwGroups;
        hrc = bwCtrl->GetAllBandwidthGroups(ComSafeArrayAsOutParam(bwGroups));              H();

        PCFGMNODE pAc;
        InsertConfigNode(pPDM, "AsyncCompletion", &pAc);
        PCFGMNODE pAcFile;
        InsertConfigNode(pAc,  "File", &pAcFile);
        PCFGMNODE pAcFileBwGroups;
        InsertConfigNode(pAcFile,  "BwGroups", &pAcFileBwGroups);
#ifdef VBOX_WITH_NETSHAPER
        PCFGMNODE pNetworkShaper;
        InsertConfigNode(pPDM, "NetworkShaper",  &pNetworkShaper);
        PCFGMNODE pNetworkBwGroups;
        InsertConfigNode(pNetworkShaper, "BwGroups", &pNetworkBwGroups);
#endif /* VBOX_WITH_NETSHAPER */

        for (size_t i = 0; i < bwGroups.size(); i++)
        {
            Bstr strName;
            hrc = bwGroups[i]->COMGETTER(Name)(strName.asOutParam());                       H();
            if (strName.isEmpty())
                return pVMM->pfnVMR3SetError(pUVM, VERR_CFGM_NO_NODE, RT_SRC_POS, N_("No bandwidth group name specified"));

            BandwidthGroupType_T enmType = BandwidthGroupType_Null;
            hrc = bwGroups[i]->COMGETTER(Type)(&enmType);                                   H();
            LONG64 cMaxBytesPerSec = 0;
            hrc = bwGroups[i]->COMGETTER(MaxBytesPerSec)(&cMaxBytesPerSec);                 H();

            if (enmType == BandwidthGroupType_Disk)
            {
                PCFGMNODE pBwGroup;
                InsertConfigNode(pAcFileBwGroups, Utf8Str(strName).c_str(), &pBwGroup);
                InsertConfigInteger(pBwGroup, "Max", cMaxBytesPerSec);
                InsertConfigInteger(pBwGroup, "Start", cMaxBytesPerSec);
                InsertConfigInteger(pBwGroup, "Step", 0);
            }
#ifdef VBOX_WITH_NETSHAPER
            else if (enmType == BandwidthGroupType_Network)
            {
                /* Network bandwidth groups. */
                PCFGMNODE pBwGroup;
                InsertConfigNode(pNetworkBwGroups, Utf8Str(strName).c_str(), &pBwGroup);
                InsertConfigInteger(pBwGroup, "Max", cMaxBytesPerSec);
            }
#endif /* VBOX_WITH_NETSHAPER */
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
        PCFGMNODE pBiosCfg = NULL;      /* /Devices/pcbios/0/Config/ */
        PCFGMNODE pNetBootCfg = NULL;   /* /Devices/pcbios/0/Config/NetBoot/ */

        InsertConfigNode(pRoot, "Devices", &pDevices);

        /*
         * GIM Device
         */
        if (fGimDeviceNeeded)
        {
            InsertConfigNode(pDevices, "GIMDev", &pDev);
            InsertConfigNode(pDev,     "0", &pInst);
            InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
            //InsertConfigNode(pInst,    "Config", &pCfg);

            if (fGimDebug)
            {
                InsertConfigNode(pInst,     "LUN#998", &pLunL0);
                InsertConfigString(pLunL0,  "Driver", "UDP");
                InsertConfigNode(pLunL0,    "Config", &pLunL1);
                InsertConfigString(pLunL1,  "ServerAddress", strGimDebugAddress);
                InsertConfigInteger(pLunL1, "ServerPort", uGimDebugPort);
            }
        }

        /*
         * PC Arch.
         */
        InsertConfigNode(pDevices, "pcarch", &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
        InsertConfigNode(pInst,    "Config", &pCfg);

        /*
         * The time offset
         */
        LONG64 timeOffset;
        hrc = biosSettings->COMGETTER(TimeOffset)(&timeOffset);                             H();
        PCFGMNODE pTMNode;
        InsertConfigNode(pRoot, "TM", &pTMNode);
        InsertConfigInteger(pTMNode, "UTCOffset", timeOffset * 1000000);

        /*
         * DMA
         */
        InsertConfigNode(pDevices, "8237A", &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigInteger(pInst, "Trusted", 1); /* boolean */

        /*
         * PCI buses.
         */
        uint32_t uIocPCIAddress, uHbcPCIAddress;
        switch (chipsetType)
        {
            default:
                AssertFailed();
                RT_FALL_THRU();
            case ChipsetType_PIIX3:
                /* Create the base for adding bridges on demand */
                InsertConfigNode(pDevices, "pcibridge", NULL);

                InsertConfigNode(pDevices, "pci", &pDev);
                uHbcPCIAddress = (0x0 << 16) | 0;
                uIocPCIAddress = (0x1 << 16) | 0; // ISA controller
                break;
            case ChipsetType_ICH9:
                /* Create the base for adding bridges on demand */
                InsertConfigNode(pDevices, "ich9pcibridge", NULL);

                InsertConfigNode(pDevices, "ich9pci", &pDev);
                uHbcPCIAddress = (0x1e << 16) | 0;
                uIocPCIAddress = (0x1f << 16) | 0; // LPC controller
                break;
        }
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
        InsertConfigNode(pInst,    "Config", &pCfg);
        InsertConfigInteger(pCfg, "IOAPIC", fIOAPIC);
        if (chipsetType == ChipsetType_ICH9)
        {
            /* Provide MCFG info */
            InsertConfigInteger(pCfg,  "McfgBase",   uMcfgBase);
            InsertConfigInteger(pCfg,  "McfgLength", cbMcfgLength);

#ifdef VBOX_WITH_PCI_PASSTHROUGH
            /* Add PCI passthrough devices */
            hrc = i_attachRawPCIDevices(pUVM, pBusMgr, pDevices);                           H();
#endif

            if (enmIommuType == IommuType_AMD)
            {
                /* AMD IOMMU. */
                InsertConfigNode(pDevices, "iommu-amd", &pDev);
                InsertConfigNode(pDev,     "0", &pInst);
                InsertConfigInteger(pInst, "Trusted",   1); /* boolean */
                InsertConfigNode(pInst,    "Config", &pCfg);
                hrc = pBusMgr->assignPCIDevice("iommu-amd", pInst);                         H();

                /* The AMD IOMMU device needs to know which PCI slot it's in, see @bugref{9654#c104}. */
                {
                    PCIBusAddress Address;
                    if (pBusMgr->findPCIAddress("iommu-amd", 0, Address))
                    {
                        uint32_t const u32IommuAddress = (Address.miDevice << 16) | Address.miFn;
                        InsertConfigInteger(pCfg, "PCIAddress", u32IommuAddress);
                    }
                    else
                        return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                                     N_("Failed to find PCI address of the assigned IOMMU device!"));
                }

                PCIBusAddress PCIAddr = PCIBusAddress((int32_t)uIoApicPciAddress);
                hrc = pBusMgr->assignPCIDevice("sb-ioapic", NULL /* pCfg */, PCIAddr, true /*fGuestAddressRequired*/);  H();
            }
            else if (enmIommuType == IommuType_Intel)
            {
                /* Intel IOMMU. */
                InsertConfigNode(pDevices, "iommu-intel", &pDev);
                InsertConfigNode(pDev,     "0", &pInst);
                InsertConfigInteger(pInst, "Trusted",   1); /* boolean */
                InsertConfigNode(pInst,    "Config", &pCfg);
                hrc = pBusMgr->assignPCIDevice("iommu-intel", pInst);                       H();

                PCIBusAddress PCIAddr = PCIBusAddress((int32_t)uIoApicPciAddress);
                hrc = pBusMgr->assignPCIDevice("sb-ioapic", NULL /* pCfg */, PCIAddr, true /*fGuestAddressRequired*/);  H();
            }
        }

        /*
         * Enable the following devices: HPET, SMC and LPC on MacOS X guests or on ICH9 chipset
         */

        /*
         * High Precision Event Timer (HPET)
         */
        BOOL fHPETEnabled;
        /* Other guests may wish to use HPET too, but MacOS X not functional without it */
        hrc = pMachine->COMGETTER(HPETEnabled)(&fHPETEnabled);                              H();
        /* so always enable HPET in extended profile */
        fHPETEnabled |= fOsXGuest;
        /* HPET is always present on ICH9 */
        fHPETEnabled |= (chipsetType == ChipsetType_ICH9);
        if (fHPETEnabled)
        {
            InsertConfigNode(pDevices, "hpet", &pDev);
            InsertConfigNode(pDev,     "0", &pInst);
            InsertConfigInteger(pInst, "Trusted",   1); /* boolean */
            InsertConfigNode(pInst,    "Config", &pCfg);
            InsertConfigInteger(pCfg,  "ICH9", (chipsetType == ChipsetType_ICH9) ? 1 : 0);  /* boolean */
        }

        /*
         * System Management Controller (SMC)
         */
        BOOL fSmcEnabled;
        fSmcEnabled = fOsXGuest;
        if (fSmcEnabled)
        {
            InsertConfigNode(pDevices, "smc", &pDev);
            InsertConfigNode(pDev,     "0", &pInst);
            InsertConfigInteger(pInst, "Trusted",   1); /* boolean */
            InsertConfigNode(pInst,    "Config", &pCfg);

            bool fGetKeyFromRealSMC;
            Utf8Str strKey;
            vrc = getSmcDeviceKey(virtualBox, pMachine, &strKey, &fGetKeyFromRealSMC);
            AssertRCReturn(vrc, vrc);

            if (!fGetKeyFromRealSMC)
                InsertConfigString(pCfg,   "DeviceKey", strKey);
            InsertConfigInteger(pCfg,  "GetKeyFromRealSMC", fGetKeyFromRealSMC);
        }

        /*
         * Low Pin Count (LPC) bus
         */
        BOOL fLpcEnabled;
        /** @todo implement appropriate getter */
        fLpcEnabled = fOsXGuest || (chipsetType == ChipsetType_ICH9);
        if (fLpcEnabled)
        {
            InsertConfigNode(pDevices, "lpc", &pDev);
            InsertConfigNode(pDev,     "0", &pInst);
            hrc = pBusMgr->assignPCIDevice("lpc", pInst);                                   H();
            InsertConfigInteger(pInst, "Trusted",   1); /* boolean */
        }

        BOOL fShowRtc;
        fShowRtc = fOsXGuest || (chipsetType == ChipsetType_ICH9);

        /*
         * PS/2 keyboard & mouse.
         */
        InsertConfigNode(pDevices, "pckbd", &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
        InsertConfigNode(pInst,    "Config", &pCfg);

        KeyboardHIDType_T aKbdHID;
        hrc = pMachine->COMGETTER(KeyboardHIDType)(&aKbdHID);                               H();
        if (aKbdHID != KeyboardHIDType_None)
        {
            InsertConfigNode(pInst,    "LUN#0", &pLunL0);
            InsertConfigString(pLunL0, "Driver",               "KeyboardQueue");
            InsertConfigNode(pLunL0,   "Config", &pCfg);
            InsertConfigInteger(pCfg,  "QueueSize",            64);

            InsertConfigNode(pLunL0,   "AttachedDriver", &pLunL1);
            InsertConfigString(pLunL1, "Driver",               "MainKeyboard");
        }

        PointingHIDType_T aPointingHID;
        hrc = pMachine->COMGETTER(PointingHIDType)(&aPointingHID);                          H();
        if (aPointingHID != PointingHIDType_None)
        {
            InsertConfigNode(pInst,    "LUN#1", &pLunL0);
            InsertConfigString(pLunL0, "Driver",               "MouseQueue");
            InsertConfigNode(pLunL0,   "Config", &pCfg);
            InsertConfigInteger(pCfg, "QueueSize",            128);

            InsertConfigNode(pLunL0,   "AttachedDriver", &pLunL1);
            InsertConfigString(pLunL1, "Driver",               "MainMouse");
        }

        /*
         * i8254 Programmable Interval Timer And Dummy Speaker
         */
        InsertConfigNode(pDevices, "i8254", &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigNode(pInst,    "Config", &pCfg);
#ifdef DEBUG
        InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
#endif

        /*
         * i8259 Programmable Interrupt Controller.
         */
        InsertConfigNode(pDevices, "i8259", &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
        InsertConfigNode(pInst,    "Config", &pCfg);

        /*
         * Advanced Programmable Interrupt Controller.
         * SMP: Each CPU has a LAPIC, but we have a single device representing all LAPICs states,
         *      thus only single insert
         */
        if (fEnableAPIC)
        {
            InsertConfigNode(pDevices, "apic", &pDev);
            InsertConfigNode(pDev, "0", &pInst);
            InsertConfigInteger(pInst, "Trusted",          1); /* boolean */
            InsertConfigNode(pInst,    "Config", &pCfg);
            InsertConfigInteger(pCfg,  "IOAPIC", fIOAPIC);
            PDMAPICMODE enmAPICMode = PDMAPICMODE_APIC;
            if (fEnableX2APIC)
                enmAPICMode = PDMAPICMODE_X2APIC;
            else if (!fEnableAPIC)
                enmAPICMode = PDMAPICMODE_NONE;
            InsertConfigInteger(pCfg,  "Mode", enmAPICMode);
            InsertConfigInteger(pCfg,  "NumCPUs", cCpus);

            if (fIOAPIC)
            {
                /*
                 * I/O Advanced Programmable Interrupt Controller.
                 */
                InsertConfigNode(pDevices, "ioapic", &pDev);
                InsertConfigNode(pDev,     "0", &pInst);
                InsertConfigInteger(pInst, "Trusted",      1); /* boolean */
                InsertConfigNode(pInst,    "Config", &pCfg);
                InsertConfigInteger(pCfg,  "NumCPUs", cCpus);
                if (enmIommuType == IommuType_AMD)
                    InsertConfigInteger(pCfg, "PCIAddress", uIoApicPciAddress);
                else if (enmIommuType == IommuType_Intel)
                {
                    InsertConfigString(pCfg, "ChipType", "DMAR");
                    InsertConfigInteger(pCfg, "PCIAddress", uIoApicPciAddress);
                }
            }
        }

        /*
         * RTC MC146818.
         */
        InsertConfigNode(pDevices, "mc146818", &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigNode(pInst,    "Config", &pCfg);
        BOOL fRTCUseUTC;
        hrc = pMachine->COMGETTER(RTCUseUTC)(&fRTCUseUTC);                                  H();
        InsertConfigInteger(pCfg,  "UseUTC", fRTCUseUTC ? 1 : 0);

        /*
         * VGA.
         */
        ComPtr<IGraphicsAdapter> pGraphicsAdapter;
        hrc = pMachine->COMGETTER(GraphicsAdapter)(pGraphicsAdapter.asOutParam());           H();
        GraphicsControllerType_T enmGraphicsController;
        hrc = pGraphicsAdapter->COMGETTER(GraphicsControllerType)(&enmGraphicsController);          H();
        switch (enmGraphicsController)
        {
            case GraphicsControllerType_Null:
                break;
#ifdef VBOX_WITH_VMSVGA
            case GraphicsControllerType_VMSVGA:
                InsertConfigInteger(pHM, "LovelyMesaDrvWorkaround", 1); /* hits someone else logging backdoor. */
                InsertConfigInteger(pNEM, "LovelyMesaDrvWorkaround", 1); /* hits someone else logging backdoor. */
                RT_FALL_THROUGH();
            case GraphicsControllerType_VBoxSVGA:
#endif
            case GraphicsControllerType_VBoxVGA:
                vrc = i_configGraphicsController(pDevices, enmGraphicsController, pBusMgr, pMachine, pGraphicsAdapter, biosSettings,
                                                 RT_BOOL(fHMEnabled));
                if (FAILED(vrc))
                    return vrc;
                break;
            default:
                AssertMsgFailed(("Invalid graphicsController=%d\n", enmGraphicsController));
                return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                             N_("Invalid graphics controller type '%d'"), enmGraphicsController);
        }

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
            InsertConfigNode(pDevices, "pcbios", &pDev);
            InsertConfigNode(pDev,     "0", &pInst);
            InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
            InsertConfigNode(pInst,    "Config", &pBiosCfg);
            InsertConfigInteger(pBiosCfg,  "NumCPUs",              cCpus);
            InsertConfigString(pBiosCfg,   "HardDiskDevice",       "piix3ide");
            InsertConfigString(pBiosCfg,   "FloppyDevice",         "i82078");
            InsertConfigInteger(pBiosCfg,  "IOAPIC",               fIOAPIC);
            InsertConfigInteger(pBiosCfg,  "APIC",                 uFwAPIC);
            BOOL fPXEDebug;
            hrc = biosSettings->COMGETTER(PXEDebugEnabled)(&fPXEDebug);                     H();
            InsertConfigInteger(pBiosCfg,  "PXEDebug",             fPXEDebug);
            InsertConfigBytes(pBiosCfg,    "UUID", &HardwareUuid,sizeof(HardwareUuid));
            BOOL fUuidLe;
            hrc = biosSettings->COMGETTER(SMBIOSUuidLittleEndian)(&fUuidLe);                H();
            InsertConfigInteger(pBiosCfg,  "UuidLe",               fUuidLe);
            InsertConfigNode(pBiosCfg,     "NetBoot", &pNetBootCfg);
            InsertConfigInteger(pBiosCfg,  "McfgBase",   uMcfgBase);
            InsertConfigInteger(pBiosCfg,  "McfgLength", cbMcfgLength);

            AssertMsgReturn(SchemaDefs::MaxBootPosition <= 9, ("Too many boot devices %d\n", SchemaDefs::MaxBootPosition),
                            VERR_INVALID_PARAMETER);

            for (ULONG pos = 1; pos <= SchemaDefs::MaxBootPosition; ++pos)
            {
                DeviceType_T enmBootDevice;
                hrc = pMachine->GetBootOrder(pos, &enmBootDevice);                          H();

                char szParamName[] = "BootDeviceX";
                szParamName[sizeof(szParamName) - 2] = (char)(pos - 1 + '0');

                const char *pszBootDevice;
                switch (enmBootDevice)
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
                        AssertMsgFailed(("Invalid enmBootDevice=%d\n", enmBootDevice));
                        return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                                     N_("Invalid boot device '%d'"), enmBootDevice);
                }
                InsertConfigString(pBiosCfg, szParamName, pszBootDevice);
            }

            /** @todo @bugref{7145}: We might want to enable this by default for new VMs. For now,
             *        this is required for Windows 2012 guests. */
            if (osTypeId == "Windows2012_64")
                InsertConfigInteger(pBiosCfg, "DmiExposeMemoryTable", 1); /* boolean */
        }
        else
        {
            /* Autodetect firmware type, basing on guest type */
            if (eFwType == FirmwareType_EFI)
                eFwType = fIsGuest64Bit ? FirmwareType_EFI64 : FirmwareType_EFI32;
            bool const f64BitEntry = eFwType == FirmwareType_EFI64;

            Assert(eFwType == FirmwareType_EFI64 || eFwType == FirmwareType_EFI32 || eFwType == FirmwareType_EFIDUAL);
#ifdef VBOX_WITH_EFI_IN_DD2
            const char *pszEfiRomFile = eFwType == FirmwareType_EFIDUAL ? "VBoxEFIDual.fd"
                                      : eFwType == FirmwareType_EFI32   ? "VBoxEFI32.fd"
                                      :                                   "VBoxEFI64.fd";
#else
            Utf8Str efiRomFile;
            vrc = findEfiRom(virtualBox, eFwType, &efiRomFile);
            AssertRCReturn(vrc, vrc);
            const char *pszEfiRomFile = efiRomFile.c_str();
#endif

            /* Get boot args */
            Utf8Str bootArgs;
            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/EfiBootArgs", &bootArgs);

            /* Get device props */
            Utf8Str deviceProps;
            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/EfiDeviceProps", &deviceProps);

            /* Get NVRAM file name */
            Utf8Str strNvram = mptrNvramStore->i_getNonVolatileStorageFile();

            BOOL fUuidLe;
            hrc = biosSettings->COMGETTER(SMBIOSUuidLittleEndian)(&fUuidLe);                H();

            /* Get graphics mode settings */
            uint32_t u32GraphicsMode = UINT32_MAX;
            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/EfiGraphicsMode", &strTmp);
            if (strTmp.isEmpty())
                GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/EfiGopMode", &strTmp);
            if (!strTmp.isEmpty())
                u32GraphicsMode = strTmp.toUInt32();

            /* Get graphics resolution settings, with some sanity checking */
            Utf8Str strResolution;
            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/EfiGraphicsResolution", &strResolution);
            if (!strResolution.isEmpty())
            {
                size_t pos = strResolution.find("x");
                if (pos != strResolution.npos)
                {
                    Utf8Str strH, strV;
                    strH.assignEx(strResolution, 0, pos);
                    strV.assignEx(strResolution, pos+1, strResolution.length()-pos-1);
                    uint32_t u32H = strH.toUInt32();
                    uint32_t u32V = strV.toUInt32();
                    if (u32H == 0 || u32V == 0)
                        strResolution.setNull();
                }
                else
                    strResolution.setNull();
            }
            else
            {
                uint32_t u32H = 0;
                uint32_t u32V = 0;
                GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/EfiHorizontalResolution", &strTmp);
                if (strTmp.isEmpty())
                    GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/EfiUgaHorizontalResolution", &strTmp);
                if (!strTmp.isEmpty())
                    u32H = strTmp.toUInt32();

                GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/EfiVerticalResolution", &strTmp);
                if (strTmp.isEmpty())
                    GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/EfiUgaVerticalResolution", &strTmp);
                if (!strTmp.isEmpty())
                    u32V = strTmp.toUInt32();
                if (u32H != 0 && u32V != 0)
                    strResolution = Utf8StrFmt("%ux%u", u32H, u32V);
            }

            /*
             * EFI subtree.
             */
            InsertConfigNode(pDevices, "efi", &pDev);
            InsertConfigNode(pDev,     "0", &pInst);
            InsertConfigInteger(pInst, "Trusted", 1); /* boolean */
            InsertConfigNode(pInst,    "Config", &pCfg);
            InsertConfigInteger(pCfg,  "NumCPUs",     cCpus);
            InsertConfigInteger(pCfg,  "McfgBase",    uMcfgBase);
            InsertConfigInteger(pCfg,  "McfgLength",  cbMcfgLength);
            InsertConfigString(pCfg,   "EfiRom",      pszEfiRomFile);
            InsertConfigString(pCfg,   "BootArgs",    bootArgs);
            InsertConfigString(pCfg,   "DeviceProps", deviceProps);
            InsertConfigInteger(pCfg,  "IOAPIC",      fIOAPIC);
            InsertConfigInteger(pCfg,  "APIC",        uFwAPIC);
            InsertConfigBytes(pCfg,    "UUID", &HardwareUuid,sizeof(HardwareUuid));
            InsertConfigInteger(pCfg,  "UuidLe",      fUuidLe);
            InsertConfigInteger(pCfg,  "64BitEntry",  f64BitEntry); /* boolean */
            InsertConfigString(pCfg,   "NvramFile",   strNvram);
            if (u32GraphicsMode != UINT32_MAX)
                InsertConfigInteger(pCfg,  "GraphicsMode",  u32GraphicsMode);
            if (!strResolution.isEmpty())
                InsertConfigString(pCfg,   "GraphicsResolution", strResolution);

            /* For OS X guests we'll force passing host's DMI info to the guest */
            if (fOsXGuest)
            {
                InsertConfigInteger(pCfg, "DmiUseHostInfo", 1);
                InsertConfigInteger(pCfg, "DmiExposeMemoryTable", 1);
            }

            /* Attach the NVRAM storage driver. */
            InsertConfigNode(pInst,    "LUN#0",     &pLunL0);
            InsertConfigString(pLunL0, "Driver",    "NvramStore");
        }

        /*
         * The USB Controllers.
         */
        com::SafeIfaceArray<IUSBController> usbCtrls;
        hrc = pMachine->COMGETTER(USBControllers)(ComSafeArrayAsOutParam(usbCtrls));
        bool fOhciPresent = false; /**< Flag whether at least one OHCI controller is present. */
        bool fXhciPresent = false; /**< Flag whether at least one XHCI controller is present. */

        if (SUCCEEDED(hrc))
        {
            for (size_t i = 0; i < usbCtrls.size(); ++i)
            {
                USBControllerType_T enmCtrlType;
                vrc = usbCtrls[i]->COMGETTER(Type)(&enmCtrlType);                                  H();
                if (enmCtrlType == USBControllerType_OHCI)
                {
                    fOhciPresent = true;
                    break;
                }
                else if (enmCtrlType == USBControllerType_XHCI)
                {
                    fXhciPresent = true;
                    break;
                }
            }
        }
        else if (hrc != E_NOTIMPL)
        {
            H();
        }

        /*
         * Currently EHCI is only enabled when an OHCI or XHCI controller is present as well.
         */
        if (fOhciPresent || fXhciPresent)
            mfVMHasUsbController = true;

        PCFGMNODE pUsbDevices = NULL; /**< Required for USB storage controller later. */
        if (mfVMHasUsbController)
        {
            for (size_t i = 0; i < usbCtrls.size(); ++i)
            {
                USBControllerType_T enmCtrlType;
                vrc = usbCtrls[i]->COMGETTER(Type)(&enmCtrlType);                                  H();

                if (enmCtrlType == USBControllerType_OHCI)
                {
                    InsertConfigNode(pDevices, "usb-ohci", &pDev);
                    InsertConfigNode(pDev,     "0", &pInst);
                    InsertConfigNode(pInst,    "Config", &pCfg);
                    InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
                    hrc = pBusMgr->assignPCIDevice("usb-ohci", pInst);                          H();
                    InsertConfigNode(pInst,    "LUN#0", &pLunL0);
                    InsertConfigString(pLunL0, "Driver",               "VUSBRootHub");
                    InsertConfigNode(pLunL0,   "Config", &pCfg);

                    /*
                     * Attach the status driver.
                     */
                    i_attachStatusDriver(pInst, DeviceType_USB);
                }
#ifdef VBOX_WITH_EHCI
                else if (enmCtrlType == USBControllerType_EHCI)
                {
                    InsertConfigNode(pDevices, "usb-ehci", &pDev);
                    InsertConfigNode(pDev,     "0", &pInst);
                    InsertConfigNode(pInst,    "Config", &pCfg);
                    InsertConfigInteger(pInst, "Trusted", 1); /* boolean */
                    hrc = pBusMgr->assignPCIDevice("usb-ehci", pInst);                  H();

                    InsertConfigNode(pInst,    "LUN#0", &pLunL0);
                    InsertConfigString(pLunL0, "Driver",               "VUSBRootHub");
                    InsertConfigNode(pLunL0,   "Config", &pCfg);

                    /*
                     * Attach the status driver.
                     */
                    i_attachStatusDriver(pInst, DeviceType_USB);
                }
#endif
                else if (enmCtrlType == USBControllerType_XHCI)
                {
                    InsertConfigNode(pDevices, "usb-xhci", &pDev);
                    InsertConfigNode(pDev,     "0", &pInst);
                    InsertConfigNode(pInst,    "Config", &pCfg);
                    InsertConfigInteger(pInst, "Trusted", 1); /* boolean */
                    hrc = pBusMgr->assignPCIDevice("usb-xhci", pInst);                  H();

                    InsertConfigNode(pInst,    "LUN#0", &pLunL0);
                    InsertConfigString(pLunL0, "Driver",               "VUSBRootHub");
                    InsertConfigNode(pLunL0,   "Config", &pCfg);

                    InsertConfigNode(pInst,    "LUN#1", &pLunL1);
                    InsertConfigString(pLunL1, "Driver",               "VUSBRootHub");
                    InsertConfigNode(pLunL1,   "Config", &pCfg);

                    /*
                     * Attach the status driver.
                     */
                    i_attachStatusDriver(pInst, DeviceType_USB, 2);
                }
            } /* for every USB controller. */


            /*
             * Virtual USB Devices.
             */
            InsertConfigNode(pRoot, "USB", &pUsbDevices);

#ifdef VBOX_WITH_USB
            {
                /*
                 * Global USB options, currently unused as we'll apply the 2.0 -> 1.1 morphing
                 * on a per device level now.
                 */
                InsertConfigNode(pUsbDevices, "USBProxy", &pCfg);
                InsertConfigNode(pCfg, "GlobalConfig", &pCfg);
                // This globally enables the 2.0 -> 1.1 device morphing of proxied devices to keep windows quiet.
                //InsertConfigInteger(pCfg, "Force11Device", true);
                // The following breaks stuff, but it makes MSDs work in vista. (I include it here so
                // that it's documented somewhere.) Users needing it can use:
                //      VBoxManage setextradata "myvm" "VBoxInternal/USB/USBProxy/GlobalConfig/Force11PacketSize" 1
                //InsertConfigInteger(pCfg, "Force11PacketSize", true);
            }
#endif

#ifdef VBOX_WITH_USB_CARDREADER
            BOOL aEmulatedUSBCardReaderEnabled = FALSE;
            hrc = pMachine->COMGETTER(EmulatedUSBCardReaderEnabled)(&aEmulatedUSBCardReaderEnabled);    H();
            if (aEmulatedUSBCardReaderEnabled)
            {
                InsertConfigNode(pUsbDevices, "CardReader", &pDev);
                InsertConfigNode(pDev,     "0", &pInst);
                InsertConfigNode(pInst,    "Config", &pCfg);

                InsertConfigNode(pInst,    "LUN#0", &pLunL0);
# ifdef VBOX_WITH_USB_CARDREADER_TEST
                InsertConfigString(pLunL0, "Driver", "DrvDirectCardReader");
                InsertConfigNode(pLunL0,   "Config", &pCfg);
# else
                InsertConfigString(pLunL0, "Driver", "UsbCardReader");
                InsertConfigNode(pLunL0,   "Config", &pCfg);
# endif
             }
#endif

            /* Virtual USB Mouse/Tablet */
            if (   aPointingHID == PointingHIDType_USBMouse
                || aPointingHID == PointingHIDType_USBTablet
                || aPointingHID == PointingHIDType_USBMultiTouch
                || aPointingHID == PointingHIDType_USBMultiTouchScreenPlusPad)
            {
                InsertConfigNode(pUsbDevices, "HidMouse", &pDev);
                InsertConfigNode(pDev,     "0", &pInst);
                InsertConfigNode(pInst,    "Config", &pCfg);

                if (aPointingHID == PointingHIDType_USBMouse)
                    InsertConfigString(pCfg,   "Mode", "relative");
                else
                    InsertConfigString(pCfg,   "Mode", "absolute");
                InsertConfigNode(pInst,    "LUN#0", &pLunL0);
                InsertConfigString(pLunL0, "Driver",        "MouseQueue");
                InsertConfigNode(pLunL0,   "Config", &pCfg);
                InsertConfigInteger(pCfg,  "QueueSize",            128);

                InsertConfigNode(pLunL0,   "AttachedDriver", &pLunL1);
                InsertConfigString(pLunL1, "Driver",        "MainMouse");
            }
            if (   aPointingHID == PointingHIDType_USBMultiTouch
                || aPointingHID == PointingHIDType_USBMultiTouchScreenPlusPad)
            {
                InsertConfigNode(pDev,     "1", &pInst);
                InsertConfigNode(pInst,    "Config", &pCfg);

                InsertConfigString(pCfg,   "Mode", "multitouch");
                InsertConfigNode(pInst,    "LUN#0", &pLunL0);
                InsertConfigString(pLunL0, "Driver",        "MouseQueue");
                InsertConfigNode(pLunL0,   "Config", &pCfg);
                InsertConfigInteger(pCfg,  "QueueSize",            128);

                InsertConfigNode(pLunL0,   "AttachedDriver", &pLunL1);
                InsertConfigString(pLunL1, "Driver",        "MainMouse");
            }
            if (aPointingHID == PointingHIDType_USBMultiTouchScreenPlusPad)
            {
                InsertConfigNode(pDev,     "2", &pInst);
                InsertConfigNode(pInst,    "Config", &pCfg);

                InsertConfigString(pCfg,   "Mode", "touchpad");
                InsertConfigNode(pInst,    "LUN#0", &pLunL0);
                InsertConfigString(pLunL0, "Driver",        "MouseQueue");
                InsertConfigNode(pLunL0,   "Config", &pCfg);
                InsertConfigInteger(pCfg,  "QueueSize",            128);

                InsertConfigNode(pLunL0,   "AttachedDriver", &pLunL1);
                InsertConfigString(pLunL1, "Driver",        "MainMouse");
            }

            /* Virtual USB Keyboard */
            if (aKbdHID == KeyboardHIDType_USBKeyboard)
            {
                InsertConfigNode(pUsbDevices, "HidKeyboard", &pDev);
                InsertConfigNode(pDev,     "0", &pInst);
                InsertConfigNode(pInst,    "Config", &pCfg);

                InsertConfigNode(pInst,    "LUN#0", &pLunL0);
                InsertConfigString(pLunL0, "Driver",               "KeyboardQueue");
                InsertConfigNode(pLunL0,   "Config", &pCfg);
                InsertConfigInteger(pCfg,  "QueueSize",            64);

                InsertConfigNode(pLunL0,   "AttachedDriver", &pLunL1);
                InsertConfigString(pLunL1, "Driver",               "MainKeyboard");
            }
        }

        /*
         * Storage controllers.
         */
        com::SafeIfaceArray<IStorageController> ctrls;
        PCFGMNODE aCtrlNodes[StorageControllerType_VirtioSCSI + 1] = {};
        hrc = pMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));       H();

        bool fFdcEnabled = false;
        for (size_t i = 0; i < ctrls.size(); ++i)
        {
            DeviceType_T *paLedDevType = NULL;

            StorageControllerType_T enmCtrlType;
            hrc = ctrls[i]->COMGETTER(ControllerType)(&enmCtrlType);                        H();
            AssertRelease((unsigned)enmCtrlType < RT_ELEMENTS(aCtrlNodes)
                          || enmCtrlType == StorageControllerType_USB);

            StorageBus_T enmBus;
            hrc = ctrls[i]->COMGETTER(Bus)(&enmBus);                                        H();

            Bstr controllerName;
            hrc = ctrls[i]->COMGETTER(Name)(controllerName.asOutParam());                   H();

            ULONG ulInstance = 999;
            hrc = ctrls[i]->COMGETTER(Instance)(&ulInstance);                               H();

            BOOL fUseHostIOCache;
            hrc = ctrls[i]->COMGETTER(UseHostIOCache)(&fUseHostIOCache);                    H();

            BOOL fBootable;
            hrc = ctrls[i]->COMGETTER(Bootable)(&fBootable);                                H();

            PCFGMNODE pCtlInst = NULL;
            const char *pszCtrlDev = i_storageControllerTypeToStr(enmCtrlType);
            if (enmCtrlType != StorageControllerType_USB)
            {
                /* /Devices/<ctrldev>/ */
                pDev = aCtrlNodes[enmCtrlType];
                if (!pDev)
                {
                    InsertConfigNode(pDevices, pszCtrlDev, &pDev);
                    aCtrlNodes[enmCtrlType] = pDev; /* IDE variants are handled in the switch */
                }

                /* /Devices/<ctrldev>/<instance>/ */
                InsertConfigNode(pDev, Utf8StrFmt("%u", ulInstance).c_str(), &pCtlInst);

                /* Device config: /Devices/<ctrldev>/<instance>/<values> & /ditto/Config/<values> */
                InsertConfigInteger(pCtlInst, "Trusted",   1);
                InsertConfigNode(pCtlInst,    "Config",    &pCfg);
            }

            static const char * const apszBiosConfigScsi[MAX_BIOS_LUN_COUNT] =
            { "ScsiLUN1", "ScsiLUN2", "ScsiLUN3", "ScsiLUN4" };

            static const char * const apszBiosConfigSata[MAX_BIOS_LUN_COUNT] =
            { "SataLUN1", "SataLUN2", "SataLUN3", "SataLUN4" };

            switch (enmCtrlType)
            {
                case StorageControllerType_LsiLogic:
                {
                    hrc = pBusMgr->assignPCIDevice("lsilogic", pCtlInst);                   H();

                    InsertConfigInteger(pCfg, "Bootable",  fBootable);

                    /* BIOS configuration values, first SCSI controller only. */
                    if (   !pBusMgr->hasPCIDevice("lsilogic", 1)
                        && !pBusMgr->hasPCIDevice("buslogic", 0)
                        && !pBusMgr->hasPCIDevice("lsilogicsas", 0)
                        && pBiosCfg)
                    {
                        InsertConfigString(pBiosCfg, "ScsiHardDiskDevice", "lsilogicscsi");
                        hrc = SetBiosDiskInfo(pMachine, pCfg, pBiosCfg, controllerName, apszBiosConfigScsi);    H();
                    }

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk) | RT_BIT_32(DeviceType_DVD),
                                         16, &paLedDevType, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                case StorageControllerType_BusLogic:
                {
                    hrc = pBusMgr->assignPCIDevice("buslogic", pCtlInst);                   H();

                    InsertConfigInteger(pCfg, "Bootable",  fBootable);

                    /* BIOS configuration values, first SCSI controller only. */
                    if (   !pBusMgr->hasPCIDevice("lsilogic", 0)
                        && !pBusMgr->hasPCIDevice("buslogic", 1)
                        && !pBusMgr->hasPCIDevice("lsilogicsas", 0)
                        && pBiosCfg)
                    {
                        InsertConfigString(pBiosCfg, "ScsiHardDiskDevice", "buslogic");
                        hrc = SetBiosDiskInfo(pMachine, pCfg, pBiosCfg, controllerName, apszBiosConfigScsi);    H();
                    }

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk) | RT_BIT_32(DeviceType_DVD),
                                         16, &paLedDevType, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                case StorageControllerType_IntelAhci:
                {
                    hrc = pBusMgr->assignPCIDevice("ahci", pCtlInst);                       H();

                    ULONG cPorts = 0;
                    hrc = ctrls[i]->COMGETTER(PortCount)(&cPorts);                          H();
                    InsertConfigInteger(pCfg, "PortCount", cPorts);
                    InsertConfigInteger(pCfg, "Bootable",  fBootable);

                    com::SafeIfaceArray<IMediumAttachment> atts;
                    hrc = pMachine->GetMediumAttachmentsOfController(controllerName.raw(),
                                                                     ComSafeArrayAsOutParam(atts));  H();

                    /* Configure the hotpluggable flag for the port. */
                    for (unsigned idxAtt = 0; idxAtt < atts.size(); ++idxAtt)
                    {
                        IMediumAttachment *pMediumAtt = atts[idxAtt];

                        LONG lPortNum = 0;
                        hrc = pMediumAtt->COMGETTER(Port)(&lPortNum);                       H();

                        BOOL fHotPluggable = FALSE;
                        hrc = pMediumAtt->COMGETTER(HotPluggable)(&fHotPluggable);          H();
                        if (SUCCEEDED(hrc))
                        {
                            PCFGMNODE pPortCfg;
                            char szName[24];
                            RTStrPrintf(szName, sizeof(szName), "Port%d", lPortNum);

                            InsertConfigNode(pCfg, szName, &pPortCfg);
                            InsertConfigInteger(pPortCfg, "Hotpluggable", fHotPluggable ? 1 : 0);
                        }
                    }

                    /* BIOS configuration values, first AHCI controller only. */
                    if (   !pBusMgr->hasPCIDevice("ahci", 1)
                        && pBiosCfg)
                    {
                        InsertConfigString(pBiosCfg, "SataHardDiskDevice", "ahci");
                        hrc = SetBiosDiskInfo(pMachine, pCfg, pBiosCfg, controllerName, apszBiosConfigSata);    H();
                    }

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk) | RT_BIT_32(DeviceType_DVD),
                                         cPorts, &paLedDevType, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                case StorageControllerType_PIIX3:
                case StorageControllerType_PIIX4:
                case StorageControllerType_ICH6:
                {
                    /*
                     * IDE (update this when the main interface changes)
                     */
                    hrc = pBusMgr->assignPCIDevice("piix3ide", pCtlInst);                   H();
                    InsertConfigString(pCfg,   "Type", controllerString(enmCtrlType));

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk) | RT_BIT_32(DeviceType_DVD),
                                         4, &paLedDevType, &mapMediumAttachments, pszCtrlDev, ulInstance);

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
                    InsertConfigInteger(pCfg, "IRQ",       6);
                    InsertConfigInteger(pCfg, "DMA",       2);
                    InsertConfigInteger(pCfg, "MemMapped", 0 );
                    InsertConfigInteger(pCfg, "IOBase",    0x3f0);

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_Floppy),
                                         2, NULL, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                case StorageControllerType_LsiLogicSas:
                {
                    hrc = pBusMgr->assignPCIDevice("lsilogicsas", pCtlInst);                H();

                    InsertConfigString(pCfg,  "ControllerType", "SAS1068");
                    InsertConfigInteger(pCfg, "Bootable",  fBootable);

                    /* BIOS configuration values, first SCSI controller only. */
                    if (   !pBusMgr->hasPCIDevice("lsilogic", 0)
                        && !pBusMgr->hasPCIDevice("buslogic", 0)
                        && !pBusMgr->hasPCIDevice("lsilogicsas", 1)
                        && pBiosCfg)
                    {
                        InsertConfigString(pBiosCfg, "ScsiHardDiskDevice", "lsilogicsas");
                        hrc = SetBiosDiskInfo(pMachine, pCfg, pBiosCfg, controllerName, apszBiosConfigScsi);    H();
                    }

                    ULONG cPorts = 0;
                    hrc = ctrls[i]->COMGETTER(PortCount)(&cPorts);                          H();
                    InsertConfigInteger(pCfg, "NumPorts", cPorts);

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk) | RT_BIT_32(DeviceType_DVD) /*?*/,
                                         8, &paLedDevType, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                case StorageControllerType_USB:
                {
                    if (pUsbDevices)
                    {
                        /*
                         * USB MSDs are handled a bit different as the device instance
                         * doesn't match the storage controller instance but the port.
                         */
                        InsertConfigNode(pUsbDevices, "Msd", &pDev);
                        pCtlInst = pDev;
                    }
                    else
                        return pVMM->pfnVMR3SetError(pUVM, VERR_NOT_FOUND, RT_SRC_POS,
                                N_("There is no USB controller enabled but there\n"
                                   "is at least one USB storage device configured for this VM.\n"
                                   "To fix this problem either enable the USB controller or remove\n"
                                   "the storage device from the VM"));
                    break;
                }

                case StorageControllerType_NVMe:
                {
                    hrc = pBusMgr->assignPCIDevice("nvme", pCtlInst);                       H();

                    ULONG cPorts = 0;
                    hrc = ctrls[i]->COMGETTER(PortCount)(&cPorts);                          H();
                    InsertConfigInteger(pCfg, "NamespacesMax", cPorts);

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk),
                                         cPorts, NULL, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                case StorageControllerType_VirtioSCSI:
                {
                    hrc = pBusMgr->assignPCIDevice("virtio-scsi", pCtlInst);                H();

                    ULONG cPorts = 0;
                    hrc = ctrls[i]->COMGETTER(PortCount)(&cPorts);                          H();
                    InsertConfigInteger(pCfg, "NumTargets", cPorts);
                    InsertConfigInteger(pCfg, "Bootable",   fBootable);

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk) | RT_BIT_32(DeviceType_DVD) /*?*/,
                                         cPorts, &paLedDevType, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                default:
                    AssertLogRelMsgFailedReturn(("invalid storage controller type: %d\n", enmCtrlType), VERR_MAIN_CONFIG_CONSTRUCTOR_IPE);
            }

            /* Attach the media to the storage controllers. */
            com::SafeIfaceArray<IMediumAttachment> atts;
            hrc = pMachine->GetMediumAttachmentsOfController(controllerName.raw(),
                                                            ComSafeArrayAsOutParam(atts));  H();

            /* Builtin I/O cache - per device setting. */
            BOOL fBuiltinIOCache = true;
            hrc = pMachine->COMGETTER(IOCacheEnabled)(&fBuiltinIOCache);                    H();

            bool fInsertDiskIntegrityDrv = false;
            Bstr strDiskIntegrityFlag;
            hrc = pMachine->GetExtraData(Bstr("VBoxInternal2/EnableDiskIntegrityDriver").raw(),
                                         strDiskIntegrityFlag.asOutParam());
            if (   hrc   == S_OK
                && strDiskIntegrityFlag == "1")
                fInsertDiskIntegrityDrv = true;

            for (size_t j = 0; j < atts.size(); ++j)
            {
                IMediumAttachment *pMediumAtt = atts[j];
                vrc = i_configMediumAttachment(pszCtrlDev,
                                               ulInstance,
                                               enmBus,
                                               !!fUseHostIOCache,
                                               enmCtrlType == StorageControllerType_NVMe ? false : !!fBuiltinIOCache,
                                               fInsertDiskIntegrityDrv,
                                               false /* fSetupMerge */,
                                               0 /* uMergeSource */,
                                               0 /* uMergeTarget */,
                                               pMediumAtt,
                                               mMachineState,
                                               NULL /* phrc */,
                                               false /* fAttachDetach */,
                                               false /* fForceUnmount */,
                                               false /* fHotplug */,
                                               pUVM,
                                               pVMM,
                                               paLedDevType,
                                               NULL /* ppLunL0 */);
                if (RT_FAILURE(vrc))
                    return vrc;
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
        InsertConfigNode(pDevices, "pcnet", &pDevPCNet);
#ifdef VBOX_WITH_E1000
        PCFGMNODE pDevE1000 = NULL;          /* E1000-type devices */
        InsertConfigNode(pDevices, "e1000", &pDevE1000);
#endif
#ifdef VBOX_WITH_VIRTIO
        PCFGMNODE pDevVirtioNet = NULL;          /* Virtio network devices */
        InsertConfigNode(pDevices, "virtio-net", &pDevVirtioNet);
#endif /* VBOX_WITH_VIRTIO */
        PCFGMNODE pDevDP8390 = NULL;         /* DP8390-type devices */
        InsertConfigNode(pDevices, "dp8390", &pDevDP8390);
        PCFGMNODE pDev3C501 = NULL;          /* EtherLink-type devices */
        InsertConfigNode(pDevices, "3c501",  &pDev3C501);

        std::list<BootNic> llBootNics;
        for (ULONG uInstance = 0; uInstance < maxNetworkAdapters; ++uInstance)
        {
            ComPtr<INetworkAdapter> networkAdapter;
            hrc = pMachine->GetNetworkAdapter(uInstance, networkAdapter.asOutParam());      H();
            BOOL fEnabledNetAdapter = FALSE;
            hrc = networkAdapter->COMGETTER(Enabled)(&fEnabledNetAdapter);                  H();
            if (!fEnabledNetAdapter)
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
                case NetworkAdapterType_Am79C960:
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
                case NetworkAdapterType_NE1000:
                case NetworkAdapterType_NE2000:
                case NetworkAdapterType_WD8003:
                case NetworkAdapterType_WD8013:
                case NetworkAdapterType_ELNK2:
                    pDev = pDevDP8390;
                    break;
                case NetworkAdapterType_ELNK1:
                    pDev = pDev3C501;
                    break;
                default:
                    AssertMsgFailed(("Invalid network adapter type '%d' for slot '%d'", adapterType, uInstance));
                    return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                                 N_("Invalid network adapter type '%d' for slot '%d'"), adapterType, uInstance);
            }

            InsertConfigNode(pDev, Utf8StrFmt("%u", uInstance).c_str(), &pInst);
            InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
            /* the first network card gets the PCI ID 3, the next 3 gets 8..10,
             * next 4 get 16..19. */
            int iPCIDeviceNo;
            switch (uInstance)
            {
                case 0:
                    iPCIDeviceNo = 3;
                    break;
                case 1: case 2: case 3:
                    iPCIDeviceNo = uInstance - 1 + 8;
                    break;
                case 4: case 5: case 6: case 7:
                    iPCIDeviceNo = uInstance - 4 + 16;
                    break;
                default:
                    /* auto assignment */
                    iPCIDeviceNo = -1;
                    break;
            }
#ifdef VMWARE_NET_IN_SLOT_11
            /*
             * Dirty hack for PCI slot compatibility with VMWare,
             * it assigns slot 0x11 to the first network controller.
             */
            if (iPCIDeviceNo == 3 && adapterType == NetworkAdapterType_I82545EM)
            {
                iPCIDeviceNo = 0x11;
                fSwapSlots3and11 = true;
            }
            else if (iPCIDeviceNo == 0x11 && fSwapSlots3and11)
                iPCIDeviceNo = 3;
#endif
            PCIBusAddress PCIAddr = PCIBusAddress(0, iPCIDeviceNo, 0);
            hrc = pBusMgr->assignPCIDevice(pszAdapterName, pInst, PCIAddr);                 H();

            InsertConfigNode(pInst, "Config", &pCfg);
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE   /* not safe here yet. */ /** @todo Make PCNet ring-0 safe on 32-bit mac kernels! */
            if (pDev == pDevPCNet)
                InsertConfigInteger(pCfg, "R0Enabled",    false);
#endif
            /*
             * Collect information needed for network booting and add it to the list.
             */
            BootNic     nic;

            nic.mInstance    = uInstance;
            /* Could be updated by reference, if auto assigned */
            nic.mPCIAddress  = PCIAddr;

            hrc = networkAdapter->COMGETTER(BootPriority)(&nic.mBootPrio);                  H();

            llBootNics.push_back(nic);

            /*
             * The virtual hardware type. PCNet supports three types, E1000 three,
             * but VirtIO only one.
             */
            switch (adapterType)
            {
                case NetworkAdapterType_Am79C970A:
                    InsertConfigString(pCfg, "ChipType", "Am79C970A");
                    break;
                case NetworkAdapterType_Am79C973:
                    InsertConfigString(pCfg, "ChipType", "Am79C973");
                    break;
                case NetworkAdapterType_Am79C960:
                    InsertConfigString(pCfg, "ChipType", "Am79C960");
                    break;
                case NetworkAdapterType_I82540EM:
                    InsertConfigInteger(pCfg, "AdapterType", 0);
                    break;
                case NetworkAdapterType_I82543GC:
                    InsertConfigInteger(pCfg, "AdapterType", 1);
                    break;
                case NetworkAdapterType_I82545EM:
                    InsertConfigInteger(pCfg, "AdapterType", 2);
                    break;
                case NetworkAdapterType_Virtio:
                    break;
                case NetworkAdapterType_NE1000:
                    InsertConfigString(pCfg, "DeviceType", "NE1000");
                    break;
                case NetworkAdapterType_NE2000:
                    InsertConfigString(pCfg, "DeviceType", "NE2000");
                    break;
                case NetworkAdapterType_WD8003:
                    InsertConfigString(pCfg, "DeviceType", "WD8003");
                    break;
                case NetworkAdapterType_WD8013:
                    InsertConfigString(pCfg, "DeviceType", "WD8013");
                    break;
                case NetworkAdapterType_ELNK2:
                    InsertConfigString(pCfg, "DeviceType", "3C503");
                    break;
                case NetworkAdapterType_ELNK1:
                    break;
                case NetworkAdapterType_Null:      AssertFailedBreak(); /* (compiler warnings) */
#ifdef VBOX_WITH_XPCOM_CPP_ENUM_HACK
                case NetworkAdapterType_32BitHack: AssertFailedBreak(); /* (compiler warnings) */
#endif
            }

            /*
             * Get the MAC address and convert it to binary representation
             */
            Bstr macAddr;
            hrc = networkAdapter->COMGETTER(MACAddress)(macAddr.asOutParam());              H();
            Assert(!macAddr.isEmpty());
            Utf8Str macAddrUtf8 = macAddr;
#ifdef VBOX_WITH_CLOUD_NET
            NetworkAttachmentType_T eAttachmentType;
            hrc = networkAdapter->COMGETTER(AttachmentType)(&eAttachmentType);                 H();
            if (eAttachmentType == NetworkAttachmentType_Cloud)
            {
                mGateway.setLocalMacAddress(macAddrUtf8);
                /* We'll insert cloud MAC later, when it becomes known. */
            }
            else
            {
#endif
            char *macStr = (char*)macAddrUtf8.c_str();
            Assert(strlen(macStr) == 12);
            RTMAC Mac;
            RT_ZERO(Mac);
            char *pMac = (char*)&Mac;
            for (uint32_t i = 0; i < 6; ++i)
            {
                int c1 = *macStr++ - '0';
                if (c1 > 9)
                    c1 -= 7;
                int c2 = *macStr++ - '0';
                if (c2 > 9)
                    c2 -= 7;
                *pMac++ = (char)(((c1 & 0x0f) << 4) | (c2 & 0x0f));
            }
            InsertConfigBytes(pCfg, "MAC", &Mac, sizeof(Mac));
#ifdef VBOX_WITH_CLOUD_NET
            }
#endif
            /*
             * Check if the cable is supposed to be unplugged
             */
            BOOL fCableConnected;
            hrc = networkAdapter->COMGETTER(CableConnected)(&fCableConnected);              H();
            InsertConfigInteger(pCfg, "CableConnected", fCableConnected ? 1 : 0);

            /*
             * Line speed to report from custom drivers
             */
            ULONG ulLineSpeed;
            hrc = networkAdapter->COMGETTER(LineSpeed)(&ulLineSpeed);                       H();
            InsertConfigInteger(pCfg, "LineSpeed", ulLineSpeed);

            /*
             * Attach the status driver.
             */
            i_attachStatusDriver(pInst, DeviceType_Network);

            /*
             * Configure the network card now
             */
            bool fIgnoreConnectFailure = mMachineState == MachineState_Restoring;
            vrc = i_configNetwork(pszAdapterName,
                                  uInstance,
                                  0,
                                  networkAdapter,
                                  pCfg,
                                  pLunL0,
                                  pInst,
                                  false /*fAttachDetach*/,
                                  fIgnoreConnectFailure,
                                  pUVM,
                                  pVMM);
            if (RT_FAILURE(vrc))
                return vrc;
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
                achBootIdx[0] = (char)('0' + uBootIdx++);   /* Boot device order. */
                InsertConfigNode(pNetBootCfg, achBootIdx, &pNetBtDevCfg);
                InsertConfigInteger(pNetBtDevCfg, "NIC", it->mInstance);
                InsertConfigInteger(pNetBtDevCfg, "PCIBusNo",      it->mPCIAddress.miBus);
                InsertConfigInteger(pNetBtDevCfg, "PCIDeviceNo",   it->mPCIAddress.miDevice);
                InsertConfigInteger(pNetBtDevCfg, "PCIFunctionNo", it->mPCIAddress.miFn);
            }
        }

        /*
         * Serial (UART) Ports
         */
        /* serial enabled mask to be passed to dev ACPI */
        uint16_t auSerialIoPortBase[SchemaDefs::SerialPortCount] = {0};
        uint8_t auSerialIrq[SchemaDefs::SerialPortCount] = {0};
        InsertConfigNode(pDevices, "serial", &pDev);
        for (ULONG ulInstance = 0; ulInstance < SchemaDefs::SerialPortCount; ++ulInstance)
        {
            ComPtr<ISerialPort> serialPort;
            hrc = pMachine->GetSerialPort(ulInstance, serialPort.asOutParam());             H();
            BOOL fEnabledSerPort = FALSE;
            if (serialPort)
            {
                hrc = serialPort->COMGETTER(Enabled)(&fEnabledSerPort);                     H();
            }
            if (!fEnabledSerPort)
            {
                m_aeSerialPortMode[ulInstance] = PortMode_Disconnected;
                continue;
            }

            InsertConfigNode(pDev, Utf8StrFmt("%u", ulInstance).c_str(), &pInst);
            InsertConfigInteger(pInst, "Trusted", 1); /* boolean */
            InsertConfigNode(pInst, "Config", &pCfg);

            ULONG ulIRQ;
            hrc = serialPort->COMGETTER(IRQ)(&ulIRQ);                                       H();
            InsertConfigInteger(pCfg, "IRQ", ulIRQ);
            auSerialIrq[ulInstance] = (uint8_t)ulIRQ;

            ULONG ulIOBase;
            hrc = serialPort->COMGETTER(IOBase)(&ulIOBase);                                 H();
            InsertConfigInteger(pCfg, "IOBase", ulIOBase);
            auSerialIoPortBase[ulInstance] = (uint16_t)ulIOBase;

            BOOL  fServer;
            hrc = serialPort->COMGETTER(Server)(&fServer);                                  H();
            hrc = serialPort->COMGETTER(Path)(bstr.asOutParam());                           H();
            UartType_T eUartType;
            const char *pszUartType;
            hrc = serialPort->COMGETTER(UartType)(&eUartType);                              H();
            switch (eUartType)
            {
                case UartType_U16450: pszUartType = "16450"; break;
                case UartType_U16750: pszUartType = "16750"; break;
                default: AssertFailed(); RT_FALL_THRU();
                case UartType_U16550A: pszUartType = "16550A"; break;
            }
            InsertConfigString(pCfg, "UartType", pszUartType);

            PortMode_T eHostMode;
            hrc = serialPort->COMGETTER(HostMode)(&eHostMode);                              H();

            m_aeSerialPortMode[ulInstance] = eHostMode;
            if (eHostMode != PortMode_Disconnected)
            {
                vrc = i_configSerialPort(pInst, eHostMode, Utf8Str(bstr).c_str(), RT_BOOL(fServer));
                if (RT_FAILURE(vrc))
                    return vrc;
            }
        }

        /*
         * Parallel (LPT) Ports
         */
        /* parallel enabled mask to be passed to dev ACPI */
        uint16_t auParallelIoPortBase[SchemaDefs::ParallelPortCount] = {0};
        uint8_t auParallelIrq[SchemaDefs::ParallelPortCount] = {0};
        InsertConfigNode(pDevices, "parallel", &pDev);
        for (ULONG ulInstance = 0; ulInstance < SchemaDefs::ParallelPortCount; ++ulInstance)
        {
            ComPtr<IParallelPort> parallelPort;
            hrc = pMachine->GetParallelPort(ulInstance, parallelPort.asOutParam());         H();
            BOOL fEnabledParPort = FALSE;
            if (parallelPort)
            {
                hrc = parallelPort->COMGETTER(Enabled)(&fEnabledParPort);                   H();
            }
            if (!fEnabledParPort)
                continue;

            InsertConfigNode(pDev, Utf8StrFmt("%u", ulInstance).c_str(), &pInst);
            InsertConfigNode(pInst, "Config", &pCfg);

            ULONG ulIRQ;
            hrc = parallelPort->COMGETTER(IRQ)(&ulIRQ);                                     H();
            InsertConfigInteger(pCfg, "IRQ", ulIRQ);
            auParallelIrq[ulInstance] = (uint8_t)ulIRQ;
            ULONG ulIOBase;
            hrc = parallelPort->COMGETTER(IOBase)(&ulIOBase);                               H();
            InsertConfigInteger(pCfg,   "IOBase", ulIOBase);
            auParallelIoPortBase[ulInstance] = (uint16_t)ulIOBase;

            hrc = parallelPort->COMGETTER(Path)(bstr.asOutParam());                         H();
            if (!bstr.isEmpty())
            {
                InsertConfigNode(pInst,     "LUN#0", &pLunL0);
                InsertConfigString(pLunL0,  "Driver", "HostParallel");
                InsertConfigNode(pLunL0,    "Config", &pLunL1);
                InsertConfigString(pLunL1,  "DevicePath", bstr);
            }
        }

        /*
         * VMM Device
         */
        InsertConfigNode(pDevices, "VMMDev", &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigNode(pInst,    "Config", &pCfg);
        InsertConfigInteger(pInst, "Trusted",              1); /* boolean */
        hrc = pBusMgr->assignPCIDevice("VMMDev", pInst);                                    H();

        Bstr hwVersion;
        hrc = pMachine->COMGETTER(HardwareVersion)(hwVersion.asOutParam());                 H();
        if (hwVersion.compare(Bstr("1").raw()) == 0) /* <= 2.0.x */
            InsertConfigInteger(pCfg, "HeapEnabled", 0);
        Bstr snapshotFolder;
        hrc = pMachine->COMGETTER(SnapshotFolder)(snapshotFolder.asOutParam());             H();
        InsertConfigString(pCfg, "GuestCoreDumpDir", snapshotFolder);

        /* the VMM device's Main driver */
        InsertConfigNode(pInst,    "LUN#0", &pLunL0);
        InsertConfigString(pLunL0, "Driver",               "HGCM");
        InsertConfigNode(pLunL0,   "Config", &pCfg);

        /*
         * Attach the status driver.
         */
        i_attachStatusDriver(pInst, DeviceType_SharedFolder);

        /*
         * Audio configuration.
         */

        /*
         * AC'97 ICH / SoundBlaster16 audio / Intel HD Audio.
         */
        ComPtr<IAudioSettings> audioSettings;
        hrc = pMachine->COMGETTER(AudioSettings)(audioSettings.asOutParam());               H();

        BOOL fAudioEnabled = FALSE;
        ComPtr<IAudioAdapter> audioAdapter;
        hrc = audioSettings->COMGETTER(Adapter)(audioAdapter.asOutParam());                 H();
        if (audioAdapter)
        {
            hrc = audioAdapter->COMGETTER(Enabled)(&fAudioEnabled);                         H();
        }

        if (fAudioEnabled)
        {
            AudioControllerType_T enmAudioController;
            hrc = audioAdapter->COMGETTER(AudioController)(&enmAudioController);            H();
            AudioCodecType_T enmAudioCodec;
            hrc = audioAdapter->COMGETTER(AudioCodec)(&enmAudioCodec);                      H();

            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/Audio/Device/TimerHz", &strTmp);
            const uint64_t uTimerHz = strTmp.toUInt64();

            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/Audio/Device/BufSizeInMs", &strTmp);
            const uint64_t uBufSizeInMs = strTmp.toUInt64();

            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/Audio/Device/BufSizeOutMs", &strTmp);
            const uint64_t uBufSizeOutMs = strTmp.toUInt64();

            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/Audio/Debug/Enabled", &strTmp);
            const bool fDebugEnabled = strTmp.equalsIgnoreCase("true") || strTmp.equalsIgnoreCase("1");

            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/Audio/Debug/Level", &strTmp);
            const uint32_t uDebugLevel = strTmp.toUInt32();

            Utf8Str strDebugPathOut;
            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/Audio/Debug/PathOut", &strDebugPathOut);

#ifdef VBOX_WITH_AUDIO_VALIDATIONKIT
            GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/Audio/VaKit/Enabled", &strTmp); /* Deprecated; do not use! */
            if (strTmp.isEmpty())
                GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/Audio/ValKit/Enabled", &strTmp);
            /* Whether the Validation Kit audio backend runs as the primary backend.
             * Can also be used with VBox release builds. */
            const bool fValKitEnabled = strTmp.equalsIgnoreCase("true") || strTmp.equalsIgnoreCase("1");
#endif
            /** @todo Implement an audio device class, similar to the audio backend class, to construct the common stuff
             *        without duplicating (more) code. */

            const char *pszAudioDevice;
            switch (enmAudioController)
            {
                case AudioControllerType_AC97:
                {
                    /* ICH AC'97. */
                    pszAudioDevice = "ichac97";

                    InsertConfigNode(pDevices,      pszAudioDevice,         &pDev);
                    InsertConfigNode(pDev,          "0",                    &pInst);
                    InsertConfigInteger(pInst,      "Trusted",              1); /* boolean */
                    hrc = pBusMgr->assignPCIDevice(pszAudioDevice, pInst);              H();
                    InsertConfigNode(pInst,         "Config",               &pCfg);
                    switch (enmAudioCodec)
                    {
                        case AudioCodecType_STAC9700:
                            InsertConfigString(pCfg, "Codec",               "STAC9700");
                            break;
                        case AudioCodecType_AD1980:
                            InsertConfigString(pCfg, "Codec",               "AD1980");
                            break;
                        default: AssertFailedBreak();
                    }
                    if (uTimerHz)
                        InsertConfigInteger(pCfg,   "TimerHz",              uTimerHz);
                    if (uBufSizeInMs)
                        InsertConfigInteger(pCfg,   "BufSizeInMs",          uBufSizeInMs);
                    if (uBufSizeOutMs)
                        InsertConfigInteger(pCfg,   "BufSizeOutMs",         uBufSizeOutMs);
                    InsertConfigInteger(pCfg,       "DebugEnabled",         fDebugEnabled);
                    if (strDebugPathOut.isNotEmpty())
                        InsertConfigString(pCfg,    "DebugPathOut",         strDebugPathOut);
                    break;
                }
                case AudioControllerType_SB16:
                {
                    /* Legacy SoundBlaster16. */
                    pszAudioDevice = "sb16";

                    InsertConfigNode(pDevices,      pszAudioDevice,         &pDev);
                    InsertConfigNode(pDev,          "0", &pInst);
                    InsertConfigInteger(pInst,      "Trusted",              1); /* boolean */
                    InsertConfigNode(pInst,         "Config",               &pCfg);
                    InsertConfigInteger(pCfg,       "IRQ",                  5);
                    InsertConfigInteger(pCfg,       "DMA",                  1);
                    InsertConfigInteger(pCfg,       "DMA16",                5);
                    InsertConfigInteger(pCfg,       "Port",                 0x220);
                    InsertConfigInteger(pCfg,       "Version",              0x0405);
                    if (uTimerHz)
                        InsertConfigInteger(pCfg,   "TimerHz",              uTimerHz);
                    InsertConfigInteger(pCfg,       "DebugEnabled",         fDebugEnabled);
                    if (strDebugPathOut.isNotEmpty())
                        InsertConfigString(pCfg,    "DebugPathOut",         strDebugPathOut);
                    break;
                }
                case AudioControllerType_HDA:
                {
                    /* Intel HD Audio. */
                    pszAudioDevice = "hda";

                    InsertConfigNode(pDevices,      pszAudioDevice,         &pDev);
                    InsertConfigNode(pDev,          "0",                    &pInst);
                    InsertConfigInteger(pInst,      "Trusted",              1); /* boolean */
                    hrc = pBusMgr->assignPCIDevice(pszAudioDevice, pInst);              H();
                    InsertConfigNode(pInst,         "Config",               &pCfg);
                    if (uBufSizeInMs)
                        InsertConfigInteger(pCfg,   "BufSizeInMs",          uBufSizeInMs);
                    if (uBufSizeOutMs)
                        InsertConfigInteger(pCfg,   "BufSizeOutMs",         uBufSizeOutMs);
                    InsertConfigInteger(pCfg,       "DebugEnabled",         fDebugEnabled);
                    if (strDebugPathOut.isNotEmpty())
                        InsertConfigString(pCfg,    "DebugPathOut",         strDebugPathOut);

                    /* macOS guests uses a different HDA variant to make 10.14+ (or maybe 10.13?) recognize the device. */
                    if (fOsXGuest)
                        InsertConfigString(pCfg,    "DeviceName",           "Intel Sunrise Point");
                    break;
                }
                default:
                    pszAudioDevice = "oops";
                    AssertFailedBreak();
            }

            PCFGMNODE pCfgAudioAdapter = NULL;
            InsertConfigNode(pInst, "AudioConfig", &pCfgAudioAdapter);
            SafeArray<BSTR> audioProps;
            hrc = audioAdapter->COMGETTER(PropertiesList)(ComSafeArrayAsOutParam(audioProps));  H();

            std::list<Utf8Str> audioPropertyNamesList;
            for (size_t i = 0; i < audioProps.size(); ++i)
            {
                Bstr bstrValue;
                audioPropertyNamesList.push_back(Utf8Str(audioProps[i]));
                hrc = audioAdapter->GetProperty(audioProps[i], bstrValue.asOutParam());
                Utf8Str strKey(audioProps[i]);
                InsertConfigString(pCfgAudioAdapter, strKey.c_str(), bstrValue);
            }

            /*
             * The audio driver.
             */
            const char *pszAudioDriver = NULL;
#ifdef VBOX_WITH_AUDIO_VALIDATIONKIT
            if (fValKitEnabled)
            {
                pszAudioDriver = "ValidationKitAudio";
                LogRel(("Audio: ValidationKit driver active\n"));
            }
#endif
            /* If nothing else was selected before, ask the API. */
            if (pszAudioDriver == NULL)
            {
                AudioDriverType_T enmAudioDriver;
                hrc = audioAdapter->COMGETTER(AudioDriver)(&enmAudioDriver);                    H();

                /* The "Default" audio driver needs special treatment, as we need to figure out which driver to use
                 * by default on the current platform. */
                bool const fUseDefaultDrv = enmAudioDriver == AudioDriverType_Default;

                AudioDriverType_T const enmDefaultAudioDriver = settings::MachineConfigFile::getHostDefaultAudioDriver();

                if (fUseDefaultDrv)
                {
                    enmAudioDriver = enmDefaultAudioDriver;
                    if (enmAudioDriver == AudioDriverType_Null)
                        LogRel(("Audio: Warning: No default driver detected for current platform -- defaulting to Null audio backend\n"));
                }

                switch (enmAudioDriver)
                {
                    case AudioDriverType_Default: /* Can't happen, but handle it anyway. */
                        RT_FALL_THROUGH();
                    case AudioDriverType_Null:
                        pszAudioDriver = "NullAudio";
                        break;
#ifdef RT_OS_WINDOWS
# ifdef VBOX_WITH_WINMM
                    case AudioDriverType_WinMM:
#  error "Port WinMM audio backend!" /** @todo Still needed? */
                        break;
# endif
                    case AudioDriverType_DirectSound:
                        /* Use the Windows Audio Session (WAS) API rather than Direct Sound on Windows
                           versions we've tested it on (currently W7+).  Since Vista, Direct Sound has
                           been emulated on top of WAS according to the docs, so better use WAS directly.

                           Set extradata value "VBoxInternal2/Audio/WindowsDrv" "dsound" to no use WasAPI.

                           Keep this hack for backwards compatibility (introduced < 7.0).
                        */
                        GetExtraDataBoth(virtualBox, pMachine, "VBoxInternal2/Audio/WindowsDrv", &strTmp); H();
                        if (   enmDefaultAudioDriver == AudioDriverType_WAS
                            && (   strTmp.isEmpty()
                                || strTmp.equalsIgnoreCase("was")
                                || strTmp.equalsIgnoreCase("wasapi")) )
                        {
                            /* Nothing to do here, fall through to WAS driver. */
                        }
                        else
                        {
                            pszAudioDriver = "DSoundAudio";
                            break;
                        }
                        RT_FALL_THROUGH();
                    case AudioDriverType_WAS:
                        if (enmDefaultAudioDriver == AudioDriverType_WAS) /* WAS supported? */
                            pszAudioDriver = "HostAudioWas";
                        else if (enmDefaultAudioDriver == AudioDriverType_DirectSound)
                        {
                            LogRel(("Audio: Warning: Windows Audio Session (WAS) not supported, defaulting to DirectSound backend\n"));
                            pszAudioDriver = "DSoundAudio";
                        }
                        break;
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_SOLARIS
                    case AudioDriverType_SolAudio:
                        /* Should not happen, as the Solaris Audio backend is not around anymore.
                         * Remove this sometime later. */
                        LogRel(("Audio: Warning: Solaris Audio is deprecated, please switch to OSS!\n"));
                        LogRel(("Audio: Automatically setting host audio backend to OSS\n"));

                        /* Manually set backend to OSS for now. */
                        pszAudioDriver = "OSSAudio";
                        break;
#endif
#ifdef VBOX_WITH_AUDIO_OSS
                    case AudioDriverType_OSS:
                        pszAudioDriver = "OSSAudio";
                        break;
#endif
#ifdef VBOX_WITH_AUDIO_ALSA
                    case AudioDriverType_ALSA:
                        pszAudioDriver = "ALSAAudio";
                        break;
#endif
#ifdef VBOX_WITH_AUDIO_PULSE
                    case AudioDriverType_Pulse:
                        pszAudioDriver = "PulseAudio";
                        break;
#endif
#ifdef RT_OS_DARWIN
                    case AudioDriverType_CoreAudio:
                        pszAudioDriver = "CoreAudio";
                        break;
#endif
                    default:
                        pszAudioDriver = "oops";
                        AssertFailedBreak();
                }

                if (fUseDefaultDrv)
                    LogRel(("Audio: Detected default audio driver type is '%s'\n", pszAudioDriver));
            }

            BOOL fAudioEnabledIn = FALSE;
            hrc = audioAdapter->COMGETTER(EnabledIn)(&fAudioEnabledIn);                            H();
            BOOL fAudioEnabledOut = FALSE;
            hrc = audioAdapter->COMGETTER(EnabledOut)(&fAudioEnabledOut);                          H();

            unsigned idxAudioLun = 0;

            InsertConfigNodeF(pInst, &pLunL0, "LUN#%u", idxAudioLun);
            i_configAudioDriver(virtualBox, pMachine, pLunL0, pszAudioDriver, !!fAudioEnabledIn, !!fAudioEnabledOut);
            idxAudioLun++;

#ifdef VBOX_WITH_AUDIO_VRDE
            /* Insert dummy audio driver to have the LUN configured. */
            InsertConfigNodeF(pInst, &pLunL0, "LUN#%u", idxAudioLun);
            InsertConfigString(pLunL0, "Driver", "AUDIO");
            {
                AudioDriverCfg DrvCfgVRDE(pszAudioDevice, 0 /* Instance */, idxAudioLun, "AudioVRDE",
                                          !!fAudioEnabledIn, !!fAudioEnabledOut);
                vrc = mAudioVRDE->InitializeConfig(&DrvCfgVRDE);
                AssertRCStmt(vrc, throw ConfigError(__FUNCTION__, vrc, "mAudioVRDE->InitializeConfig failed"));
            }
            idxAudioLun++;
#endif

#ifdef VBOX_WITH_AUDIO_RECORDING
            /* Insert dummy audio driver to have the LUN configured. */
            InsertConfigNodeF(pInst, &pLunL0, "LUN#%u", idxAudioLun);
            InsertConfigString(pLunL0, "Driver", "AUDIO");
            {
                AudioDriverCfg DrvCfgVideoRec(pszAudioDevice, 0 /* Instance */, idxAudioLun, "AudioVideoRec",
                                              false /*a_fEnabledIn*/, true /*a_fEnabledOut*/);
                vrc = mRecording.mAudioRec->InitializeConfig(&DrvCfgVideoRec);
                AssertRCStmt(vrc, throw ConfigError(__FUNCTION__, vrc, "Recording.mAudioRec->InitializeConfig failed"));
            }
            idxAudioLun++;
#endif

            if (fDebugEnabled)
            {
#ifdef VBOX_WITH_AUDIO_DEBUG
# ifdef VBOX_WITH_AUDIO_VALIDATIONKIT
                /*
                 * When both, ValidationKit and Debug mode (for audio) are enabled,
                 * skip configuring the Debug audio driver, as both modes can
                 * mess with the audio data and would lead to side effects.
                 *
                 * The ValidationKit audio driver has precedence over the Debug audio driver.
                 *
                 * This also can (and will) be used in VBox release builds.
                 */
                if (fValKitEnabled)
                {
                    LogRel(("Audio: Warning: ValidationKit running and Debug mode enabled -- disabling Debug driver\n"));
                }
                else /* Debug mode active -- run both (nice for catching errors / doing development). */
                {
                    /*
                     * The ValidationKit backend.
                     */
                    InsertConfigNodeF(pInst, &pLunL0, "LUN#%u", idxAudioLun);
                    i_configAudioDriver(virtualBox, pMachine, pLunL0, "ValidationKitAudio",
                                        !!fAudioEnabledIn, !!fAudioEnabledOut);
                    idxAudioLun++;
# endif /* VBOX_WITH_AUDIO_VALIDATIONKIT */
                    /*
                     * The Debug audio backend.
                     */
                    InsertConfigNodeF(pInst, &pLunL0, "LUN#%u", idxAudioLun);
                    i_configAudioDriver(virtualBox, pMachine, pLunL0, "DebugAudio",
                                        !!fAudioEnabledIn, !!fAudioEnabledOut);
                    idxAudioLun++;
# ifdef VBOX_WITH_AUDIO_VALIDATIONKIT
                }
# endif /* VBOX_WITH_AUDIO_VALIDATIONKIT */
#endif /* VBOX_WITH_AUDIO_DEBUG */

                /*
                 * Tweak the logging groups.
                 */
                Utf8Str strGroups("drv_audio.e.l.l2.l3.f"
                                  " audio_mixer.e.l.l2.l3.f"
                                  " dev_hda_codec.e.l.l2.l3.f"
                                  " dev_hda.e.l.l2.l3.f"
                                  " dev_ac97.e.l.l2.l3.f"
                                  " dev_sb16.e.l.l2.l3.f");

                LogRel(("Audio: Debug level set to %RU32\n", uDebugLevel));

                switch (uDebugLevel)
                {
                    case 0:
                        strGroups += " drv_host_audio.e.l.l2.l3.f";
                        break;
                    case 1:
                        RT_FALL_THROUGH();
                    case 2:
                        RT_FALL_THROUGH();
                    case 3:
                        strGroups += " drv_host_audio.e.l.l2.l3.f+audio_test.e.l.l2.l3.f";
                        break;
                    case 4:
                        RT_FALL_THROUGH();
                    default:
                        strGroups += " drv_host_audio.e.l.l2.l3.l4.f+audio_test.e.l.l2.l3.l4.f";
                        break;
                }

                vrc = RTLogGroupSettings(RTLogRelGetDefaultInstance(), strGroups.c_str());
                if (RT_FAILURE(vrc))
                    LogRel(("Audio: Setting debug logging failed, vrc=%Rrc\n", vrc));
            }
        }

#ifdef VBOX_WITH_SHARED_CLIPBOARD
        /*
         * Shared Clipboard.
         */
        {
            ClipboardMode_T enmClipboardMode = ClipboardMode_Disabled;
            hrc = pMachine->COMGETTER(ClipboardMode)(&enmClipboardMode); H();
# ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
            BOOL fFileTransfersEnabled;
            hrc = pMachine->COMGETTER(ClipboardFileTransfersEnabled)(&fFileTransfersEnabled); H();
#endif

            /* Load the service */
            vrc = pVMMDev->hgcmLoadService("VBoxSharedClipboard", "VBoxSharedClipboard");
            if (RT_SUCCESS(vrc))
            {
                LogRel(("Shared Clipboard: Service loaded\n"));

                /* Set initial clipboard mode. */
                vrc = i_changeClipboardMode(enmClipboardMode);
                AssertLogRelMsg(RT_SUCCESS(vrc), ("Shared Clipboard: Failed to set initial clipboard mode (%d): vrc=%Rrc\n",
                                                 enmClipboardMode, vrc));

                /* Setup the service. */
                VBOXHGCMSVCPARM parm;
                HGCMSvcSetU32(&parm, !i_useHostClipboard());
                vrc = pVMMDev->hgcmHostCall("VBoxSharedClipboard", VBOX_SHCL_HOST_FN_SET_HEADLESS, 1, &parm);
                AssertLogRelMsg(RT_SUCCESS(vrc), ("Shared Clipboard: Failed to set initial headless mode (%RTbool): vrc=%Rrc\n",
                                                 !i_useHostClipboard(), vrc));

# ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
                vrc = i_changeClipboardFileTransferMode(RT_BOOL(fFileTransfersEnabled));
                AssertLogRelMsg(RT_SUCCESS(vrc), ("Shared Clipboard: Failed to set initial file transfer mode (%u): vrc=%Rrc\n",
                                                 fFileTransfersEnabled, vrc));
# endif
                GuestShCl::createInstance(this /* pConsole */);
                vrc = HGCMHostRegisterServiceExtension(&m_hHgcmSvcExtShCl, "VBoxSharedClipboard",
                                                       &GuestShCl::hgcmDispatcher,
                                                       GuestShClInst());
                if (RT_FAILURE(vrc))
                    Log(("Cannot register VBoxSharedClipboard extension, vrc=%Rrc\n", vrc));
            }
            else
                LogRel(("Shared Clipboard: Not available, vrc=%Rrc\n", vrc));
            vrc = VINF_SUCCESS;  /* None of the potential failures above are fatal. */
        }
#endif /* VBOX_WITH_SHARED_CLIPBOARD */

        /*
         * HGCM HostChannel.
         */
        {
            Bstr value;
            hrc = pMachine->GetExtraData(Bstr("HGCM/HostChannel").raw(),
                                         value.asOutParam());

            if (   hrc   == S_OK
                && value == "1")
            {
                vrc = pVMMDev->hgcmLoadService("VBoxHostChannel", "VBoxHostChannel");
                if (RT_FAILURE(vrc))
                {
                    LogRel(("VBoxHostChannel is not available, vrc=%Rrc\n", vrc));
                    /* That is not a fatal failure. */
                    vrc = VINF_SUCCESS;
                }
            }
        }

#ifdef VBOX_WITH_DRAG_AND_DROP
        /*
         * Drag and Drop.
         */
        {
            DnDMode_T enmMode = DnDMode_Disabled;
            hrc = pMachine->COMGETTER(DnDMode)(&enmMode);                                   H();

            /* Load the service */
            vrc = pVMMDev->hgcmLoadService("VBoxDragAndDropSvc", "VBoxDragAndDropSvc");
            if (RT_FAILURE(vrc))
            {
                LogRel(("Drag and drop service is not available, vrc=%Rrc\n", vrc));
                /* That is not a fatal failure. */
                vrc = VINF_SUCCESS;
            }
            else
            {
                vrc = HGCMHostRegisterServiceExtension(&m_hHgcmSvcExtDragAndDrop, "VBoxDragAndDropSvc",
                                                       &GuestDnD::notifyDnDDispatcher,
                                                       GuestDnDInst());
                if (RT_FAILURE(vrc))
                    Log(("Cannot register VBoxDragAndDropSvc extension, vrc=%Rrc\n", vrc));
                else
                {
                    LogRel(("Drag and drop service loaded\n"));
                    vrc = i_changeDnDMode(enmMode);
                }
            }
        }
#endif /* VBOX_WITH_DRAG_AND_DROP */

#if defined(VBOX_WITH_TPM)
        /*
         * Configure the Trusted Platform Module.
         */
        ComObjPtr<ITrustedPlatformModule> ptrTpm;
        TpmType_T enmTpmType = TpmType_None;

        hrc = pMachine->COMGETTER(TrustedPlatformModule)(ptrTpm.asOutParam());              H();
        hrc = ptrTpm->COMGETTER(Type)(&enmTpmType);                                         H();
        if (enmTpmType != TpmType_None)
        {
            InsertConfigNode(pDevices, "tpm", &pDev);
            InsertConfigNode(pDev,     "0", &pInst);
            InsertConfigInteger(pInst, "Trusted", 1); /* boolean */
            InsertConfigNode(pInst,    "Config", &pCfg);
            InsertConfigNode(pInst,    "LUN#0", &pLunL0);

            switch (enmTpmType)
            {
                case TpmType_v1_2:
                case TpmType_v2_0:
                    InsertConfigString(pLunL0, "Driver",               "TpmEmuTpms");
                    InsertConfigNode(pLunL0,   "Config", &pCfg);
                    InsertConfigInteger(pCfg, "TpmVersion", enmTpmType == TpmType_v1_2 ? 1 : 2);
                    InsertConfigNode(pLunL0, "AttachedDriver", &pLunL1);
                    InsertConfigString(pLunL1, "Driver", "NvramStore");
                    break;
                case TpmType_Host:
#if defined(RT_OS_LINUX) || defined(RT_OS_WINDOWS)
                    InsertConfigString(pLunL0, "Driver",               "TpmHost");
                    InsertConfigNode(pLunL0,   "Config", &pCfg);
#endif
                    break;
                case TpmType_Swtpm:
                    hrc = ptrTpm->COMGETTER(Location)(bstr.asOutParam());                   H();
                    InsertConfigString(pLunL0, "Driver",               "TpmEmu");
                    InsertConfigNode(pLunL0,   "Config", &pCfg);
                    InsertConfigString(pCfg,   "Location", bstr);
                    break;
                default:
                    AssertFailedBreak();
            }
        }
#endif

        /*
         * ACPI
         */
        BOOL fACPI;
        hrc = biosSettings->COMGETTER(ACPIEnabled)(&fACPI);                                 H();
        if (fACPI)
        {
            /* Always show the CPU leafs when we have multiple VCPUs or when the IO-APIC is enabled.
             * The Windows SMP kernel needs a CPU leaf or else its idle loop will burn cpu cycles; the
             * intelppm driver refuses to register an idle state handler.
             * Always show CPU leafs for OS X guests. */
            BOOL fShowCpu = fOsXGuest;
            if (cCpus > 1 || fIOAPIC)
                fShowCpu = true;

            BOOL fCpuHotPlug;
            hrc = pMachine->COMGETTER(CPUHotPlugEnabled)(&fCpuHotPlug);                     H();

            InsertConfigNode(pDevices, "acpi", &pDev);
            InsertConfigNode(pDev,     "0", &pInst);
            InsertConfigInteger(pInst, "Trusted", 1); /* boolean */
            InsertConfigNode(pInst,    "Config", &pCfg);
            hrc = pBusMgr->assignPCIDevice("acpi", pInst);                                  H();

            InsertConfigInteger(pCfg,  "NumCPUs",          cCpus);

            InsertConfigInteger(pCfg,  "IOAPIC", fIOAPIC);
            InsertConfigInteger(pCfg,  "FdcEnabled", fFdcEnabled);
            InsertConfigInteger(pCfg,  "HpetEnabled", fHPETEnabled);
            InsertConfigInteger(pCfg,  "SmcEnabled", fSmcEnabled);
            InsertConfigInteger(pCfg,  "ShowRtc",    fShowRtc);
            if (fOsXGuest && !llBootNics.empty())
            {
                BootNic aNic = llBootNics.front();
                uint32_t u32NicPCIAddr = (aNic.mPCIAddress.miDevice << 16) | aNic.mPCIAddress.miFn;
                InsertConfigInteger(pCfg, "NicPciAddress",    u32NicPCIAddr);
            }
            if (fOsXGuest && fAudioEnabled)
            {
                PCIBusAddress Address;
                if (pBusMgr->findPCIAddress("hda", 0, Address))
                {
                    uint32_t u32AudioPCIAddr = (Address.miDevice << 16) | Address.miFn;
                    InsertConfigInteger(pCfg, "AudioPciAddress",    u32AudioPCIAddr);
                }
            }
            if (fOsXGuest)
            {
                PCIBusAddress Address;
                if (pBusMgr->findPCIAddress("nvme", 0, Address))
                {
                    uint32_t u32NvmePCIAddr = (Address.miDevice << 16) | Address.miFn;
                    InsertConfigInteger(pCfg, "NvmePciAddress",    u32NvmePCIAddr);
                }
            }
            if (enmIommuType == IommuType_AMD)
            {
                PCIBusAddress Address;
                if (pBusMgr->findPCIAddress("iommu-amd", 0, Address))
                {
                    uint32_t u32IommuAddress = (Address.miDevice << 16) | Address.miFn;
                    InsertConfigInteger(pCfg, "IommuAmdEnabled", true);
                    InsertConfigInteger(pCfg, "IommuPciAddress", u32IommuAddress);
                    if (pBusMgr->findPCIAddress("sb-ioapic", 0, Address))
                    {
                        uint32_t const u32SbIoapicAddress = (Address.miDevice << 16) | Address.miFn;
                        InsertConfigInteger(pCfg, "SbIoApicPciAddress", u32SbIoapicAddress);
                    }
                    else
                        return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                                     N_("AMD IOMMU is enabled, but the I/O APIC is not assigned a PCI address!"));
                }
            }
            else if (enmIommuType == IommuType_Intel)
            {
                PCIBusAddress Address;
                if (pBusMgr->findPCIAddress("iommu-intel", 0, Address))
                {
                    uint32_t u32IommuAddress = (Address.miDevice << 16) | Address.miFn;
                    InsertConfigInteger(pCfg, "IommuIntelEnabled", true);
                    InsertConfigInteger(pCfg, "IommuPciAddress", u32IommuAddress);
                    if (pBusMgr->findPCIAddress("sb-ioapic", 0, Address))
                    {
                        uint32_t const u32SbIoapicAddress = (Address.miDevice << 16) | Address.miFn;
                        InsertConfigInteger(pCfg, "SbIoApicPciAddress", u32SbIoapicAddress);
                    }
                    else
                        return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                                     N_("Intel IOMMU is enabled, but the I/O APIC is not assigned a PCI address!"));
                }
            }

            InsertConfigInteger(pCfg,  "IocPciAddress", uIocPCIAddress);
            if (chipsetType == ChipsetType_ICH9)
            {
                InsertConfigInteger(pCfg,  "McfgBase",   uMcfgBase);
                InsertConfigInteger(pCfg,  "McfgLength", cbMcfgLength);
                /* 64-bit prefetch window root resource: Only for ICH9 and if PAE or Long Mode is enabled (@bugref{5454}). */
                if (fIsGuest64Bit || fEnablePAE)
                    InsertConfigInteger(pCfg,  "PciPref64Enabled", 1);
            }
            InsertConfigInteger(pCfg,  "HostBusPciAddress", uHbcPCIAddress);
            InsertConfigInteger(pCfg,  "ShowCpu", fShowCpu);
            InsertConfigInteger(pCfg,  "CpuHotPlug", fCpuHotPlug);

            InsertConfigInteger(pCfg,  "Serial0IoPortBase", auSerialIoPortBase[0]);
            InsertConfigInteger(pCfg,  "Serial0Irq", auSerialIrq[0]);

            InsertConfigInteger(pCfg,  "Serial1IoPortBase", auSerialIoPortBase[1]);
            InsertConfigInteger(pCfg,  "Serial1Irq", auSerialIrq[1]);

            if (auSerialIoPortBase[2])
            {
                InsertConfigInteger(pCfg,  "Serial2IoPortBase", auSerialIoPortBase[2]);
                InsertConfigInteger(pCfg,  "Serial2Irq", auSerialIrq[2]);
            }

            if (auSerialIoPortBase[3])
            {
                InsertConfigInteger(pCfg,  "Serial3IoPortBase", auSerialIoPortBase[3]);
                InsertConfigInteger(pCfg,  "Serial3Irq", auSerialIrq[3]);
            }

            InsertConfigInteger(pCfg,  "Parallel0IoPortBase", auParallelIoPortBase[0]);
            InsertConfigInteger(pCfg,  "Parallel0Irq", auParallelIrq[0]);

            InsertConfigInteger(pCfg,  "Parallel1IoPortBase", auParallelIoPortBase[1]);
            InsertConfigInteger(pCfg,  "Parallel1Irq", auParallelIrq[1]);

#if defined(VBOX_WITH_TPM)
            switch (enmTpmType)
            {
                case TpmType_v1_2:
                    InsertConfigString(pCfg, "TpmMode", "tis1.2");
                    break;
                case TpmType_v2_0:
                    InsertConfigString(pCfg, "TpmMode", "fifo2.0");
                    break;
                /** @todo Host and swtpm. */
                default:
                    break;
            }
#endif

            InsertConfigNode(pInst,    "LUN#0", &pLunL0);
            InsertConfigString(pLunL0, "Driver",               "ACPIHost");
            InsertConfigNode(pLunL0,   "Config", &pCfg);

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
                    InsertConfigNode(pInst, Utf8StrFmt("LUN#%u", iCpuCurr).c_str(), &pLunL0);
                    InsertConfigString(pLunL0, "Driver",           "ACPICpu");
                    InsertConfigNode(pLunL0,   "Config", &pCfg);
                }
            }
        }

        /*
         * Configure DBGF (Debug(ger) Facility) and DBGC (Debugger Console).
         */
        {
            PCFGMNODE pDbgf;
            InsertConfigNode(pRoot, "DBGF", &pDbgf);

            /* Paths to search for debug info and such things. */
            hrc = pMachine->COMGETTER(SettingsFilePath)(bstr.asOutParam());                 H();
            Utf8Str strSettingsPath(bstr);
            bstr.setNull();
            strSettingsPath.stripFilename();
            strSettingsPath.append("/");

            char szHomeDir[RTPATH_MAX + 1];
            int vrc2 = RTPathUserHome(szHomeDir, sizeof(szHomeDir) - 1);
            if (RT_FAILURE(vrc2))
                szHomeDir[0] = '\0';
            RTPathEnsureTrailingSeparator(szHomeDir, sizeof(szHomeDir));


            Utf8Str strPath;
            strPath.append(strSettingsPath).append("debug/;");
            strPath.append(strSettingsPath).append(";");
            strPath.append("cache*").append(strSettingsPath).append("dbgcache/;"); /* handy for symlinking to actual cache */
            strPath.append(szHomeDir);

            InsertConfigString(pDbgf, "Path", strPath.c_str());

            /* Tracing configuration. */
            BOOL fTracingEnabled;
            hrc = pMachine->COMGETTER(TracingEnabled)(&fTracingEnabled);                    H();
            if (fTracingEnabled)
                InsertConfigInteger(pDbgf, "TracingEnabled", 1);

            hrc = pMachine->COMGETTER(TracingConfig)(bstr.asOutParam());                    H();
            if (fTracingEnabled)
                InsertConfigString(pDbgf, "TracingConfig", bstr);

            BOOL fAllowTracingToAccessVM;
            hrc = pMachine->COMGETTER(AllowTracingToAccessVM)(&fAllowTracingToAccessVM);    H();
            if (fAllowTracingToAccessVM)
                InsertConfigInteger(pPDM, "AllowTracingToAccessVM", 1);

            /* Debugger console config. */
            PCFGMNODE pDbgc;
            InsertConfigNode(pRoot, "DBGC", &pDbgc);

            hrc = virtualBox->COMGETTER(HomeFolder)(bstr.asOutParam());                     H();
            Utf8Str strVBoxHome = bstr;
            bstr.setNull();
            if (strVBoxHome.isNotEmpty())
                strVBoxHome.append("/");
            else
            {
                strVBoxHome = szHomeDir;
                strVBoxHome.append("/.vbox");
            }

            Utf8Str strFile(strVBoxHome);
            strFile.append("dbgc-history");
            InsertConfigString(pDbgc, "HistoryFile", strFile);

            strFile = strSettingsPath;
            strFile.append("dbgc-init");
            InsertConfigString(pDbgc, "LocalInitScript", strFile);

            strFile = strVBoxHome;
            strFile.append("dbgc-init");
            InsertConfigString(pDbgc, "GlobalInitScript", strFile);

            /*
             * Configure guest debug settings.
             */
            ComObjPtr<IGuestDebugControl> ptrGstDbgCtrl;
            GuestDebugProvider_T enmGstDbgProvider = GuestDebugProvider_None;

            hrc = pMachine->COMGETTER(GuestDebugControl)(ptrGstDbgCtrl.asOutParam());           H();
            hrc = ptrGstDbgCtrl->COMGETTER(DebugProvider)(&enmGstDbgProvider);                  H();
            if (enmGstDbgProvider != GuestDebugProvider_None)
            {
                GuestDebugIoProvider_T enmGstDbgIoProvider = GuestDebugIoProvider_None;
                hrc = ptrGstDbgCtrl->COMGETTER(DebugIoProvider)(&enmGstDbgIoProvider);          H();
                hrc = ptrGstDbgCtrl->COMGETTER(DebugAddress)(bstr.asOutParam());                H();
                Utf8Str strAddress = bstr;
                bstr.setNull();

                ULONG ulPort = 0;
                hrc = ptrGstDbgCtrl->COMGETTER(DebugPort)(&ulPort);                             H();

                PCFGMNODE pDbgSettings;
                InsertConfigNode(pDbgc, "Dbg", &pDbgSettings);
                InsertConfigString(pDbgSettings, "Address", strAddress);
                InsertConfigInteger(pDbgSettings, "Port", ulPort);

                switch (enmGstDbgProvider)
                {
                    case GuestDebugProvider_Native:
                        InsertConfigString(pDbgSettings, "StubType", "Native");
                        break;
                    case GuestDebugProvider_GDB:
                        InsertConfigString(pDbgSettings, "StubType", "Gdb");
                        break;
                    case GuestDebugProvider_KD:
                        InsertConfigString(pDbgSettings, "StubType", "Kd");
                        break;
                    default:
                        AssertFailed();
                        break;
                }

                switch (enmGstDbgIoProvider)
                {
                    case GuestDebugIoProvider_TCP:
                        InsertConfigString(pDbgSettings, "Provider", "tcp");
                        break;
                    case GuestDebugIoProvider_UDP:
                        InsertConfigString(pDbgSettings, "Provider", "udp");
                        break;
                    case GuestDebugIoProvider_IPC:
                        InsertConfigString(pDbgSettings, "Provider", "ipc");
                        break;
                    default:
                        AssertFailed();
                        break;
                }
            }
        }
    }
    catch (ConfigError &x)
    {
        // InsertConfig threw something:
        pVMM->pfnVMR3SetError(pUVM, x.m_vrc, RT_SRC_POS, "Caught ConfigError: %Rrc - %s", x.m_vrc, x.what());
        return x.m_vrc;
    }
    catch (HRESULT hrcXcpt)
    {
        AssertLogRelMsgFailedReturn(("hrc=%Rhrc\n", hrcXcpt), VERR_MAIN_CONFIG_CONSTRUCTOR_COM_ERROR);
    }

#ifdef VBOX_WITH_EXTPACK
    /*
     * Call the extension pack hooks if everything went well thus far.
     */
    if (RT_SUCCESS(vrc))
    {
        pAlock->release();
        vrc = mptrExtPackManager->i_callAllVmConfigureVmmHooks(this, pVM, pVMM);
        pAlock->acquire();
    }
#endif

    /*
     * Apply the CFGM overlay.
     */
    if (RT_SUCCESS(vrc))
        vrc = i_configCfgmOverlay(pRoot, virtualBox, pMachine);

    /*
     * Dump all extradata API settings tweaks, both global and per VM.
     */
    if (RT_SUCCESS(vrc))
        vrc = i_configDumpAPISettingsTweaks(virtualBox, pMachine);

#undef H

    pAlock->release(); /* Avoid triggering the lock order inversion check. */

    /*
     * Register VM state change handler.
     */
    int vrc2 = pVMM->pfnVMR3AtStateRegister(pUVM, Console::i_vmstateChangeCallback, this);
    AssertRC(vrc2);
    if (RT_SUCCESS(vrc))
        vrc = vrc2;

    /*
     * Register VM runtime error handler.
     */
    vrc2 = pVMM->pfnVMR3AtRuntimeErrorRegister(pUVM, Console::i_atVMRuntimeErrorCallback, this);
    AssertRC(vrc2);
    if (RT_SUCCESS(vrc))
        vrc = vrc2;

    pAlock->acquire();

    LogFlowFunc(("vrc = %Rrc\n", vrc));
    LogFlowFuncLeave();

    return vrc;
}


int Console::i_configGraphicsController(PCFGMNODE pDevices,
                                        const GraphicsControllerType_T enmGraphicsController,
                                        BusAssignmentManager *pBusMgr,
                                        const ComPtr<IMachine> &ptrMachine,
                                        const ComPtr<IGraphicsAdapter> &ptrGraphicsAdapter,
                                        const ComPtr<IBIOSSettings> &ptrBiosSettings,
                                        bool fHMEnabled)
{
    // InsertConfig* throws
    try
    {
        PCFGMNODE pDev, pInst, pCfg, pLunL0;
        HRESULT hrc;
        Bstr    bstr;
        const char *pcszDevice = "vga";

#define H()         AssertLogRelMsgReturn(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), VERR_MAIN_CONFIG_CONSTRUCTOR_COM_ERROR)
        InsertConfigNode(pDevices, pcszDevice, &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigInteger(pInst, "Trusted",              1); /* boolean */

        hrc = pBusMgr->assignPCIDevice(pcszDevice, pInst);                                  H();
        InsertConfigNode(pInst,    "Config", &pCfg);
        ULONG cVRamMBs;
        hrc = ptrGraphicsAdapter->COMGETTER(VRAMSize)(&cVRamMBs);                           H();
        InsertConfigInteger(pCfg,  "VRamSize",             cVRamMBs * _1M);
        ULONG cMonitorCount;
        hrc = ptrGraphicsAdapter->COMGETTER(MonitorCount)(&cMonitorCount);                  H();
        InsertConfigInteger(pCfg,  "MonitorCount",         cMonitorCount);
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        InsertConfigInteger(pCfg,  "R0Enabled",            fHMEnabled);
#else
        NOREF(fHMEnabled);
#endif
        BOOL f3DEnabled;
        hrc = ptrGraphicsAdapter->COMGETTER(Accelerate3DEnabled)(&f3DEnabled);              H();
        InsertConfigInteger(pCfg,  "3DEnabled",            f3DEnabled);

        i_attachStatusDriver(pInst, DeviceType_Graphics3D);

#ifdef VBOX_WITH_VMSVGA
        if (   enmGraphicsController == GraphicsControllerType_VMSVGA
            || enmGraphicsController == GraphicsControllerType_VBoxSVGA)
        {
            InsertConfigInteger(pCfg, "VMSVGAEnabled", true);
            if (enmGraphicsController == GraphicsControllerType_VMSVGA)
            {
                InsertConfigInteger(pCfg, "VMSVGAPciBarLayout", true);
                InsertConfigInteger(pCfg, "VMSVGAPciId", true);
            }
# ifdef VBOX_WITH_VMSVGA3D
            InsertConfigInteger(pCfg, "VMSVGA3dEnabled", f3DEnabled);
# else
            LogRel(("VMSVGA3d not available in this build!\n"));
# endif /* VBOX_WITH_VMSVGA3D */
        }
#else
        RT_NOREF(enmGraphicsController);
#endif /* VBOX_WITH_VMSVGA */

        /* Custom VESA mode list */
        unsigned cModes = 0;
        for (unsigned iMode = 1; iMode <= 16; ++iMode)
        {
            char szExtraDataKey[sizeof("CustomVideoModeXX")];
            RTStrPrintf(szExtraDataKey, sizeof(szExtraDataKey), "CustomVideoMode%u", iMode);
            hrc = ptrMachine->GetExtraData(Bstr(szExtraDataKey).raw(), bstr.asOutParam());  H();
            if (bstr.isEmpty())
                break;
            InsertConfigString(pCfg, szExtraDataKey, bstr);
            ++cModes;
        }
        InsertConfigInteger(pCfg, "CustomVideoModes", cModes);

        /* VESA height reduction */
        ULONG ulHeightReduction;
        IFramebuffer *pFramebuffer = NULL;
        hrc = i_getDisplay()->QueryFramebuffer(0, &pFramebuffer);
        if (SUCCEEDED(hrc) && pFramebuffer)
        {
            hrc = pFramebuffer->COMGETTER(HeightReduction)(&ulHeightReduction);             H();
            pFramebuffer->Release();
            pFramebuffer = NULL;
        }
        else
        {
            /* If framebuffer is not available, there is no height reduction. */
            ulHeightReduction = 0;
        }
        InsertConfigInteger(pCfg,  "HeightReduction", ulHeightReduction);

        /*
         * BIOS logo
         */
        BOOL fFadeIn;
        hrc = ptrBiosSettings->COMGETTER(LogoFadeIn)(&fFadeIn);                             H();
        InsertConfigInteger(pCfg,  "FadeIn",  fFadeIn ? 1 : 0);
        BOOL fFadeOut;
        hrc = ptrBiosSettings->COMGETTER(LogoFadeOut)(&fFadeOut);                           H();
        InsertConfigInteger(pCfg,  "FadeOut", fFadeOut ? 1: 0);
        ULONG logoDisplayTime;
        hrc = ptrBiosSettings->COMGETTER(LogoDisplayTime)(&logoDisplayTime);                H();
        InsertConfigInteger(pCfg,  "LogoTime", logoDisplayTime);
        Bstr bstrLogoImagePath;
        hrc = ptrBiosSettings->COMGETTER(LogoImagePath)(bstrLogoImagePath.asOutParam());    H();
        InsertConfigString(pCfg,   "LogoFile", bstrLogoImagePath);

        /*
         * Boot menu
         */
        BIOSBootMenuMode_T eBootMenuMode;
        int iShowBootMenu;
        hrc = ptrBiosSettings->COMGETTER(BootMenuMode)(&eBootMenuMode);                     H();
        switch (eBootMenuMode)
        {
            case BIOSBootMenuMode_Disabled: iShowBootMenu = 0;  break;
            case BIOSBootMenuMode_MenuOnly: iShowBootMenu = 1;  break;
            default:                        iShowBootMenu = 2;  break;
        }
        InsertConfigInteger(pCfg, "ShowBootMenu", iShowBootMenu);

        /* Attach the display. */
        InsertConfigNode(pInst,    "LUN#0", &pLunL0);
        InsertConfigString(pLunL0, "Driver",               "MainDisplay");
        InsertConfigNode(pLunL0,   "Config", &pCfg);
    }
    catch (ConfigError &x)
    {
        // InsertConfig threw something:
        return x.m_vrc;
    }

#undef H

    return VINF_SUCCESS;
}
