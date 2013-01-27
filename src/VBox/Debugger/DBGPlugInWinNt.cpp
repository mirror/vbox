/* $Id$ */
/** @file
 * DBGPlugInWindows - Debugger and Guest OS Digger Plugin For Windows NT.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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
#include <VBox/vmm/dbgf.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/stream.h>

#include "../Runtime/include/internal/ldrMZ.h"  /* ugly */
#include "../Runtime/include/internal/ldrPE.h"  /* ugly */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** @name Internal WinNT structures
 * @{ */
/**
 * PsLoadedModuleList entry for 32-bit NT aka LDR_DATA_TABLE_ENTRY.
 * Tested with XP.
 */
typedef struct NTMTE32
{
    struct
    {
        uint32_t    Flink;
        uint32_t    Blink;
    }               InLoadOrderLinks,
                    InMemoryOrderModuleList,
                    InInitializationOrderModuleList;
    uint32_t        DllBase;
    uint32_t        EntryPoint;
    uint32_t        SizeOfImage;
    struct
    {
        uint16_t    Length;
        uint16_t    MaximumLength;
        uint32_t    Buffer;
    }               FullDllName,
                    BaseDllName;
    uint32_t        Flags;
    uint16_t        LoadCount;
    uint16_t        TlsIndex;
    /* ... there is more ... */
} NTMTE32;
typedef NTMTE32 *PNTMTE32;

/**
 * PsLoadedModuleList entry for 32-bit NT aka LDR_DATA_TABLE_ENTRY.
 * Tested with XP.
 *
 * @todo This is incomplete and just to get rid of warnings.
 */
typedef struct NTMTE64
{
    struct
    {
        uint64_t    Flink;
        uint64_t    Blink;
    }               InLoadOrderLinks,
                    InMemoryOrderModuleList,
                    InInitializationOrderModuleList;
    uint64_t        DllBase;
    uint64_t        EntryPoint;
    uint32_t        SizeOfImage;
    uint32_t        Alignment;
    struct
    {
        uint16_t    Length;
        uint16_t    MaximumLength;
        uint32_t    Alignment;
        uint64_t    Buffer;
    }               FullDllName,
                    BaseDllName;
    uint32_t        Flags;
    uint16_t        LoadCount;
    uint16_t        TlsIndex;
    /* ... there is more ... */
} NTMTE64;
typedef NTMTE64 *PNTMTE64;

/** MTE union. */
typedef union NTMTE
{
    NTMTE32         vX_32;
    NTMTE64         vX_64;
} NTMTE;
typedef NTMTE *PNTMTE;


/**
 * The essential bits of the KUSER_SHARED_DATA structure.
 */
typedef struct NTKUSERSHAREDDATA
{
    uint32_t        TickCountLowDeprecated;
    uint32_t        TickCountMultiplier;
    struct
    {
        uint32_t    LowPart;
        int32_t     High1Time;
        int32_t     High2Time;

    }               InterruptTime,
                    SystemTime,
                    TimeZoneBias;
    uint16_t        ImageNumberLow;
    uint16_t        ImageNumberHigh;
    RTUTF16         NtSystemRoot[260];
    uint32_t        MaxStackTraceDepth;
    uint32_t        CryptoExponent;
    uint32_t        TimeZoneId;
    uint32_t        LargePageMinimum;
    uint32_t        Reserved2[7];
    uint32_t        NtProductType;
    uint8_t         ProductTypeIsValid;
    uint8_t         abPadding[3];
    uint32_t        NtMajorVersion;
    uint32_t        NtMinorVersion;
    /* uint8_t         ProcessorFeatures[64];
    ...
    */
} NTKUSERSHAREDDATA;
typedef NTKUSERSHAREDDATA *PNTKUSERSHAREDDATA;

/** KI_USER_SHARED_DATA for i386 */
#define NTKUSERSHAREDDATA_WINNT32   UINT32_C(0xffdf0000)
/** KI_USER_SHARED_DATA for AMD64 */
#define NTKUSERSHAREDDATA_WINNT64   UINT64_C(0xfffff78000000000)

/** NTKUSERSHAREDDATA::NtProductType */
typedef enum NTPRODUCTTYPE
{
    kNtProductType_Invalid = 0,
    kNtProductType_WinNt = 1,
    kNtProductType_LanManNt,
    kNtProductType_Server
} NTPRODUCTTYPE;

/**
 * PDB v2.0 in image debug info.
 * The URL is constructed from the timestamp and the %02x age?
 */
typedef struct CV_INFO_PDB20
{
    uint32_t    Signature;              /**< CV_SIGNATURE_PDB70. */
    int32_t     Offset;                 /**< Always 0. Used to be the offset to the real debug info. */
    uint32_t    TimeDateStamp;
    uint32_t    Age;
    uint8_t     PdbFilename[4];
} CV_INFO_PDB20;
/** The CV_INFO_PDB20 signature. */
#define CV_SIGNATURE_PDB20  RT_MAKE_U32_FROM_U8('N','B','1','0')

/**
 * PDB v7.0 in image debug info.
 * The URL is constructed from the signature and the %02x age.
 */
#pragma pack(4)
typedef struct CV_INFO_PDB70
{
    uint32_t    CvSignature;            /**< CV_SIGNATURE_PDB70. */
    RTUUID      Signature;
    uint32_t    Age;
    uint8_t     PdbFilename[4];
} CV_INFO_PDB70;
#pragma pack()
AssertCompileMemberOffset(CV_INFO_PDB70, Signature, 4);
AssertCompileMemberOffset(CV_INFO_PDB70, Age, 4 + 16);
/** The CV_INFO_PDB70 signature. */
#define CV_SIGNATURE_PDB70  RT_MAKE_U32_FROM_U8('R','S','D','S')

/** @} */


typedef enum DBGDIGGERWINNTVER
{
    DBGDIGGERWINNTVER_UNKNOWN,
    DBGDIGGERWINNTVER_3_1,
    DBGDIGGERWINNTVER_3_5,
    DBGDIGGERWINNTVER_4_0,
    DBGDIGGERWINNTVER_5_0,
    DBGDIGGERWINNTVER_5_1,
    DBGDIGGERWINNTVER_6_0
} DBGDIGGERWINNTVER;

/**
 * WinNT guest OS digger instance data.
 */
typedef struct DBGDIGGERWINNT
{
    /** Whether the information is valid or not.
     * (For fending off illegal interface method calls.) */
    bool                fValid;
    /** 32-bit (true) or 64-bit (false) */
    bool                f32Bit;

    /** The NT version. */
    DBGDIGGERWINNTVER   enmVer;
    /** NTKUSERSHAREDDATA::NtProductType */
    NTPRODUCTTYPE       NtProductType;
    /** NTKUSERSHAREDDATA::NtMajorVersion */
    uint32_t            NtMajorVersion;
    /** NTKUSERSHAREDDATA::NtMinorVersion */
    uint32_t            NtMinorVersion;

    /** The address of the ntoskrnl.exe image. */
    DBGFADDRESS         KernelAddr;
    /** The address of the ntoskrnl.exe module table entry. */
    DBGFADDRESS         KernelMteAddr;
    /** The address of PsLoadedModuleList. */
    DBGFADDRESS         PsLoadedModuleListAddr;
} DBGDIGGERWINNT;
/** Pointer to the linux guest OS digger instance data. */
typedef DBGDIGGERWINNT *PDBGDIGGERWINNT;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a 32-bit Windows NT kernel address */
#define WINNT32_VALID_ADDRESS(Addr)         ((Addr) >         UINT32_C(0x80000000) && (Addr) <         UINT32_C(0xfffff000))
/** Validates a 64-bit Windows NT kernel address */
#define WINNT64_VALID_ADDRESS(Addr)         ((Addr) > UINT64_C(0xffffffff80000000) && (Addr) < UINT64_C(0xfffffffffffff000))
/** Validates a kernel address. */
#define WINNT_VALID_ADDRESS(pThis, Addr)    ((pThis)->f32Bit ? WINNT32_VALID_ADDRESS(Addr) : WINNT64_VALID_ADDRESS(Addr))
/** Versioned and bitness wrapper. */
#define WINNT_UNION(pThis, pUnion, Member)  ((pThis)->f32Bit ? (pUnion)->vX_32. Member : (pUnion)->vX_64. Member )

/** The length (in chars) of the kernel file name (no path). */
#define WINNT_KERNEL_BASE_NAME_LEN          12

/** WindowsNT on little endian ASCII systems. */
#define DIG_WINNT_MOD_TAG                   UINT64_C(0x54696e646f774e54)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int)  dbgDiggerWinNtInit(PUVM pUVM, void *pvData);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Kernel names. */
static const RTUTF16 g_wszKernelNames[][WINNT_KERNEL_BASE_NAME_LEN + 1] =
{
    { 'n', 't', 'o', 's', 'k', 'r', 'n', 'l', '.', 'e', 'x', 'e' }
};


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
static void dbgDiggerWinNtProcessImage(PDBGDIGGERWINNT pThis, PUVM pUVM, const char *pszName,
                                       PCDBGFADDRESS pImageAddr, uint32_t cbImage,
                                       uint8_t *pbBuf, size_t cbBuf)
{
    LogFlow(("DigWinNt: %RGp %#x %s\n", pImageAddr->FlatPtr, cbImage, pszName));

    /*
     * Do some basic validation first.
     * This is the usual exteremely verbose and messy code...
     */
    Assert(cbBuf >= sizeof(IMAGE_NT_HEADERS64));
    if (   cbImage < sizeof(IMAGE_NT_HEADERS64)
        || cbImage >= _1M * 256)
    {
        Log(("DigWinNt: %s: Bad image size: %#x\n", pszName, cbImage));
        return;
    }

    /* Dig out the NT/PE headers. */
    IMAGE_DOS_HEADER const *pMzHdr = (IMAGE_DOS_HEADER const *)pbBuf;
    typedef union NTHDRSU
    {
        IMAGE_NT_HEADERS64  vX_32;
        IMAGE_NT_HEADERS64  vX_64;
    } NTHDRS;
    NTHDRS const   *pHdrs;
    uint32_t        offHdrs;
    if (pMzHdr->e_magic != IMAGE_DOS_SIGNATURE)
    {
        offHdrs = 0;
        pHdrs   = (NTHDRS const *)pbBuf;
    }
    else if (   pMzHdr->e_lfanew >= cbImage
             || pMzHdr->e_lfanew < sizeof(*pMzHdr)
             || pMzHdr->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > cbImage)
    {
        Log(("DigWinNt: %s: PE header to far into image: %#x  cbImage=%#x\n", pMzHdr->e_lfanew, cbImage));
        return;
    }
    else if (   pMzHdr->e_lfanew < cbBuf
             && pMzHdr->e_lfanew + sizeof(IMAGE_NT_HEADERS64) <= cbBuf)
    {
        offHdrs = pMzHdr->e_lfanew;
        pHdrs = (NTHDRS const *)(pbBuf + offHdrs);
    }
    else
    {
        Log(("DigWinNt: %s: PE header to far into image (lazy bird): %#x\n",  pMzHdr->e_lfanew));
        return;
    }
    if (pHdrs->vX_32.Signature != IMAGE_NT_SIGNATURE)
    {
        Log(("DigWinNt: %s: Bad PE signature: %#x\n", pszName, pHdrs->vX_32.Signature));
        return;
    }

    /* The file header is the same on both archs */
    if (pHdrs->vX_32.FileHeader.Machine != (pThis->f32Bit ? IMAGE_FILE_MACHINE_I386 : IMAGE_FILE_MACHINE_AMD64))
    {
        Log(("DigWinNt: %s: Invalid FH.Machine: %#x\n", pszName, pHdrs->vX_32.FileHeader.Machine));
        return;
    }
    if (pHdrs->vX_32.FileHeader.SizeOfOptionalHeader != (pThis->f32Bit ? sizeof(IMAGE_OPTIONAL_HEADER32) : sizeof(IMAGE_OPTIONAL_HEADER64)))
    {
        Log(("DigWinNt: %s: Invalid FH.SizeOfOptionalHeader: %#x\n", pszName, pHdrs->vX_32.FileHeader.SizeOfOptionalHeader));
        return;
    }
    const uint32_t TimeDateStamp = pHdrs->vX_32.FileHeader.TimeDateStamp;

    /* The optional header is not... */
    if (WINNT_UNION(pThis, pHdrs, OptionalHeader.Magic) != (pThis->f32Bit ? IMAGE_NT_OPTIONAL_HDR32_MAGIC : IMAGE_NT_OPTIONAL_HDR64_MAGIC))
    {
        Log(("DigWinNt: %s: Invalid OH.Magic: %#x\n", pszName, WINNT_UNION(pThis, pHdrs, OptionalHeader.Magic)));
        return;
    }
    if (WINNT_UNION(pThis, pHdrs, OptionalHeader.SizeOfImage) != cbImage)
    {
        Log(("DigWinNt: %s: Invalid OH.SizeOfImage: %#x, expected %#x\n", pszName, WINNT_UNION(pThis, pHdrs, OptionalHeader.SizeOfImage), cbImage));
        return;
    }
    if (WINNT_UNION(pThis, pHdrs, OptionalHeader.NumberOfRvaAndSizes) != IMAGE_NUMBEROF_DIRECTORY_ENTRIES)
    {
        Log(("DigWinNt: %s: Invalid OH.SizeOfImage: %#x\n", pszName, WINNT_UNION(pThis, pHdrs, OptionalHeader.NumberOfRvaAndSizes)));
        return;
    }

    uint32_t uRvaDebugDir = 0;
    uint32_t cbDebugDir   = 0;
    IMAGE_DATA_DIRECTORY const *pDir = &WINNT_UNION(pThis, pHdrs, OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG]);
    if (    pDir->VirtualAddress > offHdrs
        &&  pDir->VirtualAddress < cbImage
        &&  pDir->Size           >= sizeof(IMAGE_DEBUG_DIRECTORY)
        &&  pDir->Size           < cbImage
        &&  pDir->VirtualAddress + pDir->Size <= cbImage
       )
    {
        uRvaDebugDir = pDir->VirtualAddress;
        cbDebugDir   = pDir->Size;
    }

    /* dig into the section table... */

    /*
     * Create the module.
     */
    RTDBGMOD hMod;
    int rc = RTDbgModCreate(&hMod, pszName, cbImage, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return;
    rc = RTDbgModSetTag(hMod, DIG_WINNT_MOD_TAG); AssertRC(rc);

    /* temp hack: */
    rc = RTDbgModSymbolAdd(hMod, "start", 0 /*iSeg*/, 0, cbImage, 0 /*fFlags*/, NULL); AssertRC(rc);

    /* add sections? */

    /*
     * Dig out debug info if possible.  What we're after is the CODEVIEW part.
     */
    if (uRvaDebugDir != 0)
    {
        DBGFADDRESS Addr = *pImageAddr;
        DBGFR3AddrAdd(&Addr, uRvaDebugDir);
        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, pbBuf, RT_MIN(cbDebugDir, cbBuf));
        if (RT_SUCCESS(rc))
        {
            IMAGE_DEBUG_DIRECTORY const    *pa = (IMAGE_DEBUG_DIRECTORY const *)pbBuf;
            size_t                          c  = RT_MIN(RT_MIN(cbDebugDir, cbBuf) / sizeof(*pa), 10);
            for (uint32_t i = 0; i < c; i++)
                if (    pa[i].AddressOfRawData > offHdrs
                    &&  pa[i].AddressOfRawData < cbImage
                    &&  pa[i].SizeOfData       < cbImage
                    &&  pa[i].AddressOfRawData + pa[i].SizeOfData <= cbImage
                    &&  pa[i].TimeDateStamp   == TimeDateStamp /* too paranoid? */
                    &&  pa[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW
                    )
                {
                }
        }
    }

    /*
     * Link the module.
     */
    RTDBGAS hAs = DBGFR3AsResolveAndRetain(pUVM, DBGF_AS_KERNEL);
    if (hAs != NIL_RTDBGAS)
        rc = RTDbgAsModuleLink(hAs, hMod, pImageAddr->FlatPtr, RTDBGASLINK_FLAGS_REPLACE /*fFlags*/);
    else
        rc = VERR_INTERNAL_ERROR;
    RTDbgModRelease(hMod);
    RTDbgAsRelease(hAs);
}


/**
 * @copydoc DBGFOSREG::pfnQueryInterface
 */
static DECLCALLBACK(void *) dbgDiggerWinNtQueryInterface(PUVM pUVM, void *pvData, DBGFOSINTERFACE enmIf)
{
    return NULL;
}


/**
 * @copydoc DBGFOSREG::pfnQueryVersion
 */
static DECLCALLBACK(int)  dbgDiggerWinNtQueryVersion(PUVM pUVM, void *pvData, char *pszVersion, size_t cchVersion)
{
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    Assert(pThis->fValid);
    const char *pszNtProductType;
    switch (pThis->NtProductType)
    {
        case kNtProductType_WinNt:      pszNtProductType = "-WinNT";        break;
        case kNtProductType_LanManNt:   pszNtProductType = "-LanManNT";     break;
        case kNtProductType_Server:     pszNtProductType = "-Server";       break;
        default:                        pszNtProductType = "";              break;
    }
    RTStrPrintf(pszVersion, cchVersion, "%u.%u%s", pThis->NtMajorVersion, pThis->NtMinorVersion, pszNtProductType);
    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnTerm
 */
static DECLCALLBACK(void)  dbgDiggerWinNtTerm(PUVM pUVM, void *pvData)
{
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    Assert(pThis->fValid);

    pThis->fValid = false;
}


/**
 * @copydoc DBGFOSREG::pfnRefresh
 */
static DECLCALLBACK(int)  dbgDiggerWinNtRefresh(PUVM pUVM, void *pvData)
{
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
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
                if (RTDbgModGetTag(hMod) == DIG_WINNT_MOD_TAG)
                {
                    int rc = RTDbgAsModuleUnlink(hDbgAs, hMod);
                    AssertRC(rc);
                }
                RTDbgModRelease(hMod);
            }
        }
        RTDbgAsRelease(hDbgAs);
    }

    dbgDiggerWinNtTerm(pUVM, pvData);
    return dbgDiggerWinNtInit(pUVM, pvData);
}


/**
 * @copydoc DBGFOSREG::pfnInit
 */
static DECLCALLBACK(int)  dbgDiggerWinNtInit(PUVM pUVM, void *pvData)
{
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    Assert(!pThis->fValid);

    union
    {
        uint8_t             au8[0x2000];
        RTUTF16             wsz[0x2000/2];
        NTKUSERSHAREDDATA   UserSharedData;
    }               u;
    DBGFADDRESS     Addr;
    int             rc;

    /*
     * Figure the NT version.
     */
    DBGFR3AddrFromFlat(pUVM, &Addr, pThis->f32Bit ? NTKUSERSHAREDDATA_WINNT32 : NTKUSERSHAREDDATA_WINNT64);
    rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, &u, PAGE_SIZE);
    if (RT_FAILURE(rc))
        return rc;
    pThis->NtProductType  = u.UserSharedData.ProductTypeIsValid && u.UserSharedData.NtProductType <= kNtProductType_Server
                          ? (NTPRODUCTTYPE)u.UserSharedData.NtProductType
                          : kNtProductType_Invalid;
    pThis->NtMajorVersion = u.UserSharedData.NtMajorVersion;
    pThis->NtMinorVersion = u.UserSharedData.NtMinorVersion;

    /*
     * Dig out the module chain.
     */
    DBGFADDRESS AddrPrev = pThis->PsLoadedModuleListAddr;
    Addr                 = pThis->KernelMteAddr;
    do
    {
        /* Read the validate the MTE. */
        NTMTE Mte;
        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, &Mte, pThis->f32Bit ? sizeof(Mte.vX_32) : sizeof(Mte.vX_64));
        if (RT_FAILURE(rc))
            break;
        if (WINNT_UNION(pThis, &Mte, InLoadOrderLinks.Blink) != AddrPrev.FlatPtr)
        {
            Log(("DigWinNt: Bad Mte At %RGv - backpointer\n", Addr.FlatPtr));
            break;
        }
        if (!WINNT_VALID_ADDRESS(pThis, WINNT_UNION(pThis, &Mte, InLoadOrderLinks.Flink)) )
        {
            Log(("DigWinNt: Bad Mte at %RGv - forward pointer\n", Addr.FlatPtr));
            break;
        }
        if (!WINNT_VALID_ADDRESS(pThis, WINNT_UNION(pThis, &Mte, BaseDllName.Buffer)))
        {
            Log(("DigWinNt: Bad Mte at %RGv - BaseDllName=%llx\n", Addr.FlatPtr, WINNT_UNION(pThis, &Mte, BaseDllName.Buffer)));
            break;
        }
        if (!WINNT_VALID_ADDRESS(pThis, WINNT_UNION(pThis, &Mte, FullDllName.Buffer)))
        {
            Log(("DigWinNt: Bad Mte at %RGv - FullDllName=%llx\n", Addr.FlatPtr, WINNT_UNION(pThis, &Mte, FullDllName.Buffer)));
            break;
        }
        if (    !WINNT_VALID_ADDRESS(pThis, WINNT_UNION(pThis, &Mte, DllBase))
            ||  WINNT_UNION(pThis, &Mte, SizeOfImage) > _1M*256
            ||  WINNT_UNION(pThis, &Mte, EntryPoint) - WINNT_UNION(pThis, &Mte, DllBase) > WINNT_UNION(pThis, &Mte, SizeOfImage) )
        {
            Log(("DigWinNt: Bad Mte at %RGv - EntryPoint=%llx SizeOfImage=%x DllBase=%llx\n",
                 Addr.FlatPtr, WINNT_UNION(pThis, &Mte, EntryPoint), WINNT_UNION(pThis, &Mte, SizeOfImage), WINNT_UNION(pThis, &Mte, DllBase)));
            break;
        }

        /* Read the full name. */
        DBGFADDRESS AddrName;
        DBGFR3AddrFromFlat(pUVM, &AddrName, WINNT_UNION(pThis, &Mte, FullDllName.Buffer));
        uint16_t    cbName = WINNT_UNION(pThis, &Mte, FullDllName.Length);
        if (cbName < sizeof(u))
            rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &AddrName, &u, cbName);
        else
            rc = VERR_OUT_OF_RANGE;
        if (RT_FAILURE(rc))
        {
            DBGFR3AddrFromFlat(pUVM, &AddrName, WINNT_UNION(pThis, &Mte, BaseDllName.Buffer));
            cbName = WINNT_UNION(pThis, &Mte, BaseDllName.Length);
            if (cbName < sizeof(u))
                rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &AddrName, &u, cbName);
            else
                rc = VERR_OUT_OF_RANGE;
        }
        if (RT_SUCCESS(rc))
        {
            u.wsz[cbName/2] = '\0';
            char *pszName;
            rc = RTUtf16ToUtf8(u.wsz, &pszName);
            if (RT_SUCCESS(rc))
            {
                /* Read the start of the PE image and pass it along to a worker. */
                DBGFADDRESS ImageAddr;
                DBGFR3AddrFromFlat(pUVM, &ImageAddr, WINNT_UNION(pThis, &Mte, DllBase));
                uint32_t    cbImageBuf = RT_MIN(sizeof(u), WINNT_UNION(pThis, &Mte, SizeOfImage));
                rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &ImageAddr, &u, cbImageBuf);
                if (RT_SUCCESS(rc))
                    dbgDiggerWinNtProcessImage(pThis,
                                               pUVM,
                                               pszName,
                                               &ImageAddr,
                                               WINNT_UNION(pThis, &Mte, SizeOfImage),
                                               &u.au8[0],
                                               sizeof(u));
                RTStrFree(pszName);
            }
        }

        /* next */
        AddrPrev = Addr;
        DBGFR3AddrFromFlat(pUVM, &Addr, WINNT_UNION(pThis, &Mte, InLoadOrderLinks.Flink));
    } while (   Addr.FlatPtr != pThis->KernelMteAddr.FlatPtr
             && Addr.FlatPtr != pThis->PsLoadedModuleListAddr.FlatPtr);

    pThis->fValid = true;
    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnProbe
 */
static DECLCALLBACK(bool)  dbgDiggerWinNtProbe(PUVM pUVM, void *pvData)
{
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    DBGFADDRESS     Addr;
    union
    {
        uint8_t             au8[8192];
        uint16_t            au16[8192/2];
        uint32_t            au32[8192/4];
        IMAGE_DOS_HEADER    MzHdr;
        RTUTF16             wsz[8192/2];
    } u;

    /*
     * Look for the MISYSPTE section name that seems to be a part of all kernels.
     * Then try find the module table entry for it.  Since it's the first entry
     * in the PsLoadedModuleList we can easily validate the list head and report
     * success.
     */
    CPUMMODE enmMode = DBGFR3CpuGetMode(pUVM, 0 /*idCpu*/);
    if (enmMode == CPUMMODE_LONG)
    {
        /** @todo when 32-bit is working, add support for 64-bit windows nt. */
    }
    else
    {
        DBGFADDRESS KernelAddr;
        for (DBGFR3AddrFromFlat(pUVM, &KernelAddr, UINT32_C(0x80001000));
             KernelAddr.FlatPtr < UINT32_C(0xffff0000);
             KernelAddr.FlatPtr += PAGE_SIZE)
        {
            int rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &KernelAddr, UINT32_C(0xffff0000) - KernelAddr.FlatPtr,
                                   1, "MISYSPTE", sizeof("MISYSPTE") - 1, &KernelAddr);
            if (RT_FAILURE(rc))
                break;
            DBGFR3AddrSub(&KernelAddr, KernelAddr.FlatPtr & PAGE_OFFSET_MASK);

            /* MZ + PE header. */
            rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &KernelAddr, &u, sizeof(u));
            if (    RT_SUCCESS(rc)
                &&  u.MzHdr.e_magic == IMAGE_DOS_SIGNATURE
                &&  !(u.MzHdr.e_lfanew & 0x7)
                &&  u.MzHdr.e_lfanew >= 0x080
                &&  u.MzHdr.e_lfanew <= 0x200)
            {
                IMAGE_NT_HEADERS32 const *pHdrs = (IMAGE_NT_HEADERS32 const *)&u.au8[u.MzHdr.e_lfanew];
                if (    pHdrs->Signature                            == IMAGE_NT_SIGNATURE
                    &&  pHdrs->FileHeader.Machine                   == IMAGE_FILE_MACHINE_I386
                    &&  pHdrs->FileHeader.SizeOfOptionalHeader      == sizeof(pHdrs->OptionalHeader)
                    &&  pHdrs->FileHeader.NumberOfSections          >= 10 /* the kernel has lots */
                    &&  (pHdrs->FileHeader.Characteristics & (IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL)) == IMAGE_FILE_EXECUTABLE_IMAGE
                    &&  pHdrs->OptionalHeader.Magic                 == IMAGE_NT_OPTIONAL_HDR32_MAGIC
                    &&  pHdrs->OptionalHeader.NumberOfRvaAndSizes   == IMAGE_NUMBEROF_DIRECTORY_ENTRIES
                    /** @todo need more ntoskrnl signs? */
                    )
                {
                    /* Find the MTE. */
                    NTMTE32 Mte;
                    RT_ZERO(Mte);
                    Mte.DllBase     = KernelAddr.FlatPtr;
                    Mte.EntryPoint  = KernelAddr.FlatPtr + pHdrs->OptionalHeader.AddressOfEntryPoint;
                    Mte.SizeOfImage = pHdrs->OptionalHeader.SizeOfImage;
                    DBGFADDRESS HitAddr;
                    rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &KernelAddr, UINT32_MAX - KernelAddr.FlatPtr,
                                       4 /*align*/, &Mte.DllBase, 3 * sizeof(uint32_t), &HitAddr);
                    while (RT_SUCCESS(rc))
                    {
                        /* check the name. */
                        NTMTE32 Mte2;
                        DBGFADDRESS MteAddr = HitAddr;
                        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrSub(&MteAddr, RT_OFFSETOF(NTMTE32, DllBase)),
                                           &Mte2, sizeof(Mte2));
                        if (    RT_SUCCESS(rc)
                            &&  Mte2.DllBase     == Mte.DllBase
                            &&  Mte2.EntryPoint  == Mte.EntryPoint
                            &&  Mte2.SizeOfImage == Mte.SizeOfImage
                            &&  WINNT32_VALID_ADDRESS(Mte2.InLoadOrderLinks.Flink)
                            &&  Mte2.InLoadOrderLinks.Blink > KernelAddr.FlatPtr    /* list head inside ntoskrnl */
                            &&  Mte2.InLoadOrderLinks.Blink < KernelAddr.FlatPtr + Mte.SizeOfImage
                            &&  WINNT32_VALID_ADDRESS(Mte2.BaseDllName.Buffer)
                            &&  WINNT32_VALID_ADDRESS(Mte2.FullDllName.Buffer)
                            &&  Mte2.BaseDllName.Length <= Mte2.BaseDllName.MaximumLength
                            &&  Mte2.BaseDllName.Length == WINNT_KERNEL_BASE_NAME_LEN * 2
                            &&  Mte2.FullDllName.Length <= Mte2.FullDllName.MaximumLength
                            &&  Mte2.FullDllName.Length <= 256
                           )
                        {
                            rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, Mte2.BaseDllName.Buffer),
                                               u.wsz, Mte2.BaseDllName.Length);
                            u.wsz[Mte2.BaseDllName.Length / 2] = '\0';
                            if (    RT_SUCCESS(rc)
                                &&  (   !RTUtf16ICmp(u.wsz, g_wszKernelNames[0])
                                  /* || !RTUtf16ICmp(u.wsz, g_wszKernelNames[1]) */
                                    )
                               )
                            {
                                NTMTE32 Mte3;
                                rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, Mte2.InLoadOrderLinks.Blink),
                                                   &Mte3, RT_SIZEOFMEMB(NTMTE32, InLoadOrderLinks));
                                if (   RT_SUCCESS(rc)
                                    && Mte3.InLoadOrderLinks.Flink == MteAddr.FlatPtr
                                    && WINNT32_VALID_ADDRESS(Mte3.InLoadOrderLinks.Blink) )
                                {
                                    Log(("DigWinNt: MteAddr=%RGv KernelAddr=%RGv SizeOfImage=%x &PsLoadedModuleList=%RGv (32-bit)\n",
                                         MteAddr.FlatPtr, KernelAddr.FlatPtr, Mte2.SizeOfImage, Addr.FlatPtr));
                                    pThis->KernelAddr               = KernelAddr;
                                    pThis->KernelMteAddr            = MteAddr;
                                    pThis->PsLoadedModuleListAddr   = Addr;
                                    pThis->f32Bit                   = true;
                                    return true;
                                }
                            }
                        }

                        /* next */
                        DBGFR3AddrAdd(&HitAddr, 4);
                        if (HitAddr.FlatPtr <= UINT32_C(0xfffff000))
                            rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &HitAddr, UINT32_MAX - HitAddr.FlatPtr,
                                               4 /*align*/, &Mte.DllBase, 3 * sizeof(uint32_t), &HitAddr);
                        else
                            rc = VERR_DBGF_MEM_NOT_FOUND;
                    }
                }
            }
        }
    }
    return false;
}


/**
 * @copydoc DBGFOSREG::pfnDestruct
 */
static DECLCALLBACK(void)  dbgDiggerWinNtDestruct(PUVM pUVM, void *pvData)
{

}


/**
 * @copydoc DBGFOSREG::pfnConstruct
 */
static DECLCALLBACK(int)  dbgDiggerWinNtConstruct(PUVM pUVM, void *pvData)
{
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    pThis->fValid = false;
    pThis->f32Bit = false;
    pThis->enmVer = DBGDIGGERWINNTVER_UNKNOWN;
    return VINF_SUCCESS;
}


const DBGFOSREG g_DBGDiggerWinNt =
{
    /* .u32Magic = */           DBGFOSREG_MAGIC,
    /* .fFlags = */             0,
    /* .cbData = */             sizeof(DBGDIGGERWINNT),
    /* .szName = */             "WinNT",
    /* .pfnConstruct = */       dbgDiggerWinNtConstruct,
    /* .pfnDestruct = */        dbgDiggerWinNtDestruct,
    /* .pfnProbe = */           dbgDiggerWinNtProbe,
    /* .pfnInit = */            dbgDiggerWinNtInit,
    /* .pfnRefresh = */         dbgDiggerWinNtRefresh,
    /* .pfnTerm = */            dbgDiggerWinNtTerm,
    /* .pfnQueryVersion = */    dbgDiggerWinNtQueryVersion,
    /* .pfnQueryInterface = */  dbgDiggerWinNtQueryInterface,
    /* .u32EndMagic = */        DBGFOSREG_MAGIC
};

