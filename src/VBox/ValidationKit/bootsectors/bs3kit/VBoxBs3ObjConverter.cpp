/* $Id$ */
/** @file
 * VirtualBox Validation Kit - Boot Sector 3 object file convert.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/x86.h>

#include <iprt/formats/elf64.h>
#include <iprt/formats/elf-amd64.h>
#include <iprt/formats/pecoff.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#if ARCH_BITS == 64 && !defined(RT_OS_WINDOWS) && !defined(RT_OS_DARWIN)
# define ELF_FMT_X64  "lx"
# define ELF_FMT_D64  "ld"
#else
# define ELF_FMT_X64  "llx"
# define ELF_FMT_D64  "lld"
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Verbosity level. */
static unsigned g_cVerbose = 0;


/**
 * Opens a file for binary reading or writing.
 *
 * @returns File stream handle.
 * @param   pszFile             The name of the file.
 * @param   fWrite              Whether to open for writing or reading.
 */
static FILE *openfile(const char *pszFile,  bool fWrite)
{
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    FILE *pFile = fopen(pszFile, fWrite ? "wb" : "rb");
#else
    FILE *pFile = fopen(pszFile, fWrite ? "w" : "r");
#endif
    if (!pFile)
        fprintf(stderr, "error: Failed to open '%s' for %s: %s (%d)\n",
                pszFile, fWrite ? "writing" : "reading", strerror(errno), errno);
    return pFile;
}


/**
 * Read the given file into memory.
 *
 * @returns true on success, false on failure.
 * @param   pszFile     The file to read.
 * @param   ppvFile     Where to return the memory.
 * @param   pcbFile     Where to return the size.
 */
static bool readfile(const char *pszFile, void **ppvFile, size_t *pcbFile)
{
    FILE *pFile = openfile(pszFile, false);
    if (pFile)
    {
        /*
         * Figure the size.
         */
        if (fseek(pFile, 0, SEEK_END) == 0)
        {
            long cbFile = ftell(pFile);
            if (cbFile > 0)
            {
                if (fseek(pFile, SEEK_SET, 0) == 0)
                {
                    /*
                     * Allocate and read content.
                     */
                    void *pvFile = malloc((size_t)cbFile);
                    if (pvFile)
                    {
                        if (fread(pvFile, cbFile, 1, pFile) == 1)
                        {
                            *ppvFile = pvFile;
                            *pcbFile = (size_t)cbFile;
                            fclose(pFile);
                            return true;
                        }
                        free(pvFile);
                        fprintf(stderr, "error: fread failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
                    }
                    else
                        fprintf(stderr, "error: failed to allocate %ld bytes of memory for '%s'\n", cbFile, pszFile);
                }
                else
                    fprintf(stderr, "error: fseek #2 failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
            }
            else
                fprintf(stderr, "error: ftell failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
        }
        else
            fprintf(stderr, "error: fseek #1 failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
        fclose(pFile);
    }
    return false;
}


/**
 * Write the given file into memory.
 *
 * @returns true on success, false on failure.
 * @param   pszFile     The file to write.
 * @param   pvFile      Where to return the memory.
 * @param   cbFile      Where to return the size.
 */
static bool writefile(const char *pszFile, void const *pvFile, size_t cbFile)
{
    remove(pszFile);

    int rc = -1;
    FILE *pFile = openfile(pszFile, true);
    if (pFile)
    {
        if (fwrite(pvFile, cbFile, 1, pFile) == 1)
        {
            fclose(pFile);
            return true;
        }
        fprintf(stderr, "error: fwrite failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
        fclose(pFile);
    }
    return false;
}


/**
 * Reports an error and returns false.
 *
 * @returns false
 * @param   pszFile             The filename.
 * @param   pszFormat           The message format string.
 * @param   ...                 Format arguments.
 */
static bool error(const char *pszFile, const char *pszFormat, ...)
{
    fprintf(stderr, "error: %s: ", pszFile);
    va_list va;
    va_start(va, pszFormat);
    vfprintf(stderr, pszFormat, va);
    va_end(va);
    return false;
}


/** AMD64 relocation type names for ELF. */
static const char * const g_apszElfAmd64RelTypes[] =
{
    "R_X86_64_NONE",
    "R_X86_64_64",
    "R_X86_64_PC32",
    "R_X86_64_GOT32",
    "R_X86_64_PLT32",
    "R_X86_64_COPY",
    "R_X86_64_GLOB_DAT",
    "R_X86_64_JMP_SLOT",
    "R_X86_64_RELATIVE",
    "R_X86_64_GOTPCREL",
    "R_X86_64_32",
    "R_X86_64_32S",
    "R_X86_64_16",
    "R_X86_64_PC16",
    "R_X86_64_8",
    "R_X86_64_PC8",
    "R_X86_64_DTPMOD64",
    "R_X86_64_DTPOFF64",
    "R_X86_64_TPOFF64",
    "R_X86_64_TLSGD",
    "R_X86_64_TLSLD",
    "R_X86_64_DTPOFF32",
    "R_X86_64_GOTTPOFF",
    "R_X86_64_TPOFF32",
};


static bool convertelf(const char *pszFile, uint8_t *pbFile, size_t cbFile)
{
    /*
     * Validate the header and our other expectations.
     */
    Elf64_Ehdr const *pEhdr = (Elf64_Ehdr const *)pbFile;
    if (   pEhdr->e_ident[EI_CLASS] != ELFCLASS64
        || pEhdr->e_ident[EI_DATA]  != ELFDATA2LSB
        || pEhdr->e_ehsize          != sizeof(Elf64_Ehdr)
        || pEhdr->e_shentsize       != sizeof(Elf64_Shdr)
        || pEhdr->e_version         != EV_CURRENT )
        return error(pszFile, "Unsupported ELF config\n");
    if (pEhdr->e_type != ET_REL)
        return error(pszFile, "Expected relocatable ELF file (e_type=%d)\n", pEhdr->e_type);
    if (pEhdr->e_machine != EM_X86_64)
        return error(pszFile, "Expected relocatable ELF file (e_type=%d)\n", pEhdr->e_machine);
    if (pEhdr->e_phnum != 0)
        return error(pszFile, "Expected e_phnum to be zero not %u\n", pEhdr->e_phnum);
    if (pEhdr->e_shnum < 2)
        return error(pszFile, "Expected e_shnum to be two or higher\n");
    if (pEhdr->e_shstrndx >= pEhdr->e_shnum || pEhdr->e_shstrndx == 0)
        return error(pszFile, "Bad e_shstrndx=%u (e_shnum=%u)\n", pEhdr->e_shstrndx, pEhdr->e_shnum);
    if (   pEhdr->e_shoff >= cbFile
        || pEhdr->e_shoff + pEhdr->e_shnum * sizeof(Elf64_Shdr) > cbFile)
        return error(pszFile, "Section table is outside the file (e_shoff=%#llx, e_shnum=%u, cbFile=%#llx)\n",
                     pEhdr->e_shstrndx, pEhdr->e_shnum, (uint64_t)cbFile);

    /*
     * Locate the section name string table.
     * We assume it's okay as we only reference it in verbose mode.
     */
    Elf64_Shdr const *paShdrs = (Elf64_Shdr const *)&pbFile[pEhdr->e_shoff];
    const char * pszStrTab = (const char *)&pbFile[paShdrs[pEhdr->e_shstrndx].sh_offset];

    /*
     * Work the section table.
     */
    for (uint32_t i = 1; i < pEhdr->e_shnum; i++)
    {
        if (g_cVerbose)
            printf("shdr[%u]: name=%#x '%s' type=%#x flags=%#" ELF_FMT_X64 " addr=%#" ELF_FMT_X64 " off=%#" ELF_FMT_X64 " size=%#" ELF_FMT_X64 "\n"
                   "          link=%u info=%#x align=%#" ELF_FMT_X64 " entsize=%#" ELF_FMT_X64 "\n",
                   i, paShdrs[i].sh_name, &pszStrTab[paShdrs[i].sh_name], paShdrs[i].sh_type, paShdrs[i].sh_flags,
                   paShdrs[i].sh_addr, paShdrs[i].sh_offset, paShdrs[i].sh_size,
                   paShdrs[i].sh_link, paShdrs[i].sh_info, paShdrs[i].sh_addralign, paShdrs[i].sh_entsize);
        if (paShdrs[i].sh_type == SHT_RELA)
        {
            if (paShdrs[i].sh_entsize != sizeof(Elf64_Rela))
                return error(pszFile, "Expected sh_entsize to be %u not %u for section #%u (%s)\n", (unsigned)sizeof(Elf64_Rela),
                             paShdrs[i].sh_entsize, i, &pszStrTab[paShdrs[i].sh_name]);
            uint32_t const cRelocs = paShdrs[i].sh_size / sizeof(Elf64_Rela);
            if (cRelocs * sizeof(Elf64_Rela) != paShdrs[i].sh_size)
                return error(pszFile, "Uneven relocation entry count in #%u (%s): sh_size=%#" ELF_FMT_X64 "\n", (unsigned)sizeof(Elf64_Rela),
                             paShdrs[i].sh_entsize, i, &pszStrTab[paShdrs[i].sh_name], paShdrs[i].sh_size);
            if (   paShdrs[i].sh_offset > cbFile
                || paShdrs[i].sh_size  >= cbFile
                || paShdrs[i].sh_offset + paShdrs[i].sh_size > cbFile)
                return error(pszFile, "The content of section #%u '%s' is outside the file (%#" ELF_FMT_X64 " LB %#" ELF_FMT_X64 ", cbFile=%#lx)\n",
                             i, &pszStrTab[paShdrs[i].sh_name], paShdrs[i].sh_offset, paShdrs[i].sh_size, (unsigned long)cbFile);
            Elf64_Rela *paRels = (Elf64_Rela *)&pbFile[paShdrs[i].sh_offset];
            for (uint32_t j = 0; j < cRelocs; j++)
            {
                uint8_t const bType = ELF64_R_TYPE(paRels[j].r_info);
                if (g_cVerbose > 1)
                    printf("%#018" ELF_FMT_X64 "  %#018" ELF_FMT_X64 " %s  %+" ELF_FMT_D64 "\n", paRels[j].r_offset, paRels[j].r_info,
                           bType < RT_ELEMENTS(g_apszElfAmd64RelTypes) ? g_apszElfAmd64RelTypes[bType] : "unknown", paRels[j].r_addend);

                /* Truncate 64-bit wide absolute relocations, ASSUMING that the high bits
                   are already zero and won't be non-zero after calculating the fixup value. */
                if (bType == R_X86_64_64)
                {
                    paRels[j].r_info &= ~(uint64_t)0xff;
                    paRels[j].r_info |= R_X86_64_32;
                }
            }
        }
        else if (paShdrs[i].sh_type == SHT_REL)
            return error(pszFile, "Did not expect SHT_REL sections (#%u '%s')\n", i, &pszStrTab[paShdrs[i].sh_name]);
    }
    return true;
}


/** AMD64 relocation type names for (Microsoft) COFF. */
static const char * const g_apszCoffAmd64RelTypes[] =
{
    "ABSOLUTE",
    "ADDR64",
    "ADDR32",
    "ADDR32NB",
    "REL32",
    "REL32_1",
    "REL32_2",
    "REL32_3",
    "REL32_4",
    "REL32_5",
    "SECTION",
    "SECREL",
    "SECREL7",
    "TOKEN",
    "SREL32",
    "PAIR",
    "SSPAN32"
};


static const char *coffGetSymbolName(PCIMAGE_SYMBOL pSym, const char *pchStrTab, char pszShortName[16])
{
    if (pSym->N.Name.Short != 0)
    {
        memcpy(pszShortName, pSym->N.ShortName, 8);
        pszShortName[8] = '\0';
        return pszShortName;
    }
    return pchStrTab + pSym->N.Name.Long;
}

static bool convertcoff(const char *pszFile, uint8_t *pbFile, size_t cbFile)
{
    /*
     * Validate the header and our other expectations.
     */
    PIMAGE_FILE_HEADER pHdr = (PIMAGE_FILE_HEADER)pbFile;
    if (pHdr->Machine != IMAGE_FILE_MACHINE_AMD64)
        return error(pszFile, "Expected IMAGE_FILE_MACHINE_AMD64 not %#x\n", pHdr->Machine);
    if (pHdr->SizeOfOptionalHeader != 0)
        return error(pszFile, "Expected SizeOfOptionalHeader to be zero, not %#x\n", pHdr->SizeOfOptionalHeader);
    if (pHdr->NumberOfSections == 0)
        return error(pszFile, "Expected NumberOfSections to be non-zero\n");
    uint32_t const cbHeaders = pHdr->NumberOfSections * sizeof(IMAGE_SECTION_HEADER) + sizeof(*pHdr);
    if (cbHeaders > cbFile)
        return error(pszFile, "Section table goes beyond the end of the of the file (cSections=%#x)\n", pHdr->NumberOfSections);
    if (pHdr->NumberOfSymbols)
    {
        if (   pHdr->PointerToSymbolTable >= cbFile
            || pHdr->NumberOfSymbols * (uint64_t)IMAGE_SIZE_OF_SYMBOL > cbFile)
            return error(pszFile, "Symbol table goes beyond the end of the of the file (cSyms=%#x, offFile=%#x)\n",
                         pHdr->NumberOfSymbols, pHdr->PointerToSymbolTable);
    }

    /* Dump the symbol table if verbose mode. */
    PIMAGE_SYMBOL paSymTab  = (PIMAGE_SYMBOL)&pbFile[pHdr->PointerToSymbolTable];
    const char   *pchStrTab = (const char *)&paSymTab[pHdr->NumberOfSymbols];
    char          szShortName[16];
    if (g_cVerbose > 2)
        for (uint32_t i = 0; i < pHdr->NumberOfSymbols; i++)
        {
            printf("sym[0x%02x]: sect=0x%04x value=0x%08x storageclass=0x%x name=%s\n",
                   i, paSymTab[i].SectionNumber, paSymTab[i].Value, paSymTab[i].StorageClass,
                   coffGetSymbolName(&paSymTab[i], pchStrTab, szShortName));
            i += paSymTab[i].NumberOfAuxSymbols;
        }

    /* Switch it to a x86 machine. */
    pHdr->Machine = IMAGE_FILE_MACHINE_I386;

    /*
     * Work the section table.
     */
    bool fRet = true;
    PIMAGE_SECTION_HEADER paShdrs = (PIMAGE_SECTION_HEADER)(pHdr + 1);
    for (uint32_t i = 0; i < pHdr->NumberOfSections; i++)
    {
        if (g_cVerbose)
            printf("shdr[%2u]:       rva=%#010x  cbVirt=%#010x '%-8.8s'\n"
                   "            offFile=%#010x  cbFile=%#010x\n"
                   "          offRelocs=%#010x cRelocs=%#010x\n"
                   "           offLines=%#010x  cLines=%#010x Characteristics=%#010x\n",
                   i, paShdrs[i].VirtualAddress, paShdrs[i].Misc.VirtualSize, paShdrs[i].Name,
                   paShdrs[i].PointerToRawData, paShdrs[i].SizeOfRawData,
                   paShdrs[i].PointerToRelocations, paShdrs[i].NumberOfRelocations,
                   paShdrs[i].PointerToLinenumbers, paShdrs[i].NumberOfLinenumbers, paShdrs[i].Characteristics);
        uint32_t cRelocs = paShdrs[i].NumberOfRelocations;
        if (cRelocs > 0)
        {
            if (   paShdrs[i].PointerToRelocations < cbHeaders
                || paShdrs[i].PointerToRelocations >= cbFile
                || paShdrs[i].PointerToRelocations + cRelocs * sizeof(IMAGE_RELOCATION) > cbFile)
                return error(pszFile, "Relocation beyond the end of the file or overlapping the headers (section #%u)\n", i);

            uint32_t const cbRawData = paShdrs[i].SizeOfRawData;
            if (   paShdrs[i].PointerToRawData < cbHeaders
                || paShdrs[i].PointerToRawData >= cbFile
                || paShdrs[i].PointerToRawData + cbRawData > cbFile)
                return error(pszFile, "Raw data beyond the end of the file or overlapping the headers (section #%u)\n", i);
            uint8_t *pbRawData = &pbFile[paShdrs[i].PointerToRawData];

            /* Is this a section which ends up in the binary? */
            bool const fInBinary = !(paShdrs[i].Characteristics & (IMAGE_SCN_LNK_REMOVE | IMAGE_SCN_LNK_INFO));
            bool const fIsPData  = fInBinary
                                && memcmp(paShdrs[i].Name, RT_STR_TUPLE(".pdata\0")) == 0;
            bool const fIsText   = fInBinary
                                && (paShdrs[i].Characteristics & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE));

            /* Whether we've seen any __ImageBase REL32 relocation that may later
               be used with array access using the SIB encoding and ADDR32NB. */
            bool fSeenImageBase = false;

            /*
             * Convert from AMD64 fixups to I386 ones, assuming 64-bit addresses
             * being fixed up doesn't need the high dword and that it's
             * appropriately initialized already.
             */
            PIMAGE_RELOCATION paRelocs = (PIMAGE_RELOCATION)&pbFile[paShdrs[i].PointerToRelocations];
            for (uint32_t j = 0; j < cRelocs; j++)
            {
                RTPTRUNION uLoc;
                uLoc.pu8 = paRelocs[j].u.VirtualAddress < cbRawData ? &pbRawData[paRelocs[j].u.VirtualAddress] : NULL;

                /* Print it. */
                if (g_cVerbose > 1)
                {
                    size_t off = printf("%#010x  %#010x", paRelocs[j].u.VirtualAddress, paRelocs[j].SymbolTableIndex);
                    switch (paRelocs[j].Type)
                    {
                        case IMAGE_REL_AMD64_ADDR64:
                            if (uLoc.pu64)
                                off += printf("  %#018" ELF_FMT_X64 "", *uLoc.pu64);
                            break;
                        case IMAGE_REL_AMD64_ADDR32:
                        case IMAGE_REL_AMD64_ADDR32NB:
                        case IMAGE_REL_AMD64_REL32:
                        case IMAGE_REL_AMD64_REL32_1:
                        case IMAGE_REL_AMD64_REL32_2:
                        case IMAGE_REL_AMD64_REL32_3:
                        case IMAGE_REL_AMD64_REL32_4:
                        case IMAGE_REL_AMD64_REL32_5:
                        case IMAGE_REL_AMD64_SECREL:
                            if (uLoc.pu32)
                                off += printf("  %#010x", *uLoc.pu32);
                            break;
                        case IMAGE_REL_AMD64_SECTION:
                            if (uLoc.pu16)
                                off += printf("  %#06x", *uLoc.pu16);
                            break;
                        case IMAGE_REL_AMD64_SECREL7:
                            if (uLoc.pu8)
                                off += printf("  %#04x", *uLoc.pu8);
                            break;
                    }
                    while (off < 36)
                        off += printf(" ");
                    printf(" %s %s\n",
                           paRelocs[j].Type < RT_ELEMENTS(g_apszCoffAmd64RelTypes)
                           ? g_apszCoffAmd64RelTypes[paRelocs[j].Type] : "unknown",
                           coffGetSymbolName(&paSymTab[paRelocs[j].SymbolTableIndex], pchStrTab, szShortName));
                }

                /* Convert it. */
                uint8_t uDir = IMAGE_REL_AMD64_ABSOLUTE;
                switch (paRelocs[j].Type)
                {
                    case IMAGE_REL_AMD64_ADDR64:
                    {
                        uint64_t uAddend = 0;
                        if (uLoc.pu64)
                        {
                            if (paRelocs[j].u.VirtualAddress + 8 > cbRawData)
                                return error(pszFile, "ADDR64 at %#x in section %u '%-8.8s' not fully in raw data\n",
                                             paRelocs[j].u.VirtualAddress, i, paShdrs[i].Name);
                            uAddend = *uLoc.pu64;
                        }
                        if (uAddend > _1G)
                            return error(pszFile, "ADDR64 with large addend (%#llx) at %#x in section %u '%-8.8s'\n",
                                         uAddend, paRelocs[j].u.VirtualAddress, i, paShdrs[i].Name);
                        paRelocs[j].Type = IMAGE_REL_I386_DIR32;
                        uDir = IMAGE_REL_AMD64_ADDR64;
                        break;
                    }

                    case IMAGE_REL_AMD64_REL32_1:
                    case IMAGE_REL_AMD64_REL32_2:
                    case IMAGE_REL_AMD64_REL32_3:
                    case IMAGE_REL_AMD64_REL32_4:
                    case IMAGE_REL_AMD64_REL32_5:
                    {
                        if (paRelocs[j].u.VirtualAddress + 4 > cbRawData)
                            return error(pszFile, "%s at %#x in section %u '%-8.8s' is not (fully) in raw data\n",
                                         g_apszCoffAmd64RelTypes[paRelocs[j].Type], paRelocs[j].u.VirtualAddress, i,
                                         paShdrs[i].Name);
                        *uLoc.pu32 += paRelocs[j].Type - IMAGE_REL_AMD64_REL32;
                        paRelocs[j].Type = IMAGE_REL_I386_REL32;
                        break;
                    }

                    /* These are 1:1 conversions: */
                    case IMAGE_REL_AMD64_ADDR32:
#if 1   /* Turns out this is special when wlink is doing DOS/BIOS binaries. */
/** @todo this still doesn't work for bs3-cmn-SelProtFar32ToFlat32.obj!! */
                        paRelocs[j].Type = IMAGE_REL_I386_ABSOLUTE; /* Note! Don't believe MS pecoff.doc, this works with wlink. */
#else
                        paRelocs[j].Type = IMAGE_REL_I386_DIR32;
                        uDir = IMAGE_REL_AMD64_ADDR32;
#endif
                        break;
                    case IMAGE_REL_AMD64_ADDR32NB:
                        if (fSeenImageBase && fIsText) /* This is voodoo. */
                            paRelocs[j].Type = IMAGE_REL_I386_ABSOLUTE; /* Note! Don't believe MS pecoff.doc, this works with wlink. */
                        else
                        {
                            paRelocs[j].Type = IMAGE_REL_I386_DIR32NB;
                            uDir = IMAGE_REL_AMD64_ADDR32NB;
                        }
                        break;
                    case IMAGE_REL_AMD64_REL32:
                        paRelocs[j].Type = IMAGE_REL_I386_REL32;

                        /* This is voodoo! */
                        if (   fIsText
                            && strcmp(coffGetSymbolName(&paSymTab[paRelocs[j].SymbolTableIndex], pchStrTab, szShortName),
                                      "__ImageBase") == 0)
                        {
                            if (   (uLoc.pu8[-1] & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5 /* disp32 + wrt */
                                && uLoc.pu8[-2] == 0x8d /* LEA */
                                && (uLoc.pu8[-3] & (0xf8 | X86_OP_REX_W)) == X86_OP_REX_W /* 64-bit reg */ )
                            {
                                if (*uLoc.pu32)
                                {
                                    error(pszFile, "__ImageBase fixup with disp %#x at rva=%#x in section #%u '%-8.8s'!\n",
                                          *uLoc.pu32, paRelocs[j].u.VirtualAddress, i, paShdrs[i].Name);
                                    fRet = false;
                                }

                                if (fSeenImageBase)
                                    return error(pszFile, "More than one __ImageBase fixup! 2nd at rva=%#x in section #%u '%-8.8s'\n",
                                                 paRelocs[j].u.VirtualAddress, i, paShdrs[i].Name);
                                if (g_cVerbose)
                                    printf("Applying __ImageBase hack at rva=%#x in section #%u '%-8.8s'\n",
                                           paRelocs[j].u.VirtualAddress, i, paShdrs[i].Name);

                                /* Convert it into a mov reg, dword 0. Leave the extra rex prefix, as it will be ignored. */
                                uint8_t iReg = (uLoc.pu8[-1] >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK;
                                uLoc.pu8[-1] = 0xb8 | iReg;
                                uLoc.pu8[-2] = X86_OP_REX_R & uLoc.pu8[-3];
                                fSeenImageBase = true;

                                /* Neutralize the fixup.
                                   Note! wlink takes the IMAGE_REL_I386_ABSOLUTE fixups seriously, so we cannot use that
                                         to disable it, so instead we have to actually remove it from the fixup table. */
                                cRelocs--;
                                if (j != cRelocs)
                                    memmove(&paRelocs[j], &paRelocs[j + 1], (cRelocs - j) * sizeof(paRelocs[j]));
                                paShdrs[i].NumberOfRelocations = (uint16_t)cRelocs;
                                j--;
                            }
                            else
                            {
                                error(pszFile, "__ImageBase fixup that isn't a recognized LEA at rva=%#x in section #%u '%-8.8s'!\n",
                                      paRelocs[j].u.VirtualAddress, i, paShdrs[i].Name);
                                fRet = false;
                            }
                        }
                        break;
                    case IMAGE_REL_AMD64_SECTION:
                        paRelocs[j].Type = IMAGE_REL_I386_SECTION;
                        break;
                    case IMAGE_REL_AMD64_SECREL:
                        paRelocs[j].Type = IMAGE_REL_I386_SECREL;
                        break;
                    case IMAGE_REL_AMD64_SECREL7:
                        paRelocs[j].Type = IMAGE_REL_I386_SECREL7;
                        break;
                    case IMAGE_REL_AMD64_ABSOLUTE:
                        paRelocs[j].Type = IMAGE_REL_I386_ABSOLUTE;
                        /* Turns out wlink takes this seriously, so it usage must be checked out. */
                        error(pszFile, "ABSOLUTE fixup at rva=%#x in section #%u '%-8.8s'\n",
                              paRelocs[j].u.VirtualAddress, i, paShdrs[i].Name);
                        fRet = false;
                        break;

                    default:
                        return error(pszFile, "Unsupported fixup type %#x (%s) at rva=%#x in section #%u '%-8.8s'\n",
                                     paRelocs[j].Type,
                                     paRelocs[j].Type < RT_ELEMENTS(g_apszCoffAmd64RelTypes)
                                     ? g_apszCoffAmd64RelTypes[paRelocs[j].Type] : "unknown",
                                     paRelocs[j].u.VirtualAddress, i, paShdrs[i].Name);
                }

                /*
                 * Error no absolute fixup that we care about. We continue so
                 * the developer can get the full story before failing.
                 */
                if (   fInBinary
                    && !fIsPData
                    && uDir != IMAGE_REL_AMD64_ABSOLUTE)
                {
                    error(pszFile, "%s at %#x in section %u '%-8.8s': wlink won't get this right\n",
                          g_apszCoffAmd64RelTypes[uDir], paRelocs[j].u.VirtualAddress, i, paShdrs[i].Name);
                    fRet = false;
                }
            }
        }
    }
    return fRet;
}


/** @name Selected OMF record types.
 * @{ */
#define PUBDEF  UINT8_C(0x90)
#define LPUBDEF UINT8_C(0xb6)
#define THEADR  UINT8_C(0x80)
#define EXTDEF  UINT8_C(0x8c)
#define SEGDEF  UINT8_C(0x98)
#define LNAMES  UINT8_C(0x96)
#define GRPDEF  UINT8_C(0x9a)
#define REC32   UINT8_C(0x01) /**< Flag indicating 32-bit record. */
/** @} */

/** Watcom intrinsics we need to modify so we can mix 32-bit and 16-bit
 * code, since the 16 and 32 bit compilers share several names.
 * The names are length prefixed.
 */
static const char * const g_apszExtDefRenames[] =
{
    "\x05" "__I4D",
    "\x05" "__I4M",
    "\x05" "__I8D",
    "\x06" "__I8DQ",
    "\x07" "__I8DQE",
    "\x06" "__I8DR",
    "\x07" "__I8DRE",
    "\x06" "__I8LS",
    "\x05" "__I8M",
    "\x06" "__I8ME",
    "\x06" "__I8RS",
    "\x05" "__PIA",
    "\x05" "__PIS",
    "\x05" "__PTC",
    "\x05" "__PTS",
    "\x05" "__U4D",
    "\x05" "__U4M",
    "\x05" "__U8D",
    "\x06" "__U8DQ",
    "\x07" "__U8DQE",
    "\x06" "__U8DR",
    "\x07" "__U8DRE",
    "\x06" "__U8LS",
    "\x05" "__U8M",
    "\x06" "__U8ME",
    "\x06" "__U8RS",
};

/**
 * Renames references to intrinsic helper functions so they won't clash between
 * 32-bit and 16-bit code.
 *
 * @returns true / false.
 * @param   pszFile     File name for complaining.
 * @param   pbFile      Pointer to the file content.
 * @param   cbFile      Size of the file content.
 */
static bool convertomf(const char *pszFile, uint8_t *pbFile, size_t cbFile, const char **papchLNames, uint32_t cLNamesMax)
{
    uint32_t        cLNames = 0;
    uint32_t        cExtDefs = 0;
    uint32_t        cPubDefs = 0;
    bool            fProbably32bit = false;
    uint32_t        off = 0;

    while (off + 3 < cbFile)
    {
        uint8_t     bRecType = pbFile[off];
        uint16_t    cbRec    = RT_MAKE_U16(pbFile[off + 1], pbFile[off + 2]);
        if (g_cVerbose > 2)
            printf( "%#07x: type=%#04x len=%#06x\n", off, bRecType, cbRec);
        if (off + cbRec > cbFile)
            return error(pszFile, "Invalid record length at %#x: %#x (cbFile=%#lx)\n", off, cbRec, (unsigned long)cbFile);

        if (bRecType & REC32)
            fProbably32bit = true;

        uint32_t offRec = 0;
        uint8_t *pbRec  = &pbFile[off + 3];
#define OMF_CHECK_RET(a_cbReq, a_Name) /* Not taking the checksum into account, so we're good with 1 or 2 byte fields. */ \
            if (offRec + (a_cbReq) <= cbRec) {/*likely*/} \
            else return error(pszFile, "Malformed " #a_Name "! off=%#x offRec=%#x cbRec=%#x cbNeeded=%#x line=%d\n", \
                              off, offRec, cbRec, (a_cbReq), __LINE__)
        switch (bRecType)
        {
            /*
             * Scan external definitions for intrinsics needing mangling.
             */
            case EXTDEF:
            {
                while (offRec + 1 < cbRec)
                {
                    uint8_t cch = pbRec[offRec++];
                    OMF_CHECK_RET(cch, EXTDEF);
                    char *pchName = (char *)&pbRec[offRec];
                    offRec += cch;

                    OMF_CHECK_RET(2, EXTDEF);
                    uint16_t idxType = pbRec[offRec++];
                    if (idxType & 0x80)
                        idxType = ((idxType & 0x7f) << 8) | pbRec[offRec++];

                    if (g_cVerbose > 2)
                        printf("  EXTDEF [%u]: %-*.*s type=%#x\n", cExtDefs, cch, cch, pchName, idxType);
                    else if (g_cVerbose > 0)
                        printf("              U %-*.*s\n", cch, cch, pchName);

                    /* Look for g_apszExtDefRenames entries that requires changing. */
                    if (   cch >= 5
                        && cch <= 7
                        && pchName[0] == '_'
                        && pchName[1] == '_'
                        && (   pchName[2] == 'U'
                            || pchName[2] == 'I'
                            || pchName[2] == 'P')
                        && (   pchName[3] == '4'
                            || pchName[3] == '8'
                            || pchName[3] == 'I'
                            || pchName[3] == 'T') )
                    {
                        uint32_t i = RT_ELEMENTS(g_apszExtDefRenames);
                        while (i-- > 0)
                            if (   cch == (uint8_t)g_apszExtDefRenames[i][0]
                                && memcmp(&g_apszExtDefRenames[i][1], pchName, cch) == 0)
                            {
                                pchName[0] = fProbably32bit ? '?' : '_';
                                pchName[1] = '?';
                                break;
                            }
                    }

                    cExtDefs++;
                }
                break;
            }

            /*
             * Record LNAME records, scanning for FLAT.
             */
            case LNAMES:
            {
                while (offRec + 1 < cbRec)
                {
                    uint8_t cch = pbRec[offRec];
                    if (offRec + 1 + cch >= cbRec)
                        return error(pszFile, "Invalid LNAME string length at %#x+3+%#x: %#x (cbFile=%#lx)\n",
                                     off, offRec, cch, (unsigned long)cbFile);
                    if (cLNames + 1 >= cLNamesMax)
                        return error(pszFile, "Too many LNAME strings\n");

                    if (g_cVerbose > 2)
                        printf("  LNAME[%u]: %-*.*s\n", cLNames, cch, cch, &pbRec[offRec + 1]);

                    papchLNames[cLNames++] = (const char *)&pbRec[offRec];
                    if (cch == 4 && memcmp(&pbRec[offRec + 1], "FLAT", 4) == 0)
                        fProbably32bit = true;

                    offRec += cch + 1;
                }
                break;
            }

            /*
             * Display public names if -v is specified.
             */
            case PUBDEF:
            case PUBDEF | REC32:
            case LPUBDEF:
            case LPUBDEF | REC32:
            {
                if (g_cVerbose > 0)
                {
                    char const  chType  = bRecType == PUBDEF || bRecType == (PUBDEF | REC32) ? 'T' : 't';
                    const char *pszRec = "LPUBDEF";
                    if (chType == 'T')
                        pszRec++;

                    OMF_CHECK_RET(2, [L]PUBDEF);
                    uint16_t idxGrp = pbRec[offRec++];
                    if (idxGrp & 0x80)
                        idxGrp = ((idxGrp & 0x7f) << 8) | pbRec[offRec++];

                    OMF_CHECK_RET(2, [L]PUBDEF);
                    uint16_t idxSeg = pbRec[offRec++];
                    if (idxSeg & 0x80)
                        idxSeg = ((idxSeg & 0x7f) << 8) | pbRec[offRec++];

                    uint16_t uFrameBase = 0;
                    if (idxSeg == 0)
                    {
                        OMF_CHECK_RET(2, [L]PUBDEF);
                        uFrameBase = RT_MAKE_U16(pbRec[offRec], pbRec[offRec + 1]);
                        offRec += 2;
                    }
                    if (g_cVerbose > 2)
                        printf("  %s: idxGrp=%#x idxSeg=%#x uFrameBase=%#x\n", pszRec, idxGrp, idxSeg, uFrameBase);
                    uint16_t const uSeg = idxSeg ? idxSeg : uFrameBase;

                    while (offRec + 1 < cbRec)
                    {
                        uint8_t cch = pbRec[offRec++];
                        OMF_CHECK_RET(cch, [L]PUBDEF);
                        const char *pchName = (const char *)&pbRec[offRec];
                        offRec += cch;

                        uint32_t offSeg;
                        if (bRecType & REC32)
                        {
                            OMF_CHECK_RET(4, [L]PUBDEF);
                            offSeg = RT_MAKE_U32_FROM_U8(pbRec[offRec], pbRec[offRec + 1], pbRec[offRec + 2], pbRec[offRec + 3]);
                            offRec += 4;
                        }
                        else
                        {
                            OMF_CHECK_RET(2, [L]PUBDEF);
                            offSeg = RT_MAKE_U16(pbRec[offRec], pbRec[offRec + 1]);
                            offRec += 2;
                        }

                        OMF_CHECK_RET(2, [L]PUBDEF);
                        uint16_t idxType = pbRec[offRec++];
                        if (idxType & 0x80)
                            idxType = ((idxType & 0x7f) << 8) | pbRec[offRec++];

                        if (g_cVerbose > 2)
                            printf("  %s[%u]: off=%#010x type=%#x %-*.*s\n", pszRec, cPubDefs, offSeg, idxType, cch, cch, pchName);
                        else if (g_cVerbose > 0)
                            printf("%04x:%08x %c %-*.*s\n", uSeg, offSeg, chType, cch, cch, pchName);

                        cPubDefs++;
                    }
                }
                break;
            }
        }

        /* advance */
        off += cbRec + 3;
    }
    return true;
}


/**
 * Does the convertion using convertelf and convertcoff.
 *
 * @returns exit code (0 on success, non-zero on failure)
 * @param   pszFile     The file to convert.
 */
static int convertit(const char *pszFile)
{
    void  *pvFile;
    size_t cbFile;
    if (readfile(pszFile, &pvFile, &cbFile))
    {
        bool fRc = false;
        uint8_t *pbFile = (uint8_t *)pvFile;
        if (   cbFile > sizeof(Elf64_Ehdr)
            && pbFile[0] == ELFMAG0
            && pbFile[1] == ELFMAG1
            && pbFile[2] == ELFMAG2
            && pbFile[3] == ELFMAG3)
            fRc = convertelf(pszFile, pbFile, cbFile);
        else if (   cbFile > sizeof(IMAGE_FILE_HEADER)
                 && RT_MAKE_U16(pbFile[0], pbFile[1]) == IMAGE_FILE_MACHINE_AMD64
                 &&   RT_MAKE_U16(pbFile[2], pbFile[3]) * sizeof(IMAGE_SECTION_HEADER) + sizeof(IMAGE_FILE_HEADER)
                    < cbFile
                 && RT_MAKE_U16(pbFile[2], pbFile[3]) > 0)
            fRc = convertcoff(pszFile, pbFile, cbFile);
        else if (   cbFile >= 8
                 && pbFile[0] == THEADR
                 && RT_MAKE_U16(pbFile[1], pbFile[2]) < cbFile)
        {
            const char **papchLNames = (const char **)calloc(sizeof(*papchLNames), _32K);
            fRc = convertomf(pszFile, pbFile, cbFile, papchLNames, _32K);
            free(papchLNames);
        }
        else
            fprintf(stderr, "error: Don't recognize format of '%s' (%#x %#x %#x %#x, cbFile=%lu)\n",
                    pszFile, pbFile[0], pbFile[1], pbFile[2], pbFile[3], (unsigned long)cbFile);
        if (fRc)
            fRc = writefile(pszFile, pvFile, cbFile);
        free(pvFile);
        if (fRc)
            return 0;
    }
    return 1;
}


int main(int argc, char **argv)
{
    int rcExit = 0;

    /*
     * Scan the arguments.
     */
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            const char *pszOpt = &argv[i][1];
            if (*pszOpt == '-')
            {
                /* Convert long options to short ones. */
                pszOpt--;
                if (!strcmp(pszOpt, "--verbose"))
                    pszOpt = "v";
                else if (!strcmp(pszOpt, "--version"))
                    pszOpt = "V";
                else if (!strcmp(pszOpt, "--help"))
                    pszOpt = "h";
                else
                {
                    fprintf(stderr, "syntax errro: Unknown options '%s'\n", pszOpt);
                    return 2;
                }
            }

            /* Process the list of short options. */
            while (*pszOpt)
            {
                switch (*pszOpt++)
                {
                    case 'v':
                        g_cVerbose++;
                        break;

                    case 'V':
                        printf("%s\n", "$Revision$");
                        return 0;

                    case '?':
                    case 'h':
                        printf("usage: %s [options] -o <output> <input1> [input2 ... [inputN]]\n",
                               argv[0]);
                        return 0;
                }
            }
        }
        else
        {
            /*
             * File to convert.  Do the job right away.
             */
            rcExit = convertit(argv[i]);
            if (rcExit != 0)
                break;
        }
    }

    return rcExit;
}


