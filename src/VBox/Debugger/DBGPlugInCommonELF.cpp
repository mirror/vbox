/* $Id$ */
/** @file
 * DBGPlugInCommonELF - Common code for dealing with ELF images.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF ///@todo add new log group.
#include "DBGPlugInCommonELF.h"
#include <VBox/dbgf.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/assert.h>



/**
 * Common 32-bit ELF module parser.
 *
 * It takes the essential bits of the ELF module (elf header, section headers,
 * symbol table and string table), and inserts/updates the module and symbols.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM             The VM handle.
 * @param   pszModName      The module name.
 * @param   pszFilename     The filename. optional.
 * @param   fFlags          Flags.
 * @param   pEhdr           Pointer to the ELF header.
 * @param   paShdrs         Pointer to the section headers. The caller must verify that
 *                          the e_shnum member of the ELF header is within the bounds of
 *                          this table. The caller should also adjust the section addresses
 *                          so these correspond to actual load addresses.
 * @param   paSyms          Pointer to the symbol table.
 * @param   cMaxSyms        The maximum number of symbols paSyms may hold. This isn't
 *                          the exact count, it's just a cap for avoiding SIGSEGVs
 *                          and general corruption.
 * @param   pbStrings       Pointer to the string table.
 * @param   cbMaxStrings    The size of the memory pbStrings points to. This doesn't
 *                          have to match the string table size exactly, it's just to
 *                          avoid SIGSEGV when a bad string index is encountered.
 */
int DBGDiggerCommonParseElf32Mod(PVM pVM, const char *pszModName, const char *pszFilename, uint32_t fFlags,
                                 Elf32_Ehdr const *pEhdr, Elf32_Shdr const *paShdrs,
                                 Elf32_Sym const *paSyms, size_t cMaxSyms,
                                 char const *pbStrings, size_t cbMaxStrings)
{
    AssertPtrReturn(pVM, VERR_INVALID_POINTER);
    AssertPtrReturn(pszModName, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~DBG_DIGGER_ELF_MASK), VERR_INVALID_PARAMETER);
    AssertReturn((fFlags & (DBG_DIGGER_ELF_FUNNY_SHDRS | DBG_DIGGER_ELF_ADJUST_SYM_VALUE))
                 != (DBG_DIGGER_ELF_FUNNY_SHDRS | DBG_DIGGER_ELF_ADJUST_SYM_VALUE), VERR_INVALID_PARAMETER);
    AssertPtrReturn(paShdrs, VERR_INVALID_POINTER);
    AssertPtrReturn(paSyms, VERR_INVALID_POINTER);
    AssertPtrReturn(pbStrings, VERR_INVALID_POINTER);

    /*
     * Validate the ELF header.
     */
    if (    pEhdr->e_ident[EI_MAG0] != ELFMAG0
        ||  pEhdr->e_ident[EI_MAG1] != ELFMAG1
        ||  pEhdr->e_ident[EI_MAG2] != ELFMAG2
        ||  pEhdr->e_ident[EI_MAG3] != ELFMAG3)
        return VERR_INVALID_EXE_SIGNATURE;
    if (pEhdr->e_ident[EI_CLASS] != ELFCLASS32)
        return VERR_LDRELF_MACHINE;

    if (pEhdr->e_ident[EI_DATA] != ELFDATA2LSB)
        return VERR_LDRELF_ODD_ENDIAN;
    if (pEhdr->e_ident[EI_VERSION] != EV_CURRENT)
        return VERR_LDRELF_VERSION;
    if (pEhdr->e_version != EV_CURRENT)
        return VERR_LDRELF_VERSION;
    if (pEhdr->e_ehsize != sizeof(*pEhdr))
        return VERR_BAD_EXE_FORMAT;

    if (    pEhdr->e_machine != EM_386
        &&  pEhdr->e_machine != EM_486)
        return VERR_LDRELF_MACHINE;

    if (    pEhdr->e_type != ET_DYN
        &&  pEhdr->e_type != ET_REL
        &&  pEhdr->e_type != ET_EXEC) //??
        return VERR_BAD_EXE_FORMAT;
    if (    pEhdr->e_phentsize != sizeof(Elf32_Phdr)
        &&  pEhdr->e_phentsize) //??
        return VERR_BAD_EXE_FORMAT;
    if (pEhdr->e_shentsize != sizeof(Elf32_Shdr))
        return VERR_BAD_EXE_FORMAT;
    if (pEhdr->e_shentsize != sizeof(Elf32_Shdr))
        return VERR_BAD_EXE_FORMAT;
    if (ASMMemIsAll8(&pEhdr->e_ident[EI_PAD], EI_NIDENT - EI_PAD, 0) != NULL) //??
        return VERR_BAD_EXE_FORMAT;

    /*
     * Validate the section headers, finding the string
     * and symbol table headers while at it.
     */
    const Elf32_Shdr *pSymShdr = NULL;
    const Elf32_Shdr *pStrShdr = NULL;
    for (unsigned iSh = fFlags & DBG_DIGGER_ELF_FUNNY_SHDRS ? 1 : 0; iSh < pEhdr->e_shnum; iSh++)
    {
        /* Minimal validation. */
        if (paShdrs[iSh].sh_link >= pEhdr->e_shnum)
            return VERR_BAD_EXE_FORMAT;

        /* Is it the symbol table?*/
        if (paShdrs[iSh].sh_type == SHT_SYMTAB)
        {
            if (pSymShdr)
                return VERR_LDRELF_MULTIPLE_SYMTABS;
            pSymShdr = &paShdrs[iSh];
            if (pSymShdr->sh_entsize != sizeof(Elf32_Sym))
                return VERR_BAD_EXE_FORMAT;
            pStrShdr = &paShdrs[paShdrs[iSh].sh_link];
        }
    }

    /*
     * Validate the symbol table.
     */
    uint32_t const cbStrings = pStrShdr ? pStrShdr->sh_size : cbMaxStrings;
    uint32_t const cSyms = pSymShdr
                         ? RT_MIN(cMaxSyms, pSymShdr->sh_size / sizeof(Elf32_Sym))
                         : cMaxSyms;
    for (uint32_t iSym = 1; iSym < cSyms; iSym++)
    {
        if (paSyms[iSym].st_name >= cbStrings)
            return VERR_LDRELF_INVALID_SYMBOL_NAME_OFFSET;
        if (    !(fFlags & DBG_DIGGER_ELF_FUNNY_SHDRS)
            &&  paSyms[iSym].st_shndx >= pEhdr->e_shnum
            &&  paSyms[iSym].st_shndx != SHN_UNDEF
            &&  (   paSyms[iSym].st_shndx < SHN_LORESERVE
                 /*|| paSyms[iSym].st_shndx > SHN_HIRESERVE*/
                 || ELF32_ST_BIND(paSyms[iSym].st_info) == STB_GLOBAL
                 || ELF32_ST_BIND(paSyms[iSym].st_info) == STB_WEAK))
            return VERR_BAD_EXE_FORMAT;
    }

    /*
     * Create a module / update it.
     */
    /** @todo debug modules - the funny shdrs (solaris) are going to be cool here... */

    /*
     * Add all relevant symbols in the module
     */
    for (uint32_t iSym = 1; iSym < cSyms; iSym++)
    {
        /* Undefined symbols are not exports, they are imports. */
        if (    paSyms[iSym].st_shndx != SHN_UNDEF
            &&  (   ELF32_ST_BIND(paSyms[iSym].st_info) == STB_GLOBAL
                 || ELF32_ST_BIND(paSyms[iSym].st_info) == STB_WEAK))
        {
            /* Get the symbol name. */
            if (paSyms[iSym].st_name >= cbMaxStrings)
                continue;
            const char *pszSymbol = pbStrings + paSyms[iSym].st_name;
            if (!*pszSymbol)
                continue;

            /* Calc the address (value) and size. */
            RTGCUINTPTR Size = paSyms[iSym].st_size;
            RTGCUINTPTR Address = paSyms[iSym].st_value;
            if (paSyms[iSym].st_shndx == SHN_ABS)
                /* absolute symbols are not subject to any relocation. */;
            else if (fFlags & DBG_DIGGER_ELF_ADJUST_SYM_VALUE)
            {
                if (paSyms[iSym].st_shndx < pEhdr->e_shnum)
                    AssertFailed(/*impossible*/);
                /* relative to the section. */
                Address += paShdrs[paSyms[iSym].st_shndx].sh_addr;
            }

            DBGFR3SymbolAdd(pVM, 0, Address, Size, pszSymbol);
            Log(("%RGv %RGv %s::%s\n", Address, Size, pszModName, pszSymbol));
        }
        /*else: silently ignore */
    }

    return VINF_SUCCESS;
}

