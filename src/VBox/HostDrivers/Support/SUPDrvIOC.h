/* $Revision$ */
/** @file
 * VirtualBox Support Driver - IOCtl definitions.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___SUPDrvIOC_h___
#define ___SUPDrvIOC_h___

/*
 * Basic types.
 */
#include <iprt/stdint.h>

/*
 * IOCtl numbers.
 * We're using the Win32 type of numbers here, thus the macros below.
 * The SUP_IOCTL_FLAG macro is used to separate requests from 32-bit
 * and 64-bit processes.
 */
#ifdef RT_ARCH_AMD64
# define SUP_IOCTL_FLAG     128
#elif defined(RT_ARCH_X86)
# define SUP_IOCTL_FLAG     0
#else
# error "dunno which arch this is!"
#endif

#ifdef RT_OS_WINDOWS
# ifndef CTL_CODE
#  include <Windows.h>
# endif
  /* Automatic buffering, size not encoded. */
# define SUP_CTL_CODE_SIZE(Function, Size)      CTL_CODE(FILE_DEVICE_UNKNOWN, (Function) | SUP_IOCTL_FLAG, METHOD_BUFFERED, FILE_WRITE_ACCESS)
# define SUP_CTL_CODE_BIG(Function)             CTL_CODE(FILE_DEVICE_UNKNOWN, (Function) | SUP_IOCTL_FLAG, METHOD_BUFFERED, FILE_WRITE_ACCESS)
# define SUP_CTL_CODE_FAST(Function)            CTL_CODE(FILE_DEVICE_UNKNOWN, (Function) | SUP_IOCTL_FLAG, METHOD_NEITHER,  FILE_WRITE_ACCESS)
# define SUP_CTL_CODE_NO_SIZE(uIOCtl)           (uIOCtl)

#elif defined(RT_OS_SOLARIS)
  /* No automatic buffering, size limited to 255 bytes. */
# include <sys/ioccom.h>
# define SUP_CTL_CODE_SIZE(Function, Size)      _IOWRN('V', (Function) | SUP_IOCTL_FLAG, sizeof(SUPREQHDR))
# define SUP_CTL_CODE_BIG(Function)             _IOWRN('V', (Function) | SUP_IOCTL_FLAG, sizeof(SUPREQHDR))
# define SUP_CTL_CODE_FAST(Function)            _IO(   'V', (Function) | SUP_IOCTL_FLAG)
# define SUP_CTL_CODE_NO_SIZE(uIOCtl)           (uIOCtl)

#elif defined(RT_OS_OS2)
  /* No automatic buffering, size not encoded. */
# define SUP_CTL_CATEGORY                       0xc0
# define SUP_CTL_CODE_SIZE(Function, Size)      ((unsigned char)(Function))
# define SUP_CTL_CODE_BIG(Function)             ((unsigned char)(Function))
# define SUP_CTL_CATEGORY_FAST                  0xc1
# define SUP_CTL_CODE_FAST(Function)            ((unsigned char)(Function))
# define SUP_CTL_CODE_NO_SIZE(uIOCtl)           (uIOCtl)

#elif defined(RT_OS_LINUX)
  /* No automatic buffering, size limited to 16KB. */
# include <linux/ioctl.h>
# define SUP_CTL_CODE_SIZE(Function, Size)      _IOC(_IOC_READ | _IOC_WRITE, 'V', (Function) | SUP_IOCTL_FLAG, (Size))
# define SUP_CTL_CODE_BIG(Function)             _IO('V', (Function) | SUP_IOCTL_FLAG)
# define SUP_CTL_CODE_FAST(Function)            _IO('V', (Function) | SUP_IOCTL_FLAG)
# define SUP_CTL_CODE_NO_SIZE(uIOCtl)           ((uIOCtl) & ~IOCSIZE_MASK)

#elif defined(RT_OS_L4)
  /* Implemented in suplib, no worries. */
# define SUP_CTL_CODE_SIZE(Function, Size)      (Function)
# define SUP_CTL_CODE_BIG(Function)             (Function)
# define SUP_CTL_CODE_FAST(Function)            (Function)
# define SUP_CTL_CODE_NO_SIZE(uIOCtl)           (uIOCtl)

#else /* BSD Like */
  /* Automatic buffering, size limited to 4KB on *BSD and 8KB on Darwin - commands the limit, 4KB. */
# include <sys/ioccom.h>
# define SUP_CTL_CODE_SIZE(Function, Size)      _IOC(IOC_INOUT, 'V', (Function) | SUP_IOCTL_FLAG, (Size))
# define SUP_CTL_CODE_BIG(Function)             _IO('V', (Function) | SUP_IOCTL_FLAG)
# define SUP_CTL_CODE_FAST(Function)            _IO('V', (Function) | SUP_IOCTL_FLAG)
# define SUP_CTL_CODE_NO_SIZE(uIOCtl)           ( (uIOCtl) & ~_IOC(0,0,0,IOCPARM_MASK) )
#endif

/** Fast path IOCtl: VMMR0_DO_RAW_RUN */
#define SUP_IOCTL_FAST_DO_RAW_RUN               SUP_CTL_CODE_FAST(64)
/** Fast path IOCtl: VMMR0_DO_HWACC_RUN */
#define SUP_IOCTL_FAST_DO_HWACC_RUN             SUP_CTL_CODE_FAST(65)
/** Just a NOP call for profiling the latency of a fast ioctl call to VMMR0. */
#define SUP_IOCTL_FAST_DO_NOP                   SUP_CTL_CODE_FAST(66)



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#ifdef RT_ARCH_AMD64
# pragma pack(8)                        /* paranoia. */
#else
# pragma pack(4)                        /* paranoia. */
#endif


/**
 * Common In/Out header.
 */
typedef struct SUPREQHDR
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** The size of the input. */
    uint32_t        cbIn;
    /** The size of the output. */
    uint32_t        cbOut;
    /** Flags. See SUPREQHDR_FLAGS_* for details and values. */
    uint32_t        fFlags;
    /** The VBox status code of the operation, out direction only. */
    int32_t         rc;
} SUPREQHDR;
/** Pointer to a IOC header. */
typedef SUPREQHDR *PSUPREQHDR;

/** @name SUPREQHDR::fFlags values
 * @{  */
/** Masks out the magic value.  */
#define SUPREQHDR_FLAGS_MAGIC_MASK                      UINT32_C(0xff0000ff)
/** The generic mask. */
#define SUPREQHDR_FLAGS_GEN_MASK                        UINT32_C(0x0000ff00)
/** The request specific mask. */
#define SUPREQHDR_FLAGS_REQ_MASK                        UINT32_C(0x00ff0000)

/** There is extra input that needs copying on some platforms. */
#define SUPREQHDR_FLAGS_EXTRA_IN                        UINT32_C(0x00000100)
/** There is extra output that needs copying on some platforms. */
#define SUPREQHDR_FLAGS_EXTRA_OUT                       UINT32_C(0x00000200)

/** The magic value. */
#define SUPREQHDR_FLAGS_MAGIC                           UINT32_C(0x42000042)
/** The default value. Use this when no special stuff is requested. */
#define SUPREQHDR_FLAGS_DEFAULT                         SUPREQHDR_FLAGS_MAGIC
/** @} */


/** @name SUP_IOCTL_COOKIE
 * @{
 */
/** Negotiate cookie. */
#define SUP_IOCTL_COOKIE                                SUP_CTL_CODE_SIZE(1, SUP_IOCTL_COOKIE_SIZE)
/** The request size. */
#define SUP_IOCTL_COOKIE_SIZE                           sizeof(SUPCOOKIE)
/** The SUPREQHDR::cbIn value. */
#define SUP_IOCTL_COOKIE_SIZE_IN                        sizeof(SUPREQHDR) + RT_SIZEOFMEMB(SUPCOOKIE, u.In)
/** The SUPREQHDR::cbOut value. */
#define SUP_IOCTL_COOKIE_SIZE_OUT                       sizeof(SUPREQHDR) + RT_SIZEOFMEMB(SUPCOOKIE, u.Out)
/** SUPCOOKIE_IN magic word. */
#define SUPCOOKIE_MAGIC                                 "The Magic Word!"
/** The initial cookie. */
#define SUPCOOKIE_INITIAL_COOKIE                        0x69726f74 /* 'tori' */

/** Current interface version.
 * The upper 16-bit is the major version, the the lower the minor version.
 * When incompatible changes are made, the upper major number has to be changed.
 *
 * @todo Pending work on next major version change:
 *          - Eliminate supdrvPageWasLockedByPageAlloc and supdrvPageGetPhys.
 *          - Remove SUPR0PageAlloc in favor of SUPR0PageAllocEx, removing
 *            and renaming the related IOCtls too.
 */
#define SUPDRV_IOC_VERSION                              0x000c0001

/** SUP_IOCTL_COOKIE. */
typedef struct SUPCOOKIE
{
    /** The header.
     * u32Cookie must be set to SUPCOOKIE_INITIAL_COOKIE.
     * u32SessionCookie should be set to some random value. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Magic word. */
            char            szMagic[16];
            /** The requested interface version number. */
            uint32_t        u32ReqVersion;
            /** The minimum interface version number. */
            uint32_t        u32MinVersion;
        } In;
        struct
        {
            /** Cookie. */
            uint32_t        u32Cookie;
            /** Session cookie. */
            uint32_t        u32SessionCookie;
            /** Interface version for this session. */
            uint32_t        u32SessionVersion;
            /** The actual interface version in the driver. */
            uint32_t        u32DriverVersion;
            /** Number of functions available for the SUP_IOCTL_QUERY_FUNCS request. */
            uint32_t        cFunctions;
            /** Session handle. */
            R0PTRTYPE(PSUPDRVSESSION)   pSession;
        } Out;
    } u;
} SUPCOOKIE, *PSUPCOOKIE;
/** @} */


/** @name SUP_IOCTL_QUERY_FUNCS
 * Query SUPR0 functions.
 * @{
 */
#define SUP_IOCTL_QUERY_FUNCS(cFuncs)                   SUP_CTL_CODE_BIG(2)
#define SUP_IOCTL_QUERY_FUNCS_SIZE(cFuncs)              RT_UOFFSETOF(SUPQUERYFUNCS, u.Out.aFunctions[(cFuncs)])
#define SUP_IOCTL_QUERY_FUNCS_SIZE_IN                   sizeof(SUPREQHDR)
#define SUP_IOCTL_QUERY_FUNCS_SIZE_OUT(cFuncs)          SUP_IOCTL_QUERY_FUNCS_SIZE(cFuncs)

/** A function. */
typedef struct SUPFUNC
{
    /** Name - mangled. */
    char            szName[32];
    /** Address. */
    RTR0PTR         pfn;
} SUPFUNC, *PSUPFUNC;

typedef struct SUPQUERYFUNCS
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Number of functions returned. */
            uint32_t        cFunctions;
            /** Array of functions. */
            SUPFUNC         aFunctions[1];
        } Out;
    } u;
} SUPQUERYFUNCS, *PSUPQUERYFUNCS;
/** @} */


/** @name SUP_IOCTL_IDT_INSTALL
 * Install IDT patch for calling processor.
 * @{
 */
#define SUP_IOCTL_IDT_INSTALL                           SUP_CTL_CODE_SIZE(3, SUP_IOCTL_IDT_INSTALL_SIZE)
#define SUP_IOCTL_IDT_INSTALL_SIZE                      sizeof(SUPIDTINSTALL)
#define SUP_IOCTL_IDT_INSTALL_SIZE_IN                   sizeof(SUPREQHDR)
#define SUP_IOCTL_IDT_INSTALL_SIZE_OUT                  sizeof(SUPIDTINSTALL)
typedef struct SUPIDTINSTALL
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The IDT entry number. */
            uint8_t         u8Idt;
        } Out;
    } u;
} SUPIDTINSTALL, *PSUPIDTINSTALL;
/** @} */


/** @name SUP_IOCTL_IDT_REMOVE
 * Remove IDT patch for calling processor.
 * @{
 */
#define SUP_IOCTL_IDT_REMOVE                            SUP_CTL_CODE_SIZE(4, SUP_IOCTL_IDT_REMOVE_SIZE)
#define SUP_IOCTL_IDT_REMOVE_SIZE                       sizeof(SUPIDTREMOVE)
#define SUP_IOCTL_IDT_REMOVE_SIZE_IN                    sizeof(SUPIDTREMOVE)
#define SUP_IOCTL_IDT_REMOVE_SIZE_OUT                   sizeof(SUPIDTREMOVE)
typedef struct SUPIDTREMOVE
{
    /** The header. */
    SUPREQHDR               Hdr;
} SUPIDTREMOVE, *PSUPIDTREMOVE;
/** @}*/


/** @name SUP_IOCTL_LDR_OPEN
 * Open an image.
 * @{
 */
#define SUP_IOCTL_LDR_OPEN                              SUP_CTL_CODE_SIZE(5, SUP_IOCTL_LDR_OPEN_SIZE)
#define SUP_IOCTL_LDR_OPEN_SIZE                         sizeof(SUPLDROPEN)
#define SUP_IOCTL_LDR_OPEN_SIZE_IN                      sizeof(SUPLDROPEN)
#define SUP_IOCTL_LDR_OPEN_SIZE_OUT                     (sizeof(SUPREQHDR) + RT_SIZEOFMEMB(SUPLDROPEN, u.Out))
typedef struct SUPLDROPEN
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Size of the image we'll be loading. */
            uint32_t        cbImage;
            /** Image name.
             * This is the NAME of the image, not the file name. It is used
             * to share code with other processes. (Max len is 32 chars!)  */
            char            szName[32];
        } In;
        struct
        {
            /** The base address of the image. */
            RTR0PTR         pvImageBase;
            /** Indicate whether or not the image requires loading. */
            bool            fNeedsLoading;
        } Out;
    } u;
} SUPLDROPEN, *PSUPLDROPEN;
/** @} */


/** @name SUP_IOCTL_LDR_LOAD
 * Upload the image bits.
 * @{
 */
#define SUP_IOCTL_LDR_LOAD                              SUP_CTL_CODE_BIG(6)
#define SUP_IOCTL_LDR_LOAD_SIZE(cbImage)                RT_UOFFSETOF(SUPLDRLOAD, u.In.achImage[cbImage])
#define SUP_IOCTL_LDR_LOAD_SIZE_IN(cbImage)             RT_UOFFSETOF(SUPLDRLOAD, u.In.achImage[cbImage])
#define SUP_IOCTL_LDR_LOAD_SIZE_OUT                     sizeof(SUPREQHDR)

/**
 * Module initialization callback function.
 * This is called once after the module has been loaded.
 *
 * @returns 0 on success.
 * @returns Appropriate error code on failure.
 */
typedef DECLCALLBACK(int) FNR0MODULEINIT(void);
/** Pointer to a FNR0MODULEINIT(). */
typedef R0PTRTYPE(FNR0MODULEINIT *) PFNR0MODULEINIT;

/**
 * Module termination callback function.
 * This is called once right before the module is being unloaded.
 */
typedef DECLCALLBACK(void) FNR0MODULETERM(void);
/** Pointer to a FNR0MODULETERM(). */
typedef R0PTRTYPE(FNR0MODULETERM *) PFNR0MODULETERM;

/**
 * Symbol table entry.
 */
typedef struct SUPLDRSYM
{
    /** Offset into of the string table. */
    uint32_t        offName;
    /** Offset of the symbol relative to the image load address. */
    uint32_t        offSymbol;
} SUPLDRSYM;
/** Pointer to a symbol table entry. */
typedef SUPLDRSYM *PSUPLDRSYM;
/** Pointer to a const symbol table entry. */
typedef SUPLDRSYM const *PCSUPLDRSYM;

/**
 * SUPLDRLOAD::u::In::EP type.
 */
typedef enum SUPLDRLOADEP
{
    SUPLDRLOADEP_NOTHING = 0,
    SUPLDRLOADEP_VMMR0,
    SUPLDRLOADEP_SERVICE,
    SUPLDRLOADEP_32BIT_HACK = 0x7fffffff
} SUPLDRLOADEP;

typedef struct SUPLDRLOAD
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The address of module initialization function. Similar to _DLL_InitTerm(hmod, 0). */
            PFNR0MODULEINIT pfnModuleInit;
            /** The address of module termination function. Similar to _DLL_InitTerm(hmod, 1). */
            PFNR0MODULETERM pfnModuleTerm;
            /** Special entry points. */
            union
            {
                /** SUPLDRLOADEP_VMMR0. */
                struct
                {
                    /** The module handle (i.e. address). */
                    RTR0PTR                 pvVMMR0;
                    /** Address of VMMR0EntryInt function. */
                    RTR0PTR                 pvVMMR0EntryInt;
                    /** Address of VMMR0EntryFast function. */
                    RTR0PTR                 pvVMMR0EntryFast;
                    /** Address of VMMR0EntryEx function. */
                    RTR0PTR                 pvVMMR0EntryEx;
                } VMMR0;
                /** SUPLDRLOADEP_SERVICE. */
                struct
                {
                    /** The service request handler.
                     * (PFNR0SERVICEREQHANDLER isn't defined yet.) */
                    RTR0PTR                 pfnServiceReq;
                    /** Reserved, must be NIL. */
                    RTR0PTR                 apvReserved[3];
                } Service;
            }               EP;
            /** Address. */
            RTR0PTR         pvImageBase;
            /** Entry point type. */
            SUPLDRLOADEP    eEPType;
            /** The offset of the symbol table. */
            uint32_t        offSymbols;
            /** The number of entries in the symbol table. */
            uint32_t        cSymbols;
            /** The offset of the string table. */
            uint32_t        offStrTab;
            /** Size of the string table. */
            uint32_t        cbStrTab;
            /** Size of image (including string and symbol tables). */
            uint32_t        cbImage;
            /** The image data. */
            char            achImage[1];
        } In;
    } u;
} SUPLDRLOAD, *PSUPLDRLOAD;
/** @} */


/** @name SUP_IOCTL_LDR_FREE
 * Free an image.
 * @{
 */
#define SUP_IOCTL_LDR_FREE                              SUP_CTL_CODE_SIZE(7, SUP_IOCTL_LDR_FREE_SIZE)
#define SUP_IOCTL_LDR_FREE_SIZE                         sizeof(SUPLDRFREE)
#define SUP_IOCTL_LDR_FREE_SIZE_IN                      sizeof(SUPLDRFREE)
#define SUP_IOCTL_LDR_FREE_SIZE_OUT                     sizeof(SUPREQHDR)
typedef struct SUPLDRFREE
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Address. */
            RTR0PTR         pvImageBase;
        } In;
    } u;
} SUPLDRFREE, *PSUPLDRFREE;
/** @} */


/** @name SUP_IOCTL_LDR_GET_SYMBOL
 * Get address of a symbol within an image.
 * @{
 */
#define SUP_IOCTL_LDR_GET_SYMBOL                        SUP_CTL_CODE_SIZE(8, SUP_IOCTL_LDR_GET_SYMBOL_SIZE)
#define SUP_IOCTL_LDR_GET_SYMBOL_SIZE                   sizeof(SUPLDRGETSYMBOL)
#define SUP_IOCTL_LDR_GET_SYMBOL_SIZE_IN                sizeof(SUPLDRGETSYMBOL)
#define SUP_IOCTL_LDR_GET_SYMBOL_SIZE_OUT               (sizeof(SUPREQHDR) + RT_SIZEOFMEMB(SUPLDRGETSYMBOL, u.Out))
typedef struct SUPLDRGETSYMBOL
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Address. */
            RTR0PTR         pvImageBase;
            /** The symbol name. */
            char            szSymbol[64];
        } In;
        struct
        {
            /** The symbol address. */
            RTR0PTR         pvSymbol;
        } Out;
    } u;
} SUPLDRGETSYMBOL, *PSUPLDRGETSYMBOL;
/** @} */


/** @name SUP_IOCTL_CALL_VMMR0
 * Call the R0 VMM Entry point.
 *
 * @todo Might have to convert this to a big request...
 * @{
 */
#define SUP_IOCTL_CALL_VMMR0(cbReq)                     SUP_CTL_CODE_SIZE(9, SUP_IOCTL_CALL_VMMR0_SIZE(cbReq))
#define SUP_IOCTL_CALL_VMMR0_SIZE(cbReq)                RT_UOFFSETOF(SUPCALLVMMR0, abReqPkt[cbReq])
#define SUP_IOCTL_CALL_VMMR0_SIZE_IN(cbReq)             SUP_IOCTL_CALL_VMMR0_SIZE(cbReq)
#define SUP_IOCTL_CALL_VMMR0_SIZE_OUT(cbReq)            SUP_IOCTL_CALL_VMMR0_SIZE(cbReq)
typedef struct SUPCALLVMMR0
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The VM handle. */
            PVMR0           pVMR0;
            /** VCPU id. */
            uint32_t        idCpu;
            /** Which operation to execute. */
            uint32_t        uOperation;
            /** Argument to use when no request packet is supplied. */
            uint64_t        u64Arg;
        } In;
    } u;
    /** The VMMR0Entry request packet. */
    uint8_t                 abReqPkt[1];
} SUPCALLVMMR0, *PSUPCALLVMMR0;
/** @} */


/** @name SUP_IOCTL_LOW_ALLOC
 * Allocate memory below 4GB (physically).
 * @{
 */
#define SUP_IOCTL_LOW_ALLOC                             SUP_CTL_CODE_BIG(10)
#define SUP_IOCTL_LOW_ALLOC_SIZE(cPages)                ((uint32_t)RT_UOFFSETOF(SUPLOWALLOC, u.Out.aPages[cPages]))
#define SUP_IOCTL_LOW_ALLOC_SIZE_IN                     (sizeof(SUPREQHDR) + RT_SIZEOFMEMB(SUPLOWALLOC, u.In))
#define SUP_IOCTL_LOW_ALLOC_SIZE_OUT(cPages)            SUP_IOCTL_LOW_ALLOC_SIZE(cPages)
typedef struct SUPLOWALLOC
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Number of pages to allocate. */
            uint32_t        cPages;
        } In;
        struct
        {
            /** The ring-3 address of the allocated memory. */
            RTR3PTR         pvR3;
            /** The ring-0 address of the allocated memory. */
            RTR0PTR         pvR0;
            /** Array of pages. */
            RTHCPHYS        aPages[1];
        } Out;
    } u;
} SUPLOWALLOC, *PSUPLOWALLOC;
/** @} */


/** @name SUP_IOCTL_LOW_FREE
 * Free low memory.
 * @{
 */
#define SUP_IOCTL_LOW_FREE                              SUP_CTL_CODE_SIZE(11, SUP_IOCTL_LOW_FREE_SIZE)
#define SUP_IOCTL_LOW_FREE_SIZE                         sizeof(SUPLOWFREE)
#define SUP_IOCTL_LOW_FREE_SIZE_IN                      sizeof(SUPLOWFREE)
#define SUP_IOCTL_LOW_FREE_SIZE_OUT                     sizeof(SUPREQHDR)
typedef struct SUPLOWFREE
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The ring-3 address of the memory to free. */
            RTR3PTR         pvR3;
        } In;
    } u;
} SUPLOWFREE, *PSUPLOWFREE;
/** @} */


/** @name SUP_IOCTL_PAGE_ALLOC
 * Allocate memory and map into the user process.
 * The memory is of course locked.
 * @{
 */
#define SUP_IOCTL_PAGE_ALLOC                            SUP_CTL_CODE_BIG(12)
#define SUP_IOCTL_PAGE_ALLOC_SIZE(cPages)               RT_UOFFSETOF(SUPPAGEALLOC, u.Out.aPages[cPages])
#define SUP_IOCTL_PAGE_ALLOC_SIZE_IN                    (sizeof(SUPREQHDR) + RT_SIZEOFMEMB(SUPPAGEALLOC, u.In))
#define SUP_IOCTL_PAGE_ALLOC_SIZE_OUT(cPages)           SUP_IOCTL_PAGE_ALLOC_SIZE(cPages)
typedef struct SUPPAGEALLOC
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Number of pages to allocate */
            uint32_t        cPages;
        } In;
        struct
        {
            /** Returned ring-3 address. */
            RTR3PTR         pvR3;
            /** The physical addresses of the allocated pages. */
            RTHCPHYS        aPages[1];
        } Out;
    } u;
} SUPPAGEALLOC, *PSUPPAGEALLOC;
/** @} */


/** @name SUP_IOCTL_PAGE_FREE
 * Free memory allocated with SUP_IOCTL_PAGE_ALLOC or SUP_IOCTL_PAGE_ALLOC_EX.
 * @{
 */
#define SUP_IOCTL_PAGE_FREE                             SUP_CTL_CODE_SIZE(13, SUP_IOCTL_PAGE_FREE_SIZE_IN)
#define SUP_IOCTL_PAGE_FREE_SIZE                        sizeof(SUPPAGEFREE)
#define SUP_IOCTL_PAGE_FREE_SIZE_IN                     sizeof(SUPPAGEFREE)
#define SUP_IOCTL_PAGE_FREE_SIZE_OUT                    sizeof(SUPREQHDR)
typedef struct SUPPAGEFREE
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Address of memory range to free. */
            RTR3PTR         pvR3;
        } In;
    } u;
} SUPPAGEFREE, *PSUPPAGEFREE;
/** @} */


/** @name SUP_IOCTL_PAGE_LOCK
 * Pin down physical pages.
 * @{
 */
#define SUP_IOCTL_PAGE_LOCK                             SUP_CTL_CODE_BIG(14)
#define SUP_IOCTL_PAGE_LOCK_SIZE(cPages)                (RT_MAX((size_t)SUP_IOCTL_PAGE_LOCK_SIZE_IN, (size_t)SUP_IOCTL_PAGE_LOCK_SIZE_OUT(cPages)))
#define SUP_IOCTL_PAGE_LOCK_SIZE_IN                     (sizeof(SUPREQHDR) + RT_SIZEOFMEMB(SUPPAGELOCK, u.In))
#define SUP_IOCTL_PAGE_LOCK_SIZE_OUT(cPages)            RT_UOFFSETOF(SUPPAGELOCK, u.Out.aPages[cPages])
typedef struct SUPPAGELOCK
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Start of page range. Must be PAGE aligned. */
            RTR3PTR         pvR3;
            /** The range size given as a page count. */
            uint32_t        cPages;
        } In;

        struct
        {
            /** Array of pages. */
            RTHCPHYS        aPages[1];
        } Out;
    } u;
} SUPPAGELOCK, *PSUPPAGELOCK;
/** @} */


/** @name SUP_IOCTL_PAGE_UNLOCK
 * Unpin physical pages.
 * @{ */
#define SUP_IOCTL_PAGE_UNLOCK                           SUP_CTL_CODE_SIZE(15, SUP_IOCTL_PAGE_UNLOCK_SIZE)
#define SUP_IOCTL_PAGE_UNLOCK_SIZE                      sizeof(SUPPAGEUNLOCK)
#define SUP_IOCTL_PAGE_UNLOCK_SIZE_IN                   sizeof(SUPPAGEUNLOCK)
#define SUP_IOCTL_PAGE_UNLOCK_SIZE_OUT                  sizeof(SUPREQHDR)
typedef struct SUPPAGEUNLOCK
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Start of page range of a range previuosly pinned. */
            RTR3PTR         pvR3;
        } In;
    } u;
} SUPPAGEUNLOCK, *PSUPPAGEUNLOCK;
/** @} */


/** @name SUP_IOCTL_CONT_ALLOC
 * Allocate contious memory.
 * @{
 */
#define SUP_IOCTL_CONT_ALLOC                            SUP_CTL_CODE_SIZE(16, SUP_IOCTL_CONT_ALLOC_SIZE)
#define SUP_IOCTL_CONT_ALLOC_SIZE                       sizeof(SUPCONTALLOC)
#define SUP_IOCTL_CONT_ALLOC_SIZE_IN                    (sizeof(SUPREQHDR) + RT_SIZEOFMEMB(SUPCONTALLOC, u.In))
#define SUP_IOCTL_CONT_ALLOC_SIZE_OUT                   sizeof(SUPCONTALLOC)
typedef struct SUPCONTALLOC
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The allocation size given as a page count. */
            uint32_t        cPages;
        } In;

        struct
        {
            /** The address of the ring-0 mapping of the allocated memory. */
            RTR0PTR         pvR0;
            /** The address of the ring-3 mapping of the allocated memory. */
            RTR3PTR         pvR3;
            /** The physical address of the allocation. */
            RTHCPHYS        HCPhys;
        } Out;
    } u;
} SUPCONTALLOC, *PSUPCONTALLOC;
/** @} */


/** @name SUP_IOCTL_CONT_FREE Input.
 * @{
 */
/** Free contious memory. */
#define SUP_IOCTL_CONT_FREE                             SUP_CTL_CODE_SIZE(17, SUP_IOCTL_CONT_FREE_SIZE)
#define SUP_IOCTL_CONT_FREE_SIZE                        sizeof(SUPCONTFREE)
#define SUP_IOCTL_CONT_FREE_SIZE_IN                     sizeof(SUPCONTFREE)
#define SUP_IOCTL_CONT_FREE_SIZE_OUT                    sizeof(SUPREQHDR)
typedef struct SUPCONTFREE
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The ring-3 address of the memory to free. */
            RTR3PTR         pvR3;
        } In;
    } u;
} SUPCONTFREE, *PSUPCONTFREE;
/** @} */


/** @name SUP_IOCTL_GET_PAGING_MODE
 * Get the host paging mode.
 * @{
 */
#define SUP_IOCTL_GET_PAGING_MODE                       SUP_CTL_CODE_SIZE(18, SUP_IOCTL_GET_PAGING_MODE_SIZE)
#define SUP_IOCTL_GET_PAGING_MODE_SIZE                  sizeof(SUPGETPAGINGMODE)
#define SUP_IOCTL_GET_PAGING_MODE_SIZE_IN               sizeof(SUPREQHDR)
#define SUP_IOCTL_GET_PAGING_MODE_SIZE_OUT              sizeof(SUPGETPAGINGMODE)
typedef struct SUPGETPAGINGMODE
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The paging mode. */
            SUPPAGINGMODE   enmMode;
        } Out;
    } u;
} SUPGETPAGINGMODE, *PSUPGETPAGINGMODE;
/** @} */


/** @name SUP_IOCTL_SET_VM_FOR_FAST
 * Set the VM handle for doing fast call ioctl calls.
 * @{
 */
#define SUP_IOCTL_SET_VM_FOR_FAST                       SUP_CTL_CODE_SIZE(19, SUP_IOCTL_SET_VM_FOR_FAST_SIZE)
#define SUP_IOCTL_SET_VM_FOR_FAST_SIZE                  sizeof(SUPSETVMFORFAST)
#define SUP_IOCTL_SET_VM_FOR_FAST_SIZE_IN               sizeof(SUPSETVMFORFAST)
#define SUP_IOCTL_SET_VM_FOR_FAST_SIZE_OUT              sizeof(SUPREQHDR)
typedef struct SUPSETVMFORFAST
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The ring-0 VM handle (pointer). */
            PVMR0           pVMR0;
        } In;
    } u;
} SUPSETVMFORFAST, *PSUPSETVMFORFAST;
/** @} */


/** @name SUP_IOCTL_GIP_MAP
 * Map the GIP into user space.
 * @{
 */
#define SUP_IOCTL_GIP_MAP                               SUP_CTL_CODE_SIZE(20, SUP_IOCTL_GIP_MAP_SIZE)
#define SUP_IOCTL_GIP_MAP_SIZE                          sizeof(SUPGIPMAP)
#define SUP_IOCTL_GIP_MAP_SIZE_IN                       sizeof(SUPREQHDR)
#define SUP_IOCTL_GIP_MAP_SIZE_OUT                      sizeof(SUPGIPMAP)
typedef struct SUPGIPMAP
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The physical address of the GIP. */
            RTHCPHYS        HCPhysGip;
            /** Pointer to the read-only usermode GIP mapping for this session. */
            R3PTRTYPE(PSUPGLOBALINFOPAGE)   pGipR3;
            /** Pointer to the supervisor mode GIP mapping. */
            R0PTRTYPE(PSUPGLOBALINFOPAGE)   pGipR0;
        } Out;
    } u;
} SUPGIPMAP, *PSUPGIPMAP;
/** @} */


/** @name SUP_IOCTL_GIP_UNMAP
 * Unmap the GIP.
 * @{
 */
#define SUP_IOCTL_GIP_UNMAP                             SUP_CTL_CODE_SIZE(21, SUP_IOCTL_GIP_UNMAP_SIZE)
#define SUP_IOCTL_GIP_UNMAP_SIZE                        sizeof(SUPGIPUNMAP)
#define SUP_IOCTL_GIP_UNMAP_SIZE_IN                     sizeof(SUPGIPUNMAP)
#define SUP_IOCTL_GIP_UNMAP_SIZE_OUT                    sizeof(SUPGIPUNMAP)
typedef struct SUPGIPUNMAP
{
    /** The header. */
    SUPREQHDR               Hdr;
} SUPGIPUNMAP, *PSUPGIPUNMAP;
/** @} */


/** @name SUP_IOCTL_CALL_SERVICE
 * Call the a ring-0 service.
 *
 * @todo    Might have to convert this to a big request, just like
 *          SUP_IOCTL_CALL_VMMR0
 * @{
 */
#define SUP_IOCTL_CALL_SERVICE(cbReq)                   SUP_CTL_CODE_SIZE(22, SUP_IOCTL_CALL_SERVICE_SIZE(cbReq))
#define SUP_IOCTL_CALL_SERVICE_SIZE(cbReq)              RT_UOFFSETOF(SUPCALLSERVICE, abReqPkt[cbReq])
#define SUP_IOCTL_CALL_SERVICE_SIZE_IN(cbReq)           SUP_IOCTL_CALL_SERVICE_SIZE(cbReq)
#define SUP_IOCTL_CALL_SERVICE_SIZE_OUT(cbReq)          SUP_IOCTL_CALL_SERVICE_SIZE(cbReq)
typedef struct SUPCALLSERVICE
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The service name. */
            char            szName[28];
            /** Which operation to execute. */
            uint32_t        uOperation;
            /** Argument to use when no request packet is supplied. */
            uint64_t        u64Arg;
        } In;
    } u;
    /** The request packet passed to SUP. */
    uint8_t                 abReqPkt[1];
} SUPCALLSERVICE, *PSUPCALLSERVICE;
/** @} */

/** @name SUP_IOCTL_PAGE_ALLOC_EX
 * Allocate memory and map it into kernel and/or user space. The memory is of
 * course locked. This is an extended version of SUP_IOCTL_PAGE_ALLOC and the
 * result should be freed using SUP_IOCTL_PAGE_FREE.
 *
 * @remarks Allocations without a kernel mapping may fail with
 *          VERR_NOT_SUPPORTED on some platforms just like with
 *          SUP_IOCTL_PAGE_ALLOC.
 *
 * @{
 */
#define SUP_IOCTL_PAGE_ALLOC_EX                         SUP_CTL_CODE_BIG(23)
#define SUP_IOCTL_PAGE_ALLOC_EX_SIZE(cPages)            RT_UOFFSETOF(SUPPAGEALLOCEX, u.Out.aPages[cPages])
#define SUP_IOCTL_PAGE_ALLOC_EX_SIZE_IN                 (sizeof(SUPREQHDR) + RT_SIZEOFMEMB(SUPPAGEALLOCEX, u.In))
#define SUP_IOCTL_PAGE_ALLOC_EX_SIZE_OUT(cPages)        SUP_IOCTL_PAGE_ALLOC_EX_SIZE(cPages)
typedef struct SUPPAGEALLOCEX
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Number of pages to allocate */
            uint32_t        cPages;
            /** Whether it should have kernel mapping. */
            bool            fKernelMapping;
            /** Whether it should have a user mapping. */
            bool            fUserMapping;
            /** Reserved. Must be false. */
            bool            fReserved0;
            /** Reserved. Must be false. */
            bool            fReserved1;
        } In;
        struct
        {
            /** Returned ring-3 address. */
            RTR3PTR         pvR3;
            /** Returned ring-0 address. */
            RTR0PTR         pvR0;
            /** The physical addresses of the allocated pages. */
            RTHCPHYS        aPages[1];
        } Out;
    } u;
} SUPPAGEALLOCEX, *PSUPPAGEALLOCEX;
/** @} */


/** @name SUP_IOCTL_PAGE_MAP_KERNEL
 * Maps a portion of memory allocated by SUP_IOCTL_PAGE_ALLOC_EX /
 * SUPR0PageAllocEx into kernel space for use by a device or similar.
 *
 * The mapping will be freed together with the ring-3 mapping when
 * SUP_IOCTL_PAGE_FREE or SUPR0PageFree is called.
 *
 * @remarks Not necessarily supported on all platforms.
 *
 * @{
 */
#define SUP_IOCTL_PAGE_MAP_KERNEL                       SUP_CTL_CODE_SIZE(24, SUP_IOCTL_PAGE_MAP_KERNEL_SIZE)
#define SUP_IOCTL_PAGE_MAP_KERNEL_SIZE                  sizeof(SUPPAGEMAPKERNEL)
#define SUP_IOCTL_PAGE_MAP_KERNEL_SIZE_IN               sizeof(SUPPAGEMAPKERNEL)
#define SUP_IOCTL_PAGE_MAP_KERNEL_SIZE_OUT              sizeof(SUPPAGEMAPKERNEL)
typedef struct SUPPAGEMAPKERNEL
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The pointer of to the previously allocated memory. */
            RTR3PTR         pvR3;
            /** The offset to start mapping from. */
            uint32_t        offSub;
            /** Size of the section to map. */
            uint32_t        cbSub;
            /** Flags reserved for future fun. */
            uint32_t        fFlags;
        } In;
        struct
        {
            /** The ring-0 address corresponding to pvR3 + offSub. */
            RTR0PTR         pvR0;
        } Out;
    } u;
} SUPPAGEMAPKERNEL, *PSUPPAGEMAPKERNEL;
/** @} */


/** @name SUP_IOCTL_LOGGER_SETTINGS
 * Changes the ring-0 release or debug logger settings.
 * @{
 */
#define SUP_IOCTL_LOGGER_SETTINGS(cbStrTab)             SUP_CTL_CODE_SIZE(25, SUP_IOCTL_LOGGER_SETTINGS_SIZE(cbStrTab))
#define SUP_IOCTL_LOGGER_SETTINGS_SIZE(cbStrTab)        RT_UOFFSETOF(SUPLOGGERSETTINGS, u.In.szStrings[cbStrTab])
#define SUP_IOCTL_LOGGER_SETTINGS_SIZE_IN(cbStrTab)     RT_UOFFSETOF(SUPLOGGERSETTINGS, u.In.szStrings[cbStrTab])
#define SUP_IOCTL_LOGGER_SETTINGS_SIZE_OUT              sizeof(SUPREQHDR)
typedef struct SUPLOGGERSETTINGS
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** Which logger. */
            uint32_t        fWhich;
            /** What to do with it. */
            uint32_t        fWhat;
            /** Offset of the flags setting string. */
            uint32_t        offFlags;
            /** Offset of the groups setting string. */
            uint32_t        offGroups;
            /** Offset of the destination setting string. */
            uint32_t        offDestination;
            /** The string table. */
            char            szStrings[1];
        } In;
    } u;
} SUPLOGGERSETTINGS, *PSUPLOGGERSETTINGS;

/** Debug logger. */
#define SUPLOGGERSETTINGS_WHICH_DEBUG       0
/** Release logger. */
#define SUPLOGGERSETTINGS_WHICH_RELEASE     1

/** Change the settings. */
#define SUPLOGGERSETTINGS_WHAT_SETTINGS     0
/** Create the logger instance. */
#define SUPLOGGERSETTINGS_WHAT_CREATE       1
/** Destroy the logger instance. */
#define SUPLOGGERSETTINGS_WHAT_DESTROY      2

/** @} */


/** @name Semaphore Types
 * @{ */
#define SUP_SEM_TYPE_EVENT                  0
#define SUP_SEM_TYPE_EVENT_MULTI            1
/** @} */


/** @name SUP_IOCTL_SEM_CREATE
 * Create a semaphore
 * @{
 */
#define SUP_IOCTL_SEM_CREATE                            SUP_CTL_CODE_SIZE(26, SUP_IOCTL_SEM_CREATE_SIZE)
#define SUP_IOCTL_SEM_CREATE_SIZE                       sizeof(SUPSEMCREATE)
#define SUP_IOCTL_SEM_CREATE_SIZE_IN                    sizeof(SUPSEMCREATE)
#define SUP_IOCTL_SEM_CREATE_SIZE_OUT                   sizeof(SUPSEMCREATE)
typedef struct SUPSEMCREATE
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The semaphore type. */
            uint32_t        uType;
        } In;
        struct
        {
            /** The handle of the created semaphore. */
            uint32_t        hSem;
        } Out;
    } u;
} SUPSEMCREATE, *PSUPSEMCREATE;

/** @} */


/** @name SUP_IOCTL_SEM_OP
 * Semaphore operations.
 * @{
 */
#define SUP_IOCTL_SEM_OP                                SUP_CTL_CODE_SIZE(27, SUP_IOCTL_SEM_OP_SIZE)
#define SUP_IOCTL_SEM_OP_SIZE                           sizeof(SUPSEMOP)
#define SUP_IOCTL_SEM_OP_SIZE_IN                        sizeof(SUPSEMOP)
#define SUP_IOCTL_SEM_OP_SIZE_OUT                       sizeof(SUPREQHDR)
typedef struct SUPSEMOP
{
    /** The header. */
    SUPREQHDR               Hdr;
    union
    {
        struct
        {
            /** The semaphore type. */
            uint32_t        uType;
            /** The semaphore handle. */
            uint32_t        hSem;
            /** The operation. */
            uint32_t        uOp;
            /** The number of milliseconds to wait if it's a wait operation. */
            uint32_t        cMillies;
        } In;
    } u;
} SUPSEMOP, *PSUPSEMOP;

/** Wait for a number of millisecons. */
#define SUPSEMOP_WAIT       0
/** Signal the semaphore. */
#define SUPSEMOP_SIGNAL     1
/** Reset the sempahore (only applicable to SUP_SEM_TYPE_EVENT_MULTI). */
#define SUPSEMOP_RESET      2
/** Close the semaphore handle. */
#define SUPSEMOP_CLOSE      3

/** @} */


#pragma pack()                          /* paranoia */

#endif

