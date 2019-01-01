/* $Id$ */
/** @file
 * DBGPlugInOS2 - Debugger and Guest OS Digger Plugin For OS/2.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
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
/** @name LDRMTE::mte_flag2 values
 * @{ */
#define MTEFORMATMASK       UINT16_C(0x0003)
#define MTEFORMATR1         UINT16_C(0x0000)
#define MTEFORMATNE         UINT16_C(0x0001)
#define MTEFORMATLX         UINT16_C(0x0002)
#define MTEFORMATR2         UINT16_C(0x0003)
#define MTESYSTEMDLL        UINT16_C(0x0004)
#define MTELOADORATTACH     UINT16_C(0x0008)
#define MTECIRCLEREF        UINT16_C(0x0010)
#define MTEFREEFIXUPS       UINT16_C(0x0020) /* had different meaning earlier */
#define MTEPRELOADED        UINT16_C(0x0040)
#define MTEGETMTEDONE       UINT16_C(0x0080)
#define MTEPACKSEGDONE      UINT16_C(0x0100)
#define MTE20LIELIST        UINT16_C(0x0200)
#define MTESYSPROCESSED     UINT16_C(0x0400)
#define MTEPSDMOD           UINT16_C(0x0800)
#define MTEDLLONEXTLST      UINT16_C(0x1000)
#define MTEPDUMPCIRCREF     UINT16_C(0x2000)
/** @} */
/** @name LDRMTE::mte_flag1 values
 * @{ */
#define MTE1_NOAUTODS           UINT32_C(0x00000000)
#define MTE1_SOLO               UINT32_C(0x00000001)
#define MTE1_INSTANCEDS         UINT32_C(0x00000002)
#define MTE1_INSTLIBINIT        UINT32_C(0x00000004)
#define MTE1_GINISETUP          UINT32_C(0x00000008)
#define MTE1_NOINTERNFIXUPS     UINT32_C(0x00000010)
#define MTE1_NOEXTERNFIXUPS     UINT32_C(0x00000020)
#define MTE1_CLASS_ALL          UINT32_C(0x00000000)
#define MTE1_CLASS_PROGRAM      UINT32_C(0x00000040)
#define MTE1_CLASS_GLOBAL       UINT32_C(0x00000080)
#define MTE1_CLASS_SPECIFIC     UINT32_C(0x000000c0)
#define MTE1_CLASS_MASK         UINT32_C(0x000000c0)
#define MTE1_MTEPROCESSED       UINT32_C(0x00000100)
#define MTE1_USED               UINT32_C(0x00000200)
#define MTE1_DOSLIB             UINT32_C(0x00000400)
#define MTE1_DOSMOD             UINT32_C(0x00000800) /**< The OS/2 kernel (DOSCALLS).*/
#define MTE1_MEDIAFIXED         UINT32_C(0x00001000)
#define MTE1_LDRINVALID         UINT32_C(0x00002000)
#define MTE1_PROGRAMMOD         UINT32_C(0x00000000)
#define MTE1_DEVDRVMOD          UINT32_C(0x00004000)
#define MTE1_LIBRARYMOD         UINT32_C(0x00008000)
#define MTE1_VDDMOD             UINT32_C(0x00010000)
#define MTE1_MVDMMOD            UINT32_C(0x00020000)
#define MTE1_INGRAPH            UINT32_C(0x00040000)
#define MTE1_GINIDONE           UINT32_C(0x00080000)
#define MTE1_ADDRALLOCED        UINT32_C(0x00100000)
#define MTE1_FSDMOD             UINT32_C(0x00200000)
#define MTE1_FSHMOD             UINT32_C(0x00400000)
#define MTE1_LONGNAMES          UINT32_C(0x00800000)
#define MTE1_MEDIACONTIG        UINT32_C(0x01000000)
#define MTE1_MEDIA16M           UINT32_C(0x02000000)
#define MTE1_SWAPONLOAD         UINT32_C(0x04000000)
#define MTE1_PORTHOLE           UINT32_C(0x08000000)
#define MTE1_MODPROT            UINT32_C(0x10000000)
#define MTE1_NEWMOD             UINT32_C(0x20000000)
#define MTE1_DLLTERM            UINT32_C(0x40000000)
#define MTE1_SYMLOADED          UINT32_C(0x80000000)
/** @} */


/**
 * 32-bit OS/2 swappable module table entry.
 */
typedef struct LDRSMTE
{
    uint32_t    smte_mpages;      /**< 0x00: module page count. */
    uint32_t    smte_startobj;    /**< 0x04: Entrypoint segment number. */
    uint32_t    smte_eip;         /**< 0x08: Entrypoint offset value. */
    uint32_t    smte_stackobj;    /**< 0x0c: Stack segment number. */
    uint32_t    smte_esp;         /**< 0x10: Stack offset value*/
    uint32_t    smte_pageshift;   /**< 0x14: Page shift value. */
    uint32_t    smte_fixupsize;   /**< 0x18: Size of the fixup section. */
    uint32_t    smte_objtab;      /**< 0x1c: Pointer to LDROTE array. */
    uint32_t    smte_objcnt;      /**< 0x20: Number of segments. */
    uint32_t    smte_objmap;      /**< 0x20: Address of the object page map. */
    uint32_t    smte_itermap;     /**< 0x20: File offset of the iterated data map*/
    uint32_t    smte_rsrctab;     /**< 0x20: Pointer to resource table? */
    uint32_t    smte_rsrccnt;     /**< 0x30: Number of resource table entries. */
    uint32_t    smte_restab;      /**< 0x30: Pointer to the resident name table. */
    uint32_t    smte_enttab;      /**< 0x30: Possibly entry point table address, if not file offset. */
    uint32_t    smte_fpagetab;    /**< 0x30 */
    uint32_t    smte_frectab;     /**< 0x40 */
    uint32_t    smte_impmod;      /**< 0x44 */
    uint32_t    smte_impproc;     /**< 0x48 */
    uint32_t    smte_datapage;    /**< 0x4c */
    uint32_t    smte_nrestab;     /**< 0x50 */
    uint32_t    smte_cbnrestab;   /**< 0x54 */
    uint32_t    smte_autods;      /**< 0x58 */
    uint32_t    smte_debuginfo;   /**< 0x5c */
    uint32_t    smte_debuglen;    /**< 0x60 */
    uint32_t    smte_heapsize;    /**< 0x64 */
    uint32_t    smte_path;        /**< 0x68 Address of full name string. */
    uint16_t    smte_semcount;    /**< 0x6c */
    uint16_t    smte_semowner;    /**< 0x6e */
    uint32_t    smte_pfilecache;  /**< 0x70: Address of cached data if replace-module is used. */
    uint32_t    smte_stacksize;   /**< 0x74: Stack size for .exe thread 1. */
    uint16_t    smte_alignshift;  /**< 0x78: */
    uint16_t    smte_NEexpver;    /**< 0x7a: */
    uint16_t    smte_pathlen;     /**< 0x7c: Length of smte_path */
    uint16_t    smte_NEexetype;   /**< 0x7e: */
    uint16_t    smte_csegpack;    /**< 0x80: */
    uint8_t     smte_major_os;    /**< 0x82: added later to lie about OS version */
    uint8_t     smte_minor_os;    /**< 0x83: added later to lie about OS version */
} LDRSMTE;
AssertCompileSize(LDRSMTE, 0x84);

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

/** Arguments dbgdiggerOS2ProcessModule passes to the module open callback.  */
typedef struct
{
    const char     *pszModPath;
    const char     *pszModName;
    LDRMTE const   *pMte;
    LDRSMTE const  *pSwapMte;
} DBGDIGGEROS2OPEN;


/**
 * @callback_method_impl{FNRTDBGCFGOPEN, Debug image/image searching callback.}
 */
static DECLCALLBACK(int) dbgdiggerOs2OpenModule(RTDBGCFG hDbgCfg, const char *pszFilename, void *pvUser1, void *pvUser2)
{
    DBGDIGGEROS2OPEN *pArgs = (DBGDIGGEROS2OPEN *)pvUser1;

    RTDBGMOD hDbgMod = NIL_RTDBGMOD;
    int rc = RTDbgModCreateFromImage(&hDbgMod, pszFilename, pArgs->pszModName, RTLDRARCH_WHATEVER, hDbgCfg);
    if (RT_SUCCESS(rc))
    {
        /** @todo Do some info matching before using it? */

        *(PRTDBGMOD)pvUser2 = hDbgMod;
        return VINF_CALLBACK_RETURN;
    }
    LogRel(("DbgDiggerOs2: dbgdiggerOs2OpenModule: %Rrc - %s\n", rc, pszFilename));
    return rc;
}


static void dbgdiggerOS2ProcessModule(PUVM pUVM, PDBGDIGGEROS2 pThis, DBGDIGGEROS2BUF *pBuf,
                                      const char *pszCacheSubDir, RTDBGAS hAs, RTDBGCFG hDbgCfg)
{
    RT_NOREF(pThis);

    /*
     * Save the MTE.
     */
    static const char * const s_apszMteFmts[4] = { "Reserved1", "NE", "LX", "Reserved2" };
    LDRMTE const Mte = pBuf->mte;
    if ((Mte.mte_flags2 & MTEFORMATMASK) != MTEFORMATLX)
    {
        LogRel(("DbgDiggerOs2: MTE format not implemented: %s (%d)\n",
                s_apszMteFmts[(Mte.mte_flags2 & MTEFORMATMASK)], Mte.mte_flags2 & MTEFORMATMASK));
        return;
    }

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

    /*
     * Try read the path name, falling back on module name.
     */
    char szModPath[260];
    rc = VERR_READ_ERROR;
    if (SwapMte.smte_path != 0 && SwapMte.smte_pathlen > 0)
    {
        uint32_t cbToRead = RT_MIN(SwapMte.smte_path, sizeof(szModPath) - 1);
        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, SwapMte.smte_path), szModPath, cbToRead);
        szModPath[cbToRead] = '\0';
    }
    if (RT_FAILURE(rc))
    {
        memcpy(szModPath, Mte.mte_modname, sizeof(Mte.mte_modname));
        szModPath[sizeof(Mte.mte_modname)] = '\0';
        RTStrStripR(szModPath);
    }
    LogRel(("DbgDiggerOS2: szModPath='%s'\n", szModPath));

    /*
     * Sanitize the module name.
     */
    char szModName[16];
    memcpy(szModName, Mte.mte_modname, sizeof(Mte.mte_modname));
    szModName[sizeof(Mte.mte_modname)] = '\0';
    RTStrStripR(szModName);

    /*
     * Read the object table into the buffer.
     */
    rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, SwapMte.smte_objtab),
                       &pBuf->aOtes[0], sizeof(pBuf->aOtes[0]) * SwapMte.smte_objcnt);
    if (RT_FAILURE(rc))
    {
        LogRel(("DbgDiggerOs2: Error reading object table @ %#RX32 LB %#zx: %Rrc\n",
                SwapMte.smte_objtab, sizeof(pBuf->aOtes[0]) * SwapMte.smte_objcnt, rc));
        return;
    }
    for (uint32_t i = 0; i < SwapMte.smte_objcnt; i++)
    {
        LogRel(("DbgDiggerOs2:  seg%u: %RX32 LB %#x\n", i, pBuf->aOtes[i].ote_base, pBuf->aOtes[i].ote_size));
        /** @todo validate it. */
    }

    /* No need to continue without an address space (shouldn't happen). */
    if (hAs == NIL_RTDBGAS)
        return;

    /*
     * Try find a debug file for this module.
     */
    RTDBGMOD hDbgMod = NIL_RTDBGMOD;
    if (hDbgCfg != NIL_RTDBGCFG)
    {
        DBGDIGGEROS2OPEN Args = { szModPath, szModName, &Mte, &SwapMte };
        RTDbgCfgOpenEx(hDbgCfg, szModPath, pszCacheSubDir, NULL,
                       RT_OPSYS_OS2 | RTDBGCFG_O_CASE_INSENSITIVE | RTDBGCFG_O_EXECUTABLE_IMAGE
                       | RTDBGCFG_O_RECURSIVE | RTDBGCFG_O_NO_SYSTEM_PATHS,
                       dbgdiggerOs2OpenModule, &Args, &hDbgMod);
    }

    /*
     * Fallback is a simple module into which we insert sections.
     */
    uint32_t cSegments = SwapMte.smte_objcnt;
    if (hDbgMod == NIL_RTDBGMOD)
    {
        rc = RTDbgModCreate(&hDbgMod, szModName, 0 /*cbSeg*/, 0 /*fFlags*/);
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
                    cSegments = i;
                    break;
                }
                uRva += RT_ALIGN_32(pBuf->aOtes[i].ote_size, _4K);
            }
        }
        else
        {
            LogRel(("DbgDiggerOs2: RTDbgModCreate failed: %Rrc\n", rc));
            return;
        }
    }

    /*
     * Tag the module and link its segments.
     */
    rc = RTDbgModSetTag(hDbgMod, DIG_OS2_MOD_TAG);
    if (RT_SUCCESS(rc))
    {
        for (uint32_t i = 0; i < SwapMte.smte_objcnt; i++)
        {
            rc = RTDbgAsModuleLinkSeg(hAs, hDbgMod, i, pBuf->aOtes[i].ote_base, RTDBGASLINK_FLAGS_REPLACE /*fFlags*/);
            if (RT_FAILURE(rc))
                LogRel(("DbgDiggerOs2: RTDbgAsModuleLinkSeg failed (i=%u, ote_base=%#x): %Rrc\n",
                        i, pBuf->aOtes[i].ote_base, rc));
        }
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
                    uint32_t uOs2Krnl = UINT32_MAX;
                    RTDBGCFG hDbgCfg  = DBGFR3AsGetConfig(pUVM); /* (don't release this) */
                    RTDBGAS  hAs      = DBGFR3AsResolveAndRetain(pUVM, DBGF_AS_GLOBAL);

                    char szCacheSubDir[24];
                    RTStrPrintf(szCacheSubDir, sizeof(szCacheSubDir), "os2-%u.%u", pThis->OS2MajorVersion, pThis->OS2MinorVersion);

                    DBGFR3AddrFromFlat(pUVM, &Addr, uBuf.au32[0]);
                    while (Addr.FlatPtr != 0 && Addr.FlatPtr != UINT32_MAX)
                    {
                        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, &uBuf.mte, sizeof(uBuf.mte));
                        if (RT_FAILURE(rc))
                            break;
                        LogRel(("DbgDiggerOs2: Module @ %#010RX32: %.8s %#x %#x\n", (uint32_t)Addr.FlatPtr,
                                uBuf.mte.mte_modname, uBuf.mte.mte_flags1, uBuf.mte.mte_flags2));
                        if (uBuf.mte.mte_flags1 & MTE1_DOSMOD)
                            uOs2Krnl = (uint32_t)Addr.FlatPtr;

                        DBGFR3AddrFromFlat(pUVM, &Addr, uBuf.mte.mte_link);
                        dbgdiggerOS2ProcessModule(pUVM, pThis, &uBuf, szCacheSubDir, hAs, hDbgCfg);
                    }

                    /* Load the kernel again. To make sure we didn't drop any segments due
                       to overlap/conflicts/whatever.  */
                    if (uOs2Krnl != UINT32_MAX)
                    {
                        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, uOs2Krnl),
                                           &uBuf.mte, sizeof(uBuf.mte));
                        if (RT_SUCCESS(rc))
                        {
                            LogRel(("DbgDiggerOs2: Module @ %#010RX32: %.8s %#x %#x [again]\n", (uint32_t)Addr.FlatPtr,
                                    uBuf.mte.mte_modname, uBuf.mte.mte_flags1, uBuf.mte.mte_flags2));
                            dbgdiggerOS2ProcessModule(pUVM, pThis, &uBuf, szCacheSubDir, hAs, hDbgCfg);
                        }
                    }

                    RTDbgAsRelease(hAs);
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
