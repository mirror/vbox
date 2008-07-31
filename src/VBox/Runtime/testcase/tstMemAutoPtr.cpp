/* $Id$ */
/** @file
 * IPRT - Testcase the RTMemAutoPtr template.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/rand.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct TSTMEMAUTOPTRSTRUCT
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
} TSTMEMAUTOPTRSTRUCT;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifndef TST_MEM_AUTO_PTR_ONLY_DISAS
static unsigned g_cErrors = 0;
static unsigned g_cFrees;
#endif


/*
 * Feel free to inspect with gdb / objdump / whatever / g++ -fverbose-asm in
 * a release build and compare with tstMemAutoPtrDisas1PureC.
 */
extern "C" int tstMemAutoPtrDisas1(void **ppv)
{
    register RTMemAutoPtr<TSTMEMAUTOPTRSTRUCT> Handle(1);
    if (!Handle)
    {
        Handle->a = RTRandU32();
        if (Handle->a < UINT32_MAX / 2)
        {
            *ppv = Handle.release();
            return VINF_SUCCESS;
        }
    }
    return VERR_TRY_AGAIN;
}

/*
 * For comparing to tstMemAutoPtrDisas1.
 */
extern "C" int tstMemAutoPtrDisas1PureC(void **ppv)
{
    TSTMEMAUTOPTRSTRUCT *pHandle = (TSTMEMAUTOPTRSTRUCT *)RTMemRealloc(NULL, sizeof(*pHandle));
    if (pHandle)
    {
        pHandle->a = RTRandU32();
        if (pHandle->a < UINT32_MAX / 2)
        {
            *ppv = pHandle;
            return VINF_SUCCESS;
        }
        RTMemFree(pHandle);
    }
    return VERR_TRY_AGAIN;
}


#ifndef TST_MEM_AUTO_PTR_ONLY_DISAS

template <class T>
void tstMemAutoPtrDestructorCounter(T *aMem)
{
    if (aMem == NULL)
    {
        RTPrintf("tstMemAutoPtr(%d): Destructor called with NILL handle!\n");
        g_cErrors++;
    }
    else if (!VALID_PTR(aMem))
    {
        RTPrintf("tstMemAutoPtr(%d): Destructor called with a bad handle %p\n", aMem);
        g_cErrors++;
    }
    RTMemEfFree(aMem);
    g_cFrees++;
}


void *tstMemAutoPtrAllocatorNoZero(void *pvOld, size_t cbNew)
{
    void *pvNew = RTMemRealloc(pvOld, cbNew);
    if (pvNew)
        memset(pvNew, 0xfe, cbNew);
    return pvNew;
}


int main()
{
    RTR3Init(false);
    RTPrintf("tstMemAutoPtr: TESTING...\n");

#define CHECK_EXPR(expr) \
    do { bool const f = !!(expr); if (!f) { RTPrintf("tstMemAutoPtr(%d): %s!\n", __LINE__, #expr); g_cErrors++; } } while (0)

    /*
     * Some simple stuff.
     */
    {
        RTMemAutoPtr<char> NilObj;
        CHECK_EXPR(!NilObj);
        CHECK_EXPR(NilObj.get() == NULL);
        CHECK_EXPR(NilObj.release() == NULL);
        NilObj.reset();
    }

    {
        RTMemAutoPtr<char> Alloc(10);
        CHECK_EXPR(Alloc.get() != NULL);
        char *pch = Alloc.release();
        CHECK_EXPR(pch != NULL);
        CHECK_EXPR(Alloc.get() == NULL);

        RTMemAutoPtr<char> Manage(pch);
        CHECK_EXPR(Manage.get() == pch);
        CHECK_EXPR(&Manage[0] == pch);
        CHECK_EXPR(&Manage[1] == &pch[1]);
        CHECK_EXPR(&Manage[9] == &pch[9]);
    }

    /*
     * Use the electric fence memory API to check alternative template
     * arguments and also check some subscript / reference limit thing.
     */
    {
        RTMemAutoPtr<char, RTMemEfAutoFree<char>, RTMemEfRealloc> Electric(10);
        CHECK_EXPR(Electric.get() != NULL);
        Electric[0] = '0';
        CHECK_EXPR(Electric[0]  == '0');
        CHECK_EXPR(*Electric  == '0');
        //CHECK_EXPR(Electric  == '0');
        Electric[9] = '1';
        CHECK_EXPR(Electric[9] == '1');
        /* Electric[10] = '2'; - this will crash (of course) */
    }

    /*
     * Check that memory is actually free when it should be and isn't when it shouldn't.
     * Use the electric heap to get some extra checks.
     */
    g_cFrees = 0;
    {
        RTMemAutoPtr<char, tstMemAutoPtrDestructorCounter, RTMemEfRealloc> FreeIt(128);
        FreeIt[127] = '0';
    }
    CHECK_EXPR(g_cFrees == 1);

    g_cFrees = 0;
    {
        RTMemAutoPtr<char, tstMemAutoPtrDestructorCounter, RTMemEfRealloc> FreeIt2(128);
        FreeIt2[127] = '1';
        FreeIt2.reset();
        FreeIt2.alloc(128);
        FreeIt2[127] = '2';
        FreeIt2.reset(FreeIt2.get()); /* this one is weird, but it's how things works... */
    }
    CHECK_EXPR(g_cFrees == 2);

    g_cFrees = 0;
    {
        RTMemAutoPtr<char, tstMemAutoPtrDestructorCounter, RTMemEfRealloc> DontFreeIt(256);
        DontFreeIt[255] = '0';
        RTMemEfFree(DontFreeIt.release());
    }
    CHECK_EXPR(g_cFrees == 0);

    g_cFrees = 0;
    {
        RTMemAutoPtr<char, tstMemAutoPtrDestructorCounter, RTMemEfRealloc> FreeIt3(128);
        FreeIt3[127] = '0';
        CHECK_EXPR(FreeIt3.realloc(128));
        FreeIt3[127] = '0';
        CHECK_EXPR(FreeIt3.realloc(256));
        FreeIt3[255] = '0';
        CHECK_EXPR(FreeIt3.realloc(64));
        FreeIt3[63] = '0';
        CHECK_EXPR(FreeIt3.realloc(32));
        FreeIt3[31] = '0';
    }
    CHECK_EXPR(g_cFrees == 1);

    g_cFrees = 0;
    {
        RTMemAutoPtr<char, tstMemAutoPtrDestructorCounter, RTMemEfRealloc> FreeIt4;
        CHECK_EXPR(FreeIt4.alloc(123));
        CHECK_EXPR(FreeIt4.realloc(543));
        FreeIt4 = (char *)NULL;
        CHECK_EXPR(FreeIt4.get() == NULL);
    }
    CHECK_EXPR(g_cFrees == 1);

    /*
     * Check the ->, [] and * (unary) operators with some useful struct.
     */
    {
        RTMemAutoPtr<TSTMEMAUTOPTRSTRUCT> Struct1(1);
        Struct1->a = 0x11223344;
        Struct1->b = 0x55667788;
        Struct1->c = 0x99aabbcc;
        CHECK_EXPR(Struct1->a == 0x11223344);
        CHECK_EXPR(Struct1->b == 0x55667788);
        CHECK_EXPR(Struct1->c == 0x99aabbcc);

        Struct1[0].a = 0x11223344;
        Struct1[0].b = 0x55667788;
        Struct1[0].c = 0x99aabbcc;
        CHECK_EXPR(Struct1[0].a == 0x11223344);
        CHECK_EXPR(Struct1[0].b == 0x55667788);
        CHECK_EXPR(Struct1[0].c == 0x99aabbcc);

        (*Struct1).a = 0x11223344;
        (*Struct1).b = 0x55667788;
        (*Struct1).c = 0x99aabbcc;
        CHECK_EXPR((*Struct1).a == 0x11223344);
        CHECK_EXPR((*Struct1).b == 0x55667788);
        CHECK_EXPR((*Struct1).c == 0x99aabbcc);

        /* since at it... */
        Struct1.get()->a = 0x11223344;
        Struct1.get()->b = 0x55667788;
        Struct1.get()->c = 0x99aabbcc;
        CHECK_EXPR(Struct1.get()->a == 0x11223344);
        CHECK_EXPR(Struct1.get()->b == 0x55667788);
        CHECK_EXPR(Struct1.get()->c == 0x99aabbcc);
    }

    /*
     * Check the zeroing of memory.
     */
    {
        RTMemAutoPtr<uint64_t, RTMemAutoDestructor<uint64_t>, tstMemAutoPtrAllocatorNoZero> Zeroed1(1, true);
        CHECK_EXPR(*Zeroed1 == 0);
    }

    {
        RTMemAutoPtr<uint64_t, RTMemAutoDestructor<uint64_t>, tstMemAutoPtrAllocatorNoZero> Zeroed2;
        Zeroed2.alloc(5, true);
        CHECK_EXPR(Zeroed2[0] == 0);
        CHECK_EXPR(Zeroed2[1] == 0);
        CHECK_EXPR(Zeroed2[2] == 0);
        CHECK_EXPR(Zeroed2[3] == 0);
        CHECK_EXPR(Zeroed2[4] == 0);
    }

    /*
     * Summary.
     */
    if (!g_cErrors)
        RTPrintf("tstMemAutoPtr: SUCCESS\n");
    else
        RTPrintf("tstMemAutoPtr: FAILED - %d errors\n", g_cErrors);
    return !!g_cErrors;
}
#endif /* TST_MEM_AUTO_PTR_ONLY_DISAS */
