/* $Id$ */
/** @file
 * IPRT Testcase - Core Dumper.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_CORE_DUMPER
#include <VBox/log.h>
#include <iprt/coredumper.h>
#include <iprt/types.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/param.h>
#include <iprt/asm.h>
#include "coredumper-solaris.h"

#ifdef RT_OS_SOLARIS
# include <syslog.h>
# include <signal.h>
# include <stdlib.h>
# include <unistd.h>
# include <errno.h>
# include <zone.h>
# include <sys/proc.h>
# include <sys/sysmacros.h>
# include <sys/systeminfo.h>
# include <sys/mman.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <ucontext.h>
#endif  /* RT_OS_SOLARIS */

#include "internal/ldrELF.h"
#include "internal/ldrELF64.h"

/*******************************************************************************
*   Globals                                                                    *
*******************************************************************************/
static RTNATIVETHREAD volatile  g_CoreDumpThread             = NIL_RTNATIVETHREAD;
static bool volatile            g_fCoreDumpSignalSetup       = false;
static uint32_t volatile        g_fCoreDumpFlags             = 0;
static char                     g_szCoreDumpDir[PATH_MAX]    = { 0 };
static char                     g_szCoreDumpFile[PATH_MAX]   = { 0 };


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define CORELOG_NAME        "CoreDumper: "
#define CORELOG(a)          Log(a)
#define CORELOGRELSYS(a)       \
    do { \
        rtCoreDumperSysLogWrapper a; \
    } while (0)


/**
 * ELFNOTEHDR: ELF NOTE header.
 */
typedef struct ELFNOTEHDR
{
    Elf64_Nhdr                      Hdr;                        /* Header of NOTE section */
    char                            achName[8];                 /* Name of NOTE section */
} ELFNOTEHDR;
typedef ELFNOTEHDR *PELFNOTEHDR;

/**
 * Wrapper function to write IPRT format style string to the syslog.
 *
 * @param pszFormat         Format string
 */
static void rtCoreDumperSysLogWrapper(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    char szBuf[1024];
    RTStrPrintfV(szBuf, sizeof(szBuf), pszFormat, va);
    va_end(va);
    syslog(LOG_ERR, "%s", szBuf);
}


/**
 * Determines endianness of the system. Just for completeness.
 *
 * @return Will return false if system is little endian, true otherwise.
 */
static bool IsBigEndian()
{
    const int i = 1;
    char *p = (char *)&i;
    if (p[0] == 1)
        return false;
    return true;
}


/**
 * Reads from a file making sure an interruption doesn't cause a failure.
 *
 * @param hFile             Handle to the file to read.
 * @param pv                Where to store the read data.
 * @param cbToRead          Size of data to read.
 *
 * @return IPRT status code.
 */
static int ReadFileNoIntr(RTFILE hFile, void *pv, size_t cbToRead)
{
    int rc = VERR_READ_ERROR;
    while (1)
    {
        rc = RTFileRead(hFile, pv, cbToRead, NULL /* Read all */);
        if (rc == VERR_INTERRUPTED)
            continue;
        break;
    }
    return rc;
}


/**
 * Writes to a file making sure an interruption doesn't cause a failure.
 *
 * @param hFile             Handle to the file to write.
 * @param pv                Pointer to what to write.
 * @param cbToRead          Size of data to write.
 *
 * @return IPRT status code.
 */
static int WriteFileNoIntr(RTFILE hFile, const void *pcv, size_t cbToRead)
{
    int rc = VERR_READ_ERROR;
    while (1)
    {
        rc = RTFileWrite(hFile, pcv, cbToRead, NULL /* Write all */);
        if (rc == VERR_INTERRUPTED)
            continue;
        break;
    }
    return rc;
}


/**
 * Read from a given offset in the process' address space.
 *
 * @param pVBoxProc         Pointer to the VBox process.
 * @param pv                Where to read the data into.
 * @param cb                Size of the read buffer.
 * @param off               Offset to read from.
 *
 * @return VINF_SUCCESS, if all the given bytes was read in, otherwise VERR_READ_ERROR.
 */
static ssize_t ProcReadAddrSpace(PVBOXPROCESS pVBoxProc, RTFOFF off, void *pvBuf, size_t cbToRead)
{
    while (1)
    {
        int rc = RTFileReadAt(pVBoxProc->hAs, off, pvBuf, cbToRead, NULL);
        if (rc == VERR_INTERRUPTED)
            continue;
        return rc;
    }
}


/**
 * Determines if the current process' architecture is suitable for dumping core.
 *
 * @param pVBoxProc         Pointer to the VBox process.
 *
 * @return true if the architecture matches the current one.
 */
static inline bool IsProcessArchNative(PVBOXPROCESS pVBoxProc)
{
    return pVBoxProc->ProcInfo.pr_dmodel == PR_MODEL_NATIVE;
}


/**
 * Helper function to get the size of a file given it's path.
 *
 * @param pszPath           Pointer to the full path of the file.
 *
 * @return The size of the file in bytes.
 */
static size_t GetFileSize(const char *pszPath)
{
    uint64_t cb = 0;
    int fd = open(pszPath, O_RDONLY);
    if (fd >= 0)
    {
        RTFILE hFile = (RTFILE)(uintptr_t)fd;
        RTFileGetSize(hFile, &cb);
        RTFileClose(hFile);
    }
    else
        CORELOGRELSYS((CORELOG_NAME "GetFileSize: failed to open %s rc=%Rrc\n", pszPath, RTErrConvertFromErrno(fd)));
    return cb < ~(size_t)0 ? (size_t)cb : ~(size_t)0;
}


/**
 * Pre-compute and pre-allocate sufficient memory for dumping core.
 * This is meant to be called once, as a single-large anonymously
 * mapped memory area which will be used during the core dumping routines.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @return IPRT status code.
 */
static int AllocMemoryArea(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore->pvCore == NULL, VERR_ALREADY_EXISTS);

    struct VBOXSOLPREALLOCTABLE
    {
        const char *pszFilePath;        /* Proc based path */
        size_t      cbHeader;           /* Size of header */
        size_t      cbEntry;            /* Size of each entry in file */
        size_t      cbAccounting;       /* Size of each accounting entry per entry */
    } aPreAllocTable[] = {
        { "/proc/%d/map",        0,                  sizeof(prmap_t),       sizeof(VBOXSOLMAPINFO) },
        { "/proc/%d/auxv",       0,                  0,                     0 },
        { "/proc/%d/lpsinfo",    sizeof(prheader_t), sizeof(lwpsinfo_t),    sizeof(VBOXSOLTHREADINFO) },
        { "/proc/%d/lstatus",    0,                  0,                     0 },
        { "/proc/%d/ldt",        0,                  0,                     0 },
        { "/proc/%d/cred",       sizeof(prcred_t),   sizeof(gid_t),         0 },
        { "/proc/%d/priv",       sizeof(prpriv_t),   sizeof(priv_chunk_t),  0 },
    };

    size_t cb = 0;
    for (int i = 0; i < (int)RT_ELEMENTS(aPreAllocTable); i++)
    {
        char szPath[PATH_MAX];
        RTStrPrintf(szPath, sizeof(szPath), aPreAllocTable[i].pszFilePath, (int)pVBoxCore->VBoxProc.Process);
        size_t cbFile = GetFileSize(szPath);
        cb += cbFile;
        if (   cbFile > 0
            && aPreAllocTable[i].cbEntry > 0)
        {
            cb += ((cbFile - aPreAllocTable[i].cbHeader) / aPreAllocTable[i].cbEntry) * (aPreAllocTable[i].cbAccounting > 0 ?
                                                                                         aPreAllocTable[i].cbAccounting : 1);
            cb += aPreAllocTable[i].cbHeader;
        }
    }

    /*
     * Make room for our own mapping accountant entry which will also be included in the core.
     */
    cb += sizeof(VBOXSOLMAPINFO);

    /*
     * Allocate the required space, plus some extra room.
     */
    cb += _128K;
    void *pv = mmap(NULL, cb, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1 /* fd */, 0 /* offset */);
    if (pv != MAP_FAILED)
    {
        CORELOG((CORELOG_NAME "AllocMemoryArea: memory area of %u bytes allocated.\n", cb));
        pVBoxCore->pvCore = pv;
        pVBoxCore->pvFree = pv;
        pVBoxCore->cbCore = cb;
        return VINF_SUCCESS;
    }
    else
    {
        CORELOGRELSYS((CORELOG_NAME "AllocMemoryArea: failed cb=%u\n", cb));
        return VERR_NO_MEMORY;
    }
}


/**
 * Free memory area used by the core object.
 *
 * @param pVBoxCore         Pointer to the core object.
 */
static void FreeMemoryArea(PVBOXCORE pVBoxCore)
{
    AssertReturnVoid(pVBoxCore);
    AssertReturnVoid(pVBoxCore->pvCore);
    AssertReturnVoid(pVBoxCore->cbCore > 0);

    munmap(pVBoxCore->pvCore, pVBoxCore->cbCore);
    CORELOG((CORELOG_NAME "FreeMemoryArea: memory area of %u bytes freed.\n", pVBoxCore->cbCore));

    pVBoxCore->pvCore = NULL;
    pVBoxCore->pvFree= NULL;
    pVBoxCore->cbCore = 0;
}


/**
 * Get a chunk from the area of allocated memory.
 *
 * @param pVBoxCore         Pointer to the core object.
 * @param cb                Size of requested chunk.
 *
 * @return Pointer to allocated memory, or NULL on failure.
 */
static void *GetMemoryChunk(PVBOXCORE pVBoxCore, size_t cb)
{
    AssertReturn(pVBoxCore, NULL);
    AssertReturn(pVBoxCore->pvCore, NULL);
    AssertReturn(pVBoxCore->pvFree, NULL);

    size_t cbAllocated = (char *)pVBoxCore->pvFree - (char *)pVBoxCore->pvCore;
    if (cbAllocated < pVBoxCore->cbCore)
    {
        char *pb = (char *)pVBoxCore->pvFree;
        pVBoxCore->pvFree = pb + cb;
        return pb;
    }

    return NULL;
}


/**
 * Reads the proc file's content into a newly allocated buffer.
 *
 * @param pVBoxCore         Pointer to the core object.
 * @param pszFileFmt        Only the name of the file to read from (/proc/<pid> will be prepended)
 * @param ppv               Where to store the allocated buffer.
 * @param pcb               Where to store size of the buffer.
 *
 * @return IPRT status code. If the proc file is 0 bytes, VINF_SUCCESS is
 *          returned with pointed to values of @c ppv, @c pcb set to NULL and 0
 *          respectively.
 */
static int ProcReadFileInto(PVBOXCORE pVBoxCore, const char *pszProcFileName, void **ppv, size_t *pcb)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    char szPath[PATH_MAX];
    RTStrPrintf(szPath, sizeof(szPath), "/proc/%d/%s", (int)pVBoxCore->VBoxProc.Process, pszProcFileName);
    int rc = VINF_SUCCESS;
    int fd = open(szPath, O_RDONLY);
    if (fd >= 0)
    {
        RTFILE hFile = (RTFILE)(uintptr_t)fd;
        uint64_t u64Size;
        RTFileGetSize(hFile, &u64Size);
        *pcb = u64Size < ~(size_t)0 ? u64Size : ~(size_t)0;
        if (*pcb > 0)
        {
            *ppv = GetMemoryChunk(pVBoxCore, *pcb);
            if (*ppv)
                rc = ReadFileNoIntr(hFile, *ppv, *pcb);
            else
                rc = VERR_NO_MEMORY;
        }
        else
        {
            *pcb =  0;
            *ppv = NULL;
            rc = VINF_SUCCESS;
        }
        RTFileClose(hFile);
    }
    else
    {
        rc = RTErrConvertFromErrno(fd);
        CORELOGRELSYS((CORELOG_NAME "ProcReadFileInto: failed to open %s. rc=%Rrc\n", szPath, rc));
    }
    return rc;
}


/**
 * Read process information (format psinfo_t) from /proc.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @return IPRT status code.
 */
static int ProcReadInfo(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    char szPath[PATH_MAX];
    int rc = VINF_SUCCESS;

    RTStrPrintf(szPath, sizeof(szPath), "/proc/%d/psinfo", (int)pVBoxProc->Process);
    int fd = open(szPath, O_RDONLY);
    if (fd >= 0)
    {
        RTFILE hFile = (RTFILE)(uintptr_t)fd;
        size_t cbProcInfo = sizeof(psinfo_t);
        rc = ReadFileNoIntr(hFile, &pVBoxProc->ProcInfo, cbProcInfo);
        RTFileClose(hFile);
    }
    else
    {
        rc = RTErrConvertFromErrno(fd);
        CORELOGRELSYS((CORELOG_NAME "ProcReadInfo: failed to open %s. rc=%Rrc\n", szPath, rc));
    }

    return rc;
}


/**
 * Read process status (format pstatus_t) from /proc.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @return IPRT status code.
 */
static int ProcReadStatus(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;

    char szPath[PATH_MAX];
    int rc = VINF_SUCCESS;

    RTStrPrintf(szPath, sizeof(szPath), "/proc/%d/status", (int)pVBoxProc->Process);
    int fd = open(szPath, O_RDONLY);
    if (fd >= 0)
    {
        RTFILE hFile = (RTFILE)(uintptr_t)fd;
        size_t cbRead;
        size_t cbProcStatus = sizeof(pstatus_t);
        AssertCompile(sizeof(pstatus_t) == sizeof(pVBoxProc->ProcStatus));
        rc = ReadFileNoIntr(hFile, &pVBoxProc->ProcStatus, cbProcStatus);
        RTFileClose(hFile);
    }
    else
    {
        rc = RTErrConvertFromErrno(fd);
        CORELOGRELSYS((CORELOG_NAME "ProcReadStatus: failed to open %s. rc=%Rrc\n", szPath, rc));
    }
    return rc;
}


/**
 * Read process credential information (format prcred_t + array of guid_t)
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @remarks Should not be called before successful call to @see AllocMemoryArea()
 * @return IPRT status code.
 */
static int ProcReadCred(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    return ProcReadFileInto(pVBoxCore, "cred", &pVBoxProc->pvCred, &pVBoxProc->cbCred);
}


/**
 * Read process privilege information (format prpriv_t + array of priv_chunk_t)
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @remarks Should not be called before successful call to @see AllocMemoryArea()
 * @return IPRT status code.
 */
static int ProcReadPriv(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    int rc = ProcReadFileInto(pVBoxCore, "priv", (void **)&pVBoxProc->pPriv, &pVBoxProc->cbPriv);
    if (RT_FAILURE(rc))
        return rc;
    pVBoxProc->pcPrivImpl = getprivimplinfo();
    if (!pVBoxProc->pcPrivImpl)
    {
        CORELOGRELSYS((CORELOG_NAME "ProcReadPriv: getprivimplinfo returned NULL.\n"));
        return VERR_INVALID_STATE;
    }
    return rc;
}


/**
 * Read process LDT information (format array of struct ssd) from /proc.
 *
 * @param pVBoxProc         Pointer to the core object.
 *
 * @remarks Should not be called before successful call to @see AllocMemoryArea()
 * @return IPRT status code.
 */
static int ProcReadLdt(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    return ProcReadFileInto(pVBoxCore, "ldt", &pVBoxProc->pvLdt, &pVBoxProc->cbLdt);
}


/**
 * Read process auxiliary vectors (format auxv_t) for the process.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @remarks Should not be called before successful call to @see AllocMemoryArea()
 * @return IPRT status code.
 */
static int ProcReadAuxVecs(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    char szPath[PATH_MAX];
    int rc = VINF_SUCCESS;
    RTStrPrintf(szPath, sizeof(szPath), "/proc/%d/auxv", (int)pVBoxProc->Process);
    int fd = open(szPath, O_RDONLY);
    if (fd < 0)
    {
        rc = RTErrConvertFromErrno(fd);
        CORELOGRELSYS((CORELOG_NAME "ProcReadAuxVecs: failed to open %s rc=%Rrc\n", szPath, rc));
        return rc;
    }

    RTFILE hFile = (RTFILE)(uintptr_t)fd;
    uint64_t u64Size;
    RTFileGetSize(hFile, &u64Size);
    size_t cbAuxFile = u64Size < ~(size_t)0 ? u64Size : ~(size_t)0;
    if (cbAuxFile >= sizeof(auxv_t))
    {
        pVBoxProc->pAuxVecs = (auxv_t*)GetMemoryChunk(pVBoxCore, cbAuxFile + sizeof(auxv_t));
        if (pVBoxProc->pAuxVecs)
        {
            rc = ReadFileNoIntr(hFile, pVBoxProc->pAuxVecs, cbAuxFile);
            if (RT_SUCCESS(rc))
            {
                /* Terminate list of vectors */
                pVBoxProc->cAuxVecs = cbAuxFile / sizeof(auxv_t);
                CORELOG((CORELOG_NAME "ProcReadAuxVecs: cbAuxFile=%u auxv_t size %d cAuxVecs=%u\n", cbAuxFile, sizeof(auxv_t),
                         pVBoxProc->cAuxVecs));
                if (pVBoxProc->cAuxVecs > 0)
                {
                    pVBoxProc->pAuxVecs[pVBoxProc->cAuxVecs].a_type = AT_NULL;
                    pVBoxProc->pAuxVecs[pVBoxProc->cAuxVecs].a_un.a_val = 0L;
                    RTFileClose(hFile);
                    return VINF_SUCCESS;
                }
                else
                {
                    CORELOGRELSYS((CORELOG_NAME "ProcReadAuxVecs: Invalid vector count %u\n", pVBoxProc->cAuxVecs));
                    rc = VERR_READ_ERROR;
                }
            }
            else
                CORELOGRELSYS((CORELOG_NAME "ProcReadAuxVecs: ReadFileNoIntr failed. rc=%Rrc cbAuxFile=%u\n", rc, cbAuxFile));

            pVBoxProc->pAuxVecs = NULL;
            pVBoxProc->cAuxVecs = 0;
        }
        else
        {
            CORELOGRELSYS((CORELOG_NAME "ProcReadAuxVecs: no memory for %u bytes\n", cbAuxFile + sizeof(auxv_t)));
            rc = VERR_NO_MEMORY;
        }
    }
    else
    {
        CORELOGRELSYS((CORELOG_NAME "ProcReadAuxVecs: aux file too small %u, expecting %u or more\n", cbAuxFile, sizeof(auxv_t)));
        rc = VERR_READ_ERROR;
    }

    RTFileClose(hFile);
    return rc;
}


/*
 * Find an element in the process' auxiliary vector.
 */
static long GetAuxVal(PVBOXPROCESS pVBoxProc, int Type)
{
    AssertReturn(pVBoxProc, -1);
    if (pVBoxProc->pAuxVecs)
    {
        auxv_t *pAuxVec = pVBoxProc->pAuxVecs;
        for (; pAuxVec->a_type != AT_NULL; pAuxVec++)
        {
            if (pAuxVec->a_type == Type)
                return pAuxVec->a_un.a_val;
        }
    }
    return -1;
}


/**
 * Read the process mappings (format prmap_t array).
 *
 * @param   pVBoxCore           Pointer to the core object.
 *
 * @remarks Should not be called before successful call to @see AllocMemoryArea()
 * @return IPRT status code.
 */
static int ProcReadMappings(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    char szPath[PATH_MAX];
    int rc = VINF_SUCCESS;
    RTStrPrintf(szPath, sizeof(szPath), "/proc/%d/map", (int)pVBoxProc->Process);
    int fd = open(szPath, O_RDONLY);
    if (fd < 0)
    {
        rc = RTErrConvertFromErrno(fd);
        CORELOGRELSYS((CORELOG_NAME "ProcReadMappings: failed to open %s. rc=%Rrc\n", szPath, rc));
        return rc;
    }

    RTFILE hFile = (RTFILE)(uintptr_t)fd;
    RTStrPrintf(szPath, sizeof(szPath), "/proc/%d/as", (int)pVBoxProc->Process);
    fd = open(szPath, O_RDONLY);
    if (fd >= 0)
    {
        pVBoxProc->hAs = (RTFILE)(uintptr_t)fd;

        /*
         * Allocate and read all the prmap_t objects from proc.
         */
        uint64_t u64Size;
        RTFileGetSize(hFile, &u64Size);
        size_t cbMapFile = u64Size < ~(size_t)0 ? u64Size : ~(size_t)0;
        if (cbMapFile >= sizeof(prmap_t))
        {
            prmap_t *pMap = (prmap_t*)GetMemoryChunk(pVBoxCore, cbMapFile);
            if (pMap)
            {
                rc = ReadFileNoIntr(hFile, pMap, cbMapFile);
                if (RT_SUCCESS(rc))
                {
                    pVBoxProc->cMappings = cbMapFile / sizeof(prmap_t);
                    if (pVBoxProc->cMappings > 0)
                    {
                        /*
                         * Allocate for each prmap_t object, a corresponding VBOXSOLMAPINFO object.
                         */
                        pVBoxProc->pMapInfoHead = (PVBOXSOLMAPINFO)GetMemoryChunk(pVBoxCore, pVBoxProc->cMappings * sizeof(VBOXSOLMAPINFO));
                        if (pVBoxProc->pMapInfoHead)
                        {
                            /*
                             * Associate the prmap_t with the mapping info object.
                             */
                            Assert(pVBoxProc->pMapInfoHead == NULL);
                            PVBOXSOLMAPINFO pCur = pVBoxProc->pMapInfoHead;
                            PVBOXSOLMAPINFO pPrev = NULL;
                            for (uint64_t i = 0; i < pVBoxProc->cMappings; i++, pMap++, pCur++)
                            {
                                memcpy(&pCur->pMap, pMap, sizeof(pCur->pMap));
                                if (pPrev)
                                    pPrev->pNext = pCur;

                                pCur->fError = 0;

                                /*
                                 * Make sure we can read the mapping, otherwise mark them to be skipped.
                                 */
                                char achBuf[PAGE_SIZE];
                                uint64_t k = 0;
                                while (k < pCur->pMap.pr_size)
                                {
                                    size_t cb = RT_MIN(sizeof(achBuf), pCur->pMap.pr_size - k);
                                    int rc2 = ProcReadAddrSpace(pVBoxProc, pCur->pMap.pr_vaddr + k, &achBuf, cb);
                                    if (RT_FAILURE(rc2))
                                    {
                                        CORELOGRELSYS((CORELOG_NAME "ProcReadMappings: skipping mapping. vaddr=%#x rc=%Rrc\n",
                                                       pCur->pMap.pr_vaddr, rc2));

                                        /*
                                         * Instead of storing the actual mapping data which we failed to read, the core
                                         * will contain an errno in place. So we adjust the prmap_t's size field too
                                         * so the program header offsets match.
                                         */
                                        pCur->pMap.pr_size = RT_ALIGN_Z(sizeof(int), 8);
                                        pCur->fError = errno;
                                        if (pCur->fError == 0)  /* huh!? somehow errno got reset? fake one! EFAULT is nice. */
                                            pCur->fError = EFAULT;
                                        break;
                                    }
                                    k += cb;
                                }

                                pPrev = pCur;
                            }
                            if (pPrev)
                                pPrev->pNext = NULL;

                            RTFileClose(hFile);
                            RTFileClose(pVBoxProc->hAs);
                            pVBoxProc->hAs = NIL_RTFILE;
                            CORELOG((CORELOG_NAME "ProcReadMappings: successfully read in %u mappings\n", pVBoxProc->cMappings));
                            return VINF_SUCCESS;
                        }
                        else
                        {
                            CORELOGRELSYS((CORELOG_NAME "ProcReadMappings: GetMemoryChunk failed %u\n",
                                           pVBoxProc->cMappings * sizeof(VBOXSOLMAPINFO)));
                            rc = VERR_NO_MEMORY;
                        }
                    }
                    else
                    {
                        CORELOGRELSYS((CORELOG_NAME "ProcReadMappings: Invalid mapping count %u\n", pVBoxProc->cMappings));
                        rc = VERR_READ_ERROR;
                    }
                }
                else
                    CORELOGRELSYS((CORELOG_NAME "ProcReadMappings: FileReadNoIntr failed. rc=%Rrc cbMapFile=%u\n", rc, cbMapFile));
            }
            else
            {
                CORELOGRELSYS((CORELOG_NAME "ProcReadMappings: GetMemoryChunk failed. cbMapFile=%u\n", cbMapFile));
                rc = VERR_NO_MEMORY;
            }
        }

        RTFileClose(pVBoxProc->hAs);
        pVBoxProc->hAs = NIL_RTFILE;
    }
    else
        CORELOGRELSYS((CORELOG_NAME "ProcReadMappings: failed to open %s. rc=%Rrc\n", szPath, rc));

    RTFileClose(hFile);
    return rc;
}


/**
 * Reads the thread information for all threads in the process.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @remarks Should not be called before successful call to @see AllocMemoryArea()
 * @return IPRT status code.
 */
static int ProcReadThreads(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    AssertReturn(pVBoxProc->pCurThreadCtx, VERR_NO_DATA);

    /*
     * Read the information for threads.
     * Format: prheader_t + array of lwpsinfo_t's.
     */
    size_t cbInfoHdrAndData;
    void *pvInfoHdr = NULL;
    int rc = ProcReadFileInto(pVBoxCore, "lpsinfo", &pvInfoHdr, &cbInfoHdrAndData);
    if (RT_SUCCESS(rc))
    {
        /*
         * Read the status of threads.
         * Format: prheader_t + array of lwpstatus_t's.
         */
        void *pvStatusHdr = NULL;
        size_t cbStatusHdrAndData;
        rc = ProcReadFileInto(pVBoxCore, "lstatus", &pvStatusHdr, &cbStatusHdrAndData);
        if (RT_SUCCESS(rc))
        {
            prheader_t *pInfoHdr   = (prheader_t *)pvInfoHdr;
            prheader_t *pStatusHdr = (prheader_t *)pvStatusHdr;
            lwpstatus_t *pStatus   = (lwpstatus_t *)((uintptr_t)pStatusHdr + sizeof(prheader_t));
            lwpsinfo_t *pInfo      = (lwpsinfo_t *)((uintptr_t)pInfoHdr + sizeof(prheader_t));
            uint64_t cStatus       = pStatusHdr->pr_nent;
            uint64_t cInfo         = pInfoHdr->pr_nent;

            CORELOG((CORELOG_NAME "ProcReadThreads: read info(%u) status(%u), threads:cInfo=%u cStatus=%u\n", cbInfoHdrAndData,
                        cbStatusHdrAndData, cInfo, cStatus));

            /*
             * Minor sanity size check (remember sizeof lwpstatus_t & lwpsinfo_t is <= size in file per entry).
             */
            if (   (cbStatusHdrAndData - sizeof(prheader_t)) % pStatusHdr->pr_entsize == 0
                && (cbInfoHdrAndData - sizeof(prheader_t)) % pInfoHdr->pr_entsize == 0)
            {
                /*
                 * Make sure we have a matching lstatus entry for an lpsinfo entry unless
                 * it is a zombie thread, in which case we will not have a matching lstatus entry.
                 */
                for (; cInfo != 0; cInfo--)
                {
                    if (pInfo->pr_sname != 'Z') /* zombie */
                    {
                        if (   cStatus == 0
                            || pStatus->pr_lwpid != pInfo->pr_lwpid)
                        {
                            CORELOGRELSYS((CORELOG_NAME "ProcReadThreads: cStatus = %u pStatuslwpid=%d infolwpid=%d\n", cStatus,
                                        pStatus->pr_lwpid, pInfo->pr_lwpid));
                            rc = VERR_INVALID_STATE;
                            break;
                        }
                        pStatus = (lwpstatus_t *)((uintptr_t)pStatus + pStatusHdr->pr_entsize);
                        cStatus--;
                    }
                    pInfo = (lwpsinfo_t *)((uintptr_t)pInfo + pInfoHdr->pr_entsize);
                }

                if (RT_SUCCESS(rc))
                {
                    /*
                     * There can still be more lwpsinfo_t's than lwpstatus_t's, build the
                     * lists accordingly.
                     */
                    pStatus = (lwpstatus_t *)((uintptr_t)pStatusHdr + sizeof(prheader_t));
                    pInfo = (lwpsinfo_t *)((uintptr_t)pInfoHdr + sizeof(prheader_t));
                    cInfo = pInfoHdr->pr_nent;
                    cStatus = pInfoHdr->pr_nent;

                    size_t cbThreadInfo = RT_MAX(cStatus, cInfo) * sizeof(VBOXSOLTHREADINFO);
                    pVBoxProc->pThreadInfoHead = (PVBOXSOLTHREADINFO)GetMemoryChunk(pVBoxCore, cbThreadInfo);
                    if (pVBoxProc->pThreadInfoHead)
                    {
                        PVBOXSOLTHREADINFO pCur = pVBoxProc->pThreadInfoHead;
                        PVBOXSOLTHREADINFO pPrev = NULL;
                        for (uint64_t i = 0; i < cInfo; i++, pCur++)
                        {
                            pCur->Info = *pInfo;
                            if (   pInfo->pr_sname != 'Z'
                                && pInfo->pr_lwpid == pStatus->pr_lwpid)
                            {
                                /*
                                 * Adjust the context of the dumping thread to reflect the context
                                 * when the core dump got initiated before whatever signal caused it.
                                 */
                                if (   pStatus          /* noid droid */
                                    && pStatus->pr_lwpid == (id_t)pVBoxProc->hCurThread)
                                {
                                    AssertCompile(sizeof(pStatus->pr_reg) == sizeof(pVBoxProc->pCurThreadCtx->uc_mcontext.gregs));
                                    AssertCompile(sizeof(pStatus->pr_fpreg) == sizeof(pVBoxProc->pCurThreadCtx->uc_mcontext.fpregs));
                                    memcpy(&pStatus->pr_reg, &pVBoxProc->pCurThreadCtx->uc_mcontext.gregs, sizeof(pStatus->pr_reg));
                                    memcpy(&pStatus->pr_fpreg, &pVBoxProc->pCurThreadCtx->uc_mcontext.fpregs, sizeof(pStatus->pr_fpreg));

                                    AssertCompile(sizeof(pStatus->pr_lwphold) == sizeof(pVBoxProc->pCurThreadCtx->uc_sigmask));
                                    memcpy(&pStatus->pr_lwphold, &pVBoxProc->pCurThreadCtx->uc_sigmask, sizeof(pStatus->pr_lwphold));
                                    pStatus->pr_ustack = (uintptr_t)&pVBoxProc->pCurThreadCtx->uc_stack;

                                    CORELOG((CORELOG_NAME "ProcReadThreads: patched dumper thread context with pre-dump time context.\n"));
                                }

                                pCur->pStatus = pStatus;
                                pStatus = (lwpstatus_t *)((uintptr_t)pStatus + pStatusHdr->pr_entsize);
                            }
                            else
                            {
                                CORELOGRELSYS((CORELOG_NAME "ProcReadThreads: missing status for lwp %d\n", pInfo->pr_lwpid));
                                pCur->pStatus = NULL;
                            }

                            if (pPrev)
                                pPrev->pNext = pCur;
                            pPrev = pCur;
                            pInfo = (lwpsinfo_t *)((uintptr_t)pInfo + pInfoHdr->pr_entsize);
                        }
                        if (pPrev)
                            pPrev->pNext = NULL;

                        CORELOG((CORELOG_NAME "ProcReadThreads: successfully read %u threads.\n", cInfo));
                        pVBoxProc->cThreads = cInfo;
                        return VINF_SUCCESS;
                    }
                    else
                    {
                        CORELOGRELSYS((CORELOG_NAME "ProcReadThreads: GetMemoryChunk failed for %u bytes\n", cbThreadInfo));
                        rc = VERR_NO_MEMORY;
                    }
                }
                else
                    CORELOGRELSYS((CORELOG_NAME "ProcReadThreads: Invalid state information for threads. rc=%Rrc\n", rc));
            }
            else
            {
                CORELOGRELSYS((CORELOG_NAME "ProcReadThreads: huh!? cbStatusHdrAndData=%u prheader_t=%u entsize=%u\n", cbStatusHdrAndData,
                            sizeof(prheader_t), pStatusHdr->pr_entsize));
                CORELOGRELSYS((CORELOG_NAME "ProcReadThreads: huh!? cbInfoHdrAndData=%u entsize=%u\n", cbInfoHdrAndData,
                               pStatusHdr->pr_entsize));
                rc = VERR_INVALID_STATE;
            }
        }
        else
            CORELOGRELSYS((CORELOG_NAME "ProcReadThreads: ReadFileNoIntr failed for \"lpsinfo\" rc=%Rrc\n", rc));
    }
    else
        CORELOGRELSYS((CORELOG_NAME "ProcReadThreads: ReadFileNoIntr failed for \"lstatus\" rc=%Rrc\n", rc));
    return rc;
}


/**
 * Reads miscellaneous information that is collected as part of a core file.
 * This may include platform name, zone name and other OS-specific information.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @return IPRT status code.
 */
static int ProcReadMiscInfo(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;

#ifdef RT_OS_SOLARIS
    /*
     * Read the platform name, uname string and zone name.
     */
    int rc = sysinfo(SI_PLATFORM, pVBoxProc->szPlatform, sizeof(pVBoxProc->szPlatform));
    if (rc == -1)
    {
        CORELOGRELSYS((CORELOG_NAME "ProcReadMiscInfo: sysinfo failed. rc=%d errno=%d\n", rc, errno));
        return VERR_GENERAL_FAILURE;
    }
    pVBoxProc->szPlatform[sizeof(pVBoxProc->szPlatform) - 1] = '\0';

    rc = uname(&pVBoxProc->UtsName);
    if (rc == -1)
    {
        CORELOGRELSYS((CORELOG_NAME "ProcReadMiscInfo: uname failed. rc=%d errno=%d\n", rc, errno));
        return VERR_GENERAL_FAILURE;
    }

    rc = getzonenamebyid(pVBoxProc->ProcInfo.pr_zoneid, pVBoxProc->szZoneName, sizeof(pVBoxProc->szZoneName));
    if (rc < 0)
    {
        CORELOGRELSYS((CORELOG_NAME "ProcReadMiscInfo: getzonenamebyid failed. rc=%d errno=%d zoneid=%d\n", rc, errno,
                       pVBoxProc->ProcInfo.pr_zoneid));
        return VERR_GENERAL_FAILURE;
    }
    pVBoxProc->szZoneName[sizeof(pVBoxProc->szZoneName) - 1] = '\0';
    rc = VINF_SUCCESS;

#else
# error Port Me!
#endif
    return rc;
}


/**
 * On Solaris use the old-style procfs interfaces but the core file still should have this
 * info. for backward and GDB compatibility, hence the need for this ugly function.
 *
 * @param pVBoxCore         Pointer to the core object.
 * @param pInfo             Pointer to the old prpsinfo_t structure to update.
 */
static void GetOldProcessInfo(PVBOXCORE pVBoxCore, prpsinfo_t *pInfo)
{
    AssertReturnVoid(pVBoxCore);
    AssertReturnVoid(pInfo);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    psinfo_t *pSrc = &pVBoxProc->ProcInfo;
    memset(pInfo, 0, sizeof(prpsinfo_t));
    pInfo->pr_state    = pSrc->pr_lwp.pr_state;
    pInfo->pr_zomb     = (pInfo->pr_state == SZOMB);
    RTStrCopy(pInfo->pr_clname, sizeof(pInfo->pr_clname), pSrc->pr_lwp.pr_clname);
    RTStrCopy(pInfo->pr_fname, sizeof(pInfo->pr_fname), pSrc->pr_fname);
    memcpy(&pInfo->pr_psargs, &pSrc->pr_psargs, sizeof(pInfo->pr_psargs));
    pInfo->pr_nice     = pSrc->pr_lwp.pr_nice;
    pInfo->pr_flag     = pSrc->pr_lwp.pr_flag;
    pInfo->pr_uid      = pSrc->pr_uid;
    pInfo->pr_gid      = pSrc->pr_gid;
    pInfo->pr_pid      = pSrc->pr_pid;
    pInfo->pr_ppid     = pSrc->pr_ppid;
    pInfo->pr_pgrp     = pSrc->pr_pgid;
    pInfo->pr_sid      = pSrc->pr_sid;
    pInfo->pr_addr     = (caddr_t)pSrc->pr_addr;
    pInfo->pr_size     = pSrc->pr_size;
    pInfo->pr_rssize   = pSrc->pr_rssize;
    pInfo->pr_wchan    = (caddr_t)pSrc->pr_lwp.pr_wchan;
    pInfo->pr_start    = pSrc->pr_start;
    pInfo->pr_time     = pSrc->pr_time;
    pInfo->pr_pri      = pSrc->pr_lwp.pr_pri;
    pInfo->pr_oldpri   = pSrc->pr_lwp.pr_oldpri;
    pInfo->pr_cpu      = pSrc->pr_lwp.pr_cpu;
    pInfo->pr_ottydev  = cmpdev(pSrc->pr_ttydev);
    pInfo->pr_lttydev  = pSrc->pr_ttydev;
    pInfo->pr_syscall  = pSrc->pr_lwp.pr_syscall;
    pInfo->pr_ctime    = pSrc->pr_ctime;
    pInfo->pr_bysize   = pSrc->pr_size * PAGESIZE;
    pInfo->pr_byrssize = pSrc->pr_rssize * PAGESIZE;
    pInfo->pr_argc     = pSrc->pr_argc;
    pInfo->pr_argv     = (char **)pSrc->pr_argv;
    pInfo->pr_envp     = (char **)pSrc->pr_envp;
    pInfo->pr_wstat    = pSrc->pr_wstat;
    pInfo->pr_pctcpu   = pSrc->pr_pctcpu;
    pInfo->pr_pctmem   = pSrc->pr_pctmem;
    pInfo->pr_euid     = pSrc->pr_euid;
    pInfo->pr_egid     = pSrc->pr_egid;
    pInfo->pr_aslwpid  = 0;
    pInfo->pr_dmodel   = pSrc->pr_dmodel;
}


/**
 * On Solaris use the old-style procfs interfaces but the core file still should have this
 * info. for backward and GDB compatibility, hence the need for this ugly function.
 *
 * @param pVBoxCore         Pointer to the core object.
 * @param pInfo             Pointer to the thread info.
 * @param pStatus           Pointer to the thread status.
 * @param pDst              Pointer to the old-style status structure to update.
 *
 */
static void GetOldProcessStatus(PVBOXCORE pVBoxCore, lwpsinfo_t *pInfo, lwpstatus_t *pStatus, prstatus_t *pDst)
{
    AssertReturnVoid(pVBoxCore);
    AssertReturnVoid(pInfo);
    AssertReturnVoid(pStatus);
    AssertReturnVoid(pDst);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    memset(pDst, 0, sizeof(prstatus_t));
    if (pStatus->pr_flags & PR_STOPPED)
        pDst->pr_flags = 0x0001;
    if (pStatus->pr_flags & PR_ISTOP)
        pDst->pr_flags = 0x0002;
    if (pStatus->pr_flags & PR_DSTOP)
        pDst->pr_flags = 0x0004;
    if (pStatus->pr_flags & PR_ASLEEP)
        pDst->pr_flags = 0x0008;
    if (pStatus->pr_flags & PR_FORK)
        pDst->pr_flags = 0x0010;
    if (pStatus->pr_flags & PR_RLC)
        pDst->pr_flags = 0x0020;
    /* PR_PTRACE is never set */
    if (pStatus->pr_flags & PR_PCINVAL)
        pDst->pr_flags = 0x0080;
    if (pStatus->pr_flags & PR_ISSYS)
        pDst->pr_flags = 0x0100;
    if (pStatus->pr_flags & PR_STEP)
        pDst->pr_flags = 0x0200;
    if (pStatus->pr_flags & PR_KLC)
        pDst->pr_flags = 0x0400;
    if (pStatus->pr_flags & PR_ASYNC)
        pDst->pr_flags = 0x0800;
    if (pStatus->pr_flags & PR_PTRACE)
        pDst->pr_flags = 0x1000;
    if (pStatus->pr_flags & PR_MSACCT)
        pDst->pr_flags = 0x2000;
    if (pStatus->pr_flags & PR_BPTADJ)
        pDst->pr_flags = 0x4000;
    if (pStatus->pr_flags & PR_ASLWP)
        pDst->pr_flags = 0x8000;

    pDst->pr_who        = pStatus->pr_lwpid;
    pDst->pr_why        = pStatus->pr_why;
    pDst->pr_what       = pStatus->pr_what;
    pDst->pr_info       = pStatus->pr_info;
    pDst->pr_cursig     = pStatus->pr_cursig;
    pDst->pr_sighold    = pStatus->pr_lwphold;
    pDst->pr_altstack   = pStatus->pr_altstack;
    pDst->pr_action     = pStatus->pr_action;
    pDst->pr_syscall    = pStatus->pr_syscall;
    pDst->pr_nsysarg    = pStatus->pr_nsysarg;
    pDst->pr_lwppend    = pStatus->pr_lwppend;
    pDst->pr_oldcontext = (ucontext_t *)pStatus->pr_oldcontext;
    memcpy(pDst->pr_reg, pStatus->pr_reg, sizeof(pDst->pr_reg));
    memcpy(pDst->pr_sysarg, pStatus->pr_sysarg, sizeof(pDst->pr_sysarg));
    RTStrCopy(pDst->pr_clname, sizeof(pDst->pr_clname), pStatus->pr_clname);

    pDst->pr_nlwp       = pVBoxProc->ProcStatus.pr_nlwp;
    pDst->pr_sigpend    = pVBoxProc->ProcStatus.pr_sigpend;
    pDst->pr_pid        = pVBoxProc->ProcStatus.pr_pid;
    pDst->pr_ppid       = pVBoxProc->ProcStatus.pr_ppid;
    pDst->pr_pgrp       = pVBoxProc->ProcStatus.pr_pgid;
    pDst->pr_sid        = pVBoxProc->ProcStatus.pr_sid;
    pDst->pr_utime      = pVBoxProc->ProcStatus.pr_utime;
    pDst->pr_stime      = pVBoxProc->ProcStatus.pr_stime;
    pDst->pr_cutime     = pVBoxProc->ProcStatus.pr_cutime;
    pDst->pr_cstime     = pVBoxProc->ProcStatus.pr_cstime;
    pDst->pr_brkbase    = (caddr_t)pVBoxProc->ProcStatus.pr_brkbase;
    pDst->pr_brksize    = pVBoxProc->ProcStatus.pr_brksize;
    pDst->pr_stkbase    = (caddr_t)pVBoxProc->ProcStatus.pr_stkbase;
    pDst->pr_stksize    = pVBoxProc->ProcStatus.pr_stksize;

    pDst->pr_processor  = (short)pInfo->pr_onpro;
    pDst->pr_bind       = (short)pInfo->pr_bindpro;
    pDst->pr_instr      = pStatus->pr_instr;
}


/**
 * Callback for rtCoreDumperForEachThread to suspend a thread.
 *
 * @param pVBoxCore             Pointer to the core object.
 * @param pvThreadInfo          Opaque pointer to thread information.
 *
 * @return IPRT status code.
 */
static int suspendThread(PVBOXCORE pVBoxCore, void *pvThreadInfo)
{
    AssertPtrReturn(pvThreadInfo, VERR_INVALID_POINTER);
    NOREF(pVBoxCore);

    lwpsinfo_t *pThreadInfo = (lwpsinfo_t *)pvThreadInfo;
    CORELOG((CORELOG_NAME ":suspendThread %d\n", (lwpid_t)pThreadInfo->pr_lwpid));
    if ((lwpid_t)pThreadInfo->pr_lwpid != pVBoxCore->VBoxProc.hCurThread)
        _lwp_suspend(pThreadInfo->pr_lwpid);
    return VINF_SUCCESS;
}


/**
 * Callback for rtCoreDumperForEachThread to resume a thread.
 *
 * @param pVBoxCore             Pointer to the core object.
 * @param pvThreadInfo          Opaque pointer to thread information.
 *
 * @return IPRT status code.
 */
static int resumeThread(PVBOXCORE pVBoxCore, void *pvThreadInfo)
{
    AssertPtrReturn(pvThreadInfo, VERR_INVALID_POINTER);
    NOREF(pVBoxCore);

    lwpsinfo_t *pThreadInfo = (lwpsinfo_t *)pvThreadInfo;
    CORELOG((CORELOG_NAME ":resumeThread %d\n", (lwpid_t)pThreadInfo->pr_lwpid));
    if ((lwpid_t)pThreadInfo->pr_lwpid != (lwpid_t)pVBoxCore->VBoxProc.hCurThread)
        _lwp_continue(pThreadInfo->pr_lwpid);
    return VINF_SUCCESS;
}


/**
 * Calls a thread worker function for all threads in the process as described by /proc
 *
 * @param pVBoxCore             Pointer to the core object.
 * @param pcThreads             Number of threads read.
 * @param pfnWorker             Callback function for each thread.
 *
 * @return IPRT status code.
 */
static int rtCoreDumperForEachThread(PVBOXCORE pVBoxCore,  uint64_t *pcThreads, PFNCORETHREADWORKER pfnWorker)
{
    AssertPtrReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;

    /*
     * Read the information for threads.
     * Format: prheader_t + array of lwpsinfo_t's.
     */
    char szLpsInfoPath[PATH_MAX];
    RTStrPrintf(szLpsInfoPath, sizeof(szLpsInfoPath), "/proc/%d/lpsinfo", (int)pVBoxProc->Process);

    int rc = VINF_SUCCESS;
    int fd = open(szLpsInfoPath, O_RDONLY);
    if (fd >= 0)
    {
        RTFILE hFile = (RTFILE)(uintptr_t)fd;
        uint64_t u64Size;
        RTFileGetSize(hFile, &u64Size);
        size_t cbInfoHdrAndData = u64Size < ~(size_t)0 ? u64Size : ~(size_t)0;
        void *pvInfoHdr = mmap(NULL, cbInfoHdrAndData, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1 /* fd */, 0 /* offset */);
        if (pvInfoHdr != MAP_FAILED)
        {
            rc = RTFileRead(hFile, pvInfoHdr, cbInfoHdrAndData, NULL);
            if (RT_SUCCESS(rc))
            {
                prheader_t *pHeader = (prheader_t *)pvInfoHdr;
                lwpsinfo_t *pThreadInfo = (lwpsinfo_t *)((uintptr_t)pvInfoHdr + sizeof(prheader_t));
                for (long i = 0; i < pHeader->pr_nent; i++)
                {
                    pfnWorker(pVBoxCore, pThreadInfo);
                    pThreadInfo = (lwpsinfo_t *)((uintptr_t)pThreadInfo + pHeader->pr_entsize);
                }
                if (pcThreads)
                    *pcThreads = pHeader->pr_nent;
            }

            munmap(pvInfoHdr, cbInfoHdrAndData);
        }
        else
            rc = VERR_NO_MEMORY;
        RTFileClose(hFile);
    }
    else
        rc = RTErrConvertFromErrno(rc);

    return rc;
}


/**
 * Resume all threads of this process.
 *
 * @param pVBoxCore             Pointer to the core object.
 *
 * @return IPRT status code..
 */
static int rtCoreDumperResumeThreads(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

#if 1
    uint64_t cThreads;
    return rtCoreDumperForEachThread(pVBoxCore, &cThreads, resumeThread);
#else
    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;

    char szCurThread[128];
    char szPath[PATH_MAX];
    PRTDIR pDir = NULL;

    RTStrPrintf(szPath, sizeof(szPath), "/proc/%d/lwp", (int)pVBoxProc->Process);
    RTStrPrintf(szCurThread, sizeof(szCurThread), "%d", (int)pVBoxProc->hCurThread);

    int32_t cRunningThreads = 0;
    int rc = RTDirOpen(&pDir, szPath);
    if (RT_SUCCESS(rc))
    {
        /*
         * Loop through all our threads & resume them.
         */
        RTDIRENTRY DirEntry;
        while (RT_SUCCESS(RTDirRead(pDir, &DirEntry, NULL)))
        {
            if (   !strcmp(DirEntry.szName, ".")
                || !strcmp(DirEntry.szName, ".."))
                continue;

            if ( !strcmp(DirEntry.szName, szCurThread))
                continue;

            int32_t ThreadId = RTStrToInt32(DirEntry.szName);
            _lwp_continue((lwpid_t)ThreadId);
            ++cRunningThreads;
        }

        CORELOG((CORELOG_NAME "ResumeAllThreads: resumed %d threads\n", cRunningThreads));
        RTDirClose(pDir);
    }
    else
    {
        CORELOGRELSYS((CORELOG_NAME "ResumeAllThreads: Failed to open %s\n", szPath));
        rc = VERR_READ_ERROR;
    }
    return rc;
#endif
}


/**
 * Stop all running threads of this process except the current one.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @return IPRT status code.
 */
static int rtCoreDumperSuspendThreads(PVBOXCORE pVBoxCore)
{
    AssertPtrReturn(pVBoxCore, VERR_INVALID_POINTER);

    /*
     * This function tries to ensures while we suspend threads, no newly spawned threads
     * or a combination of spawning and terminating threads can cause any threads to be left running.
     * The assumption here is that threads can only increase not decrease across iterations.
     */
#if 1
    uint16_t cTries = 0;
    uint64_t aThreads[4];
    RT_ZERO(aThreads);
    int rc = VERR_GENERAL_FAILURE;
    void *pv = NULL;
    size_t cb = 0;
    for (cTries = 0; cTries < RT_ELEMENTS(aThreads); cTries++)
    {
        rc = rtCoreDumperForEachThread(pVBoxCore, &aThreads[cTries], suspendThread);
        if (RT_FAILURE(rc))
            break;
    }
    if (   RT_SUCCESS(rc)
        && aThreads[cTries - 1] != aThreads[cTries - 2])
    {
        CORELOGRELSYS((CORELOG_NAME "rtCoreDumperSuspendThreads: possible thread bomb!?\n"));
        rc = VERR_TIMEOUT;
    }
    return rc;
#else
    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;

    char szCurThread[128];
    char szPath[PATH_MAX];
    PRTDIR pDir = NULL;

    RTStrPrintf(szPath, sizeof(szPath), "/proc/%d/lwp", (int)pVBoxProc->Process);
    RTStrPrintf(szCurThread, sizeof(szCurThread), "%d", (int)pVBoxProc->hCurThread);

    int rc = -1;
    uint32_t cThreads = 0;
    uint16_t cTries = 0;
    for (cTries = 0; cTries < 10; cTries++)
    {
        uint32_t cRunningThreads = 0;
        rc = RTDirOpen(&pDir, szPath);
        if (RT_SUCCESS(rc))
        {
            /*
             * Loop through all our threads & suspend them, multiple calls to _lwp_suspend() are okay.
             */
            RTDIRENTRY DirEntry;
            while (RT_SUCCESS(RTDirRead(pDir, &DirEntry, NULL)))
            {
                if (   !strcmp(DirEntry.szName, ".")
                    || !strcmp(DirEntry.szName, ".."))
                    continue;

                if ( !strcmp(DirEntry.szName, szCurThread))
                    continue;

                int32_t ThreadId = RTStrToInt32(DirEntry.szName);
                _lwp_suspend((lwpid_t)ThreadId);
                ++cRunningThreads;
            }

            if (cTries > 5 && cThreads == cRunningThreads)
            {
                rc = VINF_SUCCESS;
                break;
            }
            cThreads = cRunningThreads;
            RTDirClose(pDir);
        }
        else
        {
            CORELOGRELSYS((CORELOG_NAME "SuspendThreads: Failed to open %s cTries=%d\n", szPath, cTries));
            rc = VERR_READ_ERROR;
            break;
        }
    }

    if (RT_SUCCESS(rc))
        CORELOG((CORELOG_NAME "SuspendThreads: Stopped %u threads successfully with %u tries\n", cThreads, cTries));

    return rc;
#endif
}


/**
 * Returns size of an ELF NOTE header given the size of data the NOTE section will contain.
 *
 * @param cb                Size of the data.
 *
 * @return Size of data actually used for NOTE header and section.
 */
static inline size_t ElfNoteHeaderSize(size_t cb)
{
    return sizeof(ELFNOTEHDR) + RT_ALIGN_Z(cb, 4);
}


/**
 * Write an ELF NOTE header into the core file.
 *
 * @param pVBoxCore         Pointer to the core object.
 * @param Type              Type of this NOTE section.
 * @param pcv               Opaque pointer to the data, if NULL only computes size.
 * @param cb                Size of the data.
 *
 * @return IPRT status code.
 */
static int ElfWriteNoteHeader(PVBOXCORE pVBoxCore, uint_t Type, const void *pcv, size_t cb)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);
    AssertReturn(pcv, VERR_INVALID_POINTER);
    AssertReturn(cb > 0, VERR_NO_DATA);
    AssertReturn(pVBoxCore->pfnWriter, VERR_WRITE_ERROR);
    AssertReturn(pVBoxCore->hCoreFile, VERR_INVALID_HANDLE);

    int rc = VERR_GENERAL_FAILURE;
#ifdef RT_OS_SOLARIS
    ELFNOTEHDR ElfNoteHdr;
    RT_ZERO(ElfNoteHdr);
    ElfNoteHdr.achName[0] = 'C';
    ElfNoteHdr.achName[1] = 'O';
    ElfNoteHdr.achName[2] = 'R';
    ElfNoteHdr.achName[3] = 'E';

    /*
     * This is a known violation of the 64-bit ELF spec., see xTracker #5211 comment#3
     * for the historic reasons as to the padding and namesz anomalies.
     */
    static const char s_achPad[3] = { 0, 0, 0 };
    size_t cbAlign = RT_ALIGN_Z(cb, 4);
    ElfNoteHdr.Hdr.n_namesz = 5;
    ElfNoteHdr.Hdr.n_type = Type;
    ElfNoteHdr.Hdr.n_descsz = cbAlign;

    /*
     * Write note header and description.
     */
    rc = pVBoxCore->pfnWriter(pVBoxCore->hCoreFile, &ElfNoteHdr, sizeof(ElfNoteHdr));
    if (RT_SUCCESS(rc))
    {
       rc = pVBoxCore->pfnWriter(pVBoxCore->hCoreFile, pcv, cb);
       if (RT_SUCCESS(rc))
       {
           if (cbAlign > cb)
               rc = pVBoxCore->pfnWriter(pVBoxCore->hCoreFile, s_achPad, cbAlign - cb);
       }
    }

    if (RT_FAILURE(rc))
        CORELOGRELSYS((CORELOG_NAME "ElfWriteNote: pfnWriter failed. Type=%d rc=%Rrc\n", Type, rc));
#else
#error Port Me!
#endif
    return rc;
}


/**
 * Computes the size of NOTE section for the given core type.
 * Solaris has two types of program header information (new and old).
 *
 * @param pVBoxCore         Pointer to the core object.
 * @param enmType           Type of core file information required.
 *
 * @return Size of NOTE section.
 */
static size_t ElfNoteSectionSize(PVBOXCORE pVBoxCore, VBOXSOLCORETYPE enmType)
{
    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    size_t cb = 0;
    switch (enmType)
    {
        case enmOldEra:
        {
            cb += ElfNoteHeaderSize(sizeof(prpsinfo_t));
            cb += ElfNoteHeaderSize(pVBoxProc->cAuxVecs * sizeof(auxv_t));
            cb += ElfNoteHeaderSize(strlen(pVBoxProc->szPlatform));

            PVBOXSOLTHREADINFO pThreadInfo = pVBoxProc->pThreadInfoHead;
            while (pThreadInfo)
            {
                if (pThreadInfo->pStatus)
                {
                    cb += ElfNoteHeaderSize(sizeof(prstatus_t));
                    cb += ElfNoteHeaderSize(sizeof(prfpregset_t));
                }
                pThreadInfo = pThreadInfo->pNext;
            }

            break;
        }

        case enmNewEra:
        {
            cb += ElfNoteHeaderSize(sizeof(psinfo_t));
            cb += ElfNoteHeaderSize(sizeof(pstatus_t));
            cb += ElfNoteHeaderSize(pVBoxProc->cAuxVecs * sizeof(auxv_t));
            cb += ElfNoteHeaderSize(strlen(pVBoxProc->szPlatform) + 1);
            cb += ElfNoteHeaderSize(sizeof(struct utsname));
            cb += ElfNoteHeaderSize(sizeof(core_content_t));
            cb += ElfNoteHeaderSize(pVBoxProc->cbCred);

            if (pVBoxProc->pPriv)
                cb += ElfNoteHeaderSize(PRIV_PRPRIV_SIZE(pVBoxProc->pPriv));   /* Ought to be same as cbPriv!? */

            if (pVBoxProc->pcPrivImpl)
                cb += ElfNoteHeaderSize(PRIV_IMPL_INFO_SIZE(pVBoxProc->pcPrivImpl));

            cb += ElfNoteHeaderSize(strlen(pVBoxProc->szZoneName) + 1);
            if (pVBoxProc->cbLdt > 0)
                cb += ElfNoteHeaderSize(pVBoxProc->cbLdt);

            PVBOXSOLTHREADINFO pThreadInfo = pVBoxProc->pThreadInfoHead;
            while (pThreadInfo)
            {
                cb += ElfNoteHeaderSize(sizeof(lwpsinfo_t));
                if (pThreadInfo->pStatus)
                    cb += ElfNoteHeaderSize(sizeof(lwpstatus_t));

                pThreadInfo = pThreadInfo->pNext;
            }

            break;
        }

        default:
        {
            CORELOGRELSYS((CORELOG_NAME "ElfNoteSectionSize: Unknown segment era %d\n", enmType));
            break;
        }
    }

    return cb;
}


/**
 * Write the note section for the given era into the core file.
 * Solaris has two types of program  header information (new and old).
 *
 * @param pVBoxCore         Pointer to the core object.
 * @param enmType           Type of core file information required.
 *
 * @return IPRT status code.
 */
static int ElfWriteNoteSection(PVBOXCORE pVBoxCore, VBOXSOLCORETYPE enmType)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    int rc = VERR_GENERAL_FAILURE;

#ifdef RT_OS_SOLARIS
    typedef int (*PFNELFWRITENOTEHDR)(PVBOXCORE pVBoxCore, uint_t, const void *pcv, size_t cb);
    typedef struct ELFWRITENOTE
    {
        const char        *pszType;
        uint_t             Type;
        const void        *pcv;
        size_t             cb;
    } ELFWRITENOTE;

    switch (enmType)
    {
        case enmOldEra:
        {
            ELFWRITENOTE aElfNotes[] =
            {
                { "NT_PRPSINFO", NT_PRPSINFO, &pVBoxProc->ProcInfoOld,  sizeof(prpsinfo_t) },
                { "NT_AUXV",     NT_AUXV,      pVBoxProc->pAuxVecs,      pVBoxProc->cAuxVecs * sizeof(auxv_t) },
                { "NT_PLATFORM", NT_PLATFORM,  pVBoxProc->szPlatform,    strlen(pVBoxProc->szPlatform) + 1 }
            };

            for (unsigned i = 0; i < RT_ELEMENTS(aElfNotes); i++)
            {
                rc = ElfWriteNoteHeader(pVBoxCore, aElfNotes[i].Type, aElfNotes[i].pcv, aElfNotes[i].cb);
                if (RT_FAILURE(rc))
                {
                    CORELOGRELSYS((CORELOG_NAME "ElfWriteNoteSection: ElfWriteNoteHeader failed for %s. rc=%Rrc\n", aElfNotes[i].pszType, rc));
                    break;
                }
            }

            /*
             * Write old-style thread info., they contain nothing about zombies,
             * so we just skip if there is no status information for them.
             */
            PVBOXSOLTHREADINFO pThreadInfo = pVBoxProc->pThreadInfoHead;
            for (; pThreadInfo; pThreadInfo = pThreadInfo->pNext)
            {
                if (!pThreadInfo->pStatus)
                    continue;

                prstatus_t OldProcessStatus;
                GetOldProcessStatus(pVBoxCore, &pThreadInfo->Info, pThreadInfo->pStatus, &OldProcessStatus);
                rc = ElfWriteNoteHeader(pVBoxCore, NT_PRSTATUS, &OldProcessStatus, sizeof(prstatus_t));
                if (RT_SUCCESS(rc))
                {
                    rc = ElfWriteNoteHeader(pVBoxCore, NT_PRFPREG, &pThreadInfo->pStatus->pr_fpreg, sizeof(prfpregset_t));
                    if (RT_FAILURE(rc))
                    {
                        CORELOGRELSYS((CORELOG_NAME "ElfWriteSegment: ElfWriteNote failed for NT_PRFPREF. rc=%Rrc\n", rc));
                        break;
                    }
                }
                else
                {
                    CORELOGRELSYS((CORELOG_NAME "ElfWriteSegment: ElfWriteNote failed for NT_PRSTATUS. rc=%Rrc\n", rc));
                    break;
                }
            }
            break;
        }

        case enmNewEra:
        {
            ELFWRITENOTE aElfNotes[] =
            {
                { "NT_PSINFO",     NT_PSINFO,     &pVBoxProc->ProcInfo,     sizeof(psinfo_t) },
                { "NT_PSTATUS",    NT_PSTATUS,    &pVBoxProc->ProcStatus,   sizeof(pstatus_t) },
                { "NT_AUXV",       NT_AUXV,        pVBoxProc->pAuxVecs,     pVBoxProc->cAuxVecs * sizeof(auxv_t) },
                { "NT_PLATFORM",   NT_PLATFORM,    pVBoxProc->szPlatform,   strlen(pVBoxProc->szPlatform) + 1 },
                { "NT_UTSNAME",    NT_UTSNAME,    &pVBoxProc->UtsName,      sizeof(struct utsname) },
                { "NT_CONTENT",    NT_CONTENT,    &pVBoxProc->CoreContent,  sizeof(core_content_t) },
                { "NT_PRCRED",     NT_PRCRED,      pVBoxProc->pvCred,       pVBoxProc->cbCred },
                { "NT_PRPRIV",     NT_PRPRIV,      pVBoxProc->pPriv,        PRIV_PRPRIV_SIZE(pVBoxProc->pPriv) },
                { "NT_PRPRIVINFO", NT_PRPRIVINFO,  pVBoxProc->pcPrivImpl,   PRIV_IMPL_INFO_SIZE(pVBoxProc->pcPrivImpl) },
                { "NT_ZONENAME",   NT_ZONENAME,    pVBoxProc->szZoneName,   strlen(pVBoxProc->szZoneName) + 1 }
            };

            for (unsigned i = 0; i < RT_ELEMENTS(aElfNotes); i++)
            {
                rc = ElfWriteNoteHeader(pVBoxCore, aElfNotes[i].Type, aElfNotes[i].pcv, aElfNotes[i].cb);
                if (RT_FAILURE(rc))
                {
                    CORELOGRELSYS((CORELOG_NAME "ElfWriteNoteSection: ElfWriteNoteHeader failed for %s. rc=%Rrc\n", aElfNotes[i].pszType, rc));
                    break;
                }
            }

            /*
             * Write new-style thread info., missing lwpstatus_t indicates it's a zombie thread
             * we only dump the lwpsinfo_t in that case.
             */
            PVBOXSOLTHREADINFO pThreadInfo = pVBoxProc->pThreadInfoHead;
            for (; pThreadInfo; pThreadInfo = pThreadInfo->pNext)
            {
                rc = ElfWriteNoteHeader(pVBoxCore, NT_LWPSINFO, &pThreadInfo->Info, sizeof(lwpsinfo_t));
                if (RT_FAILURE(rc))
                {
                    CORELOGRELSYS((CORELOG_NAME "ElfWriteNoteSection: ElfWriteNoteHeader for NT_LWPSINFO failed. rc=%Rrc\n", rc));
                    break;
                }

                if (pThreadInfo->pStatus)
                {
                    rc = ElfWriteNoteHeader(pVBoxCore, NT_LWPSTATUS, pThreadInfo->pStatus, sizeof(lwpstatus_t));
                    if (RT_FAILURE(rc))
                    {
                        CORELOGRELSYS((CORELOG_NAME "ElfWriteNoteSection: ElfWriteNoteHeader for NT_LWPSTATUS failed. rc=%Rrc\n", rc));
                        break;
                    }
                }
            }
            break;
        }

        default:
        {
            CORELOGRELSYS((CORELOG_NAME "ElfWriteNoteSection: Invalid type %d\n", enmType));
            rc = VERR_GENERAL_FAILURE;
            break;
        }
    }
#else
# error Port Me!
#endif
    return rc;
}


/**
 * Write mappings into the core file.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @return IPRT status code.
 */
static int ElfWriteMappings(PVBOXCORE pVBoxCore)
{
    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;

    int rc = VERR_GENERAL_FAILURE;
    PVBOXSOLMAPINFO pMapInfo = pVBoxProc->pMapInfoHead;
    while (pMapInfo)
    {
        if (!pMapInfo->fError)
        {
            uint64_t k = 0;
            char achBuf[PAGE_SIZE];
            while (k < pMapInfo->pMap.pr_size)
            {
                size_t cb = RT_MIN(sizeof(achBuf), pMapInfo->pMap.pr_size - k);
                int rc2 = ProcReadAddrSpace(pVBoxProc, pMapInfo->pMap.pr_vaddr + k, &achBuf, cb);
                if (RT_FAILURE(rc2))
                {
                    CORELOGRELSYS((CORELOG_NAME "ElfWriteMappings: Failed to read mapping, can't recover. Bye. rc=%Rrc\n", rc));
                    return VERR_INVALID_STATE;
                }

                rc = pVBoxCore->pfnWriter(pVBoxCore->hCoreFile, achBuf, sizeof(achBuf));
                if (RT_FAILURE(rc))
                {
                    CORELOGRELSYS((CORELOG_NAME "ElfWriteMappings: pfnWriter failed. rc=%Rrc\n", rc));
                    return rc;
                }
                k += cb;
            }
        }
        else
        {
            char achBuf[RT_ALIGN_Z(sizeof(int), 8)];
            RT_ZERO(achBuf);
            memcpy(achBuf, &pMapInfo->fError, sizeof(pMapInfo->fError));
            if (sizeof(achBuf) != pMapInfo->pMap.pr_size)
                CORELOGRELSYS((CORELOG_NAME "ElfWriteMappings: Huh!? something is wrong!\n"));
            rc = pVBoxCore->pfnWriter(pVBoxCore->hCoreFile, &achBuf, sizeof(achBuf));
            if (RT_FAILURE(rc))
            {
                CORELOGRELSYS((CORELOG_NAME "ElfWriteMappings: pfnWriter(2) failed. rc=%Rrc\n", rc));
                return rc;
            }
        }

        pMapInfo = pMapInfo->pNext;
    }

    return VINF_SUCCESS;
}


/**
 * Write program headers for all mappings into the core file.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @return IPRT status code.
 */
static int ElfWriteMappingHeaders(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    Elf_Phdr ProgHdr;
    RT_ZERO(ProgHdr);
    ProgHdr.p_type = PT_LOAD;

    int rc = VERR_GENERAL_FAILURE;
    PVBOXSOLMAPINFO pMapInfo = pVBoxProc->pMapInfoHead;
    while (pMapInfo)
    {
        ProgHdr.p_vaddr  = pMapInfo->pMap.pr_vaddr;     /* Virtual address of this mapping in the process address space */
        ProgHdr.p_offset = pVBoxCore->offWrite;         /* Where this mapping is located in the core file */
        ProgHdr.p_memsz  = pMapInfo->pMap.pr_size;      /* Size of the memory image of the mapping */
        ProgHdr.p_filesz = pMapInfo->pMap.pr_size;      /* Size of the file image of the mapping */

        ProgHdr.p_flags = 0;                            /* Reset fields in a loop when needed! */
        if (pMapInfo->pMap.pr_mflags & MA_READ)
            ProgHdr.p_flags |= PF_R;
        if (pMapInfo->pMap.pr_mflags & MA_WRITE)
            ProgHdr.p_flags |= PF_W;
        if (pMapInfo->pMap.pr_mflags & MA_EXEC)
            ProgHdr.p_flags |= PF_X;

        if (pMapInfo->fError)
            ProgHdr.p_flags |= PF_SUNW_FAILURE;

        rc = pVBoxCore->pfnWriter(pVBoxCore->hCoreFile, &ProgHdr, sizeof(ProgHdr));
        if (RT_FAILURE(rc))
        {
            CORELOGRELSYS((CORELOG_NAME "ElfWriteMappingHeaders: pfnWriter failed. rc=%Rrc\n", rc));
            return rc;
        }

        pVBoxCore->offWrite += ProgHdr.p_filesz;
        pMapInfo = pMapInfo->pNext;
    }
    return rc;
}


/**
 * Write a prepared core file using a user-passed in writer function, requires all threads
 * to be in suspended state (i.e. called after CreateCore).
 *
 * @param pVBoxCore         Pointer to the core object.
 * @param pfnWriter         Pointer to the writer function to override default writer (NULL uses default).
 *
 * @remarks Resumes all suspended threads, unless it's an invalid core. This
 *          function must be called only -after- rtCoreDumperCreateCore().
 * @return VBox status.
 */
static int rtCoreDumperWriteCore(PVBOXCORE pVBoxCore, PFNCOREWRITER pfnWriter)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);

    if (!pVBoxCore->fIsValid)
        return VERR_INVALID_STATE;

    if (pfnWriter)
        pVBoxCore->pfnWriter = pfnWriter;

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    char szPath[PATH_MAX];
    int rc = VINF_SUCCESS;

    /*
     * Open the process address space file.
     */
    RTStrPrintf(szPath, sizeof(szPath), "/proc/%d/as", (int)pVBoxProc->Process);
    int fd = open(szPath, O_RDONLY);
    if (fd < 0)
    {
        rc = RTErrConvertFromErrno(fd);
        CORELOGRELSYS((CORELOG_NAME "WriteCore: Failed to open address space, %s. rc=%Rrc\n", szPath, rc));
        goto WriteCoreDone;
    }

    pVBoxProc->hAs = (RTFILE)(uintptr_t)fd;

    /*
     * Create the core file.
     */
    fd = open(pVBoxCore->szCorePath, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR);
    if (fd < 0)
    {
        rc = RTErrConvertFromErrno(fd);
        CORELOGRELSYS((CORELOG_NAME "WriteCore: failed to open %s. rc=%Rrc\n", pVBoxCore->szCorePath, rc));
        goto WriteCoreDone;
    }

    pVBoxCore->hCoreFile = (RTFILE)(uintptr_t)fd;

    pVBoxCore->offWrite = 0;
    uint32_t cProgHdrs  = pVBoxProc->cMappings + 2; /* two PT_NOTE program headers (old, new style) */

    /*
     * Write the ELF header.
     */
    Elf_Ehdr ElfHdr;
    RT_ZERO(ElfHdr);
    ElfHdr.e_ident[EI_MAG0]  = ELFMAG0;
    ElfHdr.e_ident[EI_MAG1]  = ELFMAG1;
    ElfHdr.e_ident[EI_MAG2]  = ELFMAG2;
    ElfHdr.e_ident[EI_MAG3]  = ELFMAG3;
    ElfHdr.e_ident[EI_DATA]  = IsBigEndian() ? ELFDATA2MSB : ELFDATA2LSB;
    ElfHdr.e_type            = ET_CORE;
    ElfHdr.e_version         = EV_CURRENT;
#ifdef RT_ARCH_AMD64
    ElfHdr.e_machine         = EM_AMD64;
    ElfHdr.e_ident[EI_CLASS] = ELFCLASS64;
#else
    ElfHdr.e_machine         = EM_386;
    ElfHdr.e_ident[EI_CLASS] = ELFCLASS32;
#endif
    if (cProgHdrs >= PN_XNUM)
        ElfHdr.e_phnum       = PN_XNUM;
    else
        ElfHdr.e_phnum       = cProgHdrs;
    ElfHdr.e_ehsize          = sizeof(ElfHdr);
    ElfHdr.e_phoff           = sizeof(ElfHdr);
    ElfHdr.e_phentsize       = sizeof(Elf_Phdr);
    ElfHdr.e_shentsize       = sizeof(Elf_Shdr);
    rc = pVBoxCore->pfnWriter(pVBoxCore->hCoreFile, &ElfHdr, sizeof(ElfHdr));
    if (RT_FAILURE(rc))
    {
        CORELOGRELSYS((CORELOG_NAME "WriteCore: pfnWriter failed writing ELF header. rc=%Rrc\n", rc));
        goto WriteCoreDone;
    }

    /*
     * Setup program header.
     */
    Elf_Phdr ProgHdr;
    RT_ZERO(ProgHdr);
    ProgHdr.p_type = PT_NOTE;
    ProgHdr.p_flags = PF_R;

    /*
     * Write old-style NOTE program header.
     */
    pVBoxCore->offWrite += sizeof(ElfHdr) + cProgHdrs * sizeof(ProgHdr);
    ProgHdr.p_offset = pVBoxCore->offWrite;
    ProgHdr.p_filesz = ElfNoteSectionSize(pVBoxCore, enmOldEra);
    rc = pVBoxCore->pfnWriter(pVBoxCore->hCoreFile, &ProgHdr, sizeof(ProgHdr));
    if (RT_FAILURE(rc))
    {
        CORELOGRELSYS((CORELOG_NAME "WriteCore: pfnWriter failed writing old-style ELF program Header. rc=%Rrc\n", rc));
        goto WriteCoreDone;
    }

    /*
     * Write new-style NOTE program header.
     */
    pVBoxCore->offWrite += ProgHdr.p_filesz;
    ProgHdr.p_offset = pVBoxCore->offWrite;
    ProgHdr.p_filesz = ElfNoteSectionSize(pVBoxCore, enmNewEra);
    rc = pVBoxCore->pfnWriter(pVBoxCore->hCoreFile, &ProgHdr, sizeof(ProgHdr));
    if (RT_FAILURE(rc))
    {
        CORELOGRELSYS((CORELOG_NAME "WriteCore: pfnWriter failed writing new-style ELF program header. rc=%Rrc\n", rc));
        goto WriteCoreDone;
    }

    /*
     * Write program headers per mapping.
     */
    pVBoxCore->offWrite += ProgHdr.p_filesz;
    rc = ElfWriteMappingHeaders(pVBoxCore);
    if (RT_FAILURE(rc))
    {
        CORELOGRELSYS((CORELOG_NAME "Write: ElfWriteMappings failed. rc=%Rrc\n", rc));
        goto WriteCoreDone;
    }

    /*
     * Write old-style note section.
     */
    rc = ElfWriteNoteSection(pVBoxCore, enmOldEra);
    if (RT_FAILURE(rc))
    {
        CORELOGRELSYS((CORELOG_NAME "WriteCore: ElfWriteNoteSection old-style failed. rc=%Rrc\n", rc));
        goto WriteCoreDone;
    }

    /*
     * Write new-style section.
     */
    rc = ElfWriteNoteSection(pVBoxCore, enmNewEra);
    if (RT_FAILURE(rc))
    {
        CORELOGRELSYS((CORELOG_NAME "WriteCore: ElfWriteNoteSection new-style failed. rc=%Rrc\n", rc));
        goto WriteCoreDone;
    }

    /*
     * Write all mappings.
     */
    rc = ElfWriteMappings(pVBoxCore);
    if (RT_FAILURE(rc))
    {
        CORELOGRELSYS((CORELOG_NAME "WriteCore: ElfWriteMappings failed. rc=%Rrc\n", rc));
        goto WriteCoreDone;
    }


WriteCoreDone:
    if (pVBoxCore->hCoreFile != NIL_RTFILE)     /* Initialized in rtCoreDumperCreateCore() */
    {
        RTFileClose(pVBoxCore->hCoreFile);
        pVBoxCore->hCoreFile = NIL_RTFILE;
    }

    if (pVBoxProc->hAs != NIL_RTFILE)           /* Initialized in rtCoreDumperCreateCore() */
    {
        RTFileClose(pVBoxProc->hAs);
        pVBoxProc->hAs = NIL_RTFILE;
    }

    rtCoreDumperResumeThreads(pVBoxCore);
    return rc;
}


/**
 * Takes a process snapshot into a passed-in core object. It has the side-effect of halting
 * all threads which can lead to things like spurious wakeups of threads (if and when threads
 * are ultimately resumed en-masse) already suspended while calling this function.
 *
 * @param pVBoxCore         Pointer to a core object.
 * @param pContext          Pointer to the caller context thread.
 * @param pszCoreFilePath   Path to the core file. If NULL is passed, the global
 *                          path specified in RTCoreDumperSetup() would be used.
 *
 * @remarks Halts all threads.
 * @return IPRT status code.
 */
static int rtCoreDumperCreateCore(PVBOXCORE pVBoxCore, ucontext_t *pContext, const char *pszCoreFilePath)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);
    AssertReturn(pContext, VERR_INVALID_POINTER);

    /*
     * Initialize core structures.
     */
    memset(pVBoxCore, 0, sizeof(VBOXCORE));
    pVBoxCore->pfnReader = &ReadFileNoIntr;
    pVBoxCore->pfnWriter = &WriteFileNoIntr;
    pVBoxCore->fIsValid  = false;
    pVBoxCore->hCoreFile = NIL_RTFILE;

    PVBOXPROCESS pVBoxProc = &pVBoxCore->VBoxProc;
    pVBoxProc->Process        = RTProcSelf();
    pVBoxProc->hCurThread     = _lwp_self(); /* thr_self() */
    pVBoxProc->hAs            = NIL_RTFILE;
    pVBoxProc->pCurThreadCtx  = pContext;
    pVBoxProc->CoreContent    = CC_CONTENT_DEFAULT;

    RTProcGetExecutablePath(pVBoxProc->szExecPath, sizeof(pVBoxProc->szExecPath));  /* this gets full path not just name */
    pVBoxProc->pszExecName = RTPathFilename(pVBoxProc->szExecPath);

    /*
     * If a path has been specified, use it. Otherwise use the global path.
     */
    if (!pszCoreFilePath)
    {
        /*
         * If no output directory is specified, use current directory.
         */
        if (g_szCoreDumpDir[0] == '\0')
            g_szCoreDumpDir[0] = '.';

        if (g_szCoreDumpFile[0] == '\0')
        {
            /* We cannot call RTPathAbs*() as they call getcwd() which calls malloc. */
            RTStrPrintf(pVBoxCore->szCorePath, sizeof(pVBoxCore->szCorePath), "%s/core.vb.%s.%d",
                        g_szCoreDumpDir, pVBoxProc->pszExecName, (int)pVBoxProc->Process);
        }
        else
            RTStrPrintf(pVBoxCore->szCorePath, sizeof(pVBoxCore->szCorePath), "%s/core.vb.%s", g_szCoreDumpDir, g_szCoreDumpFile);
    }
    else
        RTStrCopy(pVBoxCore->szCorePath, sizeof(pVBoxCore->szCorePath), pszCoreFilePath);

    CORELOG((CORELOG_NAME  "CreateCore: Taking Core %s from Thread %d\n", pVBoxCore->szCorePath, (int)pVBoxProc->hCurThread));

    /*
     * Quiesce the process.
     */
    int rc = rtCoreDumperSuspendThreads(pVBoxCore);
    if (RT_SUCCESS(rc))
    {
        rc = ProcReadInfo(pVBoxCore);
        if (RT_SUCCESS(rc))
        {
            GetOldProcessInfo(pVBoxCore, &pVBoxProc->ProcInfoOld);
            if (IsProcessArchNative(pVBoxProc))
            {
                /*
                 * Read process status, information such as number of active LWPs will be invalid since we just quiesced the process.
                 */
                rc = ProcReadStatus(pVBoxCore);
                if (RT_SUCCESS(rc))
                {
                    rc = AllocMemoryArea(pVBoxCore);
                    if (RT_SUCCESS(rc))
                    {
                        struct COREACCUMULATOR
                        {
                            const char        *pszName;
                            PFNCOREACCUMULATOR pfnAcc;
                            bool               fOptional;
                        } aAccumulators[] =
                        {
                            { "ProcReadLdt",      &ProcReadLdt,      false },
                            { "ProcReadCred",     &ProcReadCred,     false },
                            { "ProcReadPriv",     &ProcReadPriv,     false },
                            { "ProcReadAuxVecs",  &ProcReadAuxVecs,  false },
                            { "ProcReadMappings", &ProcReadMappings, false },
                            { "ProcReadThreads",  &ProcReadThreads,  false },
                            { "ProcReadMiscInfo", &ProcReadMiscInfo, false }
                        };

                        for (unsigned i = 0; i < RT_ELEMENTS(aAccumulators); i++)
                        {
                            rc = aAccumulators[i].pfnAcc(pVBoxCore);
                            if (RT_FAILURE(rc))
                            {
                                CORELOGRELSYS((CORELOG_NAME "CreateCore: %s failed. rc=%Rrc\n", aAccumulators[i].pszName, rc));
                                if (!aAccumulators[i].fOptional)
                                    break;
                            }
                        }

                        if (RT_SUCCESS(rc))
                        {
                            pVBoxCore->fIsValid = true;
                            return VINF_SUCCESS;
                        }

                        FreeMemoryArea(pVBoxCore);
                    }
                    else
                        CORELOGRELSYS((CORELOG_NAME "CreateCore: AllocMemoryArea failed. rc=%Rrc\n", rc));
                }
                else
                    CORELOGRELSYS((CORELOG_NAME "CreateCore: ProcReadStatus failed. rc=%Rrc\n", rc));
            }
            else
            {
                CORELOGRELSYS((CORELOG_NAME "CreateCore: IsProcessArchNative failed.\n"));
                rc = VERR_BAD_EXE_FORMAT;
            }
        }
        else
            CORELOGRELSYS((CORELOG_NAME "CreateCore: ProcReadInfo failed. rc=%Rrc\n", rc));

        /*
         * Resume threads on failure.
         */
        rtCoreDumperResumeThreads(pVBoxCore);
    }
    else
        CORELOG((CORELOG_NAME "CreateCore: SuspendAllThreads failed. Thread bomb!?! rc=%Rrc\n", rc));

    return rc;
}


/**
 * Destroy an existing core object.
 *
 * @param pVBoxCore         Pointer to the core object.
 *
 * @return IPRT status code.
 */
static int rtCoreDumperDestroyCore(PVBOXCORE pVBoxCore)
{
    AssertReturn(pVBoxCore, VERR_INVALID_POINTER);
    if (!pVBoxCore->fIsValid)
        return VERR_INVALID_STATE;

    FreeMemoryArea(pVBoxCore);
    pVBoxCore->fIsValid = false;
    return VINF_SUCCESS;
}


/**
 * Takes a core dump. This function has no other parameters than the context
 * because it can be called from signal handlers.
 *
 * @param   pContext            The context of the caller.
 * @param   pszOutputFile       Path of the core file. If NULL is passed, the
 *                              global path passed in RTCoreDumperSetup will
 *                              be used.
 * @returns IPRT status code.
 */
static int rtCoreDumperTakeDump(ucontext_t *pContext, const char *pszOutputFile)
{
    if (!pContext)
    {
        CORELOGRELSYS((CORELOG_NAME "TakeDump: Missing context.\n"));
        return VERR_INVALID_POINTER;
    }

    /*
     * Take a snapshot, then dump core to disk, all threads except this one are halted
     * from before taking the snapshot until writing the core is completely finished.
     * Any errors would resume all threads if they were halted.
     */
    VBOXCORE VBoxCore;
    RT_ZERO(VBoxCore);
    int rc = rtCoreDumperCreateCore(&VBoxCore, pContext, pszOutputFile);
    if (RT_SUCCESS(rc))
    {
        rc = rtCoreDumperWriteCore(&VBoxCore, &WriteFileNoIntr);
        if (RT_SUCCESS(rc))
            CORELOGRELSYS((CORELOG_NAME "Core dumped in %s\n", VBoxCore.szCorePath));
        else
            CORELOGRELSYS((CORELOG_NAME "TakeDump: WriteCore failed. szCorePath=%s rc=%Rrc\n", VBoxCore.szCorePath, rc));

        rtCoreDumperDestroyCore(&VBoxCore);
    }
    else
        CORELOGRELSYS((CORELOG_NAME "TakeDump: CreateCore failed. rc=%Rrc\n", rc));

    return rc;
}


/**
 * The signal handler that will be invoked to take core dumps.
 *
 * @param Sig                   The signal that invoked us.
 * @param pSigInfo              The signal information.
 * @param pvArg                 Opaque pointer to the caller context structure,
 *                              this cannot be NULL.
 */
static void rtCoreDumperSignalHandler(int Sig, siginfo_t *pSigInfo, void *pvArg)
{
    CORELOG((CORELOG_NAME "SignalHandler Sig=%d pvArg=%p\n", Sig, pvArg));

    RTNATIVETHREAD  hCurNativeThread = RTThreadNativeSelf();
    int             rc               = VERR_GENERAL_FAILURE;
    bool            fCallSystemDump  = false;
    bool            fRc;
    ASMAtomicCmpXchgHandle(&g_CoreDumpThread, hCurNativeThread, NIL_RTNATIVETHREAD, fRc);
    if (fRc)
    {
        rc = rtCoreDumperTakeDump((ucontext_t *)pvArg, NULL /* Use Global Core filepath */);
        ASMAtomicWriteHandle(&g_CoreDumpThread, NIL_RTNATIVETHREAD);

        if (RT_FAILURE(rc))
            CORELOGRELSYS((CORELOG_NAME "TakeDump failed! rc=%Rrc\n", rc));
    }
    else if (Sig == SIGSEGV || Sig == SIGBUS || Sig == SIGTRAP)
    {
        /*
         * Core dumping is already in progress and we've somehow ended up being
         * signalled again.
         */
        rc = VERR_INTERNAL_ERROR;

        /*
         * If our dumper has crashed. No point in waiting, trigger the system one.
         * Wait only when the dumping thread is not the one generating this signal.
         */
        RTNATIVETHREAD hNativeDumperThread;
        ASMAtomicReadHandle(&g_CoreDumpThread, &hNativeDumperThread);
        if (hNativeDumperThread == RTThreadNativeSelf())
        {
            CORELOGRELSYS((CORELOG_NAME "SignalHandler: Core dumper (thread %u) crashed Sig=%d. Triggering system dump\n",
                           RTThreadSelf(), Sig));
            fCallSystemDump = true;
        }
        else
        {
            /*
             * Some other thread in the process is triggering a crash, wait a while
             * to let our core dumper finish, on timeout trigger system dump.
             */
            CORELOGRELSYS((CORELOG_NAME "SignalHandler: Core dump already in progress! Waiting a while for completion Sig=%d.\n", Sig));
            int64_t iTimeout = 16000;  /* timeout (ms) */
            for (;;)
            {
                ASMAtomicReadHandle(&g_CoreDumpThread, &hNativeDumperThread);
                if (hNativeDumperThread == NIL_RTNATIVETHREAD)
                    break;
                RTThreadSleep(200);
                iTimeout -= 200;
                if (iTimeout <= 0)
                    break;
            }
            if (iTimeout <= 0)
            {
                fCallSystemDump = true;
                CORELOGRELSYS((CORELOG_NAME "SignalHandler: Core dumper seems to be stuck. Signalling new signal %d\n", Sig));
            }
        }
    }

    if (Sig == SIGSEGV || Sig == SIGBUS || Sig == SIGTRAP)
    {
        /*
         * Reset signal handlers, we're not a live core we will be blown away
         * one way or another.
         */
        signal(SIGSEGV, SIG_DFL);
        signal(SIGBUS, SIG_DFL);
        signal(SIGTRAP, SIG_DFL);

        /*
         * Hard terminate the process if this is not a live dump without invoking
         * the system core dumping behaviour.
         */
        if (RT_SUCCESS(rc))
            raise(SIGKILL);

        /*
         * Something went wrong, fall back to the system core dumper.
         */
        if (fCallSystemDump)
            abort();
    }
}


RTDECL(int) RTCoreDumperTakeDump(const char *pszOutputFile, bool fLiveCore)
{
    ucontext_t Context;
    int rc = getcontext(&Context);
    if (!rc)
    {
        /*
         * Block SIGSEGV and co. while we write the core.
         */
        sigset_t SigSet, OldSigSet;
        sigemptyset(&SigSet);
        sigaddset(&SigSet, SIGSEGV);
        sigaddset(&SigSet, SIGBUS);
        sigaddset(&SigSet, SIGTRAP);
        sigaddset(&SigSet, SIGUSR2);
        pthread_sigmask(SIG_BLOCK, &SigSet, &OldSigSet);
        rc = rtCoreDumperTakeDump(&Context, pszOutputFile);
        if (RT_FAILURE(rc))
            CORELOGRELSYS(("RTCoreDumperTakeDump: rtCoreDumperTakeDump failed rc=%Rrc\n", rc));

        if (!fLiveCore)
        {
            signal(SIGSEGV, SIG_DFL);
            signal(SIGBUS, SIG_DFL);
            signal(SIGTRAP, SIG_DFL);
            if (RT_SUCCESS(rc))
                raise(SIGKILL);
            else
                abort();
        }
        pthread_sigmask(SIG_SETMASK, &OldSigSet, NULL);
    }
    else
    {
        CORELOGRELSYS(("RTCoreDumperTakeDump: getcontext failed rc=%d.\n", rc));
        rc = VERR_INVALID_CONTEXT;
    }

    return rc;
}


RTDECL(int) RTCoreDumperSetup(const char *pszOutputDir, uint32_t fFlags)
{
    /*
     * Validate flags.
     */
    AssertReturn(fFlags, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~(  RTCOREDUMPER_FLAGS_REPLACE_SYSTEM_DUMP
                              | RTCOREDUMPER_FLAGS_LIVE_CORE)),
                 VERR_INVALID_PARAMETER);


    /*
     * Setup/change the core dump directory if specified.
     */
    RT_ZERO(g_szCoreDumpDir);
    if (pszOutputDir)
    {
        if (!RTDirExists(pszOutputDir))
            return VERR_NOT_A_DIRECTORY;
        RTStrCopy(g_szCoreDumpDir, sizeof(g_szCoreDumpDir), pszOutputDir);
    }

    /*
     * Install core dump signal handler only if the flags changed or if it's the first time.
     */
    if (   ASMAtomicReadBool(&g_fCoreDumpSignalSetup) == false
        || ASMAtomicReadU32(&g_fCoreDumpFlags) != fFlags)
    {
        struct sigaction sigAct;
        RT_ZERO(sigAct);
        sigAct.sa_sigaction = &rtCoreDumperSignalHandler;

        if (   (fFlags & RTCOREDUMPER_FLAGS_REPLACE_SYSTEM_DUMP)
            && !(g_fCoreDumpFlags & RTCOREDUMPER_FLAGS_REPLACE_SYSTEM_DUMP))
        {
            sigemptyset(&sigAct.sa_mask);
            sigAct.sa_flags = SA_RESTART | SA_SIGINFO | SA_NODEFER;
            sigaction(SIGSEGV, &sigAct, NULL);
            sigaction(SIGBUS, &sigAct, NULL);
            sigaction(SIGTRAP, &sigAct, NULL);
        }

        if (   fFlags & RTCOREDUMPER_FLAGS_LIVE_CORE
            && !(g_fCoreDumpFlags & RTCOREDUMPER_FLAGS_LIVE_CORE))
        {
            sigfillset(&sigAct.sa_mask);                        /* Block all signals while in it's signal handler */
            sigAct.sa_flags = SA_RESTART | SA_SIGINFO;
            sigaction(SIGUSR2, &sigAct, NULL);
        }

        ASMAtomicWriteU32(&g_fCoreDumpFlags, fFlags);
        ASMAtomicWriteBool(&g_fCoreDumpSignalSetup, true);
    }

    return VINF_SUCCESS;
}


RTDECL(int) RTCoreDumperDisable(void)
{
    /*
     * Remove core dump signal handler & reset variables.
     */
    if (ASMAtomicReadBool(&g_fCoreDumpSignalSetup) == true)
    {
        signal(SIGSEGV, SIG_DFL);
        signal(SIGBUS, SIG_DFL);
        signal(SIGTRAP, SIG_DFL);
        signal(SIGUSR2, SIG_DFL);
        ASMAtomicWriteBool(&g_fCoreDumpSignalSetup, false);
    }

    RT_ZERO(g_szCoreDumpDir);
    RT_ZERO(g_szCoreDumpFile);
    ASMAtomicWriteU32(&g_fCoreDumpFlags, 0);
    return VINF_SUCCESS;
}

