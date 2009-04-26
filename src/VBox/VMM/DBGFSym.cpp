/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Symbol Management.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#if defined(RT_OS_WINDOWS) && 1 //defined(DEBUG_bird) // enabled this is you want to debug win32 guests or the hypervisor.
# include <Windows.h>
# define _IMAGEHLP64
# include <DbgHelp.h>
# define HAVE_DBGHELP /* if doing guest stuff, this can be nice. */
#endif
/** @todo Only use DBGHELP for reading modules since it doesn't do all we want (relocations), or is way to slow in some cases (add symbol)! */
#include <VBox/dbgf.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/mm.h>
#include <VBox/pdm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>

#ifndef HAVE_DBGHELP
# include <iprt/avl.h>
# include <iprt/string.h>
# include <iprt/ctype.h>
#endif

#include <stdio.h> /* for fopen(). */ /** @todo use iprt/stream.h! */
#include <stdlib.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef HAVE_DBGHELP
static DECLCALLBACK(int) dbgfR3EnumModules(PVM pVM, const char *pszFilename, const char *pszName,
                                           RTUINTPTR ImageBase, size_t cbImage, bool fRC, void *pvArg);
static int win32Error(PVM pVM);
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#ifndef HAVE_DBGHELP
/* later */
typedef struct DBGFMOD *PDBGFMOD;

/**
 * Internal represenation of a symbol.
 */
typedef struct DBGFSYM
{
    /** Node core with the symbol address range. */
    AVLRGCPTRNODECORE       Core;
    /** Pointer to the module this symbol is associated with. */
    PDBGFMOD                pModule;
    /** Pointer to the next symbol in with this name. */
    struct DBGFSYM         *pNext;
    /** Symbol name. */
    char                    szName[1];
} DBGFSYM, *PDBGFSYM;

/**
 * Symbol name space node.
 */
typedef struct DBGFSYMSPACE
{
    /** Node core with the symbol name.
     * (it's allocated in the same block as this struct) */
    RTSTRSPACECORE          Core;
    /** Pointer to the first symbol with this name (LIFO). */
    PDBGFSYM                pSym;
} DBGFSYMSPACE, *PDBGFSYMSPACE;

#endif



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifndef HAVE_DBGHELP

/**
 * Initializes the symbol tree.
 */
static int dbgfR3SymbolInit(PVM pVM)
{
    PDBGFSYM pSym = (PDBGFSYM)MMR3HeapAlloc(pVM, MM_TAG_DBGF_SYMBOL, sizeof(*pSym));
    if (pSym)
    {
        pSym->Core.Key = 0;
        pSym->Core.KeyLast = ~0;
        pSym->pModule = NULL;
        pSym->szName[0] = '\0';
        if (RTAvlrGCPtrInsert(&pVM->dbgf.s.SymbolTree, &pSym->Core))
            return VINF_SUCCESS;
        AssertReleaseMsgFailed(("Failed to insert %RGv-%RGv!\n", pSym->Core.Key, pSym->Core.KeyLast));
        return VERR_INTERNAL_ERROR;
    }
    return VERR_NO_MEMORY;
}


/**
 * Insert a record into the symbol tree.
 */
static int dbgfR3SymbolInsert(PVM pVM, const char *pszName, RTGCPTR Address, size_t cb, PDBGFMOD pModule)
{
    /*
     * Make the address space node.
     */
    size_t      cchName = strlen(pszName) + 1;
    PDBGFSYM pSym = (PDBGFSYM)MMR3HeapAlloc(pVM, MM_TAG_DBGF_SYMBOL, RT_OFFSETOF(DBGFSYM, szName[cchName]));
    if (pSym)
    {
        pSym->Core.Key = Address;
        pSym->Core.KeyLast = Address + cb;
        pSym->pModule = pModule;
        memcpy(pSym->szName, pszName, cchName);

        PDBGFSYM pOld = (PDBGFSYM)RTAvlrGCPtrRangeGet(&pVM->dbgf.s.SymbolTree, (RTGCPTR)Address);
        if (pOld)
        {
            pSym->Core.KeyLast = pOld->Core.KeyLast;
            if (pOld->Core.Key == pSym->Core.Key)
            {
                pOld = (PDBGFSYM)RTAvlrGCPtrRemove(&pVM->dbgf.s.SymbolTree, (RTGCPTR)Address);
                AssertRelease(pOld);
                MMR3HeapFree(pOld);
            }
            else
                pOld->Core.KeyLast = Address - 1;
            if (RTAvlrGCPtrInsert(&pVM->dbgf.s.SymbolTree, &pSym->Core))
            {
                /*
                 * Make the name space node.
                 */
                PDBGFSYMSPACE pName = (PDBGFSYMSPACE)RTStrSpaceGet(pVM->dbgf.s.pSymbolSpace, pszName);
                if (!pName)
                {
                    /* make new symbol space node. */
                    PDBGFSYMSPACE pName = (PDBGFSYMSPACE)MMR3HeapAlloc(pVM, MM_TAG_DBGF_SYMBOL, sizeof(*pName) + cchName);
                    if (pName)
                    {
                        pName->Core.pszString = (char *)memcpy(pName + 1, pszName, cchName);
                        pName->pSym = pSym;
                        if (RTStrSpaceInsert(pVM->dbgf.s.pSymbolSpace, &pName->Core))
                            return VINF_SUCCESS;
                    }
                    else
                        return VINF_SUCCESS;
                }
                else
                {
                    /* Add to existing symbol name. */
                    pSym->pNext = pName->pSym;
                    pName->pSym = pSym;
                    return VINF_SUCCESS;
                }
            }
            AssertReleaseMsgFailed(("Failed to insert %RGv-%RGv!\n", pSym->Core.Key, pSym->Core.KeyLast));
        }
        else
            AssertMsgFailed(("pOld! %RGv %s\n", pSym->Core.Key, pszName));
        return VERR_INTERNAL_ERROR;

    }
    return VERR_NO_MEMORY;
}


/**
 * Get nearest symbol.
 * @returns NULL if no symbol was the for that address.
 */
static PDBGFSYM dbgfR3SymbolGetAddr(PVM pVM, RTGCPTR Address)
{
    PDBGFSYM pSym = (PDBGFSYM)RTAvlrGCPtrRangeGet(&pVM->dbgf.s.SymbolTree, Address);
    Assert(pSym);
    if (pSym && pSym->szName[0])
        return pSym;
    return NULL;
}


/**
 * Get first symbol.
 * @returns NULL if no symbol by that name.
 */
static PDBGFSYM dbgfR3SymbolGetName(PVM pVM, const char *pszSymbol)
{
    PDBGFSYMSPACE pName = (PDBGFSYMSPACE)RTStrSpaceGet(pVM->dbgf.s.pSymbolSpace, pszSymbol);
    if (pName)
        return pName->pSym;
    return NULL;
}

#endif


/**
 * Strips all kind of spaces from head and tail of a string.
 */
static char *dbgfR3Strip(char *psz)
{
    while (*psz && isspace(*psz))
        psz++;
    char *psz2 = strchr(psz, '\0') - 1;
    while (psz2 >= psz && isspace(*psz2))
        *psz2-- = '\0';
    return psz;
}


/**
 * Initialize the debug info for a VM.
 *
 * This will check the CFGM for any symbols or symbol files
 * which needs loading.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
int dbgfR3SymInit(PVM pVM)
{
    int rc;

    /*
     * Initialize the symbol table.
     */
    pVM->dbgf.s.pSymbolSpace = (PRTSTRSPACE)MMR3HeapAllocZ(pVM, MM_TAG_DBGF_SYMBOL, sizeof(*pVM->dbgf.s.pSymbolSpace));
    AssertReturn(pVM->dbgf.s.pSymbolSpace, VERR_NO_MEMORY);

#ifndef HAVE_DBGHELP
    /* modules & lines later */
    rc = dbgfR3SymbolInit(pVM);
    if (RT_FAILURE(rc))
        return rc;
    pVM->dbgf.s.fSymInited = true;
#endif

    /*
     * Check if there are 'loadsyms' commands in the configuration.
     */
    PCFGMNODE pNode = CFGMR3GetChild(CFGMR3GetRoot(pVM), "/DBGF/loadsyms/");
    if (pNode)
    {
        /*
         * Enumerate the commands.
         */
        for (PCFGMNODE pCmdNode = CFGMR3GetFirstChild(pNode);
             pCmdNode;
             pCmdNode = CFGMR3GetNextChild(pCmdNode))
        {
            char szCmdName[128];
            CFGMR3GetName(pCmdNode, &szCmdName[0], sizeof(szCmdName));

            /* File */
            char *pszFilename;
            rc = CFGMR3QueryStringAlloc(pCmdNode, "Filename", &pszFilename);
            AssertMsgRCReturn(rc, ("rc=%Rrc querying the 'File' attribute of '/DBGF/loadsyms/%s'!\n", rc, szCmdName), rc);

            /* Delta (optional) */
            RTGCINTPTR offDelta;
            rc = CFGMR3QueryGCPtrS(pNode, "Delta", &offDelta);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                offDelta = 0;
            else
                AssertMsgRCReturn(rc, ("rc=%Rrc querying the 'Delta' attribute of '/DBGF/loadsyms/%s'!\n", rc, szCmdName), rc);

            /* Module (optional) */
            char *pszModule;
            rc = CFGMR3QueryStringAlloc(pCmdNode, "Module", &pszModule);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                pszModule = NULL;
            else
                AssertMsgRCReturn(rc, ("rc=%Rrc querying the 'Module' attribute of '/DBGF/loadsyms/%s'!\n", rc, szCmdName), rc);

            /* Module (optional) */
            RTGCUINTPTR ModuleAddress;
            rc = CFGMR3QueryGCPtrU(pNode, "ModuleAddress", &ModuleAddress);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                ModuleAddress = 0;
            else
                AssertMsgRCReturn(rc, ("rc=%Rrc querying the 'ModuleAddress' attribute of '/DBGF/loadsyms/%s'!\n", rc, szCmdName), rc);

            /* Image size (optional) */
            RTGCUINTPTR cbModule;
            rc = CFGMR3QueryGCPtrU(pNode, "ModuleSize", &cbModule);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                cbModule = 0;
            else
                AssertMsgRCReturn(rc, ("rc=%Rrc querying the 'ModuleAddress' attribute of '/DBGF/loadsyms/%s'!\n", rc, szCmdName), rc);


            /*
             * Execute the command.
             */
            rc = DBGFR3ModuleLoad(pVM, pszFilename, offDelta, pszModule, ModuleAddress, cbModule);
            AssertMsgRCReturn(rc, ("pszFilename=%s offDelta=%RGv pszModule=%s ModuleAddress=%RGv cbModule=%RGv\n",
                                   pszFilename, offDelta, pszModule, ModuleAddress, cbModule), rc);

            MMR3HeapFree(pszModule);
            MMR3HeapFree(pszFilename);
        }
    }

    /*
     * Check if there are any 'symadd' commands in the configuration.
     */

    return VINF_SUCCESS;
}


/**
 * We delay certain
 * Initialize the debug info for a VM.
 */
int dbgfR3SymLazyInit(PVM pVM)
{
    if (pVM->dbgf.s.fSymInited)
        return VINF_SUCCESS;
#ifdef HAVE_DBGHELP
    if (SymInitialize(pVM, NULL, FALSE))
    {
        pVM->dbgf.s.fSymInited = true;
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_AUTO_PUBLICS | SYMOPT_ALLOW_ABSOLUTE_SYMBOLS);

        /*
         * Enumerate all modules loaded by PDM and add them to the symbol database.
         */
        PDMR3LdrEnumModules(pVM, dbgfR3EnumModules, NULL);
        return VINF_SUCCESS;
    }
    return win32Error(pVM);
#else
    return VINF_SUCCESS;
#endif
}


#ifdef HAVE_DBGHELP
/**
 * Module enumeration callback function.
 *
 * @returns VBox status.
 *          Failure will stop the search and return the return code.
 *          Warnings will be ignored and not returned.
 * @param   pVM             VM Handle.
 * @param   pszFilename     Module filename.
 * @param   pszName         Module name. (short and unique)
 * @param   ImageBase       Address where to executable image is loaded.
 * @param   cbImage         Size of the executable image.
 * @param   fRC             Set if guest context, clear if host context.
 * @param   pvArg           User argument.
 */
static DECLCALLBACK(int) dbgfR3EnumModules(PVM pVM, const char *pszFilename, const char *pszName,
                                           RTUINTPTR ImageBase, size_t cbImage, bool fRC, void *pvArg)
{
    if (fRC)
    {
        DWORD64 LoadedImageBase = SymLoadModule64(pVM, NULL, (char *)(void *)pszFilename,
                                                  (char *)(void *)pszName, ImageBase, (DWORD)cbImage);
        if (!LoadedImageBase)
            Log(("SymLoadModule64(,,%s,,) -> lasterr=%d\n", pszFilename, GetLastError()));
        else
            Log(("Loaded debuginfo for %s - %s %llx\n", pszName, pszFilename, LoadedImageBase));
    }
    return VINF_SUCCESS;
}
#endif


/**
 * Terminate the debug info repository for the specified VM.
 *
 * @returns VBox status.
 * @param   pVM             VM Handle.
 */
int dbgfR3SymTerm(PVM pVM)
{
#ifdef HAVE_DBGHELP
    if (pVM->dbgf.s.fSymInited)
        SymCleanup(pVM);
    pVM->dbgf.s.fSymInited = false;
    return VINF_SUCCESS;
#else
    pVM->dbgf.s.SymbolTree = 0; /* MM cleans up allocations */
    pVM->dbgf.s.fSymInited = false;
    return VINF_SUCCESS;
#endif
}


/** Symbol file type.. */
typedef enum SYMFILETYPE
{
    SYMFILETYPE_UNKNOWN,
    SYMFILETYPE_LD_MAP,
    SYMFILETYPE_MS_MAP,
    SYMFILETYPE_OBJDUMP,
    SYMFILETYPE_LINUX_SYSTEM_MAP,
    SYMFILETYPE_PDB,
    SYMFILETYPE_DBG,
    SYMFILETYPE_MZ,
    SYMFILETYPE_ELF
} SYMFILETYPE, *PSYMFILETYPE;



/**
 * Probe the type of a symbol information file.
 *
 * @returns The file type.
 * @param   pFile   File handle.
 */
SYMFILETYPE dbgfR3ModuleProbe(FILE *pFile)
{
    char szHead[4096];
    size_t cchHead = fread(szHead, 1, sizeof(szHead) - 1, pFile);
    if (cchHead > 0)
    {
        szHead[cchHead] = '\0';
        if (strstr(szHead, "Preferred load address is"))
            return SYMFILETYPE_MS_MAP;

        if (    strstr(szHead, "Archive member included because of")
            ||  strstr(szHead, "Memory Configuration")
            ||  strstr(szHead, "Linker script and memory map"))
            return SYMFILETYPE_LD_MAP;

        if (   isxdigit(szHead[0])
            && isxdigit(szHead[1])
            && isxdigit(szHead[2])
            && isxdigit(szHead[3])
            && isxdigit(szHead[4])
            && isxdigit(szHead[5])
            && isxdigit(szHead[6])
            && isxdigit(szHead[7])
            && szHead[8] == ' '
            && isalpha(szHead[9])
            && szHead[10] == ' '
            && (isalpha(szHead[11]) || szHead[11] == '_' || szHead[11] == '$')
            )
            return SYMFILETYPE_LINUX_SYSTEM_MAP;

        if (   isxdigit(szHead[0])
            && isxdigit(szHead[1])
            && isxdigit(szHead[2])
            && isxdigit(szHead[3])
            && isxdigit(szHead[4])
            && isxdigit(szHead[5])
            && isxdigit(szHead[6])
            && isxdigit(szHead[7])
            && isxdigit(szHead[8])
            && isxdigit(szHead[9])
            && isxdigit(szHead[10])
            && isxdigit(szHead[11])
            && isxdigit(szHead[12])
            && isxdigit(szHead[13])
            && isxdigit(szHead[14])
            && isxdigit(szHead[15])
            && szHead[16] == ' '
            && isalpha(szHead[17])
            && szHead[18] == ' '
            && (isalpha(szHead[19]) || szHead[19] == '_' || szHead[19] == '$')
            )
            return SYMFILETYPE_LINUX_SYSTEM_MAP;

        if (strstr(szHead, "Microsoft C/C++ MSF") == szHead)
            return SYMFILETYPE_PDB;

        if (strstr(szHead, "ELF") == szHead + 1)
            return SYMFILETYPE_ELF;

        if (   strstr(szHead, "MZ") == szHead
            || strstr(szHead, "PE") == szHead
            || strstr(szHead, "LE") == szHead
            || strstr(szHead, "LX") == szHead
            || strstr(szHead, "NE") == szHead)
            return SYMFILETYPE_MZ;


        if (strstr(szHead, "file format"))
            return SYMFILETYPE_OBJDUMP;
    }

    return SYMFILETYPE_UNKNOWN;
}


static int dbgfR3LoadLinuxSystemMap(PVM pVM, FILE *pFile, RTGCUINTPTR ModuleAddress, RTGCUINTPTR AddressDelta)
{
    char szLine[4096];
    while (fgets(szLine, sizeof(szLine), pFile))
    {
        /* parse the line: <address> <type> <name> */
        const char *psz = dbgfR3Strip(szLine);
        char *pszEnd = NULL;
        uint64_t u64Address;
        int rc = RTStrToUInt64Ex(psz, &pszEnd, 16, &u64Address);
        RTGCUINTPTR Address = u64Address;
        if (    RT_SUCCESS(rc)
            &&  (*pszEnd == ' ' || *pszEnd == '\t')
            &&  Address == u64Address
            &&  u64Address != 0
            &&  u64Address != (RTGCUINTPTR)~0)
        {
            pszEnd++;
            if (    isalpha(*pszEnd)
                &&  (pszEnd[1] == ' ' || pszEnd[1] == '\t'))
            {
                psz = dbgfR3Strip(pszEnd + 2);
                if (*psz)
                {
                    int rc2 = DBGFR3SymbolAdd(pVM, ModuleAddress, Address + AddressDelta, 0, psz);
                    if (RT_FAILURE(rc2))
                        Log2(("DBGFR3SymbolAdd(,, %RGv, 0, '%s') -> %Rrc\n", Address, psz, rc2));
                }
            }
        }
    }
    return VINF_SUCCESS;
}


/**
 * Load debug info, optionally related to a specific module.
 *
 * @returns VBox status.
 * @param   pVM             VM Handle.
 * @param   pszFilename     Path to the file containing the symbol information.
 *                          This can be the executable image, a flat symbol file of some kind or stripped debug info.
 * @param   AddressDelta    The value to add to the loaded symbols.
 * @param   pszName         Short hand name for the module. If not related to a module specify NULL.
 * @param   ModuleAddress   Address which the image is loaded at. This will be used to reference the module other places in the api.
 *                          Ignored when pszName is NULL.
 * @param   cbImage         Size of the image.
 *                          Ignored when pszName is NULL.
 */
VMMR3DECL(int) DBGFR3ModuleLoad(PVM pVM, const char *pszFilename, RTGCUINTPTR AddressDelta, const char *pszName, RTGCUINTPTR ModuleAddress, unsigned cbImage)
{
    /*
     * Lazy init.
     */
    if (!pVM->dbgf.s.fSymInited)
    {
        int rc = dbgfR3SymLazyInit(pVM);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Open the load file.
     */
    int rc = VINF_SUCCESS;
    FILE *pFile = fopen(pszFilename, "rb");
    if (pFile)
    {
        /*
         * Probe the file type.
         */
        SYMFILETYPE enmType = dbgfR3ModuleProbe(pFile);
        if (enmType != SYMFILETYPE_UNKNOWN)
        {
            /*
             * Add the module.
             */
            if (pszName)
            {
                #ifdef HAVE_DBGHELP
                /** @todo arg! checkout the inserting of modules and then loading them again.... Or just the module representation.... */
                DWORD64 ImageBase = SymLoadModule64(pVM, NULL, (char *)(void *)pszFilename, (char *)(void *)pszName, ModuleAddress, cbImage);
                if (!ImageBase)
                    ImageBase = SymLoadModule64(pVM, NULL, (char *)(void *)pszName, (char *)(void *)pszName, ModuleAddress, cbImage);
                if (ImageBase)
                {
                    AssertMsg(ModuleAddress == 0 || ModuleAddress == ImageBase, ("ModuleAddres=%RGv ImageBase=%llx\n", ModuleAddress, ImageBase));
                    ModuleAddress = ImageBase;
                }
                else
                    rc = win32Error(pVM);
                #else
                rc = VERR_NOT_IMPLEMENTED;
                #endif
            }
            if (RT_SUCCESS(rc))
            {
                /*
                 * Seek to the start of the file.
                 */
                rc = fseek(pFile, 0, SEEK_SET);
                Assert(!rc);

                /*
                 * Process the specific.
                 */
                switch (enmType)
                {
                    case SYMFILETYPE_LINUX_SYSTEM_MAP:
                        rc = dbgfR3LoadLinuxSystemMap(pVM, pFile, ModuleAddress, AddressDelta);
                        break;

                    case SYMFILETYPE_PDB:
                    case SYMFILETYPE_DBG:
                    case SYMFILETYPE_MZ:
                #ifdef HAVE_DBGHELP
                        /* done it all above! */
                        break;
                #endif
                    case SYMFILETYPE_LD_MAP:
                    case SYMFILETYPE_MS_MAP:
                    case SYMFILETYPE_OBJDUMP:
                    case SYMFILETYPE_ELF:
                        rc = VERR_NOT_SUPPORTED;
                        break;

                    default:
                        AssertFailed();
                        rc = VERR_INTERNAL_ERROR;
                        break;
                } /* file switch. */
            } /* module added successfully. */
        } /* format identified */
        else
            rc = VERR_NOT_SUPPORTED;
        /** @todo check for read errors */
        fclose(pFile);
    }
    else
        rc = VERR_OPEN_FAILED;
    return rc;
}


/**
 * Interface used by PDMR3LdrRelocate for telling us that a GC module has been relocated.
 *
 * @param   pVM             The VM handle.
 * @param   OldImageBase    The old image base.
 * @param   NewImageBase    The new image base.
 * @param   cbImage         The image size.
 * @param   pszFilename     The image filename.
 * @param   pszName         The module name.
 */
VMMR3DECL(void) DBGFR3ModuleRelocate(PVM pVM, RTGCUINTPTR OldImageBase, RTGCUINTPTR NewImageBase, RTGCUINTPTR cbImage,
                                      const char *pszFilename, const char *pszName)
{
#ifdef HAVE_DBGHELP
    if (pVM->dbgf.s.fSymInited)
    {
        if (!SymUnloadModule64(pVM, OldImageBase))
            Log(("SymUnloadModule64(,%RGv) failed, lasterr=%d\n", OldImageBase, GetLastError()));

        DWORD ImageSize = (DWORD)cbImage; Assert(ImageSize == cbImage);
        DWORD64 LoadedImageBase = SymLoadModule64(pVM, NULL, (char *)(void *)pszFilename, (char *)(void *)pszName, NewImageBase, ImageSize);
        if (!LoadedImageBase)
            Log(("SymLoadModule64(,,%s,,) -> lasterr=%d (relocate)\n", pszFilename, GetLastError()));
        else
            Log(("Reloaded debuginfo for %s - %s %llx\n", pszName, pszFilename, LoadedImageBase));
    }
#else

#endif
}


/**
 * Adds a symbol to the debug info manager.
 *
 * @returns VBox status.
 * @param   pVM             VM Handle.
 * @param   ModuleAddress   Module address. Use 0 if no module.
 * @param   SymbolAddress   Symbol address
 * @param   cbSymbol        Size of the symbol. Use 0 if info not available.
 * @param   pszSymbol       Symbol name.
 */
VMMR3DECL(int) DBGFR3SymbolAdd(PVM pVM, RTGCUINTPTR ModuleAddress, RTGCUINTPTR SymbolAddress, RTUINT cbSymbol, const char *pszSymbol)
{
    /*
     * Validate.
     */
    if (!pszSymbol || !*pszSymbol)
    {
        AssertMsgFailed(("No symbol name!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Lazy init.
     */
    if (!pVM->dbgf.s.fSymInited)
    {
        int rc = dbgfR3SymLazyInit(pVM);
        if (RT_FAILURE(rc))
            return rc;
    }

#ifdef HAVE_DBGHELP
    if (SymAddSymbol(pVM, ModuleAddress, (char *)(void *)pszSymbol, SymbolAddress, cbSymbol, 0))
        return VINF_SUCCESS;
    return win32Error(pVM);
#else
    /** @todo module lookup. */
    return dbgfR3SymbolInsert(pVM, pszSymbol, SymbolAddress, cbSymbol, NULL);
#endif
}


/**
 * Find symbol by address (nearest).
 *
 * @returns VBox status.
 * @param   pVM                 VM handle.
 * @param   Address             Address.
 * @param   poffDisplacement    Where to store the symbol displacement from Address.
 * @param   pSymbol             Where to store the symbol info.
 */
VMMR3DECL(int) DBGFR3SymbolByAddr(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement, PDBGFSYMBOL pSymbol)
{
    /*
     * Lazy init.
     */
    if (!pVM->dbgf.s.fSymInited)
    {
        int rc = dbgfR3SymLazyInit(pVM);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Look it up.
     */
#ifdef HAVE_DBGHELP
    char                achBuffer[sizeof(IMAGEHLP_SYMBOL64) + DBGF_SYMBOL_NAME_LENGTH * sizeof(TCHAR) + sizeof(ULONG64)];
    PIMAGEHLP_SYMBOL64  pSym = (PIMAGEHLP_SYMBOL64)&achBuffer[0];
    pSym->SizeOfStruct      = sizeof(IMAGEHLP_SYMBOL64);
    pSym->MaxNameLength     = DBGF_SYMBOL_NAME_LENGTH;

    if (SymGetSymFromAddr64(pVM, Address, (PDWORD64)poffDisplacement, pSym))
    {
        pSymbol->Value  = (RTGCUINTPTR)pSym->Address;
        pSymbol->cb     = pSym->Size;
        pSymbol->fFlags = pSym->Flags;
        strcpy(pSymbol->szName, pSym->Name);
        return VINF_SUCCESS;
    }
    //return win32Error(pVM);

#else

    PDBGFSYM pSym = dbgfR3SymbolGetAddr(pVM, Address);
    if (pSym)
    {
        pSymbol->Value = pSym->Core.Key;
        pSymbol->cb = pSym->Core.KeyLast - pSym->Core.Key + 1;
        pSymbol->fFlags = 0;
        pSymbol->szName[0] = '\0';
        strncat(pSymbol->szName, pSym->szName,  sizeof(pSymbol->szName) - 1);
        if (poffDisplacement)
            *poffDisplacement = Address - pSymbol->Value;
        return VINF_SUCCESS;
    }

#endif

    /*
     * Try PDM.
     */
    if (MMHyperIsInsideArea(pVM, Address))
    {
        char        szModName[64];
        RTRCPTR     RCPtrMod;
        char        szNearSym1[260];
        RTRCPTR     RCPtrNearSym1;
        char        szNearSym2[260];
        RTRCPTR     RCPtrNearSym2;
        int rc = PDMR3LdrQueryRCModFromPC(pVM, Address,
                                          &szModName[0],  sizeof(szModName),  &RCPtrMod,
                                          &szNearSym1[0], sizeof(szNearSym1), &RCPtrNearSym1,
                                          &szNearSym2[0], sizeof(szNearSym2), &RCPtrNearSym2);
        if (RT_SUCCESS(rc) && szNearSym1[0])
        {
            pSymbol->Value = RCPtrNearSym1;
            pSymbol->cb = RCPtrNearSym2 > RCPtrNearSym1 ? RCPtrNearSym2 - RCPtrNearSym1 : 0;
            pSymbol->fFlags = 0;
            pSymbol->szName[0] = '\0';
            strncat(pSymbol->szName, szNearSym1,  sizeof(pSymbol->szName) - 1);
            if (poffDisplacement)
                *poffDisplacement = Address - pSymbol->Value;
            return VINF_SUCCESS;
        }
    }

    return VERR_SYMBOL_NOT_FOUND;
}


/**
 * Find symbol by name (first).
 *
 * @returns VBox status.
 * @param   pVM                 VM handle.
 * @param   pszSymbol           Symbol name.
 * @param   pSymbol             Where to store the symbol info.
 */
VMMR3DECL(int) DBGFR3SymbolByName(PVM pVM, const char *pszSymbol, PDBGFSYMBOL pSymbol)
{
    /*
     * Lazy init.
     */
    if (!pVM->dbgf.s.fSymInited)
    {
        int rc = dbgfR3SymLazyInit(pVM);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Look it up.
     */
#ifdef HAVE_DBGHELP
    char            achBuffer[sizeof(IMAGEHLP_SYMBOL64) + DBGF_SYMBOL_NAME_LENGTH * sizeof(TCHAR) + sizeof(ULONG64)];
    PIMAGEHLP_SYMBOL64 pSym = (PIMAGEHLP_SYMBOL64)&achBuffer[0];
    pSym->SizeOfStruct      = sizeof(IMAGEHLP_SYMBOL64);
    pSym->MaxNameLength     = DBGF_SYMBOL_NAME_LENGTH;

    if (SymGetSymFromName64(pVM, (char *)(void *)pszSymbol, pSym))
    {
        pSymbol->Value  = (RTGCUINTPTR)pSym->Address;
        pSymbol->cb     = pSym->Size;
        pSymbol->fFlags = pSym->Flags;
        strcpy(pSymbol->szName, pSym->Name);
        return VINF_SUCCESS;
    }
    return win32Error(pVM);
#else

    PDBGFSYM pSym = dbgfR3SymbolGetName(pVM, pszSymbol);
    if (pSym)
    {
        pSymbol->Value = pSym->Core.Key;
        pSymbol->cb = pSym->Core.KeyLast - pSym->Core.Key + 1;
        pSymbol->fFlags = 0;
        pSymbol->szName[0] = '\0';
        strncat(pSymbol->szName, pSym->szName, sizeof(pSymbol->szName) - 1);
        return VINF_SUCCESS;
    }

    return VERR_SYMBOL_NOT_FOUND;
#endif
}


/**
 * Duplicates a symbol.
 *
 * @returns Pointer to the duplicated symbol.
 * @param   pVM             The VM handle.
 * @param   pSymbol         The symbol to duplicate.
 */
static PDBGFSYMBOL dbgfR3SymbolDup(PVM pVM, PCDBGFSYMBOL pSymbol)
{
    size_t cb = strlen(pSymbol->szName) + RT_OFFSETOF(DBGFSYMBOL, szName[1]);
    PDBGFSYMBOL pDup = (PDBGFSYMBOL)MMR3HeapAlloc(pVM, MM_TAG_DBGF_SYMBOL_DUP, cb);
    if (pDup)
        memcpy(pDup, pSymbol, cb);
    return pDup;
}


/**
 * Find symbol by address (nearest), allocate return buffer.
 *
 * @returns Pointer to the symbol. Must be freed using DBGFR3SymbolFree().
 * @returns NULL if the symbol was not found or if we're out of memory.
 * @param   pVM                 VM handle.
 * @param   Address             Address.
 * @param   poffDisplacement    Where to store the symbol displacement from Address.
 */
VMMR3DECL(PDBGFSYMBOL) DBGFR3SymbolByAddrAlloc(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement)
{
    DBGFSYMBOL Symbol;
    int rc = DBGFR3SymbolByAddr(pVM, Address, poffDisplacement, &Symbol);
    if (RT_FAILURE(rc))
        return NULL;
    return dbgfR3SymbolDup(pVM, &Symbol);
}


/**
 * Find symbol by name (first), allocate return buffer.
 *
 * @returns Pointer to the symbol. Must be freed using DBGFR3SymbolFree().
 * @returns NULL if the symbol was not found or if we're out of memory.
 * @param   pVM                 VM handle.
 * @param   pszSymbol           Symbol name.
 */
VMMR3DECL(PDBGFSYMBOL) DBGFR3SymbolByNameAlloc(PVM pVM, const char *pszSymbol)
{
    DBGFSYMBOL Symbol;
    int rc = DBGFR3SymbolByName(pVM, pszSymbol, &Symbol);
    if (RT_FAILURE(rc))
        return NULL;
    return dbgfR3SymbolDup(pVM, &Symbol);
}


/**
 * Frees a symbol returned by DBGFR3SymbolbyNameAlloc() or DBGFR3SymbolByAddressAlloc().
 *
 * @param   pSymbol         Pointer to the symbol.
 */
VMMR3DECL(void) DBGFR3SymbolFree(PDBGFSYMBOL pSymbol)
{
    if (pSymbol)
        MMR3HeapFree(pSymbol);
}


/**
 * Find line by address (nearest).
 *
 * @returns VBox status.
 * @param   pVM                 VM handle.
 * @param   Address             Address.
 * @param   poffDisplacement    Where to store the line displacement from Address.
 * @param   pLine               Where to store the line info.
 */
VMMR3DECL(int) DBGFR3LineByAddr(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement, PDBGFLINE pLine)
{
    /*
     * Lazy init.
     */
    if (!pVM->dbgf.s.fSymInited)
    {
        int rc = dbgfR3SymLazyInit(pVM);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Look it up.
     */
#ifdef HAVE_DBGHELP
    IMAGEHLP_LINE64     Line = {0};
    DWORD               off = 0;
    Line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    if (SymGetLineFromAddr64(pVM, Address, &off, &Line))
    {
        if (poffDisplacement)
            *poffDisplacement = (long)off;
        pLine->Address      = (RTGCUINTPTR)Line.Address;
        pLine->uLineNo      = Line.LineNumber;
        pLine->szFilename[0] = '\0';
        strncat(pLine->szFilename, Line.FileName, sizeof(pLine->szFilename));
        return VINF_SUCCESS;
    }
    return win32Error(pVM);
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * Duplicates a line.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pLine           The line to duplicate.
 */
static PDBGFLINE dbgfR3LineDup(PVM pVM, PCDBGFLINE pLine)
{
    size_t cb = strlen(pLine->szFilename) + RT_OFFSETOF(DBGFLINE, szFilename[1]);
    PDBGFLINE pDup = (PDBGFLINE)MMR3HeapAlloc(pVM, MM_TAG_DBGF_LINE_DUP, cb);
    if (pDup)
        memcpy(pDup, pLine, cb);
    return pDup;
}


/**
 * Find line by address (nearest), allocate return buffer.
 *
 * @returns Pointer to the line. Must be freed using DBGFR3LineFree().
 * @returns NULL if the line was not found or if we're out of memory.
 * @param   pVM                 VM handle.
 * @param   Address             Address.
 * @param   poffDisplacement    Where to store the line displacement from Address.
 */
VMMR3DECL(PDBGFLINE) DBGFR3LineByAddrAlloc(PVM pVM, RTGCUINTPTR Address, PRTGCINTPTR poffDisplacement)
{
    DBGFLINE Line;
    int rc = DBGFR3LineByAddr(pVM, Address, poffDisplacement, &Line);
    if (RT_FAILURE(rc))
        return NULL;
    return dbgfR3LineDup(pVM, &Line);
}


/**
 * Frees a line returned by DBGFR3LineByAddressAlloc().
 *
 * @param   pLine           Pointer to the line.
 */
VMMR3DECL(void) DBGFR3LineFree(PDBGFLINE pLine)
{
    if (pLine)
        MMR3HeapFree(pLine);
}


#ifdef HAVE_DBGHELP

//static BOOL CALLBACK win32EnumModulesCallback(PSTR ModuleName, DWORD64 BaseOfDll, PVOID UserContext)
//{
//    Log(("dbg: module: %08llx %s\n", ModuleName, BaseOfDll));
//    return TRUE;
//}

static int win32Error(PVM pVM)
{
    int rc = GetLastError();
    Log(("Lasterror=%d\n", rc));

    //SymEnumerateModules64(pVM, win32EnumModulesCallback, NULL);

    return VERR_GENERAL_FAILURE;
}
#endif

