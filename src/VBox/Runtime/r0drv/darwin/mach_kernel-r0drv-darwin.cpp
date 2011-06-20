/* $Id$ */
/** @file
 * IPRT - mach_kernel symbol resolving hack, R0 Driver, Darwin.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifdef IN_RING0
# include "the-darwin-kernel.h"
#endif
#include "../../include/internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include "../../include/internal/ldrMach-O.h"

/** @def MY_CPU_TYPE
 * The CPU type targeted by the compiler. */
/** @def MY_CPU_TYPE
 * The "ALL" CPU subtype targeted by the compiler. */
/** @def MY_MACHO_HEADER
 * The Mach-O header targeted by the compiler.  */
/** @def MY_MACHO_MAGIC
 * The Mach-O header magic we're targeting.  */
/** @def MY_SEGMENT_COMMAND
 * The segment command targeted by the compiler.  */
/** @def MY_SECTION
 * The section struture targeted by the compiler.  */
/** @def MY_NLIST
 * The symbol table entry targeted by the compiler.  */
#ifdef RT_ARCH_X86
# define MY_CPU_TYPE            CPU_TYPE_I386
# define MY_CPU_SUBTYPE_ALL     CPU_SUBTYPE_I386_ALL
# define MY_MACHO_HEADER        mach_header_32_t
# define MY_MACHO_MAGIC         IMAGE_MACHO32_SIGNATURE
# define MY_SEGMENT_COMMAND     segment_command_32_t
# define MY_SECTION             section_32_t
# define MY_NLIST               macho_nlist_32_t

#elif defined(RT_ARCH_AMD64)
# define MY_CPU_TYPE            CPU_TYPE_X86_64
# define MY_CPU_SUBTYPE_ALL     CPU_SUBTYPE_X86_64_ALL
# define MY_MACHO_HEADER        mach_header_64_t
# define MY_MACHO_MAGIC         IMAGE_MACHO64_SIGNATURE
# define MY_SEGMENT_COMMAND     segment_command_64_t
# define MY_SECTION             section_64_t
# define MY_NLIST               macho_nlist_64_t

#else
# error "Port me!"
#endif

/** @name Return macros for make it simpler to track down too paranoid code.
 * @{
 */
#ifdef DEBUG
# define RETURN_VERR_BAD_EXE_FORMAT \
    do { Assert(!g_fBreakpointOnError);         return VERR_BAD_EXE_FORMAT; } while (0)
# define RETURN_VERR_LDR_UNEXPECTED \
    do { Assert(!g_fBreakpointOnError);         return VERR_LDR_UNEXPECTED; } while (0)
# define RETURN_VERR_LDR_ARCH_MISMATCH \
    do { Assert(!g_fBreakpointOnError);         return VERR_LDR_ARCH_MISMATCH; } while (0)
#else
# define RETURN_VERR_BAD_EXE_FORMAT     do {    return VERR_BAD_EXE_FORMAT; } while (0)
# define RETURN_VERR_LDR_UNEXPECTED     do {    return VERR_LDR_UNEXPECTED; } while (0)
# define RETURN_VERR_LDR_ARCH_MISMATCH  do {    return VERR_LDR_ARCH_MISMATCH; } while (0)
#endif
/** @} */

#define VERR_LDR_UNEXPECTED     (-641)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Our internal representation of the mach_kernel after loading it's symbols
 * and successfully resolving their addresses.
 */
typedef struct RTR0DARWINKERNEL
{
    /** @name Result.
     * @{ */
    /** Pointer to the string table. */
    char               *pachStrTab;
    /** The size of the string table. */
    uint32_t            cbStrTab;
    /** The file offset of the string table. */
    uint32_t            offStrTab;
    /** Pointer to the symbol table. */
    MY_NLIST           *paSyms;
    /** The size of the symbol table. */
    uint32_t            cSyms;
    /** The file offset of the symbol table. */
    uint32_t            offSyms;
    /** @} */

    /** @name Used during loading.
     * @{ */
    /** The file handle.  */
    RTFILE              hFile;
    /** The architecture image offset (fat_arch_t::offset). */
    uint64_t            offArch;
    /** The architecture image size (fat_arch_t::size). */
    uint32_t            cbArch;
    /** The number of load commands (mach_header_XX_t::ncmds). */
    uint32_t            cLoadCmds;
    /** The size of the load commands. */
    uint32_t            cbLoadCmds;
    /** The load commands. */
    load_command_t     *pLoadCmds;
    /** Section pointer table (points into the load commands). */
    MY_SECTION const   *apSections[MACHO_MAX_SECT];
    /** The number of sections. */
    uint32_t            cSections;
    /** @} */

    /** Buffer space. */
    char                abBuf[_4K];
} RTR0DARWINKERNEL;
typedef RTR0DARWINKERNEL *PRTR0DARWINKERNEL;


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#ifdef DEBUG
static bool g_fBreakpointOnError = true;
#endif


/**
 * Frees up the internal scratch data when done looking up symbols.
 *
 * @param   pKernel             The internal scratch data.
 */
static void rtR0DarwinMachKernelClose(PRTR0DARWINKERNEL pKernel)
{
    RTMemFree(pKernel->pachStrTab);
    pKernel->pachStrTab = NULL;
    RTMemFree(pKernel->paSyms);
    pKernel->paSyms = NULL;

    RTMemFree(pKernel);
}


/**
 * Close and free up resources we no longer needs.
 *
 * @param   pKernel             The internal scratch data.
 */
static void rtR0DarwinMachKernelLoadDone(PRTR0DARWINKERNEL pKernel)
{
    RTFileClose(pKernel->hFile);
    pKernel->hFile = NIL_RTFILE;

    RTMemFree(pKernel->pLoadCmds);
    pKernel->pLoadCmds = NULL;
    RT_ZERO(pKernel->apSections);
}


/**
 * Looks up a kernel symbol.
 *
 *
 * @returns The symbol address on success, 0 on failure.
 * @param   pKernel             The internal scratch data.
 * @param   pszSymbol           The symbol to resolve.  Automatically prefixed
 *                              with an underscore.
 */
static uintptr_t rtR0DarwinMachKernelLookup(PRTR0DARWINKERNEL pKernel, const char *pszSymbol)
{
    uint32_t const  cSyms = pKernel->cSyms;
    MY_NLIST const *pSym = pKernel->paSyms;

#if 1
    /* linear search. */
    for (uint32_t iSym = 0; iSym < cSyms; iSym++, pSym++)
    {
        if (pSym->n_type & MACHO_N_STAB)
            continue;

        const char *pszTabName= &pKernel->pachStrTab[(uint32_t)pSym->n_un.n_strx];
        if (   *pszTabName == '_'
            && strcmp(pszTabName + 1, pszSymbol) == 0)
            return pSym->n_value;
    }
#else
    /** @todo binary search. */

#endif
    return 0;
}


extern "C" void OSRuntimeFinalizeCPP(void);
extern "C" void OSRuntimeInitializeCPP(void);

static int rtR0DarwinMachKernelCheckStandardSymbols(PRTR0DARWINKERNEL pKernel)
{
    static struct
    {
        const char *pszName;
        uintptr_t   uAddr;
    } const s_aStandardCandles[] =
    {
#ifdef IN_RING0
# define KNOWN_ENTRY(a_Sym)  { #a_Sym, (uintptr_t)&a_Sym }
#else
# define KNOWN_ENTRY(a_Sym)  { #a_Sym, 0 }
#endif
        KNOWN_ENTRY(IOMalloc),
        KNOWN_ENTRY(IOFree),
        KNOWN_ENTRY(OSRuntimeFinalizeCPP),
        KNOWN_ENTRY(OSRuntimeInitializeCPP)
    };

    for (unsigned i = 0; i < RT_ELEMENTS(s_aStandardCandles); i++)
    {
        uintptr_t uAddr = rtR0DarwinMachKernelLookup(pKernel, s_aStandardCandles[i].pszName);
#ifdef IN_RING0
        if (uAddr != s_aStandardCandles[i].uAddr)
#else
        if (uAddr == 0)
#endif
        {
            AssertLogRelMsgFailed(("%s (%p != %p)\n", s_aStandardCandles[i].pszName, uAddr, s_aStandardCandles[i].uAddr));
            return VERR_INTERNAL_ERROR_2;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Loads and validates the symbol and string tables.
 *
 * @returns IPRT status code.
 * @param   pKernel             The internal scratch data.
 */
static int rtR0DarwinMachKernelLoadSymTab(PRTR0DARWINKERNEL pKernel)
{
    /*
     * Load the tables.
     */
    pKernel->paSyms = (MY_NLIST *)RTMemAllocZ(pKernel->cSyms * sizeof(MY_NLIST));
    if (!pKernel->paSyms)
        return VERR_NO_MEMORY;

    int rc = RTFileReadAt(pKernel->hFile, pKernel->offArch + pKernel->offSyms,
                          pKernel->paSyms, pKernel->cSyms * sizeof(MY_NLIST), NULL);
    if (RT_FAILURE(rc))
        return rc;

    pKernel->pachStrTab = (char *)RTMemAllocZ(pKernel->cbStrTab + 1);
    if (!pKernel->pachStrTab)
        return VERR_NO_MEMORY;

    rc = RTFileReadAt(pKernel->hFile, pKernel->offArch + pKernel->offStrTab,
                      pKernel->pachStrTab, pKernel->cbStrTab, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * The first string table symbol must be a zero length name.
     */
    if (pKernel->pachStrTab[0] != '\0')
        RETURN_VERR_BAD_EXE_FORMAT;

    /*
     * Validate the symbol table.
     */
    const char     *pszPrev = "";
    uint32_t const  cSyms   = pKernel->cSyms;
    MY_NLIST const  *pSym   = pKernel->paSyms;
    for (uint32_t iSym = 0; iSym < cSyms; iSym++, pSym++)
    {
        if ((uint32_t)pSym->n_un.n_strx >= pKernel->cbStrTab)
            RETURN_VERR_BAD_EXE_FORMAT;
        const char *pszSym = &pKernel->pachStrTab[(uint32_t)pSym->n_un.n_strx];
#ifdef IN_RING3
        RTAssertMsg2("%05i: %02x:%08x %02x %04x %s\n", iSym, pSym->n_sect, pSym->n_value, pSym->n_type, pSym->n_desc, pszSym);
#endif

        if (strcmp(pszSym, pszPrev) < 0)
            RETURN_VERR_BAD_EXE_FORMAT; /* not sorted */

        if (!(pSym->n_type & MACHO_N_STAB))
        {
            switch (pSym->n_type & MACHO_N_TYPE)
            {
                case MACHO_N_SECT:
                    if (pSym->n_sect == MACHO_NO_SECT)
                        RETURN_VERR_BAD_EXE_FORMAT;
                    if (pSym->n_sect > pKernel->cSections)
                        RETURN_VERR_BAD_EXE_FORMAT;
                    if (pSym->n_desc & ~(REFERENCED_DYNAMICALLY))
                        RETURN_VERR_BAD_EXE_FORMAT;
                    if (pSym->n_value < pKernel->apSections[pSym->n_sect - 1]->addr)
                        RETURN_VERR_BAD_EXE_FORMAT;
                    if (   pSym->n_value - pKernel->apSections[pSym->n_sect - 1]->addr
                        > pKernel->apSections[pSym->n_sect - 1]->size)
                        RETURN_VERR_BAD_EXE_FORMAT;
                    break;

                case MACHO_N_ABS:
#if 0 /* Spec say MACHO_NO_SECT, __mh_execute_header has 1 with 10.7/amd64 */
                    if (pSym->n_sect != MACHO_NO_SECT)
#else
                    if (pSym->n_sect > pKernel->cSections) 
#endif
                        RETURN_VERR_BAD_EXE_FORMAT;
                    if (pSym->n_desc & ~(REFERENCED_DYNAMICALLY))
                        RETURN_VERR_BAD_EXE_FORMAT;
                    break;

                case MACHO_N_UNDF:
                    /* No undefined or common symbols in the kernel. */
                    RETURN_VERR_BAD_EXE_FORMAT;

                case MACHO_N_INDR:
                    /* No indirect symbols in the kernel. */
                    RETURN_VERR_BAD_EXE_FORMAT;

                case MACHO_N_PBUD:
                    /* No prebound symbols in the kernel. */
                    RETURN_VERR_BAD_EXE_FORMAT;

                default:
                    RETURN_VERR_BAD_EXE_FORMAT;
            }
        }
        /* else: Ignore debug symbols. */
    }

    return VINF_SUCCESS;
}


/**
 * Loads the load commands and validates them.
 *
 * @returns IPRT status code.
 * @param   pKernel             The internal scratch data.
 */
static int rtR0DarwinMachKernelLoadCommands(PRTR0DARWINKERNEL pKernel)
{
    pKernel->offStrTab = 0;
    pKernel->cbStrTab  = 0;
    pKernel->offSyms   = 0;
    pKernel->cSyms     = 0;
    pKernel->cSections = 0;

    pKernel->pLoadCmds = (load_command_t *)RTMemAlloc(pKernel->cbLoadCmds);
    if (!pKernel->pLoadCmds)
        return VERR_NO_MEMORY;

    int rc = RTFileReadAt(pKernel->hFile, pKernel->offArch + sizeof(MY_MACHO_HEADER),
                          pKernel->pLoadCmds, pKernel->cbLoadCmds, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Validate the relevant commands, picking up sections and the symbol
     * table location.
     */
    load_command_t const   *pCmd = pKernel->pLoadCmds;
    for (uint32_t iCmd = 0; ; iCmd++)
    {
        /* cmd index & offset. */
        uintptr_t offCmd = (uintptr_t)pCmd - (uintptr_t)pKernel->pLoadCmds;
        if (offCmd == pKernel->cbLoadCmds && iCmd == pKernel->cLoadCmds)
            break;
        if (offCmd + sizeof(*pCmd) > pKernel->cbLoadCmds)
            RETURN_VERR_BAD_EXE_FORMAT;
        if (iCmd >= pKernel->cLoadCmds)
            RETURN_VERR_BAD_EXE_FORMAT;

        /* cmdsize */
        if (pCmd->cmdsize < sizeof(*pCmd))
            RETURN_VERR_BAD_EXE_FORMAT;
        if (pCmd->cmdsize > pKernel->cbLoadCmds)
            RETURN_VERR_BAD_EXE_FORMAT;
        if (RT_ALIGN_32(pCmd->cmdsize, 4) != pCmd->cmdsize)
            RETURN_VERR_BAD_EXE_FORMAT;

        /* cmd */
        switch (pCmd->cmd & ~LC_REQ_DYLD)
        {
            /* Validate and store the symbol table details. */
            case LC_SYMTAB:
            {
                struct symtab_command const *pSymTab = (struct symtab_command const *)pCmd;
                if (pSymTab->cmdsize != sizeof(*pSymTab))
                    RETURN_VERR_BAD_EXE_FORMAT;
                if (pSymTab->nsyms > _1M)
                    RETURN_VERR_BAD_EXE_FORMAT;
                if (pSymTab->strsize > _2M)
                    RETURN_VERR_BAD_EXE_FORMAT;

                pKernel->offStrTab = pSymTab->stroff;
                pKernel->cbStrTab  = pSymTab->strsize;
                pKernel->offSyms   = pSymTab->symoff;
                pKernel->cSyms     = pSymTab->nsyms;
                break;
            }

            /* Validate the segment. */
#if ARCH_BITS == 32
            case LC_SEGMENT_32:
#elif ARCH_BITS == 64
            case LC_SEGMENT_64:
#else
# error ARCH_BITS
#endif
            {
                MY_SEGMENT_COMMAND const *pSeg = (MY_SEGMENT_COMMAND const *)pCmd;
                if (pSeg->cmdsize < sizeof(*pSeg))
                    RETURN_VERR_BAD_EXE_FORMAT;

                if (pSeg->segname[0] == '\0')
                    RETURN_VERR_BAD_EXE_FORMAT;

                if (pSeg->nsects > MACHO_MAX_SECT)
                    RETURN_VERR_BAD_EXE_FORMAT;
                if (pSeg->nsects * sizeof(MY_SECTION) + sizeof(*pSeg) != pSeg->cmdsize)
                    RETURN_VERR_BAD_EXE_FORMAT;

                if (pSeg->flags & ~(SG_HIGHVM | SG_FVMLIB | SG_NORELOC | SG_PROTECTED_VERSION_1))
                    RETURN_VERR_BAD_EXE_FORMAT;

                if (pSeg->vmaddr != 0)
                {
                    if (pSeg->vmaddr + RT_ALIGN_Z(pSeg->vmsize, RT_BIT_32(12)) < pSeg->vmaddr)
                        RETURN_VERR_BAD_EXE_FORMAT;
                }
                else if (pSeg->vmsize)
                    RETURN_VERR_BAD_EXE_FORMAT;

                if (pSeg->maxprot & ~VM_PROT_ALL)
                    RETURN_VERR_BAD_EXE_FORMAT;
                if (pSeg->initprot & ~VM_PROT_ALL)
                    RETURN_VERR_BAD_EXE_FORMAT;

                /* Validate the sections. */
                uint32_t            uAlignment = 0;
                uintptr_t           uAddr      = pSeg->vmaddr;
                MY_SECTION const   *paSects    = (MY_SECTION const *)(pSeg + 1);
                for (uint32_t i = 0; i < pSeg->nsects; i++)
                {
                    if (paSects[i].sectname[0] == '\0')
                        RETURN_VERR_BAD_EXE_FORMAT;
                    if (memcmp(paSects[i].segname, pSeg->segname, sizeof(pSeg->segname)))
                        RETURN_VERR_BAD_EXE_FORMAT;

                    switch (paSects[i].flags & SECTION_TYPE)
                    {
                        case S_REGULAR:
                        case S_CSTRING_LITERALS:
                        case S_NON_LAZY_SYMBOL_POINTERS:
                        case S_MOD_INIT_FUNC_POINTERS:
                        case S_MOD_TERM_FUNC_POINTERS:
                        case S_COALESCED:
                            if (  pSeg->filesize != 0
                                ? paSects[i].offset - pSeg->fileoff >= pSeg->filesize
                                : paSects[i].offset - pSeg->fileoff != pSeg->filesize)
                                RETURN_VERR_BAD_EXE_FORMAT;
                            if (   paSects[i].addr != 0
                                && paSects[i].offset - pSeg->fileoff != paSects[i].addr - pSeg->vmaddr)
                                RETURN_VERR_BAD_EXE_FORMAT;
                            break;

                        case S_ZEROFILL:
                            if (paSects[i].offset != 0)
                                RETURN_VERR_BAD_EXE_FORMAT;
                            break;

                        /* not observed */
                        case S_SYMBOL_STUBS:
                        case S_INTERPOSING:
                        case S_4BYTE_LITERALS:
                        case S_8BYTE_LITERALS:
                        case S_16BYTE_LITERALS:
                        case S_DTRACE_DOF:
                        case S_LAZY_SYMBOL_POINTERS:
                        case S_LAZY_DYLIB_SYMBOL_POINTERS:
                            RETURN_VERR_LDR_UNEXPECTED;
                        case S_GB_ZEROFILL:
                            RETURN_VERR_LDR_UNEXPECTED;
                        default:
                            RETURN_VERR_BAD_EXE_FORMAT;
                    }

                    if (paSects[i].align > 12)
                        RETURN_VERR_BAD_EXE_FORMAT;
                    if (paSects[i].align > uAlignment)
                        uAlignment = paSects[i].align;

                    /* Add to the section table. */
                    if (pKernel->cSections == MACHO_MAX_SECT)
                        RETURN_VERR_BAD_EXE_FORMAT;
                    pKernel->apSections[pKernel->cSections++] = &paSects[i];
                }

                if (RT_ALIGN_Z(pSeg->vmaddr, RT_BIT_32(uAlignment)) != pSeg->vmaddr)
                    RETURN_VERR_BAD_EXE_FORMAT;
                if (   pSeg->filesize > RT_ALIGN_Z(pSeg->vmsize, RT_BIT_32(uAlignment))
                    && pSeg->vmsize != 0)
                    RETURN_VERR_BAD_EXE_FORMAT;
                break;
            }

            case LC_UUID:
                if (pCmd->cmdsize != sizeof(uuid_command))
                    RETURN_VERR_BAD_EXE_FORMAT;
                break;

            case LC_DYSYMTAB:
            case LC_UNIXTHREAD:
                break;

            /* not observed */
            case LC_SYMSEG:
#if ARCH_BITS == 32
            case LC_SEGMENT_64:
#elif ARCH_BITS == 64
            case LC_SEGMENT_32:
#endif
            case LC_ROUTINES_64:
            case LC_ROUTINES:
            case LC_THREAD:
            case LC_LOADFVMLIB:
            case LC_IDFVMLIB:
            case LC_IDENT:
            case LC_FVMFILE:
            case LC_PREPAGE:
            case LC_TWOLEVEL_HINTS:
            case LC_PREBIND_CKSUM:
                RETURN_VERR_LDR_UNEXPECTED;

            /* dylib */
            case LC_LOAD_DYLIB:
            case LC_ID_DYLIB:
            case LC_LOAD_DYLINKER:
            case LC_ID_DYLINKER:
            case LC_PREBOUND_DYLIB:
            case LC_LOAD_WEAK_DYLIB & ~LC_REQ_DYLD:
            case LC_SUB_FRAMEWORK:
            case LC_SUB_UMBRELLA:
            case LC_SUB_CLIENT:
            case LC_SUB_LIBRARY:
                RETURN_VERR_LDR_UNEXPECTED;

            default:
                RETURN_VERR_BAD_EXE_FORMAT;
        }

        /* next */
        pCmd = (load_command_t *)((uintptr_t)pCmd + pCmd->cmdsize);
    }

    return VINF_SUCCESS;
}


/**
 * Loads the FAT and MACHO headers, noting down the relevant info.
 *
 * @returns IPRT status code.
 * @param   pKernel             The internal scratch data.
 */
static int rtR0DarwinMachKernelLoadFileHeaders(PRTR0DARWINKERNEL pKernel)
{
    uint32_t i;

    pKernel->offArch = 0;
    pKernel->cbArch  = 0;

    /*
     * Read the first bit of the file, parse the FAT if found there.
     */
    int rc = RTFileReadAt(pKernel->hFile, 0, pKernel->abBuf, sizeof(fat_header_t) + sizeof(fat_arch_t) * 16, NULL);
    if (RT_FAILURE(rc))
        return rc;

    fat_header_t   *pFat        = (fat_header *)pKernel->abBuf;
    fat_arch_t     *paFatArches = (fat_arch_t *)(pFat + 1);

    /* Correct FAT endian first. */
    if (pFat->magic == IMAGE_FAT_SIGNATURE_OE)
    {
        pFat->magic     = RT_BSWAP_U32(pFat->magic);
        pFat->nfat_arch = RT_BSWAP_U32(pFat->nfat_arch);
        i = RT_MIN(pFat->nfat_arch, 16);
        while (i-- > 0)
        {
            paFatArches[i].cputype    = RT_BSWAP_U32(paFatArches[i].cputype);
            paFatArches[i].cpusubtype = RT_BSWAP_U32(paFatArches[i].cpusubtype);
            paFatArches[i].offset     = RT_BSWAP_U32(paFatArches[i].offset);
            paFatArches[i].size       = RT_BSWAP_U32(paFatArches[i].size);
            paFatArches[i].align      = RT_BSWAP_U32(paFatArches[i].align);
        }
    }

    /* Lookup our architecture in the FAT. */
    if (pFat->magic == IMAGE_FAT_SIGNATURE)
    {
        if (pFat->nfat_arch > 16)
            RETURN_VERR_BAD_EXE_FORMAT;

        for (i = 0; i < pFat->nfat_arch; i++)
        {
            if (   paFatArches[i].cputype == MY_CPU_TYPE
                && paFatArches[i].cpusubtype == MY_CPU_SUBTYPE_ALL)
            {
                pKernel->offArch = paFatArches[i].offset;
                pKernel->cbArch  = paFatArches[i].size;
                if (!pKernel->cbArch)
                    RETURN_VERR_BAD_EXE_FORMAT;
                if (pKernel->offArch < sizeof(fat_header_t) + sizeof(fat_arch_t) * pFat->nfat_arch)
                    RETURN_VERR_BAD_EXE_FORMAT;
                if (pKernel->offArch + pKernel->cbArch <= pKernel->offArch)
                    RETURN_VERR_LDR_ARCH_MISMATCH;
                break;
            }
        }
        if (i >= pFat->nfat_arch)
            RETURN_VERR_LDR_ARCH_MISMATCH;
    }

    /*
     * Read the Mach-O header and validate it.
     */
    rc = RTFileReadAt(pKernel->hFile, pKernel->offArch, pKernel->abBuf, sizeof(MY_MACHO_HEADER), NULL);
    if (RT_FAILURE(rc))
        return rc;
    MY_MACHO_HEADER const *pHdr = (MY_MACHO_HEADER const *)pKernel->abBuf;
    if (pHdr->magic != MY_MACHO_MAGIC)
    {
        if (   pHdr->magic == IMAGE_MACHO32_SIGNATURE
            || pHdr->magic == IMAGE_MACHO32_SIGNATURE_OE
            || pHdr->magic == IMAGE_MACHO64_SIGNATURE
            || pHdr->magic == IMAGE_MACHO64_SIGNATURE_OE)
            RETURN_VERR_LDR_ARCH_MISMATCH;
        RETURN_VERR_BAD_EXE_FORMAT;
    }

    if (pHdr->cputype    != MY_CPU_TYPE)
        RETURN_VERR_LDR_ARCH_MISMATCH;
    if (pHdr->cpusubtype != MY_CPU_SUBTYPE_ALL)
        RETURN_VERR_LDR_ARCH_MISMATCH;
    if (pHdr->filetype   != MH_EXECUTE)
        RETURN_VERR_LDR_UNEXPECTED;
    if (pHdr->ncmds      < 4)
        RETURN_VERR_LDR_UNEXPECTED;
    if (pHdr->ncmds      > 256)
        RETURN_VERR_LDR_UNEXPECTED;
    if (pHdr->sizeofcmds <= pHdr->ncmds * sizeof(load_command_t))
        RETURN_VERR_LDR_UNEXPECTED;
    if (pHdr->sizeofcmds >= _1M)
        RETURN_VERR_LDR_UNEXPECTED;
    if (pHdr->flags & ~MH_VALID_FLAGS)
        RETURN_VERR_LDR_UNEXPECTED;

    pKernel->cLoadCmds  = pHdr->ncmds;
    pKernel->cbLoadCmds = pHdr->sizeofcmds;
    return VINF_SUCCESS;
}


static int rtR0DarwinMachKernelOpen(const char *pszMachKernel, PRTR0DARWINKERNEL *ppHandle)
{
    PRTR0DARWINKERNEL pKernel = (PRTR0DARWINKERNEL)RTMemAllocZ(sizeof(*pKernel));
    if (!pKernel)
        return VERR_NO_MEMORY;
    pKernel->hFile = NIL_RTFILE;

    int rc = RTFileOpen(&pKernel->hFile, pszMachKernel, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
        rc = rtR0DarwinMachKernelLoadFileHeaders(pKernel);
    if (RT_SUCCESS(rc))
        rc = rtR0DarwinMachKernelLoadCommands(pKernel);
    if (RT_SUCCESS(rc))
        rc = rtR0DarwinMachKernelLoadSymTab(pKernel);
    if (RT_SUCCESS(rc))
        rc = rtR0DarwinMachKernelCheckStandardSymbols(pKernel);

    rtR0DarwinMachKernelLoadDone(pKernel);
    if (RT_FAILURE(rc))
        rtR0DarwinMachKernelClose(pKernel);
    return rc;
}

