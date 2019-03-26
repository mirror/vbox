/* $Id$ */
/** @file
 * DBGPlugInLinux - Code template for struct module processing.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifndef LNX_MK_VER
# define LNX_MK_VER(major, minor, build) (((major) << 22) | ((minor) << 12) | (build))
#endif
#if LNX_64BIT
# define LNX_ULONG_T uint64_t
#else
# define LNX_ULONG_T uint32_t
#endif
#if LNX_64BIT
# define PAD32ON64(seq) uint32_t RT_CONCAT(u32Padding,seq);
#else
# define PAD32ON64(seq)
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#if LNX_VER >= LNX_MK_VER(2,6,11)
typedef struct RT_CONCAT(LNXMODKOBJECT,LNX_SUFFIX)
{
    LNX_PTR_T           uPtrKName;
# if LNX_VER < LNX_MK_VER(2,6,24)
    char                name[20];
# endif
# if LNX_VER < LNX_MK_VER(2,6,27)
    int32_t             cRefs;
#  if LNX_VER >= LNX_MK_VER(2,6,24)
    PAD32ON64(0)
#  endif
# endif
    LNX_PTR_T           uPtrNext;
    LNX_PTR_T           uPtrPrev;
    LNX_PTR_T           uPtrParent; /**< struct kobject pointer */
    LNX_PTR_T           uPtrKset;   /**< struct kset pointer */
    LNX_PTR_T           uPtrKtype;  /**< struct kobj_type pointer */
    LNX_PTR_T           uPtrDirEntry; /**< struct dentry pointer; 2.6.23+ sysfs_dirent. */
# if LNX_VER >= LNX_MK_VER(2,6,17) && LNX_VER < LNX_MK_VER(2,6,24)
    LNX_PTR_T           aPtrWaitQueueHead[3];
# endif
# if LNX_VER >= LNX_MK_VER(2,6,27)
    int32_t             cRefs;
    uint32_t            uStateStuff;
# elif LNX_VER >= LNX_MK_VER(2,6,25)
    LNX_ULONG_T         uStateStuff;
# endif
    /* non-kobject: */
    LNX_PTR_T           uPtrModule;     /**< struct module pointer. */
#  if LNX_VER >= LNX_MK_VER(2,6,21)
    LNX_PTR_T           uPtrDriverDir;  /**< Points to struct kobject. */
#  endif
} RT_CONCAT(LNXMODKOBJECT,LNX_SUFFIX);
#endif
#if LNX_VER == LNX_MK_VER(2,6,24) && LNX_64BIT
AssertCompileMemberOffset(RT_CONCAT(LNXMODKOBJECT,LNX_SUFFIX), uPtrParent, 32);
AssertCompileMemberOffset(RT_CONCAT(LNXMODKOBJECT,LNX_SUFFIX), uPtrParent, 32);
AssertCompileSize(RT_CONCAT(LNXMODKOBJECT,LNX_SUFFIX), 80);
#endif



/**
 * Maps to the start of struct module in include/linux/module.h.
 */
typedef struct RT_CONCAT(LNXKMODULE,LNX_SUFFIX)
{
#if LNX_VER >= LNX_MK_VER(2,5,48)
    /*
     * This first part is mostly always the same.
     */
    int32_t     state;
    PAD32ON64(0)
    LNX_PTR_T   uPtrNext;
    LNX_PTR_T   uPtrPrev;
    char        name[64 - sizeof(LNX_PTR_T)];

    /*
     * Here be spaghetti dragons.
     */
# if LNX_VER >= LNX_MK_VER(2,6,11)
    RT_CONCAT(LNXMODKOBJECT,LNX_SUFFIX) mkobj; /**< Was just kobj for a while. */
    LNX_PTR_T   uPtrParamAttrs;     /**< Points to struct module_param_attrs. */
#  if LNX_VER >= LNX_MK_VER(2,6,17)
    LNX_PTR_T   uPtrModInfoAttrs;   /**< Points to struct module_attribute. */
#  endif
#  if LNX_VER == LNX_MK_VER(2,6,20)
    LNX_PTR_T   uPtrDriverDir;      /**< Points to struct kobject. */
#  elif LNX_VER >= LNX_MK_VER(2,6,21)
    LNX_PTR_T   uPtrHolderDir;      /**< Points to struct kobject. */
#  endif
#  if LNX_VER >= LNX_MK_VER(2,6,13)
    LNX_PTR_T   uPtrVersion;        /**< String pointers. */
    LNX_PTR_T   uPtrSrcVersion;     /**< String pointers. */
#  endif
# else
#  if LNX_VER >= LNX_MK_VER(2,6,7)
    LNX_PTR_T   uPtrMkObj;
#  endif
#  if LNX_VER >= LNX_MK_VER(2,6,10)
    LNX_PTR_T   uPtrParamsKobject;
#  endif
# endif

    /** @name Exported Symbols
     * @{ */
# if LNX_VER < LNX_MK_VER(2,5,67)
    LNX_PTR_T   uPtrSymsNext, uPtrSymsPrev, uPtrSymsOwner;
#  if LNX_VER >= LNX_MK_VER(2,5,55)
    int32_t     syms_gplonly;
    uint32_t    num_syms;
# else
    uint32_t    num_syms;
    PAD32ON64(1)
#  endif
# endif
    LNX_PTR_T   uPtrSyms; /**< Array of struct kernel_symbol. */
# if LNX_VER >= LNX_MK_VER(2,5,67)
    uint32_t    num_syms;
    PAD32ON64(1)
# endif
# if LNX_VER >= LNX_MK_VER(2,5,60)
    LNX_PTR_T   uPtrCrcs; /**< unsigned long array */
# endif
    /** @} */

    /** @name GPL Symbols
     * @since 2.5.55
     * @{ */
# if LNX_VER >= LNX_MK_VER(2,5,55)
#  if LNX_VER < LNX_MK_VER(2,5,67)
    LNX_PTR_T   uPtrGplSymsNext, uPtrGplSymsPrev, uPtrGplSymsOwner;
#   if LNX_VER >= LNX_MK_VER(2,5,55)
    int32_t     gpl_syms_gplonly;
    uint32_t    num_gpl_syms;
#  else
    uint32_t    num_gpl_syms;
    PAD32ON64(2)
#   endif
#  endif
    LNX_PTR_T   uPtrGplSyms; /**< Array of struct kernel_symbol. */
#  if LNX_VER >= LNX_MK_VER(2,5,67)
    uint32_t    num_gpl_syms;
    PAD32ON64(2)
#  endif
#  if LNX_VER >= LNX_MK_VER(2,5,60)
    LNX_PTR_T   uPtrGplCrcs; /**< unsigned long array */
#  endif
# endif /* > 2.5.55 */
    /** @} */

    /** @name Unused Exported Symbols
     * @since 2.6.18
     * @{ */
# if LNX_VER >= LNX_MK_VER(2,6,18)
    LNX_PTR_T   uPtrUnusedSyms; /**< Array of struct kernel_symbol. */
    uint32_t    num_unused_syms;
    PAD32ON64(4)
    LNX_PTR_T   uPtrUnusedCrcs; /**< unsigned long array */
# endif
    /** @} */

    /** @name Unused GPL Symbols
     * @since 2.6.18
     * @{ */
# if LNX_VER >= LNX_MK_VER(2,6,18)
    LNX_PTR_T   uPtrUnusedGplSyms; /**< Array of struct kernel_symbol. */
    uint32_t    num_unused_gpl_syms;
    PAD32ON64(5)
    LNX_PTR_T   uPtrUnusedGplCrcs; /**< unsigned long array */
# endif
    /** @} */

    /** @name Future GPL Symbols
     * @since 2.6.17
     * @{ */
# if LNX_VER >= LNX_MK_VER(2,6,17)
    LNX_PTR_T   uPtrGplFutureSyms; /**< Array of struct kernel_symbol. */
    uint32_t    num_gpl_future_syms;
    PAD32ON64(3)
    LNX_PTR_T   uPtrGplFutureCrcs; /**< unsigned long array */
# endif
    /** @} */

    /** @name Exception table.
     * @{ */
# if LNX_VER < LNX_MK_VER(2,5,67)
    LNX_PTR_T   uPtrXcptTabNext, uPtrXcptTabPrev;
# endif
    uint32_t    num_exentries;
    PAD32ON64(6)
    LNX_PTR_T   uPtrEntries; /**< struct exception_table_entry array. */
    /** @} */

    /*
     * Hopefully less spaghetti from here on...
     */
    LNX_PTR_T   pfnInit;
    LNX_PTR_T   uPtrModuleInit;
    LNX_PTR_T   uPtrModuleCore;
    LNX_ULONG_T cbInit;
    LNX_ULONG_T cbCore;
# if LNX_VER >= LNX_MK_VER(2,5,74)
    LNX_ULONG_T cbInitText;
    LNX_ULONG_T cbCoreText;
# endif

# if LNX_VER >= LNX_MK_VER(2,6,18)
    LNX_PTR_T   uPtrUnwindInfo;
# endif
#else
   uint32_t     structure_size;

#endif
} RT_CONCAT(LNXKMODULE,LNX_SUFFIX);

# if LNX_VER == LNX_MK_VER(2,6,24) && LNX_64BIT
AssertCompileMemberOffset(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), uPtrParamAttrs, 160);
AssertCompileMemberOffset(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), num_syms, 208);
AssertCompileMemberOffset(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), num_gpl_syms, 232);
AssertCompileMemberOffset(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), num_unused_syms, 256);
AssertCompileMemberOffset(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), num_unused_gpl_syms, 280);
AssertCompileMemberOffset(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), num_gpl_future_syms, 304);
AssertCompileMemberOffset(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), num_exentries, 320);
AssertCompileMemberOffset(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), uPtrModuleCore, 352);
AssertCompileMemberOffset(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), uPtrUnwindInfo, 392);
#endif



/**
 * Version specific module processing code.
 */
static uint64_t RT_CONCAT(dbgDiggerLinuxLoadModule,LNX_SUFFIX)(PDBGDIGGERLINUX pThis, PUVM pUVM, PDBGFADDRESS pAddrModule)
{
    RT_CONCAT(LNXKMODULE,LNX_SUFFIX) Module;

    int rc = DBGFR3MemRead(pUVM, 0, DBGFR3AddrSub(pAddrModule, RT_UOFFSETOF(RT_CONCAT(LNXKMODULE,LNX_SUFFIX), uPtrNext)),
                           &Module, sizeof(Module));
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("Failed to read module structure at %#RX64: %Rrc\n", pAddrModule->FlatPtr, rc));
        return 0;
    }

    /*
     * Check the module name.
     */
#if LNX_VER >= LNX_MK_VER(2,5,48)
    const char  *pszName = Module.name;
    size_t const cbName  = sizeof(Module.name);
#else

#endif
    if (   RTStrNLen(pszName, cbName) >= cbName
        || RT_FAILURE(RTStrValidateEncoding(pszName))
        || *pszName == '\0')
    {
        LogRelFunc(("%#RX64: Bad name: %.*Rhxs\n", pAddrModule->FlatPtr, (int)cbName, pszName));
        return 0;
    }

    /*
     * Create a simple module for it.
     */
    LogRelFunc((" %#RX64: %#RX64 LB %#RX64 %s\n", pAddrModule->FlatPtr, Module.uPtrModuleCore, Module.cbCore, pszName));

    RTDBGMOD hDbgMod;
    rc = RTDbgModCreate(&hDbgMod, pszName, Module.cbCore, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
    {
        rc = RTDbgModSetTag(hDbgMod, DIG_LNX_MOD_TAG);
        if (RT_SUCCESS(rc))
        {
            RTDBGAS hAs = DBGFR3AsResolveAndRetain(pUVM, DBGF_AS_KERNEL);
            rc = RTDbgAsModuleLink(hAs, hDbgMod, Module.uPtrModuleCore, RTDBGASLINK_FLAGS_REPLACE /*fFlags*/);
            RTDbgAsRelease(hAs);
        }
        else
            LogRel(("DbgDiggerOs2: RTDbgModSetTag failed: %Rrc\n", rc));
        RTDbgModRelease(hDbgMod);
    }

    RT_NOREF(pThis);
    return Module.uPtrNext;
}

#undef LNX_VER
#undef LNX_SUFFIX
#undef LNX_ULONG_T
#undef PAD32ON64
