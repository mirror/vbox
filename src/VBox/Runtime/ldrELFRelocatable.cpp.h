/* $Id$ */
/** @file
 * innotek Portable Runtime - Binary Image Loader, Template for ELF Relocatable Images.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if ELF_MODE == 32
#define RTLDRELF_NAME(name) rtldrELF32##name
#define RTLDRELF_SUFF(name) name##32
#define RTLDRELF_MID(pre,suff) pre##32##suff
#define FMT_ELF_ADDR    "%08RX32"
#define FMT_ELF_HALF    "%04RX16"
#define FMT_ELF_OFF     "%08RX32"
#define FMT_ELF_SIZE    "%08RX32"
#define FMT_ELF_SWORD   "%RI32"
#define FMT_ELF_WORD    "%08RX32"
#define FMT_ELF_XWORD   "%08RX32"
#define FMT_ELF_SXWORD  "%RI32"

#elif ELF_MODE == 64
#define RTLDRELF_NAME(name) rtldrELF64##name
#define RTLDRELF_SUFF(name) name##64
#define RTLDRELF_MID(pre,suff) pre##64##suff
#define FMT_ELF_ADDR    "%016RX64"
#define FMT_ELF_HALF    "%04RX16"
#define FMT_ELF_SHALF   "%RI16"
#define FMT_ELF_OFF     "%016RX64"
#define FMT_ELF_SIZE    "%016RX64"
#define FMT_ELF_SWORD   "%RI32"
#define FMT_ELF_WORD    "%08RX32"
#define FMT_ELF_XWORD   "%016RX64"
#define FMT_ELF_SXWORD  "%RI64"
#endif

#define Elf_Ehdr            RTLDRELF_MID(Elf,_Ehdr)
#define Elf_Phdr            RTLDRELF_MID(Elf,_Phdr)
#define Elf_Shdr            RTLDRELF_MID(Elf,_Shdr)
#define Elf_Sym             RTLDRELF_MID(Elf,_Sym)
#define Elf_Rel             RTLDRELF_MID(Elf,_Rel)
#define Elf_Rela            RTLDRELF_MID(Elf,_Rela)
#define Elf_Nhdr            RTLDRELF_MID(Elf,_Nhdr)
#define Elf_Dyn             RTLDRELF_MID(Elf,_Dyn)
#define Elf_Addr            RTLDRELF_MID(Elf,_Addr)
#define Elf_Half            RTLDRELF_MID(Elf,_Half)
#define Elf_Off             RTLDRELF_MID(Elf,_Off)
#define Elf_Size            RTLDRELF_MID(Elf,_Size)
#define Elf_Sword           RTLDRELF_MID(Elf,_Sword)
#define Elf_Word            RTLDRELF_MID(Elf,_Word)

#define RTLDRMODELF         RTLDRELF_MID(RTLDRMODELF,)
#define PRTLDRMODELF        RTLDRELF_MID(PRTLDRMODELF,)

#define ELF_R_SYM(info)     RTLDRELF_MID(ELF,_R_SYM)(info)
#define ELF_R_TYPE(info)    RTLDRELF_MID(ELF,_R_TYPE)(info)
#define ELF_R_INFO(sym, type) RTLDRELF_MID(ELF,_R_INFO)(sym, type)

#define ELF_ST_BIND(info)   RTLDRELF_MID(ELF,_ST_BIND)(info)



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The ELF loader structure.
 */
typedef struct RTLDRMODELF
{
    /** Core module structure. */
    RTLDRMODINTERNAL        Core;
    /** Pointer to the reader instance. */
    PRTLDRREADER            pReader;
    /** Pointer to readonly mapping of the image bits.
     * This mapping is provided by the pReader. */
    const void             *pvBits;

    /** The ELF header. */
    Elf_Ehdr                Ehdr;
    /** Pointer to our copy of the section headers.
     * The virtual addresses in this array is the 0 based assignments we've given the image.
     * Not valid if the image is DONE. */
    Elf_Shdr               *paShdrs;
    /** The size of the loaded image. */
    size_t                  cbImage;

    /** The symbol section index. */
    unsigned                iSymSh;
    /** Number of symbols in the table. */
    unsigned                cSyms;
    /** Pointer to symbol table within RTLDRMODELF::pvBits. */
    const Elf_Sym          *paSyms;

    /** The string section index. */
    unsigned                iStrSh;
    /** Size of the string table. */
    unsigned                cbStr;
    /** Pointer to string table within RTLDRMODELF::pvBits. */
    const char             *pStr;
} RTLDRMODELF, *PRTLDRMODELF;


/**
 * Maps the image bits into memory and resolve pointers into it.
 *
 * @returns iprt status code.
 * @param   pModElf         The ELF loader module instance data.
 * @param   fNeedsBits      Set if we actually need the pvBits member.
 *                          If we don't, we can simply read the string and symbol sections, thus saving memory.
 */
static int RTLDRELF_NAME(MapBits)(PRTLDRMODELF pModElf, bool fNeedsBits)
{
    NOREF(fNeedsBits);
    if (pModElf->pvBits)
        return VINF_SUCCESS;
    int rc = pModElf->pReader->pfnMap(pModElf->pReader, &pModElf->pvBits);
    if (RT_SUCCESS(rc))
    {
        const uint8_t *pu8 = (const uint8_t *)pModElf->pvBits;
        pModElf->paSyms = (const Elf_Sym *)(pu8 + pModElf->paShdrs[pModElf->iSymSh].sh_offset);
        pModElf->pStr   =    (const char *)(pu8 + pModElf->paShdrs[pModElf->iStrSh].sh_offset);
    }
    return rc;
}


/**
 * Get the symbol and symbol value.
 *
 * @returns iprt status code.
 * @param   pModElf         The ELF loader module instance data.
 * @param   BaseAddr        The base address which the module is being fixedup to.
 * @param   pfnGetImport    The callback function to use to resolve imports (aka unresolved externals).
 * @param   pvUser          User argument to pass to the callback.
 * @param   iSym            The symbol to get.
 * @param   ppSym           Where to store the symbol pointer on success. (read only)
 * @param   pSymValue       Where to store the symbol value on success.
 */
static int RTLDRELF_NAME(Symbol)(PRTLDRMODELF pModElf, Elf_Addr BaseAddr, PFNRTLDRIMPORT pfnGetImport, void *pvUser,
                                 Elf_Size iSym, const Elf_Sym **ppSym, Elf_Addr *pSymValue)
{
    /*
     * Validate and find the symbol.
     */
    if (iSym >= pModElf->cSyms)
    {
        AssertMsgFailed(("iSym=%d is an invalid symbol index!\n", iSym));
        return VERR_LDRELF_INVALID_SYMBOL_INDEX;
    }
    const Elf_Sym *pSym = &pModElf->paSyms[iSym];
    *ppSym = pSym;

    if (pSym->st_name >= pModElf->cbStr)
    {
        AssertMsgFailed(("iSym=%d st_name=%d str sh_size=%d\n", iSym, pSym->st_name, pModElf->cbStr));
        return VERR_LDRELF_INVALID_SYMBOL_NAME_OFFSET;
    }
    const char *pszName = ELF_STR(pModElf, pSym->st_name);

    /*
     * Determin the symbol value.
     *
     * Symbols needs different treatment depending on which section their are in.
     * Undefined and absolute symbols goes into special non-existing sections.
     */
    switch (pSym->st_shndx)
    {
        /*
         * Undefined symbol, needs resolving.
         *
         * Since ELF has no generic concept of importing from specific module (the OS/2 ELF format
         * has but that's a OS extension and only applies to programs and dlls), we'll have to ask
         * the resolver callback to do a global search.
         */
        case SHN_UNDEF:
        {
            /* Try to resolve the symbol. */
            RTUINTPTR Value;
            int rc = pfnGetImport(&pModElf->Core, "", pszName, ~0, &Value, pvUser);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("Failed to resolve '%s' rc=%Rrc\n", pszName, rc));
                return rc;
            }
            *pSymValue = (Elf_Addr)Value;
            if ((RTUINTPTR)*pSymValue != Value)
            {
                AssertMsgFailed(("Symbol value overflowed! '%s'\n", pszName));
                return VERR_SYMBOL_VALUE_TOO_BIG;
            }

            Log2(("rtldrELF: #%-3d - UNDEF " FMT_ELF_ADDR " '%s'\n", iSym, *pSymValue, pszName));
            break;
        }

        /*
         * Absolute symbols needs no fixing since they are, well, absolute.
         */
        case SHN_ABS:
            *pSymValue = pSym->st_value;
            Log2(("rtldrELF: #%-3d - ABS   " FMT_ELF_ADDR " '%s'\n", iSym, *pSymValue, pszName));
            break;

        /*
         * All other symbols are adressed relative to their section and need to be fixed up.
         */
        default:
            if (pSym->st_shndx >= pModElf->Ehdr.e_shnum)
            {
                /* what about common symbols? */
                AssertMsg(pSym->st_shndx < pModElf->Ehdr.e_shnum,
                          ("iSym=%d st_shndx=%d e_shnum=%d pszName=%s\n", iSym, pSym->st_shndx, pModElf->Ehdr.e_shnum, pszName));
                return VERR_BAD_EXE_FORMAT;
            }
            *pSymValue = pSym->st_value + pModElf->paShdrs[pSym->st_shndx].sh_addr + BaseAddr;
            Log2(("rtldrELF: #%-3d - %5d " FMT_ELF_ADDR " '%s'\n", iSym, pSym->st_shndx, *pSymValue, pszName));
            break;
    }

    return VINF_SUCCESS;
}


/**
 * Applies the fixups for a sections.
 *
 * @returns iprt status code.
 * @param   pModElf         The ELF loader module instance data.
 * @param   BaseAddr        The base address which the module is being fixedup to.
 * @param   pfnGetImport    The callback function to use to resolve imports (aka unresolved externals).
 * @param   pvUser          User argument to pass to the callback.
 * @param   SecAddr         The section address. This is the address the relocations are relative to.
 * @param   cbSec           The section size. The relocations must be inside this.
 * @param   pu8SecBaseR     Where we read section bits from.
 * @param   pu8SecBaseW     Where we write section bits to.
 * @param   pvRelocs        Pointer to where we read the relocations from.
 * @param   cbRelocs        Size of the relocations.
 */
static int RTLDRELF_NAME(RelocateSection)(PRTLDRMODELF pModElf, Elf_Addr BaseAddr, PFNRTLDRIMPORT pfnGetImport, void *pvUser,
                                          const Elf_Addr SecAddr, Elf_Size cbSec, const uint8_t *pu8SecBaseR, uint8_t *pu8SecBaseW,
                                          const void *pvRelocs, Elf_Size cbRelocs)
{
    /*
     * Iterate the relocations.
     * The relocations are stored in an array of Elf32_Rel records and covers the entire relocation section.
     */
    const Elf_Reloc  *paRels = (const Elf_Reloc *)pvRelocs;
    const unsigned   iRelMax = (unsigned)(cbRelocs / sizeof(paRels[0]));
    AssertMsgReturn(iRelMax == cbRelocs / sizeof(paRels[0]), (FMT_ELF_SIZE "\n", cbRelocs / sizeof(paRels[0])), VERR_IMAGE_TOO_BIG);
    for (unsigned iRel = 0; iRel < iRelMax; iRel++)
    {
        /*
         * Get the symbol.
         */
        const Elf_Sym  *pSym;
        Elf_Addr        SymValue = 0; /* shut up gcc-4 */
        int rc = RTLDRELF_NAME(Symbol)(pModElf, BaseAddr, pfnGetImport, pvUser, ELF_R_SYM(paRels[iRel].r_info), &pSym, &SymValue);
        if (RT_FAILURE(rc))
            return rc;

        Log3(("rtldrELF: " FMT_ELF_ADDR " %02x %06x - " FMT_ELF_ADDR " %3d %02x %s\n",
              paRels[iRel].r_offset, ELF_R_TYPE(paRels[iRel].r_info), (unsigned)ELF_R_SYM(paRels[iRel].r_info),
              SymValue, (unsigned)pSym->st_shndx, pSym->st_info, ELF_STR(pModElf, pSym->st_name)));

        /*
         * Apply the fixup.
         */
        AssertMsgReturn(paRels[iRel].r_offset < cbSec, (FMT_ELF_ADDR " " FMT_ELF_SIZE "\n", paRels[iRel].r_offset, cbSec), VERR_LDRELF_INVALID_RELOCATION_OFFSET);
#if   ELF_MODE == 32
        const Elf_Addr *pAddrR = (const Elf_Addr *)(pu8SecBaseR + paRels[iRel].r_offset);    /* Where to read the addend. */
#endif
        Elf_Addr       *pAddrW =       (Elf_Addr *)(pu8SecBaseW + paRels[iRel].r_offset);    /* Where to write the fixup. */
        switch (ELF_R_TYPE(paRels[iRel].r_info))
        {
#if   ELF_MODE == 32
            /*
             * Absolute addressing.
             */
            case R_386_32:
            {
                const Elf_Addr Value = SymValue + *pAddrR;
                *(uint32_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR": R_386_32   Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                break;
            }

            /*
             * PC relative addressing.
             */
            case R_386_PC32:
            {
                const Elf_Addr SourceAddr = SecAddr + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                const Elf_Addr Value = SymValue + *(uint32_t *)pAddrR - SourceAddr;
                *(uint32_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR": R_386_PC32 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SourceAddr, Value, SymValue));
                break;
            }

            /* ignore */
            case R_386_NONE:
                break;

#elif ELF_MODE == 64

            /*
             * Absolute addressing
             */
            case R_X86_64_64:
            {
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend;
                *(uint64_t *)pAddrW = Value;
                Log4((FMT_ELF_ADDR": R_X86_64_64   Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                break;
            }

            /*
             * Trunacated 32-bit value (zero-extendedable to the 64-bit value).
             */
            case R_X86_64_32:
            {
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend;
                *(uint32_t *)pAddrW = (uint32_t)Value;
                Log4((FMT_ELF_ADDR": R_X86_64_32   Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(uint32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG);
                break;
            }

            /*
             * Truncated 32-bit value (sign-extendedable to the 64-bit value).
             */
            case R_X86_64_32S:
            {
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend;
                *(int32_t *)pAddrW = (int32_t)Value;
                Log4((FMT_ELF_ADDR": R_X86_64_32S  Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SecAddr + paRels[iRel].r_offset + BaseAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG); /** @todo check the sign-extending here. */
                break;
            }

            /*
             * PC relative addressing.
             */
            case R_X86_64_PC32:
            {
                const Elf_Addr SourceAddr = SecAddr + paRels[iRel].r_offset + BaseAddr; /* Where the source really is. */
                const Elf_Addr Value = SymValue + paRels[iRel].r_addend - SourceAddr;
                *(int32_t *)pAddrW = (int32_t)Value;
                Log4((FMT_ELF_ADDR": R_X86_64_PC32 Value=" FMT_ELF_ADDR " SymValue=" FMT_ELF_ADDR "\n",
                      SourceAddr, Value, SymValue));
                AssertMsgReturn((Elf_Addr)*(int32_t *)pAddrW == Value, ("Value=" FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG); /** @todo check the sign-extending here. */
                break;
            }

            /* ignore */
            case R_X86_64_NONE:
                break;
#endif

            default:
                AssertMsgFailed(("Unknown relocation type: %d (iRel=%d iRelMax=%d)\n",
                                 ELF_R_TYPE(paRels[iRel].r_info), iRel, iRelMax));
                return VERR_LDRELF_RELOCATION_NOT_SUPPORTED;
        }
    }

    return VINF_SUCCESS;
}



/** @copydoc RTLDROPS::pfnClose */
static DECLCALLBACK(int) RTLDRELF_NAME(Close)(PRTLDRMODINTERNAL pMod)
{
    PRTLDRMODELF pModElf = (PRTLDRMODELF)pMod;

    if (pModElf->paShdrs)
    {
        RTMemFree(pModElf->paShdrs);
        pModElf->paShdrs = NULL;
    }

    if (pModElf->pReader)
    {
        pModElf->pReader->pfnDestroy(pModElf->pReader);
        pModElf->pReader = NULL;
    }

    pModElf->pvBits = NULL;

    return VINF_SUCCESS;
}


/** @copydoc RTLDROPS::Done */
static DECLCALLBACK(int) RTLDRELF_NAME(Done)(PRTLDRMODINTERNAL pMod)
{
    //PRTLDRMODELF pModElf = (PRTLDRMODELF)pMod;
    /** @todo  Have to think more about this .... */
    return -1;
}


/** @copydoc RTLDROPS::EnumSymbols */
static DECLCALLBACK(int) RTLDRELF_NAME(EnumSymbols)(PRTLDRMODINTERNAL pMod, unsigned fFlags, const void *pvBits, RTUINTPTR BaseAddress,
                                                    PFNRTLDRENUMSYMS pfnCallback, void *pvUser)
{
    PRTLDRMODELF pModElf = (PRTLDRMODELF)pMod;

    /*
     * Validate the input.
     */
    Elf_Addr BaseAddr = (Elf_Addr)BaseAddress;
    AssertMsgReturn((RTUINTPTR)BaseAddr == BaseAddress, ("#RTptr", BaseAddress), VERR_IMAGE_BASE_TOO_HIGH);

    /*
     * Make sure we've got the string and symbol tables. (We don't need the pvBits.)
     */
    int rc = RTLDRELF_NAME(MapBits)(pModElf, false);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Enumerate the symbol table.
     */
    const Elf_Sym  *paSyms = pModElf->paSyms;
    unsigned        cSyms  = pModElf->cSyms;
    for (unsigned iSym = 1; iSym < cSyms; iSym++)
    {
        /*
         * Skip imports (undefined).
         */
        if (paSyms[iSym].st_shndx != SHN_UNDEF)
        {
            /*
             * Calc value and get name.
             */
            Elf_Addr Value;
            if (paSyms[iSym].st_shndx == SHN_ABS)
                /* absolute symbols are not subject to any relocation. */
                Value = paSyms[iSym].st_value;
            else if (paSyms[iSym].st_shndx < pModElf->Ehdr.e_shnum)
                /* relative to the section. */
                Value = BaseAddr + paSyms[iSym].st_value + pModElf->paShdrs[paSyms[iSym].st_shndx].sh_addr;
            else
            {
                AssertMsgFailed(("Arg! paSyms[%u].st_shndx=" FMT_ELF_HALF "\n", iSym, paSyms[iSym].st_shndx));
                return VERR_BAD_EXE_FORMAT;
            }
            const char *pszName = ELF_STR(pModElf, paSyms[iSym].st_name);
            if (    (pszName && *pszName)
                &&  (   (fFlags & RTLDR_ENUM_SYMBOL_FLAGS_ALL)
                     || ELF_ST_BIND(paSyms[iSym].st_info) == STB_GLOBAL)
               )
            {
                /*
                 * Call back.
                 */
                AssertMsgReturn(Value == (RTUINTPTR)Value, (FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG);
                int rc = pfnCallback(pMod, pszName, ~0, (RTUINTPTR)Value, pvUser);
                if (rc)
                    return rc;
            }
        }
    }

    return VINF_SUCCESS;
}


/** @copydoc RTLDROPS::GetImageSize */
static DECLCALLBACK(size_t) RTLDRELF_NAME(GetImageSize)(PRTLDRMODINTERNAL pMod)
{
    PRTLDRMODELF pModElf = (PRTLDRMODELF)pMod;

    return pModElf->cbImage;
}


/** @copydoc RTLDROPS::GetBits */
static DECLCALLBACK(int) RTLDRELF_NAME(GetBits)(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR BaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    PRTLDRMODELF pModElf = (PRTLDRMODELF)pMod;

    /*
     * Load the bits into pvBits.
     */
    const Elf_Shdr *paShdrs = pModElf->paShdrs;
    for (unsigned iShdr = 0; iShdr < pModElf->Ehdr.e_shnum; iShdr++)
    {
        if (paShdrs[iShdr].sh_flags & SHF_ALLOC)
        {
            AssertMsgReturn((size_t)paShdrs[iShdr].sh_size == (size_t)paShdrs[iShdr].sh_size, (FMT_ELF_SIZE "\n", paShdrs[iShdr].sh_size), VERR_IMAGE_TOO_BIG);
            switch (paShdrs[iShdr].sh_type)
            {
                case SHT_NOBITS:
                    memset((uint8_t *)pvBits + paShdrs[iShdr].sh_addr, 0, (size_t)paShdrs[iShdr].sh_size);
                    break;

                case SHT_PROGBITS:
                default:
                {
                    int rc = pModElf->pReader->pfnRead(pModElf->pReader, (uint8_t *)pvBits + paShdrs[iShdr].sh_addr,
                                                       (size_t)paShdrs[iShdr].sh_size, paShdrs[iShdr].sh_offset);
                    if (RT_FAILURE(rc))
                    {
                        Log(("RTLdrELF: %s: Read error when reading " FMT_ELF_SIZE " bytes at " FMT_ELF_OFF ", iShdr=%d\n",
                             pModElf->pReader->pfnLogName(pModElf->pReader),
                             paShdrs[iShdr].sh_size, paShdrs[iShdr].sh_offset, iShdr));
                        return rc;
                    }
                }
            }
        }
    }

    /*
     * Relocate the image.
     */
    return pModElf->Core.pOps->pfnRelocate(pMod, pvBits, BaseAddress, ~(RTUINTPTR)0, pfnGetImport, pvUser);
}


/** @copydoc RTLDROPS::Relocate */
static DECLCALLBACK(int) RTLDRELF_NAME(Relocate)(PRTLDRMODINTERNAL pMod, void *pvBits, RTUINTPTR NewBaseAddress, RTUINTPTR OldBaseAddress, PFNRTLDRIMPORT pfnGetImport, void *pvUser)
{
    PRTLDRMODELF pModElf = (PRTLDRMODELF)pMod;

    /*
     * Validate the input.
     */
    Elf_Addr BaseAddr = (Elf_Addr)NewBaseAddress;
    AssertMsgReturn((RTUINTPTR)BaseAddr == NewBaseAddress, ("#RTptr", NewBaseAddress), VERR_IMAGE_BASE_TOO_HIGH);

    /*
     * Map the image bits if not already done and setup pointer into it.
     */
    int rc = RTLDRELF_NAME(MapBits)(pModElf, true);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Iterate the sections looking for interesting SHT_REL[A] sections.
     * SHT_REL[A] sections have the section index of the section they contain fixups
     * for in the sh_info member.
     */
    const Elf_Shdr *paShdrs = pModElf->paShdrs;
#ifdef LOG_ENABLED
    const char     *pszLogName = pModElf->pReader->pfnLogName(pModElf->pReader);
    Log2(("rtLdrElf: %s: Fixing up image\n", pszLogName));
#endif
    for (unsigned iShdr = 0; iShdr < pModElf->Ehdr.e_shnum; iShdr++)
    {
        const Elf_Shdr *pShdrRel = &paShdrs[iShdr];

        /*
         * Skip sections without interest to us.
         */
#if ELF_MODE == 32
        if (pShdrRel->sh_type != SHT_REL)
#else
        if (pShdrRel->sh_type != SHT_RELA)
#endif
            continue;
        if (pShdrRel->sh_info >= pModElf->Ehdr.e_shnum)
            continue;
        const Elf_Shdr *pShdr = &paShdrs[pShdrRel->sh_info]; /* the section to fixup. */
        if (!(pShdr->sh_flags & SHF_ALLOC))
            continue;

        /*
         * Relocate the section.
         */
        Log2(("rtldrELF: %s: Relocation records for #%d [%s] (sh_info=%d sh_link=%d) found in #%d [%s] (sh_info=%d sh_link=%d)\n",
              pszLogName, (int)pShdrRel->sh_info, ELF_STR(pModElf, pShdr->sh_name), (int)pShdr->sh_info, (int)pShdr->sh_link,
              iShdr, ELF_STR(pModElf, pShdrRel->sh_name), (int)pShdrRel->sh_info, (int)pShdrRel->sh_link));

        /** @todo Make RelocateSection a function pointer so we can select the one corresponding to the machine when opening the image. */
        rc = RTLDRELF_NAME(RelocateSection)(pModElf, BaseAddr, pfnGetImport, pvUser,
                                            pShdr->sh_addr,
                                            pShdr->sh_size,
                                            (const uint8_t *)pModElf->pvBits + pShdr->sh_offset,
                                            (uint8_t *)pvBits + pShdr->sh_addr,
                                            (const uint8_t *)pModElf->pvBits + pShdrRel->sh_offset,
                                            pShdrRel->sh_size);
        if (RT_FAILURE(rc))
            return rc;
    }
    return VINF_SUCCESS;
}


/** @copydoc RTLDROPS::pfnGetSymbolEx */
static DECLCALLBACK(int) RTLDRELF_NAME(GetSymbolEx)(PRTLDRMODINTERNAL pMod, const void *pvBits, RTUINTPTR BaseAddress, const char *pszSymbol, RTUINTPTR *pValue)
{
    PRTLDRMODELF pModElf = (PRTLDRMODELF)pMod;
    NOREF(pvBits);

    /*
     * Validate the input.
     */
    Elf_Addr BaseAddr = (Elf_Addr)BaseAddress;
    AssertMsgReturn((RTUINTPTR)BaseAddr == BaseAddress, ("#RTptr", BaseAddress), VERR_IMAGE_BASE_TOO_HIGH);

    /*
     * Map the image bits if not already done and setup pointer into it.
     */
    int rc = RTLDRELF_NAME(MapBits)(pModElf, true);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Calc all kinds of pointers before we start iterating the symbol table.
     */
    const char         *pStr  = pModElf->pStr;
    const Elf_Sym    *paSyms = pModElf->paSyms;
    unsigned            cSyms = pModElf->cSyms;
    for (unsigned iSym = 1; iSym < cSyms; iSym++)
    {
        /* Undefined symbols are not exports, they are imports. */
	if (    paSyms[iSym].st_shndx != SHN_UNDEF
            &&  (   ELF_ST_BIND(paSyms[iSym].st_info) == STB_GLOBAL
                 || ELF_ST_BIND(paSyms[iSym].st_info) == STB_WEAK))
	{
            /* Validate the name string and try match with it. */
            if (paSyms[iSym].st_name < pModElf->cbStr)
            {
                if (!strcmp(pszSymbol, pStr + paSyms[iSym].st_name))
                {
                    /* matched! */
                    Elf_Addr Value;
                    if (paSyms[iSym].st_shndx == SHN_ABS)
                        /* absolute symbols are not subject to any relocation. */
                        Value = paSyms[iSym].st_value;
                    else if (paSyms[iSym].st_shndx < pModElf->Ehdr.e_shnum)
                        /* relative to the section. */
                        Value = BaseAddr + paSyms[iSym].st_value + pModElf->paShdrs[paSyms[iSym].st_shndx].sh_addr;
                    else
                    {
                        AssertMsgFailed(("Arg. paSyms[iSym].st_shndx=%d\n", paSyms[iSym].st_shndx));
                        return VERR_BAD_EXE_FORMAT;
                    }
                    AssertMsgReturn(Value == (RTUINTPTR)Value, (FMT_ELF_ADDR "\n", Value), VERR_SYMBOL_VALUE_TOO_BIG);
                    *pValue = (RTUINTPTR)Value;
                    return VINF_SUCCESS;
                }
            }
            else
            {
                AssertMsgFailed(("String outside string table! iSym=%d paSyms[iSym].st_name=%#x\n", iSym, paSyms[iSym].st_name));
                return VERR_LDRELF_INVALID_SYMBOL_NAME_OFFSET;
            }
	}
    }

    return VERR_SYMBOL_NOT_FOUND;
}



/**
 * The ELF module operations.
 */
static RTLDROPS RTLDRELF_MID(s_rtldrElf,Ops) =
{
#if   ELF_MODE == 32
    "elf32",
#elif ELF_MODE == 64
    "elf64",
#endif
    RTLDRELF_NAME(Close),
    NULL, /* Get Symbol */
    RTLDRELF_NAME(Done),
    RTLDRELF_NAME(EnumSymbols),
    /* ext: */
    RTLDRELF_NAME(GetImageSize),
    RTLDRELF_NAME(GetBits),
    RTLDRELF_NAME(Relocate),
    RTLDRELF_NAME(GetSymbolEx),
    0
};



/**
 * Validates the ELF header.
 *
 * @returns iprt status code.
 * @param   pEhdr       Pointer to the ELF header.
 * @param   pszLogName  The log name.
 * @param   cbRawImage  The size of the raw image.
 */
static int RTLDRELF_NAME(ValidateElfHeader)(const Elf_Ehdr *pEhdr, const char *pszLogName, uint64_t cbRawImage)
{
    Log3(("RTLdrELF:     e_ident: %.*Rhxs\n"
          "RTLdrELF:      e_type: " FMT_ELF_HALF "\n"
          "RTLdrELF:   e_version: " FMT_ELF_HALF "\n"
          "RTLdrELF:     e_entry: " FMT_ELF_ADDR "\n"
          "RTLdrELF:     e_phoff: " FMT_ELF_OFF  "\n"
          "RTLdrELF:     e_shoff: " FMT_ELF_OFF  "\n"
          "RTLdrELF:     e_flags: " FMT_ELF_WORD "\n"
          "RTLdrELF:    e_ehsize: " FMT_ELF_HALF "\n"
          "RTLdrELF: e_phentsize: " FMT_ELF_HALF "\n"
          "RTLdrELF:     e_phnum: " FMT_ELF_HALF "\n"
          "RTLdrELF: e_shentsize: " FMT_ELF_HALF "\n"
          "RTLdrELF:     e_shnum: " FMT_ELF_HALF "\n"
          "RTLdrELF:  e_shstrndx: " FMT_ELF_HALF "\n",
          ELEMENTS(pEhdr->e_ident), &pEhdr->e_ident[0], pEhdr->e_type, pEhdr->e_version,
          pEhdr->e_entry, pEhdr->e_phoff, pEhdr->e_shoff,pEhdr->e_flags, pEhdr->e_ehsize, pEhdr->e_phentsize,
          pEhdr->e_phnum, pEhdr->e_shentsize, pEhdr->e_shnum, pEhdr->e_shstrndx));

    if (    pEhdr->e_ident[EI_MAG0] != ELFMAG0
        ||  pEhdr->e_ident[EI_MAG1] != ELFMAG1
        ||  pEhdr->e_ident[EI_MAG2] != ELFMAG2
        ||  pEhdr->e_ident[EI_MAG3] != ELFMAG3
       )
    {
        Log(("RTLdrELF: %s: Invalid ELF magic (%.*Rhxs)\n", pszLogName, sizeof(pEhdr->e_ident), pEhdr->e_ident));
        return VERR_BAD_EXE_FORMAT;
    }
    if (pEhdr->e_ident[EI_CLASS] != RTLDRELF_SUFF(ELFCLASS))
    {
        Log(("RTLdrELF: %s: Invalid ELF class (%.*Rhxs)\n", pszLogName, sizeof(pEhdr->e_ident), pEhdr->e_ident));
        return VERR_BAD_EXE_FORMAT;
    }
    if (pEhdr->e_ident[EI_DATA] != ELFDATA2LSB)
    {
        Log(("RTLdrELF: %s: ELF endian %x is unsupported\n", pEhdr->e_ident[EI_DATA]));
        return VERR_LDRELF_ODD_ENDIAN;
    }
    if (pEhdr->e_version != EV_CURRENT)
    {
        Log(("RTLdrELF: %s: ELF version %x is unsupported\n", pEhdr->e_version));
        return VERR_LDRELF_VERSION;
    }

    if (sizeof(Elf_Ehdr) != pEhdr->e_ehsize)
    {
        Log(("RTLdrELF: %s: Elf header e_ehsize is %d expected %d!\n",
             pszLogName, pEhdr->e_ehsize, sizeof(Elf_Ehdr)));
        return VERR_BAD_EXE_FORMAT;
    }
    if (    sizeof(Elf_Phdr) != pEhdr->e_phentsize
        &&  (    pEhdr->e_phnum != 0
             ||  pEhdr->e_type == ET_DYN))
    {
        Log(("RTLdrELF: %s: Elf header e_phentsize is %d expected %d!\n",
             pszLogName, pEhdr->e_phentsize, sizeof(Elf_Phdr)));
        return VERR_BAD_EXE_FORMAT;
    }
    if (sizeof(Elf_Shdr) != pEhdr->e_shentsize)
    {
        Log(("RTLdrELF: %s: Elf header e_shentsize is %d expected %d!\n",
             pszLogName, pEhdr->e_shentsize, sizeof(Elf_Shdr)));
        return VERR_BAD_EXE_FORMAT;
    }

    switch (pEhdr->e_type)
    {
        case ET_REL:
            break;
        case ET_EXEC:
            Log(("RTLdrELF: %s: Executable images are not supported yet!\n", pszLogName));
            return VERR_LDRELF_EXEC;
        case ET_DYN:
            Log(("RTLdrELF: %s: Dynamic images are not supported yet!\n", pszLogName));
            return VERR_LDRELF_DYN;
        default:
            Log(("RTLdrELF: %s: image type %#x is not supported!\n", pszLogName, pEhdr->e_type));
            return VERR_BAD_EXE_FORMAT;
    }

    switch (pEhdr->e_machine)
    {
#if   ELF_MODE == 32
        case EM_386:
        case EM_486:
#elif ELF_MODE == 64
        case EM_X86_64:
#endif
            break;
        default:
            Log(("RTLdrELF: %s: machine type %u is not supported!\n", pEhdr->e_machine));
            return VERR_LDRELF_MACHINE;
    }

    if (    pEhdr->e_phoff < pEhdr->e_ehsize
        &&  !(pEhdr->e_phoff && pEhdr->e_phnum)
        &&  pEhdr->e_phnum)
    {
        Log(("RTLdrELF: %s: The program headers overlap with the ELF header! e_phoff=" FMT_ELF_OFF "\n",
             pszLogName, pEhdr->e_phoff));
        return VERR_BAD_EXE_FORMAT;
    }
    if (    pEhdr->e_phoff + pEhdr->e_phnum * pEhdr->e_phentsize > cbRawImage
        ||  pEhdr->e_phoff + pEhdr->e_phnum * pEhdr->e_phentsize < pEhdr->e_phoff)
    {
        Log(("RTLdrELF: %s: The program headers extends beyond the file! e_phoff=" FMT_ELF_OFF " e_phnum=" FMT_ELF_HALF "\n",
             pszLogName, pEhdr->e_phoff, pEhdr->e_phnum));
        return VERR_BAD_EXE_FORMAT;
    }


    if (    pEhdr->e_shoff < pEhdr->e_ehsize
        &&  !(pEhdr->e_shoff && pEhdr->e_shnum))
    {
        Log(("RTLdrELF: %s: The section headers overlap with the ELF header! e_shoff=" FMT_ELF_OFF "\n",
             pszLogName, pEhdr->e_shoff));
        return VERR_BAD_EXE_FORMAT;
    }
    if (    pEhdr->e_shoff + pEhdr->e_shnum * pEhdr->e_shentsize > cbRawImage
        ||  pEhdr->e_shoff + pEhdr->e_shnum * pEhdr->e_shentsize < pEhdr->e_shoff)
    {
        Log(("RTLdrELF: %s: The section headers extends beyond the file! e_shoff=" FMT_ELF_OFF " e_shnum=" FMT_ELF_HALF "\n",
             pszLogName, pEhdr->e_shoff, pEhdr->e_shnum));
        return VERR_BAD_EXE_FORMAT;
    }

    return VINF_SUCCESS;
}

/**
 * Gets the section header name.
 *
 * @returns pszName.
 * @param   pReader         The loader reader instance.
 * @param   pEhdr           The elf header.
 * @param   offName         The offset of the section header name.
 * @param   pszName         Where to store the name.
 * @param   cbName          The size of the buffer pointed to by pszName.
 */
const char *RTLDRELF_NAME(GetSHdrName)(PRTLDRMODELF pModElf, Elf_Word offName, char *pszName, size_t cbName)
{
    RTFOFF off = pModElf->paShdrs[pModElf->Ehdr.e_shstrndx].sh_offset + offName;
    int rc = pModElf->pReader->pfnRead(pModElf->pReader, pszName, cbName - 1, off);
    if (RT_FAILURE(rc))
    {
        /* read by for byte. */
        for (unsigned i = 0; i < cbName; i++, off++)
        {
            int rc = pModElf->pReader->pfnRead(pModElf->pReader, pszName + i, 1, off);
            if (RT_FAILURE(rc))
            {
                pszName[i] = '\0';
                break;
            }
        }
    }

    pszName[cbName - 1] = '\0';
    return pszName;
}


/**
 * Validates a section header.
 *
 * @returns iprt status code.
 * @param   pModElf     Pointer to the module structure.
 * @param   iShdr       The index of section header which should be validated.
 *                      The section headers are found in the pModElf->paShdrs array.
 * @param   pszLogName  The log name.
 * @param   cbRawImage  The size of the raw image.
 */
static int RTLDRELF_NAME(ValidateSectionHeader)(PRTLDRMODELF pModElf, unsigned iShdr, const char *pszLogName, RTFOFF cbRawImage)
{
    const Elf_Shdr *pShdr = &pModElf->paShdrs[iShdr];
    char szSectionName[80]; NOREF(szSectionName);
    Log3(("RTLdrELF: Section Header #%d:\n"
          "RTLdrELF:      sh_name: " FMT_ELF_WORD " - %s\n"
          "RTLdrELF:      sh_type: " FMT_ELF_WORD " (%s)\n"
          "RTLdrELF:     sh_flags: " FMT_ELF_XWORD "\n"
          "RTLdrELF:      sh_addr: " FMT_ELF_ADDR "\n"
          "RTLdrELF:    sh_offset: " FMT_ELF_OFF "\n"
          "RTLdrELF:      sh_size: " FMT_ELF_XWORD "\n"
          "RTLdrELF:      sh_link: " FMT_ELF_WORD "\n"
          "RTLdrELF:      sh_info: " FMT_ELF_WORD "\n"
          "RTLdrELF: sh_addralign: " FMT_ELF_XWORD "\n"
          "RTLdrELF:   sh_entsize: " FMT_ELF_XWORD "\n",
          iShdr,
          pShdr->sh_name, RTLDRELF_NAME(GetSHdrName)(pModElf, pShdr->sh_name, szSectionName, sizeof(szSectionName)),
          pShdr->sh_type, rtldrElfGetShdrType(pShdr->sh_type), pShdr->sh_flags, pShdr->sh_addr,
          pShdr->sh_offset, pShdr->sh_size, pShdr->sh_link, pShdr->sh_info, pShdr->sh_addralign,
          pShdr->sh_entsize));

    if (pShdr->sh_link >= pModElf->Ehdr.e_shnum)
    {
        Log(("RTLdrELF: %s: Shdr #%d: sh_link (%d) is beyond the end of the section table (%d)!\n",
             pszLogName, iShdr, pShdr->sh_link, pModElf->Ehdr.e_shnum));
        return VERR_BAD_EXE_FORMAT;
    }

    switch (pShdr->sh_type)
    {
        /** @todo find specs and check up which sh_info fields indicates section table entries */
        case 12301230:
            if (pShdr->sh_info >= pModElf->Ehdr.e_shnum)
            {
                Log(("RTLdrELF: %s: Shdr #%d: sh_info (%d) is beyond the end of the section table (%d)!\n",
                     pszLogName, iShdr, pShdr->sh_link, pModElf->Ehdr.e_shnum));
                return VERR_BAD_EXE_FORMAT;
            }
            break;

        case SHT_NULL:
        case SHT_PROGBITS:
        case SHT_SYMTAB:
        case SHT_STRTAB:
        case SHT_RELA:
        case SHT_HASH:
        case SHT_DYNAMIC:
        case SHT_NOTE:
        case SHT_NOBITS:
        case SHT_REL:
        case SHT_SHLIB:
        case SHT_DYNSYM:
            /*
             * For these types sh_info doesn't have any special meaning, or anything which
             * we need/can validate now.
             */
            break;


        default:
            Log(("RTLdrELF: %s: Warning, unknown type %d!\n", pszLogName, pShdr->sh_type));
            break;
    }

    if (    pShdr->sh_type != SHT_NOBITS
        &&  pShdr->sh_size)
    {
        RTFOFF offEnd = pShdr->sh_offset + pShdr->sh_size;
        if (    offEnd > cbRawImage
            ||  offEnd < (RTFOFF)pShdr->sh_offset)
        {
            Log(("RTLdrELF: %s: Shdr #%d: sh_offset (" FMT_ELF_OFF ") + sh_size (" FMT_ELF_XWORD " = %RTfoff) is beyond the end of the file (%RTfoff)!\n",
                 pszLogName, iShdr, pShdr->sh_offset, pShdr->sh_size, offEnd, cbRawImage));
            return VERR_BAD_EXE_FORMAT;
        }
        if (pShdr->sh_offset < sizeof(Elf_Ehdr))
        {
            Log(("RTLdrELF: %s: Shdr #%d: sh_offset (" FMT_ELF_OFF ") + sh_size (" FMT_ELF_XWORD ") is starting in the ELF header!\n",
                 pszLogName, iShdr, pShdr->sh_offset, pShdr->sh_size, cbRawImage));
            return VERR_BAD_EXE_FORMAT;
        }
    }

    return VINF_SUCCESS;
}



/**
 * Opens an ELF image, fixed bitness.
 *
 * @returns iprt status code.
 * @param   pReader     The loader reader instance which will provide the raw image bits.
 * @param   phLdrMod    Where to store the handle.
 */
static int RTLDRELF_NAME(Open)(PRTLDRREADER pReader, PRTLDRMOD phLdrMod)
{
    const char *pszLogName = pReader->pfnLogName(pReader);
    RTFOFF      cbRawImage = pReader->pfnSize(pReader);

    /*
     * Create the loader module instance.
     */
    PRTLDRMODELF pModElf = (PRTLDRMODELF)RTMemAllocZ(sizeof(*pModElf));
    if (!pModElf)
        return VERR_NO_MEMORY;

    pModElf->Core.u32Magic  = RTLDRMOD_MAGIC;
    pModElf->Core.eState    = LDR_STATE_INVALID;
    pModElf->pReader        = pReader;
    //pModElf->pvBits         = NULL;
    //pModElf->Ehdr           = {0};
    //pModElf->paShdrs        = NULL;
    //pModElf->paSyms         = NULL;
    pModElf->iSymSh         = ~0U;
    pModElf->cSyms          = 0;
    pModElf->iStrSh         = ~0U;
    pModElf->cbStr          = 0;
    pModElf->cbImage        = 0;
    //pModElf->pStr           = NULL;

    /*
     * Read and validate the ELF header.
     */
    int rc = pReader->pfnRead(pReader, &pModElf->Ehdr, sizeof(pModElf->Ehdr), 0);
    if (RT_SUCCESS(rc))
        rc = RTLDRELF_NAME(ValidateElfHeader)(&pModElf->Ehdr, pszLogName, cbRawImage);
    if (RT_SUCCESS(rc))
    {
        /*
         * Read the section headers.
         */
        Elf_Shdr *paShdrs = (Elf_Shdr *)RTMemAlloc(pModElf->Ehdr.e_shnum * sizeof(Elf_Shdr));
        if (paShdrs)
        {
            pModElf->paShdrs = paShdrs;
            rc = pReader->pfnRead(pReader, paShdrs, pModElf->Ehdr.e_shnum * sizeof(Elf_Shdr),
                                  pModElf->Ehdr.e_shoff);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Validate the section headers, allocate memory for the sections (determin the image size),
                 * and find relevant sections.
                 */
                for (unsigned i = 0; i < pModElf->Ehdr.e_shnum; i++)
                {
                    rc = RTLDRELF_NAME(ValidateSectionHeader)(pModElf, i, pszLogName, cbRawImage);
                    if (RT_FAILURE(rc))
                        break;

                    /* Allocate memory addresses for the section. */
                    if (paShdrs[i].sh_flags & SHF_ALLOC)
                    {
                        paShdrs[i].sh_addr = paShdrs[i].sh_addralign
                            ? RT_ALIGN_T(pModElf->cbImage, paShdrs[i].sh_addralign, Elf_Addr)
                            : pModElf->cbImage;
                        pModElf->cbImage = (size_t)paShdrs[i].sh_addr + (size_t)paShdrs[i].sh_size;
                        AssertMsgReturn(pModElf->cbImage == paShdrs[i].sh_addr + paShdrs[i].sh_size,
                                        (FMT_ELF_ADDR "\n", paShdrs[i].sh_addr + paShdrs[i].sh_size),
                                        VERR_IMAGE_TOO_BIG);
                        Log2(("RTLdrElf: %s: Assigned " FMT_ELF_ADDR " to section #%d\n", pszLogName, paShdrs[i].sh_addr, i));
                    }

                    /* We're looking for symbol tables. */
                    if (paShdrs[i].sh_type == SHT_SYMTAB)
                    {
                        if (pModElf->iSymSh != ~0U)
                        {
                            Log(("RTLdrElf: %s: Multiple symbol tabs! iSymSh=%d i=%d\n", pszLogName, pModElf->iSymSh, i));
                            rc = VERR_LDRELF_MULTIPLE_SYMTABS;
                            break;
                        }
                        pModElf->iSymSh = i;
                        pModElf->cSyms  = (unsigned)(paShdrs[i].sh_size / sizeof(Elf_Sym));
                        AssertReturn(pModElf->cSyms == paShdrs[i].sh_size / sizeof(Elf_Sym), VERR_IMAGE_TOO_BIG);
                        pModElf->iStrSh = paShdrs[i].sh_link;
                        pModElf->cbStr  = (unsigned)paShdrs[pModElf->iStrSh].sh_size;
                        AssertReturn(pModElf->cbStr == paShdrs[pModElf->iStrSh].sh_size, VERR_IMAGE_TOO_BIG);
                    }
                } /* for each section header */

                Log2(("RTLdrElf: iSymSh=%u cSyms=%u iStrSh=%u cbStr=%u rc=%Rrc cbImage=%#zx\n",
                      pModElf->iSymSh, pModElf->cSyms, pModElf->iStrSh, pModElf->cbStr, rc, pModElf->cbImage));

                /*
                 * Are the section headers fine?
                 * We require there to be symbol & string tables (at least for the time being).
                 */
                if (    pModElf->iSymSh == ~0U
                    ||  pModElf->iStrSh == ~0U)
                    rc = VERR_LDRELF_NO_SYMBOL_OR_NO_STRING_TABS;
                if (RT_SUCCESS(rc))
                {
                    pModElf->Core.pOps      = &RTLDRELF_MID(s_rtldrElf,Ops);
                    pModElf->Core.eState    = LDR_STATE_OPENED;
                    *phLdrMod = &pModElf->Core;

                    LogFlow(("%s: %s: returns VINF_SUCCESS *phLdrMod=%p\n", __FUNCTION__, pszLogName, *phLdrMod));
                    return VINF_SUCCESS;
                }
            }

            RTMemFree(paShdrs);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    RTMemFree(pModElf);
    LogFlow(("%s: returns %Rrc\n", __FUNCTION__, rc));
    return rc;
}




/*******************************************************************************
*   Cleanup Constants And Macros                                               *
*******************************************************************************/
#undef RTLDRELF_NAME
#undef RTLDRELF_SUFF
#undef RTLDRELF_MID

#undef FMT_ELF_ADDR
#undef FMT_ELF_HALF
#undef FMT_ELF_SHALF
#undef FMT_ELF_OFF
#undef FMT_ELF_SIZE
#undef FMT_ELF_SWORD
#undef FMT_ELF_WORD
#undef FMT_ELF_XWORD
#undef FMT_ELF_SXWORD

#undef Elf_Ehdr
#undef Elf_Phdr
#undef Elf_Shdr
#undef Elf_Sym
#undef Elf_Rel
#undef Elf_Rela
#undef Elf_Reloc
#undef Elf_Nhdr
#undef Elf_Dyn

#undef Elf_Addr
#undef Elf_Half
#undef Elf_Off
#undef Elf_Size
#undef Elf_Sword
#undef Elf_Word

#undef RTLDRMODELF
#undef PRTLDRMODELF

#undef ELF_R_SYM
#undef ELF_R_TYPE
#undef ELF_R_INFO

#undef ELF_ST_BIND

