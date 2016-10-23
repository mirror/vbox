/* $Id$ */
/** @file
 * DBGPlugInFreeBsd - Debugger and Guest OS Digger Plugin For FreeBSD.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF /// @todo add new log group.
#include "DBGPlugIns.h"
#include "DBGPlugInCommonELF.h"
#include <VBox/vmm/dbgf.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** FreeBSD on little endian ASCII systems. */
#define DIG_FBSD_MOD_TAG     UINT64_C(0x0044534265657246)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * FreeBSD guest OS digger instance data.
 */
typedef struct DBGDIGGERFBSD
{
    /** Whether the information is valid or not.
     * (For fending off illegal interface method calls.) */
    bool        fValid;
    /** 64-bit/32-bit indicator. */
    bool        f64Bit;

    /** Address of the start of the kernel ELF image,
     * set during probing. */
    DBGFADDRESS AddrKernelElfStart;

} DBGDIGGERFBSD;
/** Pointer to the FreeBSD guest OS digger instance data. */
typedef DBGDIGGERFBSD *PDBGDIGGERFBSD;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Min kernel address (32bit). */
#define FBSD32_MIN_KRNL_ADDR             UINT32_C(0x80000000)
/** Max kernel address (32bit). */
#define FBSD32_MAX_KRNL_ADDR             UINT32_C(0xfffff000)

/** Min kernel address (64bit). */
#define FBSD64_MIN_KRNL_ADDR             UINT64_C(0xFFFFFE0000000000)
/** Max kernel address (64bit). */
#define FBSD64_MAX_KRNL_ADDR             UINT64_C(0xFFFFFFFFFFF00000)


/** Validates a 32-bit FreeBSD kernel address */
#define FBSD32_VALID_ADDRESS(Addr)      (   (Addr) > FBSD32_MIN_KRNL_ADDR \
                                         && (Addr) < FBSD32_MAX_KRNL_ADDR)
/** Validates a 64-bit FreeBSD kernel address */
#define FBSD64_VALID_ADDRESS(Addr)       (   (Addr) > FBSD64_MIN_KRNL_ADDR \
                                          && (Addr) < FBSD64_MAX_KRNL_ADDR)

/** Maximum offset from the start of the ELF image we look for the /red/herring .interp section content. */
#define FBSD_MAX_INTERP_OFFSET           _16K
/** The max kernel size. */
#define FBSD_MAX_KERNEL_SIZE             UINT32_C(0x0f000000)


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int)  dbgDiggerFreeBsdInit(PUVM pUVM, void *pvData);

/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Table of common FreeBSD kernel addresses. */
static uint64_t g_au64FreeBsdKernelAddresses[] =
{
    UINT64_C(0xc0100000),
    UINT64_C(0xffffffff80100000)
};

/**
 * @copydoc DBGFOSREG::pfnQueryInterface
 */
static DECLCALLBACK(void *) dbgDiggerFreeBsdQueryInterface(PUVM pUVM, void *pvData, DBGFOSINTERFACE enmIf)
{
    RT_NOREF3(pUVM, pvData, enmIf);
    return NULL;
}


/**
 * @copydoc DBGFOSREG::pfnQueryVersion
 */
static DECLCALLBACK(int)  dbgDiggerFreeBsdQueryVersion(PUVM pUVM, void *pvData, char *pszVersion, size_t cchVersion)
{
    PDBGDIGGERFBSD pThis = (PDBGDIGGERFBSD)pvData;
    Assert(pThis->fValid);

    RT_NOREF4(pUVM, pThis, pszVersion, cchVersion);

    return VERR_NOT_IMPLEMENTED;
}



/**
 * @copydoc DBGFOSREG::pfnTerm
 */
static DECLCALLBACK(void)  dbgDiggerFreeBsdTerm(PUVM pUVM, void *pvData)
{
    RT_NOREF1(pUVM);
    PDBGDIGGERFBSD pThis = (PDBGDIGGERFBSD)pvData;
    Assert(pThis->fValid);

    RT_NOREF1(pUVM);

    pThis->fValid = false;
}


/**
 * @copydoc DBGFOSREG::pfnRefresh
 */
static DECLCALLBACK(int)  dbgDiggerFreeBsdRefresh(PUVM pUVM, void *pvData)
{
    PDBGDIGGERFBSD pThis = (PDBGDIGGERFBSD)pvData;
    NOREF(pThis);
    Assert(pThis->fValid);

    dbgDiggerFreeBsdTerm(pUVM, pvData);
    return dbgDiggerFreeBsdInit(pUVM, pvData);
}


/**
 * @copydoc DBGFOSREG::pfnInit
 */
static DECLCALLBACK(int)  dbgDiggerFreeBsdInit(PUVM pUVM, void *pvData)
{
    PDBGDIGGERFBSD pThis = (PDBGDIGGERFBSD)pvData;
    Assert(!pThis->fValid);

    RT_NOREF1(pUVM);

    pThis->fValid = true;
    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnProbe
 */
static DECLCALLBACK(bool)  dbgDiggerFreeBsdProbe(PUVM pUVM, void *pvData)
{
    PDBGDIGGERFBSD pThis = (PDBGDIGGERFBSD)pvData;

    /*
     * Look for the magic ELF header near the known start addresses.
     * If one is found look for the magic "/red/herring" string which is in the
     * "interp" section not far away and then validate the start of the ELF header
     * to be sure.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(g_au64FreeBsdKernelAddresses); i++)
    {
        static const uint8_t s_abNeedle[] = ELFMAG;
        DBGFADDRESS KernelAddr;
        DBGFR3AddrFromFlat(pUVM, &KernelAddr, g_au64FreeBsdKernelAddresses[i]);
        DBGFADDRESS HitAddr;
        uint32_t    cbLeft  = FBSD_MAX_KERNEL_SIZE;

        while (cbLeft > X86_PAGE_4K_SIZE)
        {
            int rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &KernelAddr, cbLeft, 1,
                                   s_abNeedle, sizeof(s_abNeedle) - 1, &HitAddr);
            if (RT_FAILURE(rc))
                break;

            /*
             * Look for the magic "/red/herring" near the header and verify the basic
             * ELF header.
             */
            static const uint8_t s_abNeedleInterp[] = "/red/herring";
            DBGFADDRESS HitAddrInterp;
            rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &HitAddr, FBSD_MAX_INTERP_OFFSET, 1,
                               s_abNeedleInterp, sizeof(s_abNeedleInterp), &HitAddrInterp);
            if (RT_SUCCESS(rc))
            {
                union
                {
                    uint8_t    ab[2 * X86_PAGE_4K_SIZE];
                    Elf32_Ehdr Hdr32;
                    Elf64_Ehdr Hdr64;
                } ElfHdr;
                AssertCompileMembersSameSizeAndOffset(Elf64_Ehdr, e_ident,   Elf32_Ehdr, e_ident);
                AssertCompileMembersSameSizeAndOffset(Elf64_Ehdr, e_type,    Elf32_Ehdr, e_type);
                AssertCompileMembersSameSizeAndOffset(Elf64_Ehdr, e_machine, Elf32_Ehdr, e_machine);
                AssertCompileMembersSameSizeAndOffset(Elf64_Ehdr, e_version, Elf32_Ehdr, e_version);

                rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &HitAddr, &ElfHdr.ab[0], X86_PAGE_4K_SIZE);
                if (RT_SUCCESS(rc))
                {
                    /* We verified the magic above already by scanning for it. */
                    if (   (   ElfHdr.Hdr32.e_ident[EI_CLASS] == ELFCLASS32
                            || ElfHdr.Hdr32.e_ident[EI_CLASS] == ELFCLASS64)
                        && ElfHdr.Hdr32.e_ident[EI_DATA] == ELFDATA2LSB
                        && ElfHdr.Hdr32.e_ident[EI_VERSION] == EV_CURRENT
                        && ElfHdr.Hdr32.e_ident[EI_OSABI] == ELFOSABI_FREEBSD
                        && ElfHdr.Hdr32.e_type == ET_EXEC
                        && (   ElfHdr.Hdr32.e_machine == EM_386
                            || ElfHdr.Hdr32.e_machine == EM_X86_64)
                        && ElfHdr.Hdr32.e_version == EV_CURRENT)
                    {
                        pThis->f64Bit = ElfHdr.Hdr32.e_ident[EI_CLASS] == ELFCLASS64;
                        pThis->AddrKernelElfStart = HitAddr;
                        return true;
                    }
                }
            }

            /*
             * Advance.
             */
            RTGCUINTPTR cbDistance = HitAddr.FlatPtr - KernelAddr.FlatPtr + sizeof(s_abNeedle) - 1;
            if (RT_UNLIKELY(cbDistance >= cbLeft))
                break;

            cbLeft -= cbDistance;
            DBGFR3AddrAdd(&KernelAddr, cbDistance);
        }
    }
    return false;
}


/**
 * @copydoc DBGFOSREG::pfnDestruct
 */
static DECLCALLBACK(void)  dbgDiggerFreeBsdDestruct(PUVM pUVM, void *pvData)
{
    RT_NOREF2(pUVM, pvData);
}


/**
 * @copydoc DBGFOSREG::pfnConstruct
 */
static DECLCALLBACK(int)  dbgDiggerFreeBsdConstruct(PUVM pUVM, void *pvData)
{
    RT_NOREF2(pUVM, pvData);
    return VINF_SUCCESS;
}


const DBGFOSREG g_DBGDiggerFreeBsd =
{
    /* .u32Magic = */           DBGFOSREG_MAGIC,
    /* .fFlags = */             0,
    /* .cbData = */             sizeof(DBGDIGGERFBSD),
    /* .szName = */             "FreeBSD",
    /* .pfnConstruct = */       dbgDiggerFreeBsdConstruct,
    /* .pfnDestruct = */        dbgDiggerFreeBsdDestruct,
    /* .pfnProbe = */           dbgDiggerFreeBsdProbe,
    /* .pfnInit = */            dbgDiggerFreeBsdInit,
    /* .pfnRefresh = */         dbgDiggerFreeBsdRefresh,
    /* .pfnTerm = */            dbgDiggerFreeBsdTerm,
    /* .pfnQueryVersion = */    dbgDiggerFreeBsdQueryVersion,
    /* .pfnQueryInterface = */  dbgDiggerFreeBsdQueryInterface,
    /* .u32EndMagic = */        DBGFOSREG_MAGIC
};

