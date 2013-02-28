/* $Id$ */
/** @file
 * tstDeviceStructSizeGC - Generate structure member and size checks from the RC perspective.
 *
 * This is built using the VBoxRc template but linked into a host
 * ring-3 executable, rather hacky.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*
 * Sanity checks.
 */
#ifndef IN_RC
# error Incorrect template!
#endif
#if defined(IN_RING3) || defined(IN_RING0)
# error Incorrect template!
#endif


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define VBOX_DEVICE_STRUCT_TESTCASE
#define VBOX_WITH_HGCM                  /* grumble */
#undef LOG_GROUP
#include "../Bus/DevPCI.cpp" /* must be first! */
#undef LOG_GROUP
#include "../Bus/DevPciIch9.cpp"
#undef LOG_GROUP
#include "../Graphics/DevVGA.cpp"
#undef LOG_GROUP
#include "../Input/DevPS2.cpp"
#undef LOG_GROUP
#include "../Input/PS2K.cpp"
#undef LOG_GROUP
#include "../Network/DevPCNet.cpp"
#undef LOG_GROUP
#include "../PC/DevACPI.cpp"
#undef LOG_GROUP
#include "../PC/DevPIC.cpp"
#undef LOG_GROUP
#include "../PC/DevPit-i8254.cpp"
#undef LOG_GROUP
#include "../PC/DevRTC.cpp"
#undef LOG_GROUP
#include "../PC/DevAPIC.cpp"
#undef LOG_GROUP
#include "../PC/DevIoApic.cpp"
#undef LOG_GROUP
#include "../Storage/DevATA.cpp"
#ifdef VBOX_WITH_USB
# undef LOG_GROUP
# include "../USB/DevOHCI.cpp"
# ifdef VBOX_WITH_EHCI_IMPL
#  include "../USB/DevEHCI.cpp"
# endif
#endif
#undef LOG_GROUP
#include "../VMMDev/VMMDev.cpp"
#undef LOG_GROUP
#include "../Parallel/DevParallel.cpp"
#undef LOG_GROUP
#include "../Serial/DevSerial.cpp"
#ifdef VBOX_WITH_AHCI
# undef LOG_GROUP
# include "../Storage/DevAHCI.cpp"
#endif
#ifdef VBOX_WITH_E1000
# undef LOG_GROUP
# include "../Network/DevE1000.cpp"
#endif
#ifdef VBOX_WITH_VIRTIO
# undef LOG_GROUP
# include "../Network/DevVirtioNet.cpp"
#endif
#ifdef VBOX_WITH_BUSLOGIC
# undef LOG_GROUP
# include "../Storage/DevBusLogic.cpp"
#endif
#ifdef VBOX_WITH_LSILOGIC
# undef LOG_GROUP
# include "../Storage/DevLsiLogicSCSI.cpp"
#endif
#undef LOG_GROUP
#include "../PC/DevHPET.cpp"

/* we don't use iprt here because we're pretending to be in GC! */
#include <stdio.h>

#define GEN_CHECK_SIZE(s)           printf("    CHECK_SIZE(%s, %d);\n", #s, (int)sizeof(s))
#define GEN_CHECK_OFF(s, m)         printf("    CHECK_OFF(%s, %d, %s);\n", #s, (int)RT_OFFSETOF(s, m), #m)
#define GEN_CHECK_PADDING(s, m, a)  printf("    CHECK_PADDING(%s, %s, %u);\n", #s, #m, (a))

int main()
{
    /* misc */
    GEN_CHECK_SIZE(PDMDEVINS);
    GEN_CHECK_OFF(PDMDEVINS, Internal);
    GEN_CHECK_OFF(PDMDEVINS, pReg);
    GEN_CHECK_OFF(PDMDEVINS, pCfg);
    GEN_CHECK_OFF(PDMDEVINS, iInstance);
    GEN_CHECK_OFF(PDMDEVINS, IBase);
    GEN_CHECK_OFF(PDMDEVINS, pHlpR3);
    GEN_CHECK_OFF(PDMDEVINS, pHlpR0);
    GEN_CHECK_OFF(PDMDEVINS, pHlpRC);
    GEN_CHECK_OFF(PDMDEVINS, pvInstanceDataR3);
    GEN_CHECK_OFF(PDMDEVINS, pvInstanceDataR0);
    GEN_CHECK_OFF(PDMDEVINS, pvInstanceDataRC);
    GEN_CHECK_OFF(PDMDEVINS, achInstanceData);

    /* DevPCI.cpp */
    GEN_CHECK_SIZE(PCIDEVICE);
    GEN_CHECK_SIZE(PCIDEVICEINT);
    GEN_CHECK_SIZE(PCIIOREGION);
    GEN_CHECK_OFF(PCIDEVICE, config);
    GEN_CHECK_OFF(PCIDEVICE, devfn);
    GEN_CHECK_OFF(PCIDEVICE, name);
    GEN_CHECK_OFF(PCIDEVICE, pDevIns);
    GEN_CHECK_OFF(PCIDEVICE, Int);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[1]);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[PCI_NUM_REGIONS - 1]);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[0].addr);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[0].size);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[0].type);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[0].padding);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pBusR3);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pBusR0);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pBusRC);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pfnConfigRead);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pfnConfigWrite);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.fFlags);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.uIrqPinState);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pfnBridgeConfigRead);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pfnBridgeConfigWrite);
    GEN_CHECK_PADDING(PCIDEVICE, Int, 8);
    GEN_CHECK_SIZE(PIIX3State);
    GEN_CHECK_SIZE(PCIBUS);
    GEN_CHECK_OFF(PCIBUS, iBus);
    GEN_CHECK_OFF(PCIBUS, iDevSearch);
    GEN_CHECK_OFF(PCIBUS, cBridges);
    GEN_CHECK_OFF(PCIBUS, devices);
    GEN_CHECK_OFF(PCIBUS, devices[1]);
    GEN_CHECK_OFF(PCIBUS, pDevInsR3);
    GEN_CHECK_OFF(PCIBUS, pPciHlpR3);
    GEN_CHECK_OFF(PCIBUS, papBridgesR3);
    GEN_CHECK_OFF(PCIBUS, pDevInsR0);
    GEN_CHECK_OFF(PCIBUS, pPciHlpR0);
    GEN_CHECK_OFF(PCIBUS, pDevInsRC);
    GEN_CHECK_OFF(PCIBUS, pPciHlpRC);
    GEN_CHECK_OFF(PCIBUS, PciDev);
    GEN_CHECK_SIZE(PCIGLOBALS);
    GEN_CHECK_OFF(PCIGLOBALS, pci_bios_io_addr);
    GEN_CHECK_OFF(PCIGLOBALS, pci_bios_mem_addr);
    GEN_CHECK_OFF(PCIGLOBALS, pci_irq_levels);
    GEN_CHECK_OFF(PCIGLOBALS, pci_irq_levels[1]);
    GEN_CHECK_OFF(PCIGLOBALS, fUseIoApic);
    GEN_CHECK_OFF(PCIGLOBALS, pci_apic_irq_levels);
    GEN_CHECK_OFF(PCIGLOBALS, pci_apic_irq_levels[1]);
    GEN_CHECK_OFF(PCIGLOBALS, acpi_irq_level);
    GEN_CHECK_OFF(PCIGLOBALS, acpi_irq);
    GEN_CHECK_OFF(PCIGLOBALS, uConfigReg);
    GEN_CHECK_OFF(PCIGLOBALS, pDevInsR3);
    GEN_CHECK_OFF(PCIGLOBALS, pDevInsR0);
    GEN_CHECK_OFF(PCIGLOBALS, pDevInsRC);
    GEN_CHECK_OFF(PCIGLOBALS, PIIX3State);
    GEN_CHECK_OFF(PCIGLOBALS, PciBus);

    /* DevPciIch9.cpp */
    GEN_CHECK_SIZE(ICH9PCIBUS);
    GEN_CHECK_OFF(ICH9PCIBUS, iBus);
    GEN_CHECK_OFF(ICH9PCIBUS, cBridges);
    GEN_CHECK_OFF(ICH9PCIBUS, apDevices);
    GEN_CHECK_OFF(ICH9PCIBUS, apDevices[1]);
    GEN_CHECK_OFF(ICH9PCIBUS, pDevInsR3);
    GEN_CHECK_OFF(ICH9PCIBUS, pPciHlpR3);
    GEN_CHECK_OFF(ICH9PCIBUS, papBridgesR3);
    GEN_CHECK_OFF(ICH9PCIBUS, pDevInsR0);
    GEN_CHECK_OFF(ICH9PCIBUS, pPciHlpR0);
    GEN_CHECK_OFF(ICH9PCIBUS, pDevInsRC);
    GEN_CHECK_OFF(ICH9PCIBUS, pPciHlpRC);
    GEN_CHECK_OFF(ICH9PCIBUS, aPciDev);
    GEN_CHECK_SIZE(ICH9PCIGLOBALS);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, pDevInsR3);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, pDevInsR0);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, pDevInsRC);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, uConfigReg);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, uaPciApicIrqLevels);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, uaPciApicIrqLevels[1]);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, uPciBiosIo);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, uPciBiosMmio);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, uBus);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, u64PciConfigMMioAddress);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, u64PciConfigMMioLength);
    GEN_CHECK_OFF(ICH9PCIGLOBALS, aPciBus);

    /* DevVGA.cpp */
    GEN_CHECK_SIZE(VGASTATE);
    GEN_CHECK_OFF(VGASTATE, vram_ptrR3);
    GEN_CHECK_OFF(VGASTATE, get_bpp);
    GEN_CHECK_OFF(VGASTATE, get_offsets);
    GEN_CHECK_OFF(VGASTATE, get_resolution);
    GEN_CHECK_OFF(VGASTATE, rgb_to_pixel);
    GEN_CHECK_OFF(VGASTATE, cursor_invalidate);
    GEN_CHECK_OFF(VGASTATE, cursor_draw_line);
    GEN_CHECK_OFF(VGASTATE, vram_size);
    GEN_CHECK_OFF(VGASTATE, latch);
    GEN_CHECK_OFF(VGASTATE, sr_index);
    GEN_CHECK_OFF(VGASTATE, sr);
    GEN_CHECK_OFF(VGASTATE, sr[1]);
    GEN_CHECK_OFF(VGASTATE, gr_index);
    GEN_CHECK_OFF(VGASTATE, gr);
    GEN_CHECK_OFF(VGASTATE, gr[1]);
    GEN_CHECK_OFF(VGASTATE, ar_index);
    GEN_CHECK_OFF(VGASTATE, ar);
    GEN_CHECK_OFF(VGASTATE, ar[1]);
    GEN_CHECK_OFF(VGASTATE, ar_flip_flop);
    GEN_CHECK_OFF(VGASTATE, cr_index);
    GEN_CHECK_OFF(VGASTATE, cr);
    GEN_CHECK_OFF(VGASTATE, cr[1]);
    GEN_CHECK_OFF(VGASTATE, msr);
    GEN_CHECK_OFF(VGASTATE, msr);
    GEN_CHECK_OFF(VGASTATE, fcr);
    GEN_CHECK_OFF(VGASTATE, st00);
    GEN_CHECK_OFF(VGASTATE, st01);
    GEN_CHECK_OFF(VGASTATE, dac_state);
    GEN_CHECK_OFF(VGASTATE, dac_sub_index);
    GEN_CHECK_OFF(VGASTATE, dac_read_index);
    GEN_CHECK_OFF(VGASTATE, dac_write_index);
    GEN_CHECK_OFF(VGASTATE, dac_cache);
    GEN_CHECK_OFF(VGASTATE, dac_cache[1]);
    GEN_CHECK_OFF(VGASTATE, palette);
    GEN_CHECK_OFF(VGASTATE, palette[1]);
    GEN_CHECK_OFF(VGASTATE, bank_offset);
#ifdef CONFIG_BOCHS_VBE
    GEN_CHECK_OFF(VGASTATE, vbe_index);
    GEN_CHECK_OFF(VGASTATE, vbe_regs);
    GEN_CHECK_OFF(VGASTATE, vbe_regs[1]);
    GEN_CHECK_OFF(VGASTATE, vbe_regs[VBE_DISPI_INDEX_NB - 1]);
    GEN_CHECK_OFF(VGASTATE, vbe_start_addr);
    GEN_CHECK_OFF(VGASTATE, vbe_line_offset);
    GEN_CHECK_OFF(VGASTATE, vbe_bank_max);
#endif
    GEN_CHECK_OFF(VGASTATE, font_offsets);
    GEN_CHECK_OFF(VGASTATE, font_offsets[1]);
    GEN_CHECK_OFF(VGASTATE, graphic_mode);
    GEN_CHECK_OFF(VGASTATE, shift_control);
    GEN_CHECK_OFF(VGASTATE, double_scan);
    GEN_CHECK_OFF(VGASTATE, line_offset);
    GEN_CHECK_OFF(VGASTATE, line_compare);
    GEN_CHECK_OFF(VGASTATE, start_addr);
    GEN_CHECK_OFF(VGASTATE, plane_updated);
    GEN_CHECK_OFF(VGASTATE, last_cw);
    GEN_CHECK_OFF(VGASTATE, last_ch);
    GEN_CHECK_OFF(VGASTATE, last_width);
    GEN_CHECK_OFF(VGASTATE, last_height);
    GEN_CHECK_OFF(VGASTATE, last_scr_width);
    GEN_CHECK_OFF(VGASTATE, last_scr_height);
    GEN_CHECK_OFF(VGASTATE, last_bpp);
    GEN_CHECK_OFF(VGASTATE, cursor_start);
    GEN_CHECK_OFF(VGASTATE, cursor_end);
    GEN_CHECK_OFF(VGASTATE, cursor_offset);
    GEN_CHECK_OFF(VGASTATE, invalidated_y_table);
    GEN_CHECK_OFF(VGASTATE, invalidated_y_table[1]);
    GEN_CHECK_OFF(VGASTATE, invalidated_y_table[(VGA_MAX_HEIGHT / 32) - 1]);
    GEN_CHECK_OFF(VGASTATE, last_palette);
    GEN_CHECK_OFF(VGASTATE, last_palette[1]);
    GEN_CHECK_OFF(VGASTATE, last_ch_attr);
    GEN_CHECK_OFF(VGASTATE, last_ch_attr[CH_ATTR_SIZE - 1]);
    GEN_CHECK_OFF(VGASTATE, u32Marker);
    GEN_CHECK_OFF(VGASTATE, pDevInsRC);
    GEN_CHECK_OFF(VGASTATE, vram_ptrRC);
    GEN_CHECK_OFF(VGASTATE, RCPtrLFBHandler);
    GEN_CHECK_OFF(VGASTATE, pDevInsR3);
#ifdef VBOX_WITH_HGSMI
    GEN_CHECK_OFF(VGASTATE, pHGSMI);
#endif
#ifdef VBOX_WITH_VDMA
    GEN_CHECK_OFF(VGASTATE, pVdma);
#endif
    GEN_CHECK_OFF(VGASTATE, IBase);
    GEN_CHECK_OFF(VGASTATE, IPort);
#if defined(VBOX_WITH_HGSMI) && (defined(VBOX_WITH_VIDEOHWACCEL) || defined(VBOX_WITH_CRHGSMI))
    GEN_CHECK_OFF(VGASTATE, IVBVACallbacks);
#endif
    GEN_CHECK_OFF(VGASTATE, pDrvBase);
    GEN_CHECK_OFF(VGASTATE, pDrv);
    GEN_CHECK_OFF(VGASTATE, RefreshTimer);
    GEN_CHECK_OFF(VGASTATE, pDevInsR0);
    GEN_CHECK_OFF(VGASTATE, cMonitors);
    GEN_CHECK_OFF(VGASTATE, cMilliesRefreshInterval);
    GEN_CHECK_OFF(VGASTATE, au32DirtyBitmap);
    GEN_CHECK_OFF(VGASTATE, au32DirtyBitmap[1]);
    GEN_CHECK_OFF(VGASTATE, au32DirtyBitmap[(VGA_VRAM_MAX / PAGE_SIZE / 32) - 1]);
    GEN_CHECK_OFF(VGASTATE, fHasDirtyBits);
    GEN_CHECK_OFF(VGASTATE, fLFBUpdated);
    GEN_CHECK_OFF(VGASTATE, fGCEnabled);
    GEN_CHECK_OFF(VGASTATE, fR0Enabled);
    GEN_CHECK_OFF(VGASTATE, fRemappedVGA);
    GEN_CHECK_OFF(VGASTATE, fRenderVRAM);
    GEN_CHECK_OFF(VGASTATE, GCPhysVRAM);
    GEN_CHECK_OFF(VGASTATE, CritSect);
    GEN_CHECK_OFF(VGASTATE, Dev);
    GEN_CHECK_OFF(VGASTATE, StatRZMemoryRead);
    GEN_CHECK_OFF(VGASTATE, StatR3MemoryRead);
    GEN_CHECK_OFF(VGASTATE, StatRZMemoryWrite);
    GEN_CHECK_OFF(VGASTATE, StatR3MemoryWrite);
#ifdef VBE_BYTEWISE_IO
    GEN_CHECK_OFF(VGASTATE, fReadVBEData);
    GEN_CHECK_OFF(VGASTATE, fWriteVBEData);
    GEN_CHECK_OFF(VGASTATE, fReadVBEIndex);
    GEN_CHECK_OFF(VGASTATE, fWriteVBEIndex);
    GEN_CHECK_OFF(VGASTATE, cbWriteVBEData);
    GEN_CHECK_OFF(VGASTATE, cbWriteVBEIndex);
# ifdef VBE_NEW_DYN_LIST
    GEN_CHECK_OFF(VGASTATE, cbWriteVBEExtraAddress);
# endif
#endif
#ifdef VBE_NEW_DYN_LIST
    GEN_CHECK_OFF(VGASTATE, pu8VBEExtraData);
    GEN_CHECK_OFF(VGASTATE, cbVBEExtraData);
    GEN_CHECK_OFF(VGASTATE, u16VBEExtraAddress);
#endif
    GEN_CHECK_OFF(VGASTATE, pu8Logo);
    GEN_CHECK_OFF(VGASTATE, pszLogoFile);
    GEN_CHECK_OFF(VGASTATE, pu8LogoBitmap);
    GEN_CHECK_OFF(VGASTATE, offLogoData);
    GEN_CHECK_OFF(VGASTATE, cbLogo);
    GEN_CHECK_OFF(VGASTATE, LogoCommand);
    GEN_CHECK_OFF(VGASTATE, cxLogo);
    GEN_CHECK_OFF(VGASTATE, cyLogo);
    GEN_CHECK_OFF(VGASTATE, cLogoPlanes);
    GEN_CHECK_OFF(VGASTATE, cLogoBits);
    GEN_CHECK_OFF(VGASTATE, LogoCompression);
    GEN_CHECK_OFF(VGASTATE, cLogoUsedColors);
    GEN_CHECK_OFF(VGASTATE, cLogoPalEntries);
    GEN_CHECK_OFF(VGASTATE, fLogoClearScreen);
    GEN_CHECK_OFF(VGASTATE, au32LogoPalette);
    GEN_CHECK_OFF(VGASTATE, pu8VgaBios);
    GEN_CHECK_OFF(VGASTATE, cbVgaBios);
    GEN_CHECK_OFF(VGASTATE, pszVgaBiosFile);
#ifdef VBOX_WITH_HGSMI
    GEN_CHECK_OFF(VGASTATE, IOPortBase);
#endif
#ifdef VBOX_WITH_WDDM
    GEN_CHECK_OFF(VGASTATE, fGuestCaps);
#endif

    /* Input/pckbd.c */
    GEN_CHECK_SIZE(MouseCmdQueue);
    GEN_CHECK_OFF(MouseCmdQueue, data);
    GEN_CHECK_OFF(MouseCmdQueue, rptr);
    GEN_CHECK_OFF(MouseCmdQueue, wptr);
    GEN_CHECK_OFF(MouseCmdQueue, count);
    GEN_CHECK_SIZE(MouseEventQueue);
    GEN_CHECK_OFF(MouseEventQueue, data);
    GEN_CHECK_OFF(MouseEventQueue, rptr);
    GEN_CHECK_OFF(MouseEventQueue, wptr);
    GEN_CHECK_OFF(MouseEventQueue, count);
    GEN_CHECK_SIZE(KBDState);
    GEN_CHECK_OFF(KBDState, mouse_command_queue);
    GEN_CHECK_OFF(KBDState, mouse_event_queue);
    GEN_CHECK_OFF(KBDState, write_cmd);
    GEN_CHECK_OFF(KBDState, status);
    GEN_CHECK_OFF(KBDState, mode);
    GEN_CHECK_OFF(KBDState, mouse_write_cmd);
    GEN_CHECK_OFF(KBDState, mouse_status);
    GEN_CHECK_OFF(KBDState, mouse_resolution);
    GEN_CHECK_OFF(KBDState, mouse_sample_rate);
    GEN_CHECK_OFF(KBDState, mouse_wrap);
    GEN_CHECK_OFF(KBDState, mouse_type);
    GEN_CHECK_OFF(KBDState, mouse_detect_state);
    GEN_CHECK_OFF(KBDState, mouse_dx);
    GEN_CHECK_OFF(KBDState, mouse_dy);
    GEN_CHECK_OFF(KBDState, mouse_dz);
    GEN_CHECK_OFF(KBDState, mouse_dw);
    GEN_CHECK_OFF(KBDState, mouse_buttons);
    GEN_CHECK_OFF(KBDState, pDevInsR3);
    GEN_CHECK_OFF(KBDState, pDevInsR0);
    GEN_CHECK_OFF(KBDState, pDevInsRC);
    GEN_CHECK_SIZE(KbdKeyQ);
    GEN_CHECK_OFF(KbdCmdQ, rpos);
    GEN_CHECK_OFF(KbdCmdQ, wpos);
    GEN_CHECK_OFF(KbdCmdQ, cUsed);
    GEN_CHECK_OFF(KbdCmdQ, cSize);
    GEN_CHECK_OFF(KbdCmdQ, abQueue);
    GEN_CHECK_SIZE(KbdCmdQ);
    GEN_CHECK_OFF(PS2K, fScanning);
    GEN_CHECK_OFF(PS2K, fNumLockOn);
    GEN_CHECK_OFF(PS2K, u8ScanSet);
    GEN_CHECK_OFF(PS2K, u8Typematic);
    GEN_CHECK_OFF(PS2K, enmTypematicState);
    GEN_CHECK_OFF(PS2K, keyQ);
    GEN_CHECK_OFF(PS2K, cmdQ);
    GEN_CHECK_OFF(PS2K, uTypematicDelay);
    GEN_CHECK_OFF(PS2K, pKbdDelayTimerRC);
    GEN_CHECK_OFF(PS2K, pKbdDelayTimerR3);
    GEN_CHECK_OFF(PS2K, pKbdDelayTimerR0);
    GEN_CHECK_OFF(PS2K, pKbdTypematicTimerRC);
    GEN_CHECK_OFF(PS2K, pKbdTypematicTimerR3);
    GEN_CHECK_OFF(PS2K, pKbdTypematicTimerR0);
    GEN_CHECK_OFF(PS2K, pCritSectR3);
    GEN_CHECK_OFF(PS2K, Keyboard.IBase);
    GEN_CHECK_OFF(PS2K, Keyboard.IPort);
    GEN_CHECK_OFF(PS2K, Keyboard.pDrvBase);
    GEN_CHECK_OFF(PS2K, Keyboard.pDrv);
    GEN_CHECK_OFF(KBDState, Mouse.IBase);
    GEN_CHECK_OFF(KBDState, Mouse.IPort);
    GEN_CHECK_OFF(KBDState, Mouse.pDrvBase);
    GEN_CHECK_OFF(KBDState, Mouse.pDrv);

    /* Network/DevPCNet.cpp */
    GEN_CHECK_SIZE(PCNetState);
    GEN_CHECK_OFF(PCNetState, PciDev);
#ifndef PCNET_NO_POLLING
    GEN_CHECK_OFF(PCNetState, pTimerPollR3);
    GEN_CHECK_OFF(PCNetState, pTimerPollR0);
    GEN_CHECK_OFF(PCNetState, pTimerPollRC);
#endif
    GEN_CHECK_OFF(PCNetState, pTimerSoftIntR3);
    GEN_CHECK_OFF(PCNetState, pTimerSoftIntR0);
    GEN_CHECK_OFF(PCNetState, pTimerSoftIntRC);
    GEN_CHECK_OFF(PCNetState, u32RAP);
    GEN_CHECK_OFF(PCNetState, iISR);
    GEN_CHECK_OFF(PCNetState, u32Lnkst);
    GEN_CHECK_OFF(PCNetState, GCRDRA);
    GEN_CHECK_OFF(PCNetState, GCTDRA);
    GEN_CHECK_OFF(PCNetState, aPROM);
    GEN_CHECK_OFF(PCNetState, aPROM[1]);
    GEN_CHECK_OFF(PCNetState, aCSR);
    GEN_CHECK_OFF(PCNetState, aCSR[1]);
    GEN_CHECK_OFF(PCNetState, aCSR[CSR_MAX_REG - 1]);
    GEN_CHECK_OFF(PCNetState, aBCR);
    GEN_CHECK_OFF(PCNetState, aBCR[1]);
    GEN_CHECK_OFF(PCNetState, aBCR[BCR_MAX_RAP - 1]);
    GEN_CHECK_OFF(PCNetState, aMII);
    GEN_CHECK_OFF(PCNetState, aMII[1]);
    GEN_CHECK_OFF(PCNetState, aMII[MII_MAX_REG - 1]);
    GEN_CHECK_OFF(PCNetState, u16CSR0LastSeenByGuest);
    GEN_CHECK_OFF(PCNetState, u64LastPoll);
    GEN_CHECK_OFF(PCNetState, abLoopBuf);
    GEN_CHECK_OFF(PCNetState, abRecvBuf);
    GEN_CHECK_OFF(PCNetState, iLog2DescSize);
    GEN_CHECK_OFF(PCNetState, GCUpperPhys);
    GEN_CHECK_OFF(PCNetState, pXmitQueueR3);
    GEN_CHECK_OFF(PCNetState, pXmitQueueR0);
    GEN_CHECK_OFF(PCNetState, pXmitQueueRC);
    GEN_CHECK_OFF(PCNetState, pCanRxQueueR3);
    GEN_CHECK_OFF(PCNetState, pCanRxQueueR0);
    GEN_CHECK_OFF(PCNetState, pCanRxQueueRC);
    GEN_CHECK_OFF(PCNetState, pTimerRestore);
    GEN_CHECK_OFF(PCNetState, pDevInsR3);
    GEN_CHECK_OFF(PCNetState, pDevInsR0);
    GEN_CHECK_OFF(PCNetState, pDevInsRC);
    GEN_CHECK_OFF(PCNetState, pDrvR3);
    GEN_CHECK_OFF(PCNetState, pDrvBase);
    GEN_CHECK_OFF(PCNetState, IBase);
    GEN_CHECK_OFF(PCNetState, INetworkDown);
    GEN_CHECK_OFF(PCNetState, INetworkConfig);
    GEN_CHECK_OFF(PCNetState, MMIOBase);
    GEN_CHECK_OFF(PCNetState, IOPortBase);
    GEN_CHECK_OFF(PCNetState, fLinkUp);
    GEN_CHECK_OFF(PCNetState, fLinkTempDown);
    GEN_CHECK_OFF(PCNetState, cLinkDownReported);
    GEN_CHECK_OFF(PCNetState, MacConfigured);
    GEN_CHECK_OFF(PCNetState, Led);
    GEN_CHECK_OFF(PCNetState, ILeds);
    GEN_CHECK_OFF(PCNetState, pLedsConnector);
    GEN_CHECK_OFF(PCNetState, CritSect);
#ifdef PCNET_NO_POLLING
    GEN_CHECK_OFF(PCNetState, TDRAPhysOld);
    GEN_CHECK_OFF(PCNetState, cbTDRAOld);
    GEN_CHECK_OFF(PCNetState, RDRAPhysOld);
    GEN_CHECK_OFF(PCNetState, cbRDRAOld);
    GEN_CHECK_OFF(PCNetState, pfnEMInterpretInstructionGC
    GEN_CHECK_OFF(PCNetState, pfnEMInterpretInstructionR0
#endif
    GEN_CHECK_OFF(PCNetState, pSharedMMIOR3);
    GEN_CHECK_OFF(PCNetState, pSharedMMIOR0);
    GEN_CHECK_OFF(PCNetState, pSharedMMIORC);
    GEN_CHECK_OFF(PCNetState, fPrivIfEnabled);
    GEN_CHECK_OFF(PCNetState, fGCEnabled);
    GEN_CHECK_OFF(PCNetState, fR0Enabled);
    GEN_CHECK_OFF(PCNetState, fAm79C973);
    GEN_CHECK_OFF(PCNetState, u32LinkSpeed);
    GEN_CHECK_OFF(PCNetState, StatReceiveBytes);
    GEN_CHECK_OFF(PCNetState, StatTransmitBytes);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(PCNetState, StatMMIOReadR3);
    GEN_CHECK_OFF(PCNetState, StatMMIOReadRZ);
    GEN_CHECK_OFF(PCNetState, StatMIIReads);
# ifdef PCNET_NO_POLLING
    GEN_CHECK_OFF(PCNetState, StatRCVRingWrite);
    GEN_CHECK_OFF(PCNetState, StatRingWriteOutsideRangeR3);
# endif
#endif

    /* PC/DevACPI.cpp */
    GEN_CHECK_SIZE(ACPIState);
    GEN_CHECK_OFF(ACPIState, dev);
    GEN_CHECK_OFF(ACPIState, pm1a_en);
    GEN_CHECK_OFF(ACPIState, pm1a_sts);
    GEN_CHECK_OFF(ACPIState, pm1a_ctl);
    GEN_CHECK_OFF(ACPIState, u64PmTimerInitial);
    GEN_CHECK_OFF(ACPIState, pPmTimerR3);
    GEN_CHECK_OFF(ACPIState, pPmTimerR0);
    GEN_CHECK_OFF(ACPIState, pPmTimerRC);
    GEN_CHECK_OFF(ACPIState, gpe0_en);
    GEN_CHECK_OFF(ACPIState, gpe0_sts);
    GEN_CHECK_OFF(ACPIState, uBatteryIndex);
    GEN_CHECK_OFF(ACPIState, au8BatteryInfo);
    GEN_CHECK_OFF(ACPIState, uSystemInfoIndex);
    GEN_CHECK_OFF(ACPIState, u64RamSize);
    GEN_CHECK_OFF(ACPIState, uSleepState);
    GEN_CHECK_OFF(ACPIState, au8RSDPPage);
    GEN_CHECK_OFF(ACPIState, u8IndexShift);
    GEN_CHECK_OFF(ACPIState, u8UseIOApic);
    GEN_CHECK_OFF(ACPIState, fUseFdc);
    GEN_CHECK_OFF(ACPIState, fUseHpet);
    GEN_CHECK_OFF(ACPIState, fUseSmc);
    GEN_CHECK_OFF(ACPIState, CpuSetAttached);
    GEN_CHECK_OFF(ACPIState, idCpuLockCheck);
    GEN_CHECK_OFF(ACPIState, CpuSetLocked);
    GEN_CHECK_OFF(ACPIState, u32CpuEventType);
    GEN_CHECK_OFF(ACPIState, u32CpuEvent);
    GEN_CHECK_OFF(ACPIState, fCpuHotPlug);
    GEN_CHECK_OFF(ACPIState, IBase);
    GEN_CHECK_OFF(ACPIState, IACPIPort);
    GEN_CHECK_OFF(ACPIState, pDevIns);
    GEN_CHECK_OFF(ACPIState, pDrvBase);
    GEN_CHECK_OFF(ACPIState, pDrv);

    /* PC/DevPIC.cpp */
    GEN_CHECK_SIZE(PICSTATE);
    GEN_CHECK_OFF(PICSTATE, last_irr);
    GEN_CHECK_OFF(PICSTATE, irr);
    GEN_CHECK_OFF(PICSTATE, imr);
    GEN_CHECK_OFF(PICSTATE, isr);
    GEN_CHECK_OFF(PICSTATE, priority_add);
    GEN_CHECK_OFF(PICSTATE, irq_base);
    GEN_CHECK_OFF(PICSTATE, read_reg_select);
    GEN_CHECK_OFF(PICSTATE, poll);
    GEN_CHECK_OFF(PICSTATE, special_mask);
    GEN_CHECK_OFF(PICSTATE, init_state);
    GEN_CHECK_OFF(PICSTATE, auto_eoi);
    GEN_CHECK_OFF(PICSTATE, rotate_on_auto_eoi);
    GEN_CHECK_OFF(PICSTATE, special_fully_nested_mode);
    GEN_CHECK_OFF(PICSTATE, init4);
    GEN_CHECK_OFF(PICSTATE, elcr);
    GEN_CHECK_OFF(PICSTATE, elcr_mask);
    GEN_CHECK_OFF(PICSTATE, pDevInsR3);
    GEN_CHECK_OFF(PICSTATE, pDevInsR0);
    GEN_CHECK_OFF(PICSTATE, pDevInsRC);
    GEN_CHECK_OFF(PICSTATE, idxPic);
    GEN_CHECK_OFF(PICSTATE, auTags);

    GEN_CHECK_SIZE(DEVPIC);
    GEN_CHECK_OFF(DEVPIC, aPics);
    GEN_CHECK_OFF(DEVPIC, aPics[1]);
    GEN_CHECK_OFF(DEVPIC, pDevInsR3);
    GEN_CHECK_OFF(DEVPIC, pDevInsR0);
    GEN_CHECK_OFF(DEVPIC, pDevInsRC);
    GEN_CHECK_OFF(DEVPIC, pPicHlpR3);
    GEN_CHECK_OFF(DEVPIC, pPicHlpR0);
    GEN_CHECK_OFF(DEVPIC, pPicHlpRC);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(DEVPIC, StatSetIrqGC);
    GEN_CHECK_OFF(DEVPIC, StatClearedActiveSlaveIRQ);
#endif

    /* PC/DevPit-i8254.cpp */
    GEN_CHECK_SIZE(PITCHANNEL);
    GEN_CHECK_OFF(PITCHANNEL, pPitR3);
    GEN_CHECK_OFF(PITCHANNEL, pTimerR3);
    GEN_CHECK_OFF(PITCHANNEL, pPitR0);
    GEN_CHECK_OFF(PITCHANNEL, pTimerR0);
    GEN_CHECK_OFF(PITCHANNEL, pPitRC);
    GEN_CHECK_OFF(PITCHANNEL, pTimerRC);
    GEN_CHECK_OFF(PITCHANNEL, u64ReloadTS);
    GEN_CHECK_OFF(PITCHANNEL, u64NextTS);
    GEN_CHECK_OFF(PITCHANNEL, count_load_time);
    GEN_CHECK_OFF(PITCHANNEL, next_transition_time);
    GEN_CHECK_OFF(PITCHANNEL, irq);
    GEN_CHECK_OFF(PITCHANNEL, cRelLogEntries);
    GEN_CHECK_OFF(PITCHANNEL, count);
    GEN_CHECK_OFF(PITCHANNEL, latched_count);
    GEN_CHECK_OFF(PITCHANNEL, count_latched);
    GEN_CHECK_OFF(PITCHANNEL, status_latched);
    GEN_CHECK_OFF(PITCHANNEL, status);
    GEN_CHECK_OFF(PITCHANNEL, read_state);
    GEN_CHECK_OFF(PITCHANNEL, write_state);
    GEN_CHECK_OFF(PITCHANNEL, write_latch);
    GEN_CHECK_OFF(PITCHANNEL, rw_mode);
    GEN_CHECK_OFF(PITCHANNEL, mode);
    GEN_CHECK_OFF(PITCHANNEL, bcd);
    GEN_CHECK_OFF(PITCHANNEL, gate);
    GEN_CHECK_SIZE(PITSTATE);
    GEN_CHECK_OFF(PITSTATE, channels);
    GEN_CHECK_OFF(PITSTATE, channels[1]);
    GEN_CHECK_OFF(PITSTATE, speaker_data_on);
//    GEN_CHECK_OFF(PITSTATE, dummy_refresh_clock);
    GEN_CHECK_OFF(PITSTATE, IOPortBaseCfg);
    GEN_CHECK_OFF(PITSTATE, fSpeakerCfg);
    GEN_CHECK_OFF(PITSTATE, pDevIns);
    GEN_CHECK_OFF(PITSTATE, StatPITIrq);
    GEN_CHECK_OFF(PITSTATE, StatPITHandler);

    /* PC/DevRTC.cpp */
    GEN_CHECK_SIZE(RTCSTATE);
    GEN_CHECK_OFF(RTCSTATE, cmos_data);
    GEN_CHECK_OFF(RTCSTATE, cmos_data[1]);
    GEN_CHECK_OFF(RTCSTATE, cmos_index);
    GEN_CHECK_OFF(RTCSTATE, current_tm);
    GEN_CHECK_OFF(RTCSTATE, current_tm.tm_sec);
    GEN_CHECK_OFF(RTCSTATE, current_tm.tm_min);
    GEN_CHECK_OFF(RTCSTATE, current_tm.tm_hour);
    GEN_CHECK_OFF(RTCSTATE, current_tm.tm_mday);
    GEN_CHECK_OFF(RTCSTATE, current_tm.tm_mon);
    GEN_CHECK_OFF(RTCSTATE, current_tm.tm_year);
    GEN_CHECK_OFF(RTCSTATE, current_tm.tm_wday);
    GEN_CHECK_OFF(RTCSTATE, current_tm.tm_yday);
    GEN_CHECK_OFF(RTCSTATE, irq);
    GEN_CHECK_OFF(RTCSTATE, fUTC);
    GEN_CHECK_OFF(RTCSTATE, IOPortBase);
    GEN_CHECK_OFF(RTCSTATE, pPeriodicTimerR0);
    GEN_CHECK_OFF(RTCSTATE, pPeriodicTimerR3);
    GEN_CHECK_OFF(RTCSTATE, pPeriodicTimerRC);
    GEN_CHECK_OFF(RTCSTATE, next_periodic_time);
    GEN_CHECK_OFF(RTCSTATE, next_second_time);
    GEN_CHECK_OFF(RTCSTATE, pSecondTimerR0);
    GEN_CHECK_OFF(RTCSTATE, pSecondTimerR3);
    GEN_CHECK_OFF(RTCSTATE, pSecondTimerRC);
    GEN_CHECK_OFF(RTCSTATE, pSecondTimer2R0);
    GEN_CHECK_OFF(RTCSTATE, pSecondTimer2R3);
    GEN_CHECK_OFF(RTCSTATE, pSecondTimer2RC);
    GEN_CHECK_OFF(RTCSTATE, pDevInsR0);
    GEN_CHECK_OFF(RTCSTATE, pDevInsR3);
    GEN_CHECK_OFF(RTCSTATE, pDevInsRC);
    GEN_CHECK_OFF(RTCSTATE, RtcReg);
    GEN_CHECK_OFF(RTCSTATE, pRtcHlpR3);
    GEN_CHECK_OFF(RTCSTATE, cRelLogEntries);
    GEN_CHECK_OFF(RTCSTATE, CurLogPeriod);
    GEN_CHECK_OFF(RTCSTATE, CurHintPeriod);

    /* PC/DevAPIC.cpp */
    GEN_CHECK_SIZE(APICState);
    GEN_CHECK_OFF(APICState, apicbase);
    GEN_CHECK_OFF(APICState, id);
    GEN_CHECK_OFF(APICState, arb_id);
    GEN_CHECK_OFF(APICState, tpr);
    GEN_CHECK_OFF(APICState, spurious_vec);
    GEN_CHECK_OFF(APICState, log_dest);
    GEN_CHECK_OFF(APICState, dest_mode);
    GEN_CHECK_OFF(APICState, isr);
    GEN_CHECK_OFF(APICState, isr.au32Bitmap[1]);
    GEN_CHECK_OFF(APICState, tmr);
    GEN_CHECK_OFF(APICState, tmr.au32Bitmap[1]);
    GEN_CHECK_OFF(APICState, irr);
    GEN_CHECK_OFF(APICState, irr.au32Bitmap[1]);
    GEN_CHECK_OFF(APICState, lvt);
    GEN_CHECK_OFF(APICState, lvt[1]);
    GEN_CHECK_OFF(APICState, lvt[APIC_LVT_NB - 1]);
    GEN_CHECK_OFF(APICState, esr);
    GEN_CHECK_OFF(APICState, icr);
    GEN_CHECK_OFF(APICState, icr[1]);
    GEN_CHECK_OFF(APICState, divide_conf);
    GEN_CHECK_OFF(APICState, count_shift);
    GEN_CHECK_OFF(APICState, initial_count);
    GEN_CHECK_OFF(APICState, initial_count_load_time);
    GEN_CHECK_OFF(APICState, next_time);
    GEN_CHECK_OFF(APICState, pTimerR3);
    GEN_CHECK_OFF(APICState, pTimerR0);
    GEN_CHECK_OFF(APICState, pTimerRC);
    GEN_CHECK_OFF(APICState, fTimerArmed);
    GEN_CHECK_OFF(APICState, uHintedInitialCount);
    GEN_CHECK_OFF(APICState, uHintedCountShift);
    GEN_CHECK_OFF(APICState, pszDesc);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(APICState, StatTimerSetInitialCount);
    GEN_CHECK_OFF(APICState, StatTimerSetLvtNoRelevantChange);
#endif

    GEN_CHECK_SIZE(APICDeviceInfo);
    GEN_CHECK_OFF(APICDeviceInfo, pDevInsR3);
    GEN_CHECK_OFF(APICDeviceInfo, pApicHlpR3);
    GEN_CHECK_OFF(APICDeviceInfo, paLapicsR3);
    GEN_CHECK_OFF(APICDeviceInfo, pCritSectR3);
    GEN_CHECK_OFF(APICDeviceInfo, pDevInsR0);
    GEN_CHECK_OFF(APICDeviceInfo, pApicHlpR0);
    GEN_CHECK_OFF(APICDeviceInfo, paLapicsR0);
    GEN_CHECK_OFF(APICDeviceInfo, pCritSectR0);
    GEN_CHECK_OFF(APICDeviceInfo, pDevInsRC);
    GEN_CHECK_OFF(APICDeviceInfo, pApicHlpRC);
    GEN_CHECK_OFF(APICDeviceInfo, paLapicsRC);
    GEN_CHECK_OFF(APICDeviceInfo, pCritSectRC);
    GEN_CHECK_OFF(APICDeviceInfo, enmVersion);
    GEN_CHECK_OFF(APICDeviceInfo, cTPRPatchAttempts);
    GEN_CHECK_OFF(APICDeviceInfo, cCpus);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(APICDeviceInfo, StatMMIOReadGC);
    GEN_CHECK_OFF(APICDeviceInfo, StatMMIOWriteHC);
#endif

    /* PC/DevIoApic.cpp */
    GEN_CHECK_SIZE(IOAPIC);
    GEN_CHECK_OFF(IOAPIC, id);
    GEN_CHECK_OFF(IOAPIC, ioregsel);
    GEN_CHECK_OFF(IOAPIC, irr);
    GEN_CHECK_OFF(IOAPIC, ioredtbl);
    GEN_CHECK_OFF(IOAPIC, ioredtbl[1]);
    GEN_CHECK_OFF(IOAPIC, ioredtbl[IOAPIC_NUM_PINS - 1]);
    GEN_CHECK_OFF(IOAPIC, pDevInsR3);
    GEN_CHECK_OFF(IOAPIC, pIoApicHlpR3);
    GEN_CHECK_OFF(IOAPIC, pDevInsR0);
    GEN_CHECK_OFF(IOAPIC, pIoApicHlpR0);
    GEN_CHECK_OFF(IOAPIC, pDevInsRC);
    GEN_CHECK_OFF(IOAPIC, pIoApicHlpRC);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(IOAPIC, StatMMIOReadGC);
    GEN_CHECK_OFF(IOAPIC, StatSetIrqHC);
#endif

    /* Storage/DevATA.cpp */
    GEN_CHECK_SIZE(BMDMAState);
    GEN_CHECK_OFF(BMDMAState, u8Cmd);
    GEN_CHECK_OFF(BMDMAState, u8Status);
    GEN_CHECK_OFF(BMDMAState, pvAddr);
    GEN_CHECK_SIZE(BMDMADesc);
    GEN_CHECK_OFF(BMDMADesc, pBuffer);
    GEN_CHECK_OFF(BMDMADesc, cbBuffer);
    GEN_CHECK_SIZE(ATADevState);
    GEN_CHECK_OFF(ATADevState, fLBA48);
    GEN_CHECK_OFF(ATADevState, fATAPI);
    GEN_CHECK_OFF(ATADevState, fIrqPending);
    GEN_CHECK_OFF(ATADevState, cMultSectors);
    GEN_CHECK_OFF(ATADevState, PCHSGeometry.cCylinders);
    GEN_CHECK_OFF(ATADevState, PCHSGeometry.cHeads);
    GEN_CHECK_OFF(ATADevState, PCHSGeometry.cSectors);
    GEN_CHECK_OFF(ATADevState, cSectorsPerIRQ);
    GEN_CHECK_OFF(ATADevState, cTotalSectors);
    GEN_CHECK_OFF(ATADevState, uATARegFeature);
    GEN_CHECK_OFF(ATADevState, uATARegFeatureHOB);
    GEN_CHECK_OFF(ATADevState, uATARegError);
    GEN_CHECK_OFF(ATADevState, uATARegNSector);
    GEN_CHECK_OFF(ATADevState, uATARegNSectorHOB);
    GEN_CHECK_OFF(ATADevState, uATARegSector);
    GEN_CHECK_OFF(ATADevState, uATARegSectorHOB);
    GEN_CHECK_OFF(ATADevState, uATARegLCyl);
    GEN_CHECK_OFF(ATADevState, uATARegLCylHOB);
    GEN_CHECK_OFF(ATADevState, uATARegHCyl);
    GEN_CHECK_OFF(ATADevState, uATARegHCylHOB);
    GEN_CHECK_OFF(ATADevState, uATARegSelect);
    GEN_CHECK_OFF(ATADevState, uATARegStatus);
    GEN_CHECK_OFF(ATADevState, uATARegCommand);
    GEN_CHECK_OFF(ATADevState, uATARegDevCtl);
    GEN_CHECK_OFF(ATADevState, uATATransferMode);
    GEN_CHECK_OFF(ATADevState, uTxDir);
    GEN_CHECK_OFF(ATADevState, iBeginTransfer);
    GEN_CHECK_OFF(ATADevState, iSourceSink);
    GEN_CHECK_OFF(ATADevState, fDMA);
    GEN_CHECK_OFF(ATADevState, fATAPITransfer);
    GEN_CHECK_OFF(ATADevState, cbTotalTransfer);
    GEN_CHECK_OFF(ATADevState, cbElementaryTransfer);
    GEN_CHECK_OFF(ATADevState, iIOBufferCur);
    GEN_CHECK_OFF(ATADevState, iIOBufferEnd);
    GEN_CHECK_OFF(ATADevState, iIOBufferPIODataStart);
    GEN_CHECK_OFF(ATADevState, iIOBufferPIODataEnd);
    GEN_CHECK_OFF(ATADevState, iATAPILBA);
    GEN_CHECK_OFF(ATADevState, cbATAPISector);
    GEN_CHECK_OFF(ATADevState, aATAPICmd);
    GEN_CHECK_OFF(ATADevState, aATAPICmd[ATAPI_PACKET_SIZE - 1]);
    GEN_CHECK_OFF(ATADevState, abATAPISense);
    GEN_CHECK_OFF(ATADevState, abATAPISense[ATAPI_SENSE_SIZE - 1]);
    GEN_CHECK_OFF(ATADevState, cNotifiedMediaChange);
    GEN_CHECK_OFF(ATADevState, MediaEventStatus);
    GEN_CHECK_OFF(ATADevState, MediaTrackType);
    GEN_CHECK_OFF(ATADevState, Led);
    GEN_CHECK_OFF(ATADevState, cbIOBuffer);
    GEN_CHECK_OFF(ATADevState, pbIOBufferR3);
    GEN_CHECK_OFF(ATADevState, pbIOBufferR0);
    GEN_CHECK_OFF(ATADevState, pbIOBufferRC);
    GEN_CHECK_OFF(ATADevState, StatATADMA);
    GEN_CHECK_OFF(ATADevState, StatATAPIO);
    GEN_CHECK_OFF(ATADevState, StatATAPIDMA);
    GEN_CHECK_OFF(ATADevState, StatATAPIPIO);
    GEN_CHECK_OFF(ATADevState, StatReads);
    GEN_CHECK_OFF(ATADevState, StatBytesRead);
    GEN_CHECK_OFF(ATADevState, StatWrites);
    GEN_CHECK_OFF(ATADevState, StatBytesWritten);
    GEN_CHECK_OFF(ATADevState, StatFlushes);
    GEN_CHECK_OFF(ATADevState, fNonRotational);
    GEN_CHECK_OFF(ATADevState, fATAPIPassthrough);
    GEN_CHECK_OFF(ATADevState, fOverwriteInquiry);
    GEN_CHECK_OFF(ATADevState, cErrors);
    GEN_CHECK_OFF(ATADevState, pDrvBase);
    GEN_CHECK_OFF(ATADevState, pDrvBlock);
    GEN_CHECK_OFF(ATADevState, pDrvBlockBios);
    GEN_CHECK_OFF(ATADevState, pDrvMount);
    GEN_CHECK_OFF(ATADevState, IBase);
    GEN_CHECK_OFF(ATADevState, IPort);
    GEN_CHECK_OFF(ATADevState, IMountNotify);
    GEN_CHECK_OFF(ATADevState, iLUN);
    GEN_CHECK_OFF(ATADevState, pDevInsR3);
    GEN_CHECK_OFF(ATADevState, pDevInsR0);
    GEN_CHECK_OFF(ATADevState, pDevInsRC);
    GEN_CHECK_OFF(ATADevState, pControllerR3);
    GEN_CHECK_OFF(ATADevState, pControllerR0);
    GEN_CHECK_OFF(ATADevState, pControllerRC);
    GEN_CHECK_OFF(ATADevState, szSerialNumber);
    GEN_CHECK_OFF(ATADevState, szSerialNumber[ATA_SERIAL_NUMBER_LENGTH]);
    GEN_CHECK_OFF(ATADevState, szFirmwareRevision);
    GEN_CHECK_OFF(ATADevState, szFirmwareRevision[ATA_FIRMWARE_REVISION_LENGTH]);
    GEN_CHECK_OFF(ATADevState, szModelNumber);
    GEN_CHECK_OFF(ATADevState, szModelNumber[ATA_MODEL_NUMBER_LENGTH]);
    GEN_CHECK_OFF(ATADevState, szInquiryVendorId);
    GEN_CHECK_OFF(ATADevState, szInquiryVendorId[ATAPI_INQUIRY_VENDOR_ID_LENGTH]);
    GEN_CHECK_OFF(ATADevState, szInquiryProductId);
    GEN_CHECK_OFF(ATADevState, szInquiryProductId[ATAPI_INQUIRY_PRODUCT_ID_LENGTH]);
    GEN_CHECK_OFF(ATADevState, szInquiryRevision);
    GEN_CHECK_OFF(ATADevState, szInquiryRevision[ATAPI_INQUIRY_REVISION_LENGTH]);
    GEN_CHECK_OFF(ATADevState, pTrackList);
    GEN_CHECK_SIZE(ATATransferRequest);
    GEN_CHECK_OFF(ATATransferRequest, iIf);
    GEN_CHECK_OFF(ATATransferRequest, iBeginTransfer);
    GEN_CHECK_OFF(ATATransferRequest, iSourceSink);
    GEN_CHECK_OFF(ATATransferRequest, cbTotalTransfer);
    GEN_CHECK_OFF(ATATransferRequest, uTxDir);
    GEN_CHECK_SIZE(ATAAbortRequest);
    GEN_CHECK_OFF(ATAAbortRequest, iIf);
    GEN_CHECK_OFF(ATAAbortRequest, fResetDrive);
    GEN_CHECK_SIZE(ATARequest);
    GEN_CHECK_OFF(ATARequest, ReqType);
    GEN_CHECK_OFF(ATARequest, u);
    GEN_CHECK_OFF(ATARequest, u.t);
    GEN_CHECK_OFF(ATARequest, u.a);
    GEN_CHECK_SIZE(ATACONTROLLER);
    GEN_CHECK_OFF(ATACONTROLLER, IOPortBase1);
    GEN_CHECK_OFF(ATACONTROLLER, IOPortBase2);
    GEN_CHECK_OFF(ATACONTROLLER, irq);
    GEN_CHECK_OFF(ATACONTROLLER, lock);
    GEN_CHECK_OFF(ATACONTROLLER, iSelectedIf);
    GEN_CHECK_OFF(ATACONTROLLER, iAIOIf);
    GEN_CHECK_OFF(ATACONTROLLER, uAsyncIOState);
    GEN_CHECK_OFF(ATACONTROLLER, fChainedTransfer);
    GEN_CHECK_OFF(ATACONTROLLER, fReset);
    GEN_CHECK_OFF(ATACONTROLLER, fRedo);
    GEN_CHECK_OFF(ATACONTROLLER, fRedoIdle);
    GEN_CHECK_OFF(ATACONTROLLER, fRedoDMALastDesc);
    GEN_CHECK_OFF(ATACONTROLLER, BmDma);
    GEN_CHECK_OFF(ATACONTROLLER, pFirstDMADesc);
    GEN_CHECK_OFF(ATACONTROLLER, pLastDMADesc);
    GEN_CHECK_OFF(ATACONTROLLER, pRedoDMABuffer);
    GEN_CHECK_OFF(ATACONTROLLER, cbRedoDMABuffer);
    GEN_CHECK_OFF(ATACONTROLLER, aIfs);
    GEN_CHECK_OFF(ATACONTROLLER, aIfs[1]);
    GEN_CHECK_OFF(ATACONTROLLER, pDevInsR3);
    GEN_CHECK_OFF(ATACONTROLLER, pDevInsR0);
    GEN_CHECK_OFF(ATACONTROLLER, pDevInsRC);
    GEN_CHECK_OFF(ATACONTROLLER, fShutdown);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIOThread);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIOSem);
    GEN_CHECK_OFF(ATACONTROLLER, aAsyncIORequests[4]);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIOReqHead);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIOReqTail);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIORequestMutex);
    GEN_CHECK_OFF(ATACONTROLLER, SuspendIOSem);
    GEN_CHECK_OFF(ATACONTROLLER, fSignalIdle);
    GEN_CHECK_OFF(ATACONTROLLER, DelayIRQMillies);
    GEN_CHECK_OFF(ATACONTROLLER, u64ResetTime);
    GEN_CHECK_OFF(ATACONTROLLER, StatAsyncOps);
    GEN_CHECK_OFF(ATACONTROLLER, StatAsyncMinWait);
    GEN_CHECK_OFF(ATACONTROLLER, StatAsyncMaxWait);
    GEN_CHECK_OFF(ATACONTROLLER, StatAsyncTimeUS);
    GEN_CHECK_OFF(ATACONTROLLER, StatAsyncTime);
    GEN_CHECK_OFF(ATACONTROLLER, StatLockWait);
    GEN_CHECK_SIZE(PCIATAState);
    GEN_CHECK_OFF(PCIATAState, dev);
    GEN_CHECK_OFF(PCIATAState, aCts);
    GEN_CHECK_OFF(PCIATAState, aCts[1]);
    GEN_CHECK_OFF(PCIATAState, pDevIns);
    GEN_CHECK_OFF(PCIATAState, IBase);
    GEN_CHECK_OFF(PCIATAState, ILeds);
    GEN_CHECK_OFF(PCIATAState, pLedsConnector);
    GEN_CHECK_OFF(PCIATAState, fGCEnabled);
    GEN_CHECK_OFF(PCIATAState, fR0Enabled);

#ifdef VBOX_WITH_USB
    /* USB/DevOHCI.cpp */
    GEN_CHECK_SIZE(OHCIHUBPORT);
    GEN_CHECK_OFF(OHCIHUBPORT, fReg);
    GEN_CHECK_OFF(OHCIHUBPORT, pDev);

    GEN_CHECK_SIZE(OHCIROOTHUB);
    GEN_CHECK_OFF(OHCIROOTHUB, pIBase);
    GEN_CHECK_OFF(OHCIROOTHUB, pIRhConn);
    GEN_CHECK_OFF(OHCIROOTHUB, pIDev);
    GEN_CHECK_OFF(OHCIROOTHUB, IBase);
    GEN_CHECK_OFF(OHCIROOTHUB, IRhPort);
    GEN_CHECK_OFF(OHCIROOTHUB, status);
    GEN_CHECK_OFF(OHCIROOTHUB, desc_a);
    GEN_CHECK_OFF(OHCIROOTHUB, desc_b);
    GEN_CHECK_OFF(OHCIROOTHUB, aPorts);
    GEN_CHECK_OFF(OHCIROOTHUB, aPorts[1]);
    GEN_CHECK_OFF(OHCIROOTHUB, aPorts[OHCI_NDP - 1]);
    GEN_CHECK_OFF(OHCIROOTHUB, pOhci);

    GEN_CHECK_SIZE(OHCI);
    GEN_CHECK_OFF(OHCI, PciDev);
    GEN_CHECK_OFF(OHCI, MMIOBase);
    GEN_CHECK_OFF(OHCI, pEndOfFrameTimerR3);
    GEN_CHECK_OFF(OHCI, pEndOfFrameTimerR0);
    GEN_CHECK_OFF(OHCI, pEndOfFrameTimerRC);
    GEN_CHECK_OFF(OHCI, pDevInsR3);
    GEN_CHECK_OFF(OHCI, pDevInsR0);
    GEN_CHECK_OFF(OHCI, pDevInsRC);
    GEN_CHECK_OFF(OHCI, SofTime);
    //GEN_CHECK_OFF(OHCI, dqic:3);
    //GEN_CHECK_OFF(OHCI, fno:1);
    GEN_CHECK_OFF(OHCI, RootHub);
    GEN_CHECK_OFF(OHCI, ctl);
    GEN_CHECK_OFF(OHCI, status);
    GEN_CHECK_OFF(OHCI, intr_status);
    GEN_CHECK_OFF(OHCI, intr);
    GEN_CHECK_OFF(OHCI, hcca);
    GEN_CHECK_OFF(OHCI, per_cur);
    GEN_CHECK_OFF(OHCI, ctrl_cur);
    GEN_CHECK_OFF(OHCI, ctrl_head);
    GEN_CHECK_OFF(OHCI, bulk_cur);
    GEN_CHECK_OFF(OHCI, bulk_head);
    GEN_CHECK_OFF(OHCI, done);
    //GEN_CHECK_OFF(OHCI, fsmps:15);
    //GEN_CHECK_OFF(OHCI, fit:1);
    //GEN_CHECK_OFF(OHCI, fi:14);
    //GEN_CHECK_OFF(OHCI, frt:1);
    GEN_CHECK_OFF(OHCI, HcFmNumber);
    GEN_CHECK_OFF(OHCI, pstart);
    GEN_CHECK_OFF(OHCI, cTicksPerFrame);
    GEN_CHECK_OFF(OHCI, cTicksPerUsbTick);
    GEN_CHECK_OFF(OHCI, cInFlight);
    GEN_CHECK_OFF(OHCI, aInFlight);
    GEN_CHECK_OFF(OHCI, aInFlight[0].GCPhysTD);
    GEN_CHECK_OFF(OHCI, aInFlight[0].pUrb);
    GEN_CHECK_OFF(OHCI, aInFlight[1]);
    GEN_CHECK_OFF(OHCI, cInDoneQueue);
    GEN_CHECK_OFF(OHCI, aInDoneQueue);
    GEN_CHECK_OFF(OHCI, aInDoneQueue[0].GCPhysTD);
    GEN_CHECK_OFF(OHCI, aInDoneQueue[1]);
    GEN_CHECK_OFF(OHCI, u32FmDoneQueueTail);
    GEN_CHECK_OFF(OHCI, pLoad);
# ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(OHCI, StatCanceledIsocUrbs);
    GEN_CHECK_OFF(OHCI, StatCanceledGenUrbs);
    GEN_CHECK_OFF(OHCI, StatDroppedUrbs);
    GEN_CHECK_OFF(OHCI, StatTimer);
# endif
# ifdef VBOX_WITH_EHCI_IMPL
    /* USB/DevEHCI.cpp */
    GEN_CHECK_SIZE(EHCIHUBPORT);
    GEN_CHECK_OFF(EHCIHUBPORT, fReg);
    GEN_CHECK_OFF(EHCIHUBPORT, pDev);

    GEN_CHECK_SIZE(EHCIROOTHUB);
    GEN_CHECK_OFF(EHCIROOTHUB, pIBase);
    GEN_CHECK_OFF(EHCIROOTHUB, pIRhConn);
    GEN_CHECK_OFF(EHCIROOTHUB, pIDev);
    GEN_CHECK_OFF(EHCIROOTHUB, IBase);
    GEN_CHECK_OFF(EHCIROOTHUB, IRhPort);
    GEN_CHECK_OFF(EHCIROOTHUB, Led);
    GEN_CHECK_OFF(EHCIROOTHUB, ILeds);
    GEN_CHECK_OFF(EHCIROOTHUB, pLedsConnector);
    GEN_CHECK_OFF(EHCIROOTHUB, status);
    GEN_CHECK_OFF(EHCIROOTHUB, desc_a);
    GEN_CHECK_OFF(EHCIROOTHUB, desc_b);
    GEN_CHECK_OFF(EHCIROOTHUB, aPorts);
    GEN_CHECK_OFF(EHCIROOTHUB, aPorts[1]);
    GEN_CHECK_OFF(EHCIROOTHUB, aPorts[EHCI_NDP - 1]);
    GEN_CHECK_OFF(EHCIROOTHUB, pEhci);

    GEN_CHECK_SIZE(EHCI);
    GEN_CHECK_OFF(EHCI, PciDev);
    GEN_CHECK_OFF(EHCI, MMIOBase);
    GEN_CHECK_OFF(EHCI, pEndOfFrameTimerR3);
    GEN_CHECK_OFF(EHCI, pEndOfFrameTimerR0);
    GEN_CHECK_OFF(EHCI, pEndOfFrameTimerRC);
    GEN_CHECK_OFF(EHCI, pDevInsR3);
    GEN_CHECK_OFF(EHCI, pDevInsR0);
    GEN_CHECK_OFF(EHCI, pDevInsRC);
    GEN_CHECK_OFF(EHCI, MMIOBase);
    GEN_CHECK_OFF(EHCI, SofTime);
    GEN_CHECK_OFF(EHCI, RootHub);
    GEN_CHECK_OFF(EHCI, cap_length);
    GEN_CHECK_OFF(EHCI, hci_version);
    GEN_CHECK_OFF(EHCI, hcs_params);
    GEN_CHECK_OFF(EHCI, hcc_params);
    GEN_CHECK_OFF(EHCI, cmd);
    GEN_CHECK_OFF(EHCI, intr_status);
    GEN_CHECK_OFF(EHCI, intr);
    GEN_CHECK_OFF(EHCI, frame_idx);
    GEN_CHECK_OFF(EHCI, ds_segment);
    GEN_CHECK_OFF(EHCI, periodic_list_base);
    GEN_CHECK_OFF(EHCI, async_list_base);
    GEN_CHECK_OFF(EHCI, config);
    GEN_CHECK_OFF(EHCI, uIrqInterval);
    GEN_CHECK_OFF(EHCI, HcFmNumber);
    GEN_CHECK_OFF(EHCI, uFramesPerTimerCall);
    GEN_CHECK_OFF(EHCI, cTicksPerFrame);
    GEN_CHECK_OFF(EHCI, cTicksPerUsbTick);
    GEN_CHECK_OFF(EHCI, cInFlight);
    GEN_CHECK_OFF(EHCI, aInFlight);
    GEN_CHECK_OFF(EHCI, aInFlight[0].GCPhysTD);
    GEN_CHECK_OFF(EHCI, aInFlight[0].pUrb);
    GEN_CHECK_OFF(EHCI, aInFlight[1]);
    GEN_CHECK_OFF(EHCI, aInFlight[256]);
    GEN_CHECK_OFF(EHCI, pLoad);
    GEN_CHECK_OFF(EHCI, fAsyncTraversalTimerActive);
#  ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(EHCI, StatCanceledIsocUrbs);
    GEN_CHECK_OFF(EHCI, StatCanceledGenUrbs);
    GEN_CHECK_OFF(EHCI, StatDroppedUrbs);
    GEN_CHECK_OFF(EHCI, StatTimer);
#  endif
    GEN_CHECK_OFF(EHCI, SavedStateEnd);
    GEN_CHECK_OFF(EHCI, u64TimerHz);
    GEN_CHECK_OFF(EHCI, cIdleCycles);
    GEN_CHECK_OFF(EHCI, uFrameRate);
    GEN_CHECK_OFF(EHCI, fIdle);
    GEN_CHECK_OFF(EHCI, pEOFTimerSyncR3);
    GEN_CHECK_OFF(EHCI, pEOFTimerSyncR0);
    GEN_CHECK_OFF(EHCI, pEOFTimerSyncRC);
    GEN_CHECK_OFF(EHCI, pEOFTimerNoSyncRC);
    GEN_CHECK_OFF(EHCI, pEOFTimerNoSyncR3);
    GEN_CHECK_OFF(EHCI, pEOFTimerNoSyncR0);
# endif /* VBOX_WITH_EHCI_IMPL */
#endif /* VBOX_WITH_USB */

    /* VMMDev/VBoxDev.cpp */

    /* Parallel/DevParallel.cpp */
    GEN_CHECK_SIZE(PARALLELPORT);
    GEN_CHECK_OFF(PARALLELPORT, pDevInsR3);
    GEN_CHECK_OFF(PARALLELPORT, pDevInsR0);
    GEN_CHECK_OFF(PARALLELPORT, pDevInsRC);
    GEN_CHECK_OFF(PARALLELPORT, IBase);
    GEN_CHECK_OFF(PARALLELPORT, IHostParallelPort);
    GEN_CHECK_OFF(PARALLELPORT, pDrvHostParallelConnector);
    GEN_CHECK_OFF(PARALLELPORT, fGCEnabled);
    GEN_CHECK_OFF(PARALLELPORT, fR0Enabled);
    GEN_CHECK_OFF(PARALLELPORT, fEppTimeout);
    GEN_CHECK_OFF(PARALLELPORT, IOBase);
    GEN_CHECK_OFF(PARALLELPORT, iIrq);
    GEN_CHECK_OFF(PARALLELPORT, regData);
    GEN_CHECK_OFF(PARALLELPORT, regStatus);
    GEN_CHECK_OFF(PARALLELPORT, regControl);
    GEN_CHECK_OFF(PARALLELPORT, regEppAddr);
    GEN_CHECK_OFF(PARALLELPORT, regEppData);
#if 0
    GEN_CHECK_OFF(PARALLELPORT, reg_ecp_ecr);
    GEN_CHECK_OFF(PARALLELPORT, reg_ecp_base_plus_400h);
    GEN_CHECK_OFF(PARALLELPORT, reg_ecp_config_b);
    GEN_CHECK_OFF(PARALLELPORT, ecp_fifo);
    GEN_CHECK_OFF(PARALLELPORT, ecp_fifo[1]);
    GEN_CHECK_OFF(PARALLELPORT, act_fifo_pos_write);
    GEN_CHECK_OFF(PARALLELPORT, act_fifo_pos_read);
#endif

    /* Serial/DevSerial.cpp */
    GEN_CHECK_SIZE(SerialState);
    GEN_CHECK_OFF(SerialState, CritSect);
    GEN_CHECK_OFF(SerialState, pDevInsR3);
    GEN_CHECK_OFF(SerialState, pDevInsR0);
    GEN_CHECK_OFF(SerialState, pDevInsRC);
    GEN_CHECK_OFF(SerialState, IBase);
    GEN_CHECK_OFF(SerialState, ICharPort);
    GEN_CHECK_OFF(SerialState, pDrvBase);
    GEN_CHECK_OFF(SerialState, pDrvChar);
    GEN_CHECK_OFF(SerialState, ReceiveSem);
    GEN_CHECK_OFF(SerialState, base);
    GEN_CHECK_OFF(SerialState, divider);
    GEN_CHECK_OFF(SerialState, recv_fifo);
    GEN_CHECK_OFF(SerialState, xmit_fifo);
    GEN_CHECK_OFF(SerialState, rbr);
    GEN_CHECK_OFF(SerialState, thr);
    GEN_CHECK_OFF(SerialState, tsr);
    GEN_CHECK_OFF(SerialState, ier);
    GEN_CHECK_OFF(SerialState, iir);
    GEN_CHECK_OFF(SerialState, lcr);
    GEN_CHECK_OFF(SerialState, mcr);
    GEN_CHECK_OFF(SerialState, lsr);
    GEN_CHECK_OFF(SerialState, msr);
    GEN_CHECK_OFF(SerialState, scr);
    GEN_CHECK_OFF(SerialState, fcr);
    GEN_CHECK_OFF(SerialState, fcr_vmstate);
    GEN_CHECK_OFF(SerialState, thr_ipending);
    GEN_CHECK_OFF(SerialState, timeout_ipending);
    GEN_CHECK_OFF(SerialState, irq);
    GEN_CHECK_OFF(SerialState, last_break_enable);
    GEN_CHECK_OFF(SerialState, tsr_retry);
    GEN_CHECK_OFF(SerialState, msr_changed);
    GEN_CHECK_OFF(SerialState, fGCEnabled);
    GEN_CHECK_OFF(SerialState, fR0Enabled);
    GEN_CHECK_OFF(SerialState, fYieldOnLSRRead);
    GEN_CHECK_OFF(SerialState, char_transmit_time);

#ifdef VBOX_WITH_AHCI
    /* Storage/DevAHCI.cpp */

    GEN_CHECK_SIZE(AHCIPort);
    GEN_CHECK_OFF(AHCIPort, pDevInsR3);
    GEN_CHECK_OFF(AHCIPort, pDevInsR0);
    GEN_CHECK_OFF(AHCIPort, pDevInsRC);
    GEN_CHECK_OFF(AHCIPort, pAhciR3);
    GEN_CHECK_OFF(AHCIPort, pAhciR0);
    GEN_CHECK_OFF(AHCIPort, pAhciRC);
    GEN_CHECK_OFF(AHCIPort, regCLB);
    GEN_CHECK_OFF(AHCIPort, regCLBU);
    GEN_CHECK_OFF(AHCIPort, regFB);
    GEN_CHECK_OFF(AHCIPort, regFBU);
    GEN_CHECK_OFF(AHCIPort, regIS);
    GEN_CHECK_OFF(AHCIPort, regIE);
    GEN_CHECK_OFF(AHCIPort, regCMD);
    GEN_CHECK_OFF(AHCIPort, regTFD);
    GEN_CHECK_OFF(AHCIPort, regSIG);
    GEN_CHECK_OFF(AHCIPort, regSSTS);
    GEN_CHECK_OFF(AHCIPort, regSCTL);
    GEN_CHECK_OFF(AHCIPort, regSERR);
    GEN_CHECK_OFF(AHCIPort, regSACT);
    GEN_CHECK_OFF(AHCIPort, regCI);
    GEN_CHECK_OFF(AHCIPort, GCPhysAddrClb);
    GEN_CHECK_OFF(AHCIPort, GCPhysAddrFb);
    GEN_CHECK_OFF(AHCIPort, cTasksActive);
    GEN_CHECK_OFF(AHCIPort, fPoweredOn);
    GEN_CHECK_OFF(AHCIPort, fSpunUp);
    GEN_CHECK_OFF(AHCIPort, fFirstD2HFisSend);
    GEN_CHECK_OFF(AHCIPort, fATAPI);
    GEN_CHECK_OFF(AHCIPort, fATAPIPassthrough);
    GEN_CHECK_OFF(AHCIPort, fPortReset);
    GEN_CHECK_OFF(AHCIPort, fAsyncInterface);
    GEN_CHECK_OFF(AHCIPort, fResetDevice);
    GEN_CHECK_OFF(AHCIPort, fAsyncIOThreadIdle);
    GEN_CHECK_OFF(AHCIPort, fRedo);
    GEN_CHECK_OFF(AHCIPort, cTotalSectors);
    GEN_CHECK_OFF(AHCIPort, cMultSectors);
    GEN_CHECK_OFF(AHCIPort, uATATransferMode);
    GEN_CHECK_OFF(AHCIPort, abATAPISense);
    GEN_CHECK_OFF(AHCIPort, cNotifiedMediaChange);
    GEN_CHECK_OFF(AHCIPort, cLogSectorsPerPhysicalExp);
    GEN_CHECK_OFF(AHCIPort, MediaEventStatus);
    GEN_CHECK_OFF(AHCIPort, MediaTrackType);
    GEN_CHECK_OFF(AHCIPort, iLUN);
    GEN_CHECK_OFF(AHCIPort, u32TasksFinished);
    GEN_CHECK_OFF(AHCIPort, u32QueuedTasksFinished);
    GEN_CHECK_OFF(AHCIPort, u32TasksNew);
    GEN_CHECK_OFF(AHCIPort, u32CurrentCommandSlot);
    GEN_CHECK_OFF(AHCIPort, pDrvBase);
    GEN_CHECK_OFF(AHCIPort, pDrvBlock);
    GEN_CHECK_OFF(AHCIPort, pDrvBlockAsync);
    GEN_CHECK_OFF(AHCIPort, pDrvBlockBios);
    GEN_CHECK_OFF(AHCIPort, pDrvMount);
    GEN_CHECK_OFF(AHCIPort, IBase);
    GEN_CHECK_OFF(AHCIPort, IPort);
    GEN_CHECK_OFF(AHCIPort, IPortAsync);
    GEN_CHECK_OFF(AHCIPort, IMountNotify);
    GEN_CHECK_OFF(AHCIPort, PCHSGeometry);
    GEN_CHECK_OFF(AHCIPort, Led);
    GEN_CHECK_OFF(AHCIPort, pAsyncIOThread);
    GEN_CHECK_OFF(AHCIPort, AsyncIORequestSem);

    GEN_CHECK_OFF(AHCIPort, aCachedTasks);
    GEN_CHECK_OFF(AHCIPort, pTaskErr);
    GEN_CHECK_OFF(AHCIPort, pTrackList);
    GEN_CHECK_OFF(AHCIPort, StatDMA);
    GEN_CHECK_OFF(AHCIPort, StatBytesWritten);
    GEN_CHECK_OFF(AHCIPort, StatBytesRead);
    GEN_CHECK_OFF(AHCIPort, StatIORequestsPerSecond);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(AHCIPort, StatProfileProcessTime);
    GEN_CHECK_OFF(AHCIPort, StatProfileReadWrite);
#endif
    GEN_CHECK_OFF(AHCIPort, fAsyncIOThreadIdle);
    GEN_CHECK_OFF(AHCIPort, szSerialNumber);
    GEN_CHECK_OFF(AHCIPort, szSerialNumber[AHCI_SERIAL_NUMBER_LENGTH]); /* One additional byte for the termination.*/
    GEN_CHECK_OFF(AHCIPort, szFirmwareRevision);
    GEN_CHECK_OFF(AHCIPort, szFirmwareRevision[AHCI_FIRMWARE_REVISION_LENGTH]); /* One additional byte for the termination.*/
    GEN_CHECK_OFF(AHCIPort, szModelNumber);
    GEN_CHECK_OFF(AHCIPort, szModelNumber[AHCI_MODEL_NUMBER_LENGTH]); /* One additional byte for the termination.*/
    GEN_CHECK_OFF(AHCIPort, szInquiryVendorId[AHCI_ATAPI_INQUIRY_VENDOR_ID_LENGTH]);
    GEN_CHECK_OFF(AHCIPort, szInquiryProductId);
    GEN_CHECK_OFF(AHCIPort, szInquiryProductId[AHCI_ATAPI_INQUIRY_PRODUCT_ID_LENGTH]);
    GEN_CHECK_OFF(AHCIPort, szInquiryRevision);
    GEN_CHECK_OFF(AHCIPort, szInquiryRevision[AHCI_ATAPI_INQUIRY_REVISION_LENGTH]);
    GEN_CHECK_OFF(AHCIPort, cErrors);
    GEN_CHECK_OFF(AHCIPort, fRedo);

    GEN_CHECK_SIZE(AHCI);
    GEN_CHECK_OFF(AHCI, dev);
    GEN_CHECK_OFF(AHCI, pDevInsR3);
    GEN_CHECK_OFF(AHCI, pDevInsR0);
    GEN_CHECK_OFF(AHCI, pDevInsRC);
    GEN_CHECK_OFF(AHCI, IBase);
    GEN_CHECK_OFF(AHCI, ILeds);
    GEN_CHECK_OFF(AHCI, pLedsConnector);
    GEN_CHECK_OFF(AHCI, MMIOBase);
    GEN_CHECK_OFF(AHCI, regHbaCap);
    GEN_CHECK_OFF(AHCI, regHbaCtrl);
    GEN_CHECK_OFF(AHCI, regHbaIs);
    GEN_CHECK_OFF(AHCI, regHbaPi);
    GEN_CHECK_OFF(AHCI, regHbaVs);
    GEN_CHECK_OFF(AHCI, regHbaCccCtl);
    GEN_CHECK_OFF(AHCI, regHbaCccPorts);
    GEN_CHECK_OFF(AHCI, regIdx);
    GEN_CHECK_OFF(AHCI, pHbaCccTimerR3);
    GEN_CHECK_OFF(AHCI, pHbaCccTimerR0);
    GEN_CHECK_OFF(AHCI, pHbaCccTimerRC);
    GEN_CHECK_OFF(AHCI, pNotifierQueueR3);
    GEN_CHECK_OFF(AHCI, pNotifierQueueR0);
    GEN_CHECK_OFF(AHCI, pNotifierQueueRC);
    GEN_CHECK_OFF(AHCI, uCccPortNr);
    GEN_CHECK_OFF(AHCI, uCccTimeout);
    GEN_CHECK_OFF(AHCI, uCccNr);
    GEN_CHECK_OFF(AHCI, uCccCurrentNr);
    GEN_CHECK_OFF(AHCI, ahciPort);
    GEN_CHECK_OFF(AHCI, ahciPort[AHCI_MAX_NR_PORTS_IMPL-1]);
    GEN_CHECK_OFF(AHCI, lock);
    GEN_CHECK_OFF(AHCI, u32PortsInterrupted);
    GEN_CHECK_OFF(AHCI, fReset);
    GEN_CHECK_OFF(AHCI, f64BitAddr);
    GEN_CHECK_OFF(AHCI, fGCEnabled);
    GEN_CHECK_OFF(AHCI, fR0Enabled);
    GEN_CHECK_OFF(AHCI, fSignalIdle);
    GEN_CHECK_OFF(AHCI, fBootable);
    GEN_CHECK_OFF(AHCI, cPortsImpl);
    GEN_CHECK_OFF(AHCI, cCmdSlotsAvail);
    GEN_CHECK_OFF(AHCI, f8ByteMMIO4BytesWrittenSuccessfully);
#endif /* VBOX_WITH_AHCI */

#ifdef VBOX_WITH_E1000
    GEN_CHECK_SIZE(EEPROM93C46);
    GEN_CHECK_OFF(EEPROM93C46, m_eState);
    GEN_CHECK_OFF(EEPROM93C46, m_au16Data);
    GEN_CHECK_OFF(EEPROM93C46, m_fWriteEnabled);
    GEN_CHECK_OFF(EEPROM93C46, m_u16Word);
    GEN_CHECK_OFF(EEPROM93C46, m_u16Mask);
    GEN_CHECK_OFF(EEPROM93C46, m_u16Addr);
    GEN_CHECK_OFF(EEPROM93C46, m_u32InternalWires);
    GEN_CHECK_OFF(EEPROM93C46, m_eOp);

    GEN_CHECK_SIZE(E1KSTATE);
    GEN_CHECK_OFF(E1KSTATE, IBase);
    GEN_CHECK_OFF(E1KSTATE, INetworkDown);
    GEN_CHECK_OFF(E1KSTATE, INetworkConfig);
    GEN_CHECK_OFF(E1KSTATE, ILeds);
    GEN_CHECK_OFF(E1KSTATE, pDrvBase);
    GEN_CHECK_OFF(E1KSTATE, pDrvR3);
    GEN_CHECK_OFF(E1KSTATE, pDrvR0);
    GEN_CHECK_OFF(E1KSTATE, pDrvRC);
    GEN_CHECK_OFF(E1KSTATE, pLedsConnector);
    GEN_CHECK_OFF(E1KSTATE, pDevInsR3);
    GEN_CHECK_OFF(E1KSTATE, pDevInsR0);
    GEN_CHECK_OFF(E1KSTATE, pDevInsRC);
    GEN_CHECK_OFF(E1KSTATE, pTxQueueR3);
    GEN_CHECK_OFF(E1KSTATE, pTxQueueR0);
    GEN_CHECK_OFF(E1KSTATE, pTxQueueRC);
    GEN_CHECK_OFF(E1KSTATE, pCanRxQueueR3);
    GEN_CHECK_OFF(E1KSTATE, pCanRxQueueR0);
    GEN_CHECK_OFF(E1KSTATE, pCanRxQueueRC);
    GEN_CHECK_OFF(E1KSTATE, pRIDTimerR3);
    GEN_CHECK_OFF(E1KSTATE, pRIDTimerR0);
    GEN_CHECK_OFF(E1KSTATE, pRIDTimerRC);
    GEN_CHECK_OFF(E1KSTATE, pRADTimerR3);
    GEN_CHECK_OFF(E1KSTATE, pRADTimerR0);
    GEN_CHECK_OFF(E1KSTATE, pRADTimerRC);
    GEN_CHECK_OFF(E1KSTATE, pTIDTimerR3);
    GEN_CHECK_OFF(E1KSTATE, pTIDTimerR0);
    GEN_CHECK_OFF(E1KSTATE, pTIDTimerRC);
    GEN_CHECK_OFF(E1KSTATE, pTADTimerR3);
    GEN_CHECK_OFF(E1KSTATE, pTADTimerR0);
    GEN_CHECK_OFF(E1KSTATE, pTADTimerRC);
    GEN_CHECK_OFF(E1KSTATE, pIntTimerR3);
    GEN_CHECK_OFF(E1KSTATE, pIntTimerR0);
    GEN_CHECK_OFF(E1KSTATE, pIntTimerRC);
    GEN_CHECK_OFF(E1KSTATE, pLUTimerR3);
    GEN_CHECK_OFF(E1KSTATE, pLUTimerR0);
    GEN_CHECK_OFF(E1KSTATE, pLUTimerRC);
    GEN_CHECK_OFF(E1KSTATE, cs);
# ifndef E1K_GLOBAL_MUTEX
    GEN_CHECK_OFF(E1KSTATE, csRx);
# endif
    GEN_CHECK_OFF(E1KSTATE, addrMMReg);
    GEN_CHECK_OFF(E1KSTATE, macConfigured);
    GEN_CHECK_OFF(E1KSTATE, IOPortBase);
    GEN_CHECK_OFF(E1KSTATE, pciDevice);
    GEN_CHECK_OFF(E1KSTATE, u64AckedAt);
    GEN_CHECK_OFF(E1KSTATE, fIntRaised);
    GEN_CHECK_OFF(E1KSTATE, fCableConnected);
    GEN_CHECK_OFF(E1KSTATE, fR0Enabled);
    GEN_CHECK_OFF(E1KSTATE, fRCEnabled);
    GEN_CHECK_OFF(E1KSTATE, auRegs[E1K_NUM_OF_32BIT_REGS]);
    GEN_CHECK_OFF(E1KSTATE, led);
    GEN_CHECK_OFF(E1KSTATE, u32PktNo);
    GEN_CHECK_OFF(E1KSTATE, uSelectedReg);
    GEN_CHECK_OFF(E1KSTATE, auMTA[128]);
    GEN_CHECK_OFF(E1KSTATE, aRecAddr);
    GEN_CHECK_OFF(E1KSTATE, auVFTA[128]);
    GEN_CHECK_OFF(E1KSTATE, u16RxBSize);
    GEN_CHECK_OFF(E1KSTATE, fLocked);
    GEN_CHECK_OFF(E1KSTATE, fDelayInts);
    GEN_CHECK_OFF(E1KSTATE, fIntMaskUsed);
    GEN_CHECK_OFF(E1KSTATE, fMaybeOutOfSpace);
    GEN_CHECK_OFF(E1KSTATE, hEventMoreRxDescAvail);
    GEN_CHECK_OFF(E1KSTATE, contextTSE);
    GEN_CHECK_OFF(E1KSTATE, contextNormal);
# ifdef E1K_WITH_TXD_CACHE
    GEN_CHECK_OFF(E1KSTATE, aTxDescriptors);
    GEN_CHECK_OFF(E1KSTATE, nTxDFetched);
    GEN_CHECK_OFF(E1KSTATE, iTxDCurrent);
    GEN_CHECK_OFF(E1KSTATE, fGSO);
    GEN_CHECK_OFF(E1KSTATE, cbTxAlloc);
# endif
    GEN_CHECK_OFF(E1KSTATE, GsoCtx);
    GEN_CHECK_OFF(E1KSTATE, uTxFallback);
    GEN_CHECK_OFF(E1KSTATE, fVTag);
    GEN_CHECK_OFF(E1KSTATE, u16VTagTCI);
    GEN_CHECK_OFF(E1KSTATE, aTxPacketFallback[E1K_MAX_TX_PKT_SIZE]);
    GEN_CHECK_OFF(E1KSTATE, u16TxPktLen);
    GEN_CHECK_OFF(E1KSTATE, fIPcsum);
    GEN_CHECK_OFF(E1KSTATE, fTCPcsum);
    GEN_CHECK_OFF(E1KSTATE, u32PayRemain);
    GEN_CHECK_OFF(E1KSTATE, u16HdrRemain);
    GEN_CHECK_OFF(E1KSTATE, u16SavedFlags);
    GEN_CHECK_OFF(E1KSTATE, u32SavedCsum);
    GEN_CHECK_OFF(E1KSTATE, eeprom);
    GEN_CHECK_OFF(E1KSTATE, phy);
    GEN_CHECK_OFF(E1KSTATE, StatReceiveBytes);
#endif /* VBOX_WITH_E1000 */

#ifdef VBOX_WITH_VIRTIO
    GEN_CHECK_OFF(VPCISTATE, cs);
    GEN_CHECK_OFF(VPCISTATE, szInstance);
    GEN_CHECK_OFF(VPCISTATE, IBase);
    GEN_CHECK_OFF(VPCISTATE, ILeds);
    GEN_CHECK_OFF(VPCISTATE, pLedsConnector);
    GEN_CHECK_OFF(VPCISTATE, pDevInsR3);
    GEN_CHECK_OFF(VPCISTATE, pDevInsR0);
    GEN_CHECK_OFF(VPCISTATE, pDevInsRC);
    GEN_CHECK_OFF(VPCISTATE, pciDevice);
    GEN_CHECK_OFF(VPCISTATE, IOPortBase);
    GEN_CHECK_OFF(VPCISTATE, led);
    GEN_CHECK_OFF(VPCISTATE, uGuestFeatures);
    GEN_CHECK_OFF(VPCISTATE, uQueueSelector);
    GEN_CHECK_OFF(VPCISTATE, uStatus);
    GEN_CHECK_OFF(VPCISTATE, uISR);
    GEN_CHECK_OFF(VPCISTATE, Queues);
    GEN_CHECK_OFF(VPCISTATE, Queues[VIRTIO_MAX_NQUEUES]);
    GEN_CHECK_OFF(VNETSTATE, VPCI);
    GEN_CHECK_OFF(VNETSTATE, INetworkDown);
    GEN_CHECK_OFF(VNETSTATE, INetworkConfig);
    GEN_CHECK_OFF(VNETSTATE, pDrvBase);
    GEN_CHECK_OFF(VNETSTATE, pCanRxQueueR3);
    GEN_CHECK_OFF(VNETSTATE, pCanRxQueueR0);
    GEN_CHECK_OFF(VNETSTATE, pCanRxQueueRC);
    GEN_CHECK_OFF(VNETSTATE, pLinkUpTimer);
# ifdef VNET_TX_DELAY
    GEN_CHECK_OFF(VNETSTATE, pTxTimerR3);
    GEN_CHECK_OFF(VNETSTATE, pTxTimerR0);
    GEN_CHECK_OFF(VNETSTATE, pTxTimerRC);
# endif /* VNET_TX_DELAY */
    GEN_CHECK_OFF(VNETSTATE, config);
    GEN_CHECK_OFF(VNETSTATE, macConfigured);
    GEN_CHECK_OFF(VNETSTATE, fCableConnected);
    GEN_CHECK_OFF(VNETSTATE, u32PktNo);
    GEN_CHECK_OFF(VNETSTATE, fPromiscuous);
    GEN_CHECK_OFF(VNETSTATE, fAllMulti);
    GEN_CHECK_OFF(VNETSTATE, pRxQueue);
    GEN_CHECK_OFF(VNETSTATE, pTxQueue);
    GEN_CHECK_OFF(VNETSTATE, pCtlQueue);
    GEN_CHECK_OFF(VNETSTATE, fMaybeOutOfSpace);
    GEN_CHECK_OFF(VNETSTATE, hEventMoreRxDescAvail);
#endif /* VBOX_WITH_VIRTIO */

#ifdef VBOX_WITH_SCSI
    GEN_CHECK_SIZE(VBOXSCSI);
    GEN_CHECK_OFF(VBOXSCSI, regIdentify);
    GEN_CHECK_OFF(VBOXSCSI, uTargetDevice);
    GEN_CHECK_OFF(VBOXSCSI, uTxDir);
    GEN_CHECK_OFF(VBOXSCSI, cbCDB);
    GEN_CHECK_OFF(VBOXSCSI, abCDB);
    GEN_CHECK_OFF(VBOXSCSI, abCDB[11]);
    GEN_CHECK_OFF(VBOXSCSI, iCDB);
    GEN_CHECK_OFF(VBOXSCSI, pbBuf);
    GEN_CHECK_OFF(VBOXSCSI, cbBuf);
    GEN_CHECK_OFF(VBOXSCSI, iBuf);
    GEN_CHECK_OFF(VBOXSCSI, fBusy);
    GEN_CHECK_OFF(VBOXSCSI, enmState);
#endif

    /* VMMDev*.cpp/h */
    GEN_CHECK_SIZE(VMMDEV);
    GEN_CHECK_OFF(VMMDEV, PciDev);
    GEN_CHECK_OFF(VMMDEV, CritSect);
    GEN_CHECK_OFF(VMMDEV, hypervisorSize);
    GEN_CHECK_OFF(VMMDEV, mouseCapabilities);
    GEN_CHECK_OFF(VMMDEV, mouseXAbs);
    GEN_CHECK_OFF(VMMDEV, mouseYAbs);
    GEN_CHECK_OFF(VMMDEV, fHostCursorRequested);
    GEN_CHECK_OFF(VMMDEV, pDevIns);
    GEN_CHECK_OFF(VMMDEV, IBase);
    GEN_CHECK_OFF(VMMDEV, IPort);
#ifdef VBOX_WITH_HGCM
    GEN_CHECK_OFF(VMMDEV, IHGCMPort);
#endif
    GEN_CHECK_OFF(VMMDEV, pDrvBase);
    GEN_CHECK_OFF(VMMDEV, pDrv);
#ifdef VBOX_WITH_HGCM
    GEN_CHECK_OFF(VMMDEV, pHGCMDrv);
#endif
    GEN_CHECK_OFF(VMMDEV, szMsg);
    GEN_CHECK_OFF(VMMDEV, iMsg);
    GEN_CHECK_OFF(VMMDEV, irq);
    GEN_CHECK_OFF(VMMDEV, u32HostEventFlags);
    GEN_CHECK_OFF(VMMDEV, u32GuestFilterMask);
    GEN_CHECK_OFF(VMMDEV, u32NewGuestFilterMask);
    GEN_CHECK_OFF(VMMDEV, fNewGuestFilterMask);
    GEN_CHECK_OFF(VMMDEV, GCPhysVMMDevRAM);
    GEN_CHECK_OFF(VMMDEV, pVMMDevRAMR3);
    GEN_CHECK_OFF(VMMDEV, pVMMDevHeapR3);
    GEN_CHECK_OFF(VMMDEV, GCPhysVMMDevHeap);
    GEN_CHECK_OFF(VMMDEV, guestInfo);
    GEN_CHECK_OFF(VMMDEV, guestCaps);
    GEN_CHECK_OFF(VMMDEV, fu32AdditionsOk);
    GEN_CHECK_OFF(VMMDEV, u32VideoAccelEnabled);
    GEN_CHECK_OFF(VMMDEV, displayChangeData);
    GEN_CHECK_OFF(VMMDEV, pCredentials);
    GEN_CHECK_OFF(VMMDEV, cMbMemoryBalloon);
    GEN_CHECK_OFF(VMMDEV, cMbMemoryBalloonLast);
    GEN_CHECK_OFF(VMMDEV, cbGuestRAM);
    GEN_CHECK_OFF(VMMDEV, idSession);
    GEN_CHECK_OFF(VMMDEV, u32StatIntervalSize);
    GEN_CHECK_OFF(VMMDEV, u32LastStatIntervalSize);
    GEN_CHECK_OFF(VMMDEV, fLastSeamlessEnabled),
    GEN_CHECK_OFF(VMMDEV, fSeamlessEnabled);
    GEN_CHECK_OFF(VMMDEV, fVRDPEnabled);
    GEN_CHECK_OFF(VMMDEV, uVRDPExperienceLevel);
#ifdef VMMDEV_WITH_ALT_TIMESYNC
    GEN_CHECK_OFF(VMMDEV, hostTime);
    GEN_CHECK_OFF(VMMDEV, fTimesyncBackdoorLo);
#endif
    GEN_CHECK_OFF(VMMDEV, fGetHostTimeDisabled);
    GEN_CHECK_OFF(VMMDEV, fBackdoorLogDisabled);
    GEN_CHECK_OFF(VMMDEV, fKeepCredentials);
    GEN_CHECK_OFF(VMMDEV, fHeapEnabled);
#ifdef VBOX_WITH_HGCM
    GEN_CHECK_OFF(VMMDEV, pHGCMCmdList);
    GEN_CHECK_OFF(VMMDEV, critsectHGCMCmdList);
    GEN_CHECK_OFF(VMMDEV, u32HGCMEnabled);
#endif
    GEN_CHECK_OFF(VMMDEV, SharedFolders);
    GEN_CHECK_OFF(VMMDEV, SharedFolders.Led);
    GEN_CHECK_OFF(VMMDEV, SharedFolders.ILeds);
    GEN_CHECK_OFF(VMMDEV, SharedFolders.pLedsConnector);
    GEN_CHECK_OFF(VMMDEV, fCpuHotPlugEventsEnabled);
    GEN_CHECK_OFF(VMMDEV, enmCpuHotPlugEvent);
    GEN_CHECK_OFF(VMMDEV, idCpuCore);
    GEN_CHECK_OFF(VMMDEV, idCpuPackage);
    GEN_CHECK_OFF(VMMDEV, StatMemBalloonChunks);
    GEN_CHECK_OFF(VMMDEV, fRZEnabled);
    GEN_CHECK_OFF(VMMDEV, fTestingEnabled);
    GEN_CHECK_OFF(VMMDEV, u32TestingHighTimestamp);
    GEN_CHECK_OFF(VMMDEV, u32TestingCmd);
    GEN_CHECK_OFF(VMMDEV, offTestingData);
    GEN_CHECK_OFF(VMMDEV, TestingData);
    GEN_CHECK_OFF(VMMDEV, TestingData.Value.u64Value);
    GEN_CHECK_OFF(VMMDEV, TestingData.Value.u32Unit);
    GEN_CHECK_OFF(VMMDEV, TestingData.Value.szName);

#ifdef VBOX_WITH_BUSLOGIC
    GEN_CHECK_SIZE(BUSLOGICDEVICE);
    GEN_CHECK_OFF(BUSLOGICDEVICE, pBusLogicR3);
    GEN_CHECK_OFF(BUSLOGICDEVICE, pBusLogicR0);
    GEN_CHECK_OFF(BUSLOGICDEVICE, pBusLogicRC);
    GEN_CHECK_OFF(BUSLOGICDEVICE, fPresent);
    GEN_CHECK_OFF(BUSLOGICDEVICE, iLUN);
    GEN_CHECK_OFF(BUSLOGICDEVICE, IBase);
    GEN_CHECK_OFF(BUSLOGICDEVICE, ISCSIPort);
    GEN_CHECK_OFF(BUSLOGICDEVICE, ILed);
    GEN_CHECK_OFF(BUSLOGICDEVICE, pDrvBase);
    GEN_CHECK_OFF(BUSLOGICDEVICE, pDrvSCSIConnector);
    GEN_CHECK_OFF(BUSLOGICDEVICE, Led);
    GEN_CHECK_OFF(BUSLOGICDEVICE, cOutstandingRequests);

    GEN_CHECK_SIZE(BUSLOGIC);
    GEN_CHECK_OFF(BUSLOGIC, dev);
    GEN_CHECK_OFF(BUSLOGIC, pDevInsR3);
    GEN_CHECK_OFF(BUSLOGIC, pDevInsR0);
    GEN_CHECK_OFF(BUSLOGIC, pDevInsRC);
    GEN_CHECK_OFF(BUSLOGIC, IOPortBase);
    GEN_CHECK_OFF(BUSLOGIC, MMIOBase);
    GEN_CHECK_OFF(BUSLOGIC, regStatus);
    GEN_CHECK_OFF(BUSLOGIC, regInterrupt);
    GEN_CHECK_OFF(BUSLOGIC, regGeometry);
    GEN_CHECK_OFF(BUSLOGIC, LocalRam);
    GEN_CHECK_OFF(BUSLOGIC, uOperationCode);
    GEN_CHECK_OFF(BUSLOGIC, aCommandBuffer);
    GEN_CHECK_OFF(BUSLOGIC, aCommandBuffer[BUSLOGIC_COMMAND_SIZE_MAX]);
    GEN_CHECK_OFF(BUSLOGIC, iParameter);
    GEN_CHECK_OFF(BUSLOGIC, cbCommandParametersLeft);
    GEN_CHECK_OFF(BUSLOGIC, fUseLocalRam);
    GEN_CHECK_OFF(BUSLOGIC, aReplyBuffer);
    GEN_CHECK_OFF(BUSLOGIC, aReplyBuffer[BUSLOGIC_REPLY_SIZE_MAX]);
    GEN_CHECK_OFF(BUSLOGIC, iReply);
    GEN_CHECK_OFF(BUSLOGIC, cbReplyParametersLeft);
    GEN_CHECK_OFF(BUSLOGIC, fIRQEnabled);
    GEN_CHECK_OFF(BUSLOGIC, cMailbox);
    GEN_CHECK_OFF(BUSLOGIC, GCPhysAddrMailboxOutgoingBase);
    GEN_CHECK_OFF(BUSLOGIC, uMailboxOutgoingPositionCurrent);
    GEN_CHECK_OFF(BUSLOGIC, cMailboxesReady);
    GEN_CHECK_OFF(BUSLOGIC, fNotificationSend);
    GEN_CHECK_OFF(BUSLOGIC, GCPhysAddrMailboxIncomingBase);
    GEN_CHECK_OFF(BUSLOGIC, uMailboxIncomingPositionCurrent);
    GEN_CHECK_OFF(BUSLOGIC, fStrictRoundRobinMode);
    GEN_CHECK_OFF(BUSLOGIC, fExtendedLunCCBFormat);
    GEN_CHECK_OFF(BUSLOGIC, pNotifierQueueR3);
    GEN_CHECK_OFF(BUSLOGIC, pNotifierQueueR0);
    GEN_CHECK_OFF(BUSLOGIC, pNotifierQueueRC);
    GEN_CHECK_OFF(BUSLOGIC, CritSectIntr);
    GEN_CHECK_OFF(BUSLOGIC, hTaskCache);
    GEN_CHECK_OFF(BUSLOGIC, VBoxSCSI);
    GEN_CHECK_OFF(BUSLOGIC, aDeviceStates);
    GEN_CHECK_OFF(BUSLOGIC, aDeviceStates[BUSLOGIC_MAX_DEVICES-1]);
    GEN_CHECK_OFF(BUSLOGIC, IBase);
    GEN_CHECK_OFF(BUSLOGIC, ILeds);
    GEN_CHECK_OFF(BUSLOGIC, pLedsConnector);
    GEN_CHECK_OFF(BUSLOGIC, fSignalIdle);
    GEN_CHECK_OFF(BUSLOGIC, fRedo);
    GEN_CHECK_OFF(BUSLOGIC, pTasksRedoHead);
#endif /* VBOX_WITH_BUSLOGIC */

#ifdef VBOX_WITH_LSILOGIC
    GEN_CHECK_SIZE(LSILOGICSCSI);
    GEN_CHECK_OFF(LSILOGICSCSI, PciDev);
    GEN_CHECK_OFF(LSILOGICSCSI, pDevInsR3);
    GEN_CHECK_OFF(LSILOGICSCSI, pDevInsR0);
    GEN_CHECK_OFF(LSILOGICSCSI, pDevInsRC);
    GEN_CHECK_OFF(LSILOGICSCSI, fGCEnabled);
    GEN_CHECK_OFF(LSILOGICSCSI, fR0Enabled);
    GEN_CHECK_OFF(LSILOGICSCSI, enmState);
    GEN_CHECK_OFF(LSILOGICSCSI, enmWhoInit);
    GEN_CHECK_OFF(LSILOGICSCSI, fDoorbellInProgress);
    GEN_CHECK_OFF(LSILOGICSCSI, fDiagnosticEnabled);
    GEN_CHECK_OFF(LSILOGICSCSI, fNotificationSend);
    GEN_CHECK_OFF(LSILOGICSCSI, fEventNotificationEnabled);
    GEN_CHECK_OFF(LSILOGICSCSI, pNotificationQueueR3);
    GEN_CHECK_OFF(LSILOGICSCSI, pNotificationQueueR0);
    GEN_CHECK_OFF(LSILOGICSCSI, pNotificationQueueRC);
    GEN_CHECK_OFF(LSILOGICSCSI, cDeviceStates);
    GEN_CHECK_OFF(LSILOGICSCSI, paDeviceStates);
    GEN_CHECK_OFF(LSILOGICSCSI, GCPhysMMIOBase);
    GEN_CHECK_OFF(LSILOGICSCSI, IOPortBase);
    GEN_CHECK_OFF(LSILOGICSCSI, uInterruptMask);
    GEN_CHECK_OFF(LSILOGICSCSI, uInterruptStatus);
    GEN_CHECK_OFF(LSILOGICSCSI, aMessage);
    GEN_CHECK_OFF(LSILOGICSCSI, aMessage[sizeof(MptConfigurationRequest)-1]);
    GEN_CHECK_OFF(LSILOGICSCSI, iMessage);
    GEN_CHECK_OFF(LSILOGICSCSI, cMessage);
    GEN_CHECK_OFF(LSILOGICSCSI, ReplyBuffer);
    GEN_CHECK_OFF(LSILOGICSCSI, uNextReplyEntryRead);
    GEN_CHECK_OFF(LSILOGICSCSI, cReplySize);
    GEN_CHECK_OFF(LSILOGICSCSI, u16IOCFaultCode);
    GEN_CHECK_OFF(LSILOGICSCSI, u32HostMFAHighAddr);
    GEN_CHECK_OFF(LSILOGICSCSI, u32SenseBufferHighAddr);
    GEN_CHECK_OFF(LSILOGICSCSI, cMaxDevices);
    GEN_CHECK_OFF(LSILOGICSCSI, cMaxBuses);
    GEN_CHECK_OFF(LSILOGICSCSI, cbReplyFrame);
    GEN_CHECK_OFF(LSILOGICSCSI, iDiagnosticAccess);
    GEN_CHECK_OFF(LSILOGICSCSI, cReplyQueueEntries);
    GEN_CHECK_OFF(LSILOGICSCSI, cRequestQueueEntries);
    GEN_CHECK_OFF(LSILOGICSCSI, ReplyPostQueueCritSect);
    GEN_CHECK_OFF(LSILOGICSCSI, ReplyFreeQueueCritSect);
    GEN_CHECK_OFF(LSILOGICSCSI, pReplyFreeQueueBaseR3);
    GEN_CHECK_OFF(LSILOGICSCSI, pReplyPostQueueBaseR3);
    GEN_CHECK_OFF(LSILOGICSCSI, pRequestQueueBaseR3);
    GEN_CHECK_OFF(LSILOGICSCSI, pReplyFreeQueueBaseR0);
    GEN_CHECK_OFF(LSILOGICSCSI, pReplyPostQueueBaseR0);
    GEN_CHECK_OFF(LSILOGICSCSI, pRequestQueueBaseR0);
    GEN_CHECK_OFF(LSILOGICSCSI, pReplyFreeQueueBaseRC);
    GEN_CHECK_OFF(LSILOGICSCSI, pReplyPostQueueBaseRC);
    GEN_CHECK_OFF(LSILOGICSCSI, pRequestQueueBaseRC);
    GEN_CHECK_OFF(LSILOGICSCSI, uReplyFreeQueueNextEntryFreeWrite);
    GEN_CHECK_OFF(LSILOGICSCSI, uReplyFreeQueueNextAddressRead);
    GEN_CHECK_OFF(LSILOGICSCSI, uReplyPostQueueNextEntryFreeWrite);
    GEN_CHECK_OFF(LSILOGICSCSI, uReplyPostQueueNextAddressRead);
    GEN_CHECK_OFF(LSILOGICSCSI, uRequestQueueNextEntryFreeWrite);
    GEN_CHECK_OFF(LSILOGICSCSI, uRequestQueueNextAddressRead);
    GEN_CHECK_OFF(LSILOGICSCSI, u16NextHandle);
    GEN_CHECK_OFF(LSILOGICSCSI, enmCtrlType);
    GEN_CHECK_OFF(LSILOGICSCSI, VBoxSCSI);
    GEN_CHECK_OFF(LSILOGICSCSI, hTaskCache);
    GEN_CHECK_OFF(LSILOGICSCSI, IBase);
    GEN_CHECK_OFF(LSILOGICSCSI, ILeds);
    GEN_CHECK_OFF(LSILOGICSCSI, pLedsConnector);
    GEN_CHECK_OFF(LSILOGICSCSI, pConfigurationPages);
    GEN_CHECK_OFF(LSILOGICSCSI, fSignalIdle);
    GEN_CHECK_OFF(LSILOGICSCSI, fRedo);
    GEN_CHECK_OFF(LSILOGICSCSI, pTasksRedoHead);
#endif /* VBOX_WITH_LSILOGIC */

    GEN_CHECK_SIZE(HPET);
    GEN_CHECK_OFF(HPET, pDevInsR3);
    GEN_CHECK_OFF(HPET, pDevInsR0);
    GEN_CHECK_OFF(HPET, pDevInsRC);
    GEN_CHECK_OFF(HPET, u64HpetOffset);
    GEN_CHECK_OFF(HPET, u32Capabilities);
    GEN_CHECK_OFF(HPET, u32Period);
    GEN_CHECK_OFF(HPET, u64HpetConfig);
    GEN_CHECK_OFF(HPET, u64Isr);
    GEN_CHECK_OFF(HPET, u64HpetCounter);
    GEN_CHECK_OFF(HPET, CritSect);
    GEN_CHECK_OFF(HPET, fIch9);

    GEN_CHECK_SIZE(HPETTIMER);
    GEN_CHECK_OFF(HPETTIMER, pTimerR3);
    GEN_CHECK_OFF(HPETTIMER, pHpetR3);
    GEN_CHECK_OFF(HPETTIMER, pTimerR0);
    GEN_CHECK_OFF(HPETTIMER, pHpetR0);
    GEN_CHECK_OFF(HPETTIMER, pTimerRC);
    GEN_CHECK_OFF(HPETTIMER, pHpetRC);
    GEN_CHECK_OFF(HPETTIMER, idxTimer);
    GEN_CHECK_OFF(HPETTIMER, u64Config);
    GEN_CHECK_OFF(HPETTIMER, u64Cmp);
    GEN_CHECK_OFF(HPETTIMER, u64Fsb);
    GEN_CHECK_OFF(HPETTIMER, u64Period);
    GEN_CHECK_OFF(HPETTIMER, u8Wrap);

    return (0);
}
