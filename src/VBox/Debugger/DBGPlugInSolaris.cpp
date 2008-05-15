/* $Id$ */
/** @file
 * DBGPlugInSolaris - Debugger and Guest OS Digger Plugin For Solaris.
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
#include "DBGPlugIns.h"
#include "DBGPlugInCommonELF.h"
#include <VBox/dbgf.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/stream.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** @name InternalSolaris structures
 * @{ */

typedef struct SOL32_modctl
{
    uint32_t    mod_next;               /**<  0 */
    uint32_t    mod_prev;               /**<  4 */
    int32_t     mod_id;                 /**<  8 */
    uint32_t    mod_mp;                 /**<  c Pointer to the kernel runtime loader bits. */
    uint32_t    mod_inprogress_thread;  /**< 10 */
    uint32_t    mod_modinfo;            /**< 14 */
    uint32_t    mod_linkage;            /**< 18 */
    uint32_t    mod_filename;           /**< 1c */
    uint32_t    mod_modname;            /**< 20 */
    int8_t      mod_busy;               /**< 24 */
    int8_t      mod_want;               /**< 25 */
    int8_t      mod_prim;               /**< 26 this is 1 for 'unix' and a few others. */
    int8_t      mod_unused_padding;     /**< 27 */
    int32_t     mod_ref;                /**< 28 */
    int8_t      mod_loaded;             /**< 2c */
    int8_t      mod_installed;          /**< 2d */
    int8_t      mod_loadflags;          /**< 2e */
    int8_t      mod_delay_unload;       /**< 2f */
    uint32_t    mod_requisites;         /**< 30 */
    uint32_t    mod___unused;           /**< 34 */
    int32_t     mod_loadcnt;            /**< 38 */
    int32_t     mod_nenabled;           /**< 3c */
    uint32_t    mod_text;               /**< 40 */
    uint32_t    mod_text_size;          /**< 44 */
    int32_t     mod_gencount;           /**< 48 */
    uint32_t    mod_requisite_loading;  /**< 4c */
} SOL32_modctl_t;
AssertCompileSize(SOL32_modctl_t, 0x50);

typedef struct SOL32_module
{
    int32_t     total_allocated;        /**<  0 */
    Elf32_Ehdr  hdr;                    /**<  4 Easy to validate */
    uint32_t    shdrs;                  /**< 38 */
    uint32_t    symhdr;                 /**< 3c */
    uint32_t    strhdr;                 /**< 40 */
    uint32_t    depends_on;             /**< 44 */
    uint32_t    symsize;                /**< 48 */
    uint32_t    symspace;               /**< 4c */
    int32_t     flags;                  /**< 50 */
    uint32_t    text_size;              /**< 54 */
    uint32_t    data_size;              /**< 58 */
    uint32_t    text;                   /**< 5c */
    uint32_t    data;                   /**< 60 */
    uint32_t    symtbl_section;         /**< 64 */
    uint32_t    symtbl;                 /**< 68 */
    uint32_t    strings;                /**< 6c */
    uint32_t    hashsize;               /**< 70 */
    uint32_t    buckets;                /**< 74 */
    uint32_t    chains;                 /**< 78 */
    uint32_t    nsyms;                  /**< 7c */
    uint32_t    bss_align;              /**< 80 */
    uint32_t    bss_size;               /**< 84 */
    uint32_t    bss;                    /**< 88 */
    uint32_t    filename;               /**< 8c */
    uint32_t    head;                   /**< 90 */
    uint32_t    tail;                   /**< 94 */
    uint32_t    destination;            /**< 98 */
    uint32_t    machdata;               /**< 9c */
    uint32_t    ctfdata;                /**< a0 */
    uint32_t    ctfsize;                /**< a4 */
    uint32_t    fbt_tab;                /**< a8 */
    uint32_t    fbt_size;               /**< ac */
    uint32_t    fbt_nentries;           /**< b0 */
    uint32_t    textwin;                /**< b4 */
    uint32_t    textwin_base;           /**< b8 */
    uint32_t    sdt_probes;             /**< bc */
    uint32_t    sdt_nprobes;            /**< c0 */
    uint32_t    sdt_tab;                /**< c4 */
    uint32_t    sdt_size;               /**< c8 */
    uint32_t    sigdata;                /**< cc */
    uint32_t    sigsize;                /**< d0 */
} SOL32_module_t;
AssertCompileSize(Elf32_Ehdr, 0x34);
AssertCompileSize(SOL32_module_t, 0xd4);

typedef struct SOL_utsname
{
    char        sysname[257];
    char        nodename[257];
    char        release[257];
    char        version[257];
    char        machine[257];
} SOL_utsname_t;
AssertCompileSize(SOL_utsname_t, 5 * 257);

/** @} */


/**
 * Solaris guest OS digger instance data.
 */
typedef struct DBGDIGGERSOLARIS
{
    /** Whether the information is valid or not.
     * (For fending off illegal interface method calls.) */
    bool fValid;

    /** Address of the 'unix' text segment.
     * This is set during probing. */
    DBGFADDRESS AddrUnixText;
    /** Address of the 'unix' text segment.
     * This is set during probing. */
    DBGFADDRESS AddrUnixData;
    /** Address of the 'unix' modctl_t (aka modules). */
    DBGFADDRESS AddrUnixModCtl;

} DBGDIGGERSOLARIS;
/** Pointer to the solaris guest OS digger instance data. */
typedef DBGDIGGERSOLARIS *PDBGDIGGERSOLARIS;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a 32-bit solaris kernel address */
#define SOL32_VALID_ADDRESS(Addr)       ((Addr) > UINT32_C(0x80000000) && (Addr) < UINT32_C(0xfffff000))

/** The max data segment size of the 'unix' module. */
#define SOL_UNIX_MAX_DATA_SEG_SIZE      0x01000000


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int)  dbgDiggerSolarisInit(PVM pVM, void *pvData);



/**
 * @copydoc DBGFOSREG::pfnQueryInterface
 */
static DECLCALLBACK(void *) dbgDiggerSolarisQueryInterface(PVM pVM, void *pvData, DBGFOSINTERFACE enmIf)
{
    return NULL;
}


/**
 * @copydoc DBGFOSREG::pfnQueryVersion
 */
static DECLCALLBACK(int)  dbgDiggerSolarisQueryVersion(PVM pVM, void *pvData, char *pszVersion, size_t cchVersion)
{
    PDBGDIGGERSOLARIS pThis = (PDBGDIGGERSOLARIS)pvData;
    Assert(pThis->fValid);

    /*
     * It's all in the utsname symbol...
     */
    DBGFADDRESS Addr;
    SOL_utsname_t UtsName;
    DBGFSYMBOL SymUtsName;
    int rc = DBGFR3SymbolByName(pVM, "utsname", &SymUtsName);
    if (RT_SUCCESS(rc))
        rc = DBGFR3MemRead(pVM, DBGFR3AddrFromFlat(pVM, &Addr, SymUtsName.Value), &UtsName, sizeof(UtsName));
    if (RT_FAILURE(rc))
    {
        /*
         * Try searching by the name...
         */
        memset(&UtsName, '\0', sizeof(UtsName));
        strcpy(&UtsName.sysname[0], "SunOS");
        rc = DBGFR3MemScan(pVM, &pThis->AddrUnixData, SOL_UNIX_MAX_DATA_SEG_SIZE,
                           (uint8_t *)&UtsName.sysname[0], sizeof(UtsName.sysname), &Addr);
        if (RT_SUCCESS(rc))
            rc = DBGFR3MemRead(pVM, DBGFR3AddrFromFlat(pVM, &Addr, Addr.FlatPtr - RT_OFFSETOF(SOL_utsname_t, sysname)),
                               &UtsName, sizeof(UtsName));
    }

    /*
     * Copy out the result (if any).
     */
    if (RT_SUCCESS(rc))
    {
        if (    UtsName.nodename[-1] != '\0'
            ||  UtsName.release[-1] != '\0'
            ||  UtsName.version[-1] != '\0'
            ||  UtsName.machine[-1] != '\0'
            ||  UtsName.machine[sizeof(UtsName.machine) - 1] != '\0')
        {
            //rc = VERR_DBGF_UNEXPECTED_OS_DATA;
            rc = VERR_GENERAL_FAILURE;
            RTStrPrintf(pszVersion, cchVersion, "failed - bogus utsname");
        }
        else
            RTStrPrintf(pszVersion, cchVersion, "%s %s", UtsName.version, UtsName.release);
    }
    else
        RTStrPrintf(pszVersion, cchVersion, "failed - %Rrc", rc);

    return rc;
}



/**
 * Processes a modctl_t.
 *
 * @param   pVM     The VM handle.
 * @param   pThis   Our instance data.
 * @param   pModCtl Pointer to the modctl structure.
 */
static void dbgDiggerSolarisProcessModCtl(PVM pVM, PDBGDIGGERSOLARIS pThis, SOL32_modctl_t const *pModCtl)
{
    /* skip it if it's not loaded and installed */
    if (    !pModCtl->mod_loaded
        ||  !pModCtl->mod_installed)
        return;

    /*
     * Read the module and file names first
     */
    char szModName[64];
    DBGFADDRESS Addr;
    int rc = DBGFR3MemReadString(pVM, DBGFR3AddrFromFlat(pVM, &Addr, pModCtl->mod_modname), szModName, sizeof(szModName));
    if (RT_FAILURE(rc))
        return;
    if (!memchr(szModName, '\0', sizeof(szModName)))
        szModName[sizeof(szModName) - 1] = '\0';

    char szFilename[256];
    rc = DBGFR3MemReadString(pVM, DBGFR3AddrFromFlat(pVM, &Addr, pModCtl->mod_filename), szFilename, sizeof(szFilename));
    if (RT_FAILURE(rc))
        strcpy(szFilename, szModName);
    else if (!memchr(szFilename, '\0', sizeof(szFilename)))
        szFilename[sizeof(szFilename) - 1] = '\0';

    /*
     * Then read the module struct and validate it.
     */
    struct SOL32_module Module;
    rc = DBGFR3MemRead(pVM, DBGFR3AddrFromFlat(pVM, &Addr, pModCtl->mod_mp), &Module, sizeof(Module));
    if (RT_FAILURE(rc))
        return;

    /* Basic validations of the elf header. */
    if (    Module.hdr.e_ident[EI_MAG0] != ELFMAG0
        ||  Module.hdr.e_ident[EI_MAG1] != ELFMAG1
        ||  Module.hdr.e_ident[EI_MAG2] != ELFMAG2
        ||  Module.hdr.e_ident[EI_MAG3] != ELFMAG3
        ||  Module.hdr.e_ident[EI_CLASS] != ELFCLASS32
        ||  Module.hdr.e_ident[EI_DATA] != ELFDATA2LSB
        ||  Module.hdr.e_ident[EI_VERSION] != EV_CURRENT
        ||  ASMMemIsAll8(&Module.hdr.e_ident[EI_PAD], EI_NIDENT - EI_PAD, 0) != NULL
        )
        return;
    if (Module.hdr.e_version != EV_CURRENT)
        return;
    if (Module.hdr.e_ehsize != sizeof(Module.hdr))
        return;
    if (    Module.hdr.e_type != ET_DYN
        &&  Module.hdr.e_type != ET_REL
        &&  Module.hdr.e_type != ET_EXEC) //??
        return;
    if (    Module.hdr.e_machine != EM_386
        &&  Module.hdr.e_machine != EM_486)
        return;
    if (    Module.hdr.e_phentsize != sizeof(Elf32_Phdr)
        &&  Module.hdr.e_phentsize) //??
        return;
    if (Module.hdr.e_shentsize != sizeof(Elf32_Shdr))
        return;

    if (Module.hdr.e_shentsize != sizeof(Elf32_Shdr))
        return;

    /* Basic validations of the rest of the stuff. */
    if (    !SOL32_VALID_ADDRESS(Module.shdrs)
        ||  !SOL32_VALID_ADDRESS(Module.symhdr)
        ||  !SOL32_VALID_ADDRESS(Module.strhdr)
        ||  (!SOL32_VALID_ADDRESS(Module.symspace) && Module.symspace)
        ||  !SOL32_VALID_ADDRESS(Module.text)
        ||  !SOL32_VALID_ADDRESS(Module.data)
        ||  (!SOL32_VALID_ADDRESS(Module.symtbl) && Module.symtbl)
        ||  (!SOL32_VALID_ADDRESS(Module.strings) && Module.strings)
        ||  (!SOL32_VALID_ADDRESS(Module.head) && Module.head)
        ||  (!SOL32_VALID_ADDRESS(Module.tail) && Module.tail)
        ||  !SOL32_VALID_ADDRESS(Module.filename))
        return;
    if (    Module.symsize > _4M
        ||  Module.hdr.e_shnum > 4096
        ||  Module.nsyms > _256K)
        return;

    /* Ignore modules without symbols. */
    if (!Module.symtbl || !Module.strings || !Module.symspace || !Module.symspace)
        return;

    /* Check that the symtbl and strings points inside the symspace. */
    if (Module.strings - Module.symspace >= Module.symsize)
        return;
    if (Module.symtbl - Module.symspace >= Module.symsize)
        return;

    /*
     * Read the section headers, symbol table and string tables.
     */
    size_t cb = Module.hdr.e_shnum * sizeof(Elf32_Shdr);
    Elf32_Shdr *paShdrs = (Elf32_Shdr *)RTMemTmpAlloc(cb);
    if (!paShdrs)
        return;
    rc = DBGFR3MemRead(pVM, DBGFR3AddrFromFlat(pVM, &Addr, Module.shdrs), paShdrs, cb);
    if (RT_SUCCESS(rc))
    {
        void *pvSymSpace = RTMemTmpAlloc(Module.symsize + 1);
        if (pvSymSpace)
        {
            rc = DBGFR3MemRead(pVM, DBGFR3AddrFromFlat(pVM, &Addr, Module.shdrs), pvSymSpace, Module.symsize);
            if (RT_SUCCESS(rc))
            {
                ((uint8_t *)pvSymSpace)[Module.symsize] = 0;

                /*
                 * Hand it over to the common ELF32 module parser.
                 */
                char const *pbStrings = (char const *)pvSymSpace + (Module.strings - Module.symspace);
                size_t cbMaxStrings = Module.symsize - (Module.strings - Module.symspace);

                Elf32_Sym const *paSyms = (Elf32_Sym const *)((uintptr_t)pvSymSpace + (Module.symtbl - Module.symspace));
                size_t cMaxSyms = (Module.symsize - (Module.symtbl - Module.symspace)) / sizeof(Elf32_Sym);
                cMaxSyms = RT_MIN(cMaxSyms, Module.nsyms);

                DBGDiggerCommonParseElf32Mod(pVM, szModName, szFilename, DBG_DIGGER_ELF_FUNNY_SHDRS,
                                             &Module.hdr, paShdrs, paSyms, cMaxSyms, pbStrings, cbMaxStrings);
            }
            RTMemTmpFree(pvSymSpace);
        }
    }

    RTMemTmpFree(paShdrs);
    return;
}


/**
 * @copydoc DBGFOSREG::pfnTerm
 */
static DECLCALLBACK(void)  dbgDiggerSolarisTerm(PVM pVM, void *pvData)
{
    PDBGDIGGERSOLARIS pThis = (PDBGDIGGERSOLARIS)pvData;
    Assert(pThis->fValid);

    pThis->fValid = false;
}


/**
 * @copydoc DBGFOSREG::pfnRefresh
 */
static DECLCALLBACK(int)  dbgDiggerSolarisRefresh(PVM pVM, void *pvData)
{
    PDBGDIGGERSOLARIS pThis = (PDBGDIGGERSOLARIS)pvData;
    Assert(pThis->fValid);

    /*
     * For now we'll flush and reload everything.
     */
    dbgDiggerSolarisTerm(pVM, pvData);
    return dbgDiggerSolarisInit(pVM, pvData);
}


/**
 * @copydoc DBGFOSREG::pfnInit
 */
static DECLCALLBACK(int)  dbgDiggerSolarisInit(PVM pVM, void *pvData)
{
    PDBGDIGGERSOLARIS pThis = (PDBGDIGGERSOLARIS)pvData;
    Assert(!pThis->fValid);
    int rc;

    /*
     * Find the 'unix' modctl_t structure (aka modules).
     * We know it resides in the unix data segment.
     */
    DBGFR3AddrFromFlat(pVM, &pThis->AddrUnixModCtl, 0);

    DBGFADDRESS     CurAddr = pThis->AddrUnixData;
    DBGFADDRESS     MaxAddr;
    DBGFR3AddrFromFlat(pVM, &MaxAddr, CurAddr.FlatPtr + SOL_UNIX_MAX_DATA_SEG_SIZE);
    const uint8_t  *pbExpr = (const uint8_t *)&pThis->AddrUnixText.FlatPtr;
    const uint32_t  cbExpr = sizeof(uint32_t);//pThis->AddrUnixText.FlatPtr < _4G ? sizeof(uint32_t) : sizeof(uint64_t)
    while (   CurAddr.FlatPtr < MaxAddr.FlatPtr
           && CurAddr.FlatPtr >= pThis->AddrUnixData.FlatPtr)
    {
        DBGFADDRESS HitAddr;
        rc = DBGFR3MemScan(pVM, &CurAddr, MaxAddr.FlatPtr - CurAddr.FlatPtr, pbExpr, cbExpr, &HitAddr);
        if (RT_FAILURE(rc))
            break;

        /*
         * Read out the modctl_t structure.
         */
        DBGFADDRESS ModCtlAddr;
        DBGFR3AddrFromFlat(pVM, &ModCtlAddr, HitAddr.FlatPtr - RT_OFFSETOF(SOL32_modctl_t, mod_text));
        SOL32_modctl_t ModCtl;
        rc = DBGFR3MemRead(pVM, &ModCtlAddr, &ModCtl, sizeof(ModCtl));
        if (RT_SUCCESS(rc))
        {
            if (    SOL32_VALID_ADDRESS(ModCtl.mod_next)
                &&  SOL32_VALID_ADDRESS(ModCtl.mod_prev)
                &&  ModCtl.mod_id == 0
                &&  SOL32_VALID_ADDRESS(ModCtl.mod_mp)
                &&  SOL32_VALID_ADDRESS(ModCtl.mod_filename)
                &&  SOL32_VALID_ADDRESS(ModCtl.mod_modname)
                &&  ModCtl.mod_prim == 1
                &&  ModCtl.mod_loaded == 1
                &&  ModCtl.mod_installed == 1
                &&  ModCtl.mod_requisites == 0
                &&  ModCtl.mod_loadcnt == 1
                /*&&  ModCtl.mod_text == pThis->AddrUnixText.FlatPtr*/
                &&  ModCtl.mod_text_size < UINT32_C(0xfec00000) - UINT32_C(0xfe800000) )
            {
                char szUnix[5];
                DBGFADDRESS NameAddr;
                DBGFR3AddrFromFlat(pVM, &NameAddr, ModCtl.mod_modname);
                rc = DBGFR3MemRead(pVM, &NameAddr, &szUnix, sizeof(szUnix));
                if (RT_SUCCESS(rc))
                {
                    if (!strcmp(szUnix, "unix"))
                    {
                        pThis->AddrUnixModCtl = ModCtlAddr;
                        break;
                    }
                    Log(("sol32 mod_name=%.*s\n", sizeof(szUnix), szUnix));
                }
            }
        }

        /* next */
        DBGFR3AddrFromFlat(pVM, &CurAddr, HitAddr.FlatPtr + cbExpr);
    }

    /*
     * Walk the module chain and add the modules and their symbols.
     */
    if (pThis->AddrUnixModCtl.FlatPtr)
    {
        int iMod = 0;
        CurAddr = pThis->AddrUnixModCtl;
        do
        {
            /* read it */
            SOL32_modctl_t ModCtl;
            rc = DBGFR3MemRead(pVM, &CurAddr, &ModCtl, sizeof(ModCtl));
            if (RT_FAILURE(rc))
            {
                LogRel(("sol32: bad modctl_t chain: %RGv - %Rrc\n", iMod, CurAddr.FlatPtr, rc));
                break;
            }

            /* process it. */
            dbgDiggerSolarisProcessModCtl(pVM, pThis, &ModCtl);

            /* next */
            if (!SOL32_VALID_ADDRESS(ModCtl.mod_next))
            {
                LogRel(("sol32: bad modctl_t chain at %RGv: %RGv\n", iMod, CurAddr.FlatPtr, (RTGCUINTPTR)ModCtl.mod_next));
                break;
            }
            if (++iMod >= 1024)
            {
                LogRel(("sol32: too many modules (%d)\n", iMod));
                break;
            }
            DBGFR3AddrFromFlat(pVM, &CurAddr, ModCtl.mod_next);
        } while (CurAddr.FlatPtr != pThis->AddrUnixModCtl.FlatPtr);
    }

    pThis->fValid = true;
    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnProbe
 */
static DECLCALLBACK(bool)  dbgDiggerSolarisProbe(PVM pVM, void *pvData)
{
    PDBGDIGGERSOLARIS pThis = (PDBGDIGGERSOLARIS)pvData;

    /*
     * Look for "SunOS Release" in the text segment.
     */
    DBGFADDRESS Addr;
    DBGFR3AddrFromFlat(pVM, &Addr, 0xfe800000);
    RTGCUINTPTR cbRange = 0xfec00000 - 0xfe800000;

    DBGFADDRESS HitAddr;
    static const uint8_t s_abSunRelease[] = "SunOS Release ";
    int rc = DBGFR3MemScan(pVM, &Addr, cbRange, s_abSunRelease, sizeof(s_abSunRelease) - 1, &HitAddr);
    if (RT_FAILURE(rc))
        return false;

    /*
     * Look for the copy right string too, just to be sure.
     */
    static const uint8_t s_abSMI[] = "Sun Microsystems, Inc.";
    rc = DBGFR3MemScan(pVM, &Addr, cbRange, s_abSMI, sizeof(s_abSMI) - 1, &HitAddr);
    if (RT_FAILURE(rc))
        return false;

    /*
     * Remember the unix text and data addresses (32-bit vs 64-bit).
     */
    pThis->AddrUnixText = Addr;
//    if (pThis->AddrUnixText.FlatPtr == 0xfe800000)
    {
        DBGFR3AddrFromFlat(pVM, &Addr, 0xfec00000);
        pThis->AddrUnixData = Addr;
    }
//    else
//    {
//        DBGFR3AddrFromFlat(pVM, &Addr, UINT64_C(0xwhateveritis));
//        pThis->AddrUnixData = Addr;
//    }

    return true;
}


/**
 * @copydoc DBGFOSREG::pfnDestruct
 */
static DECLCALLBACK(void)  dbgDiggerSolarisDestruct(PVM pVM, void *pvData)
{

}


/**
 * @copydoc DBGFOSREG::pfnConstruct
 */
static DECLCALLBACK(int)  dbgDiggerSolarisConstruct(PVM pVM, void *pvData)
{
    return VINF_SUCCESS;
}


const DBGFOSREG g_DBGDiggerSolaris =
{
    /* .u32Magic = */           DBGFOSREG_MAGIC,
    /* .fFlags = */             0,
    /* .cbData = */             sizeof(DBGDIGGERSOLARIS),
    /* .szName = */             "Solaris",
    /* .pfnConstruct = */       dbgDiggerSolarisConstruct,
    /* .pfnDestruct = */        dbgDiggerSolarisDestruct,
    /* .pfnProbe = */           dbgDiggerSolarisProbe,
    /* .pfnInit = */            dbgDiggerSolarisInit,
    /* .pfnRefresh = */         dbgDiggerSolarisRefresh,
    /* .pfnTerm = */            dbgDiggerSolarisTerm,
    /* .pfnQueryVersion = */    dbgDiggerSolarisQueryVersion,
    /* .pfnQueryInterface = */  dbgDiggerSolarisQueryInterface,
    /* .u32EndMagic = */        DBGFOSREG_MAGIC
};

