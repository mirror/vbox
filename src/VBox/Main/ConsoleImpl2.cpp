/** $Id$ */
/** @file
 * VBox Console COM Class implementation
 *
 * @remark  We've split out the code that the 64-bit VC++ v8 compiler
 *          finds problematic to optimize so we can disable optimizations
 *          and later, perhaps, find a real solution for it.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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

#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/param.h>

#include <VBox/vmapi.h>
#include <VBox/err.h>
#include <VBox/version.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>


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
    /** Note: hardcoded assumption about number of slots; see rom bios */
    bool faPciDeviceNo[15] = {false};

#if defined(RT_OS_WINDOWS)
    {
        /* initialize COM */
        HRESULT hrc = CoInitializeEx(NULL,
                                     COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE |
                                     COINIT_SPEED_OVER_MEMORY);
        LogFlow (("Console::configConstructor(): CoInitializeEx()=%08X\n", hrc));
        AssertComRCReturn (hrc, VERR_GENERAL_FAILURE);
    }
#endif

    AssertReturn (pvConsole, VERR_GENERAL_FAILURE);
    ComObjPtr <Console> pConsole = static_cast <Console *> (pvConsole);

    AutoCaller autoCaller (pConsole);
    AssertComRCReturn (autoCaller.rc(), VERR_ACCESS_DENIED);

    /* lock the console because we widely use internal fields and methods */
    AutoLock alock (pConsole);

    ComPtr <IMachine> pMachine = pConsole->machine();

    int             rc;
    HRESULT         hrc;
    char           *psz = NULL;
    BSTR            str = NULL;
    unsigned        i;

#define STR_CONV()  do { rc = RTStrUcs2ToUtf8(&psz, str); RC_CHECK(); } while (0)
#define STR_FREE()  do { if (str) { SysFreeString(str); str = NULL; } if (psz) { RTStrFree(psz); psz = NULL; } } while (0)
#define RC_CHECK()  do { if (VBOX_FAILURE(rc)) { AssertMsgFailed(("rc=%Vrc\n", rc)); STR_FREE(); return rc; } } while (0)
#define H()         do { if (FAILED(hrc)) { AssertMsgFailed(("hrc=%#x\n", hrc)); STR_FREE(); return VERR_GENERAL_FAILURE; } } while (0)

    /*
     * Get necessary objects and frequently used parameters.
     */
    ComPtr<IVirtualBox> virtualBox;
    hrc = pMachine->COMGETTER(Parent)(virtualBox.asOutParam());                     H();

    ComPtr<IHost> host;
    hrc = virtualBox->COMGETTER(Host)(host.asOutParam());                           H();

    ComPtr <ISystemProperties> systemProperties;
    hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());   H();

    ComPtr<IBIOSSettings> biosSettings;
    hrc = pMachine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());             H();

    Guid uuid;
    hrc = pMachine->COMGETTER(Id)(uuid.asOutParam());                               H();
    PCRTUUID pUuid = uuid.raw();

    ULONG cRamMBs;
    hrc = pMachine->COMGETTER(MemorySize)(&cRamMBs);                                H();


    /*
     * Get root node first.
     * This is the only node in the tree.
     */
    PCFGMNODE pRoot = CFGMR3GetRoot(pVM);
    Assert(pRoot);

    /*
     * Set the root level values.
     */
    hrc = pMachine->COMGETTER(Name)(&str);                                          H();
    STR_CONV();
    rc = CFGMR3InsertString(pRoot,  "Name",                 psz);                   RC_CHECK();
    STR_FREE();
    rc = CFGMR3InsertBytes(pRoot,   "UUID", pUuid, sizeof(*pUuid));                 RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RamSize",              cRamMBs * _1M);         RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "TimerMillies",         10);                    RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RawR3Enabled",         1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "RawR0Enabled",         1);     /* boolean */   RC_CHECK();
    /** @todo Config: RawR0, PATMEnabled and CASMEnabled needs attention later. */
    rc = CFGMR3InsertInteger(pRoot, "PATMEnabled",          1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pRoot, "CSAMEnabled",          1);     /* boolean */   RC_CHECK();

    /* hardware virtualization extensions */
    TriStateBool_T hwVirtExEnabled;
    BOOL fHWVirtExEnabled;
    hrc = pMachine->COMGETTER(HWVirtExEnabled)(&hwVirtExEnabled);                   H();
    if (hwVirtExEnabled == TriStateBool_Default)
    {
        /* check the default value */
        hrc = systemProperties->COMGETTER(HWVirtExEnabled)(&fHWVirtExEnabled);      H();
    }
    else
        fHWVirtExEnabled = (hwVirtExEnabled == TriStateBool_True);
#ifndef RT_OS_DARWIN /** @todo Implement HWVirtExt on darwin. See #1865. */
    if (fHWVirtExEnabled)
    {
        PCFGMNODE pHWVirtExt;
        rc = CFGMR3InsertNode(pRoot, "HWVirtExt", &pHWVirtExt);                     RC_CHECK();
        rc = CFGMR3InsertInteger(pHWVirtExt, "Enabled", 1);                         RC_CHECK();
    }
#endif

    BOOL fIOAPIC;
    hrc = biosSettings->COMGETTER(IOAPICEnabled)(&fIOAPIC);                          H();

    BOOL fPXEDebug;
    hrc = biosSettings->COMGETTER(PXEDebugEnabled)(&fPXEDebug);                      H();

    /*
     * Virtual IDE controller type.
     */
    IDEControllerType_T controllerType;
    BOOL fPIIX4;
    hrc = biosSettings->COMGETTER(IDEControllerType)(&controllerType);               H();
    switch (controllerType)
    {
        case IDEControllerType_IDEControllerPIIX3:
            fPIIX4 = FALSE;
            break;
        case IDEControllerType_IDEControllerPIIX4:
            fPIIX4 = TRUE;
            break;
        default:
            AssertMsgFailed(("Invalid IDE controller type '%d'", controllerType));
            return VERR_INVALID_PARAMETER;
    }

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

    rc = CFGMR3InsertNode(pRoot, "Devices", &pDevices);                             RC_CHECK();

    /*
     * PC Arch.
     */
    rc = CFGMR3InsertNode(pDevices, "pcarch", &pDev);                               RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();

    /*
     * PC Bios.
     */
    rc = CFGMR3InsertNode(pDevices, "pcbios", &pDev);                               RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "RamSize",              cRamMBs * _1M);         RC_CHECK();
    rc = CFGMR3InsertString(pCfg,   "HardDiskDevice",       "piix3ide");            RC_CHECK();
    rc = CFGMR3InsertString(pCfg,   "FloppyDevice",         "i82078");              RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC",               fIOAPIC);               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "PXEDebug",             fPXEDebug);             RC_CHECK();
    rc = CFGMR3InsertBytes(pCfg,    "UUID", pUuid, sizeof(*pUuid));                 RC_CHECK();

    DeviceType_T bootDevice;
    if (SchemaDefs::MaxBootPosition > 9)
    {
        AssertMsgFailed (("Too many boot devices %d\n",
                          SchemaDefs::MaxBootPosition));
        return VERR_INVALID_PARAMETER;
    }

    for (ULONG pos = 1; pos <= SchemaDefs::MaxBootPosition; pos ++)
    {
        hrc = pMachine->GetBootOrder(pos, &bootDevice);                             H();

        char szParamName[] = "BootDeviceX";
        szParamName[sizeof (szParamName) - 2] = ((char (pos - 1)) + '0');

        const char *pszBootDevice;
        switch (bootDevice)
        {
            case DeviceType_NoDevice:
                pszBootDevice = "NONE";
                break;
            case DeviceType_HardDiskDevice:
                pszBootDevice = "IDE";
                break;
            case DeviceType_DVDDevice:
                pszBootDevice = "DVD";
                break;
            case DeviceType_FloppyDevice:
                pszBootDevice = "FLOPPY";
                break;
            case DeviceType_NetworkDevice:
                pszBootDevice = "LAN";
                break;
            default:
                AssertMsgFailed(("Invalid bootDevice=%d\n", bootDevice));
                return VERR_INVALID_PARAMETER;
        }
        rc = CFGMR3InsertString(pCfg, szParamName, pszBootDevice);                 RC_CHECK();
    }

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
    rc = CFGMR3InsertString(pCfg,   "LogoFile", logoImagePath ? Utf8Str(logoImagePath) : ""); RC_CHECK();

    /*
     * Boot menu
     */
    BIOSBootMenuMode_T bootMenuMode;
    int value;
    biosSettings->COMGETTER(BootMenuMode)(&bootMenuMode);
    switch (bootMenuMode)
    {
        case BIOSBootMenuMode_Disabled:
            value = 0;
            break;
        case BIOSBootMenuMode_MenuOnly:
            value = 1;
            break;
        default:
            value = 2;
    }
    rc = CFGMR3InsertInteger(pCfg, "ShowBootMenu", value);                          RC_CHECK();

    /*
     * The time offset
     */
    LONG64 timeOffset;
    hrc = biosSettings->COMGETTER(TimeOffset)(&timeOffset);                         H();
    PCFGMNODE pTMNode;
    rc = CFGMR3InsertNode(pRoot, "TM", &pTMNode);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pTMNode, "UTCOffset", timeOffset * 1000000);           RC_CHECK();

    /*
     * ACPI
     */
    BOOL fACPI;
    hrc = biosSettings->COMGETTER(ACPIEnabled)(&fACPI);                             H();
    if (fACPI)
    {
        rc = CFGMR3InsertNode(pDevices, "acpi", &pDev);                             RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted", 1);              /* boolean */   RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "RamSize",          cRamMBs * _1M);         RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                         RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          7);                 RC_CHECK();
        Assert(!faPciDeviceNo[7]);
        faPciDeviceNo[7] = true;
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                 RC_CHECK();

        rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                          RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "ACPIHost");        RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
    }

    /*
     * DMA
     */
    rc = CFGMR3InsertNode(pDevices, "8237A", &pDev);                                RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted", 1);                  /* boolean */   RC_CHECK();

    /*
     * PCI bus.
     */
    rc = CFGMR3InsertNode(pDevices, "pci", &pDev); /* piix3 */                      RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                             RC_CHECK();

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
     * i82078 Floppy drive controller
     */
    ComPtr<IFloppyDrive> floppyDrive;
    hrc = pMachine->COMGETTER(FloppyDrive)(floppyDrive.asOutParam());               H();
    BOOL fFloppyEnabled;
    hrc = floppyDrive->COMGETTER(Enabled)(&fFloppyEnabled);                         H();
    if (fFloppyEnabled)
    {
        rc = CFGMR3InsertNode(pDevices, "i82078",    &pDev);                        RC_CHECK();
        rc = CFGMR3InsertNode(pDev,     "0",         &pInst);                       RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",   1);                            RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config",    &pCfg);                        RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "IRQ",       6);                            RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "DMA",       2);                            RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "MemMapped", 0 );                           RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "IOBase",    0x3f0);                        RC_CHECK();

        /* Attach the status driver */
        rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                            RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");          RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapFDLeds[0]); RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                 RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                                 RC_CHECK();

        rc = CFGMR3InsertNode(pInst,    "LUN#0",     &pLunL0);                          RC_CHECK();

        ComPtr<IFloppyImage> floppyImage;
        hrc = floppyDrive->GetImage(floppyImage.asOutParam());                          H();
        if (floppyImage)
        {
            pConsole->meFloppyState = DriveState_ImageMounted;
            rc = CFGMR3InsertString(pLunL0, "Driver",    "Block");                      RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config",    &pCfg);                        RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Type",      "Floppy 1.44");                RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "Mountable", 1);                            RC_CHECK();

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver",          "RawImage");             RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
            hrc = floppyImage->COMGETTER(FilePath)(&str);                               H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Path",             psz);                   RC_CHECK();
            STR_FREE();
        }
        else
        {
            ComPtr<IHostFloppyDrive> hostFloppyDrive;
            hrc = floppyDrive->GetHostDrive(hostFloppyDrive.asOutParam());              H();
            if (hostFloppyDrive)
            {
                pConsole->meFloppyState = DriveState_HostDriveCaptured;
                rc = CFGMR3InsertString(pLunL0, "Driver",      "HostFloppy");           RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
                hrc = hostFloppyDrive->COMGETTER(Name)(&str);                           H();
                STR_CONV();
                rc = CFGMR3InsertString(pCfg,   "Path",         psz);                   RC_CHECK();
                STR_FREE();
            }
            else
            {
                pConsole->meFloppyState = DriveState_NotMounted;
                rc = CFGMR3InsertString(pLunL0, "Driver",    "Block");       RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,   "Config",    &pCfg);         RC_CHECK();
                rc = CFGMR3InsertString(pCfg,   "Type",      "Floppy 1.44"); RC_CHECK();
                rc = CFGMR3InsertInteger(pCfg,  "Mountable", 1);             RC_CHECK();
            }
        }
    }

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
     */
    rc = CFGMR3InsertNode(pDevices, "apic", &pDev);                                 RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "IOAPIC", fIOAPIC);                             RC_CHECK();

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
    Assert(!faPciDeviceNo[2]);
    faPciDeviceNo[2] = true;
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    hrc = pMachine->COMGETTER(VRAMSize)(&cRamMBs);                                  H();
    rc = CFGMR3InsertInteger(pCfg,  "VRamSize",             cRamMBs * _1M);         RC_CHECK();

    /* Custom VESA mode list */
    unsigned cModes = 0;
    for (unsigned iMode = 1; iMode <= 16; iMode++)
    {
        char szExtraDataKey[sizeof("CustomVideoModeXX")];
        RTStrPrintf(szExtraDataKey, sizeof(szExtraDataKey), "CustomVideoMode%d", iMode);
        hrc = pMachine->GetExtraData(Bstr(szExtraDataKey), &str);                   H();
        if (!str || !*str)
            break;
        STR_CONV();
        rc = CFGMR3InsertString(pCfg, szExtraDataKey, psz);
        STR_FREE();
        cModes++;
    }
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
     * IDE (update this when the main interface changes)
     */
    rc = CFGMR3InsertNode(pDevices, "piix3ide", &pDev); /* piix3 */                 RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          1);                     RC_CHECK();
    Assert(!faPciDeviceNo[1]);
    faPciDeviceNo[1] = true;
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        1);                     RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "PIIX4", fPIIX4);               /* boolean */   RC_CHECK();

    /* Attach the status driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                            RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");          RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapIDELeds[0]);RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "First",    0);                                 RC_CHECK();
    rc = CFGMR3InsertInteger(pCfg,  "Last",     3);                                 RC_CHECK();

    /* Attach the harddisks */
    ComPtr<IHardDiskAttachmentCollection> hdaColl;
    hrc = pMachine->COMGETTER(HardDiskAttachments)(hdaColl.asOutParam());           H();
    ComPtr<IHardDiskAttachmentEnumerator> hdaEnum;
    hrc = hdaColl->Enumerate(hdaEnum.asOutParam());                                 H();

    BOOL fMore = FALSE;
    while (     SUCCEEDED(hrc = hdaEnum->HasMore(&fMore))
           &&   fMore)
    {
        ComPtr<IHardDiskAttachment> hda;
        hrc = hdaEnum->GetNext(hda.asOutParam());                                   H();
        ComPtr<IHardDisk> hardDisk;
        hrc = hda->COMGETTER(HardDisk)(hardDisk.asOutParam());                      H();
        DiskControllerType_T enmCtl;
        hrc = hda->COMGETTER(Controller)(&enmCtl);                                  H();
        LONG lDev;
        hrc = hda->COMGETTER(DeviceNumber)(&lDev);                                  H();

        switch (enmCtl)
        {
            case DiskControllerType_IDE0Controller:
                i = 0;
                break;
            case DiskControllerType_IDE1Controller:
                i = 2;
                break;
            default:
                AssertMsgFailed(("invalid disk controller type: %d\n", enmCtl));
                return VERR_GENERAL_FAILURE;
        }

        if (lDev < 0 || lDev >= 2)
        {
            AssertMsgFailed(("invalid controller device number: %d\n", lDev));
            return VERR_GENERAL_FAILURE;
        }

        i = i + lDev;

        char szLUN[16];
        RTStrPrintf(szLUN, sizeof(szLUN), "LUN#%d", i);
        rc = CFGMR3InsertNode(pInst,    szLUN, &pLunL0);                            RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "Block");           RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
        rc = CFGMR3InsertString(pCfg,   "Type",                 "HardDisk");        RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,  "Mountable",            0);                 RC_CHECK();

        HardDiskStorageType_T hddType;
        hardDisk->COMGETTER(StorageType)(&hddType);
        if (hddType == HardDiskStorageType_VirtualDiskImage)
        {
            ComPtr<IVirtualDiskImage> vdiDisk = hardDisk;
            AssertBreak (!vdiDisk.isNull(), hrc = E_FAIL);

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver",         "VBoxHDD");               RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
            hrc = vdiDisk->COMGETTER(FilePath)(&str);                                   H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Path",             psz);                   RC_CHECK();
            STR_FREE();

            /* Create an inversed tree of parents. */
            ComPtr<IHardDisk> parentHardDisk = hardDisk;
            for (PCFGMNODE pParent = pCfg;;)
            {
                ComPtr<IHardDisk> curHardDisk;
                hrc = parentHardDisk->COMGETTER(Parent)(curHardDisk.asOutParam());      H();
                if (!curHardDisk)
                    break;

                vdiDisk = curHardDisk;
                AssertBreak (!vdiDisk.isNull(), hrc = E_FAIL);

                PCFGMNODE pCur;
                rc = CFGMR3InsertNode(pParent, "Parent", &pCur);                        RC_CHECK();
                hrc = vdiDisk->COMGETTER(FilePath)(&str);                               H();
                STR_CONV();
                rc = CFGMR3InsertString(pCur,  "Path", psz);                            RC_CHECK();
                STR_FREE();
                rc = CFGMR3InsertInteger(pCur, "ReadOnly", 1);                          RC_CHECK();

                /* next */
                pParent = pCur;
                parentHardDisk = curHardDisk;
            }
        }
        else if (hddType == HardDiskStorageType_ISCSIHardDisk)
        {
            ComPtr<IISCSIHardDisk> iSCSIDisk = hardDisk;
            AssertBreak (!iSCSIDisk.isNull(), hrc = E_FAIL);

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver",         "iSCSI");                 RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();

            /* Set up the iSCSI initiator driver configuration. */
            hrc = iSCSIDisk->COMGETTER(Target)(&str);                                   H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "TargetName",   psz);                       RC_CHECK();
            STR_FREE();

            // @todo currently there is no Initiator name config.
            rc = CFGMR3InsertString(pCfg,   "InitiatorName", "iqn.2006-02.de.innotek.initiator"); RC_CHECK();

            ULONG64 lun;
            hrc = iSCSIDisk->COMGETTER(Lun)(&lun);                                      H();
            rc = CFGMR3InsertInteger(pCfg,   "LUN",         lun);                       RC_CHECK();

            hrc = iSCSIDisk->COMGETTER(Server)(&str);                                   H();
            STR_CONV();
            USHORT port;
            hrc = iSCSIDisk->COMGETTER(Port)(&port);                                    H();
            if (port != 0)
            {
                char *pszTN;
                RTStrAPrintf(&pszTN, "%s:%u", psz, port);
                rc = CFGMR3InsertString(pCfg,   "TargetAddress",    pszTN);             RC_CHECK();
                RTStrFree(pszTN);
            }
            else
            {
                rc = CFGMR3InsertString(pCfg,   "TargetAddress",    psz);               RC_CHECK();
            }
            STR_FREE();

            hrc = iSCSIDisk->COMGETTER(UserName)(&str);                                 H();
            if (str)
            {
                STR_CONV();
                rc = CFGMR3InsertString(pCfg,   "InitiatorUsername",    psz);           RC_CHECK();
                STR_FREE();
            }

            hrc = iSCSIDisk->COMGETTER(Password)(&str);                                 H();
            if (str)
            {
                STR_CONV();
                rc = CFGMR3InsertString(pCfg,   "InitiatorSecret",      psz);           RC_CHECK();
                STR_FREE();
            }

            // @todo currently there is no target username config.
            //rc = CFGMR3InsertString(pCfg,   "TargetUsername",   "");                    RC_CHECK();

            // @todo currently there is no target password config.
            //rc = CFGMR3InsertString(pCfg,   "TargetSecret",     "");                    RC_CHECK();

            /* The iSCSI initiator needs an attached iSCSI transport driver. */
            rc = CFGMR3InsertNode(pLunL1,   "AttachedDriver", &pLunL2);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL2, "Driver",           "iSCSITCP");            RC_CHECK();
            /* Currently the transport driver has no config options. */
        }
        else if (hddType == HardDiskStorageType_VMDKImage)
        {
            ComPtr<IVMDKImage> vmdkDisk = hardDisk;
            AssertBreak (!vmdkDisk.isNull(), hrc = E_FAIL);

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
#if 1 /* Enable new VD container code (and new VMDK), as the bugs are fixed. */
            rc = CFGMR3InsertString(pLunL1, "Driver",         "VD");               RC_CHECK();
#else
            rc = CFGMR3InsertString(pLunL1, "Driver",         "VmdkHDD");               RC_CHECK();
#endif
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
            hrc = vmdkDisk->COMGETTER(FilePath)(&str);                                  H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Path",             psz);                   RC_CHECK();
            STR_FREE();
        }
        else if (hddType == HardDiskStorageType_CustomHardDisk)
        {
            ComPtr<ICustomHardDisk> customHardDisk = hardDisk;
            AssertBreak (!customHardDisk.isNull(), hrc = E_FAIL);

            rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);                 RC_CHECK();
            rc = CFGMR3InsertString(pLunL1, "Driver",         "VD");                    RC_CHECK();
            rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                           RC_CHECK();
            hrc = customHardDisk->COMGETTER(Location)(&str);                            H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Path",             psz);                   RC_CHECK();
            STR_FREE();
            hrc = customHardDisk->COMGETTER(Format)(&str);                              H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Format",           psz);                   RC_CHECK();
            STR_FREE();
        }
        else
           AssertFailed();
    }
    H();

    ComPtr<IDVDDrive> dvdDrive;
    hrc = pMachine->COMGETTER(DVDDrive)(dvdDrive.asOutParam());                     H();
    if (dvdDrive)
    {
        // ASSUME: DVD drive is always attached to LUN#2 (i.e. secondary IDE master)
        rc = CFGMR3InsertNode(pInst,    "LUN#2", &pLunL0);                          RC_CHECK();
        ComPtr<IHostDVDDrive> hostDvdDrive;
        hrc = dvdDrive->GetHostDrive(hostDvdDrive.asOutParam());                    H();
        if (hostDvdDrive)
        {
            pConsole->meDVDState = DriveState_HostDriveCaptured;
            rc = CFGMR3InsertString(pLunL0, "Driver",      "HostDVD");              RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
            hrc = hostDvdDrive->COMGETTER(Name)(&str);                              H();
            STR_CONV();
            rc = CFGMR3InsertString(pCfg,   "Path",         psz);                   RC_CHECK();
            STR_FREE();
            BOOL fPassthrough;
            hrc = dvdDrive->COMGETTER(Passthrough)(&fPassthrough);                  H();
            rc = CFGMR3InsertInteger(pCfg,  "Passthrough",  !!fPassthrough);        RC_CHECK();
        }
        else
        {
            pConsole->meDVDState = DriveState_NotMounted;
            rc = CFGMR3InsertString(pLunL0, "Driver",               "Block");       RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                       RC_CHECK();
            rc = CFGMR3InsertString(pCfg,   "Type",                 "DVD");         RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "Mountable",            1);             RC_CHECK();

            ComPtr<IDVDImage> dvdImage;
            hrc = dvdDrive->GetImage(dvdImage.asOutParam());                        H();
            if (dvdImage)
            {
                pConsole->meDVDState = DriveState_ImageMounted;
                rc = CFGMR3InsertNode(pLunL0,   "AttachedDriver", &pLunL1);         RC_CHECK();
                rc = CFGMR3InsertString(pLunL1, "Driver",          "MediaISO");     RC_CHECK();
                rc = CFGMR3InsertNode(pLunL1,   "Config", &pCfg);                   RC_CHECK();
                hrc = dvdImage->COMGETTER(FilePath)(&str);                          H();
                STR_CONV();
                rc = CFGMR3InsertString(pCfg,   "Path",             psz);           RC_CHECK();
                STR_FREE();
            }
        }
    }

    /*
     * Network adapters
     */
    rc = CFGMR3InsertNode(pDevices, "pcnet", &pDev);                                RC_CHECK();
    //rc = CFGMR3InsertNode(pDevices, "ne2000", &pDev);                               RC_CHECK();
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::NetworkAdapterCount; ulInstance++)
    {
        ComPtr<INetworkAdapter> networkAdapter;
        hrc = pMachine->GetNetworkAdapter(ulInstance, networkAdapter.asOutParam()); H();
        BOOL fEnabled = FALSE;
        hrc = networkAdapter->COMGETTER(Enabled)(&fEnabled);                        H();
        if (!fEnabled)
            continue;

        char szInstance[4]; Assert(ulInstance <= 999);
        RTStrPrintf(szInstance, sizeof(szInstance), "%lu", ulInstance);
        rc = CFGMR3InsertNode(pDev, szInstance, &pInst);                            RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "Trusted",              1); /* boolean */   RC_CHECK();
        /* the first network card gets the PCI ID 3, the followings starting from 8 */
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo", !ulInstance ? 3 : ulInstance - 1 + 8); RC_CHECK();
        Assert(!faPciDeviceNo[!ulInstance ? 3 : ulInstance - 1 + 8]);
        faPciDeviceNo[!ulInstance ? 3 : ulInstance - 1 + 8] = true;
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                 RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                              RC_CHECK();

        /*
         * The virtual hardware type.
         */
        NetworkAdapterType_T adapterType;
        hrc = networkAdapter->COMGETTER(AdapterType)(&adapterType);                 H();
        switch (adapterType)
        {
            case NetworkAdapterType_NetworkAdapterAm79C970A:
                rc = CFGMR3InsertInteger(pCfg, "Am79C973", 0);                      RC_CHECK();
                break;
            case NetworkAdapterType_NetworkAdapterAm79C973:
                rc = CFGMR3InsertInteger(pCfg, "Am79C973", 1);                      RC_CHECK();
                break;
            default:
                AssertMsgFailed(("Invalid network adapter type '%d' for slot '%d'",
                                 adapterType, ulInstance));
                return VERR_GENERAL_FAILURE;
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
        PDMMAC Mac;
        memset(&Mac, 0, sizeof(Mac));
        char *pMac = (char*)&Mac;
        for (uint32_t i = 0; i < 6; i++)
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
         * Enable the packet sniffer if requested.
         */
        BOOL fSniffer;
        hrc = networkAdapter->COMGETTER(TraceEnabled)(&fSniffer);                   H();
        if (fSniffer)
        {
            /* insert the sniffer filter driver. */
            rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                         RC_CHECK();
            rc = CFGMR3InsertString(pLunL0, "Driver", "NetSniffer");                RC_CHECK();
            rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                         RC_CHECK();
            hrc = networkAdapter->COMGETTER(TraceFile)(&str);                       H();
            if (str) /* check convention for indicating default file. */
            {
                STR_CONV();
                rc = CFGMR3InsertString(pCfg, "File", psz);                         RC_CHECK();
                STR_FREE();
            }
        }

        NetworkAttachmentType_T networkAttachment;
        hrc = networkAdapter->COMGETTER(AttachmentType)(&networkAttachment);        H();
        switch (networkAttachment)
        {
            case NetworkAttachmentType_NoNetworkAttachment:
                break;

            case NetworkAttachmentType_NATNetworkAttachment:
            {
                if (fSniffer)
                {
                    rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);       RC_CHECK();
                }
                else
                {
                    rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                 RC_CHECK();
                }
                rc = CFGMR3InsertString(pLunL0, "Driver", "NAT");                   RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                     RC_CHECK();
                /* (Port forwarding goes here.) */

                /* Configure TFTP prefix and boot filename. */
                hrc = virtualBox->COMGETTER(HomeFolder)(&str);                      H();
                STR_CONV();
                if (psz && *psz)
                {
                    char *pszTFTPPrefix = NULL;
                    RTStrAPrintf(&pszTFTPPrefix, "%s%c%s", psz, RTPATH_DELIMITER, "TFTP");
                    rc = CFGMR3InsertString(pCfg, "TFTPPrefix", pszTFTPPrefix);     RC_CHECK();
                    RTStrFree(pszTFTPPrefix);
                }
                STR_FREE();
                hrc = pMachine->COMGETTER(Name)(&str);                              H();
                STR_CONV();
                char *pszBootFile = NULL;
                RTStrAPrintf(&pszBootFile, "%s.pxe", psz);
                STR_FREE();
                rc = CFGMR3InsertString(pCfg, "BootFile", pszBootFile);             RC_CHECK();
                RTStrFree(pszBootFile);
                break;
            }

            case NetworkAttachmentType_HostInterfaceNetworkAttachment:
            {
                /*
                 * Perform the attachment if required (don't return on error!)
                 */
                hrc = pConsole->attachToHostInterface(networkAdapter);
                if (SUCCEEDED(hrc))
                {
#ifdef VBOX_WITH_UNIXY_TAP_NETWORKING
                    Assert (pConsole->maTapFD[ulInstance] >= 0);
                    if (pConsole->maTapFD[ulInstance] >= 0)
                    {
                        if (fSniffer)
                        {
                            rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0); RC_CHECK();
                        }
                        else
                        {
                            rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);         RC_CHECK();
                        }
                        rc = CFGMR3InsertString(pLunL0, "Driver", "HostInterface"); RC_CHECK();
                        rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);             RC_CHECK();
# if defined(RT_OS_SOLARIS)
                        /* Device name/number is required for Solaris as we need it for TAP PPA. */
                        Bstr tapDeviceName;
                        networkAdapter->COMGETTER(HostInterface)(tapDeviceName.asOutParam());
                        if (!tapDeviceName.isEmpty())
                            rc = CFGMR3InsertString(pCfg, "Device", Utf8Str(tapDeviceName)); RC_CHECK();

                        /* TAP setup application/script */
                        Bstr tapSetupApp;
                        networkAdapter->COMGETTER(TAPSetupApplication)(tapSetupApp.asOutParam());
                        if (!tapSetupApp.isEmpty())
                            rc = CFGMR3InsertString(pCfg, "TAPSetupApplication", Utf8Str(tapSetupApp)); RC_CHECK();

                        /* TAP terminate application/script */
                        Bstr tapTerminateApp;
                        networkAdapter->COMGETTER(TAPTerminateApplication)(tapTerminateApp.asOutParam());
                        if (!tapTerminateApp.isEmpty())
                            rc = CFGMR3InsertString(pCfg, "TAPTerminateApplication", Utf8Str(tapTerminateApp)); RC_CHECK();

                        /* "FileHandle" must NOT be inserted here, it is done in DrvTAP.cpp */

#  ifdef VBOX_WITH_CROSSBOW
                        /* Crossbow: needs the MAC address for setting up TAP. */
                        /** @todo r=bird: pass it as bytes like we do above for the NIC and drop the messy double conversion. */
                        //rc = CFGMR3InsertBytes(pCfg, "MAC", &Mac, sizeof(Mac)); RC_CHECK();
#  endif
# else
                        rc = CFGMR3InsertInteger(pCfg, "FileHandle", pConsole->maTapFD[ulInstance]); RC_CHECK();
# endif
                    }
#elif defined(RT_OS_WINDOWS)
                    if (fSniffer)
                    {
                        rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);   RC_CHECK();
                    }
                    else
                    {
                        rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);             RC_CHECK();
                    }
                    Bstr hostInterfaceName;
                    hrc = networkAdapter->COMGETTER(HostInterface)(hostInterfaceName.asOutParam()); H();
                    ComPtr<IHostNetworkInterfaceCollection> coll;
                    hrc = host->COMGETTER(NetworkInterfaces)(coll.asOutParam());    H();
                    ComPtr<IHostNetworkInterface> hostInterface;
                    rc = coll->FindByName(hostInterfaceName, hostInterface.asOutParam());
                    if (!SUCCEEDED(rc))
                    {
                        AssertMsgFailed(("Cannot get GUID for host interface '%ls'\n", hostInterfaceName));
                        hrc = networkAdapter->Detach();                              H();
                    }
                    else
                    {
                        rc = CFGMR3InsertString(pLunL0, "Driver", "HostInterface");     RC_CHECK();
                        rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                 RC_CHECK();
                        rc = CFGMR3InsertString(pCfg, "HostInterfaceName", Utf8Str(hostInterfaceName)); RC_CHECK();
                        Guid hostIFGuid;
                        hrc = hostInterface->COMGETTER(Id)(hostIFGuid.asOutParam());    H();
                        char szDriverGUID[256] = {0};
                        /* add curly brackets */
                        szDriverGUID[0] = '{';
                        strcpy(szDriverGUID + 1, hostIFGuid.toString().raw());
                        strcat(szDriverGUID, "}");
                        rc = CFGMR3InsertBytes(pCfg, "GUID", szDriverGUID, sizeof(szDriverGUID)); RC_CHECK();
                    }
#else
# error "Port me"
#endif
                }
                else
                {
                    switch (hrc)
                    {
#ifdef RT_OS_LINUX
                        case VERR_ACCESS_DENIED:
                            return VMSetError(pVM, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS,  N_(
                                             "Failed to open '/dev/net/tun' for read/write access. Please check the "
                                             "permissions of that node. Either do 'chmod 0666 /dev/net/tun' or "
                                             "change the group of that node and get member of that group. Make "
                                             "sure that these changes are permanently in particular if you are "
                                             "using udev"));
#endif /* RT_OS_LINUX */
                        default:
                            AssertMsgFailed(("Could not attach to host interface! Bad!\n"));
                            return VMSetError(pVM, VERR_HOSTIF_INIT_FAILED, RT_SRC_POS, N_(
                                             "Failed to initialize Host Interface Networking"));
                    }
                }
                break;
            }

            case NetworkAttachmentType_InternalNetworkAttachment:
            {
                hrc = networkAdapter->COMGETTER(InternalNetwork)(&str);                 H();
                STR_CONV();
                if (psz && *psz)
                {
                    if (fSniffer)
                    {
                        rc = CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL0);       RC_CHECK();
                    }
                    else
                    {
                        rc = CFGMR3InsertNode(pInst, "LUN#0", &pLunL0);                 RC_CHECK();
                    }
                    rc = CFGMR3InsertString(pLunL0, "Driver", "IntNet");                RC_CHECK();
                    rc = CFGMR3InsertNode(pLunL0, "Config", &pCfg);                     RC_CHECK();
                    rc = CFGMR3InsertString(pCfg, "Network", psz);                      RC_CHECK();
                }
                STR_FREE();
                break;
            }

            default:
                AssertMsgFailed(("should not get here!\n"));
                break;
        }
    }

    /*
     * Serial (UART) Ports
     */
    rc = CFGMR3InsertNode(pDevices, "serial", &pDev);                               RC_CHECK();
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::SerialPortCount; ulInstance++)
    {
        ComPtr<ISerialPort> serialPort;
        hrc = pMachine->GetSerialPort (ulInstance, serialPort.asOutParam());        H();
        BOOL fEnabled = FALSE;
        if (serialPort)
            hrc = serialPort->COMGETTER(Enabled)(&fEnabled);                        H();
        if (!fEnabled)
            continue;

        char szInstance[4]; Assert(ulInstance <= 999);
        RTStrPrintf(szInstance, sizeof(szInstance), "%lu", ulInstance);

        rc = CFGMR3InsertNode(pDev, szInstance, &pInst);                            RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                              RC_CHECK();

        ULONG ulIRQ, ulIOBase;
        PortMode_T HostMode;
        Bstr  path;
        BOOL  fServer;
        hrc = serialPort->COMGETTER(HostMode)(&HostMode);                           H();
        hrc = serialPort->COMGETTER(IRQ)(&ulIRQ);                                   H();
        hrc = serialPort->COMGETTER(IOBase)(&ulIOBase);                             H();
        hrc = serialPort->COMGETTER(Path)(path.asOutParam());                       H();
        hrc = serialPort->COMGETTER(Server)(&fServer);                              H();
        rc = CFGMR3InsertInteger(pCfg,   "IRQ", ulIRQ);                             RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,   "IOBase", ulIOBase);                       RC_CHECK();
        if (HostMode != PortMode_DisconnectedPort)
        {
            rc = CFGMR3InsertNode(pInst,     "LUN#0", &pLunL0);                     RC_CHECK();
            if (HostMode == PortMode_HostPipePort)
            {
                rc = CFGMR3InsertString(pLunL0,  "Driver", "Char");                 RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,    "AttachedDriver", &pLunL1);        RC_CHECK();
                rc = CFGMR3InsertString(pLunL1,  "Driver", "NamedPipe");            RC_CHECK();
                rc = CFGMR3InsertNode(pLunL1,    "Config", &pLunL2);                RC_CHECK();
                rc = CFGMR3InsertString(pLunL2,  "Location", Utf8Str(path));        RC_CHECK();
                rc = CFGMR3InsertInteger(pLunL2, "IsServer", fServer);              RC_CHECK();
            }
            else if (HostMode == PortMode_HostDevicePort)
            {
                rc = CFGMR3InsertString(pLunL0,  "Driver", "Host Serial");          RC_CHECK();
                rc = CFGMR3InsertNode(pLunL0,    "Config", &pLunL1);                RC_CHECK();
                rc = CFGMR3InsertString(pLunL1,  "DevicePath", Utf8Str(path));      RC_CHECK();
            }
        }
    }

    /*
     * Parallel (LPT) Ports
     */
    rc = CFGMR3InsertNode(pDevices, "parallel", &pDev);                             RC_CHECK();
    for (ULONG ulInstance = 0; ulInstance < SchemaDefs::ParallelPortCount; ulInstance++)
    {
        ComPtr<IParallelPort> parallelPort;
        hrc = pMachine->GetParallelPort (ulInstance, parallelPort.asOutParam());    H();
        BOOL fEnabled = FALSE;
        if (parallelPort)
            hrc = parallelPort->COMGETTER(Enabled)(&fEnabled);                      H();
        if (!fEnabled)
            continue;

        char szInstance[4]; Assert(ulInstance <= 999);
        RTStrPrintf(szInstance, sizeof(szInstance), "%lu", ulInstance);

        rc = CFGMR3InsertNode(pDev, szInstance, &pInst);                            RC_CHECK();
        rc = CFGMR3InsertNode(pInst, "Config", &pCfg);                              RC_CHECK();

        ULONG ulIRQ, ulIOBase;
        Bstr  DevicePath;
        hrc = parallelPort->COMGETTER(IRQ)(&ulIRQ);                                 H();
        hrc = parallelPort->COMGETTER(IOBase)(&ulIOBase);                           H();
        hrc = parallelPort->COMGETTER(Path)(DevicePath.asOutParam());               H();
        rc = CFGMR3InsertInteger(pCfg,   "IRQ", ulIRQ);                             RC_CHECK();
        rc = CFGMR3InsertInteger(pCfg,   "IOBase", ulIOBase);                       RC_CHECK();
        rc = CFGMR3InsertNode(pInst,     "LUN#0", &pLunL0);                         RC_CHECK();
        rc = CFGMR3InsertString(pLunL0,  "Driver", "HostParallel");                 RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,    "AttachedDriver", &pLunL1);                RC_CHECK();
        rc = CFGMR3InsertString(pLunL1,  "DevicePath", Utf8Str(DevicePath));        RC_CHECK();
    }

    /*
     * VMM Device
     */
    rc = CFGMR3InsertNode(pDevices, "VMMDev", &pDev);                               RC_CHECK();
    rc = CFGMR3InsertNode(pDev,     "0", &pInst);                                   RC_CHECK();
    rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                               RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
    rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          4);                     RC_CHECK();
    Assert(!faPciDeviceNo[4]);
    faPciDeviceNo[4] = true;
    rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();

    /* the VMM device's Main driver */
    rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainVMMDev");          RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
    VMMDev *pVMMDev = pConsole->mVMMDev;
    rc = CFGMR3InsertInteger(pCfg,  "Object", (uintptr_t)pVMMDev);                  RC_CHECK();

    /*
     * Attach the status driver.
     */
    rc = CFGMR3InsertNode(pInst,    "LUN#999", &pLunL0);                        RC_CHECK();
    rc = CFGMR3InsertString(pLunL0, "Driver",               "MainStatus");      RC_CHECK();
    rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                           RC_CHECK();
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
     * AC'97 ICH audio
     */
    ComPtr<IAudioAdapter> audioAdapter;
    hrc = pMachine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());             H();
    BOOL enabled = FALSE;
    if (audioAdapter)
    {
        hrc = audioAdapter->COMGETTER(Enabled)(&enabled);                           H();
    }
    if (enabled)
    {
        rc = CFGMR3InsertNode(pDevices, "ichac97", &pDev); /* ichac97 */
        rc = CFGMR3InsertNode(pDev,     "0", &pInst);
        rc = CFGMR3InsertInteger(pInst, "Trusted",              1);     /* boolean */   RC_CHECK();
        rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          5);                     RC_CHECK();
        Assert(!faPciDeviceNo[5]);
        faPciDeviceNo[5] = true;
        rc = CFGMR3InsertInteger(pInst, "PCIFunctionNo",        0);                     RC_CHECK();
        rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);

        /* the Audio driver */
        rc = CFGMR3InsertNode(pInst,    "LUN#0", &pLunL0);                              RC_CHECK();
        rc = CFGMR3InsertString(pLunL0, "Driver",               "AUDIO");               RC_CHECK();
        rc = CFGMR3InsertNode(pLunL0,   "Config", &pCfg);                               RC_CHECK();
        AudioDriverType_T audioDriver;
        hrc = audioAdapter->COMGETTER(AudioDriver)(&audioDriver);                       H();
        switch (audioDriver)
        {
            case AudioDriverType_NullAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "null");                   RC_CHECK();
                break;
            }
#ifdef RT_OS_WINDOWS
#ifdef VBOX_WITH_WINMM
            case AudioDriverType_WINMMAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "winmm");                  RC_CHECK();
                break;
            }
#endif
            case AudioDriverType_DSOUNDAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "dsound");                 RC_CHECK();
                break;
            }
#endif /* RT_OS_WINDOWS */
#ifdef RT_OS_LINUX
            case AudioDriverType_OSSAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "oss");                    RC_CHECK();
                break;
            }
# ifdef VBOX_WITH_ALSA
            case AudioDriverType_ALSAAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "alsa");                   RC_CHECK();
                break;
            }
# endif
#endif /* RT_OS_LINUX */
#ifdef RT_OS_DARWIN
            case AudioDriverType_CoreAudioDriver:
            {
                rc = CFGMR3InsertString(pCfg, "AudioDriver", "coreaudio");              RC_CHECK();
                break;
            }
#endif
        }
    }

    /*
     * The USB Controller.
     */
    ComPtr<IUSBController> USBCtlPtr;
    hrc = pMachine->COMGETTER(USBController)(USBCtlPtr.asOutParam());
    if (USBCtlPtr)
    {
        BOOL fEnabled;
        hrc = USBCtlPtr->COMGETTER(Enabled)(&fEnabled);                                 H();
        if (fEnabled)
        {
            rc = CFGMR3InsertNode(pDevices, "usb-ohci", &pDev);                         RC_CHECK();
            rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
            rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
            rc = CFGMR3InsertInteger(pInst, "Trusted",              1); /* boolean */   RC_CHECK();
            rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          6);                 RC_CHECK();
            Assert(!faPciDeviceNo[6]);
            faPciDeviceNo[6] = true;
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
            rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapUSBLed);RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "First",    0);                             RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                             RC_CHECK();

#ifdef VBOX_WITH_EHCI
            rc = CFGMR3InsertNode(pDevices, "usb-ehci", &pDev);                         RC_CHECK();
            rc = CFGMR3InsertNode(pDev,     "0", &pInst);                               RC_CHECK();
            rc = CFGMR3InsertNode(pInst,    "Config", &pCfg);                           RC_CHECK();
            rc = CFGMR3InsertInteger(pInst, "Trusted",              1); /* boolean */   RC_CHECK();
            rc = CFGMR3InsertInteger(pInst, "PCIDeviceNo",          11);                RC_CHECK();
            Assert(!faPciDeviceNo[11]);
            faPciDeviceNo[11] = true;
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
            rc = CFGMR3InsertInteger(pCfg,  "papLeds", (uintptr_t)&pConsole->mapUSBLed);RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "First",    0);                             RC_CHECK();
            rc = CFGMR3InsertInteger(pCfg,  "Last",     0);                             RC_CHECK();
#endif
        }
    }

    /*
     * Clipboard
     */
    {
        ClipboardMode_T mode = ClipboardMode_ClipDisabled;
        hrc = pMachine->COMGETTER(ClipboardMode) (&mode);                               H();

        if (mode != ClipboardMode_ClipDisabled)
        {
            /* Load the service */
            rc = pConsole->mVMMDev->hgcmLoadService ("VBoxSharedClipboard", "VBoxSharedClipboard");

            if (VBOX_FAILURE (rc))
            {
                LogRel(("VBoxSharedClipboard is not available. rc = %Vrc\n", rc));
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
                    case ClipboardMode_ClipDisabled:
                    {
                        LogRel(("VBoxSharedClipboard mode: Off\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_OFF;
                        break;
                    }
                    case ClipboardMode_ClipGuestToHost:
                    {
                        LogRel(("VBoxSharedClipboard mode: Guest to Host\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST;
                        break;
                    }
                    case ClipboardMode_ClipHostToGuest:
                    {
                        LogRel(("VBoxSharedClipboard mode: Host to Guest\n"));
                        parm.u.uint32 = VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST;
                        break;
                    }
                    case ClipboardMode_ClipBidirectional:
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

    /*
     * CFGM overlay handling.
     *
     * Here we check the extra data entries for CFGM values
     * and create the nodes and insert the values on the fly. Existing
     * values will be removed and reinserted. If a value is a valid number,
     * it will be inserted as a number, otherwise as a string.
     *
     * We first perform a run on global extra data, then on the machine
     * extra data to support global settings with local overrides.
     *
     */
    Bstr strExtraDataKey;
    bool fGlobalExtraData = true;
    for (;;)
    {
        Bstr strNextExtraDataKey;
        Bstr strExtraDataValue;

        /* get the next key */
        if (fGlobalExtraData)
            hrc = virtualBox->GetNextExtraDataKey(strExtraDataKey, strNextExtraDataKey.asOutParam(),
                                                  strExtraDataValue.asOutParam());
        else
            hrc = pMachine->GetNextExtraDataKey(strExtraDataKey, strNextExtraDataKey.asOutParam(),
                                                strExtraDataValue.asOutParam());

        /* stop if for some reason there's nothing more to request */
        if (FAILED(hrc) || !strNextExtraDataKey)
        {
            /* if we're out of global keys, continue with machine, otherwise we're done */
            if (fGlobalExtraData)
            {
                fGlobalExtraData = false;
                strExtraDataKey.setNull();
                continue;
            }
            break;
        }

        strExtraDataKey = strNextExtraDataKey;
        Utf8Str strExtraDataKeyUtf8 = Utf8Str(strExtraDataKey);

        /* we only care about keys starting with "VBoxInternal/" */
        if (strncmp(strExtraDataKeyUtf8.raw(), "VBoxInternal/", 13) != 0)
            continue;
        char *pszExtraDataKey = (char*)strExtraDataKeyUtf8.raw() + 13;

        /* the key will be in the format "Node1/Node2/Value" or simply "Value". */
        PCFGMNODE pNode;
        char *pszCFGMValueName = strrchr(pszExtraDataKey, '/');
        if (pszCFGMValueName)
        {
            /* terminate the node and advance to the value */
            *pszCFGMValueName = '\0';
            pszCFGMValueName++;

            /* does the node already exist? */
            pNode = CFGMR3GetChild(pRoot, pszExtraDataKey);
            if (pNode)
            {
                /* the value might already exist, remove it to be safe */
                CFGMR3RemoveValue(pNode, pszCFGMValueName);
            }
            else
            {
                /* create the node */
                rc = CFGMR3InsertNode(pRoot, pszExtraDataKey, &pNode);
                AssertMsgRC(rc, ("failed to insert node '%s'\n", pszExtraDataKey));
                if (VBOX_FAILURE(rc) || !pNode)
                    continue;
            }
        }
        else
        {
            pNode = pRoot;
            pszCFGMValueName = pszExtraDataKey;
            pszExtraDataKey--;

            /* the value might already exist, remove it to be safe */
            CFGMR3RemoveValue(pNode, pszCFGMValueName);
        }

        /* now let's have a look at the value */
        Utf8Str strCFGMValueUtf8 = Utf8Str(strExtraDataValue);
        const char *pszCFGMValue = strCFGMValueUtf8.raw();
        /* empty value means remove value which we've already done */
        if (pszCFGMValue && *pszCFGMValue)
        {
            /* if it's a valid number, we'll insert it as such, otherwise string */
            uint64_t u64Value;
            if (RTStrToUInt64Ex(pszCFGMValue, NULL, 0, &u64Value) == VINF_SUCCESS)
            {
                rc = CFGMR3InsertInteger(pNode, pszCFGMValueName, u64Value);
            }
            else
            {
                rc = CFGMR3InsertString(pNode, pszCFGMValueName, pszCFGMValue);
            }
            AssertMsgRC(rc, ("failed to insert CFGM value '%s' to key '%s'\n", pszCFGMValue, pszExtraDataKey));
        }
    }

#undef H
#undef RC_CHECK
#undef STR_FREE
#undef STR_CONV

    /* Register VM state change handler */
    int rc2 = VMR3AtStateRegister (pVM, Console::vmstateChangeCallback, pConsole);
    AssertRC (rc2);
    if (VBOX_SUCCESS (rc))
        rc = rc2;

    /* Register VM runtime error handler */
    rc2 = VMR3AtRuntimeErrorRegister (pVM, Console::setVMRuntimeErrorCallback, pConsole);
    AssertRC (rc2);
    if (VBOX_SUCCESS (rc))
        rc = rc2;

    /* Save the VM pointer in the machine object */
    pConsole->mpVM = pVM;

    LogFlowFunc (("vrc = %Vrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}


