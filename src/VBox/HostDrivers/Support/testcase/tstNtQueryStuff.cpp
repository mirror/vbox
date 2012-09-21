/* $Id$ */
/** @file
 * SUP Testcase - Exploring some NT Query APIs.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include <Windows.h>
#include <winternl.h>

typedef enum
{
    MemoryBasicInformation = 0,
    MemoryWorkingSetList,
    MemorySectionName,
    MemoryBasicVlmInformation
} MEMORY_INFORMATION_CLASS;

typedef struct
{
    UNICODE_STRING  SectionFileName;
    WCHAR           NameBuffer[ANYSIZE_ARRAY];
} MEMORY_SECTION_NAME;

extern "C"
NTSYSAPI NTSTATUS NTAPI NtQueryVirtualMemory(IN HANDLE hProcess,
                                             IN LPCVOID pvWhere,
                                             IN MEMORY_INFORMATION_CLASS MemoryInfo,
                                             OUT PVOID pvBuf,
                                             IN SIZE_T cbBuf,
                                             OUT PSIZE_T pcbReturned OPTIONAL);


#include <iprt/test.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct FLAGDESC
{
    ULONG       f;
    const char *psz;
} FLAGDESC;
typedef const FLAGDESC *PCFLAGDESC;



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTTEST g_hTest = NIL_RTTEST;


static char *stringifyAppend(char *pszBuf, size_t *pcbBuf, const char *pszAppend, bool fWithSpace)
{
    size_t cchAppend = strlen(pszAppend);
    if (cchAppend + 1 + fWithSpace <= *pcbBuf)
    {
        if (fWithSpace)
        {
            *pszBuf++ = ' ';
            *pcbBuf  += 1;
        }
        memcpy(pszBuf, pszAppend, cchAppend + 1);
        *pcbBuf -= cchAppend;
        pszBuf  += cchAppend;
    }

    return pszBuf;
}


static char *stringifyAppendUnknownFlags(uint32_t fFlags, char *pszBuf, size_t *pcbBuf, bool fWithSpace)
{
    for (unsigned iBit = 0; iBit < 32; iBit++)
        if (fFlags & RT_BIT_32(iBit))
        {
            char szTmp[32];             /* lazy bird */
            RTStrPrintf(szTmp, sizeof(szTmp), "BIT(%d)", iBit);
            pszBuf = stringifyAppend(pszBuf, pcbBuf, szTmp, fWithSpace);
            fWithSpace = true;
        }

    return pszBuf;
}


static char *stringifyFlags(uint32_t fFlags, char *pszBuf, size_t cbBuf, PCFLAGDESC paFlagDesc, size_t cFlagDesc)
{
    char *pszBufStart = pszBuf;
    if (fFlags)
    {
        for (size_t i = 0; i < cFlagDesc; i++)
        {
            if (fFlags & paFlagDesc[i].f)
            {
                fFlags &= ~paFlagDesc[i].f;
                pszBuf = stringifyAppend(pszBuf, &cbBuf, paFlagDesc[i].psz, pszBuf != pszBufStart);
            }
        }

        if (fFlags)
            stringifyAppendUnknownFlags(fFlags, pszBuf, &cbBuf, pszBuf != pszBufStart);
    }
    else
    {
        pszBuf[0] = '0';
        pszBuf[1] = '\0';
    }
    return pszBufStart;
}


static char *stringifyMemType(uint32_t fType, char *pszBuf, size_t cbBuf)
{
    static const FLAGDESC s_aMemTypes[] =
    {
        { MEM_PRIVATE,      "PRIVATE" },
        { MEM_MAPPED,       "MAPPED" },
        { MEM_IMAGE,        "IMAGE" },
    };
    return stringifyFlags(fType, pszBuf, cbBuf, s_aMemTypes, RT_ELEMENTS(s_aMemTypes));
}


static char *stringifyMemState(uint32_t fState, char *pszBuf, size_t cbBuf)
{
    static const FLAGDESC s_aMemStates[] =
    {
        { MEM_FREE,         "FREE" },
        { MEM_COMMIT,       "COMMIT" },
        { MEM_RESERVE,      "RESERVE" },
        { MEM_DECOMMIT,     "DECOMMMIT" },
    };
    return stringifyFlags(fState, pszBuf, cbBuf, s_aMemStates, RT_ELEMENTS(s_aMemStates));
}


static char *stringifyMemProt(uint32_t fProt, char *pszBuf, size_t cbBuf)
{
    static const FLAGDESC s_aProtections[] =
    {
        { PAGE_NOACCESS,            "NOACCESS" },
        { PAGE_READONLY,            "READONLY" },
        { PAGE_READWRITE,           "READWRITE" },
        { PAGE_WRITECOPY,           "WRITECOPY" },
        { PAGE_EXECUTE,             "EXECUTE" },
        { PAGE_EXECUTE_READ,        "EXECUTE_READ" },
        { PAGE_EXECUTE_READWRITE,   "EXECUTE_READWRITE" },
        { PAGE_EXECUTE_WRITECOPY,   "EXECUTE_WRITECOPY" },
        { PAGE_GUARD,               "GUARD" },
        { PAGE_NOCACHE,             "NOCACHE" },
        { PAGE_WRITECOMBINE,        "WRITECOMBINE" },

    };
    return stringifyFlags(fProt, pszBuf, cbBuf, s_aProtections, RT_ELEMENTS(s_aProtections));
}



static void tstQueryVirtualMemory(void)
{
    RTTestISub("NtQueryVirtualMemory");


    uintptr_t   cbAdvance = 0;
    uintptr_t   uPtrWhere = 0;
    for (;;)
    {
        SIZE_T                      cbActual = 0;
        MEMORY_BASIC_INFORMATION    MemInfo  = { 0, 0, 0, 0, 0, 0, 0 };
        NTSTATUS ntRc = NtQueryVirtualMemory(GetCurrentProcess(),
                                             (void const *)uPtrWhere,
                                             MemoryBasicInformation,
                                             &MemInfo,
                                             sizeof(MemInfo),
                                             &cbActual);
        if (!NT_SUCCESS(ntRc))
        {
            RTTestIPrintf(RTTESTLVL_ALWAYS, "%p: ntRc=%#x\n", uPtrWhere, ntRc);
            break;
        }

        /* stringify the memory state. */
        char szMemType[1024];
        char szMemState[1024];
        char szMemProt[1024];

        RTTestIPrintf(RTTESTLVL_ALWAYS, "%p-%p  %-8s  %-8s  %-12s\n",
                      MemInfo.BaseAddress, (uintptr_t)MemInfo.BaseAddress + MemInfo.RegionSize - 1,
                      stringifyMemType(MemInfo.Type, szMemType, sizeof(szMemType)),
                      stringifyMemState(MemInfo.State, szMemState, sizeof(szMemState)),
                      stringifyMemProt(MemInfo.Protect, szMemProt, sizeof(szMemProt))
                      );

        if ((uintptr_t)MemInfo.BaseAddress != uPtrWhere)
            RTTestIPrintf(RTTESTLVL_ALWAYS, " !Warning! Queried %p got BaseAddress=%p!\n",
                          uPtrWhere, MemInfo.BaseAddress);

        cbAdvance = MemInfo.RegionSize;
        //cbAdvance = 0;
        if (uPtrWhere + cbAdvance <= uPtrWhere)
            break;
        uPtrWhere += MemInfo.RegionSize;
    }
}



//NtQueryInformationProcess

int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstNtQueryStuff", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    tstQueryVirtualMemory();


    return RTTestSummaryAndDestroy(g_hTest);
}


