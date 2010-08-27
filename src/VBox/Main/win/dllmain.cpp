/** @file
 *
 * DLLMAIN - COM DLL exports
 *
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBox/com/defs.h"

#include <SessionImpl.h>

#include <atlbase.h>
#include <atlcom.h>

#include <iprt/initterm.h>

/* Memory leak detection. */
#ifdef VBOX_WITH_VRDP_MEMLEAK_DETECTOR
typedef struct _MLDMemBlock
{
    unsigned uSignature;
    struct _MLDMemBlock *next;
    struct _MLDMemBlock *prev;
    const char *pszCaller;
    int iLine;
    bool fTmp;
    size_t size;
    void *pv;
} MLDMemBlock;

static MLDMemBlock *gMemBlockListHead;
static RTCRITSECT g_critsect;
static const char *gszMDLPrefix = "MLDMEM";
static int gAllocated = 0;
static int gFreed = 0;
static bool gfMLD = false;


#define MLD_BLOCK_TO_PTR(__p) ((__p)? (void *) ( (uint8_t *)(__p) + sizeof (MLDMemBlock) ) : NULL)
#define MLD_PTR_TO_BLOCK(__p) ((__p)? (MLDMemBlock *)((uint8_t *)(__p) - sizeof (MLDMemBlock)): NULL)

#define MLD_BLOCK_SIGNATURE 0xFEDCBA98

static void vrdpMemLock (void)
{
    int rc = RTCritSectEnter (&g_critsect);
    AssertRC(rc);
}

static void vrdpMemUnlock (void)
{
    RTCritSectLeave (&g_critsect);
}

static void vrdpMemAppendBlock (MLDMemBlock *pBlock)
{
    pBlock->next = gMemBlockListHead;
    pBlock->prev = NULL;

    if (gMemBlockListHead)
    {
        gMemBlockListHead->prev = pBlock;
    }
    gMemBlockListHead = pBlock;
}

static void vrdpMemExcludeBlock (MLDMemBlock *pBlock)
{
    /* Assert that the block is in the list. */
    MLDMemBlock *pIter = gMemBlockListHead;

    while (pIter && pIter != pBlock)
    {
        pIter = pIter->next;
    }

    Assert (pIter == pBlock);

    /* Exclude the block from list. */
    if (pBlock->next)
    {
        pBlock->next->prev = pBlock->prev;
    }
    else
    {
        /* do nothing */
    }

    if (pBlock->prev)
    {
        pBlock->prev->next = pBlock->next;
    }
    else
    {
        gMemBlockListHead = pBlock->next;
    }

    pBlock->next = NULL;
    pBlock->prev = NULL;
}

void *MLDMemAllocDbg (size_t cb, bool fTmp, bool fZero, const char *pszCaller, int iLine)
{
    size_t cbAlloc;
    MLDMemBlock *pBlock;

    // LogFlowFunc(("cb = %d, fTmp = %d, fZero = %d, pszCaller = %s, iLine = %d\n",
    //              cb, fTmp, fZero, pszCaller, iLine));

    vrdpMemLock ();

    cbAlloc = cb + sizeof (MLDMemBlock);

    pBlock = (MLDMemBlock *)RTMemAlloc (cbAlloc);

    if (pBlock)
    {
        if (fZero)
        {
            memset (pBlock, 0, cbAlloc);
        }

        pBlock->pszCaller  = pszCaller;
        pBlock->iLine      = iLine;
        pBlock->size       = cb;
        pBlock->fTmp       = fTmp;
        pBlock->uSignature = MLD_BLOCK_SIGNATURE;
        pBlock->pv         = MLD_BLOCK_TO_PTR(pBlock);

        vrdpMemAppendBlock (pBlock);

        gAllocated++;
    }

    vrdpMemUnlock ();

    return MLD_BLOCK_TO_PTR(pBlock);
}

void *MLDMemReallocDbg (void *pv, size_t cb, const char *pszCaller, int iLine)
{
    MLDMemBlock *pBlock;

    // LogFlowFunc(("pv = %p, cb = %d, pszCaller = %s, iLine = %d\n",
    //              pv, cb, pszCaller, iLine));

    vrdpMemLock ();

    pBlock = MLD_PTR_TO_BLOCK(pv);

    if (pBlock)
    {
        size_t cbAlloc = cb + sizeof (MLDMemBlock);

        Assert(pBlock->uSignature == MLD_BLOCK_SIGNATURE);
        Assert(!pBlock->fTmp); /* Tmp blocks are not to be reallocated. */
        Assert(pBlock->pv == pv);

        vrdpMemExcludeBlock (pBlock);

        pBlock = (MLDMemBlock *)RTMemRealloc (pBlock, cbAlloc);

        pBlock->pszCaller = pszCaller;
        pBlock->iLine     = iLine;
        pBlock->size      = cb;

        vrdpMemAppendBlock (pBlock);

        pv = MLD_BLOCK_TO_PTR(pBlock);

        pBlock->pv = pv;
    }
    else
    {
        pv = MLDMemAllocDbg (cb, false /* fTmp */, false /* fZero */, pszCaller, iLine);
    }

    vrdpMemUnlock ();

    return pv;
}

void MLDMemFreeDbg (void *pv, bool fTmp)
{
    MLDMemBlock *pBlock;

    // LogFlowFunc(("pv = %d, fTmp = %d\n",
    //              pv, fTmp));

    vrdpMemLock ();

    pBlock = MLD_PTR_TO_BLOCK(pv);

    if (pBlock)
    {
        Assert(pBlock->uSignature == MLD_BLOCK_SIGNATURE);
        Assert(pBlock->fTmp == fTmp);
        Assert(pBlock->pv == pv);

        vrdpMemExcludeBlock (pBlock);

        RTMemFree (pBlock);
        gFreed++;
    }

    vrdpMemUnlock ();
}


void MLDMemDump (void)
{
    MLDMemBlock *pBlock = gMemBlockListHead;

    int c = 0;
    size_t size = 0;
    while (pBlock)
    {
        LogRel(("%s-MLDMEM: %p 0x%8X bytes %d %s@%d\n", gszMDLPrefix, pBlock, pBlock->size, pBlock->fTmp, pBlock->pszCaller, pBlock->iLine));
        c++;
        size += pBlock->size;
        pBlock = pBlock->next;
    }

    LogRel(("%s-MLDMEM: %d/%d/%d blocks allocated/freed/left, total %dKb in use.\n", gszMDLPrefix, gAllocated, gFreed, c, size / 1024));
}

static int gcRefsMem = 0;

void MLDMemInit (const char *pszPrefix)
{
    if (++gcRefsMem == 1)
    {
        int rc = RTCritSectInit (&g_critsect);
        AssertRC(rc);
        gMemBlockListHead = NULL;
        gszMDLPrefix = pszPrefix;
        gAllocated = 0;
        gFreed = 0;
        gfMLD = true;
    }
}

void MLDMemUninit (void)
{
    MLDMemDump();

    if (--gcRefsMem == 0)
    {
        gfMLD = false;
        gMemBlockListHead = NULL;

        if (RTCritSectIsInitialized (&g_critsect))
        {
            RTCritSectDelete (&g_critsect);
        }
    }
}

void* operator new (std::size_t size) throw (std::bad_alloc)
{
    if (gfMLD)
        return MLDMemAllocDbg (size, false /* bool fTmp */, true /* bool fZero */, __FILE__, __LINE__);
    else
        return malloc(size);
}

void operator delete (void* ptr) throw ()
{
    if (gfMLD)
        MLDMemFreeDbg (ptr, false /* bool fTmp */);
    else
        free(ptr);
}

#endif /* !VBOX_WITH_VRDP_MEMLEAK_DETECTOR */

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
    OBJECT_ENTRY(CLSID_Session, Session)
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        _Module.Init(ObjectMap, hInstance, &LIBID_VirtualBox);
        DisableThreadLibraryCalls(hInstance);

        // idempotent, so doesn't harm, and needed for COM embedding scenario
        RTR3Init();

#ifdef VBOX_WITH_VRDP_MEMLEAK_DETECTOR
        MLDMemInit("VBOXC");
#endif /* !VBOX_WITH_VRDP_MEMLEAK_DETECTOR */
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        _Module.Term();

#ifdef VBOX_WITH_VRDP_MEMLEAK_DETECTOR
        MLDMemUninit();
#endif /* !VBOX_WITH_VRDP_MEMLEAK_DETECTOR */
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
    return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
    // registers object, typelib and all interfaces in typelib
    return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
    return _Module.UnregisterServer(TRUE);
}
