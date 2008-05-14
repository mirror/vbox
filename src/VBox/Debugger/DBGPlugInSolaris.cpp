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
#include "DBGPlugIns.h"
#include <VBox/dbgf.h>
#include <iprt/string.h>


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
#define SOL32_VALID_ADDRESS(Addr)   ((Addr) > UINT32_C(0x80000000) && (Addr) < UINT32_C(0xfffff000))



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

    return VERR_NOT_IMPLEMENTED;
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
    DBGFR3AddrFromFlat(pVM, &MaxAddr, CurAddr.FlatPtr + 0x01000000);
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

