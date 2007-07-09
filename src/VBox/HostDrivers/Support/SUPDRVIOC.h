/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Shared code:
 * IOCtl definitions
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __SUPDRVIOC_h__
#define __SUPDRVIOC_h__

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
#ifdef __AMD64__
# define SUP_IOCTL_FLAG     128
#elif defined(__X86__)
# define SUP_IOCTL_FLAG     0
#else
# error "dunno which arch this is!"
#endif

#ifdef __WIN__
# define SUP_CTL_CODE(Function)         CTL_CODE(FILE_DEVICE_UNKNOWN, (Function) | SUP_IOCTL_FLAG, METHOD_BUFFERED, FILE_WRITE_ACCESS)
# define SUP_CTL_CODE_FAST(Function)    CTL_CODE(FILE_DEVICE_UNKNOWN, (Function) | SUP_IOCTL_FLAG, METHOD_NEITHER, FILE_WRITE_ACCESS)

/** @todo get rid of this duplication of window header #defines! */
# ifndef CTL_CODE
#  define CTL_CODE(DeviceType, Function, Method, Access) \
    ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) )
# endif
# ifndef METHOD_BUFFERED
#  define METHOD_BUFFERED        0
# endif
# ifndef METHOD_NEITHER
#  define METHOD_NEITHER         3
# endif
# ifndef FILE_WRITE_ACCESS
#  define FILE_WRITE_ACCESS      0x0002
# endif
# ifndef FILE_DEVICE_UNKNOWN
#  define FILE_DEVICE_UNKNOWN    0x00000022
# endif

#elif defined(__OS2__)
# define SUP_CTL_CATEGORY               0xc0
# define SUP_CTL_CODE(Function)         ((unsigned char)(Function))
# define SUP_CTL_CATEGORY_FAST          0xc1
# define SUP_CTL_CODE_FAST(Function)    ((unsigned char)(Function))

#elif defined(__LINUX__)
# ifdef __X86__ /** @todo With the next major version change, drop this branch. */
#  define SUP_CTL_CODE(Function) \
    ( (3U << 30) | ((0x22) << 8) | ((Function) | SUP_IOCTL_FLAG) | (sizeof(SUPDRVIOCTLDATA) << 16) )
#  define SUP_CTL_CODE_FAST(Function) \
    ( (3U << 30) | ((0x22) << 8) | ((Function) | SUP_IOCTL_FLAG) | (0 << 16) )
# else
#  include <linux/ioctl.h>
#  if 1 /* figure out when this changed. */
#   define SUP_CTL_CODE(Function)         _IOWR('V', (Function) | SUP_IOCTL_FLAG, SUPDRVIOCTLDATA)
#   define SUP_CTL_CODE_FAST(Function)    _IO(  'V', (Function) | SUP_IOCTL_FLAG)
#  else /* now: _IO_BAD and _IOWR_BAD */
#   define SUP_CTL_CODE(Function)         _IOWR('V', (Function) | SUP_IOCTL_FLAG, sizeof(SUPDRVIOCTLDATA))
#   define SUP_CTL_CODE_FAST(Function)    _IO(  'V', (Function) | SUP_IOCTL_FLAG)
#  endif
# endif

#elif defined(__L4__)
# define SUP_CTL_CODE(Function) \
    ( (3U << 30) | ((0x22) << 8) | ((Function) | SUP_IOCTL_FLAG) | (sizeof(SUPDRVIOCTLDATA) << 16) )
# define SUP_CTL_CODE_FAST(Function) \
    ( (3U << 30) | ((0x22) << 8) | ((Function) | SUP_IOCTL_FLAG) | (0 << 16) )

#else /* BSD */
# include <sys/ioccom.h>
# define SUP_CTL_CODE(Function)         _IOWR('V', (Function) | SUP_IOCTL_FLAG, SUPDRVIOCTLDATA)
# define SUP_CTL_CODE_FAST(Function)    _IO(  'V', (Function) | SUP_IOCTL_FLAG)
#endif


/** Negotiate cookie. */
#define SUP_IOCTL_COOKIE            SUP_CTL_CODE( 1)
/** Query SUPR0 functions. */
#define SUP_IOCTL_QUERY_FUNCS       SUP_CTL_CODE( 2)
/** Install IDT patch for calling processor. */
#define SUP_IOCTL_IDT_INSTALL       SUP_CTL_CODE( 3)
/** Remove IDT patch for calling processor. */
#define SUP_IOCTL_IDT_REMOVE        SUP_CTL_CODE( 4)
/** Pin down physical pages. */
#define SUP_IOCTL_PINPAGES          SUP_CTL_CODE( 5)
/** Unpin physical pages. */
#define SUP_IOCTL_UNPINPAGES        SUP_CTL_CODE( 6)
/** Allocate contious memory. */
#define SUP_IOCTL_CONT_ALLOC        SUP_CTL_CODE( 7)
/** Free contious memory. */
#define SUP_IOCTL_CONT_FREE         SUP_CTL_CODE( 8)
/** Open an image. */
#define SUP_IOCTL_LDR_OPEN          SUP_CTL_CODE( 9)
/** Upload the image bits. */
#define SUP_IOCTL_LDR_LOAD          SUP_CTL_CODE(10)
/** Free an image. */
#define SUP_IOCTL_LDR_FREE          SUP_CTL_CODE(11)
/** Get address of a symbol within an image. */
#define SUP_IOCTL_LDR_GET_SYMBOL    SUP_CTL_CODE(12)
/** Call the R0 VMM Entry point. */
#define SUP_IOCTL_CALL_VMMR0        SUP_CTL_CODE(14)
/** Get the host paging mode. */
#define SUP_IOCTL_GET_PAGING_MODE   SUP_CTL_CODE(15)
/** Allocate memory below 4GB (physically). */
#define SUP_IOCTL_LOW_ALLOC         SUP_CTL_CODE(16)
/** Free low memory. */
#define SUP_IOCTL_LOW_FREE          SUP_CTL_CODE(17)
/** Map the GIP into user space. */
#define SUP_IOCTL_GIP_MAP           SUP_CTL_CODE(18)
/** Unmap the GIP. */
#define SUP_IOCTL_GIP_UNMAP         SUP_CTL_CODE(19)
/** Set the VM handle for doing fast call ioctl calls. */
#define SUP_IOCTL_SET_VM_FOR_FAST   SUP_CTL_CODE(20)

/** Fast path IOCtl: VMMR0_DO_RAW_RUN */
#define SUP_IOCTL_FAST_DO_RAW_RUN   SUP_CTL_CODE_FAST(64)
/** Fast path IOCtl: VMMR0_DO_HWACC_RUN */
#define SUP_IOCTL_FAST_DO_HWACC_RUN SUP_CTL_CODE_FAST(65)
/** Just a NOP call for profiling the latency of a fast ioctl call to VMMR0. */
#define SUP_IOCTL_FAST_DO_NOP       SUP_CTL_CODE_FAST(66)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#ifdef __AMD64__
# pragma pack(8)                        /* paranoia. */
#else
# pragma pack(4)                        /* paranoia. */
#endif

#ifndef __WIN__
/**
 * Structure used by OSes with less advanced ioctl interfaces, i.e. most
 * Unix like OSes :-)
 */
typedef struct SUPDRVIOCTLDATA
{
    void 	   *pvIn;
    unsigned long   cbIn;
    void           *pvOut;
    unsigned long   cbOut;
#ifdef __OS2__
    int             rc;
#endif
} SUPDRVIOCTLDATA, *PSUPDRVIOCTLDATA;
#endif


/** SUPCOOKIE_IN magic word. */
#define SUPCOOKIE_MAGIC             "The Magic Word!"
/** Current interface version.
 * The upper 16-bit is the major version, the the lower the minor version.
 * When incompatible changes are made, the upper major number has to be changed. */
#define SUPDRVIOC_VERSION           0x00050000

/** SUP_IOCTL_COOKIE Input. */
typedef struct SUPCOOKIE_IN
{
    /** Magic word. */
    char            szMagic[16];
    /** The requested interface version number. */
    uint32_t        u32ReqVersion;
    /** The minimum interface version number. */
    uint32_t        u32MinVersion;
} SUPCOOKIE_IN, *PSUPCOOKIE_IN;

/** SUP_IOCTL_COOKIE Output. */
typedef struct SUPCOOKIE_OUT
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
} SUPCOOKIE_OUT, *PSUPCOOKIE_OUT;



/** SUP_IOCTL_QUERY_FUNCS Input. */
typedef struct SUPQUERYFUNCS_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
} SUPQUERYFUNCS_IN, *PSUPQUERYFUNCS_IN;

/** Function. */
typedef struct SUPFUNC
{
    /** Name - mangled. */
    char            szName[32];
    /** Address. */
    RTR0PTR         pfn;
} SUPFUNC, *PSUPFUNC;

/** SUP_IOCTL_QUERY_FUNCS Output. */
typedef struct SUPQUERYFUNCS_OUT
{
    /** Number of functions returned. */
    uint32_t        cFunctions;
    /** Array of functions. */
    SUPFUNC         aFunctions[1];
} SUPQUERYFUNCS_OUT, *PSUPQUERYFUNCS_OUT;



/** SUP_IOCTL_IDT_INSTALL Input. */
typedef struct SUPIDTINSTALL_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
} SUPIDTINSTALL_IN, *PSUPIDTINSTALL_IN;

/** SUP_IOCTL_IDT_INSTALL Output. */
typedef struct SUPIDTINSTALL_OUT
{
    /** Cookie. */
    uint8_t         u8Idt;
} SUPIDTINSTALL_OUT, *PSUPIDTINSTALL_OUT;



/** SUP_IOCTL_IDT_REMOVE Input. */
typedef struct SUPIDTREMOVE_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
} SUPIDTREMOVE_IN, *PSUPIDTREMOVE_IN;



/** SUP_IOCTL_PINPAGES Input. */
typedef struct SUPPINPAGES_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** Start of page range. Must be PAGE aligned. */
    RTR3PTR         pvR3;
    /** Size of the range. Must be PAGE aligned. */
    uint32_t        cPages;
} SUPPINPAGES_IN, *PSUPPINPAGES_IN;

/** SUP_IOCTL_PINPAGES Output. */
typedef struct SUPPINPAGES_OUT
{
    /** Array of pages. */
    SUPPAGE         aPages[1];
} SUPPINPAGES_OUT, *PSUPPINPAGES_OUT;



/** SUP_IOCTL_UNPINPAGES Input. */
typedef struct SUPUNPINPAGES_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** Start of page range of a range previuosly pinned. */
    RTR3PTR         pvR3;
} SUPUNPINPAGES_IN, *PSUPUNPINPAGES_IN;



/** SUP_IOCTL_CONT_ALLOC Input. */
typedef struct SUPCONTALLOC_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** Number of bytes to allocate. */
    uint32_t        cPages;
} SUPCONTALLOC_IN, *PSUPCONTALLOC_IN;



/** SUP_IOCTL_CONT_ALLOC Output. */
typedef struct SUPCONTALLOC_OUT
{
    /** The address of the ring-0 mapping of the allocated memory. */
    RTR0PTR         pvR0;
    /** The address of the ring-3 mapping of the allocated memory. */
    RTR3PTR         pvR3;
    /** The physical address of the allocation. */
    RTHCPHYS        HCPhys;
} SUPCONTALLOC_OUT, *PSUPCONTALLOC_OUT;



/** SUP_IOCTL_CONT_FREE Input. */
typedef struct SUPCONTFREE_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** The ring-3 address of the memory to free. */
    RTR3PTR         pvR3;
} SUPCONTFREE_IN, *PSUPCONTFREE_IN;



/** SUP_IOCTL_LDR_OPEN Input. */
typedef struct SUPLDROPEN_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** Size of the image we'll be loading. */
    uint32_t        cbImage;
    /** Image name.
     * This is the NAME of the image, not the file name. It is used
     * to share code with other processes. (Max len is 32 chars!)  */
    char            szName[32];
} SUPLDROPEN_IN, *PSUPLDROPEN_IN;

/** SUP_IOCTL_LDR_OPEN Output. */
typedef struct SUPLDROPEN_OUT
{
    /** The base address of the image. */
    RTR0PTR         pvImageBase;
    /** Indicate whether or not the image requires loading. */
    bool            fNeedsLoading;
} SUPLDROPEN_OUT, *PSUPLDROPEN_OUT;



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
} SUPLDRSYM, *PSUPLDRSYM;

/** SUP_IOCTL_LDR_LOAD Input. */
typedef struct SUPLDRLOAD_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** The address of module initialization function. Similar to _DLL_InitTerm(hmod, 0). */
    PFNR0MODULEINIT pfnModuleInit;
    /** The address of module termination function. Similar to _DLL_InitTerm(hmod, 1). */
    PFNR0MODULETERM pfnModuleTerm;
    /** Special entry points. */
    union
    {
        struct
        {
            /** The module handle (i.e. address). */
            RTR0PTR         pvVMMR0;
            /** Address of VMMR0Entry function. */
            RTR0PTR         pvVMMR0Entry;
        } VMMR0;
    }               EP;
    /** Address. */
    RTR0PTR         pvImageBase;
    /** Entry point type. */
    enum { EP_NOTHING, EP_VMMR0 }
                    eEPType;
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
} SUPLDRLOAD_IN, *PSUPLDRLOAD_IN;



/** SUP_IOCTL_LDR_FREE Input. */
typedef struct SUPLDRFREE_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** Address. */
    RTR0PTR         pvImageBase;
} SUPLDRFREE_IN, *PSUPLDRFREE_IN;



/** SUP_IOCTL_LDR_GET_SYMBOL Input. */
typedef struct SUPLDRGETSYMBOL_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** Address. */
    RTR0PTR         pvImageBase;
    /** The symbol name (variable length). */
    char            szSymbol[1];
} SUPLDRGETSYMBOL_IN, *PSUPLDRGETSYMBOL_IN;

/** SUP_IOCTL_LDR_GET_SYMBOL Output. */
typedef struct SUPLDRGETSYMBOL_OUT
{
    /** The symbol address. */
    RTR0PTR         pvSymbol;
} SUPLDRGETSYMBOL_OUT, *PSUPLDRGETSYMBOL_OUT;



/** SUP_IOCTL_CALL_VMMR0 Input. */
typedef struct SUPCALLVMMR0_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** The VM handle. */
    PVMR0           pVMR0;
    /** Which operation to execute. */
    uint32_t        uOperation;
    /** The size of the buffer pointed to by pvArg. */
    uint32_t        cbArg;
    /** Argument to that operation. */
    RTR3PTR         pvArg;
} SUPCALLVMMR0_IN, *PSUPCALLVMMR0_IN;

/** SUP_IOCTL_CALL_VMMR0 Output. */
typedef struct SUPCALLVMMR0_OUT
{
    /** The VBox status code for the operation. */
    int32_t         rc;
} SUPCALLVMMR0_OUT, *PSUPCALLVMMR0_OUT;



/** SUP_IOCTL_GET_PAGING_MODE Input. */
typedef struct SUPGETPAGINGMODE_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
} SUPGETPAGINGMODE_IN, *PSUPGETPAGINGMODE_IN;

/** SUP_IOCTL_GET_PAGING_MODE Output. */
typedef struct SUPGETPAGINGMODE_OUT
{
    /** The paging mode. */
    SUPPAGINGMODE       enmMode;
} SUPGETPAGINGMODE_OUT, *PSUPGETPAGINGMODE_OUT;



/** SUP_IOCTL_LOW_ALLOC Input. */
typedef struct SUPLOWALLOC_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** Number of pages to allocate. */
    uint32_t        cPages;
} SUPLOWALLOC_IN, *PSUPLOWALLOC_IN;

/** SUP_IOCTL_LOW_ALLOC Output. */
typedef struct SUPLOWALLOC_OUT
{
    /** The ring-3 address of the allocated memory. */
    RTR3PTR         pvR3;
    /** The ring-0 address of the allocated memory. */
    RTR0PTR         pvR0;
    /** Array of pages. */
    SUPPAGE         aPages[1];
} SUPLOWALLOC_OUT, *PSUPLOWALLOC_OUT;



/** SUP_IOCTL_LOW_FREE Input. */
typedef struct SUPLOWFREE_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** The ring-3 address of the memory to free. */
    RTR3PTR         pvR3;
} SUPLOWFREE_IN, *PSUPLOWFREE_IN;



/** SUP_IOCTL_GIP_MAP Input. */
typedef struct SUPGIPMAP_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
} SUPGIPMAP_IN, *PSUPGIPMAP_IN;

/** SUP_IOCTL_GIP_MAP Output. */
typedef struct SUPGIPMAP_OUT
{
    /** Pointer to the read-only usermode GIP mapping for this session. */
    R3PTRTYPE(PSUPGLOBALINFOPAGE)   pGipR3;
    /** Pointer to the supervisor mode GIP mapping. */
    R0PTRTYPE(PSUPGLOBALINFOPAGE)   pGipR0;
    /** The physical address of the GIP. */
    RTHCPHYS                        HCPhysGip;
} SUPGIPMAP_OUT, *PSUPGIPMAP_OUT;



/** SUP_IOCTL_GIP_UNMAP Input. */
typedef struct SUPGIPUNMAP_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
} SUPGIPUNMAP_IN, *PSUPGIPUNMAP_IN;



/** SUP_IOCTL_SET_VM_FOR_FAST Input. */
typedef struct SUPSETVMFORFAST_IN
{
    /** Cookie. */
    uint32_t        u32Cookie;
    /** Session cookie. */
    uint32_t        u32SessionCookie;
    /** The ring-0 VM handle (pointer). */
    PVMR0           pVMR0;
} SUPSETVMFORFAST_IN, *PSUPSETVMFORFAST_IN;

#pragma pack()                          /* paranoia */

#endif

