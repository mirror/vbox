/* $Id$ */
/** @file
 * tstDeviceStructSizeGC - Generate structure member and size checks from the GC perspective.
 *
 * This is built using the VBOXGC template but linked into a host
 * ring-3 executable, rather hacky.
 */

/*
 * Copyright (C) 2006-2008 innotek GmbH
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
#ifndef IN_GC
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
#include "Bus/DevPCI.cpp" /* must be first! */
#undef LOG_GROUP
#include "Graphics/DevVGA.cpp"
#undef LOG_GROUP
#include "Input/DevPS2.cpp"
#undef LOG_GROUP
#include "Network/DevPCNet.cpp"
//#undef LOG_GROUP
//#include "Network/ne2000.c"
#undef LOG_GROUP
#include "PC/DevACPI.cpp"
#undef LOG_GROUP
#include "PC/DevPIC.cpp"
#undef LOG_GROUP
#include "PC/DevPit-i8254.cpp"
#undef LOG_GROUP
#include "PC/DevRTC.cpp"
#undef LOG_GROUP
#include "PC/DevAPIC.cpp"
#undef LOG_GROUP
#include "Storage/DevATA.cpp"
#ifdef VBOX_WITH_USB
# undef LOG_GROUP
# include "USB/DevOHCI.cpp"
# include "USB/DevEHCI.cpp"
#endif
#undef LOG_GROUP
#include "VMMDev/VBoxDev.cpp"
#undef LOG_GROUP
#include "Serial/DevSerial.cpp"
#ifdef VBOX_WITH_AHCI
#undef LOG_GROUP
#include "Storage/DevAHCI.cpp"
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
    GEN_CHECK_OFF(PDMDEVINS, pDevHlp);
    GEN_CHECK_OFF(PDMDEVINS, pDevHlpGC);
    GEN_CHECK_OFF(PDMDEVINS, pDevHlpR0);
    GEN_CHECK_OFF(PDMDEVINS, pDevReg);
    GEN_CHECK_OFF(PDMDEVINS, pCfgHandle);
    GEN_CHECK_OFF(PDMDEVINS, iInstance);
    GEN_CHECK_OFF(PDMDEVINS, pvInstanceDataR3);
    GEN_CHECK_OFF(PDMDEVINS, pvInstanceDataGC);
    GEN_CHECK_OFF(PDMDEVINS, pvInstanceDataR0);
    GEN_CHECK_OFF(PDMDEVINS, IBase);
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
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pBus);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pfnConfigRead);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.pfnConfigWrite);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.iIrq);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.fRequestedDevFn);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[1]);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[PCI_NUM_REGIONS - 1]);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[0].addr);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[0].size);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[0].type);
    GEN_CHECK_OFF(PCIDEVICE, Int.s.aIORegions[0].padding);
    GEN_CHECK_PADDING(PCIDEVICE, Int);
    GEN_CHECK_SIZE(PIIX3State);
    GEN_CHECK_SIZE(PCIBUS);
    GEN_CHECK_OFF(PCIBUS, uIrqIndex);
    GEN_CHECK_OFF(PCIBUS, iBus);
    GEN_CHECK_OFF(PCIBUS, iDevSearch);
    GEN_CHECK_OFF(PCIBUS, uConfigReg);
    GEN_CHECK_OFF(PCIBUS, devices);
    GEN_CHECK_OFF(PCIBUS, devices[1]);
    GEN_CHECK_OFF(PCIBUS, pDevInsHC);
    GEN_CHECK_OFF(PCIBUS, pPciHlpR3);
    GEN_CHECK_OFF(PCIBUS, pDevInsGC);
    GEN_CHECK_OFF(PCIBUS, pPciHlpGC);
    GEN_CHECK_OFF(PCIBUS, pPciHlpR0);
    GEN_CHECK_OFF(PCIBUS, PciDev);
    GEN_CHECK_OFF(PCIBUS, PIIX3State);
    GEN_CHECK_OFF(PCIBUS, Globals);
    GEN_CHECK_SIZE(PCIGLOBALS);
    GEN_CHECK_OFF(PCIGLOBALS, pci_mem_base);
    GEN_CHECK_OFF(PCIGLOBALS, pci_bios_io_addr);
    GEN_CHECK_OFF(PCIGLOBALS, pci_bios_mem_addr);
    GEN_CHECK_OFF(PCIGLOBALS, pci_irq_levels);
    GEN_CHECK_OFF(PCIGLOBALS, pci_irq_levels[1][1]);
    GEN_CHECK_OFF(PCIGLOBALS, fUseIoApic);
    GEN_CHECK_OFF(PCIGLOBALS, pci_apic_irq_levels);
    GEN_CHECK_OFF(PCIGLOBALS, pci_apic_irq_levels[1][1]);
    GEN_CHECK_OFF(PCIGLOBALS, acpi_irq_level);
    GEN_CHECK_OFF(PCIGLOBALS, acpi_irq);

    /* DevVGA.cpp */
    GEN_CHECK_SIZE(VGASTATE);
    GEN_CHECK_OFF(VGASTATE, vram_ptrHC);
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
    GEN_CHECK_OFF(VGASTATE, vbe_bank_mask);
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
    GEN_CHECK_OFF(VGASTATE, vram_ptrGC);
    GEN_CHECK_OFF(VGASTATE, fLFBUpdated);
    GEN_CHECK_OFF(VGASTATE, fGCEnabled);
    GEN_CHECK_OFF(VGASTATE, fR0Enabled);
    GEN_CHECK_OFF(VGASTATE, GCPtrLFBHandler);
    GEN_CHECK_OFF(VGASTATE, fHaveDirtyBits);
    GEN_CHECK_OFF(VGASTATE, au32DirtyBitmap);
    GEN_CHECK_OFF(VGASTATE, au32DirtyBitmap[1]);
    GEN_CHECK_OFF(VGASTATE, au32DirtyBitmap[(VGA_VRAM_MAX / PAGE_SIZE / 32) - 1]);
    GEN_CHECK_OFF(VGASTATE, pDevInsHC);
    GEN_CHECK_OFF(VGASTATE, Base);
    GEN_CHECK_OFF(VGASTATE, Port);
    GEN_CHECK_OFF(VGASTATE, pDrvBase);
    GEN_CHECK_OFF(VGASTATE, pDrv);
    GEN_CHECK_OFF(VGASTATE, RefreshTimer);
    GEN_CHECK_OFF(VGASTATE, cMilliesRefreshInterval);
    GEN_CHECK_OFF(VGASTATE, fRenderVRAM);
    GEN_CHECK_OFF(VGASTATE, Dev);
    GEN_CHECK_OFF(VGASTATE, StatGCMemoryRead);
    GEN_CHECK_OFF(VGASTATE, StatGCMemoryWrite);
    GEN_CHECK_OFF(VGASTATE, StatGCIOPortRead);
    GEN_CHECK_OFF(VGASTATE, StatGCIOPortWrite);
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
    GEN_CHECK_OFF(KBDState, pDevInsHC);
    GEN_CHECK_OFF(KBDState, pDevInsGC);
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
    GEN_CHECK_OFF(PCNetState, pTimerPollHC);
    GEN_CHECK_OFF(PCNetState, pTimerPollGC);
#endif
    GEN_CHECK_OFF(PCNetState, pTimerSoftIntHC);
    GEN_CHECK_OFF(PCNetState, pTimerSoftIntGC);
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
    GEN_CHECK_OFF(PCNetState, pXmitQueueHC);
    GEN_CHECK_OFF(PCNetState, pXmitQueueGC);
    GEN_CHECK_OFF(PCNetState, pCanRxQueueHC);
    GEN_CHECK_OFF(PCNetState, pCanRxQueueGC);
    GEN_CHECK_OFF(PCNetState, pTimerRestore);
    GEN_CHECK_OFF(PCNetState, pDevInsHC);
    GEN_CHECK_OFF(PCNetState, pDevInsGC);
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
    GEN_CHECK_OFF(PCNetState, fGCEnabled);
    GEN_CHECK_OFF(PCNetState, fR0Enabled);
    GEN_CHECK_OFF(PCNetState, fAm79C973);
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
    GEN_CHECK_OFF(ACPIState, tsHC);
    GEN_CHECK_OFF(ACPIState, tsGC);
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
    GEN_CHECK_OFF(PicState, pDevInsHC);
    GEN_CHECK_OFF(PicState, pDevInsGC);

    GEN_CHECK_SIZE(DEVPIC);
    GEN_CHECK_OFF(DEVPIC, aPics);
    GEN_CHECK_OFF(DEVPIC, aPics[1]);
    GEN_CHECK_OFF(DEVPIC, pDevInsHC);
    GEN_CHECK_OFF(DEVPIC, pDevInsGC);
    GEN_CHECK_OFF(DEVPIC, pPicHlpR3);
    GEN_CHECK_OFF(DEVPIC, pPicHlpGC);
    GEN_CHECK_OFF(DEVPIC, pPicHlpR0);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(DEVPIC, StatSetIrqGC);
    GEN_CHECK_OFF(DEVPIC, StatClearedActiveSlaveIRQ);
#endif

    /* PC/DevPit-i8254.cpp */
    GEN_CHECK_SIZE(PITChannelState);
    GEN_CHECK_OFF(PITChannelState, pPitHC);
    GEN_CHECK_OFF(PITChannelState, pTimerHC);
    GEN_CHECK_OFF(PITChannelState, pPitGC);
    GEN_CHECK_OFF(PITChannelState, pTimerGC);
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
    GEN_CHECK_OFF(RTCState, pPeriodicTimerHC);
    GEN_CHECK_OFF(RTCState, pPeriodicTimerGC);
    GEN_CHECK_OFF(RTCState, next_periodic_time);
    GEN_CHECK_OFF(RTCState, next_second_time);
    GEN_CHECK_OFF(RTCState, pSecondTimerHC);
    GEN_CHECK_OFF(RTCState, pSecondTimerGC);
    GEN_CHECK_OFF(RTCState, pSecondTimer2HC);
    GEN_CHECK_OFF(RTCState, pSecondTimer2GC);
    GEN_CHECK_OFF(RTCState, pDevInsHC);
    GEN_CHECK_OFF(RTCState, pDevInsGC);
    GEN_CHECK_OFF(RTCState, fUTC);
    GEN_CHECK_OFF(RTCState, RtcReg);
    GEN_CHECK_OFF(RTCState, pRtcHlpHC);

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
    GEN_CHECK_OFF(APICState, pDevInsHC);
    GEN_CHECK_OFF(APICState, pApicHlpR3);
    GEN_CHECK_OFF(APICState, pTimerHC);
    GEN_CHECK_OFF(APICState, pDevInsGC);
    GEN_CHECK_OFF(APICState, pApicHlpGC);
    GEN_CHECK_OFF(APICState, pTimerGC);
    GEN_CHECK_OFF(APICState, pApicHlpR0);
    GEN_CHECK_OFF(APICState, ulTPRPatchAttempts);
#ifdef VBOX_WITH_STATISTICS
    GEN_CHECK_OFF(APICState, StatMMIOReadGC);
    GEN_CHECK_OFF(APICState, StatMMIOWriteHC);
#endif

    GEN_CHECK_SIZE(IOAPICState);
    GEN_CHECK_OFF(IOAPICState, id);
    GEN_CHECK_OFF(IOAPICState, ioregsel);
    GEN_CHECK_OFF(IOAPICState, irr);
    GEN_CHECK_OFF(IOAPICState, ioredtbl);
    GEN_CHECK_OFF(IOAPICState, ioredtbl[1]);
    GEN_CHECK_OFF(IOAPICState, ioredtbl[IOAPIC_NUM_PINS - 1]);
    GEN_CHECK_OFF(IOAPICState, pDevInsHC);
    GEN_CHECK_OFF(IOAPICState, pIoApicHlpR3);
    GEN_CHECK_OFF(IOAPICState, pDevInsGC);
    GEN_CHECK_OFF(IOAPICState, pIoApicHlpGC);
    GEN_CHECK_OFF(IOAPICState, pIoApicHlpR0);
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
    GEN_CHECK_OFF(ATADevState, uATAPISenseKey);
    GEN_CHECK_OFF(ATADevState, uATAPIASC);
    GEN_CHECK_OFF(ATADevState, cNotifiedMediaChange);
    GEN_CHECK_OFF(ATADevState, Led);
    GEN_CHECK_OFF(ATADevState, cbIOBuffer);
    GEN_CHECK_OFF(ATADevState, pbIOBufferHC);
    GEN_CHECK_OFF(ATADevState, pbIOBufferGC);
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
    GEN_CHECK_OFF(ATADevState, pDevInsHC);
    GEN_CHECK_OFF(ATADevState, pDevInsGC);
    GEN_CHECK_OFF(ATADevState, pControllerHC);
    GEN_CHECK_OFF(ATADevState, pControllerGC);
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
    GEN_CHECK_OFF(ATACONTROLLER, pDevInsHC);
    GEN_CHECK_OFF(ATACONTROLLER, pDevInsGC);
    GEN_CHECK_OFF(ATACONTROLLER, fShutdown);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIOThread);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIOSem);
    GEN_CHECK_OFF(ATACONTROLLER, aAsyncIORequests[4]);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIOReqHead);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIOReqTail);
    GEN_CHECK_OFF(ATACONTROLLER, AsyncIORequestMutex);
    GEN_CHECK_OFF(ATACONTROLLER, SuspendIOSem);
    GEN_CHECK_OFF(ATACONTROLLER, DelayIRQMillies);
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
    GEN_CHECK_OFF(OHCI, pEndOfFrameTimerHC);
    GEN_CHECK_OFF(OHCI, pEndOfFrameTimerGC);
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
    GEN_CHECK_OFF(OHCI, pDevInsHC);
    GEN_CHECK_OFF(OHCI, pDevInsGC);
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
    GEN_CHECK_OFF(EHCI, pEndOfFrameTimerHC);
    GEN_CHECK_OFF(EHCI, pEndOfFrameTimerGC);
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
    GEN_CHECK_OFF(EHCI, pDevInsHC);
    GEN_CHECK_OFF(EHCI, pDevInsGC);
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
    GEN_CHECK_OFF(SerialState, pDevInsGC);
    GEN_CHECK_OFF(SerialState, pDevInsHC);
    GEN_CHECK_OFF(SerialState, IBase);
    GEN_CHECK_OFF(SerialState, ICharPort);
    GEN_CHECK_OFF(SerialState, pDrvBase);
    GEN_CHECK_OFF(SerialState, pDrvChar);
    GEN_CHECK_OFF(SerialState, CritSect);
    GEN_CHECK_OFF(SerialState, ReceiveSem);
    GEN_CHECK_OFF(SerialState, last_break_enable);
    GEN_CHECK_OFF(SerialState, base);

#ifdef VBOX_WITH_AHCI
    /* Storage/DevAHCI.cpp */
    GEN_CHECK_SIZE(AHCIPORTTASKSTATE);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, uTag);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, fQueued);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, cmdHdr);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, cmdFis);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, cmdFis[AHCI_CMDFIS_TYPE_H2D_SIZE-1]);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, GCPhysCmdHdrAddr);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, uLBAStartSector);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, cSectors);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, pvBufHC);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, uATARegError);
    GEN_CHECK_OFF(AHCIPORTTASKSTATE, uATARegStatus);

    GEN_CHECK_SIZE(AHCIPort);
    GEN_CHECK_OFF(AHCIPort, pDevInsHC);
    GEN_CHECK_OFF(AHCIPort, pDevInsGC);
    GEN_CHECK_OFF(AHCIPort, pAhciHC);
    GEN_CHECK_OFF(AHCIPort, pAhciGC);
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
    GEN_CHECK_OFF(AHCIPort, ahciIOTasks[AHCI_NR_COMMAND_SLOTS-1]);
    GEN_CHECK_OFF(AHCIPort, uActWritePos);
    GEN_CHECK_OFF(AHCIPort, uActReadPos);
    GEN_CHECK_OFF(AHCIPort, uActTasksActive);
    GEN_CHECK_OFF(AHCIPort, fPoweredOn);
    GEN_CHECK_OFF(AHCIPort, fSpunUp);
    GEN_CHECK_OFF(AHCIPort, pDrvBase);
    GEN_CHECK_OFF(AHCIPort, pDrvBlock);
    GEN_CHECK_OFF(AHCIPort, pDrvBlockAsync);
    GEN_CHECK_OFF(AHCIPort, pDrvBlockBios);
    GEN_CHECK_OFF(AHCIPort, IBase);
    GEN_CHECK_OFF(AHCIPort, IPort);
    GEN_CHECK_OFF(AHCIPort, IPortAsync);
    GEN_CHECK_OFF(AHCIPort, PCHSGeometry);
    GEN_CHECK_OFF(AHCIPort, cTotalSectors);
    GEN_CHECK_OFF(AHCIPort, cMultSectors);
    GEN_CHECK_OFF(AHCIPort, uATATransferMode);
    GEN_CHECK_OFF(AHCIPort, iLUN);
    GEN_CHECK_OFF(AHCIPort, fResetDevice);
    GEN_CHECK_OFF(AHCIPort, cbIOBuffer);
    GEN_CHECK_OFF(AHCIPort, pIOBufferHC);
    GEN_CHECK_OFF(AHCIPort, u32TasksFinished);
    GEN_CHECK_OFF(AHCIPort, u32QueuedTasksFinished);
    GEN_CHECK_OFF(AHCIPort, StatAsyncTime);

    GEN_CHECK_SIZE(AHCI);
    GEN_CHECK_OFF(AHCI, dev);
    GEN_CHECK_OFF(AHCI, pDevInsHC);
    GEN_CHECK_OFF(AHCI, pDevInsGC);
    GEN_CHECK_OFF(AHCI, MMIOBase);
    GEN_CHECK_OFF(AHCI, regHbaCap);
    GEN_CHECK_OFF(AHCI, regHbaCtrl);
    GEN_CHECK_OFF(AHCI, regHbaIs);
    GEN_CHECK_OFF(AHCI, regHbaPi);
    GEN_CHECK_OFF(AHCI, regHbaVs);
    GEN_CHECK_OFF(AHCI, regHbaCccCtl);
    GEN_CHECK_OFF(AHCI, regHbaCccPorts);
    GEN_CHECK_OFF(AHCI, pHbaCccTimerHC);
    GEN_CHECK_OFF(AHCI, pHbaCccTimerGC);
    GEN_CHECK_OFF(AHCI, pNotifierQueueHC);
    GEN_CHECK_OFF(AHCI, pNotifierQueueGC);
    GEN_CHECK_OFF(AHCI, uCccPortNr);
    GEN_CHECK_OFF(AHCI, uCccTimeout);
    GEN_CHECK_OFF(AHCI, uCccNr);
    GEN_CHECK_OFF(AHCI, uCccCurrentNr);
    GEN_CHECK_OFF(AHCI, ahciPort);
    GEN_CHECK_OFF(AHCI, ahciPort[AHCI_NR_PORTS_IMPL-1]);
    GEN_CHECK_OFF(AHCI, aCts);
    GEN_CHECK_OFF(AHCI, aCts[1]);
    GEN_CHECK_OFF(AHCI, u32PortsInterrupted);
    GEN_CHECK_OFF(AHCI, fReset);
    GEN_CHECK_OFF(AHCI, f64BitAddr);
    GEN_CHECK_OFF(AHCI, fGCEnabled);
    GEN_CHECK_OFF(AHCI, fR0Enabled);
    GEN_CHECK_OFF(AHCI, lock);
#endif /* VBOX_WITH_AHCI */

    return (0);
}

