/* $Id$ */
/** @file
 * DBGPlugInOS2 - Debugger and Guest OS Digger Plugin For OS/2.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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
#include <VBox/vmm/dbgf.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/stream.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/** @name Internal OS/2 structures */

/** @} */


typedef enum DBGDIGGEROS2VER
{
    DBGDIGGEROS2VER_UNKNOWN,
    DBGDIGGEROS2VER_1_x,
    DBGDIGGEROS2VER_2_x,
    DBGDIGGEROS2VER_3_0,
    DBGDIGGEROS2VER_4_0,
    DBGDIGGEROS2VER_4_5
} DBGDIGGEROS2VER;

/**
 * OS/2 guest OS digger instance data.
 */
typedef struct DBGDIGGEROS2
{
    /** Whether the information is valid or not.
     * (For fending off illegal interface method calls.) */
    bool                fValid;
    /** 32-bit (true) or 16-bit (false) */
    bool                f32Bit;

    /** The OS/2 guest version. */
    DBGDIGGEROS2VER     enmVer;
    uint8_t             OS2MajorVersion;
    uint8_t             OS2MinorVersion;

    /** Guest's Global Info Segment selector. */
    uint16_t            selGIS;

} DBGDIGGEROS2;
/** Pointer to the OS/2 guest OS digger instance data. */
typedef DBGDIGGEROS2 *PDBGDIGGEROS2;

/**
 * 32-bit OS/2 loader module table entry.
 */
typedef struct LDRMTE
{
    uint16_t    mte_flags2;
    uint16_t    mte_handle;
    uint32_t    mte_swapmte;    /**< Pointer to LDRSMTE. */
    uint32_t    mte_link;       /**< Pointer to next LDRMTE. */
    uint32_t    mte_flags1;
    uint32_t    mte_impmodcnt;
    uint16_t    mte_sfn;
    uint16_t    mte_usecnt;
    char        mte_modname[8];
    uint32_t    mte_RAS;        /**< added later */
    uint32_t    mte_modver;     /**< added even later. */
} LDRMTE;

/**
 * 32-bit OS/2 swappable module table entry.
 */
typedef struct LDRSMTE
{
    uint32_t    smte_mpages;      /**< module page count. */
    uint32_t    smte_startobj;    /**< Entrypoint segment number. */
    uint32_t    smte_eip;         /**< Entrypoint offset value. */
    uint32_t    smte_stackobj;    /**< Stack segment number. */
    uint32_t    smte_esp;         /**< Stack offset value*/
    uint32_t    smte_pageshift;   /**< Page shift value. */
    uint32_t    smte_fixupsize;   /**< Size of the fixup section. */
    uint32_t    smte_objtab;      /**< Pointer to LDROTE array. */
    uint32_t    smte_objcnt;      /**< Number of segments. */
    uint32_t    smte_objmap;      /**< Address of the object page map. */
    uint32_t    smte_itermap;     /**< File offset of the iterated data map*/
    uint32_t    smte_rsrctab;     /**< Pointer to resource table? */
    uint32_t    smte_rsrccnt;     /**< Number of resource table entries. */
    uint32_t    smte_restab;      /**< Pointer to the resident name table. */
    uint32_t    smte_enttab;      /**< Possibly entry point table address, if not file offset. */
    uint32_t    smte_fpagetab;    /* Offset of Fixup Page Table */
    uint32_t    smte_frectab;     /* Offset of Fixup Record Table */
    uint32_t    smte_impmod;
    uint32_t    smte_impproc;
    uint32_t    smte_datapage;
    uint32_t    smte_nrestab;
    uint32_t    smte_cbnrestab;
    uint32_t    smte_autods;
    uint32_t    smte_debuginfo;   /* Offset of the debugging info */
    uint32_t    smte_debuglen;    /* The len of the debug info in bytes */
    uint32_t    smte_heapsize;
    uint32_t    smte_path;        /**< Address of full name string. */
    uint16_t    smte_semcount;
    uint16_t    smte_semowner;
    uint32_t    smte_pfilecache;  /** Address of cached data if replace-module is used. */
    uint32_t    smte_stacksize;   /**< Stack size for .exe thread 1. */
    uint16_t    smte_alignshift;
    uint16_t    smte_NEexpver;
    uint16_t    smte_pathlen;     /**< Length of smte_path */
    uint16_t    smte_NEexetype;
    uint16_t    smte_csegpack;
    uint8_t     smte_major_os;    /**< added later to lie about OS version */
    uint8_t     smte_minor_os;    /**< added later to lie about OS version */
} LDRSMTE;

typedef struct LDROTE
{
    uint32_t    ote_size;
    uint32_t    ote_base;
    uint32_t    ote_flags;
    uint32_t    ote_pagemap;
    uint32_t    ote_mapsize;
    union
    {
        uint32_t ote_vddaddr;
        uint32_t ote_krnaddr;
        struct
        {
            uint16_t ote_selector;
            uint16_t ote_handle;
        } s;
    };
} LDROTE;
AssertCompileSize(LDROTE, 24);


/**
 * 32-bit system anchor block segment header.
 */
typedef struct SAS
{
    uint8_t     SAS_signature[4];
    uint16_t    SAS_tables_data;    /**< Offset to SASTABLES.  */
    uint16_t    SAS_flat_sel;       /**< 32-bit kernel DS (flat). */
    uint16_t    SAS_config_data;    /**< Offset to SASCONFIG. */
    uint16_t    SAS_dd_data;        /**< Offset to SASDD. */
    uint16_t    SAS_vm_data;        /**< Offset to SASVM. */
    uint16_t    SAS_task_data;      /**< Offset to SASTASK. */
    uint16_t    SAS_RAS_data;       /**< Offset to SASRAS. */
    uint16_t    SAS_file_data;      /**< Offset to SASFILE. */
    uint16_t    SAS_info_data;      /**< Offset to SASINFO. */
    uint16_t    SAS_mp_data;        /**< Offset to SASMP. SMP only. */
} SAS;
#define SAS_SIGNATURE "SAS "

typedef struct SASTABLES
{
    uint16_t    SAS_tbl_GDT;
    uint16_t    SAS_tbl_LDT;
    uint16_t    SAS_tbl_IDT;
    uint16_t    SAS_tbl_GDTPOOL;
} SASTABLES;

typedef struct SASCONFIG
{
    uint16_t    SAS_config_table;
} SASCONFIG;

typedef struct SASDD
{
    uint16_t    SAS_dd_bimodal_chain;
    uint16_t    SAS_dd_real_chain;
    uint16_t    SAS_dd_DPB_segment;
    uint16_t    SAS_dd_CDA_anchor_p;
    uint16_t    SAS_dd_CDA_anchor_r;
    uint16_t    SAS_dd_FSC;
} SASDD;

typedef struct SASVM
{
    uint32_t    SAS_vm_arena;
    uint32_t    SAS_vm_object;
    uint32_t    SAS_vm_context;
    uint32_t    SAS_vm_krnl_mte;    /**< Flat address of kernel MTE. */
    uint32_t    SAS_vm_glbl_mte;    /**< Flat address of global MTE list head pointer variable. */
    uint32_t    SAS_vm_pft;
    uint32_t    SAS_vm_prt;
    uint32_t    SAS_vm_swap;
    uint32_t    SAS_vm_idle_head;
    uint32_t    SAS_vm_free_head;
    uint32_t    SAS_vm_heap_info;
    uint32_t    SAS_vm_all_mte;     /**< Flat address of global MTE list head pointer variable. */
} SASVM;


#pragma pack(1)
typedef struct SASTASK
{
    uint16_t    SAS_task_PTDA;        /**< Current PTDA selector. */
    uint32_t    SAS_task_ptdaptrs;    /**< Flat address of process tree root. */
    uint32_t    SAS_task_threadptrs;  /**< Flat address array of thread pointer array. */
    uint32_t    SAS_task_tasknumber;  /**< Flat address of the TaskNumber variable. */
    uint32_t    SAS_task_threadcount; /**< Flat address of the ThreadCount variable. */
} SASTASK;
#pragma pack()


#pragma pack(1)
typedef struct SASRAS
{
    uint16_t    SAS_RAS_STDA_p;
    uint16_t    SAS_RAS_STDA_r;
    uint16_t    SAS_RAS_event_mask;
    uint32_t    SAS_RAS_Perf_Buff;
} SASRAS;
#pragma pack()

typedef struct SASFILE
{
    uint32_t    SAS_file_MFT;       /**< Handle. */
    uint16_t    SAS_file_SFT;       /**< Selector. */
    uint16_t    SAS_file_VPB;       /**< Selector. */
    uint16_t    SAS_file_CDS;       /**< Selector. */
    uint16_t    SAS_file_buffers;   /**< Selector. */
} SASFILE;

#pragma pack(1)
typedef struct SASINFO
{
    uint16_t    SAS_info_global;    /**< GIS selector. */
    uint32_t    SAS_info_local;     /**< Flat address of LIS for current task. */
    uint32_t    SAS_info_localRM;
    uint16_t    SAS_info_CDIB;      /**< Selector. */
} SASINFO;
#pragma pack()

typedef struct SASMP
{
    uint32_t    SAS_mp_PCBFirst;        /**< Flat address of PCB head. */
    uint32_t    SAS_mp_pLockHandles;    /**< Flat address of lock handles. */
    uint32_t    SAS_mp_cProcessors;     /**< Flat address of CPU count variable. */
    uint32_t    SAS_mp_pIPCInfo;        /**< Flat address of IPC info pointer variable. */
    uint32_t    SAS_mp_pIPCHistory;     /**< Flat address of IPC history pointer. */
    uint32_t    SAS_mp_IPCHistoryIdx;   /**< Flat address of IPC history index variable. */
    uint32_t    SAS_mp_pFirstPSA;       /**< Flat address of PSA. Added later. */
    uint32_t    SAS_mp_pPSAPages;       /**< Flat address of PSA pages. */
} SASMP;



/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The 'SAS ' signature. */
#define DIG_OS2_SAS_SIG     RT_MAKE_U32_FROM_U8('S','A','S',' ')

/** OS/2Warp on little endian ASCII systems. */
#define DIG_OS2_MOD_TAG     UINT64_C(0x43532f3257617270)


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int)  dbgDiggerOS2Init(PUVM pUVM, void *pvData);



#if 0 /* unused */
/**
 * Process a PE image found in guest memory.
 *
 * @param   pThis           The instance data.
 * @param   pUVM            The user mode VM handle.
 * @param   pszName         The image name.
 * @param   pImageAddr      The image address.
 * @param   cbImage         The size of the image.
 * @param   pbBuf           Scratch buffer containing the first
 *                          RT_MIN(cbBuf, cbImage) bytes of the image.
 * @param   cbBuf           The scratch buffer size.
 */
static void dbgDiggerOS2ProcessImage(PDBGDIGGEROS2 pThis, PUVM pUVM, const char *pszName,
                                     PCDBGFADDRESS pImageAddr, uint32_t cbImage,
                                     uint8_t *pbBuf, size_t cbBuf)
{
    RT_NOREF7(pThis, pUVM, pszName, pImageAddr, cbImage, pbBuf, cbBuf);
    LogFlow(("DigOS2: %RGp %#x %s\n", pImageAddr->FlatPtr, cbImage, pszName));

    /* To be implemented.*/
}
#endif


/**
 * @copydoc DBGFOSREG::pfnStackUnwindAssist
 */
static DECLCALLBACK(int) dbgDiggerOS2StackUnwindAssist(PUVM pUVM, void *pvData, VMCPUID idCpu, PDBGFSTACKFRAME pFrame,
                                                       PRTDBGUNWINDSTATE pState, PCCPUMCTX pInitialCtx, RTDBGAS hAs,
                                                       uint64_t *puScratch)
{
    RT_NOREF(pUVM, pvData, idCpu, pFrame, pState, pInitialCtx, hAs, puScratch);
    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnQueryInterface
 */
static DECLCALLBACK(void *) dbgDiggerOS2QueryInterface(PUVM pUVM, void *pvData, DBGFOSINTERFACE enmIf)
{
    RT_NOREF3(pUVM, pvData, enmIf);
    return NULL;
}


/**
 * @copydoc DBGFOSREG::pfnQueryVersion
 */
static DECLCALLBACK(int)  dbgDiggerOS2QueryVersion(PUVM pUVM, void *pvData, char *pszVersion, size_t cchVersion)
{
    RT_NOREF1(pUVM);
    PDBGDIGGEROS2 pThis = (PDBGDIGGEROS2)pvData;
    Assert(pThis->fValid);
    char *achOS2ProductType[32];
    char *pszOS2ProductType = (char *)achOS2ProductType;

    if (pThis->OS2MajorVersion == 10)
    {
        RTStrPrintf(pszOS2ProductType, sizeof(achOS2ProductType), "OS/2 1.%02d", pThis->OS2MinorVersion);
        pThis->enmVer = DBGDIGGEROS2VER_1_x;
    }
    else if (pThis->OS2MajorVersion == 20)
    {
        if (pThis->OS2MinorVersion < 30)
        {
            RTStrPrintf(pszOS2ProductType, sizeof(achOS2ProductType), "OS/2 2.%02d", pThis->OS2MinorVersion);
            pThis->enmVer = DBGDIGGEROS2VER_2_x;
        }
        else if (pThis->OS2MinorVersion < 40)
        {
            RTStrPrintf(pszOS2ProductType, sizeof(achOS2ProductType), "OS/2 Warp");
            pThis->enmVer = DBGDIGGEROS2VER_3_0;
        }
        else if (pThis->OS2MinorVersion == 40)
        {
            RTStrPrintf(pszOS2ProductType, sizeof(achOS2ProductType), "OS/2 Warp 4");
            pThis->enmVer = DBGDIGGEROS2VER_4_0;
        }
        else
        {
            RTStrPrintf(pszOS2ProductType, sizeof(achOS2ProductType), "OS/2 Warp %d.%d",
                        pThis->OS2MinorVersion / 10, pThis->OS2MinorVersion % 10);
            pThis->enmVer = DBGDIGGEROS2VER_4_5;
        }
    }
    RTStrPrintf(pszVersion, cchVersion, "%u.%u (%s)", pThis->OS2MajorVersion, pThis->OS2MinorVersion, pszOS2ProductType);
    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnTerm
 */
static DECLCALLBACK(void)  dbgDiggerOS2Term(PUVM pUVM, void *pvData)
{
    RT_NOREF1(pUVM);
    PDBGDIGGEROS2 pThis = (PDBGDIGGEROS2)pvData;
    Assert(pThis->fValid);

    pThis->fValid = false;
}


/**
 * @copydoc DBGFOSREG::pfnRefresh
 */
static DECLCALLBACK(int)  dbgDiggerOS2Refresh(PUVM pUVM, void *pvData)
{
    PDBGDIGGEROS2 pThis = (PDBGDIGGEROS2)pvData;
    NOREF(pThis);
    Assert(pThis->fValid);

    /*
     * For now we'll flush and reload everything.
     */
    RTDBGAS hDbgAs = DBGFR3AsResolveAndRetain(pUVM, DBGF_AS_KERNEL);
    if (hDbgAs != NIL_RTDBGAS)
    {
        uint32_t iMod = RTDbgAsModuleCount(hDbgAs);
        while (iMod-- > 0)
        {
            RTDBGMOD hMod = RTDbgAsModuleByIndex(hDbgAs, iMod);
            if (hMod != NIL_RTDBGMOD)
            {
                if (RTDbgModGetTag(hMod) == DIG_OS2_MOD_TAG)
                {
                    int rc = RTDbgAsModuleUnlink(hDbgAs, hMod);
                    AssertRC(rc);
                }
                RTDbgModRelease(hMod);
            }
        }
        RTDbgAsRelease(hDbgAs);
    }

    dbgDiggerOS2Term(pUVM, pvData);
    return dbgDiggerOS2Init(pUVM, pvData);
}

/** Buffer shared by dbgdiggerOS2ProcessModule and dbgDiggerOS2Init.*/
typedef union DBGDIGGEROS2BUF
{
    uint8_t             au8[0x2000];
    uint16_t            au16[0x2000/2];
    uint32_t            au32[0x2000/4];
    RTUTF16             wsz[0x2000/2];
    char                ach[0x2000];
    LDROTE              aOtes[0x2000 / sizeof(LDROTE)];
    SAS                 sas;
    SASVM               sasvm;
    LDRMTE              mte;
    LDRSMTE             smte;
    LDROTE              ote;
} DBGDIGGEROS2BUF;


static void dbgdiggerOS2ProcessModule(PUVM pUVM, PDBGDIGGEROS2 pThis, DBGDIGGEROS2BUF *pBuf)
{
    RT_NOREF(pThis);

    /*
     * Save the MTE.
     */
    LDRMTE const Mte = pBuf->mte;

    /*
     * Try read the swappable MTE.  Save it too.
     */
    DBGFADDRESS     Addr;
    int rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, Mte.mte_swapmte), &pBuf->smte, sizeof(pBuf->smte));
    if (RT_FAILURE(rc))
    {
        LogRel(("DbgDiggerOs2: Error reading swap mte @ %RX32: %Rrc\n", Mte.mte_swapmte, rc));
        return;
    }
    LDRSMTE const   SwapMte = pBuf->smte;

    /* Ignore empty modules or modules with too many segments. */
    if (SwapMte.smte_objcnt == 0 || SwapMte.smte_objcnt > RT_ELEMENTS(pBuf->aOtes))
    {
        LogRel(("DbgDiggerOs2: Skipping: smte_objcnt= %#RX32\n", SwapMte.smte_objcnt));
        return;
    }

#if 0
    /*
     * Try read the path name, falling back on module name.
     */
    rc = VERR_NOT_AVAILABLE;
    if (SwapMte.smte_path != 0 && SwapMte.smte_pathlen > 0)
    {
        uint32_t cbToRead = RT_MIN(SwapMte.smte_path, sizeof(*pBuf) - 1);
        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, SwapMte.smte_path), pBuf->ach, cbToRead);
        pBuf->ach[cbToRead] = '\0';
    }
    if (RT_FAILURE(rc))
    {
        memcpy(pBuf->ach, Mte.mte_modname, sizeof(Mte.mte_modname));
        pBuf->ach[sizeof(Mte.mte_modname)] = '\0';
        RTStrStripR(pBuf->ach);
    }
#endif

    /*
     * Create a simple module.
     */
    memcpy(pBuf->ach, Mte.mte_modname, sizeof(Mte.mte_modname));
    pBuf->ach[sizeof(Mte.mte_modname)] = '\0';
    RTStrStripR(pBuf->ach);

    RTDBGMOD hDbgMod;
    rc = RTDbgModCreate(&hDbgMod, pBuf->ach, 0 /*cbSeg*/, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
    {
        LogRel(("DbgDiggerOs2: RTDbgModCreate failed: %Rrc\n", rc));
        return;
    }
    rc = RTDbgModSetTag(hDbgMod, DIG_OS2_MOD_TAG);
    if (RT_SUCCESS(rc))
    {
        RTDBGAS hAs = DBGFR3AsResolveAndRetain(pUVM, DBGF_AS_KERNEL);
        if (hAs != NIL_RTDBGAS)
        {
            /*
             * Read the object table and do the linking of each of them.
             */
            rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, SwapMte.smte_objtab),
                               &pBuf->aOtes[0], sizeof(pBuf->aOtes[0]) * SwapMte.smte_objcnt);
            if (RT_SUCCESS(rc))
            {
                uint32_t uRva = 0;
                for (uint32_t i = 0; i < SwapMte.smte_objcnt; i++)
                {
                    char szSegNm[16];
                    RTStrPrintf(szSegNm, sizeof(szSegNm), "seg%u", i);
                    rc = RTDbgModSegmentAdd(hDbgMod, uRva, pBuf->aOtes[i].ote_size, szSegNm, 0 /*fFlags*/, NULL);
                    if (RT_FAILURE(rc))
                    {
                        LogRel(("DbgDiggerOs2: RTDbgModSegmentAdd failed (i=%u, ote_size=%#x): %Rrc\n",
                                i, pBuf->aOtes[i].ote_size, rc));
                        break;
                    }
                    rc = RTDbgAsModuleLinkSeg(hAs, hDbgMod, i, pBuf->aOtes[i].ote_base, RTDBGASLINK_FLAGS_REPLACE /*fFlags*/);
                    if (RT_FAILURE(rc))
                        LogRel(("DbgDiggerOs2: RTDbgAsModuleLinkSeg failed (i=%u, ote_base=%#x): %Rrc\n",
                                i, pBuf->aOtes[i].ote_base, rc));
                    uRva += RT_ALIGN_32(pBuf->aOtes[i].ote_size, _4K);
                }
            }
            else
                LogRel(("DbgDiggerOs2: Error reading object table @ %#RX32 LB %#zx: %Rrc\n",
                        SwapMte.smte_objtab, sizeof(pBuf->aOtes[0]) * SwapMte.smte_objcnt, rc));
        }
        else
            LogRel(("DbgDiggerOs2: DBGFR3AsResolveAndRetain failed\n"));
        RTDbgAsRelease(hAs);
    }
    else
        LogRel(("DbgDiggerOs2: RTDbgModSetTag failed: %Rrc\n", rc));
    RTDbgModRelease(hDbgMod);

}


/**
 * @copydoc DBGFOSREG::pfnInit
 */
static DECLCALLBACK(int)  dbgDiggerOS2Init(PUVM pUVM, void *pvData)
{
    PDBGDIGGEROS2 pThis = (PDBGDIGGEROS2)pvData;
    Assert(!pThis->fValid);

    DBGDIGGEROS2BUF uBuf;
    DBGFADDRESS     Addr;
    int             rc;

    /*
     * Determine the OS/2 version.
     */
    /* Version info is at GIS:15h (major/minor/revision). */
    rc = DBGFR3AddrFromSelOff(pUVM, 0 /*idCpu*/, &Addr, pThis->selGIS, 0x15);
    if (RT_FAILURE(rc))
        return VERR_NOT_SUPPORTED;
    rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, uBuf.au32, sizeof(uint32_t));
    if (RT_FAILURE(rc))
        return VERR_NOT_SUPPORTED;

    pThis->OS2MajorVersion = uBuf.au8[0];
    pThis->OS2MinorVersion = uBuf.au8[1];

    pThis->fValid = true;

    /*
     * Try use SAS to find the module list.
     */
    rc = DBGFR3AddrFromSelOff(pUVM, 0 /*idCpu*/, &Addr, 0x70, 0x00);
    if (RT_SUCCESS(rc))
    {
        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, &uBuf.sas, sizeof(uBuf.sas));
        if (RT_SUCCESS(rc))
        {
            rc = DBGFR3AddrFromSelOff(pUVM, 0 /*idCpu*/, &Addr, 0x70, uBuf.sas.SAS_vm_data);
            if (RT_SUCCESS(rc))
                rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, &uBuf.sasvm, sizeof(uBuf.sasvm));
            if (RT_SUCCESS(rc))
            {
                /*
                 * Work the module list.
                 */
                rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, uBuf.sasvm.SAS_vm_all_mte),
                                   &uBuf.au32[0], sizeof(uBuf.au32[0]));
                if (RT_SUCCESS(rc))
                {
                    DBGFR3AddrFromFlat(pUVM, &Addr, uBuf.au32[0]);
                    while (Addr.FlatPtr != 0 && Addr.FlatPtr != UINT32_MAX)
                    {
                        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, &uBuf.mte, sizeof(uBuf.mte));
                        if (RT_FAILURE(rc))
                            break;
                        LogRel(("DbgDiggerOs2: Module @ %#010RX32: %.8s %#x %#x\n", (uint32_t)Addr.FlatPtr,
                                uBuf.mte.mte_modname, uBuf.mte.mte_flags1, uBuf.mte.mte_flags2));

                        DBGFR3AddrFromFlat(pUVM, &Addr, uBuf.mte.mte_link);
                        dbgdiggerOS2ProcessModule(pUVM, pThis, &uBuf);
                    }
                }
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnProbe
 */
static DECLCALLBACK(bool)  dbgDiggerOS2Probe(PUVM pUVM, void *pvData)
{
    PDBGDIGGEROS2   pThis = (PDBGDIGGEROS2)pvData;
    DBGFADDRESS     Addr;
    int             rc;
    uint16_t        offInfo;
    union
    {
        uint8_t             au8[8192];
        uint16_t            au16[8192/2];
        uint32_t            au32[8192/4];
        RTUTF16             wsz[8192/2];
    } u;

    /*
     * If the DWORD at 70:0 contains 'SAS ' it's quite unlikely that this wouldn't be OS/2.
     * Note: The SAS layout is similar between 16-bit and 32-bit OS/2, but not identical.
     * 32-bit OS/2 will have the flat kernel data selector at SAS:06. The selector is 168h
     * or similar. For 16-bit OS/2 the field contains a table offset into the SAS which will
     * be much smaller. Fun fact: The global infoseg selector in the SAS is bimodal in 16-bit
     * OS/2 and will work in real mode as well.
     */
    do {
        rc = DBGFR3AddrFromSelOff(pUVM, 0 /*idCpu*/, &Addr, 0x70, 0x00);
        if (RT_FAILURE(rc))
            break;
        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, u.au32, 256);
        if (RT_FAILURE(rc))
            break;
        if (u.au32[0] != DIG_OS2_SAS_SIG)
            break;

        /* This sure looks like OS/2, but a bit of paranoia won't hurt. */
        if (u.au16[2] >= u.au16[4])
            break;

        /* If 4th word is bigger than 5th, it's the flat kernel mode selector. */
        if (u.au16[3] > u.au16[4])
            pThis->f32Bit = true;

        /* Offset into info table is either at SAS:14h or SAS:16h. */
        if (pThis->f32Bit)
            offInfo = u.au16[0x14/2];
        else
            offInfo = u.au16[0x16/2];

        /* The global infoseg selector is the first entry in the info table. */
        pThis->selGIS = u.au16[offInfo/2];
        return true;
    } while (0);

    return false;
}


/**
 * @copydoc DBGFOSREG::pfnDestruct
 */
static DECLCALLBACK(void)  dbgDiggerOS2Destruct(PUVM pUVM, void *pvData)
{
    RT_NOREF2(pUVM, pvData);
}


/**
 * @copydoc DBGFOSREG::pfnConstruct
 */
static DECLCALLBACK(int)  dbgDiggerOS2Construct(PUVM pUVM, void *pvData)
{
    RT_NOREF1(pUVM);
    PDBGDIGGEROS2 pThis = (PDBGDIGGEROS2)pvData;
    pThis->fValid = false;
    pThis->f32Bit = false;
    pThis->enmVer = DBGDIGGEROS2VER_UNKNOWN;
    return VINF_SUCCESS;
}


const DBGFOSREG g_DBGDiggerOS2 =
{
    /* .u32Magic = */               DBGFOSREG_MAGIC,
    /* .fFlags = */                 0,
    /* .cbData = */                 sizeof(DBGDIGGEROS2),
    /* .szName = */                 "OS/2",
    /* .pfnConstruct = */           dbgDiggerOS2Construct,
    /* .pfnDestruct = */            dbgDiggerOS2Destruct,
    /* .pfnProbe = */               dbgDiggerOS2Probe,
    /* .pfnInit = */                dbgDiggerOS2Init,
    /* .pfnRefresh = */             dbgDiggerOS2Refresh,
    /* .pfnTerm = */                dbgDiggerOS2Term,
    /* .pfnQueryVersion = */        dbgDiggerOS2QueryVersion,
    /* .pfnQueryInterface = */      dbgDiggerOS2QueryInterface,
    /* .pfnStackUnwindAssist = */   dbgDiggerOS2StackUnwindAssist,
    /* .u32EndMagic = */            DBGFOSREG_MAGIC
};
