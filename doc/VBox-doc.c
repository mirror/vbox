/* $Id$ */
/** @file
 * VirtualBox Top Level Documentation File.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @mainpage   VirtualBox
 *
 * (add introduction here)
 *
 * @section pg_main_comp    Components
 *
 *  - VM / @ref pg_vmm "VMM" / GVM / @ref pg_gvmm "GVMM" - Virtual Machine
 *    Monitor.
 *      - @ref pg_cfgm
 *      - @ref pg_cpum
 *      - CSAM - Guest OS Code Scanning and Analyis Manager.
 *      - @ref pg_dbgf
 *          - @ref pg_dbgf_addr_space
 *          - @ref pg_dbgf_vmcore
 *          - @ref pg_dbgf_module
 *          - @ref pg_dbgc
 *          - VBoxDbg - Debugger GUI (Qt).
 *      - DIS - Disassembler.
 *      - @ref pg_em
 *      - HWACCM - Intel/AMD VM Hardware Support Manager.
 *      - REM - Recompiled Execution Monitor.
 *          - @ref pg_vboxrem_amd64
 *      - @ref pg_iem
 *      - @ref pg_gmm
 *          - @ref pg_mm
 *          - @ref pg_pgm
 *              - @ref pg_pgm_phys
 *              - @ref pg_pgm_pool
 *          - @ref pg_selm
 *      - @ref pg_iom
 *      - PATM - Dynamic Guest OS Patching Manager.
 *      - @ref pg_pdm
 *          - Devices / USB Devices, Drivers and their public interfaces.
 *          - Async I/O Completion API.
 *          - Async Task API.
 *          - Critical Section API.
 *          - Queue API.
 *          - Thread API.
 *          - @ref pg_pdm_block_cache
 *      - @ref pg_ssm
 *      - @ref pg_stam
 *      - @ref pg_tm
 *      - @ref pg_trpm
 *      - VMM docs:
 *          - @ref pg_vmm_guideline
 *          - @ref pg_raw
 *  - Pluggable Components (via PDM).
 *      - DevPCArch - PC Architecture Device (chipset, legacy ++).
 *      - DevPCBios - Basic Input Output System.
 *      - DevDMAC - DMA Controller.
 *      - DevPIC - Programmable Interrupt Controller.
 *      - DevPIT - Programmable Interval Timer (i8254).
 *      - DevRTC - Real Time Clock.
 *      - DevVGA - Video Graphic Array.
 *      - DevPCI - Peripheral Component Interface (Bus).
 *      - VBoxDev - Special PCI Device which serves as an interface between
 *                  the VMM and the guest OS for the additions.
 *      - Networking:
 *          - DevPCNet - AMD PCNet Device Emulation.
 *          - DevE1000 - Intel E1000 Device Emulation.
 *          - DevEEPROM - Intel E1000 EPROM Device Emulation.
 *          - SrvINetNetR0 - Internal Networking Ring-0 Service.
 *          - DevINIP - IP Stack Service for the internal networking.
 *          - DrvIntNet - Internal Networking Driver.
 *          - DrvNetSniffer - Wireshark Compatible Sniffer Driver (pass thru).
 *          - DrvNAT - Network Address Translation Driver.
 *          - DrvTAP - Host Interface Networking Driver.
 *      - Storage:
 *          - DevATA - ATA ((E)IDE) Device Emulation.
 *          - @ref pg_dev_ahci
 *          - DevFDC - Floppy Controller Device Emulation.
 *          - DrvBlock - Intermediate block driver.
 *          - DrvHostBase - Common code for the host drivers.
 *          - DrvHostDVD - Host DVD drive driver.
 *          - DrvHostFloppy - Host floppy drive driver.
 *          - DrvHostRawDisk - Host raw disk drive driver.
 *          - DrvMediaISO - ISO media driver.
 *          - DrvRawImage - Raw image driver (floppy images etc).
 *          - DrvVD - Intermediate Virtual Drive (Media) driver.
 *          - DrvVDI - VirtualBox Drive Image Container Driver.
 *          - DrvVmdk - VMDK Drive Image Container Driver.
 *      - USB:
 *          - @ref pg_dev_ohci
 *          - @ref pg_dev_ehci
 *          - @ref pg_dev_vusb
 *          - @ref pg_dev_vusb_old
 *  - Host Drivers.
 *      - SUPDRV - The Support driver (aka VBoxDrv).
 *          - @ref pg_sup
 *      - @ref pg_netflt
 *      - @ref pg_netadp
 *      - VBoxUSB - The USB support driver.
 *      - @ref pg_netflt
 *      - @ref pg_rawpci
 *  - Host Services.
 *      - @ref pg_hostclip Shared Clipboard.
 *      - Shared Folders.
 *      - Shared OpenGL. See PDF. (TODO: translate PDF to doxygen)
 *          - @ref pg_opengl_cocoa
 *      - @ref pg_svc_guest_properties
 *      - @ref pg_svc_guest_control
 *  - Guest Additions.
 *      - VBoxGuest.
 *          - @ref pg_guest_lib
 *      - VBoxService.
 *          - @ref pg_vboxervice_timesync
 *          - ...
 *      - VBoxControl.
 *      - VBoxVideo.
 *      - crOpenGL.
 *      - VBoxClient / VBoxTray.
 *      - pam.
 *      - ...
 *  - Network Services:
 *      - @ref pg_net_dhcp
 *      - @ref pg_net_nat
 *  - @ref pg_main
 *      - @ref pg_main_events
 *      - @ref pg_vrdb_usb
 *  - IPRT - Runtime Library for hiding host OS differences.
 *  - Testsuite:
 *      - @ref pg_testsuite_guideline
 *
 * @todo Make links to the components.
 */

