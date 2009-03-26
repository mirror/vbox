/* $Id$ */
/** @file
 * tstDeviceStructSizeGC - Generate structure member and size checks from the GC perspective.
 *
 * This is built using the VBOXGC template but linked into a host
 * ring-3 executable, rather hacky.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#undef LOG_GROUP
#include "../Bus/DevPCI.cpp" /* must be first! */
#undef LOG_GROUP
#include "../Graphics/DevVGA.cpp"
#undef LOG_GROUP
#include "../Input/DevPS2.cpp"
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
#include "../Storage/DevATA.cpp"
#ifdef VBOX_WITH_USB
# undef LOG_GROUP
# include "../USB/DevOHCI.cpp"
# include "../USB/DevEHCI.cpp"
#endif
/*#undef LOG_GROUP
#include "../VMMDev/VBoxDev.cpp"*/
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
#ifdef VBOX_WITH_BUSLOGIC
# undef LOG_GROUP
# include "../Storage/DevBusLogic.cpp"
#endif
#ifdef VBOX_WITH_LSILOGIC
# undef LOG_GROUP
# include "../Storage/DevLsiLogicSCSI.cpp"
#endif
#ifdef VBOX_WITH_HPET
# undef LOG_GROUP
# include "../PC/DevHPET.cpp"
#endif

/* we don't use iprt here because we're pretending to be in GC! */
#include <stdio.h>

#define GEN_CHECK_SIZE(s)   printf("    CHECK_SIZE(%s, %d);\n", #s, (int)sizeof(s))
#define GEN_CHECK_OFF(s, m) printf("    CHECK_OFF(%s, %d, %s);\n", #s, (int)RT_OFFSETOF(s, m), #m)
#define GEN_CHECK_PADDING(s, m) printf("    CHECK_PADDING(%s, %s);\n", #s, #m)

int main()
{
    /* misc */
    GEN_CHECK_SIZE(PDMDEVINS);
    GEN_CHECK_OFF(PDMDEVINS, Internal);
    GEN_CHECK_OFF(PDMDEVINS, pDevReg);
    GEN_CHECK_OFF(PDMDEVINS, pCfgHandle);
    GEN_CHECK_OFF(PDMDEVINS, iInstance);
    GEN_CHECK_OFF(PDMDEVINS, IBase);
    GEN_CHECK_OFF(PDMDEVINS, pDevHlpR3);
    GEN_CHECK_OFF(PDMDEVINS, pDevHlpR0);
    GEN_CHECK_OFF(PDMDEVINS, pDevHlpRC);
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
    GEN_CHECK_OFF(PCIDEVICE, Int.s.fRequestedDevFn);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.fPciToPciBridge);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.uIrqPinState);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pfnBridgeConfigRead);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pfnBridgeConfigWrite);
    GEN_CHECK_PADDING(PCIDEVICE, Int);
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

    /* DevVGA.cpp */
    GEN_CHECK_SIZE(VGASTATE);
    GEN_CHECK_OFF(VGASTATE, vram_ptrR3);
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
    GEN_CHECK_OFF(VGASTATE, get_bpp);
    GEN_CHECK_OFF(VGASTATE, get_offsets);
    GEN_CHECK_OFF(VGASTATE, get_resolution);
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
    GEN_CHECK_OFF(VGASTATE, rgb_to_pixel);
    GEN_CHECK_OFF(VGASTATE, invalidated_y_table);
    GEN_CHECK_OFF(VGASTATE, invalidated_y_table[1]);
    GEN_CHECK_OFF(VGASTATE, invalidated_y_table[(VGA_MAX_HEIGHT / 32) - 1]);
    GEN_CHECK_OFF(VGASTATE, cursor_invalidate);
    GEN_CHECK_OFF(VGASTATE, cursor_draw_line);
    GEN_CHECK_OFF(VGASTATE, last_palette);
    GEN_CHECK_OFF(VGASTATE, last_palette[1]);
    GEN_CHECK_OFF(VGASTATE, last_ch_attr);
    GEN_CHECK_OFF(VGASTATE, last_ch_attr[CH_ATTR_SIZE - 1]);
    GEN_CHECK_OFF(VGASTATE, u32Marker);
    GEN_CHECK_OFF(VGASTATE, GCPhysVRAM);
    GEN_CHECK_OFF(VGASTATE, vram_ptrR0);
    GEN_CHECK_OFF(VGASTATE, vram_ptrRC);
    GEN_CHECK_OFF(VGASTATE, fLFBUpdated);
    GEN_CHECK_OFF(VGASTATE, fGCEnabled);
    GEN_CHECK_OFF(VGASTATE, fR0Enabled);
    GEN_CHECK_OFF(VGASTATE, fHasDirtyBits);
    GEN_CHECK_OFF(VGASTATE, fRemappedVGA);
    GEN_CHECK_OFF(VGASTATE, fRenderVRAM);
    GEN_CHECK_OFF(VGASTATE, RCPtrLFBHandler);
    GEN_CHECK_OFF(VGASTATE, au32DirtyBitmap);
    GEN_CHECK_OFF(VGASTATE, au32DirtyBitmap[1]);
    GEN_CHECK_OFF(VGASTATE, au32DirtyBitmap[(VGA_VRAM_MAX / PAGE_SIZE / 32) - 1]);
    GEN_CHECK_OFF(VGASTATE, pDevInsR3);
    GEN_CHECK_OFF(VGASTATE, pDevInsR0);
    GEN_CHECK_OFF(VGASTATE, pDevInsRC);
    GEN_CHECK_OFF(VGASTATE, Base);
    GEN_CHECK_OFF(VGASTATE, Port);
    GEN_CHECK_OFF(VGASTATE, pDrvBase);
    GEN_CHECK_OFF(VGASTATE, pDrv);
    GEN_CHECK_OFF(VGASTATE, RefreshTimer);
    GEN_CHECK_OFF(VGASTATE, cMilliesRefreshInterval);
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
#ifdef VBE_NEW_DYN_LIST
    GEN_CHECK_OFF(VGASTATE, cbWriteVBEExtraAddress);
#endif
#endif
#ifdef VBE_NEW_DYN_LIST
    GEN_CHECK_OFF(VGASTATE, cbVBEExtraData);
    GEN_CHECK_OFF(VGASTATE, pu8VBEExtraData);
    GEN_CHECK_OFF(VGASTATE, u16VBEExtraAddress);
#endif

    /* Input/pckbd.c */
    GEN_CHECK_SIZE(KBDQueue);
    GEN_CHECK_OFF(KBDQueue, data);
    GEN_CHECK_OFF(KBDQueue, rptr);
    GEN_CHECK_OFF(KBDQueue, wptr);
    GEN_CHECK_OFF(KBDQueue, count);
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
    GEN_CHECK_OFF(KBDState, queue);
    GEN_CHECK_OFF(KBDState, mouse_command_queue);
    GEN_CHECK_OFF(KBDState, mouse_event_queue);
    GEN_CHECK_OFF(KBDState, write_cmd);
    GEN_CHECK_OFF(KBDState, status);
    GEN_CHECK_OFF(KBDState, mode);
    GEN_CHECK_OFF(KBDState, kbd_write_cmd);
    GEN_CHECK_OFF(KBDState, scan_enabled);
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
    GEN_CHECK_OFF(KBDState, mouse_buttons);
    GEN_CHECK_OFF(KBDState, pDevInsR3);
    GEN_CHECK_OFF(KBDState, pDevInsR0);
    GEN_CHECK_OFF(KBDState, pDevInsRC);
    GEN_CHECK_OFF(KBDState, Keyboard.Base);
    GEN_CHECK_OFF(KBDState, Keyboard.Port);
    GEN_CHECK_OFF(KBDState, Keyboard.pDrvBase);
    GEN_CHECK_OFF(KBDState, Keyboard.pDrv);
    GEN_CHECK_OFF(KBDState, Mouse.Base);
    GEN_CHECK_OFF(KBDState, Mouse.Port);
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
    GEN_CHECK_OFF(PCNetState, abSendBuf);
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
    GEN_CHECK_OFF(PCNetState, pDrv);
    GEN_CHECK_OFF(PCNetState, pDrvBase);
    GEN_CHECK_OFF(PCNetState, IBase);
    GEN_CHECK_OFF(PCNetState, INetworkPort);
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
    GEN_CHECK_OFF(PCNetState, hSendEventSem);
    GEN_CHECK_OFF(PCNetState, pSendThread);
    GEN_CHECK_OFF(PCNetState, CritSect);
    GEN_CHECK_OFF(PCNetState, cPendingSends);
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
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(PCNetState, StatMMIOReadGC);
    GEN_CHECK_OFF(PCNetState, StatMIIReads);
# ifdef PCNET_NO_POLLING
    GEN_CHECK_OFF(PCNetState, StatRCVRingWrite);
    GEN_CHECK_OFF(PCNetState, StatRingWriteOutsideRangeGC);
# endif
#endif

    /* PC/DevACPI.cpp */
    GEN_CHECK_SIZE(ACPIState);
    GEN_CHECK_OFF(ACPIState, dev);
    GEN_CHECK_OFF(ACPIState, pm1a_en);
    GEN_CHECK_OFF(ACPIState, pm1a_sts);
    GEN_CHECK_OFF(ACPIState, pm1a_ctl);
    GEN_CHECK_OFF(ACPIState, pm_timer_initial);
    GEN_CHECK_OFF(ACPIState, tsR3);
    GEN_CHECK_OFF(ACPIState, tsR0);
    GEN_CHECK_OFF(ACPIState, tsRC);
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
    GEN_CHECK_OFF(ACPIState, IBase);
    GEN_CHECK_OFF(ACPIState, IACPIPort);
    GEN_CHECK_OFF(ACPIState, pDevIns);
    GEN_CHECK_OFF(ACPIState, pDrvBase);
    GEN_CHECK_OFF(ACPIState, pDrv);

    /* PC/DevPIC.cpp */
    GEN_CHECK_SIZE(PicState);
    GEN_CHECK_OFF(PicState, last_irr);
    GEN_CHECK_OFF(PicState, irr);
    GEN_CHECK_OFF(PicState, imr);
    GEN_CHECK_OFF(PicState, isr);
    GEN_CHECK_OFF(PicState, priority_add);
    GEN_CHECK_OFF(PicState, irq_base);
    GEN_CHECK_OFF(PicState, read_reg_select);
    GEN_CHECK_OFF(PicState, poll);
    GEN_CHECK_OFF(PicState, special_mask);
    GEN_CHECK_OFF(PicState, init_state);
    GEN_CHECK_OFF(PicState, auto_eoi);
    GEN_CHECK_OFF(PicState, rotate_on_auto_eoi);
    GEN_CHECK_OFF(PicState, special_fully_nested_mode);
    GEN_CHECK_OFF(PicState, init4);
    GEN_CHECK_OFF(PicState, elcr);
    GEN_CHECK_OFF(PicState, elcr_mask);
    GEN_CHECK_OFF(PicState, pDevInsR3);
    GEN_CHECK_OFF(PicState, pDevInsR0);
    GEN_CHECK_OFF(PicState, pDevInsRC);

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
    GEN_CHECK_SIZE(PITChannelState);
    GEN_CHECK_OFF(PITChannelState, pPitR3);
    GEN_CHECK_OFF(PITChannelState, pTimerR3);
    GEN_CHECK_OFF(PITChannelState, pPitR0);
    GEN_CHECK_OFF(PITChannelState, pTimerR0);
    GEN_CHECK_OFF(PITChannelState, pPitRC);
    GEN_CHECK_OFF(PITChannelState, pTimerRC);
    GEN_CHECK_OFF(PITChannelState, u64ReloadTS);
    GEN_CHECK_OFF(PITChannelState, u64NextTS);
    GEN_CHECK_OFF(PITChannelState, count_load_time);
    GEN_CHECK_OFF(PITChannelState, next_transition_time);
    GEN_CHECK_OFF(PITChannelState, irq);
    GEN_CHECK_OFF(PITChannelState, cRelLogEntries);
    GEN_CHECK_OFF(PITChannelState, count);
    GEN_CHECK_OFF(PITChannelState, latched_count);
    GEN_CHECK_OFF(PITChannelState, count_latched);
    GEN_CHECK_OFF(PITChannelState, status_latched);
    GEN_CHECK_OFF(PITChannelState, status);
    GEN_CHECK_OFF(PITChannelState, read_state);
    GEN_CHECK_OFF(PITChannelState, write_state);
    GEN_CHECK_OFF(PITChannelState, write_latch);
    GEN_CHECK_OFF(PITChannelState, rw_mode);
    GEN_CHECK_OFF(PITChannelState, mode);
    GEN_CHECK_OFF(PITChannelState, bcd);
    GEN_CHECK_OFF(PITChannelState, gate);
    GEN_CHECK_SIZE(PITState);
    GEN_CHECK_OFF(PITState, channels);
    GEN_CHECK_OFF(PITState, channels[1]);
    GEN_CHECK_OFF(PITState, speaker_data_on);
//    GEN_CHECK_OFF(PITState, dummy_refresh_clock);
    GEN_CHECK_OFF(PITState, pDevIns);
    GEN_CHECK_OFF(PITState, StatPITIrq);
    GEN_CHECK_OFF(PITState, StatPITHandler);

    /* PC/DevRTC.cpp */
    GEN_CHECK_SIZE(RTCState);
    GEN_CHECK_OFF(RTCState, cmos_data);
    GEN_CHECK_OFF(RTCState, cmos_data[1]);
    GEN_CHECK_OFF(RTCState, cmos_index);
    GEN_CHECK_OFF(RTCState, current_tm);
    GEN_CHECK_OFF(RTCState, current_tm.tm_sec);
    GEN_CHECK_OFF(RTCState, current_tm.tm_min);
    GEN_CHECK_OFF(RTCState, current_tm.tm_hour);
    GEN_CHECK_OFF(RTCState, current_tm.tm_mday);
    GEN_CHECK_OFF(RTCState, current_tm.tm_mon);
    GEN_CHECK_OFF(RTCState, current_tm.tm_year);
    GEN_CHECK_OFF(RTCState, current_tm.tm_wday);
    GEN_CHECK_OFF(RTCState, current_tm.tm_yday);
    GEN_CHECK_OFF(RTCState, irq);
    GEN_CHECK_OFF(RTCState, pPeriodicTimerR0);
    GEN_CHECK_OFF(RTCState, pPeriodicTimerR3);
    GEN_CHECK_OFF(RTCState, pPeriodicTimerRC);
    GEN_CHECK_OFF(RTCState, next_periodic_time);
    GEN_CHECK_OFF(RTCState, next_second_time);
    GEN_CHECK_OFF(RTCState, pSecondTimerR0);
    GEN_CHECK_OFF(RTCState, pSecondTimerR3);
    GEN_CHECK_OFF(RTCState, pSecondTimerRC);
    GEN_CHECK_OFF(RTCState, pSecondTimer2R0);
    GEN_CHECK_OFF(RTCState, pSecondTimer2R3);
    GEN_CHECK_OFF(RTCState, pSecondTimer2RC);
    GEN_CHECK_OFF(RTCState, pDevInsR0);
    GEN_CHECK_OFF(RTCState, pDevInsR3);
    GEN_CHECK_OFF(RTCState, pDevInsRC);
    GEN_CHECK_OFF(RTCState, fUTC);
    GEN_CHECK_OFF(RTCState, RtcReg);
    GEN_CHECK_OFF(RTCState, pRtcHlpR3);
    GEN_CHECK_OFF(RTCState, cRelLogEntries);
    GEN_CHECK_OFF(RTCState, CurPeriod);

    /* PC/apic.c */
    GEN_CHECK_SIZE(APICState);
    GEN_CHECK_OFF(APICState, apicbase);
    GEN_CHECK_OFF(APICState, id);
    GEN_CHECK_OFF(APICState, arb_id);
    GEN_CHECK_OFF(APICState, tpr);
    GEN_CHECK_OFF(APICState, spurious_vec);
    GEN_CHECK_OFF(APICState, log_dest);
    GEN_CHECK_OFF(APICState, dest_mode);
    GEN_CHECK_OFF(APICState, isr);
    GEN_CHECK_OFF(APICState, isr[1]);
    GEN_CHECK_OFF(APICState, tmr);
    GEN_CHECK_OFF(APICState, tmr[1]);
    GEN_CHECK_OFF(APICState, irr);
    GEN_CHECK_OFF(APICState, irr[1]);
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

    GEN_CHECK_SIZE(APICDeviceInfo);
    GEN_CHECK_OFF(APICDeviceInfo, pDevInsR3);
    GEN_CHECK_OFF(APICDeviceInfo, pApicHlpR3);
    GEN_CHECK_OFF(APICDeviceInfo, pDevInsR0);
    GEN_CHECK_OFF(APICDeviceInfo, pApicHlpR0);
    GEN_CHECK_OFF(APICDeviceInfo, pDevInsRC);
    GEN_CHECK_OFF(APICDeviceInfo, pApicHlpRC);
    GEN_CHECK_OFF(APICDeviceInfo, ulTPRPatchAttempts);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(APICDeviceInfo, StatMMIOReadGC);
    GEN_CHECK_OFF(APICDeviceInfo, StatMMIOWriteHC);
#endif

    GEN_CHECK_SIZE(IOAPICState);
    GEN_CHECK_OFF(IOAPICState, id);
    GEN_CHECK_OFF(IOAPICState, ioregsel);
    GEN_CHECK_OFF(IOAPICState, irr);
    GEN_CHECK_OFF(IOAPICState, ioredtbl);
    GEN_CHECK_OFF(IOAPICState, ioredtbl[1]);
    GEN_CHECK_OFF(IOAPICState, ioredtbl[IOAPIC_NUM_PINS - 1]);
    GEN_CHECK_OFF(IOAPICState, pDevInsR3);
    GEN_CHECK_OFF(IOAPICState, pIoApicHlpR3);
    GEN_CHECK_OFF(IOAPICState, pDevInsR0);
    GEN_CHECK_OFF(IOAPICState, pIoApicHlpR0);
    GEN_CHECK_OFF(IOAPICState, pDevInsRC);
    GEN_CHECK_OFF(IOAPICState, pIoApicHlpRC);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(IOAPICState, StatMMIOReadGC);
    GEN_CHECK_OFF(IOAPICState, StatSetIrqHC);
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
    GEN_CHECK_OFF(ATADevState, fATAPIPassthrough);
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
    GEN_CHECK_OFF(EHCI, SofTime);
    GEN_CHECK_OFF(EHCI, RootHub);
    GEN_CHECK_OFF(EHCI, intr_status);
    GEN_CHECK_OFF(EHCI, intr);
    GEN_CHECK_OFF(EHCI, HcFmNumber);
    GEN_CHECK_OFF(EHCI, cTicksPerFrame);
    GEN_CHECK_OFF(EHCI, cTicksPerUsbTick);
    GEN_CHECK_OFF(EHCI, cInFlight);
    GEN_CHECK_OFF(EHCI, aInFlight);
    GEN_CHECK_OFF(EHCI, aInFlight[0].GCPhysTD);
    GEN_CHECK_OFF(EHCI, aInFlight[0].pUrb);
    GEN_CHECK_OFF(EHCI, aInFlight[1]);
    GEN_CHECK_OFF(EHCI, pLoad);
    GEN_CHECK_OFF(EHCI, fAsyncTraversalTimerActive);
# ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(EHCI, StatCanceledIsocUrbs);
    GEN_CHECK_OFF(EHCI, StatCanceledGenUrbs);
    GEN_CHECK_OFF(EHCI, StatDroppedUrbs);
    GEN_CHECK_OFF(EHCI, StatTimer);
# endif
#endif /* VBOX_WITH_USB */

    /* VMMDev/VBoxDev.cpp */

    /* Parallel/DevParallel.cpp */
    GEN_CHECK_SIZE(ParallelState);
    GEN_CHECK_OFF(ParallelState, pDevInsR3);
    GEN_CHECK_OFF(ParallelState, pDevInsR0);
    GEN_CHECK_OFF(ParallelState, pDevInsRC);
    GEN_CHECK_OFF(ParallelState, IBase);
    GEN_CHECK_OFF(ParallelState, IHostParallelPort);
    GEN_CHECK_OFF(ParallelState, pDrvHostParallelConnector);
    GEN_CHECK_OFF(ParallelState, ReceiveSem);
    GEN_CHECK_OFF(ParallelState, reg_data);
    GEN_CHECK_OFF(ParallelState, reg_status);
    GEN_CHECK_OFF(ParallelState, reg_control);
    GEN_CHECK_OFF(ParallelState, reg_epp_addr);
    GEN_CHECK_OFF(ParallelState, reg_epp_data);
    GEN_CHECK_OFF(ParallelState, reg_ecp_ecr);
    GEN_CHECK_OFF(ParallelState, reg_ecp_base_plus_400h);
    GEN_CHECK_OFF(ParallelState, reg_ecp_config_b);
    GEN_CHECK_OFF(ParallelState, ecp_fifo);
    GEN_CHECK_OFF(ParallelState, ecp_fifo[1]);
    GEN_CHECK_OFF(ParallelState, act_fifo_pos_write);
    GEN_CHECK_OFF(ParallelState, act_fifo_pos_read);
    GEN_CHECK_OFF(ParallelState, irq);
    GEN_CHECK_OFF(ParallelState, epp_timeout);
    GEN_CHECK_OFF(ParallelState, fGCEnabled);
    GEN_CHECK_OFF(ParallelState, fR0Enabled);
    GEN_CHECK_OFF(ParallelState, base);

    /* Serial/DevSerial.cpp */
    GEN_CHECK_SIZE(SerialState);
    GEN_CHECK_OFF(SerialState, divider);
    GEN_CHECK_OFF(SerialState, rbr);
    GEN_CHECK_OFF(SerialState, ier);
    GEN_CHECK_OFF(SerialState, iir);
    GEN_CHECK_OFF(SerialState, lcr);
    GEN_CHECK_OFF(SerialState, mcr);
    GEN_CHECK_OFF(SerialState, lsr);
    GEN_CHECK_OFF(SerialState, msr);
    GEN_CHECK_OFF(SerialState, scr);
    GEN_CHECK_OFF(SerialState, thr_ipending);
    GEN_CHECK_OFF(SerialState, irq);
    GEN_CHECK_OFF(SerialState, msr_changed);
    GEN_CHECK_OFF(SerialState, fGCEnabled);
    GEN_CHECK_OFF(SerialState, fR0Enabled);
    GEN_CHECK_OFF(SerialState, pDevInsR3);
    GEN_CHECK_OFF(SerialState, pDevInsR0);
    GEN_CHECK_OFF(SerialState, pDevInsRC);
    GEN_CHECK_OFF(SerialState, IBase);
    GEN_CHECK_OFF(SerialState, ICharPort);
    GEN_CHECK_OFF(SerialState, pDrvBase);
    GEN_CHECK_OFF(SerialState, pDrvChar);
    GEN_CHECK_OFF(SerialState, CritSect);
    GEN_CHECK_OFF(SerialState, ReceiveSem);
    GEN_CHECK_OFF(SerialState, last_break_enable);
    GEN_CHECK_OFF(SerialState, base);

#ifdef VBOX_WITH_AHCI
    /* Storage/ATAController.h */
    GEN_CHECK_SIZE(AHCIATADevState);
    GEN_CHECK_OFF(AHCIATADevState, fLBA48);
    GEN_CHECK_OFF(AHCIATADevState, fATAPI);
    GEN_CHECK_OFF(AHCIATADevState, fIrqPending);
    GEN_CHECK_OFF(AHCIATADevState, cMultSectors);
    GEN_CHECK_OFF(AHCIATADevState, PCHSGeometry.cCylinders);
    GEN_CHECK_OFF(AHCIATADevState, PCHSGeometry.cHeads);
    GEN_CHECK_OFF(AHCIATADevState, PCHSGeometry.cSectors);
    GEN_CHECK_OFF(AHCIATADevState, cSectorsPerIRQ);
    GEN_CHECK_OFF(AHCIATADevState, cTotalSectors);
    GEN_CHECK_OFF(AHCIATADevState, uATARegFeature);
    GEN_CHECK_OFF(AHCIATADevState, uATARegFeatureHOB);
    GEN_CHECK_OFF(AHCIATADevState, uATARegError);
    GEN_CHECK_OFF(AHCIATADevState, uATARegNSector);
    GEN_CHECK_OFF(AHCIATADevState, uATARegNSectorHOB);
    GEN_CHECK_OFF(AHCIATADevState, uATARegSector);
    GEN_CHECK_OFF(AHCIATADevState, uATARegSectorHOB);
    GEN_CHECK_OFF(AHCIATADevState, uATARegLCyl);
    GEN_CHECK_OFF(AHCIATADevState, uATARegLCylHOB);
    GEN_CHECK_OFF(AHCIATADevState, uATARegHCyl);
    GEN_CHECK_OFF(AHCIATADevState, uATARegHCylHOB);
    GEN_CHECK_OFF(AHCIATADevState, uATARegSelect);
    GEN_CHECK_OFF(AHCIATADevState, uATARegStatus);
    GEN_CHECK_OFF(AHCIATADevState, uATARegCommand);
    GEN_CHECK_OFF(AHCIATADevState, uATARegDevCtl);
    GEN_CHECK_OFF(AHCIATADevState, uATATransferMode);
    GEN_CHECK_OFF(AHCIATADevState, uTxDir);
    GEN_CHECK_OFF(AHCIATADevState, iBeginTransfer);
    GEN_CHECK_OFF(AHCIATADevState, iSourceSink);
    GEN_CHECK_OFF(AHCIATADevState, fDMA);
    GEN_CHECK_OFF(AHCIATADevState, fATAPITransfer);
    GEN_CHECK_OFF(AHCIATADevState, cbTotalTransfer);
    GEN_CHECK_OFF(AHCIATADevState, cbElementaryTransfer);
    GEN_CHECK_OFF(AHCIATADevState, iIOBufferCur);
    GEN_CHECK_OFF(AHCIATADevState, iIOBufferEnd);
    GEN_CHECK_OFF(AHCIATADevState, iIOBufferPIODataStart);
    GEN_CHECK_OFF(AHCIATADevState, iIOBufferPIODataEnd);
    GEN_CHECK_OFF(AHCIATADevState, iATAPILBA);
    GEN_CHECK_OFF(AHCIATADevState, cbATAPISector);
    GEN_CHECK_OFF(AHCIATADevState, aATAPICmd);
    GEN_CHECK_OFF(AHCIATADevState, aATAPICmd[ATAPI_PACKET_SIZE - 1]);
    GEN_CHECK_OFF(AHCIATADevState, abATAPISense);
    GEN_CHECK_OFF(AHCIATADevState, abATAPISense[ATAPI_SENSE_SIZE - 1]);
    GEN_CHECK_OFF(AHCIATADevState, cNotifiedMediaChange);
    GEN_CHECK_OFF(AHCIATADevState, MediaEventStatus);
    GEN_CHECK_OFF(AHCIATADevState, pLed);
    GEN_CHECK_OFF(AHCIATADevState, cbIOBuffer);
    GEN_CHECK_OFF(AHCIATADevState, pbIOBufferR3);
    GEN_CHECK_OFF(AHCIATADevState, pbIOBufferR0);
    GEN_CHECK_OFF(AHCIATADevState, pbIOBufferRC);
    GEN_CHECK_OFF(AHCIATADevState, StatATADMA);
    GEN_CHECK_OFF(AHCIATADevState, StatATAPIO);
    GEN_CHECK_OFF(AHCIATADevState, StatATAPIDMA);
    GEN_CHECK_OFF(AHCIATADevState, StatATAPIPIO);
    GEN_CHECK_OFF(AHCIATADevState, StatReads);
    GEN_CHECK_OFF(AHCIATADevState, pStatBytesRead);
    GEN_CHECK_OFF(AHCIATADevState, StatWrites);
    GEN_CHECK_OFF(AHCIATADevState, pStatBytesWritten);
    GEN_CHECK_OFF(AHCIATADevState, StatFlushes);
    GEN_CHECK_OFF(AHCIATADevState, fATAPIPassthrough);
    GEN_CHECK_OFF(AHCIATADevState, cErrors);
    GEN_CHECK_OFF(AHCIATADevState, pDrvBase);
    GEN_CHECK_OFF(AHCIATADevState, pDrvBlock);
    GEN_CHECK_OFF(AHCIATADevState, pDrvBlockBios);
    GEN_CHECK_OFF(AHCIATADevState, pDrvMount);
    GEN_CHECK_OFF(AHCIATADevState, IBase);
    GEN_CHECK_OFF(AHCIATADevState, IPort);
    GEN_CHECK_OFF(AHCIATADevState, IMountNotify);
    GEN_CHECK_OFF(AHCIATADevState, iLUN);
    GEN_CHECK_OFF(AHCIATADevState, pDevInsR3);
    GEN_CHECK_OFF(AHCIATADevState, pDevInsR0);
    GEN_CHECK_OFF(AHCIATADevState, pDevInsRC);
    GEN_CHECK_OFF(AHCIATADevState, pControllerR3);
    GEN_CHECK_OFF(AHCIATADevState, pControllerR0);
    GEN_CHECK_OFF(AHCIATADevState, pControllerRC);
    GEN_CHECK_SIZE(AHCIATATransferRequest);
    GEN_CHECK_OFF(AHCIATATransferRequest, iIf);
    GEN_CHECK_OFF(AHCIATATransferRequest, iBeginTransfer);
    GEN_CHECK_OFF(AHCIATATransferRequest, iSourceSink);
    GEN_CHECK_OFF(AHCIATATransferRequest, cbTotalTransfer);
    GEN_CHECK_OFF(AHCIATATransferRequest, uTxDir);
    GEN_CHECK_SIZE(AHCIATAAbortRequest);
    GEN_CHECK_OFF(AHCIATAAbortRequest, iIf);
    GEN_CHECK_OFF(AHCIATAAbortRequest, fResetDrive);
    GEN_CHECK_SIZE(AHCIATARequest);
    GEN_CHECK_OFF(AHCIATARequest, ReqType);
    GEN_CHECK_OFF(AHCIATARequest, u);
    GEN_CHECK_OFF(AHCIATARequest, u.t);
    GEN_CHECK_OFF(AHCIATARequest, u.a);
    GEN_CHECK_SIZE(AHCIATACONTROLLER);
    GEN_CHECK_OFF(AHCIATACONTROLLER, IOPortBase1);
    GEN_CHECK_OFF(AHCIATACONTROLLER, IOPortBase2);
    GEN_CHECK_OFF(AHCIATACONTROLLER, irq);
    GEN_CHECK_OFF(AHCIATACONTROLLER, lock);
    GEN_CHECK_OFF(AHCIATACONTROLLER, iSelectedIf);
    GEN_CHECK_OFF(AHCIATACONTROLLER, iAIOIf);
    GEN_CHECK_OFF(AHCIATACONTROLLER, uAsyncIOState);
    GEN_CHECK_OFF(AHCIATACONTROLLER, fChainedTransfer);
    GEN_CHECK_OFF(AHCIATACONTROLLER, fReset);
    GEN_CHECK_OFF(AHCIATACONTROLLER, fRedo);
    GEN_CHECK_OFF(AHCIATACONTROLLER, fRedoIdle);
    GEN_CHECK_OFF(AHCIATACONTROLLER, fRedoDMALastDesc);
    GEN_CHECK_OFF(AHCIATACONTROLLER, BmDma);
    GEN_CHECK_OFF(AHCIATACONTROLLER, pFirstDMADesc);
    GEN_CHECK_OFF(AHCIATACONTROLLER, pLastDMADesc);
    GEN_CHECK_OFF(AHCIATACONTROLLER, pRedoDMABuffer);
    GEN_CHECK_OFF(AHCIATACONTROLLER, cbRedoDMABuffer);
    GEN_CHECK_OFF(AHCIATACONTROLLER, aIfs);
    GEN_CHECK_OFF(AHCIATACONTROLLER, aIfs[1]);
    GEN_CHECK_OFF(AHCIATACONTROLLER, pDevInsR3);
    GEN_CHECK_OFF(AHCIATACONTROLLER, pDevInsR0);
    GEN_CHECK_OFF(AHCIATACONTROLLER, pDevInsRC);
    GEN_CHECK_OFF(AHCIATACONTROLLER, fShutdown);
    GEN_CHECK_OFF(AHCIATACONTROLLER, AsyncIOThread);
    GEN_CHECK_OFF(AHCIATACONTROLLER, AsyncIOSem);
    GEN_CHECK_OFF(AHCIATACONTROLLER, aAsyncIORequests[4]);
    GEN_CHECK_OFF(AHCIATACONTROLLER, AsyncIOReqHead);
    GEN_CHECK_OFF(AHCIATACONTROLLER, AsyncIOReqTail);
    GEN_CHECK_OFF(AHCIATACONTROLLER, AsyncIORequestMutex);
    GEN_CHECK_OFF(AHCIATACONTROLLER, SuspendIOSem);
    GEN_CHECK_OFF(AHCIATACONTROLLER, DelayIRQMillies);
    GEN_CHECK_OFF(AHCIATACONTROLLER, StatAsyncOps);
    GEN_CHECK_OFF(AHCIATACONTROLLER, StatAsyncMinWait);
    GEN_CHECK_OFF(AHCIATACONTROLLER, StatAsyncMaxWait);
    GEN_CHECK_OFF(AHCIATACONTROLLER, StatAsyncTimeUS);
    GEN_CHECK_OFF(AHCIATACONTROLLER, StatAsyncTime);
    GEN_CHECK_OFF(AHCIATACONTROLLER, StatLockWait);

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
    GEN_CHECK_OFF(AHCIPort, fAsyncInterface);
    GEN_CHECK_OFF(AHCIPort, pAsyncIOThread);
    GEN_CHECK_OFF(AHCIPort, AsyncIORequestSem);
    GEN_CHECK_OFF(AHCIPort, ahciIOTasks);
    GEN_CHECK_OFF(AHCIPort, ahciIOTasks[2*AHCI_NR_COMMAND_SLOTS-1]);
    GEN_CHECK_OFF(AHCIPort, uActWritePos);
    GEN_CHECK_OFF(AHCIPort, uActReadPos);
    GEN_CHECK_OFF(AHCIPort, uActTasksActive);
    GEN_CHECK_OFF(AHCIPort, fPoweredOn);
    GEN_CHECK_OFF(AHCIPort, fSpunUp);
    GEN_CHECK_OFF(AHCIPort, fFirstD2HFisSend);
    GEN_CHECK_OFF(AHCIPort, fATAPI);
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
    GEN_CHECK_OFF(AHCIPort, cTotalSectors);
    GEN_CHECK_OFF(AHCIPort, cMultSectors);
    GEN_CHECK_OFF(AHCIPort, uATATransferMode);
    GEN_CHECK_OFF(AHCIPort, uATAPISenseKey);
    GEN_CHECK_OFF(AHCIPort, uATAPIASC);
    GEN_CHECK_OFF(AHCIPort, cNotifiedMediaChange);
    GEN_CHECK_OFF(AHCIPort, iLUN);
    GEN_CHECK_OFF(AHCIPort, fResetDevice);
    GEN_CHECK_OFF(AHCIPort, u32TasksFinished);
    GEN_CHECK_OFF(AHCIPort, u32QueuedTasksFinished);
    GEN_CHECK_OFF(AHCIPort, StatDMA);
    GEN_CHECK_OFF(AHCIPort, StatBytesWritten);
    GEN_CHECK_OFF(AHCIPort, StatBytesRead);
    GEN_CHECK_OFF(AHCIPort, StatIORequestsPerSecond);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(AHCIPort, StatProfileProcessTime);
    GEN_CHECK_OFF(AHCIPort, StatProfileMapIntoR3);
    GEN_CHECK_OFF(AHCIPort, StatProfileReadWrite);
    GEN_CHECK_OFF(AHCIPort, StatProfileDestroyScatterGatherList);
#endif
    GEN_CHECK_OFF(AHCIPort, fNotificationSend);
    GEN_CHECK_OFF(AHCIPort, fPortReset);
    GEN_CHECK_OFF(AHCIPort, fAsyncIOThreadIdle);
    GEN_CHECK_OFF(AHCIPort, szSerialNumber);
    GEN_CHECK_OFF(AHCIPort, szSerialNumber[AHCI_SERIAL_NUMBER_LENGTH]); /* One additional byte for the termination.*/
    GEN_CHECK_OFF(AHCIPort, szFirmwareRevision);
    GEN_CHECK_OFF(AHCIPort, szFirmwareRevision[AHCI_FIRMWARE_REVISION_LENGTH]); /* One additional byte for the termination.*/
    GEN_CHECK_OFF(AHCIPort, szModelNumber);
    GEN_CHECK_OFF(AHCIPort, szModelNumber[AHCI_MODEL_NUMBER_LENGTH]); /* One additional byte for the termination.*/

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
    GEN_CHECK_OFF(AHCI, aCts);
    GEN_CHECK_OFF(AHCI, aCts[1]);
    GEN_CHECK_OFF(AHCI, u32PortsInterrupted);
    GEN_CHECK_OFF(AHCI, fReset);
    GEN_CHECK_OFF(AHCI, f64BitAddr);
    GEN_CHECK_OFF(AHCI, fGCEnabled);
    GEN_CHECK_OFF(AHCI, fR0Enabled);
    GEN_CHECK_OFF(AHCI, lock);
    GEN_CHECK_OFF(AHCI, cPortsImpl);
    GEN_CHECK_OFF(AHCI, f8ByteMMIO4BytesWrittenSuccessfully);
    GEN_CHECK_OFF(AHCI, cHighIOThreshold);
    GEN_CHECK_OFF(AHCI, cMillisToSleep);
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
    GEN_CHECK_OFF(E1KSTATE, INetworkPort);
    GEN_CHECK_OFF(E1KSTATE, INetworkConfig);
    GEN_CHECK_OFF(E1KSTATE, ILeds);
    GEN_CHECK_OFF(E1KSTATE, pDrvBase);
    GEN_CHECK_OFF(E1KSTATE, pDrv);
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
    GEN_CHECK_OFF(E1KSTATE, pLUTimer);
    GEN_CHECK_OFF(E1KSTATE, pTxThread);
    GEN_CHECK_OFF(E1KSTATE, cs);
#ifndef E1K_GLOBAL_MUTEX
    GEN_CHECK_OFF(E1KSTATE, csRx);
#endif
    GEN_CHECK_OFF(E1KSTATE, hTxSem);
    GEN_CHECK_OFF(E1KSTATE, addrMMReg);
    GEN_CHECK_OFF(E1KSTATE, macAddress);
    GEN_CHECK_OFF(E1KSTATE, addrIOPort);
    GEN_CHECK_OFF(E1KSTATE, pciDevice);
    GEN_CHECK_OFF(E1KSTATE, u64AckedAt);
    GEN_CHECK_OFF(E1KSTATE, fIntRaised);
    GEN_CHECK_OFF(E1KSTATE, fCableConnected);
    GEN_CHECK_OFF(E1KSTATE, fR0Enabled);
    GEN_CHECK_OFF(E1KSTATE, fGCEnabled);
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
    GEN_CHECK_OFF(E1KSTATE, aTxPacket[E1K_MAX_TX_PKT_SIZE]);
    GEN_CHECK_OFF(E1KSTATE, u16TxPktLen);
    GEN_CHECK_OFF(E1KSTATE, fIPcsum);
    GEN_CHECK_OFF(E1KSTATE, fTCPcsum);
    GEN_CHECK_OFF(E1KSTATE, u32PayRemain);
    GEN_CHECK_OFF(E1KSTATE, u16HdrRemain);
    GEN_CHECK_OFF(E1KSTATE, u16SavedFlags);
    GEN_CHECK_OFF(E1KSTATE, u32SavedCsum);
    GEN_CHECK_OFF(E1KSTATE, eeprom);
    GEN_CHECK_OFF(E1KSTATE, phy);
#endif /* VBOX_WITH_E1000 */

#ifdef VBOX_WITH_SCSI
    GEN_CHECK_SIZE(VBOXSCSI);
    GEN_CHECK_OFF(VBOXSCSI, regIdentify);
    GEN_CHECK_OFF(VBOXSCSI, uTargetDevice);
    GEN_CHECK_OFF(VBOXSCSI, uTxDir);
    GEN_CHECK_OFF(VBOXSCSI, cbCDB);
    GEN_CHECK_OFF(VBOXSCSI, aCDB);
    GEN_CHECK_OFF(VBOXSCSI, aCDB[11]);
    GEN_CHECK_OFF(VBOXSCSI, iCDB);
    GEN_CHECK_OFF(VBOXSCSI, pBuf);
    GEN_CHECK_OFF(VBOXSCSI, cbBuf);
    GEN_CHECK_OFF(VBOXSCSI, iBuf);
    GEN_CHECK_OFF(VBOXSCSI, fBusy);
    GEN_CHECK_OFF(VBOXSCSI, enmState);
#endif

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
    GEN_CHECK_OFF(BUSLOGIC, pTaskCache);
    GEN_CHECK_OFF(BUSLOGIC, VBoxSCSI);
    GEN_CHECK_OFF(BUSLOGIC, aDeviceStates);
    GEN_CHECK_OFF(BUSLOGIC, aDeviceStates[BUSLOGIC_MAX_DEVICES-1]);
    GEN_CHECK_OFF(BUSLOGIC, IBase);
    GEN_CHECK_OFF(BUSLOGIC, ILeds);
    GEN_CHECK_OFF(BUSLOGIC, pLedsConnector);
#endif /* VBOX_WITH_BUSLOGIC */

#ifdef VBOX_WITH_LSILOGIC
    GEN_CHECK_SIZE(LSILOGICDEVICE);
    GEN_CHECK_OFF(LSILOGICDEVICE, pLsiLogicR3);
    GEN_CHECK_OFF(LSILOGICDEVICE, pLsiLogicR0);
    GEN_CHECK_OFF(LSILOGICDEVICE, pLsiLogicRC);
    GEN_CHECK_OFF(LSILOGICDEVICE, iLUN);
    GEN_CHECK_OFF(LSILOGICDEVICE, cOutstandingRequests);
    GEN_CHECK_OFF(LSILOGICDEVICE, IBase);
    GEN_CHECK_OFF(LSILOGICDEVICE, ISCSIPort);
    GEN_CHECK_OFF(LSILOGICDEVICE, ILed);
    GEN_CHECK_OFF(LSILOGICDEVICE, pDrvBase);
    GEN_CHECK_OFF(LSILOGICDEVICE, pDrvSCSIConnector);
    GEN_CHECK_OFF(LSILOGICDEVICE, Led);

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
    GEN_CHECK_OFF(LSILOGICSCSI, aDeviceStates);
    GEN_CHECK_OFF(LSILOGICSCSI, aDeviceStates[LSILOGIC_DEVICES_MAX-1]);
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
    GEN_CHECK_OFF(LSILOGICSCSI, ConfigurationPages);
    GEN_CHECK_OFF(LSILOGICSCSI, VBoxSCSI);
    GEN_CHECK_OFF(LSILOGICSCSI, pTaskCache);
    GEN_CHECK_OFF(LSILOGICSCSI, IBase);
    GEN_CHECK_OFF(LSILOGICSCSI, ILeds);
    GEN_CHECK_OFF(LSILOGICSCSI, pLedsConnector);
#endif /* VBOX_WITH_LSILOGIC */

#ifdef VBOX_WITH_HPET
    GEN_CHECK_SIZE(HpetState);
    GEN_CHECK_OFF(HpetState, pDevInsR3);
    GEN_CHECK_OFF(HpetState, pDevInsR0);
    GEN_CHECK_OFF(HpetState, pDevInsRC);
    GEN_CHECK_OFF(HpetState, u64HpetOffset);
    GEN_CHECK_OFF(HpetState, u64Capabilities);
    GEN_CHECK_OFF(HpetState, u64Config);
    GEN_CHECK_OFF(HpetState, u64Isr);
    GEN_CHECK_OFF(HpetState, u64HpetCounter);

    GEN_CHECK_SIZE(HpetTimer);
    GEN_CHECK_OFF(HpetTimer, pTimerR3);
    GEN_CHECK_OFF(HpetTimer, pHpetR3);
    GEN_CHECK_OFF(HpetTimer, pTimerR0);
    GEN_CHECK_OFF(HpetTimer, pHpetR0);
    GEN_CHECK_OFF(HpetTimer, pTimerRC);
    GEN_CHECK_OFF(HpetTimer, pHpetRC);
    GEN_CHECK_OFF(HpetTimer, u8TimerNumber);
    GEN_CHECK_OFF(HpetTimer, u64Config);
    GEN_CHECK_OFF(HpetTimer, u64Cmp);
    GEN_CHECK_OFF(HpetTimer, u64Fsb);
    GEN_CHECK_OFF(HpetTimer, u64Period);
    GEN_CHECK_OFF(HpetTimer, u8Wrap);
#endif

    return (0);
}
