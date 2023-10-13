/* $Id$ */
/** @file
 * VBox Console COM Class implementation - VM Configuration Bits for ARMv8.
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
#define LOG_GROUP LOG_GROUP_MAIN_CONSOLE
#include "LoggingNew.h"

#include "ConsoleImpl.h"
#include "ResourceStoreImpl.h"
#include "Global.h"
#include "VMMDev.h"

// generated header
#include "SchemaDefs.h"

#include "AutoCaller.h"

#include <iprt/base64.h>
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/fdt.h>
#include <iprt/file.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/system.h>
#if 0 /* enable to play with lots of memory. */
# include <iprt/env.h>
#endif
#include <iprt/stream.h>

#include <iprt/formats/arm-psci.h>

#include <VBox/vmm/vmmr3vtable.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/version.h>
#include <VBox/platforms/vbox-armv8.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD
# include <VBox/HostServices/VBoxClipboardSvc.h>
#endif
#ifdef VBOX_WITH_DRAG_AND_DROP
# include "GuestImpl.h"
# include "GuestDnDPrivate.h"
#endif

#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/* Darwin compile kludge */
#undef PVM

#ifdef VBOX_WITH_VIRT_ARMV8
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
 *
 * @todo This is a big hack at the moment and provides a static VM config to work with, will be adjusted later
 *       on to adhere to the VM config when sorting out the API bits.
 */
int Console::i_configConstructorArmV8(PUVM pUVM, PVM pVM, PCVMMR3VTABLE pVMM, AutoWriteLock *pAlock)
{
    RT_NOREF(pVM /* when everything is disabled */);
    VMMDev         *pVMMDev   = m_pVMMDev; Assert(pVMMDev);
    ComPtr<IMachine> pMachine = i_machine();

    HRESULT         hrc;
    Utf8Str         strTmp;
    Bstr            bstr;

    RTFDT hFdt = NIL_RTFDT;
    int vrc = RTFdtCreateEmpty(&hFdt);
    AssertRCReturn(vrc, vrc);

#define H()         AssertLogRelMsgReturnStmt(!FAILED(hrc), ("hrc=%Rhrc\n", hrc), RTFdtDestroy(hFdt), VERR_MAIN_CONFIG_CONSTRUCTOR_COM_ERROR)
#define VRC()       AssertLogRelMsgReturnStmt(RT_SUCCESS(vrc), ("vrc=%Rrc\n", vrc), RTFdtDestroy(hFdt), vrc)

    /** @todo Find a way to figure it out before CPUM is set up, can't use CPUMGetGuestAddrWidths() and on macOS we need
     * access to Hypervisor.framework to query the ID registers (Linux can in theory parse /proc/cpuinfo, no idea for Windows). */
    RTGCPHYS GCPhysTopOfAddrSpace = RT_BIT_64(36);

    /*
     * Get necessary objects and frequently used parameters.
     */
    ComPtr<IVirtualBox> virtualBox;
    hrc = pMachine->COMGETTER(Parent)(virtualBox.asOutParam());                             H();

    ComPtr<IHost> host;
    hrc = virtualBox->COMGETTER(Host)(host.asOutParam());                                   H();

    ComPtr<ISystemProperties> systemProperties;
    hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());           H();

    ComPtr<IFirmwareSettings> firmwareSettings;
    hrc = pMachine->COMGETTER(FirmwareSettings)(firmwareSettings.asOutParam());             H();

    ComPtr<INvramStore> nvramStore;
    hrc = pMachine->COMGETTER(NonVolatileStore)(nvramStore.asOutParam());                   H();

    hrc = pMachine->COMGETTER(HardwareUUID)(bstr.asOutParam());                             H();
    RTUUID HardwareUuid;
    vrc = RTUuidFromUtf16(&HardwareUuid, bstr.raw());
    AssertRCReturn(vrc, vrc);

    ULONG cRamMBs;
    hrc = pMachine->COMGETTER(MemorySize)(&cRamMBs);                                        H();
    uint64_t const cbRam   = cRamMBs * (uint64_t)_1M;

    ComPtr<IPlatform> pPlatform;
    hrc = pMachine->COMGETTER(Platform)(pPlatform.asOutParam());                            H();

    ComPtr<IPlatformProperties> pPlatformProperties;
    hrc = pPlatform->COMGETTER(Properties)(pPlatformProperties.asOutParam());               H();

    ChipsetType_T chipsetType;
    hrc = pPlatform->COMGETTER(ChipsetType)(&chipsetType);                                  H();

    ULONG cCpus = 1;
    hrc = pMachine->COMGETTER(CPUCount)(&cCpus);                                            H();
    Assert(cCpus);

    ULONG ulCpuExecutionCap = 100;
    hrc = pMachine->COMGETTER(CPUExecutionCap)(&ulCpuExecutionCap);                         H();

    LogRel(("Guest architecture: ARM\n"));

    Bstr osTypeId;
    hrc = pMachine->COMGETTER(OSTypeId)(osTypeId.asOutParam());                             H();
    LogRel(("Guest OS type: '%s'\n", Utf8Str(osTypeId).c_str()));

    ULONG maxNetworkAdapters;
    hrc = pPlatformProperties->GetMaxNetworkAdapters(chipsetType, &maxNetworkAdapters);     H();

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
        InsertConfigInteger(pRoot, "NumCPUs",              cCpus);
        InsertConfigInteger(pRoot, "CpuExecutionCap",      ulCpuExecutionCap);
        InsertConfigInteger(pRoot, "TimerMillies",         10);

        uint32_t idPHandleIntCtrl = RTFdtPHandleAllocate(hFdt);
        Assert(idPHandleIntCtrl != UINT32_MAX);
        uint32_t idPHandleIntCtrlMsi = RTFdtPHandleAllocate(hFdt);
        Assert(idPHandleIntCtrlMsi != UINT32_MAX); RT_NOREF(idPHandleIntCtrlMsi);
        uint32_t idPHandleAbpPClk = RTFdtPHandleAllocate(hFdt);
        Assert(idPHandleAbpPClk != UINT32_MAX);
        uint32_t idPHandleGpio = RTFdtPHandleAllocate(hFdt);
        Assert(idPHandleGpio != UINT32_MAX);

        uint32_t aidPHandleCpus[VMM_MAX_CPU_COUNT];
        for (uint32_t i = 0; i < cCpus; i++)
        {
            aidPHandleCpus[i] = RTFdtPHandleAllocate(hFdt);
            Assert(aidPHandleCpus[i] != UINT32_MAX);
        }

        vrc = RTFdtNodePropertyAddU32(   hFdt, "interrupt-parent", idPHandleIntCtrl);       VRC();
        vrc = RTFdtNodePropertyAddString(hFdt, "model",            "linux,dummy-virt");     VRC();
        vrc = RTFdtNodePropertyAddU32(   hFdt, "#size-cells",      2);                      VRC();
        vrc = RTFdtNodePropertyAddU32(   hFdt, "#address-cells",   2);                      VRC();
        vrc = RTFdtNodePropertyAddString(hFdt, "compatible",       "linux,dummy-virt");     VRC();

        /* Configure the Power State Coordination Interface. */
        vrc = RTFdtNodeAdd(hFdt, "psci");                                                   VRC();
        vrc = RTFdtNodePropertyAddU32(   hFdt, "migrate",          ARM_PSCI_FUNC_ID_CREATE_FAST_32(ARM_PSCI_FUNC_ID_MIGRATE));             VRC();
        vrc = RTFdtNodePropertyAddU32(   hFdt, "cpu_on",           ARM_PSCI_FUNC_ID_CREATE_FAST_32(ARM_PSCI_FUNC_ID_CPU_ON));              VRC();
        vrc = RTFdtNodePropertyAddU32(   hFdt, "cpu_off",          ARM_PSCI_FUNC_ID_CREATE_FAST_32(ARM_PSCI_FUNC_ID_CPU_OFF));             VRC();
        vrc = RTFdtNodePropertyAddU32(   hFdt, "cpu_suspend",      ARM_PSCI_FUNC_ID_CREATE_FAST_32(ARM_PSCI_FUNC_ID_CPU_SUSPEND));         VRC();
        vrc = RTFdtNodePropertyAddString(hFdt, "method",           "hvc");                  VRC();
        vrc = RTFdtNodePropertyAddStringList(hFdt, "compatible",   3,
                                             "arm,psci-1.0", "arm,psci-0.2", "arm,psci");   VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        /* Configure some misc system wide properties. */
        vrc = RTFdtNodeAdd(hFdt, "chosen");                                                 VRC();
        vrc = RTFdtNodePropertyAddString(hFdt, "stdout-path",      "/pl011@9000000");       VRC();
        vrc = RTFdtNodeFinalize(hFdt);

        /* Configure the timer and clock. */
        vrc = RTFdtNodeAdd(hFdt, "timer");                                                  VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "interrupts", 12,
                                           0x01, 0x0d, 0x104,
                                           0x01, 0x0e, 0x104,
                                           0x01, 0x0b, 0x104,
                                           0x01, 0x0a, 0x104);                              VRC();
        vrc = RTFdtNodePropertyAddEmpty(   hFdt, "always-on");                              VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "compatible",       "arm,armv7-timer");    VRC();
        vrc = RTFdtNodeFinalize(hFdt);

        vrc = RTFdtNodeAdd(hFdt, "apb-clk");                                                VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "phandle", idPHandleAbpPClk);              VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "clock-output-names", "clk24mhz");         VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "clock-frequency",    24 * 1000 * 1000);   VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#clock-cells",       0);                  VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "compatible",         "fixed-clock");      VRC();
        vrc = RTFdtNodeFinalize(hFdt);

        /* Configure gpio keys (non functional at the moment). */
        vrc = RTFdtNodeAdd(hFdt, "gpio-keys");                                              VRC();
        vrc = RTFdtNodePropertyAddString(hFdt, "compatible",           "gpio-keys");        VRC();

        vrc = RTFdtNodeAdd(hFdt, "poweroff");                                               VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "gpios", 3, idPHandleGpio, 3, 0);          VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "linux,code", 0x74);                       VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "label",      "GPIO Key Poweroff");        VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        /*
         * NEM
         */
        PCFGMNODE pNEM;
        InsertConfigNode(pRoot, "NEM", &pNEM);

        /*
         * MM values.
         */
        PCFGMNODE pMM;
        InsertConfigNode(pRoot, "MM", &pMM);

        /*
         * Memory setup.
         */
        PCFGMNODE pMem = NULL;
        InsertConfigNode(pMM, "MemRegions", &pMem);

        PCFGMNODE pMemRegion = NULL;
        InsertConfigNode(pMem, "Conventional", &pMemRegion);
        InsertConfigInteger(pMemRegion, "GCPhysStart", 0x40000000);
        InsertConfigInteger(pMemRegion, "Size", cbRam);

        vrc = RTFdtNodeAddF(hFdt, "memory@%RX32", 0x40000000);                              VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 4,
                                           0, 0x40000000,
                                           (uint32_t)(cbRam >> 32), cbRam & UINT32_MAX);    VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "device_type",      "memory");             VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        /* Configure the CPUs in the system, only one socket and cluster at the moment. */
        vrc = RTFdtNodeAdd(hFdt, "cpus");                                                   VRC();
        vrc = RTFdtNodePropertyAddU32(hFdt, "#size-cells",    0);                           VRC();
        vrc = RTFdtNodePropertyAddU32(hFdt, "#address-cells", 1);                           VRC();

        vrc = RTFdtNodeAdd(hFdt, "socket0");                                                VRC();
        vrc = RTFdtNodeAdd(hFdt, "cluster0");                                               VRC();

        for (uint32_t i = 0; i < cCpus; i++)
        {
            vrc = RTFdtNodeAddF(hFdt, "core%u", i);                                         VRC();
            vrc = RTFdtNodePropertyAddU32(hFdt, "cpu", aidPHandleCpus[i]);                  VRC();
            vrc = RTFdtNodeFinalize(hFdt);                                                  VRC();
        }

        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        for (uint32_t i = 0; i < cCpus; i++)
        {
            vrc = RTFdtNodeAddF(hFdt, "cpu@%u", i);                                         VRC();
            vrc = RTFdtNodePropertyAddU32(hFdt, "phandle", aidPHandleCpus[i]);              VRC();
            vrc = RTFdtNodePropertyAddU32(hFdt, "reg", i);                                  VRC();
            vrc = RTFdtNodePropertyAddString(hFdt, "compatible",  "arm,cortex-a15");        VRC();
            vrc = RTFdtNodePropertyAddString(hFdt, "device_type", "cpu");                   VRC();
            if (cCpus > 1)
            {
                vrc = RTFdtNodePropertyAddString(hFdt, "enable-method",  "psci");           VRC();
            }
            vrc = RTFdtNodeFinalize(hFdt);                                                  VRC();
        }

        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();


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
        vrc = RTPathAppPrivateArch(szPathVBoxC, RTPATH_MAX);                                VRC();
        vrc = RTPathAppend(szPathVBoxC, RTPATH_MAX, "/components/VBoxC");                   VRC();
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
         * VGA.
         */
        ComPtr<IGraphicsAdapter> pGraphicsAdapter;
        hrc = pMachine->COMGETTER(GraphicsAdapter)(pGraphicsAdapter.asOutParam());           H();
        GraphicsControllerType_T enmGraphicsController;
        hrc = pGraphicsAdapter->COMGETTER(GraphicsControllerType)(&enmGraphicsController);   H();

        /*
         * Devices
         */
        PCFGMNODE pDevices = NULL;      /* /Devices */
        PCFGMNODE pDev = NULL;          /* /Devices/Dev/ */
        PCFGMNODE pInst = NULL;         /* /Devices/Dev/0/ */
        PCFGMNODE pCfg = NULL;          /* /Devices/Dev/.../Config/ */
        PCFGMNODE pLunL0 = NULL;        /* /Devices/Dev/0/LUN#0/ */
        PCFGMNODE pLunL1 = NULL;        /* /Devices/Dev/0/LUN#0/AttachedDriver/ */

        InsertConfigNode(pRoot, "Devices", &pDevices);

        InsertConfigNode(pDevices, "platform",              &pDev);
        InsertConfigNode(pDev,     "0",                     &pInst);
        InsertConfigNode(pInst,    "Config",                &pCfg);
        InsertConfigNode(pInst,    "LUN#0",                 &pLunL0);
        InsertConfigString(pLunL0, "Driver",                "ResourceStore");

        /* Add the resources. */
        PCFGMNODE pResources = NULL;    /* /Devices/efi-armv8/Config/Resources */
        PCFGMNODE pRes = NULL;          /* /Devices/efi-armv8/Config/Resources/<Resource> */
        InsertConfigString(pCfg,        "ResourceNamespace",     "resources");
        InsertConfigNode(pCfg,          "Resources",             &pResources);
        InsertConfigNode(pResources,    "EfiRom",                &pRes);
        InsertConfigInteger(pRes,       "RegisterAsRom",         1);
        InsertConfigInteger(pRes,       "GCPhysLoadAddress",     0);

        /** @todo r=aeichner 32-bit guests and query the firmware type from VBoxSVC. */
        /*
         * Firmware.
         */
        FirmwareType_T eFwType =  FirmwareType_EFI64;
#ifdef VBOX_WITH_EFI_IN_DD2
        const char *pszEfiRomFile = eFwType == FirmwareType_EFIDUAL ? "<INVALID>"
                                  : eFwType == FirmwareType_EFI32   ? "VBoxEFIAArch32.fd"
                                  :                                   "VBoxEFIAArch64.fd";
        const char *pszKey = "ResourceId";
#else
        Utf8Str efiRomFile;
        vrc = findEfiRom(virtualBox, PlatformArchitecture_ARM, eFwType, &efiRomFile);
        AssertRCReturn(vrc, vrc);
        const char *pszEfiRomFile = efiRomFile.c_str();
        const char *pszKey = "Filename";
#endif
        InsertConfigString(pRes,        pszKey,                  pszEfiRomFile);

        InsertConfigNode(pResources,    "ArmV8Desc",             &pRes);
        InsertConfigInteger(pRes,       "RegisterAsRom",         1);
        InsertConfigInteger(pRes,       "GCPhysLoadAddress",     UINT64_MAX); /* End of physical address space. */
        InsertConfigString(pRes,        "ResourceId",            "VBoxArmV8Desc");

        vrc = RTFdtNodeAddF(hFdt, "platform-bus@%RX32", 0x0c000000);                        VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "interrupt-parent", idPHandleIntCtrl);     VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "ranges", 4, 0, 0, 0x0c000000, 0x02000000); VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#address-cells",   1);                    VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#size-cells",      1);                    VRC();
        vrc = RTFdtNodePropertyAddStringList(hFdt, "compatible",     2,
                                             "qemu,platform", "simple-bus");                VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        InsertConfigNode(pDevices, "gic",                   &pDev);
        InsertConfigNode(pDev,     "0",                     &pInst);
        InsertConfigInteger(pInst, "Trusted",               1);
        InsertConfigNode(pInst,    "Config",                &pCfg);
        InsertConfigInteger(pCfg,  "DistributorMmioBase",   0x08000000);
        InsertConfigInteger(pCfg,  "RedistributorMmioBase", 0x080a0000);

        vrc = RTFdtNodeAddF(hFdt, "intc@%RX32", 0x08000000);                                VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "phandle",          idPHandleIntCtrl);     VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 8,
                                           0, 0x08000000, 0, 0x10000,
                                           0, 0x080a0000, 0, 0xf60000);                     VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#redistributor-regions",   1);             VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "compatible",       "arm,gic-v3");         VRC();
        vrc = RTFdtNodePropertyAddEmpty(   hFdt, "ranges");                                 VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#size-cells",      2);                    VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#address-cells",   2);                    VRC();
        vrc = RTFdtNodePropertyAddEmpty(   hFdt, "interrupt-controller");                   VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#interrupt-cells", 3);                    VRC();

#if 0
        vrc = RTFdtNodeAddF(hFdt, "its@%RX32", 0x08080000);                                 VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "phandle",          idPHandleIntCtrlMsi);  VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 4, 0, 0x08080000, 0, 0x20000);      VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#msi-cells", 1);                          VRC();
        vrc = RTFdtNodePropertyAddEmpty(   hFdt, "msi-controller");                         VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "compatible", "arm,gic-v3-its");           VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();
#endif

        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();


        InsertConfigNode(pDevices, "qemu-fw-cfg",   &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "MmioSize",       4096);
        InsertConfigInteger(pCfg,  "MmioBase", 0x09020000);
        InsertConfigInteger(pCfg,  "DmaEnabled",        1);
        InsertConfigInteger(pCfg,  "QemuRamfbSupport",  enmGraphicsController == GraphicsControllerType_QemuRamFB ? 1 : 0);
        if (enmGraphicsController == GraphicsControllerType_QemuRamFB)
        {
            InsertConfigNode(pInst,    "LUN#0",           &pLunL0);
            InsertConfigString(pLunL0, "Driver",          "MainDisplay");
        }

        vrc = RTFdtNodeAddF(hFdt, "fw-cfg@%RX32", 0x09020000);                              VRC();
        vrc = RTFdtNodePropertyAddEmpty(   hFdt, "dma-coherent");                           VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 4, 0, 0x09020000, 0, 0x18);         VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "compatible", "qemu,fw-cfg-mmio");         VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();


        InsertConfigNode(pDevices, "flash-cfi",         &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "BaseAddress",  64 * _1M);
        InsertConfigInteger(pCfg,  "Size",        768 * _1K);
        InsertConfigString(pCfg,   "FlashFile",   "nvram");
        /* Attach the NVRAM storage driver. */
        InsertConfigNode(pInst,    "LUN#0",       &pLunL0);
        InsertConfigString(pLunL0, "Driver",      "NvramStore");

        vrc = RTFdtNodeAddF(hFdt, "flash@%RX32", 0);                                        VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "bank-width", 4);                          VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 8,
                                           0,          0, 0, 0x04000000,
                                           0, 0x04000000, 0, 0x04000000);                   VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "compatible", "cfi-flash");                VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        InsertConfigNode(pDevices, "arm-pl011",     &pDev);
        for (ULONG ulInstance = 0; ulInstance < 1 /** @todo SchemaDefs::SerialPortCount*/; ++ulInstance)
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

            InsertConfigInteger(pCfg,  "Irq",               1);
            InsertConfigInteger(pCfg,  "MmioBase", 0x09000000);

            vrc = RTFdtNodeAddF(hFdt, "pl011@%RX32", 0x09000000);                               VRC();
            vrc = RTFdtNodePropertyAddStringList(hFdt, "clock-names", 2, "uartclk", "apb_pclk"); VRC();
            vrc = RTFdtNodePropertyAddCellsU32(hFdt, "clocks", 2,
                                               idPHandleAbpPClk, idPHandleAbpPClk);             VRC();
            vrc = RTFdtNodePropertyAddCellsU32(hFdt, "interrupts", 3, 0x00, 0x01, 0x04);        VRC();
            vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 4, 0, 0x09000000, 0, 0x1000);       VRC();
            vrc = RTFdtNodePropertyAddStringList(hFdt, "compatible", 2,
                                                 "arm,pl011", "arm,primecell");                 VRC();
            vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

            BOOL  fServer;
            hrc = serialPort->COMGETTER(Server)(&fServer);                                  H();
            hrc = serialPort->COMGETTER(Path)(bstr.asOutParam());                           H();

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

        InsertConfigNode(pDevices, "arm-pl031-rtc", &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "Irq",               2);
        InsertConfigInteger(pCfg,  "MmioBase", 0x09010000);
        vrc = RTFdtNodeAddF(hFdt, "pl032@%RX32", 0x09010000);                               VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "clock-names", "apb_pclk");                VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "clocks", idPHandleAbpPClk);               VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "interrupts", 3, 0x00, 0x02, 0x04);        VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 4, 0, 0x09010000, 0, 0x1000);       VRC();
        vrc = RTFdtNodePropertyAddStringList(hFdt, "compatible", 2,
                                             "arm,pl031", "arm,primecell");                 VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        InsertConfigNode(pDevices, "arm-pl061-gpio",&pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "Irq",               7);
        InsertConfigInteger(pCfg,  "MmioBase", 0x09030000);
        vrc = RTFdtNodeAddF(hFdt, "pl061@%RX32", 0x09030000);                               VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "phandle", idPHandleGpio);                 VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "clock-names", "apb_pclk");                VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "clocks", idPHandleAbpPClk);               VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "interrupts", 3, 0x00, 0x07, 0x04);        VRC();
        vrc = RTFdtNodePropertyAddEmpty(   hFdt, "gpio-controller");                        VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#gpio-cells", 2);                         VRC();
        vrc = RTFdtNodePropertyAddStringList(hFdt, "compatible", 2,
                                             "arm,pl061", "arm,primecell");                 VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 4, 0, 0x09030000, 0, 0x1000);       VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        InsertConfigNode(pDevices, "pci-generic-ecam",  &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigInteger(pCfg,  "MmioEcamBase",   0x3f000000);
        InsertConfigInteger(pCfg,  "MmioEcamLength", 0x01000000);
        InsertConfigInteger(pCfg,  "MmioPioBase",    0x3eff0000);
        InsertConfigInteger(pCfg,  "MmioPioSize",    0x0000ffff);
        InsertConfigInteger(pCfg,  "IntPinA",        3);
        InsertConfigInteger(pCfg,  "IntPinB",        4);
        InsertConfigInteger(pCfg,  "IntPinC",        5);
        InsertConfigInteger(pCfg,  "IntPinD",        6);
        vrc = RTFdtNodeAddF(hFdt, "pcie@%RX32", 0x10000000);                                VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "interrupt-map-mask", 4, 0x1800, 0, 0, 7); VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "interrupt-map", 16 * 10,
                                           0x00,   0x00, 0x00, 0x01, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x03, 0x04,
                                           0x00,   0x00, 0x00, 0x02, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x04, 0x04,
                                           0x00,   0x00, 0x00, 0x03, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x05, 0x04,
                                           0x00,   0x00, 0x00, 0x04, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x06, 0x04,
                                           0x800,  0x00, 0x00, 0x01, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x04, 0x04,
                                           0x800,  0x00, 0x00, 0x02, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x05, 0x04,
                                           0x800,  0x00, 0x00, 0x03, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x06, 0x04,
                                           0x800,  0x00, 0x00, 0x04, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x03, 0x04,
                                           0x1000, 0x00, 0x00, 0x01, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x05, 0x04,
                                           0x1000, 0x00, 0x00, 0x02, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x06, 0x04,
                                           0x1000, 0x00, 0x00, 0x03, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x03, 0x04,
                                           0x1000, 0x00, 0x00, 0x04, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x04, 0x04,
                                           0x1800, 0x00, 0x00, 0x01, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x06, 0x04,
                                           0x1800, 0x00, 0x00, 0x02, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x03, 0x04,
                                           0x1800, 0x00, 0x00, 0x03, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x04, 0x04,
                                           0x1800, 0x00, 0x00, 0x04, idPHandleIntCtrl, 0x00, 0x00, 0x00, 0x05, 0x04); VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#interrupt-cells", 1);                    VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "ranges", 14,
                                           0x1000000, 0, 0, 0, 0x3eff0000, 0, 0x10000,
                                           0x2000000, 0, 0x10000000, 0, 0x10000000, 0,
                                           0x2eff0000);                                     VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 4, 0, 0x3f000000, 0, 0x1000000);    VRC();
        /** @todo msi-map */
        vrc = RTFdtNodePropertyAddEmpty(   hFdt, "dma-coherent");                           VRC();
        vrc = RTFdtNodePropertyAddCellsU32(hFdt, "bus-range", 2, 0, 0xf);                   VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "linux,pci-domain", 0);                    VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#size-cells", 2);                         VRC();
        vrc = RTFdtNodePropertyAddU32(     hFdt, "#address-cells", 3);                      VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "device_type", "pci");                     VRC();
        vrc = RTFdtNodePropertyAddString(  hFdt, "compatible", "pci-host-ecam-generic");    VRC();
        vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();

        if (   enmGraphicsController != GraphicsControllerType_QemuRamFB
            && enmGraphicsController != GraphicsControllerType_Null)
        {
            InsertConfigNode(pDevices, "vga", &pDev);
            InsertConfigNode(pDev,     "0", &pInst);
            InsertConfigInteger(pInst, "Trusted",           1);
            InsertConfigInteger(pInst, "PCIBusNo",          0);
            InsertConfigInteger(pInst, "PCIDeviceNo",       2);
            InsertConfigInteger(pInst, "PCIFunctionNo",     0);
            InsertConfigNode(pInst,    "Config", &pCfg);
            InsertConfigInteger(pCfg,  "VRamSize",          32 * _1M);
            InsertConfigInteger(pCfg,  "MonitorCount",         1);
            i_attachStatusDriver(pInst, DeviceType_Graphics3D);
            InsertConfigInteger(pCfg, "VMSVGAEnabled", true);
            InsertConfigInteger(pCfg, "VMSVGAPciBarLayout", true);
            InsertConfigInteger(pCfg, "VMSVGAPciId", true);
            InsertConfigInteger(pCfg, "VMSVGA3dEnabled", false);
            InsertConfigInteger(pCfg, "VmSvga3", true);
            InsertConfigInteger(pCfg, "VmSvgaExposeLegacyVga", false);

            /* Attach the display. */
            InsertConfigNode(pInst,    "LUN#0", &pLunL0);
            InsertConfigString(pLunL0, "Driver",               "MainDisplay");
            InsertConfigNode(pLunL0,   "Config", &pCfg);
        }

        InsertConfigNode(pDevices, "VMMDev",          &pDev);
        InsertConfigNode(pDev,     "0",              &pInst);
        InsertConfigInteger(pInst, "Trusted",             1);
        InsertConfigInteger(pInst, "PCIBusNo",            0);
        InsertConfigInteger(pInst, "PCIDeviceNo",         0);
        InsertConfigInteger(pInst, "PCIFunctionNo",       0);
        InsertConfigNode(pInst,    "Config",          &pCfg);
        InsertConfigInteger(pCfg,  "MmioReq",             1);

        /* the VMM device's Main driver */
        InsertConfigNode(pInst,    "LUN#0", &pLunL0);
        InsertConfigString(pLunL0, "Driver",               "HGCM");
        InsertConfigNode(pLunL0,   "Config", &pCfg);

        /*
         * Attach the status driver.
         */
        i_attachStatusDriver(pInst, DeviceType_SharedFolder);

#ifdef VBOX_WITH_SHARED_CLIPBOARD
        /*
         * Shared Clipboard.
         */
        {
            ClipboardMode_T enmClipboardMode = ClipboardMode_Disabled;
            hrc = pMachine->COMGETTER(ClipboardMode)(&enmClipboardMode); H();
#  ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
            BOOL fFileTransfersEnabled;
            hrc = pMachine->COMGETTER(ClipboardFileTransfersEnabled)(&fFileTransfersEnabled); H();
#  endif

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

#  ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
                vrc = i_changeClipboardFileTransferMode(RT_BOOL(fFileTransfersEnabled));
                AssertLogRelMsg(RT_SUCCESS(vrc), ("Shared Clipboard: Failed to set initial file transfers mode (%u): vrc=%Rrc\n",
                                                 fFileTransfersEnabled, vrc));

                /** @todo Register area callbacks? (See also deregistration todo in Console::i_powerDown.) */
#  endif
            }
            else
                LogRel(("Shared Clipboard: Not available, vrc=%Rrc\n", vrc));
            vrc = VINF_SUCCESS;  /* None of the potential failures above are fatal. */
        }
#endif /* VBOX_WITH_SHARED_CLIPBOARD */

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

        InsertConfigNode(pDevices, "usb-xhci",      &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigInteger(pInst, "Trusted",           1);
        InsertConfigInteger(pInst, "PCIBusNo",          0);
        InsertConfigInteger(pInst, "PCIDeviceNo",       1);
        InsertConfigInteger(pInst, "PCIFunctionNo",     0);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigNode(pInst,    "LUN#0",       &pLunL0);
        InsertConfigString(pLunL0, "Driver","VUSBRootHub");
        InsertConfigNode(pInst,    "LUN#1",       &pLunL0);
        InsertConfigString(pLunL0, "Driver","VUSBRootHub");

        /*
         * Network adapters
         */
        PCFGMNODE pDevE1000 = NULL;          /* E1000-type devices */
        InsertConfigNode(pDevices, "e1000", &pDevE1000);
        PCFGMNODE pDevVirtioNet = NULL;          /* Virtio network devices */
        InsertConfigNode(pDevices, "virtio-net", &pDevVirtioNet);

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
                case NetworkAdapterType_Am79C970A:
                case NetworkAdapterType_Am79C973:
                case NetworkAdapterType_Am79C960:
                case NetworkAdapterType_NE1000:
                case NetworkAdapterType_NE2000:
                case NetworkAdapterType_WD8003:
                case NetworkAdapterType_WD8013:
                case NetworkAdapterType_ELNK2:
                case NetworkAdapterType_ELNK1:
                default:
                    AssertMsgFailed(("Invalid/Unsupported network adapter type '%d' for slot '%d'", adapterType, uInstance));
                    return pVMM->pfnVMR3SetError(pUVM, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                                 N_("Invalid/Unsupported network adapter type '%d' for slot '%d'"), adapterType, uInstance);
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

            InsertConfigNode(pInst, "Config", &pCfg);

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
                {
                    uint32_t GCPhysMmioBase = 0x0a000000 + uInstance * GUEST_PAGE_SIZE;
                    uint32_t uIrq = 16 + uInstance;

                    InsertConfigInteger(pCfg,  "MmioBase",          GCPhysMmioBase);
                    InsertConfigInteger(pCfg,  "Irq",               uIrq);

                    vrc = RTFdtNodeAddF(hFdt, "virtio_mmio@%RX32", GCPhysMmioBase);                         VRC();
                    vrc = RTFdtNodePropertyAddEmpty(  hFdt, "dma-coherent");                            VRC();
                    vrc = RTFdtNodePropertyAddCellsU32(hFdt, "interrupts", 3, 0x00, uIrq, 0x04);        VRC();
                    vrc = RTFdtNodePropertyAddCellsU32(hFdt, "reg", 4, 0, GCPhysMmioBase, 0, 0x200);    VRC();
                    vrc = RTFdtNodePropertyAddString(hFdt, "compatible", "virtio,mmio");                VRC();
                    vrc = RTFdtNodeFinalize(hFdt);                                                      VRC();
                    break;
                }
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

        PCFGMNODE pUsb = NULL;
        InsertConfigNode(pRoot,    "USB",           &pUsb);

        /*
         * Storage controllers.
         */
        com::SafeIfaceArray<IStorageController> ctrls;
        PCFGMNODE aCtrlNodes[StorageControllerType_VirtioSCSI + 1] = {};
        hrc = pMachine->COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(ctrls));       H();

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

            switch (enmCtrlType)
            {
                case StorageControllerType_USB:
                {
                    if (pUsb)
                    {
                        /*
                         * USB MSDs are handled a bit different as the device instance
                         * doesn't match the storage controller instance but the port.
                         */
                        InsertConfigNode(pUsb, "Msd", &pDev);
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

                case StorageControllerType_IntelAhci:
                {
                    InsertConfigInteger(pCtlInst, "PCIBusNo",          0);
                    InsertConfigInteger(pCtlInst, "PCIDeviceNo",       3);
                    InsertConfigInteger(pCtlInst, "PCIFunctionNo",     0);

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
                    break;
                }
                case StorageControllerType_VirtioSCSI:
                {
                    InsertConfigInteger(pCtlInst, "PCIBusNo",          0);
                    InsertConfigInteger(pCtlInst, "PCIDeviceNo",       3);
                    InsertConfigInteger(pCtlInst, "PCIFunctionNo",     0);

                    ULONG cPorts = 0;
                    hrc = ctrls[i]->COMGETTER(PortCount)(&cPorts);                          H();
                    InsertConfigInteger(pCfg, "NumTargets", cPorts);
                    InsertConfigInteger(pCfg, "Bootable",   fBootable);

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk) | RT_BIT_32(DeviceType_DVD) /*?*/,
                                         cPorts, &paLedDevType, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                case StorageControllerType_NVMe:
                {
                    InsertConfigInteger(pCtlInst, "PCIBusNo",          0);
                    InsertConfigInteger(pCtlInst, "PCIDeviceNo",       3);
                    InsertConfigInteger(pCtlInst, "PCIFunctionNo",     0);

                    /* Attach the status driver */
                    i_attachStatusDriver(pCtlInst, RT_BIT_32(DeviceType_HardDisk) | RT_BIT_32(DeviceType_DVD) /*?*/,
                                         1, &paLedDevType, &mapMediumAttachments, pszCtrlDev, ulInstance);
                    break;
                }

                case StorageControllerType_LsiLogic:
                case StorageControllerType_BusLogic:
                case StorageControllerType_PIIX3:
                case StorageControllerType_PIIX4:
                case StorageControllerType_ICH6:
                case StorageControllerType_I82078:
                case StorageControllerType_LsiLogicSas:

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

        InsertConfigNode(pUsb,     "HidKeyboard",   &pDev);
        InsertConfigNode(pDev,     "0",            &pInst);
        InsertConfigInteger(pInst, "Trusted",           1);
        InsertConfigNode(pInst,    "Config",        &pCfg);
        InsertConfigNode(pInst,    "LUN#0",       &pLunL0);
        InsertConfigString(pLunL0, "Driver",       "KeyboardQueue");
        InsertConfigNode(pLunL0,   "AttachedDriver",  &pLunL1);
        InsertConfigString(pLunL1, "Driver",          "MainKeyboard");

        InsertConfigNode(pUsb,     "HidMouse", &pDev);
        InsertConfigNode(pDev,     "0", &pInst);
        InsertConfigNode(pInst,    "Config", &pCfg);
        InsertConfigString(pCfg,   "Mode", "absolute");
        InsertConfigNode(pInst,    "LUN#0", &pLunL0);
        InsertConfigString(pLunL0, "Driver",        "MouseQueue");
        InsertConfigNode(pLunL0,   "Config", &pCfg);
        InsertConfigInteger(pCfg,  "QueueSize",            128);

        InsertConfigNode(pLunL0,   "AttachedDriver", &pLunL1);
        InsertConfigString(pLunL1, "Driver",        "MainMouse");
    }
    catch (ConfigError &x)
    {
        RTFdtDestroy(hFdt);

        // InsertConfig threw something:
        pVMM->pfnVMR3SetError(pUVM, x.m_vrc, RT_SRC_POS, "Caught ConfigError: %Rrc - %s", x.m_vrc, x.what());
        return x.m_vrc;
    }
    catch (HRESULT hrcXcpt)
    {
        RTFdtDestroy(hFdt);
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

    vrc = RTFdtNodeAdd(hFdt, "chosen");                                                 VRC();
    vrc = RTFdtNodePropertyAddString(  hFdt, "stdout-path", "pl011@9000000");           VRC();
    vrc = RTFdtNodePropertyAddString(  hFdt, "stdin-path", "pl011@9000000");            VRC();
    vrc = RTFdtNodeFinalize(hFdt);

    /* Finalize the FDT and add it to the resource store. */
    vrc = RTFdtFinalize(hFdt);
    AssertRCReturnStmt(vrc, RTFdtDestroy(hFdt), vrc);

    RTVFSFILE hVfsFileDesc = NIL_RTVFSFILE;
    vrc = RTVfsMemFileCreate(NIL_RTVFSIOSTREAM, 0 /*cbEstimate*/, &hVfsFileDesc);
    AssertRCReturnStmt(vrc, RTFdtDestroy(hFdt), vrc);
    RTVFSIOSTREAM hVfsIosDesc = RTVfsFileToIoStream(hVfsFileDesc);
    AssertRelease(hVfsIosDesc != NIL_RTVFSIOSTREAM);

    /* Initialize the VBox platform descriptor. */
    VBOXPLATFORMARMV8 ArmV8Platform; RT_ZERO(ArmV8Platform);

    vrc = RTFdtDumpToVfsIoStrm(hFdt, RTFDTTYPE_DTB, 0 /*fFlags*/, hVfsIosDesc, NULL /*pErrInfo*/);
    if (RT_SUCCESS(vrc))
        vrc = RTVfsFileQuerySize(hVfsFileDesc, &ArmV8Platform.cbFdt);
    AssertRCReturnStmt(vrc, RTFdtDestroy(hFdt), vrc);

    vrc = RTVfsIoStrmZeroFill(hVfsIosDesc, (RTFOFF)(RT_ALIGN_64(ArmV8Platform.cbFdt, _64K) - ArmV8Platform.cbFdt));
    AssertRCReturn(vrc, vrc);

    ArmV8Platform.u32Magic            = VBOXPLATFORMARMV8_MAGIC;
    ArmV8Platform.u32Version          = VBOXPLATFORMARMV8_VERSION;
    ArmV8Platform.cbDesc              = sizeof(ArmV8Platform);
    ArmV8Platform.fFlags              = 0;
    ArmV8Platform.u64PhysAddrRamBase  = UINT64_C(0x40000000);
    ArmV8Platform.cbRamBase           = cbRam;
    ArmV8Platform.u64OffBackFdt       = RT_ALIGN_64(ArmV8Platform.cbFdt, _64K);
    ArmV8Platform.cbFdt               = RT_ALIGN_64(ArmV8Platform.cbFdt, _64K);
    ArmV8Platform.u64OffBackAcpiXsdp  = 0;
    ArmV8Platform.cbAcpiXsdp          = 0;
    ArmV8Platform.u64OffBackUefiRom   = GCPhysTopOfAddrSpace - sizeof(ArmV8Platform);
    ArmV8Platform.cbUefiRom           = _64M; /** @todo Fixed reservation but the ROM region is usually much smaller. */
    ArmV8Platform.u64OffBackMmio      = GCPhysTopOfAddrSpace - sizeof(ArmV8Platform) - 0x08000000; /** @todo Start of generic MMIO area containing the GIC,UART,RTC, etc. Will be changed soon */
    ArmV8Platform.cbMmio              = _128M;

    /* Add the VBox platform descriptor to the resource store. */
    vrc = RTVfsIoStrmWrite(hVfsIosDesc, &ArmV8Platform, sizeof(ArmV8Platform), true /*fBlocking*/, NULL /*pcbWritten*/);
    RTVfsIoStrmRelease(hVfsIosDesc);
    vrc = mptrResourceStore->i_addItem("resources", "VBoxArmV8Desc", hVfsFileDesc);
    RTVfsFileRelease(hVfsFileDesc);
    AssertRCReturn(vrc, vrc);

    /* Dump the DTB for debugging purposes if requested. */
    Bstr DtbDumpVal;
    hrc = mMachine->GetExtraData(Bstr("VBoxInternal2/DumpDtb").raw(),
                                 DtbDumpVal.asOutParam());
    if (   hrc == S_OK
        && DtbDumpVal.isNotEmpty())
    {
        vrc = RTFdtDumpToFile(hFdt, RTFDTTYPE_DTB, 0 /*fFlags*/, Utf8Str(DtbDumpVal).c_str(), NULL /*pErrInfo*/);
        AssertRCReturnStmt(vrc, RTFdtDestroy(hFdt), vrc);
    }


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
#endif /* !VBOX_WITH_VIRT_ARMV8 */

