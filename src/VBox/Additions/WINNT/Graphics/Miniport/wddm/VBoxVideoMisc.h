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
 */

#ifndef ___VBoxVideoMisc_h__
#define ___VBoxVideoMisc_h__

DECLINLINE(void) vboxVideoLeDetach(LIST_ENTRY *pList, LIST_ENTRY *pDstList)
{
    if (IsListEmpty(pList))
    {
        InitializeListHead(pDstList);
    }
    else
    {
        *pDstList = *pList;
        Assert(pDstList->Flink->Blink == pList);
        Assert(pDstList->Blink->Flink == pList);
        /* pDstList->Flink & pDstList->Blink point to the "real| entries, never to pList
         * since we've checked IsListEmpty(pList) above */
        pDstList->Flink->Blink = pDstList;
        pDstList->Blink->Flink = pDstList;
        InitializeListHead(pList);
    }
}

typedef struct _DEVICE_EXTENSION *PDEVICE_EXTENSION;
typedef struct VBOXWDDM_SWAPCHAIN *PVBOXWDDM_SWAPCHAIN;
typedef struct VBOXWDDM_CONTEXT *PVBOXWDDM_CONTEXT;
typedef struct VBOXWDDM_ALLOCATION *PVBOXWDDM_ALLOCATION;

typedef uint32_t VBOXWDDM_HANDLE;
#define VBOXWDDM_HANDLE_INVALID 0UL

typedef struct VBOXWDDM_HTABLE
{
    uint32_t cData;
    uint32_t iNext2Search;
    uint32_t cSize;
    PVOID *paData;
} VBOXWDDM_HTABLE, *PVBOXWDDM_HTABLE;

typedef struct VBOXWDDM_HTABLE_ITERATOR
{
    PVBOXWDDM_HTABLE pTbl;
    uint32_t iCur;
    uint32_t cLeft;
} VBOXWDDM_HTABLE_ITERATOR, *PVBOXWDDM_HTABLE_ITERATOR;

VOID vboxWddmHTableIterInit(PVBOXWDDM_HTABLE pTbl, PVBOXWDDM_HTABLE_ITERATOR pIter);
PVOID vboxWddmHTableIterNext(PVBOXWDDM_HTABLE_ITERATOR pIter, VBOXWDDM_HANDLE *phHandle);
BOOL vboxWddmHTableIterHasNext(PVBOXWDDM_HTABLE_ITERATOR pIter);
PVOID vboxWddmHTableIterRemoveCur(PVBOXWDDM_HTABLE_ITERATOR pIter);
NTSTATUS vboxWddmHTableCreate(PVBOXWDDM_HTABLE pTbl, uint32_t cSize);
VOID vboxWddmHTableDestroy(PVBOXWDDM_HTABLE pTbl);
NTSTATUS vboxWddmHTableRealloc(PVBOXWDDM_HTABLE pTbl, uint32_t cNewSize);
VBOXWDDM_HANDLE vboxWddmHTablePut(PVBOXWDDM_HTABLE pTbl, PVOID pvData);
PVOID vboxWddmHTableRemove(PVBOXWDDM_HTABLE pTbl, VBOXWDDM_HANDLE hHandle);
PVOID vboxWddmHTableGet(PVBOXWDDM_HTABLE pTbl, VBOXWDDM_HANDLE hHandle);


PVBOXWDDM_SWAPCHAIN vboxWddmSwapchainCreate();
DECLINLINE(BOOLEAN) vboxWddmSwapchainRetain(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain);
DECLINLINE(VOID) vboxWddmSwapchainRelease(PVBOXWDDM_SWAPCHAIN pSwapchain);
PVBOXWDDM_SWAPCHAIN vboxWddmSwapchainRetainByAlloc(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAlloc);
VOID vboxWddmSwapchainAllocRemove(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, PVBOXWDDM_ALLOCATION pAlloc);
BOOLEAN vboxWddmSwapchainAllocAdd(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, PVBOXWDDM_ALLOCATION pAlloc);
VOID vboxWddmSwapchainAllocRemoveAll(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain);
VOID vboxWddmSwapchainDestroy(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain);
VOID vboxWddmSwapchainCtxDestroyAll(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext);
NTSTATUS vboxWddmSwapchainCtxEscape(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXDISPIFESCAPE_SWAPCHAININFO pSwapchainInfo, UINT cbSize);


NTSTATUS vboxWddmRegQueryDisplaySettingsKeyName(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult);
NTSTATUS vboxWddmRegOpenDisplaySettingsKey(IN PDEVICE_EXTENSION pDeviceExtension, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, OUT PHANDLE phKey);
NTSTATUS vboxWddmRegDisplaySettingsQueryRelX(HANDLE hKey, int * pResult);
NTSTATUS vboxWddmRegDisplaySettingsQueryRelY(HANDLE hKey, int * pResult);
NTSTATUS vboxWddmDisplaySettingsQueryPos(IN PDEVICE_EXTENSION pDeviceExtension, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, POINT * pPos);
NTSTATUS vboxWddmRegQueryVideoGuidString(ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult);

NTSTATUS vboxWddmRegQueryDrvKeyName(PDEVICE_EXTENSION pDevExt, ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult);

NTSTATUS vboxWddmRegOpenKey(OUT PHANDLE phKey, IN PWCHAR pName, IN ACCESS_MASK fAccess);
NTSTATUS vboxWddmRegQueryValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT PDWORD pDword);
NTSTATUS vboxWddmRegSetValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT DWORD val);
#endif /* #ifndef ___VBoxVideoMisc_h__ */
