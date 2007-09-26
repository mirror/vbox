/* $Id$ */
/** @file
 * PDM - Pluggable Device Manager, module loader.
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

//#define PDMLDR_FAKE_MODE

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM_LDR
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/vmm.h>
#include <VBox/vm.h>
#include <VBox/sup.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/hwaccm.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <limits.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Structure which the user argument of the RTLdrGetBits() callback points to.
 * @internal
 */
typedef struct PDMGETIMPORTARGS
{
    PVM         pVM;
    PPDMMOD     pModule;
} PDMGETIMPORTARGS, *PPDMGETIMPORTARGS;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) pdmr3GetImportGC(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser);
static int      pdmR3LoadR0(PVM pVM, const char *pszFilename, const char *pszName);
static char *   pdmR3FileGC(const char *pszFile);
static char *   pdmR3FileR0(const char *pszFile);
static char *   pdmR3File(const char *pszFile, const char *pszDefaultExt, bool fShared);
static DECLCALLBACK(int) pdmR3QueryModFromEIPEnumSymbols(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTUINTPTR Value, void *pvUser);



/**
 * Loads the VMMR0.r0 module before the VM is created.
 *
 * The opqaue VMMR0 module pointer is passed on to PDMR3Init later in
 * the init process or PDMR3LdrUnloadVMMR0 in case of some init failure before PDMR3Init.
 *
 * @returns VBox status code.
 * @param   ppvOpaque       Where to return the opaque VMMR0.r0 module handle one success.
 *
 * @remarks Yes, this is a kind of hacky and should go away. See @todo in VMR3Create.
 */
PDMR3DECL(int) PDMR3LdrLoadVMMR0(void **ppvOpaque)
{
    *ppvOpaque = NULL;

    /*
     * Resolve the filename and allocate the module list node.
     */
    char *pszFilename = pdmR3FileR0(VMMR0_MAIN_MODULE_NAME);
    PPDMMOD pModule = (PPDMMOD)RTMemAllocZ(sizeof(*pModule) + strlen(pszFilename));
    if (!pModule)
    {
        RTMemTmpFree(pszFilename);
        return VERR_NO_MEMORY;
    }
    strcpy(pModule->szName, VMMR0_MAIN_MODULE_NAME);
    pModule->eType = PDMMOD_TYPE_R0;
    strcpy(pModule->szFilename, pszFilename);
    RTMemTmpFree(pszFilename);

    /*
     * Ask the support library to load it.
     */
    void *pvImageBase;
    int rc = SUPLoadModule(pModule->szFilename, pModule->szName, &pvImageBase);
    if (RT_SUCCESS(rc))
    {
        pModule->hLdrMod = NIL_RTLDRMOD;
        pModule->ImageBase = (uintptr_t)pvImageBase;
        *ppvOpaque = pModule;

        Log(("PDMR3LdrLoadVMMR0: Loaded %s at %VGvx (%s)\n", pModule->szName, (RTGCPTR)pModule->ImageBase, pModule->szFilename));
        return VINF_SUCCESS;
    }

    LogRel(("PDMR3LdrLoadVMMR0: rc=%Vrc szName=%s szFilename=%s\n", rc, pModule->szName, pModule->szFilename));
    RTMemFree(pModule);
    return rc;
}


/**
 * Register the VMMR0.r0 module with the created VM or unload it if
 * we failed to create the VM (pVM == NULL).
 *
 * @param   pVM         The VM pointer. NULL if we failed to create the VM and
 *                      the module should be unloaded and freed.
 * @param   pvOpaque    The value returned by PDMR3LDrLoadVMMR0().
 *
 * @remarks Yes, this is a kind of hacky and should go away. See @todo in VMR3Create.
 */
PDMR3DECL(void) PDMR3LdrLoadVMMR0Part2(PVM pVM, void *pvOpaque)
{
    PPDMMOD pModule = (PPDMMOD)pvOpaque;
    AssertPtrReturnVoid(pModule);

    if (pVM)
    {
        /*
         * Register the R0 module loaded by PDMR3LdrLoadVMMR0
         */
        Assert(!pVM->pdm.s.pModules);
        pModule->pNext = pVM->pdm.s.pModules;
        pVM->pdm.s.pModules = pModule;
    }
    else
    {
        /*
         * Failed, unload the module.
         */
        int rc2 = SUPFreeModule((void *)(uintptr_t)pModule->ImageBase);
        AssertRC(rc2);
        pModule->ImageBase = 0;
        RTMemFree(pvOpaque);
    }
}


/**
 * Init the module loader part of PDM.
 *
 * This routine will load the Host Context Ring-0 and Guest
 * Context VMM modules.
 *
 * @returns VBox stutus code.
 * @param   pVM         VM handle.
 * @param   pvVMMR0Mod  The opqaue returned by PDMR3LdrLoadVMMR0.
 */
int pdmR3LdrInit(PVM pVM)
{
#ifdef PDMLDR_FAKE_MODE
    return VINF_SUCCESS;

#else

    /*
     * Load the mandatory GC module, the VMMR0.r0 is loaded before VM creation.
     */
    return PDMR3LoadGC(pVM, NULL, VMMGC_MAIN_MODULE_NAME);
#endif
}


/**
 * Terminate the module loader part of PDM.
 *
 * This will unload and free all modules.
 *
 * @param   pVM         The VM handle.
 */
void pdmR3LdrTerm(PVM pVM)
{
    /*
     * Free the modules.
     */
    PPDMMOD pModule = pVM->pdm.s.pModules;
    while (pModule)
    {
        /* free loader item. */
        if (pModule->hLdrMod != NIL_RTLDRMOD)
        {
            int rc2 = RTLdrClose(pModule->hLdrMod);
            AssertRC(rc2);
            pModule->hLdrMod = NIL_RTLDRMOD;
        }

        /* free bits. */
        switch (pModule->eType)
        {
            case PDMMOD_TYPE_R0:
            {
                Assert(pModule->ImageBase);
                int rc2 = SUPFreeModule((void *)(uintptr_t)pModule->ImageBase);
                AssertRC(rc2);
                pModule->ImageBase = 0;
                break;
            }

            case PDMMOD_TYPE_GC:
            case PDMMOD_TYPE_R3:
                /* MM will free this memory for us - it's alloc only memory. :-) */
                break;

            default:
                AssertMsgFailed(("eType=%d\n", pModule->eType));
                break;
        }
        pModule->pvBits = NULL;

        void    *pvFree = pModule;
        pModule = pModule->pNext;
        RTMemFree(pvFree);
    }
}


/**
 * Applies relocations to GC modules.
 *
 * This must be done very early in the relocation
 * process so that components can resolve GC symbols during relocation.
 *
 * @param   pVM         VM handle.
 * @param   offDelta    Relocation delta relative to old location.
 */
PDMR3DECL(void) PDMR3LdrRelocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("PDMR3LdrRelocate: offDelta=%VGv\n", offDelta));

    /*
     * GC Modules.
     */
    if (pVM->pdm.s.pModules)
    {
        /*
         * The relocation have to be done in two passes so imports
         * can be correctely resolved. The first pass will update
         * the ImageBase saving the current value in OldImageBase.
         * The second pass will do the actual relocation.
         */
        /* pass 1 */
        PPDMMOD pCur;
        for (pCur = pVM->pdm.s.pModules; pCur; pCur = pCur->pNext)
        {
            if (pCur->eType == PDMMOD_TYPE_GC)
            {
                pCur->OldImageBase = pCur->ImageBase;
                pCur->ImageBase = MMHyperHC2GC(pVM, pCur->pvBits);
            }
        }

        /* pass 2 */
        for (pCur = pVM->pdm.s.pModules; pCur; pCur = pCur->pNext)
        {
            if (pCur->eType == PDMMOD_TYPE_GC)
            {
                PDMGETIMPORTARGS Args;
                Args.pVM = pVM;
                Args.pModule = pCur;
                int rc = RTLdrRelocate(pCur->hLdrMod, pCur->pvBits, pCur->ImageBase, pCur->OldImageBase,
                                       pdmr3GetImportGC, &Args);
                AssertFatalMsgRC(rc, ("RTLdrRelocate failed, rc=%d\n", rc));
                DBGFR3ModuleRelocate(pVM, pCur->OldImageBase, pCur->ImageBase, RTLdrSize(pCur->hLdrMod),
                                     pCur->szFilename, pCur->szName);
            }
        }
    }
}


/**
 * Loads a module into the host context ring-3.
 *
 * This is used by the driver and device init functions to load modules
 * containing the drivers and devices. The function can be extended to
 * load modules which are not native to the environment we're running in,
 * but at the moment this is not required.
 *
 * No reference counting is kept, since we don't implement any facilities
 * for unloading the module. But the module will naturally be released
 * when the VM terminates.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to load it into.
 * @param   pszFilename     Filename of the module binary.
 * @param   pszName         Module name. Case sensitive and the length is limited!
 */
int pdmR3LoadR3(PVM pVM, const char *pszFilename, const char *pszName)
{
    /*
     * Validate input.
     */
    AssertMsg(pVM->pdm.s.offVM, ("bad init order!\n"));
    Assert(pszFilename);
    size_t cchFilename = strlen(pszFilename);
    Assert(pszName);
    size_t cchName = strlen(pszName);
    PPDMMOD  pCur;
    if (cchName >= sizeof(pCur->szName))
    {
        AssertMsgFailed(("Name is too long, cchName=%d pszName='%s'\n", cchName, pszName));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Try lookup the name and see if the module exists.
     */
    for (pCur = pVM->pdm.s.pModules; pCur; pCur = pCur->pNext)
    {
        if (!strcmp(pCur->szName, pszName))
        {
            if (pCur->eType == PDMMOD_TYPE_R3)
                return VINF_PDM_ALREADY_LOADED;
            AssertMsgFailed(("We've already got a module '%s' loaded!\n", pszName));
            return VERR_PDM_MODULE_NAME_CLASH;
        }
    }

    /*
     * Allocate the module list node and initialize it.
     */
    PPDMMOD     pModule = (PPDMMOD)RTMemAllocZ(sizeof(*pModule) + cchFilename);
    if (!pModule)
        return VERR_NO_MEMORY;

    pModule->eType = PDMMOD_TYPE_R3;
    memcpy(pModule->szName, pszName, cchName); /* memory is zero'ed, no need to copy terminator :-) */
    memcpy(pModule->szFilename, pszFilename, cchFilename);

    /*
     * Load the loader item.
     */
    int rc = RTLdrLoad(pszFilename, &pModule->hLdrMod);
    if (VBOX_SUCCESS(rc))
    {
        pModule->pNext = pVM->pdm.s.pModules;
        pVM->pdm.s.pModules = pModule;
        return rc;
    }

    /* Something went wrong, most likely module not found. Don't consider other unlikely errors */
    RTMemFree(pModule);
    return VMSetError(pVM, rc, RT_SRC_POS, N_("Unable to load R3 module %s"), pszFilename);
}


/**
 * Resolve an external symbol during RTLdrGetBits() of a GC module.
 *
 * @returns VBox status code.
 * @param   hLdrMod         The loader module handle.
 * @param   pszModule       Module name.
 * @param   pszSymbol       Symbol name, NULL if uSymbol should be used.
 * @param   uSymbol         Symbol ordinal, ~0 if pszSymbol should be used.
 * @param   pValue          Where to store the symbol value (address).
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) pdmr3GetImportGC(RTLDRMOD hLdrMod, const char *pszModule, const char *pszSymbol, unsigned uSymbol, RTUINTPTR *pValue, void *pvUser)
{
    PVM         pVM = ((PPDMGETIMPORTARGS)pvUser)->pVM;
    PPDMMOD     pModule = ((PPDMGETIMPORTARGS)pvUser)->pModule;

    /*
     * Adjust input.
     */
    if (pszModule && !*pszModule)
        pszModule = NULL;

    /*
     * Builtin module.
     */
    if (!pszModule || !strcmp(pszModule, "VMMGCBuiltin.gc"))
    {
        int rc = VINF_SUCCESS;
        if (!strcmp(pszSymbol, "g_VM"))
            *pValue = pVM->pVMGC;
        else if (!strcmp(pszSymbol, "g_CPUM"))
            *pValue = VM_GUEST_ADDR(pVM, &pVM->cpum);
        else if (!strcmp(pszSymbol, "g_TRPM"))
            *pValue = VM_GUEST_ADDR(pVM, &pVM->trpm);
        else if (   !strncmp(pszSymbol, "VMM", 3)
                 || !strcmp(pszSymbol, "g_Logger")
                 || !strcmp(pszSymbol, "g_RelLogger"))
        {
            RTGCPTR GCPtr = 0;
            rc = VMMR3GetImportGC(pVM, pszSymbol, &GCPtr);
            if (VBOX_SUCCESS(rc))
                *pValue = GCPtr;
        }
        else if (   !strncmp(pszSymbol, "TM", 2)
                 || !strcmp(pszSymbol, "g_pSUPGlobalInfoPage"))
        {
            RTGCPTR GCPtr = 0;
            rc = TMR3GetImportGC(pVM, pszSymbol, &GCPtr);
            if (VBOX_SUCCESS(rc))
                *pValue = GCPtr;
        }
        else
        {
            AssertMsg(!pszModule, ("Unknown builtin symbol '%s' for module '%s'!\n", pszSymbol, pModule->szName)); NOREF(pModule);
            rc = VERR_SYMBOL_NOT_FOUND;
        }
        if (VBOX_SUCCESS(rc) || pszModule)
            return rc;
    }

    /*
     * Search for module.
     */
    PPDMMOD  pCur = pVM->pdm.s.pModules;
    while (pCur)
    {
        if (    pCur->eType == PDMMOD_TYPE_GC
            &&  (   !pszModule
                 || !strcmp(pCur->szName, pszModule))
           )
        {
            /* Search for the symbol. */
            int rc = RTLdrGetSymbolEx(pCur->hLdrMod, pCur->pvBits, pCur->ImageBase, pszSymbol, pValue);
            if (VBOX_SUCCESS(rc))
            {
                AssertMsg(*pValue - pCur->ImageBase < RTLdrSize(pCur->hLdrMod),
                          ("%VGv-%VGv %s %VGv\n", (RTGCPTR)pCur->ImageBase,
                           (RTGCPTR)(pCur->ImageBase + RTLdrSize(pCur->hLdrMod) - 1),
                           pszSymbol, (RTGCPTR)*pValue));
                return rc;
            }
            if (pszModule)
            {
                AssertMsgFailed(("Couldn't find symbol '%s' in module '%s'!\n", pszSymbol, pszModule));
                LogRel(("PDMLdr: Couldn't find symbol '%s' in module '%s'!\n", pszSymbol, pszModule));
                return VERR_SYMBOL_NOT_FOUND;
            }
        }

        /* next */
        pCur = pCur->pNext;
    }

    AssertMsgFailed(("Couldn't find module '%s' for resolving symbol '%s'!\n", pszModule, pszSymbol));
    return VERR_SYMBOL_NOT_FOUND;
}


/**
 * Loads a module into the guest context (i.e. into the Hypervisor memory region).
 *
 * The external (to PDM) use of this interface is to load VMMGC.gc.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to load it into.
 * @param   pszFilename     Filename of the module binary.
 * @param   pszName         Module name. Case sensitive and the length is limited!
 */
PDMR3DECL(int) PDMR3LoadGC(PVM pVM, const char *pszFilename, const char *pszName)
{
    /*
     * Validate input.
     */
    AssertMsg(pVM->pdm.s.offVM, ("bad init order!\n"));
    PPDMMOD  pCur = pVM->pdm.s.pModules;
    while (pCur)
    {
        if (!strcmp(pCur->szName, pszName))
        {
            AssertMsgFailed(("We've already got a module '%s' loaded!\n", pszName));
            return VERR_PDM_MODULE_NAME_CLASH;
        }
        /* next */
        pCur = pCur->pNext;
    }

    /*
     * Find the file if not specified.
     */
    char *pszFile = NULL;
    if (!pszFilename)
        pszFilename = pszFile = pdmR3FileGC(pszName);

    /*
     * Allocate the module list node.
     */
    PPDMMOD     pModule = (PPDMMOD)RTMemAllocZ(sizeof(*pModule) + strlen(pszFilename));
    if (!pModule)
    {
        RTMemTmpFree(pszFile);
        return VERR_NO_MEMORY;
    }
    AssertMsg(strlen(pszName) + 1 < sizeof(pModule->szName),
              ("pazName is too long (%d chars) max is %d chars.\n", strlen(pszName), sizeof(pModule->szName) - 1));
    strcpy(pModule->szName, pszName);
    pModule->eType = PDMMOD_TYPE_GC;
    strcpy(pModule->szFilename, pszFilename);


    /*
     * Open the loader item.
     */
    int rc = RTLdrOpen(pszFilename, &pModule->hLdrMod);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Allocate space in the hypervisor.
         */
        size_t cb = RTLdrSize(pModule->hLdrMod);
        cb = RT_ALIGN_Z(cb, PAGE_SIZE);
        rc = SUPPageAlloc(cb >> PAGE_SHIFT, &pModule->pvBits);
        if (VBOX_SUCCESS(rc))
        {
            RTGCPTR GCPtr;
            rc = MMR3HyperMapHCRam(pVM, pModule->pvBits, cb, true, pModule->szName, &GCPtr);
            if (VBOX_SUCCESS(rc))
            {
                MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);

                /*
                 * Get relocated image bits.
                 */
                Assert(MMHyperHC2GC(pVM, pModule->pvBits) == GCPtr);
                pModule->ImageBase = GCPtr;
                PDMGETIMPORTARGS Args;
                Args.pVM = pVM;
                Args.pModule = pModule;
                rc = RTLdrGetBits(pModule->hLdrMod, pModule->pvBits, pModule->ImageBase, pdmr3GetImportGC, &Args);
                if (VBOX_SUCCESS(rc))
                {
                    /*
                     * Insert the module.
                     */
                    if (pVM->pdm.s.pModules)
                    {
                        /* we don't expect this list to be very long, so rather save the tail pointer. */
                        PPDMMOD pCur = pVM->pdm.s.pModules;
                        while (pCur->pNext)
                            pCur = pCur->pNext;
                        pCur->pNext = pModule;
                    }
                    else
                        pVM->pdm.s.pModules = pModule; /* (pNext is zeroed by alloc) */
                    Log(("PDM: GC Module at %VGvx %s (%s)\n", (RTGCPTR)pModule->ImageBase, pszName, pszFilename));
                    RTMemTmpFree(pszFile);
                    return VINF_SUCCESS;
                }
            }
            else
            {
                AssertRC(rc);
                SUPPageFree(pModule->pvBits, cb >> PAGE_SHIFT);
            }
        }
        else
            AssertMsgFailed(("SUPPageAlloc(%d,) -> %Vrc\n", cb >> PAGE_SHIFT, rc));
        int rc2 = RTLdrClose(pModule->hLdrMod);
        AssertRC(rc2);
    }
    RTMemFree(pModule);
    RTMemTmpFree(pszFile);

    /* Don't consider VERR_PDM_MODULE_NAME_CLASH and VERR_NO_MEMORY above as these are very unlikely. */
    if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS, N_("Cannot load GC module %s"), pszFilename);
    return rc;
}


/**
 * Loads a module into the ring-0 context.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to load it into.
 * @param   pszFilename     Filename of the module binary.
 * @param   pszName         Module name. Case sensitive and the length is limited!
 */
static int pdmR3LoadR0(PVM pVM, const char *pszFilename, const char *pszName)
{
    /*
     * Validate input.
     */
    AssertMsg(pVM->pdm.s.offVM, ("bad init order!\n"));
    PPDMMOD  pCur = pVM->pdm.s.pModules;
    while (pCur)
    {
        if (!strcmp(pCur->szName, pszName))
        {
            AssertMsgFailed(("We've already got a module '%s' loaded!\n", pszName));
            return VERR_PDM_MODULE_NAME_CLASH;
        }
        /* next */
        pCur = pCur->pNext;
    }
#if 0 /* breaks VT-x */
    AssertReturn(!strcmp(pszName, VMMR0_MAIN_MODULE_NAME), VERR_INTERNAL_ERROR);
#endif

    /*
     * Find the file if not specified.
     */
    char *pszFile = NULL;
    if (!pszFilename)
        pszFilename = pszFile = pdmR3FileR0(pszName);

    /*
     * Allocate the module list node.
     */
    PPDMMOD     pModule = (PPDMMOD)RTMemAllocZ(sizeof(*pModule) + strlen(pszFilename));
    if (!pModule)
    {
        RTMemTmpFree(pszFile);
        return VERR_NO_MEMORY;
    }
    AssertMsg(strlen(pszName) + 1 < sizeof(pModule->szName),
              ("pazName is too long (%d chars) max is %d chars.\n", strlen(pszName), sizeof(pModule->szName) - 1));
    strcpy(pModule->szName, pszName);
    pModule->eType = PDMMOD_TYPE_R0;
    strcpy(pModule->szFilename, pszFilename);

    /*
     * Ask the support library to load it.
     */
    void *pvImageBase;
    int rc = SUPLoadModule(pszFilename, pszName, &pvImageBase);
    if (VBOX_SUCCESS(rc))
    {
        pModule->hLdrMod = NIL_RTLDRMOD;
        pModule->ImageBase = (uintptr_t)pvImageBase;

        /*
         * Insert the module.
         */
        if (pVM->pdm.s.pModules)
        {
            /* we don't expect this list to be very long, so rather save the tail pointer. */
            PPDMMOD pCur = pVM->pdm.s.pModules;
            while (pCur->pNext)
                pCur = pCur->pNext;
            pCur->pNext = pModule;
        }
        else
            pVM->pdm.s.pModules = pModule; /* (pNext is zeroed by alloc) */
        Log(("PDM: GC Module at %VGvx %s (%s)\n", (RTGCPTR)pModule->ImageBase, pszName, pszFilename));
        RTMemTmpFree(pszFile);
        return VINF_SUCCESS;
    }

    RTMemFree(pModule);
    RTMemTmpFree(pszFile);
    LogRel(("pdmR3LoadR0: pszName=\"%s\" rc=%Vrc\n", pszName, rc));

    /* Don't consider VERR_PDM_MODULE_NAME_CLASH and VERR_NO_MEMORY above as these are very unlikely. */
    if (VBOX_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS, N_("Cannot load R0 module %s"), pszFilename);
    return rc;
}



/**
 * Get the address of a symbol in a given HC ring 3 module.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolR3(PVM pVM, const char *pszModule, const char *pszSymbol, void **ppvValue)
{
    /*
     * Validate input.
     */
    AssertMsg(pVM->pdm.s.offVM, ("bad init order!\n"));

    /*
     * Find the module.
     */
    for (PPDMMOD pModule = pVM->pdm.s.pModules; pModule; pModule = pModule->pNext)
    {
        if (    pModule->eType == PDMMOD_TYPE_R3
            &&  !strcmp(pModule->szName, pszModule))
        {
            RTUINTPTR Value = 0;
            int rc = RTLdrGetSymbolEx(pModule->hLdrMod, pModule->pvBits, pModule->ImageBase, pszSymbol, &Value);
            if (VBOX_SUCCESS(rc))
            {
                *ppvValue = (void *)(uintptr_t)Value;
                Assert((uintptr_t)*ppvValue == Value);
            }
            else
            {
                if (pszSymbol < (const char*)(void*)0x10000)
                    AssertMsg(rc, ("Couldn't symbol '%u' in module '%s'\n", (unsigned)(uintptr_t)pszSymbol, pszModule));
                else
                    AssertMsg(rc, ("Couldn't symbol '%s' in module '%s'\n", pszSymbol, pszModule));
            }
            return rc;
        }
    }

    AssertMsgFailed(("Couldn't locate module '%s'\n", pszModule));
    return VERR_SYMBOL_NOT_FOUND;
}


/**
 * Get the address of a symbol in a given HC ring 0 module.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main R0 module (VMMR0.r0) is assumes.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolR0(PVM pVM, const char *pszModule, const char *pszSymbol, PRTR0PTR ppvValue)
{
#ifdef PDMLDR_FAKE_MODE
    *ppvValue = 0xdeadbeef;
    return VINF_SUCCESS;

#else
    /*
     * Validate input.
     */
    AssertMsg(pVM->pdm.s.offVM, ("bad init order!\n"));
    if (!pszModule)
        pszModule = "VMMR0.r0";

    /*
     * Find the module.
     */
    for (PPDMMOD pModule = pVM->pdm.s.pModules; pModule; pModule = pModule->pNext)
    {
        if (    pModule->eType == PDMMOD_TYPE_R0
            &&  !strcmp(pModule->szName, pszModule))
        {
            int rc = SUPGetSymbolR0((void *)(uintptr_t)pModule->ImageBase, pszSymbol, (void **)ppvValue);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgRC(rc, ("Couldn't find symbol '%s' in module '%s'\n", pszSymbol, pszModule));
                LogRel(("PDMGetSymbol: Couldn't find symbol '%s' in module '%s'\n", pszSymbol, pszModule));
            }
            return rc;
        }
    }

    AssertMsgFailed(("Couldn't locate module '%s'\n", pszModule));
    return VERR_SYMBOL_NOT_FOUND;
#endif
}


/**
 * Same as PDMR3GetSymbolR0 except that the module will be attempted loaded if not found.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main R0 module (VMMR0.r0) is assumed.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   ppvValue        Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolR0Lazy(PVM pVM, const char *pszModule, const char *pszSymbol, PRTR0PTR ppvValue)
{
#ifdef PDMLDR_FAKE_MODE
    *ppvValue = 0xdeadbeef;
    return VINF_SUCCESS;

#else
    /*
     * Since we're lazy, we'll only check if the module is present
     * and hand it over to PDMR3GetSymbolR0 when that's done.
     */
    AssertMsg(pVM->pdm.s.offVM, ("bad init order!\n"));
    if (pszModule)
    {
        AssertMsgReturn(!strpbrk(pszModule, "/\\:\n\r\t"), ("pszModule=%s\n", pszModule), VERR_INVALID_PARAMETER);
        PPDMMOD pModule;
        for (pModule = pVM->pdm.s.pModules; pModule; pModule = pModule->pNext)
            if (    pModule->eType == PDMMOD_TYPE_R0
                &&  !strcmp(pModule->szName, pszModule))
                break;
        if (!pModule)
        {
            int rc = pdmR3LoadR0(pVM, NULL, pszModule);
            AssertMsgRCReturn(rc, ("pszModule=%s rc=%Vrc\n", pszModule, rc), VERR_MODULE_NOT_FOUND);
        }
    }
    return PDMR3GetSymbolR0(pVM, pszModule, pszSymbol, ppvValue);
#endif
}


/**
 * Get the address of a symbol in a given GC module.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main R0 module (VMMGC.gc) is assumes.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   pGCPtrValue     Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolGC(PVM pVM, const char *pszModule, const char *pszSymbol, PRTGCPTR pGCPtrValue)
{
#ifdef PDMLDR_FAKE_MODE
    *pGCPtrValue = 0xfeedf00d;
    return VINF_SUCCESS;

#else
    /*
     * Validate input.
     */
    AssertMsg(pVM->pdm.s.offVM, ("bad init order!\n"));
    if (!pszModule)
        pszModule = "VMMGC.gc";

    /*
     * Find the module.
     */
    for (PPDMMOD pModule = pVM->pdm.s.pModules; pModule; pModule = pModule->pNext)
    {
        if (    pModule->eType == PDMMOD_TYPE_GC
            &&  !strcmp(pModule->szName, pszModule))
        {
            RTUINTPTR Value;
            int rc = RTLdrGetSymbolEx(pModule->hLdrMod, pModule->pvBits, pModule->ImageBase, pszSymbol, &Value);
            if (VBOX_SUCCESS(rc))
            {
                *pGCPtrValue = (RTGCPTR)Value;
                Assert(*pGCPtrValue == Value);
            }
            else
            {
                if (pszSymbol < (const char*)(void*)0x10000)
                    AssertMsg(rc, ("Couldn't symbol '%u' in module '%s'\n", (unsigned)(uintptr_t)pszSymbol, pszModule));
                else
                    AssertMsg(rc, ("Couldn't symbol '%s' in module '%s'\n", pszSymbol, pszModule));
            }
            return rc;
        }
    }

    AssertMsgFailed(("Couldn't locate module '%s'\n", pszModule));
    return VERR_SYMBOL_NOT_FOUND;
#endif
}


/**
 * Same as PDMR3GetSymbolGC except that the module will be attempted loaded if not found.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pszModule       Module name. If NULL the main R0 module (VMMGC.gc) is assumes.
 * @param   pszSymbol       Symbol name. If it's value is less than 64k it's treated like a
 *                          ordinal value rather than a string pointer.
 * @param   pGCPtrValue     Where to store the symbol value.
 */
PDMR3DECL(int) PDMR3GetSymbolGCLazy(PVM pVM, const char *pszModule, const char *pszSymbol, PRTGCPTR pGCPtrValue)
{
#ifdef PDMLDR_FAKE_MODE
    *pGCPtrValue = 0xfeedf00d;
    return VINF_SUCCESS;

#else
    /*
     * Since we're lazy, we'll only check if the module is present
     * and hand it over to PDMR3GetSymbolGC when that's done.
     */
    AssertMsg(pVM->pdm.s.offVM, ("bad init order!\n"));
    if (pszModule)
    {
        AssertMsgReturn(!strpbrk(pszModule, "/\\:\n\r\t"), ("pszModule=%s\n", pszModule), VERR_INVALID_PARAMETER);
        PPDMMOD pModule;
        for (pModule = pVM->pdm.s.pModules; pModule; pModule = pModule->pNext)
            if (    pModule->eType == PDMMOD_TYPE_GC
                &&  !strcmp(pModule->szName, pszModule))
                break;
        if (!pModule)
        {
            char *pszFilename = pdmR3FileGC(pszModule);
            AssertMsgReturn(pszFilename, ("pszModule=%s\n", pszModule), VERR_MODULE_NOT_FOUND);
            int rc = PDMR3LoadGC(pVM, pszFilename, pszModule);
            RTMemTmpFree(pszFilename);
            AssertMsgRCReturn(rc, ("pszModule=%s rc=%Vrc\n", pszModule, rc), VERR_MODULE_NOT_FOUND);
        }
    }
    return PDMR3GetSymbolGC(pVM, pszModule, pszSymbol, pGCPtrValue);
#endif
}


/**
 * Constructs the full filename for a R3 image file.
 *
 * @returns Pointer to temporary memory containing the filename.
 *          Caller must free this using RTMemTmpFree().
 * @returns NULL on failure.
 * @param   pszFile     File name (no path).
 * @todo    We'll have this elsewhere than in the root later!
 */
char *pdmR3FileR3(const char *pszFile, bool fShared)
{
    return pdmR3File(pszFile, NULL, fShared);
}


/**
 * Constructs the full filename for a R0 image file.
 *
 * @returns Pointer to temporary memory containing the filename.
 *          Caller must free this using RTMemTmpFree().
 * @returns NULL on failure.
 * @param   pszFile     File name (no path).
 * @todo    We'll have this elsewhere than in the root later!
 */
char * pdmR3FileR0(const char *pszFile)
{
    return pdmR3File(pszFile, NULL, /*fShared=*/false);
}


/**
 * Constructs the full filename for a GC image file.
 *
 * @returns Pointer to temporary memory containing the filename.
 *          Caller must free this using RTMemTmpFree().
 * @returns NULL on failure.
 * @param   pszFile     File name (no path).
 * @todo    We'll have this elsewhere than in the root later!
 */
char * pdmR3FileGC(const char *pszFile)
{
    return pdmR3File(pszFile, NULL, /*fShared=*/false);
}


/**
 * Worker for pdmR3File().
 *
 * @returns Pointer to temporary memory containing the filename.
 *          Caller must free this using RTMemTmpFree().
 * @returns NULL on failure.
 * @param   pszDir        Directory part
 * @param   pszFile       File name part
 * @param   pszDefaultExt Extension part
 */
static char * pdmR3FileConstruct(const char *pszDir, const char *pszFile, const char *pszDefaultExt)
{
    /*
     * Allocate temp memory for return buffer.
     */
    unsigned cchDir  = strlen(pszDir);
    unsigned cchFile = strlen(pszFile);
    unsigned cchDefaultExt;

    /*
     * Default extention?
     */
    if (!pszDefaultExt || strchr(pszFile, '.'))
        cchDefaultExt = 0;
    else
        cchDefaultExt = strlen(pszDefaultExt);

    unsigned cchPath = cchDir + 1 + cchFile + cchDefaultExt + 1;
    if (cchPath > RTPATH_MAX)
    {
        AssertMsgFailed(("Path too long!\n"));
        return NULL;
    }

    char *pszRet = (char *)RTMemTmpAlloc(cchDir + 1 + cchFile + cchDefaultExt + 1);
    if (!pszRet)
    {
        AssertMsgFailed(("Out of temporary memory!\n"));
        return NULL;
    }

    /*
     * Construct the filename.
     */
    memcpy(pszRet, pszDir, cchDir);
    pszRet[cchDir++] = '/';            /* this works everywhere */
    memcpy(pszRet + cchDir, pszFile, cchFile + 1);
    if (cchDefaultExt)
        memcpy(pszRet + cchDir + cchFile, pszDefaultExt, cchDefaultExt + 1);

    return pszRet;
}


/**
 * Worker for pdmR3FileGC(), pdmR3FileR0() and pdmR3FileR3().
 *
 * @returns Pointer to temporary memory containing the filename.
 *          Caller must free this using RTMemTmpFree().
 * @returns NULL on failure.
 * @param   pszFile         File name (no path).
 * @param   pszDefaultExt   The default extention, NULL if none.
 * @param   fShared         If true, search in the shared directory (/usr/lib on Unix), else
 *                          search in the private directory (/usr/lib/virtualbox on Unix).
 *                          Ignored if VBOX_PATH_SHARED_LIBS is not defined.
 * @todo    We'll have this elsewhere than in the root later!
 * @todo    Remove the fShared hack again once we don't need to link against VBoxDD anymore!
 */
static char * pdmR3File(const char *pszFile, const char *pszDefaultExt, bool fShared)
{
    char szPath[RTPATH_MAX];
    int  rc;

    rc = fShared ? RTPathSharedLibs(szPath, sizeof(szPath))
                 : RTPathAppPrivateArch(szPath, sizeof(szPath));
    if (!VBOX_SUCCESS(rc))
    {
        AssertMsgFailed(("RTPathProgram(,%d) failed rc=%d!\n", sizeof(szPath), rc));
        return NULL;
    }

    return pdmR3FileConstruct(szPath, pszFile, pszDefaultExt);
}


/** @internal */
typedef struct QMFEIPARG
{
    uint32_t    uEIP;

    char       *pszNearSym1;
    unsigned    cchNearSym1;
    RTINTPTR  offNearSym1;

    char       *pszNearSym2;
    unsigned    cchNearSym2;
    RTINTPTR  offNearSym2;
} QMFEIPARG, *PQMFEIPARG;

/**
 * Queries module information from an EIP.
 *
 * This is typically used to locate a crash address.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle
 * @param   uEIP        EIP to locate.
 * @param   pszModName  Where to store the module name.
 * @param   cchModName  Size of the module name buffer.
 * @param   pMod        Base address of the module.
 * @param   pszNearSym1 Name of the closes symbol from below.
 * @param   cchNearSym1 Size of the buffer pointed to by pszNearSym1.
 * @param   pNearSym1   The address of pszNearSym1.
 * @param   pszNearSym2 Name of the closes symbol from below.
 * @param   cchNearSym2 Size of the buffer pointed to by pszNearSym2.
 * @param   pNearSym2   The address of pszNearSym2.
 */
PDMR3DECL(int) PDMR3QueryModFromEIP(PVM pVM, uint32_t uEIP,
                                    char *pszModName,  unsigned cchModName,  RTGCPTR *pMod,
                                    char *pszNearSym1, unsigned cchNearSym1, RTGCPTR *pNearSym1,
                                    char *pszNearSym2, unsigned cchNearSym2, RTGCPTR *pNearSym2)
{
    int     rc = VERR_MODULE_NOT_FOUND;
    PPDMMOD pCur;
    for (pCur = pVM->pdm.s.pModules; pCur; pCur = pCur->pNext)
    {
        /* Skip anything which isn't in GC. */
        if (pCur->eType != PDMMOD_TYPE_GC)
            continue;
        if ((RTUINTPTR)uEIP - pCur->ImageBase < RTLdrSize(pCur->hLdrMod))
        {
            if (pMod)
                *pMod = pCur->ImageBase;
            if (pszModName && cchModName)
            {
                *pszModName = '\0';
                strncat(pszModName, pCur->szName, cchModName);
            }
            if (pNearSym1)   *pNearSym1   = 0;
            if (pNearSym2)   *pNearSym2   = 0;
            if (pszNearSym1) *pszNearSym1 = '\0';
            if (pszNearSym2) *pszNearSym2 = '\0';

            /*
             * Locate the nearest symbols.
             */
            QMFEIPARG   Args;
            Args.uEIP        = uEIP;
            Args.pszNearSym1 = pszNearSym1;
            Args.cchNearSym1 = cchNearSym1;
            Args.offNearSym1 = INT_MIN;             /** @todo fix INT_MIN/MAX! */
            Args.pszNearSym2 = pszNearSym2;
            Args.cchNearSym2 = cchNearSym2;
            Args.offNearSym2 = INT_MAX;

            rc = RTLdrEnumSymbols(pCur->hLdrMod, RTLDR_ENUM_SYMBOL_FLAGS_ALL, pCur->pvBits, pCur->ImageBase,
                                  pdmR3QueryModFromEIPEnumSymbols, &Args);
            if (pNearSym1 && Args.offNearSym1 != INT_MIN)
                *pNearSym1 = Args.offNearSym1 + uEIP;
            if (pNearSym2 && Args.offNearSym2 != INT_MAX)
                *pNearSym2 = Args.offNearSym2 + uEIP;

            rc = VINF_SUCCESS;
            if (pCur->eType == PDMMOD_TYPE_GC)
                break;
        }

    }
    return rc;
}


/**
 * Enumeration callback function used by RTLdrEnumSymbols().
 *
 * @returns VBox status code. Failure will stop the enumeration.
 * @param   hLdrMod         The loader module handle.
 * @param   pszSymbol       Symbol name. NULL if ordinal only.
 * @param   uSymbol         Symbol ordinal, ~0 if not used.
 * @param   Value           Symbol value.
 * @param   pvUser          The user argument specified to RTLdrEnumSymbols().
 */
static DECLCALLBACK(int) pdmR3QueryModFromEIPEnumSymbols(RTLDRMOD hLdrMod, const char *pszSymbol, unsigned uSymbol, RTUINTPTR Value, void *pvUser)
{
    PQMFEIPARG pArgs = (PQMFEIPARG)pvUser;

    RTINTPTR off = Value - pArgs->uEIP;
    if (off <= 0)   /* near1 is before or at same location. */
    {
        if (off > pArgs->offNearSym1)
        {
            pArgs->offNearSym1 = off;
            if (pArgs->pszNearSym1 && pArgs->cchNearSym1)
            {
                *pArgs->pszNearSym1 = '\0';
                if (pszSymbol)
                    strncat(pArgs->pszNearSym1, pszSymbol, pArgs->cchNearSym1);
                else
                {
                    char szOrd[32];
                    RTStrPrintf(szOrd, sizeof(szOrd), "#%#x", uSymbol);
                    strncat(pArgs->pszNearSym1, szOrd, pArgs->cchNearSym1);
                }
            }
        }
    }
    else            /* near2 is after */
    {
        if (off < pArgs->offNearSym2)
        {
            pArgs->offNearSym2 = off;
            if (pArgs->pszNearSym2 && pArgs->cchNearSym2)
            {
                *pArgs->pszNearSym2 = '\0';
                if (pszSymbol)
                    strncat(pArgs->pszNearSym2, pszSymbol, pArgs->cchNearSym2);
                else
                {
                    char szOrd[32];
                    RTStrPrintf(szOrd, sizeof(szOrd), "#%#x", uSymbol);
                    strncat(pArgs->pszNearSym2, szOrd, pArgs->cchNearSym2);
                }
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * Enumerate all PDM modules.
 *
 * @returns VBox status.
 * @param   pVM             VM Handle.
 * @param   pfnCallback     Function to call back for each of the modules.
 * @param   pvArg           User argument.
 */
PDMR3DECL(int)  PDMR3EnumModules(PVM pVM, PFNPDMR3ENUM pfnCallback, void *pvArg)
{
    PPDMMOD pCur;
    for (pCur = pVM->pdm.s.pModules; pCur; pCur = pCur->pNext)
    {
        int rc = pfnCallback(pVM,
                             pCur->szFilename,
                             pCur->szName,
                             pCur->ImageBase,
                             pCur->eType == PDMMOD_TYPE_GC ? RTLdrSize(pCur->hLdrMod) : 0,
                             pCur->eType == PDMMOD_TYPE_GC);
        if (VBOX_FAILURE(rc))
            return rc;
    }
    return VINF_SUCCESS;
}

