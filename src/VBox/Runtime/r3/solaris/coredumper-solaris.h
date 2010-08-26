/* $Id$ */
/** @file
 * IPRT Testcase - Core dump, header.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
 */

#include <iprt/process.h>
#include <iprt/file.h>

#ifdef RT_OS_SOLARIS
# if defined(RT_ARCH_X86) && _FILE_OFFSET_BITS==64
/*
 * Solaris' procfs cannot be used with large file environment in 32-bit.
 */
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 32
# include <procfs.h>
# include <sys/procfs.h>
# include <sys/old_procfs.h>
# undef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 64
#else
# include <procfs.h>
# include <sys/procfs.h>
# include <sys/old_procfs.h>
#endif
# include <limits.h>
# include <thread.h>
# include <sys/auxv.h>
# include <sys/lwp.h>
# include <sys/zone.h>
# include <sys/utsname.h>

#ifdef RT_ARCH_AMD64
# define _ELF64
# undef _ELF32_COMPAT
#endif
# include <sys/corectl.h>
#endif


#ifdef RT_OS_SOLARIS
/**
 * VBOXSOLMAPINFO: Memory mapping description.
 */
typedef struct VBOXSOLMAPINFO
{
    prmap_t                         pMap;                       /* Proc description of this mapping */
    int                             fError;                     /* Any error reading this mapping (errno) */
    struct VBOXSOLMAPINFO          *pNext;                      /* Pointer to the next mapping */
} VBOXSOLMAPINFO;
typedef VBOXSOLMAPINFO *PVBOXSOLMAPINFO;

/**
 * VBOXSOLCORETYPE: Whether this is an old or new style core.
 */
typedef enum VBOXSOLCORETYPE
{
    enmOldEra       = 0x01d,        /* old */
    enmNewEra       = 0x5c1f1       /* sci-fi */
} VBOXSOLCORETYPE;

/**
 * VBOXSOLTHREADINFO: Per-Thread information.
 */
typedef struct VBOXSOLTHREADINFO
{
    lwpsinfo_t                      Info;                       /* Proc description of this thread */
    lwpstatus_t                    *pStatus;                    /* Proc description of this thread's status (can be NULL, zombie lwp) */
    struct VBOXSOLTHREADINFO       *pNext;                      /* Pointer to the next thread */
} VBOXSOLTHREADINFO;
typedef VBOXSOLTHREADINFO *PVBOXSOLTHREADINFO;
#endif


/**
 * VBOXPROCESS: Current (also the core target) process information.
 */
typedef struct VBOXPROCESS
{
    RTPROCESS                       Process;                    /* The pid of the process */
    char                            szExecPath[PATH_MAX];       /* Path of the executable */
    char                           *pszExecName;                /* Name of the executable file */
#ifdef RT_OS_SOLARIS
    psinfo_t                        ProcInfo;                   /* Process info. */
    prpsinfo_t                      ProcInfoOld;                /* Process info. Older version (for GDB compat.) */
    pstatus_t                       ProcStatus;                 /* Process status info. */
    thread_t                        hCurThread;                 /* The current thread */
    ucontext_t                     *pCurThreadCtx;              /* Context info. of current thread before starting to dump */
    RTFILE                          hAs;                        /* proc/<pid/as file handle */
    auxv_t                         *pAuxVecs;                   /* Aux vector of process */
    int                             cAuxVecs;                   /* Number of aux vector entries */
    PVBOXSOLMAPINFO                 pMapInfoHead;               /* Pointer to the head of list of mappings */
    uint32_t                        cMappings;                  /* Number of mappings (count of pMapInfoHead list) */
    PVBOXSOLTHREADINFO              pThreadInfoHead;            /* Pointer to the head of list of threads */
    uint64_t                        cThreads;                   /* Number of threads (count of pThreadInfoHead list) */
    char                            szPlatform[SYS_NMLN];       /* Platform name  */
    char                            szZoneName[ZONENAME_MAX];   /* Zone name */
    struct utsname                  UtsName;                    /* UTS name */
    void                           *pvCred;                     /* Process credential info. */
    size_t                          cbCred;                     /* Size of process credential info. */
    void                           *pvLdt;                      /* Process LDT info. */
    size_t                          cbLdt;                      /* Size of the LDT info. */
    prpriv_t                       *pPriv;                      /* Process privilege info. */
    size_t                          cbPriv;                     /* Size of process privilege info. */
    const priv_impl_info_t         *pcPrivImpl;                 /* Process privilege implementation info. (opaque handle) */
    core_content_t                  CoreContent;                /* What information goes in the core */
#else
# error Port Me!
#endif

} VBOXPROCESS;
typedef VBOXPROCESS *PVBOXPROCESS;

typedef int (*PFNCOREREADER)(RTFILE hFile, void *pv, size_t cb);
typedef int (*PFNCOREWRITER)(RTFILE hFile, const void *pcv, size_t cb);

/**
 * VBOXCORE: Core file object.
 */
typedef struct VBOXCORE
{
    char                            szCorePath[PATH_MAX];       /* Path of the core file */
    VBOXPROCESS                     VBoxProc;                   /* Current process information */
    void                           *pvCore;                     /* Pointer to memory area during dumping */
    size_t                          cbCore;                     /* Size of memory area during dumping */
    void                           *pvFree;                     /* Pointer to base of free range in preallocated memory area */
    bool                            fIsValid;                   /* Whether core information has been fully collected */
    PFNCOREREADER                   pfnReader;                  /* Reader function */
    PFNCOREWRITER                   pfnWriter;                  /* Writer function */
    RTFILE                          hCoreFile;                  /* Core file (used only while writing the core) */
    RTFOFF                          offWrite;                   /* Segment/section offset (used only while writing the core) */
} VBOXCORE;
typedef VBOXCORE *PVBOXCORE;

typedef int (*PFNCOREACCUMULATOR)(PVBOXCORE pVBoxCore);
typedef int (*PFNCORETHREADWORKER)(PVBOXCORE pVBoxCore, void *pvThreadInfo);

