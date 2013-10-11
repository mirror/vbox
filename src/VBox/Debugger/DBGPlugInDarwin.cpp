/* $Id$ */
/** @file
 * DBGPlugInDarwin - Debugger and Guest OS Digger Plugin For Darwin / OS X.
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
#include <VBox/vmm/dbgf.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/uuid.h>
#include <iprt/ctype.h>
#include <iprt/formats/mach-o.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** @name Internal Darwin structures
 * @{ */


/** @} */


/**
 * Linux guest OS digger instance data.
 */
typedef struct DBGDIGGERDARWIN
{
    /** Whether the information is valid or not.
     * (For fending off illegal interface method calls.) */
    bool fValid;

    /** The address of an kernel version string (there are several).
     * This is set during probing. */
    DBGFADDRESS AddrKernelVersion;
    /** Kernel base address.
     * This is set during probing. */
    DBGFADDRESS AddrKernel;
} DBGDIGGERDARWIN;
/** Pointer to the linux guest OS digger instance data. */
typedef DBGDIGGERDARWIN *PDBGDIGGERDARWIN;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a 32-bit linux kernel address */
#define DARWIN32_VALID_ADDRESS(Addr)    ((Addr) > UINT32_C(0x80000000) && (Addr) < UINT32_C(0xfffff000))

/** The max kernel size. */
#define DARWIN_MAX_KERNEL_SIZE          0x0f000000

/** AppleOsX on little endian ASCII systems. */
#define DIG_DARWIN_MOD_TAG              UINT64_C(0x58734f656c707041)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int)  dbgDiggerDarwinInit(PUVM pUVM, void *pvData);


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
static DECLCALLBACK(void *) dbgDiggerDarwinQueryInterface(PUVM pUVM, void *pvData, DBGFOSINTERFACE enmIf)
{
    return NULL;
}


/**
 * @copydoc DBGFOSREG::pfnQueryVersion
 */
static DECLCALLBACK(int)  dbgDiggerDarwinQueryVersion(PUVM pUVM, void *pvData, char *pszVersion, size_t cchVersion)
{
    PDBGDIGGERDARWIN pThis = (PDBGDIGGERDARWIN)pvData;
    Assert(pThis->fValid);

    /*
     * It's all in the linux banner.
     */
    int rc = DBGFR3MemReadString(pUVM, 0, &pThis->AddrKernelVersion, pszVersion, cchVersion);
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
static DECLCALLBACK(void)  dbgDiggerDarwinTerm(PUVM pUVM, void *pvData)
{
    PDBGDIGGERDARWIN pThis = (PDBGDIGGERDARWIN)pvData;

    pThis->fValid = false;
}


/**
 * @copydoc DBGFOSREG::pfnRefresh
 */
static DECLCALLBACK(int)  dbgDiggerDarwinRefresh(PUVM pUVM, void *pvData)
{
    PDBGDIGGERDARWIN pThis = (PDBGDIGGERDARWIN)pvData;
    NOREF(pThis);
    Assert(pThis->fValid);

    /*
     * For now we'll flush and reload everything.
     */
    dbgDiggerDarwinTerm(pUVM, pvData);
    return dbgDiggerDarwinInit(pUVM, pvData);
}


/**
 * Helper function that validates a segment (or section) name.
 *
 * @returns true if valid, false if not.
 * @param   pszName             The name string.
 * @param   cbName              The size of the string, including terminator.
 */
static bool dbgDiggerDarwinIsValidSegOrSectName(const char *pszName, size_t cbName)
{
    /* ascii chars */
    char ch;
    size_t off = 0;
    while (off < cbName && (ch = pszName[off]))
    {
        if (RT_C_IS_CNTRL(ch) || ch > 127)
            return false;
        off++;
    }

    /* Not empty nor 100% full. */
    if (off == 0 || off == cbName)
        return false;

    /* remainder should be zeros. */
    while (off < cbName)
    {
        if (pszName[off])
            return false;
        off++;
    }

    return true;
}


static int dbgDiggerDarwinAddModule(PDBGDIGGERDARWIN pThis, PUVM pUVM, uint64_t uModAddr, const char *pszName)
{
    union
    {
        uint8_t             ab[2 * X86_PAGE_4K_SIZE];
        mach_header_64_t    Hdr64;
        mach_header_32_t    Hdr32;
    } uBuf;

    /* Read the first page of the image. */
    DBGFADDRESS ModAddr;
    int rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &ModAddr, uModAddr), uBuf.ab, X86_PAGE_4K_SIZE);
    if (RT_FAILURE(rc))
        return rc;

    /* Validate the header. */
    AssertCompileMembersSameSizeAndOffset(mach_header_64_t, magic,   mach_header_32_t, magic);
    if (   uBuf.Hdr64.magic != IMAGE_MACHO64_SIGNATURE
        && uBuf.Hdr32.magic != IMAGE_MACHO32_SIGNATURE)
        return VERR_INVALID_EXE_SIGNATURE;
    AssertCompileMembersSameSizeAndOffset(mach_header_64_t, cputype, mach_header_32_t, cputype);
    bool f64Bit = uBuf.Hdr64.magic == IMAGE_MACHO64_SIGNATURE;
    if (uBuf.Hdr32.cputype != (f64Bit ? CPU_TYPE_X86_64 : CPU_TYPE_I386))
        return VERR_LDR_ARCH_MISMATCH;
    AssertCompileMembersSameSizeAndOffset(mach_header_64_t, filetype, mach_header_32_t, filetype);
    if (   uBuf.Hdr32.filetype != MH_EXECUTE
        && uBuf.Hdr32.filetype != (f64Bit ? MH_KEXT_BUNDLE : MH_OBJECT))
        return VERR_BAD_EXE_FORMAT;
    AssertCompileMembersSameSizeAndOffset(mach_header_64_t, ncmds, mach_header_32_t, ncmds);
    if (uBuf.Hdr32.ncmds > 256)
        return VERR_BAD_EXE_FORMAT;
    AssertCompileMembersSameSizeAndOffset(mach_header_64_t, sizeofcmds, mach_header_32_t, sizeofcmds);
    if (uBuf.Hdr32.sizeofcmds > X86_PAGE_4K_SIZE * 2 - sizeof(mach_header_64_t))
        return VERR_BAD_EXE_FORMAT;

    /* Do we need to read a 2nd page to get all the load commands? If so, do it. */
    if (uBuf.Hdr32.sizeofcmds + (f64Bit ? sizeof(mach_header_64_t) : sizeof(mach_header_32_t)) > X86_PAGE_4K_SIZE)
    {
        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &ModAddr, uModAddr + X86_PAGE_4K_SIZE),
                           &uBuf.ab[X86_PAGE_4K_SIZE], X86_PAGE_4K_SIZE);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Process the load commands.
     */
    RTDBGSEGMENT    aSegs[12];
    uint32_t        cSegs  = 0;
    RTUUID          Uuid   = RTUUID_INITIALIZE_NULL;
    uint32_t        cLeft  = uBuf.Hdr32.ncmds;
    uint32_t        cbLeft = uBuf.Hdr32.sizeofcmds;
    union
    {
        uint8_t const              *pb;
        load_command_t const       *pGenric;
        segment_command_32_t const *pSeg32;
        segment_command_64_t const *pSeg64;
        section_32_t const         *pSect32;
        section_64_t const         *pSect64;
        symtab_command_t const     *pSymTab;
        uuid_command_t const       *pUuid;
    } uLCmd;
    uLCmd.pb = &uBuf.ab[f64Bit ? sizeof(mach_header_64_t) : sizeof(mach_header_32_t)];

    while (cLeft-- > 0)
    {
        uint32_t const cbCmd = uLCmd.pGenric->cmdsize;
        if (cbCmd > cbLeft || cbCmd < sizeof(load_command_t))
            return VERR_BAD_EXE_FORMAT;

        switch (uLCmd.pGenric->cmd)
        {
            case LC_SEGMENT_32:
                if (cbCmd != sizeof(segment_command_32_t) + uLCmd.pSeg32->nsects * sizeof(section_32_t))
                    return VERR_BAD_EXE_FORMAT;
                if (!dbgDiggerDarwinIsValidSegOrSectName(uLCmd.pSeg32->segname, sizeof(uLCmd.pSeg32->segname)))
                    return VERR_INVALID_NAME;
                if (!strcmp(uLCmd.pSeg32->segname, "__LINKEDIT"))
                    continue; /* This usually is discarded or not loaded at all. */
                if (cSegs >= RT_ELEMENTS(aSegs))
                    return VERR_BUFFER_OVERFLOW;
                aSegs[cSegs].Address = uLCmd.pSeg32->vmaddr;
                aSegs[cSegs].uRva    = uLCmd.pSeg32->vmaddr - uModAddr;
                aSegs[cSegs].cb      = uLCmd.pSeg32->vmsize;
                aSegs[cSegs].fFlags  = uLCmd.pSeg32->flags; /* Abusing the flags field here... */
                aSegs[cSegs].iSeg    = cSegs;
                AssertCompile(RTDBG_SEGMENT_NAME_LENGTH > sizeof(uLCmd.pSeg32->segname));
                strcpy(aSegs[cSegs].szName, uLCmd.pSeg32->segname);
                cSegs++;
                break;

            case LC_SEGMENT_64:
                if (cbCmd != sizeof(segment_command_64_t) + uLCmd.pSeg64->nsects * sizeof(section_64_t))
                    return VERR_BAD_EXE_FORMAT;
                if (!dbgDiggerDarwinIsValidSegOrSectName(uLCmd.pSeg64->segname, sizeof(uLCmd.pSeg64->segname)))
                    return VERR_INVALID_NAME;
                if (!strcmp(uLCmd.pSeg64->segname, "__LINKEDIT"))
                    continue; /* This usually is discarded or not loaded at all. */
                if (cSegs >= RT_ELEMENTS(aSegs))
                    return VERR_BUFFER_OVERFLOW;
                aSegs[cSegs].Address = uLCmd.pSeg64->vmaddr;
                aSegs[cSegs].uRva    = uLCmd.pSeg64->vmaddr - uModAddr;
                aSegs[cSegs].cb      = uLCmd.pSeg64->vmsize;
                aSegs[cSegs].fFlags  = uLCmd.pSeg64->flags; /* Abusing the flags field here... */
                aSegs[cSegs].iSeg    = cSegs;
                AssertCompile(RTDBG_SEGMENT_NAME_LENGTH > sizeof(uLCmd.pSeg64->segname));
                strcpy(aSegs[cSegs].szName, uLCmd.pSeg64->segname);
                cSegs++;
                break;

            case LC_UUID:
                if (cbCmd != sizeof(uuid_command_t))
                    return VERR_BAD_EXE_FORMAT;
                if (RTUuidIsNull((PCRTUUID)&uLCmd.pUuid->uuid[0]))
                    return VERR_BAD_EXE_FORMAT;
                memcpy(&Uuid, &uLCmd.pUuid->uuid[0], sizeof(uLCmd.pUuid->uuid));
                break;

            default:
                /* Current known max plus a lot of slack. */
                if (uLCmd.pGenric->cmd > LC_DYLIB_CODE_SIGN_DRS + 32)
                    return VERR_BAD_EXE_FORMAT;
                break;
        }

        /* next */
        cbLeft   -= cbCmd;
        uLCmd.pb += cbCmd;
    }

    if (cbLeft != 0)
        return VERR_BAD_EXE_FORMAT;

    /*
     * Some post processing checks.
     */
    uint32_t iSeg;
    for (iSeg = 0; iSeg < cSegs; iSeg++)
        if (aSegs[iSeg].Address == uModAddr)
            break;
    if (iSeg >= cSegs)
        return VERR_ADDRESS_CONFLICT;

    /*
     * Create a debug module.
     */
    RTDBGMOD hMod;
    rc = RTDbgModCreateFromMachOImage(&hMod, pszName, NULL, f64Bit ? RTLDRARCH_AMD64 : RTLDRARCH_X86_32, 0 /*cbImage*/,
                                      cSegs, aSegs, &Uuid, DBGFR3AsGetConfig(pUVM), RTDBGMOD_F_NOT_DEFERRED);

    if (RT_FAILURE(rc))
    {
        /*
         * Final fallback is a container module.
         */
        rc = RTDbgModCreate(&hMod, pszName, 0, 0);
        if (RT_FAILURE(rc))
            return rc;

        uint64_t uRvaNext = 0;
        for (iSeg = 0; iSeg < cSegs && RT_SUCCESS(rc); iSeg++)
        {
            if (   aSegs[iSeg].uRva > uRvaNext
                && aSegs[iSeg].uRva - uRvaNext < _1M)
                uRvaNext = aSegs[iSeg].uRva;
            rc = RTDbgModSegmentAdd(hMod, aSegs[iSeg].uRva, aSegs[iSeg].cb, aSegs[iSeg].szName, 0, NULL);
            if (aSegs[iSeg].cb > 0 && RT_SUCCESS(rc))
            {
                char szTmp[RTDBG_SEGMENT_NAME_LENGTH + sizeof("_start")];
                strcat(strcpy(szTmp, aSegs[iSeg].szName), "_start");
                rc = RTDbgModSymbolAdd(hMod, szTmp, iSeg, 0 /*uRva*/, 0 /*cb*/, 0 /*fFlags*/, NULL);
            }
            uRvaNext += aSegs[iSeg].cb;
        }

        if (RT_FAILURE(rc))
        {
            RTDbgModRelease(hMod);
            return rc;
        }
    }

    /* Tag the module. */
    rc = RTDbgModSetTag(hMod, DIG_DARWIN_MOD_TAG);
    AssertRC(rc);

    /*
     * Link the module.
     */
    RTDBGAS hAs = DBGFR3AsResolveAndRetain(pUVM, DBGF_AS_KERNEL);
    if (hAs != NIL_RTDBGAS)
    {
        uint64_t uRvaNext = 0;
        uint32_t cLinked  = 0;
        for (iSeg = 0; iSeg < cSegs; iSeg++)
            if (aSegs[iSeg].cb)
            {
                int rc2 = RTDbgAsModuleLinkSeg(hAs, hMod, iSeg, aSegs[iSeg].Address, RTDBGASLINK_FLAGS_REPLACE /*fFlags*/);
                if (RT_SUCCESS(rc2))
                    cLinked++;
                else if (RT_SUCCESS(rc))
                    rc = rc2;
            }
        if (RT_FAILURE(rc) && cLinked != 0)
            rc = -rc;
    }
    else
        rc = VERR_INTERNAL_ERROR;
    RTDbgModRelease(hMod);
    RTDbgAsRelease(hAs);

    return rc;
}

/**
 * @copydoc DBGFOSREG::pfnInit
 */
static DECLCALLBACK(int)  dbgDiggerDarwinInit(PUVM pUVM, void *pvData)
{
    PDBGDIGGERDARWIN pThis = (PDBGDIGGERDARWIN)pvData;
    Assert(!pThis->fValid);

    /*
     * Add the kernel module (and later the other kernel modules we can find).
     */
    int rc = dbgDiggerDarwinAddModule(pThis, pUVM, pThis->AddrKernel.FlatPtr, "mach_kernel");
    if (RT_SUCCESS(rc))
    {
        /** @todo  */
        pThis->fValid = true;
        return VINF_SUCCESS;
    }

    return rc;
}


/**
 * @copydoc DBGFOSREG::pfnProbe
 */
static DECLCALLBACK(bool)  dbgDiggerDarwinProbe(PUVM pUVM, void *pvData)
{
    PDBGDIGGERDARWIN pThis = (PDBGDIGGERDARWIN)pvData;

    /*
     * Look for a section + segment combo that normally only occures in
     * mach_kernel.  Follow it up with probing of the rest of the executable
     * header.  We must search a largish area because the more recent versions
     * of darwin have random load address for security raisins.
     */
    static struct { uint64_t uStart, uEnd; } const s_aRanges[] =
    {
        /* 64-bit: */
        { UINT64_C(0xffffff8000000000), UINT64_C(0xffffff81ffffffff), },

        /* 32-bit - always search for this because of the hybrid 32-bit kernel
           with cpu in long mode that darwin used for a number of versions. */
        { UINT64_C(0x00001000), UINT64_C(0x0ffff000), }
    };
    for (unsigned iRange = DBGFR3CpuGetMode(pUVM, 0 /*idCpu*/) != CPUMMODE_LONG;
          iRange < RT_ELEMENTS(s_aRanges);
          iRange++)
    {
        DBGFADDRESS     KernelAddr;
        for (DBGFR3AddrFromFlat(pUVM, &KernelAddr, s_aRanges[iRange].uStart);
             KernelAddr.FlatPtr < s_aRanges[iRange].uEnd;
             KernelAddr.FlatPtr += X86_PAGE_4K_SIZE)
        {
            static const uint8_t s_abNeedle[16 + 16] =
            {
                '_','_','t','e','x','t',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* section_32_t::sectname */
                '_','_','K','L','D',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, /* section_32_t::segname. */
            };

            int rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &KernelAddr, s_aRanges[iRange].uEnd - KernelAddr.FlatPtr,
                                   1, s_abNeedle, sizeof(s_abNeedle), &KernelAddr);
            if (RT_FAILURE(rc))
                break;
            DBGFR3AddrSub(&KernelAddr, KernelAddr.FlatPtr & X86_PAGE_4K_OFFSET_MASK);

            /*
             * Read the first page of the image and check the headers.
             */
            union
            {
                uint8_t             ab[X86_PAGE_4K_SIZE];
                mach_header_64_t    Hdr64;
                mach_header_32_t    Hdr32;
            } uBuf;
            rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &KernelAddr, uBuf.ab, X86_PAGE_4K_SIZE);
            if (RT_FAILURE(rc))
                continue;
            AssertCompileMembersSameSizeAndOffset(mach_header_64_t, magic,   mach_header_32_t, magic);
            if (   uBuf.Hdr64.magic != IMAGE_MACHO64_SIGNATURE
                && uBuf.Hdr32.magic != IMAGE_MACHO32_SIGNATURE)
                continue;
            AssertCompileMembersSameSizeAndOffset(mach_header_64_t, cputype, mach_header_32_t, cputype);
            bool f64Bit = uBuf.Hdr64.magic == IMAGE_MACHO64_SIGNATURE;
            if (uBuf.Hdr32.cputype != (f64Bit ? CPU_TYPE_X86_64 : CPU_TYPE_I386))
                continue;
            AssertCompileMembersSameSizeAndOffset(mach_header_64_t, filetype, mach_header_32_t, filetype);
            if (uBuf.Hdr32.filetype != MH_EXECUTE)
                continue;
            AssertCompileMembersSameSizeAndOffset(mach_header_64_t, ncmds, mach_header_32_t, ncmds);
            if (uBuf.Hdr32.ncmds > 256)
                continue;
            AssertCompileMembersSameSizeAndOffset(mach_header_64_t, sizeofcmds, mach_header_32_t, sizeofcmds);
            if (uBuf.Hdr32.sizeofcmds > X86_PAGE_4K_SIZE - sizeof(mach_header_64_t))
                continue;

            /* Seems good enough for now.

               If the above causes false positives, check the segments and make
               sure there is a kernel version string in the right one. */
            pThis->AddrKernel = KernelAddr;

            /*
             * Finally, find the kernel version string.
             */
            rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &KernelAddr, 32*_1M, 1, RT_STR_TUPLE("Darwin Kernel Version"),
                               &pThis->AddrKernelVersion);
            if (RT_FAILURE(rc))
                DBGFR3AddrFromFlat(pUVM, &pThis->AddrKernelVersion, 0);
            return true;
        }
    }
    return false;
}


/**
 * @copydoc DBGFOSREG::pfnDestruct
 */
static DECLCALLBACK(void)  dbgDiggerDarwinDestruct(PUVM pUVM, void *pvData)
{

}


/**
 * @copydoc DBGFOSREG::pfnConstruct
 */
static DECLCALLBACK(int)  dbgDiggerDarwinConstruct(PUVM pUVM, void *pvData)
{
    return VINF_SUCCESS;
}


const DBGFOSREG g_DBGDiggerDarwin =
{
    /* .u32Magic = */           DBGFOSREG_MAGIC,
    /* .fFlags = */             0,
    /* .cbData = */             sizeof(DBGDIGGERDARWIN),
    /* .szName = */             "Darwin",
    /* .pfnConstruct = */       dbgDiggerDarwinConstruct,
    /* .pfnDestruct = */        dbgDiggerDarwinDestruct,
    /* .pfnProbe = */           dbgDiggerDarwinProbe,
    /* .pfnInit = */            dbgDiggerDarwinInit,
    /* .pfnRefresh = */         dbgDiggerDarwinRefresh,
    /* .pfnTerm = */            dbgDiggerDarwinTerm,
    /* .pfnQueryVersion = */    dbgDiggerDarwinQueryVersion,
    /* .pfnQueryInterface = */  dbgDiggerDarwinQueryInterface,
    /* .u32EndMagic = */        DBGFOSREG_MAGIC
};

