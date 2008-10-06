/* $Id$ */
/** @file
 * VirtualBox Top Level Documentation File.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *          - DBGC - Debugger Console.
 *          - VBoxDbg - Debugger GUI (Qt).
 *      - DIS - Disassembler.
 *      - @ref pg_em
 *      - HWACCM - Intel/AMD VM Hardware Support Manager.
 *      - REM - Recompiled Execution Monitor.
 *      - @ref pg_gmm
 *          - @ref pg_mm
 *          - @ref pg_pgm
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
 *      - @ref pg_ssm
 *      - @ref pg_stam
 *      - @ref pg_tm
 *      - @ref pg_trpm
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
 *          - DevAHCI - Serial ATA / AHCI Device Emulation.
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
 *  - Host Drivers.
 *      - SUPDRV - The Support driver (aka VBoxDrv).
 *      - VBoxUSB - The USB support driver.
 *      - VBoxTAP - The Host Interface Networking driver.
 *      - @ref pg_netflt
 *  - Host Services.
 *      - Shared Clipboard.
 *      - 3D
 *  - Main API.
 *
 * @todo Make links to the components.
 */

