/* $Id$ */
/** @file
 * EFI for VirtualBox Common Definitions.
 *
 * WARNING: This header is used by both firmware and VBox device,
 * thus don't put anything here but numeric constants or helper
 * inline functions.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ___EFI_VBoxEFI_h
#define ___EFI_VBoxEFI_h

#include <iprt/assert.h>

/** @defgroup grp_devefi    DevEFI <-> Firmware Interfaces
 * @{
 */

/** The base of the I/O ports used for interfaction between the EFI firmware and DevEFI. */
#define EFI_PORT_BASE           0xEF10
/** The number of ports. */
#define EFI_PORT_COUNT          0x0004

/** Information querying.
 * 32-bit write sets the info index and resets the reading, see EfiInfoIndex.
 * 32-bit read returns the size of the info (in bytes).
 * 8-bit reads returns the info as a byte sequence. */
#define EFI_INFO_PORT           (EFI_PORT_BASE+0x0)
/** Information requests.
 * @todo Put this in DEVEFIINFO, that's much easier to access. */
typedef enum
{
    EFI_INFO_INDEX_INVALID                    = 0,
    EFI_INFO_INDEX_VOLUME_BASE,
    EFI_INFO_INDEX_VOLUME_SIZE,
    EFI_INFO_INDEX_TEMPMEM_BASE,
    EFI_INFO_INDEX_TEMPMEM_SIZE,
    EFI_INFO_INDEX_STACK_BASE,
    EFI_INFO_INDEX_STACK_SIZE,
    EFI_INFO_INDEX_END
} EfiInfoIndex;

/** Panic port.
 * Write causes action to be taken according to the value written,
 * see the EFI_PANIC_CMD_* defines below.
 * Reading from the port has no effect. */
#define EFI_PANIC_PORT          (EFI_PORT_BASE+0x1)
/** @defgroup grp_devefi_panic_cmd  Panic Commands for EFI_PANIC_PORT
 * @{ */
/** Used by the EfiThunk.asm to signal ORG inconsistency. */
#define EFI_PANIC_CMD_BAD_ORG           1
/** Used by the EfiThunk.asm to signal unexpected trap or interrupt. */
#define EFI_PANIC_CMD_THUNK_TRAP        2
/** Starts a panic message.
 * Makes sure the panic message buffer is empty. */
#define EFI_PANIC_CMD_START_MSG         3
/** Ends a panic message and enters guru meditation state. */
#define EFI_PANIC_CMD_END_MSG           4
/** The first panic message command.
 * The low byte of the command is the char to be added to the panic message. */
#define EFI_PANIC_CMD_MSG_FIRST         0x4201
/** The last panic message command. */
#define EFI_PANIC_CMD_MSG_LAST          0x427f
/** Makes a panic message command from a char. */
#define EFI_PANIC_CMD_MSG_FROM_CHAR(ch) (0x4200 | ((ch) & 0x7f) )
/** Extracts the char from a panic message command. */
#define EFI_PANIC_CMD_MSG_GET_CHAR(u32) ((u32) & 0x7f)
/** @} */

/** Undefined port. */
#define EFI_PORT_UNDEFINED      (EFI_PORT_BASE+0x2)

/** Debug logging.
 * The chars written goes to the log.
 * Reading has no effect.
 * @remarks The port number is the same as on of those used by the PC BIOS. */
#define EFI_DEBUG_PORT          (EFI_PORT_BASE+0x3)

#define VBOX_EFI_DEBUG_BUFFER   512
/** The top of the EFI stack.
 * The firmware expects a 128KB stack.
 * @todo Move this to 1MB + 128KB and drop the stack relocation the firmware
 *       does. It expects the stack to be within the temporary memory that
 *       SEC hands to PEI and the VBoxAutoScan PEIM reports. */
#define VBOX_EFI_TOP_OF_STACK   0x300000

/**
 * DevEFI Info stored at DEVEFI_INFO_PHYS_ADDR
 */
typedef struct DEVEFIINFO
{
    /** 0x00 - The physical address of the firmware entry point. */
    uint32_t    pfnFirmwareEP;
    /** 0x04 - Spaced reserved for the high part of a 64-bit entrypoint address. */
    uint32_t    HighEPAddress;
    /** 0x08 - The address of the firmware volume. */
    RTGCPHYS    PhysFwVol;
    /** 0x10 - The size of the firmware volume. */
    uint32_t    cbFwVol;
    /** 0x14 - Amount of memory below 4GB (in bytes). */
    uint32_t    cbBelow4GB;
    /** 0x18 - Amount of memory above 4GB (in bytes). */
    uint64_t    cbAbove4GB;
    /** 0x20 - see flags values below */
    uint32_t    fFlags;
    /** 0x24 - The nubmer of Virtual CPUs. */
    uint32_t    cCpus;
    /** 0x28 - Reserved for future use, must be zero. */
    uint32_t    pfnPeiEP;
    /** 0x2c - Reserved for future use, must be zero. */
    uint32_t    u32Reserved2;
} DEVEFIINFO;
AssertCompileSize(DEVEFIINFO, 0x30);
/** Pointer to a DevEFI info structure. */
typedef DEVEFIINFO *PDEVEFIINFO;
/** Pointer to a const DevEFI info structure. */
typedef DEVEFIINFO const *PCDEVEFIINFO;

/** The physical address where DEVEFIINFO can be found. */
#define DEVEFI_INFO_PHYS_ADDR   (0xfffff000)
#define DEVEFI_INFO_FLAGS_AMD64   RT_BIT(0)

/** @} */

#define KB(x) ((x) * 1024)
#define MB(x) ((KB(x)) * 1024)

#endif

