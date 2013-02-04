/* $Id$ */
/** @file
 * DBGPlugInLinux - Debugger and Guest OS Digger Plugin For Linux.
 */

/*
 * Copyright (C) 2008-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF ///@todo add new log group.
#include "DBGPlugIns.h"
#include "DBGPlugInCommonELF.h"
#include <VBox/vmm/dbgf.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/ctype.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** @name InternalLinux structures
 * @{ */


/** @} */


/**
 * Linux guest OS digger instance data.
 */
typedef struct DBGDIGGERLINUX
{
    /** Whether the information is valid or not.
     * (For fending off illegal interface method calls.) */
    bool fValid;

    /** The address of the linux banner.
     * This is set during probing. */
    DBGFADDRESS AddrLinuxBanner;
    /** Kernel base address.
     * This is set during probing. */
    DBGFADDRESS AddrKernelBase;
} DBGDIGGERLINUX;
/** Pointer to the linux guest OS digger instance data. */
typedef DBGDIGGERLINUX *PDBGDIGGERLINUX;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a 32-bit linux kernel address */
#define LNX32_VALID_ADDRESS(Addr)       ((Addr) > UINT32_C(0x80000000) && (Addr) < UINT32_C(0xfffff000))

/** The max kernel size. */
#define LNX_MAX_KERNEL_SIZE         0x0f000000


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int)  dbgDiggerLinuxInit(PUVM pUVM, void *pvData);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Table of common linux kernel addresses. */
static uint64_t g_au64LnxKernelAddresses[] =
{
    UINT64_C(0xc0100000),
    UINT64_C(0x90100000),
    UINT64_C(0xffffffff80200000)
};


/**
 * @copydoc DBGFOSREG::pfnQueryInterface
 */
static DECLCALLBACK(void *) dbgDiggerLinuxQueryInterface(PUVM pUVM, void *pvData, DBGFOSINTERFACE enmIf)
{
    return NULL;
}


/**
 * @copydoc DBGFOSREG::pfnQueryVersion
 */
static DECLCALLBACK(int)  dbgDiggerLinuxQueryVersion(PUVM pUVM, void *pvData, char *pszVersion, size_t cchVersion)
{
    PDBGDIGGERLINUX pThis = (PDBGDIGGERLINUX)pvData;
    Assert(pThis->fValid);

    /*
     * It's all in the linux banner.
     */
    int rc = DBGFR3MemReadString(pUVM, 0, &pThis->AddrLinuxBanner, pszVersion, cchVersion);
    if (RT_SUCCESS(rc))
    {
        char *pszEnd = RTStrEnd(pszVersion, cchVersion);
        AssertReturn(pszEnd, VERR_BUFFER_OVERFLOW);
        while (     pszEnd > pszVersion
               &&   RT_C_IS_SPACE(pszEnd[-1]))
            pszEnd--;
        *pszEnd = '\0';
    }
    else
        RTStrPrintf(pszVersion, cchVersion, "DBGFR3MemRead -> %Rrc", rc);

    return rc;
}


/**
 * @copydoc DBGFOSREG::pfnTerm
 */
static DECLCALLBACK(void)  dbgDiggerLinuxTerm(PUVM pUVM, void *pvData)
{
    PDBGDIGGERLINUX pThis = (PDBGDIGGERLINUX)pvData;
    Assert(pThis->fValid);

    pThis->fValid = false;
}


/**
 * @copydoc DBGFOSREG::pfnRefresh
 */
static DECLCALLBACK(int)  dbgDiggerLinuxRefresh(PUVM pUVM, void *pvData)
{
    PDBGDIGGERLINUX pThis = (PDBGDIGGERLINUX)pvData;
    NOREF(pThis);
    Assert(pThis->fValid);

    /*
     * For now we'll flush and reload everything.
     */
    dbgDiggerLinuxTerm(pUVM, pvData);
    return dbgDiggerLinuxInit(pUVM, pvData);
}


/**
 * @copydoc DBGFOSREG::pfnInit
 */
static DECLCALLBACK(int)  dbgDiggerLinuxInit(PUVM pUVM, void *pvData)
{
    PDBGDIGGERLINUX pThis = (PDBGDIGGERLINUX)pvData;
    Assert(!pThis->fValid);
#if 0  /* later */
    int rc;

    /*
     * Algorithm to find the ksymtab:
     *  1. Find a known export string in kstrtab ("init_task", "enable_hlt" or something).
     *  2. Search for the address its at, this should give you the corresponding ksymtab entry.
     *  3. Search backwards assuming that kstrtab is corresponding to ksymtab.
     */
    DBGFADDRESS AddrKernelKsymTab;


#endif
    pThis->fValid = true;
    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnProbe
 */
static DECLCALLBACK(bool)  dbgDiggerLinuxProbe(PUVM pUVM, void *pvData)
{
    PDBGDIGGERLINUX pThis = (PDBGDIGGERLINUX)pvData;

    /*
     * Look for "Linux version " at the start of the rodata segment.
     * Hope that this comes before any message buffer or other similar string.
     *                                                                       .
     * Note! Only Linux version 2.x.y, where x in {0..6}.                                                                      .
     */
    for (unsigned i = 0; i < RT_ELEMENTS(g_au64LnxKernelAddresses); i++)
    {
        DBGFADDRESS KernelAddr;
        DBGFR3AddrFromFlat(pUVM, &KernelAddr, g_au64LnxKernelAddresses[i]);
        DBGFADDRESS HitAddr;
        static const uint8_t s_abLinuxVersion[] = "Linux version 2.";
        int rc = DBGFR3MemScan(pUVM, 0, &KernelAddr, LNX_MAX_KERNEL_SIZE, 1,
                               s_abLinuxVersion, sizeof(s_abLinuxVersion) - 1, &HitAddr);
        if (RT_SUCCESS(rc))
        {
            char szTmp[128];
            char const *pszY = &szTmp[sizeof(s_abLinuxVersion) - 1];
            rc = DBGFR3MemReadString(pUVM, 0, &HitAddr, szTmp, sizeof(szTmp));
            if (    RT_SUCCESS(rc)
                &&  *pszY >= '0'
                &&  *pszY <= '6')
            {
                pThis->AddrKernelBase  = KernelAddr;
                pThis->AddrLinuxBanner = HitAddr;
                return true;
            }
        }
    }
    return false;
}


/**
 * @copydoc DBGFOSREG::pfnDestruct
 */
static DECLCALLBACK(void)  dbgDiggerLinuxDestruct(PUVM pUVM, void *pvData)
{

}


/**
 * @copydoc DBGFOSREG::pfnConstruct
 */
static DECLCALLBACK(int)  dbgDiggerLinuxConstruct(PUVM pUVM, void *pvData)
{
    return VINF_SUCCESS;
}


const DBGFOSREG g_DBGDiggerLinux =
{
    /* .u32Magic = */           DBGFOSREG_MAGIC,
    /* .fFlags = */             0,
    /* .cbData = */             sizeof(DBGDIGGERLINUX),
    /* .szName = */             "Linux",
    /* .pfnConstruct = */       dbgDiggerLinuxConstruct,
    /* .pfnDestruct = */        dbgDiggerLinuxDestruct,
    /* .pfnProbe = */           dbgDiggerLinuxProbe,
    /* .pfnInit = */            dbgDiggerLinuxInit,
    /* .pfnRefresh = */         dbgDiggerLinuxRefresh,
    /* .pfnTerm = */            dbgDiggerLinuxTerm,
    /* .pfnQueryVersion = */    dbgDiggerLinuxQueryVersion,
    /* .pfnQueryInterface = */  dbgDiggerLinuxQueryInterface,
    /* .u32EndMagic = */        DBGFOSREG_MAGIC
};

