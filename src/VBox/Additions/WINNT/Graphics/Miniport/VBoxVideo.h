/** @file
 * VirtualBox Video miniport driver
 *
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

#ifndef VBOXVIDEO_H
#define VBOXVIDEO_H

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>

//#include <iprt/thread.h>

#include <VBox/HGSMI/HGSMI.h>
#include <VBox/HGSMI/HGSMIChSetup.h>
#include <VBox/VBoxVideo.h>
#include "VBoxHGSMI.h"

RT_C_DECLS_BEGIN
#ifndef VBOX_WITH_WDDM
#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"
#include "ntddvdeo.h"
#include "video.h"
#else
#   ifdef PAGE_SIZE
#    undef PAGE_SIZE
#   endif
#   ifdef PAGE_SHIFT
#    undef PAGE_SHIFT
#   endif
#   define VBOX_WITH_WORKAROUND_MISSING_PACK
#   if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#       define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#       define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#       define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#       define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#       define _interlockedbittestandset      _interlockedbittestandset_StupidDDKVsCompilerCrap
#       define _interlockedbittestandreset    _interlockedbittestandreset_StupidDDKVsCompilerCrap
#       define _interlockedbittestandset64    _interlockedbittestandset64_StupidDDKVsCompilerCrap
#       define _interlockedbittestandreset64  _interlockedbittestandreset64_StupidDDKVsCompilerCrap
#       pragma warning(disable : 4163)
#       ifdef VBOX_WITH_WORKAROUND_MISSING_PACK
#          pragma warning(disable : 4103)
#       endif
#       include <ntddk.h>
#       pragma warning(default : 4163)
#       ifdef VBOX_WITH_WORKAROUND_MISSING_PACK
#         pragma pack()
#         pragma warning(default : 4103)
#       endif
#       undef  _InterlockedExchange
#       undef  _InterlockedExchangeAdd
#       undef  _InterlockedCompareExchange
#       undef  _InterlockedAddLargeStatistic
#       undef  _interlockedbittestandset
#       undef  _interlockedbittestandreset
#       undef  _interlockedbittestandset64
#       undef  _interlockedbittestandreset64
#   else
#       include <ntddk.h>
#   endif
#include "dispmprt.h"
#include "ntddvdeo.h"
#include "dderror.h"
#endif
RT_C_DECLS_END

#define VBE_DISPI_IOPORT_INDEX          0x01CE
#define VBE_DISPI_IOPORT_DATA           0x01CF
#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_INDEX_X_OFFSET        0x8
#define VBE_DISPI_INDEX_Y_OFFSET        0x9
#define VBE_DISPI_INDEX_VBOX_VIDEO      0xa

#define VBE_DISPI_ID2                   0xB0C2
/* The VBOX interface id. Indicates support for VBE_DISPI_INDEX_VBOX_VIDEO. */
#define VBE_DISPI_ID_VBOX_VIDEO         0xBE00
#define VBE_DISPI_ID_HGSMI              0xBE01
#define VBE_DISPI_ID_ANYX               0xBE02
#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_LFB_ENABLED           0x40
#define VBE_DISPI_LFB_PHYSICAL_ADDRESS  0xE0000000
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_MB 4
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_KB         (VBE_DISPI_TOTAL_VIDEO_MEMORY_MB * 1024)
#define VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES      (VBE_DISPI_TOTAL_VIDEO_MEMORY_KB * 1024)

#define VGA_PORT_HGSMI_HOST  0x3b0
#define VGA_PORT_HGSMI_GUEST 0x3d0

/* common API types */
#ifdef VBOX_WITH_WDDM
#define VBOX_WITH_GENERIC_MULTIMONITOR
typedef struct _DEVICE_EXTENSION *PDEVICE_EXTENSION;
#include <VBox/VBoxVideo.h>
#include "wddm/VBoxVideoIf.h"
#include "wddm/VBoxVideoMisc.h"
#include "wddm/VBoxVideoShgsmi.h"
#include "wddm/VBoxVideoCm.h"
#include "wddm/VBoxVideoVdma.h"
#include "wddm/VBoxVideoWddm.h"
#include "wddm/VBoxVideoVidPn.h"
#ifdef VBOXWDDM_WITH_VBVA
# include "wddm/VBoxVideoVbva.h"
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
# include "wddm/VBoxVideoVhwa.h"
#endif

#define VBOXWDDM_POINTER_ATTRIBUTES_SIZE VBOXWDDM_ROUNDBOUND( \
        VBOXWDDM_ROUNDBOUND( sizeof (VIDEO_POINTER_ATTRIBUTES), 4 ) + \
        VBOXWDDM_ROUNDBOUND(VBOXWDDM_C_POINTER_MAX_WIDTH * VBOXWDDM_C_POINTER_MAX_HEIGHT * 4, 4) + \
        VBOXWDDM_ROUNDBOUND((VBOXWDDM_C_POINTER_MAX_WIDTH * VBOXWDDM_C_POINTER_MAX_HEIGHT + 7) >> 3, 4) \
         , 8)

typedef struct VBOXWDDM_POINTER_INFO
{
    uint32_t xPos;
    uint32_t yPos;
    union
    {
        VIDEO_POINTER_ATTRIBUTES data;
        char buffer[VBOXWDDM_POINTER_ATTRIBUTES_SIZE];
    } Attributes;
} VBOXWDDM_POINTER_INFO, *PVBOXWDDM_POINTER_INFO;

typedef struct VBOXWDDM_GLOBAL_POINTER_INFO
{
    uint32_t cVisible;
} VBOXWDDM_GLOBAL_POINTER_INFO, *PVBOXWDDM_GLOBAL_POINTER_INFO;

#ifdef VBOX_WITH_VIDEOHWACCEL
typedef struct VBOXWDDM_VHWA
{
    VBOXVHWA_INFO Settings;
    volatile uint32_t cOverlaysCreated;
} VBOXWDDM_VHWA;
#endif

typedef struct VBOXWDDM_SOURCE
{
    struct VBOXWDDM_ALLOCATION * pPrimaryAllocation;
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
    struct VBOXWDDM_ALLOCATION * pShadowAllocation;
    VBOXVIDEOOFFSET offVram;
    VBOXWDDM_SURFACE_DESC SurfDesc;
    VBOXVBVAINFO Vbva;
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* @todo: in our case this seems more like a target property,
     * but keep it here for now */
    VBOXWDDM_VHWA Vhwa;
    volatile uint32_t cOverlays;
    LIST_ENTRY OverlayList;
    KSPIN_LOCK OverlayListLock;
#endif
    POINT VScreenPos;
    VBOXWDDM_POINTER_INFO PointerInfo;
} VBOXWDDM_SOURCE, *PVBOXWDDM_SOURCE;

typedef struct VBOXWDDM_TARGET
{
    uint32_t ScanLineState;
    uint32_t HeightVisible;
    uint32_t HeightTotal;
} VBOXWDDM_TARGET, *PVBOXWDDM_TARGET;

#endif

typedef struct VBOXVIDEO_COMMON
{
    int cDisplays;                      /* Number of displays. */

    ULONG cbVRAM;                       /* The VRAM size. */

    ULONG cbMiniportHeap;               /* The size of reserved VRAM for miniport driver heap.
                                         * It is at offset:
                                         *   cbAdapterMemorySize - VBOX_VIDEO_ADAPTER_INFORMATION_SIZE - cbMiniportHeap
                                         */
    PVOID pvMiniportHeap;               /* The pointer to the miniport heap VRAM.
                                         * This is mapped by miniport separately.
                                         */
    volatile HGSMIHOSTFLAGS * pHostFlags; /* HGSMI host flags */
    volatile bool bHostCmdProcessing;

    PVOID pvAdapterInformation;         /* The pointer to the last 4K of VRAM.
                                         * This is mapped by miniport separately.
                                         */

    /** Host HGSMI capabilities the guest can handle */
    uint32_t fCaps;

    BOOLEAN bHGSMI;                     /* Whether HGSMI is enabled. */

    HGSMIAREA areaHostHeap;             /* Host heap VRAM area. */

    HGSMICHANNELINFO channels;

    HGSMIHEAP hgsmiAdapterHeap;

    /* The IO Port Number for host commands. */
    RTIOPORT IOPortHost;

    /* The IO Port Number for guest commands. */
    RTIOPORT IOPortGuest;
} VBOXVIDEO_COMMON, *PVBOXVIDEO_COMMON;

typedef struct _DEVICE_EXTENSION
{
   struct _DEVICE_EXTENSION *pNext;            /* Next extension in the DualView extension list.
                                                * The primary extension is the first one.
                                                */
#ifndef VBOX_WITH_WDDM
   struct _DEVICE_EXTENSION *pPrimary;         /* Pointer to the primary device extension. */

   ULONG iDevice;                              /* Device index: 0 for primary, otherwise a secondary device. */

   ULONG CurrentMode;                          /* Saved information about video modes */
   ULONG CurrentModeWidth;
   ULONG CurrentModeHeight;
   ULONG CurrentModeBPP;

   ULONG ulFrameBufferOffset;                  /* The framebuffer position in the VRAM. */
   ULONG ulFrameBufferSize;                    /* The size of the current framebuffer. */
#endif
   union {
       /* Information that is only relevant to the primary device or is the same for all devices. */
       struct {

           void *pvReqFlush;                   /* Pointer to preallocated generic request structure for
                                                * VMMDevReq_VideoAccelFlush. Allocated when VBVA status
                                                * is changed. Deallocated on HwReset.
                                                */


           ULONG ulVbvaEnabled;                /* Indicates that VBVA mode is enabled. */

#ifdef VBOX_WITH_WDDM
           VBOXVDMAINFO Vdma;
# ifdef VBOXVDMA_WITH_VBVA
           VBOXVBVAINFO Vbva;
# endif
#endif
           ULONG ulMaxFrameBufferSize;         /* The size of the VRAM allocated for the a single framebuffer. */

           BOOLEAN fMouseHidden;               /* Has the mouse cursor been hidden by the guest? */


           VBOXVIDEO_COMMON commonInfo;
#ifndef VBOX_WITH_WDDM
           /* Video Port API dynamically picked up at runtime for binary backwards compatibility with older NT versions */
           VBOXVIDEOPORTPROCS VideoPortProcs;
#else
           /* committed VidPn handle */
           D3DKMDT_HVIDPN hCommittedVidPn;
           /* Display Port handle and callbacks */
           DXGKRNL_INTERFACE DxgkInterface;
#endif
       } primary;

       /* Secondary device information. */
       struct {
           BOOLEAN bEnabled;                   /* Device enabled flag */
       } secondary;
   } u;

   HGSMIAREA areaDisplay;                      /* Entire VRAM chunk for this display device. */

#ifdef VBOX_WITH_WDDM
   PDEVICE_OBJECT pPDO;
   UNICODE_STRING RegKeyName;
   UNICODE_STRING VideoGuid;

   uint8_t * pvVisibleVram;

   VBOXVIDEOCM_MGR CmMgr;
   /* hgsmi allocation manager */
   VBOXVIDEOCM_ALLOC_MGR AllocMgr;
   VBOXVDMADDI_CMD_QUEUE DdiCmdQueue;
   LIST_ENTRY SwapchainList3D;
   /* mutex for context list operations */
   FAST_MUTEX ContextMutex;
   KSPIN_LOCK SynchLock;
   volatile uint32_t cContexts3D;
   volatile uint32_t cUnlockedVBVADisabled;

   VBOXWDDM_GLOBAL_POINTER_INFO PointerInfo;

   VBOXSHGSMILIST CtlList;
   VBOXSHGSMILIST DmaCmdList;
#ifdef VBOX_WITH_VIDEOHWACCEL
   VBOXSHGSMILIST VhwaCmdList;
#endif
//   BOOL bSetNotifyDxDpc;
   BOOL bNotifyDxDpc;

   VBOXWDDM_SOURCE aSources[VBOX_VIDEO_MAX_SCREENS];
   VBOXWDDM_TARGET aTargets[VBOX_VIDEO_MAX_SCREENS];
#endif
   BOOLEAN fAnyX;   /* Unrestricted horizontal resolution flag. */
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

static inline PVBOXVIDEO_COMMON commonFromDeviceExt(PDEVICE_EXTENSION pExt)
{
#ifndef VBOX_WITH_WDDM
    return &pExt->pPrimary->u.primary.commonInfo;
#else
    return &pExt->u.primary.commonInfo;
#endif
}

static inline PDEVICE_EXTENSION commonToPrimaryExt(PVBOXVIDEO_COMMON pCommon)
{
    return RT_FROM_MEMBER(pCommon, DEVICE_EXTENSION, u.primary.commonInfo);
}

#ifndef VBOX_WITH_WDDM
#define DEV_MOUSE_HIDDEN(dev) ((dev)->pPrimary->u.primary.fMouseHidden)
#define DEV_SET_MOUSE_HIDDEN(dev)   \
do { \
    (dev)->pPrimary->u.primary.fMouseHidden = TRUE; \
} while (0)
#define DEV_SET_MOUSE_SHOWN(dev)   \
do { \
    (dev)->pPrimary->u.primary.fMouseHidden = FALSE; \
} while (0)
#else
#define DEV_MOUSE_HIDDEN(dev) ((dev)->u.primary.fMouseHidden)
#define DEV_SET_MOUSE_HIDDEN(dev)   \
do { \
    (dev)->u.primary.fMouseHidden = TRUE; \
} while (0)
#define DEV_SET_MOUSE_SHOWN(dev)   \
do { \
    (dev)->u.primary.fMouseHidden = FALSE; \
} while (0)
#endif
extern "C"
{
/** Signal an event in a guest-OS-specific way.  pvEvent will be re-cast to
 * something OS-specific. */
void VBoxVideoCmnSignalEvent(PVBOXVIDEO_COMMON pCommon, uint64_t pvEvent);

/** Allocate memory to be used in normal driver operation (dispatch level in
 * Windows) but not necessarily in IRQ context. */
void *VBoxVideoCmnMemAllocDriver(PVBOXVIDEO_COMMON pCommon, size_t cb);

/** Free memory allocated by @a VBoxVideoCmnMemAllocDriver */
void VBoxVideoCmnMemFreeDriver(PVBOXVIDEO_COMMON pCommon, void *pv);

/** Write an 8-bit value to an I/O port. */
void VBoxVideoCmnPortWriteUchar(RTIOPORT Port, uint8_t Value);

/** Write a 16-bit value to an I/O port. */
void VBoxVideoCmnPortWriteUshort(RTIOPORT Port, uint16_t Value);

/** Write a 32-bit value to an I/O port. */
void VBoxVideoCmnPortWriteUlong(RTIOPORT Port, uint32_t Value);

/** Read an 8-bit value from an I/O port. */
uint8_t VBoxVideoCmnPortReadUchar(RTIOPORT Port);

/** Read a 16-bit value from an I/O port. */
uint16_t VBoxVideoCmnPortReadUshort(RTIOPORT Port);

/** Read a 32-bit value from an I/O port. */
uint32_t VBoxVideoCmnPortReadUlong(RTIOPORT Port);

#ifndef VBOX_WITH_WDDM

RT_C_DECLS_BEGIN
ULONG DriverEntry(IN PVOID Context1, IN PVOID Context2);
RT_C_DECLS_END

#else

RT_C_DECLS_BEGIN
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );
RT_C_DECLS_END

PVBOXWDDM_VIDEOMODES_INFO vboxWddmGetVideoModesInfo(PDEVICE_EXTENSION DeviceExtension, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId);
PVBOXWDDM_VIDEOMODES_INFO vboxWddmGetAllVideoModesInfos(PDEVICE_EXTENSION DeviceExtension);

void vboxVideoInitCustomVideoModes(PDEVICE_EXTENSION pDevExt);

VOID vboxWddmInvalidateVideoModesInfo(PDEVICE_EXTENSION DeviceExtension);

/* @return STATUS_BUFFER_TOO_SMALL - if buffer is too small, STATUS_SUCCESS - on success */
NTSTATUS vboxWddmGetModesForResolution(VIDEO_MODE_INFORMATION *pAllModes, uint32_t cAllModes, int iSearchPreferredMode,
        const D3DKMDT_2DREGION *pResolution, VIDEO_MODE_INFORMATION * pModes, uint32_t cModes, uint32_t *pcModes, int32_t *piPreferrableMode);

int vboxWddmVideoModeFind(const VIDEO_MODE_INFORMATION *pModes, int cModes, const VIDEO_MODE_INFORMATION *pM);
int vboxWddmVideoResolutionFind(const D3DKMDT_2DREGION *pResolutions, int cResolutions, const D3DKMDT_2DREGION *pRes);
bool vboxWddmVideoResolutionsMatch(const D3DKMDT_2DREGION *pResolutions1, const D3DKMDT_2DREGION *pResolutions2, int cResolutions);
bool vboxWddmVideoModesMatch(const VIDEO_MODE_INFORMATION *pModes1, const VIDEO_MODE_INFORMATION *pModes2, int cModes);

D3DDDIFORMAT vboxWddmCalcPixelFormat(const VIDEO_MODE_INFORMATION *pInfo);
bool vboxWddmFillMode(VIDEO_MODE_INFORMATION *pInfo, D3DDDIFORMAT enmFormat, ULONG w, ULONG h);

DECLINLINE(ULONG) vboxWddmVramCpuVisibleSize(PDEVICE_EXTENSION pDevExt)
{
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
    /* all memory layout info should be initialized */
    Assert(pDevExt->aSources[0].Vbva.offVBVA);
    /* page aligned */
    Assert(!(pDevExt->aSources[0].Vbva.offVBVA & 0xfff));

    return (ULONG)(pDevExt->aSources[0].Vbva.offVBVA & ~0xfffULL);
#else
    /* all memory layout info should be initialized */
    Assert(pDevExt->u.primary.Vdma.CmdHeap.area.offBase);
    /* page aligned */
    Assert(!(pDevExt->u.primary.Vdma.CmdHeap.area.offBase & 0xfff));

    return pDevExt->u.primary.Vdma.CmdHeap.area.offBase & ~0xfffUL;
#endif
}

DECLINLINE(ULONG) vboxWddmVramCpuVisibleSegmentSize(PDEVICE_EXTENSION pDevExt)
{
    return vboxWddmVramCpuVisibleSize(pDevExt);
}

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
DECLINLINE(ULONG) vboxWddmVramCpuInvisibleSegmentSize(PDEVICE_EXTENSION pDevExt)
{
    return vboxWddmVramCpuVisibleSegmentSize(pDevExt);
}

DECLINLINE(bool) vboxWddmCmpSurfDescsBase(VBOXWDDM_SURFACE_DESC *pDesc1, VBOXWDDM_SURFACE_DESC *pDesc2)
{
    if (pDesc1->width != pDesc2->width)
        return false;
    if (pDesc1->height != pDesc2->height)
        return false;
    if (pDesc1->format != pDesc2->format)
        return false;
    if (pDesc1->bpp != pDesc2->bpp)
        return false;
    if (pDesc1->pitch != pDesc2->pitch)
        return false;
    return true;
}

DECLINLINE(void) vboxWddmAssignShadow(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SOURCE pSource, PVBOXWDDM_ALLOCATION pAllocation, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    if (pSource->pShadowAllocation == pAllocation)
    {
        Assert(pAllocation->bAssigned);
        return;
    }

    if (pSource->pShadowAllocation)
    {
        PVBOXWDDM_ALLOCATION pOldAlloc = pSource->pShadowAllocation;
        /* clear the visibility info fo the current primary */
        pOldAlloc->bVisible = FALSE;
        pOldAlloc->bAssigned = FALSE;
        Assert(pOldAlloc->SurfDesc.VidPnSourceId == srcId);
        /* release the shadow surface */
        pOldAlloc->SurfDesc.VidPnSourceId = D3DDDI_ID_UNINITIALIZED;
    }

    if (pAllocation)
    {
        Assert(!pAllocation->bAssigned);
        Assert(!pAllocation->bVisible);
        pAllocation->bVisible = FALSE;
        /* this check ensures the shadow is not used for other source simultaneously */
        Assert(pAllocation->SurfDesc.VidPnSourceId == D3DDDI_ID_UNINITIALIZED);
        pAllocation->SurfDesc.VidPnSourceId = srcId;
        pAllocation->bAssigned = TRUE;
        if (!vboxWddmCmpSurfDescsBase(&pSource->SurfDesc, &pAllocation->SurfDesc))
            pSource->offVram = VBOXVIDEOOFFSET_VOID; /* force guest->host notification */
        pSource->SurfDesc = pAllocation->SurfDesc;
    }

    pSource->pShadowAllocation = pAllocation;
}
#endif

DECLINLINE(VOID) vboxWddmAssignPrimary(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SOURCE pSource, PVBOXWDDM_ALLOCATION pAllocation, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    if (pSource->pPrimaryAllocation == pAllocation)
        return;

    if (pSource->pPrimaryAllocation)
    {
        PVBOXWDDM_ALLOCATION pOldAlloc = pSource->pPrimaryAllocation;
        /* clear the visibility info fo the current primary */
        pOldAlloc->bVisible = FALSE;
        pOldAlloc->bAssigned = FALSE;
        Assert(pOldAlloc->SurfDesc.VidPnSourceId == srcId);
    }

    if (pAllocation)
    {
        pAllocation->bVisible = FALSE;
        Assert(pAllocation->SurfDesc.VidPnSourceId == srcId);
        pAllocation->SurfDesc.VidPnSourceId = srcId;
        pAllocation->bAssigned = TRUE;
    }

    pSource->pPrimaryAllocation = pAllocation;
}


#endif

void* vboxHGSMIBufferAlloc(PVBOXVIDEO_COMMON pCommon,
                         HGSMISIZE cbData,
                         uint8_t u8Ch,
                         uint16_t u16Op);
void vboxHGSMIBufferFree (PVBOXVIDEO_COMMON pCommon, void *pvBuffer);
int vboxHGSMIBufferSubmit (PVBOXVIDEO_COMMON pCommon, void *pvBuffer);

BOOLEAN FASTCALL VBoxVideoSetCurrentModePerform(PDEVICE_EXTENSION DeviceExtension,
        USHORT width, USHORT height, USHORT bpp
#ifdef VBOX_WITH_WDDM
        , ULONG offDisplay
#endif
        );

BOOLEAN FASTCALL VBoxVideoSetCurrentMode(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE RequestedMode,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoResetDevice(
   PDEVICE_EXTENSION DeviceExtension,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoMapVideoMemory(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MEMORY RequestedAddress,
   PVIDEO_MEMORY_INFORMATION MapInformation,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoUnmapVideoMemory(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MEMORY VideoMemory,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoQueryNumAvailModes(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_NUM_MODES Modes,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoQueryAvailModes(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE_INFORMATION ReturnedModes,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoQueryCurrentMode(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_MODE_INFORMATION VideoModeInfo,
   PSTATUS_BLOCK StatusBlock);

BOOLEAN FASTCALL VBoxVideoSetColorRegisters(
   PDEVICE_EXTENSION DeviceExtension,
   PVIDEO_CLUT ColorLookUpTable,
   PSTATUS_BLOCK StatusBlock);

int VBoxMapAdapterMemory (PVBOXVIDEO_COMMON pCommon,
                          void **ppv,
                          ULONG ulOffset,
                          ULONG ulSize);

void VBoxUnmapAdapterMemory (PVBOXVIDEO_COMMON pCommon, void **ppv);

typedef bool(*PFNVIDEOIRQSYNC)(void *);
bool VBoxSyncToVideoIRQ(PVBOXVIDEO_COMMON pCommon, PFNVIDEOIRQSYNC pfnSync,
                        void *pvUser);

void VBoxComputeFrameBufferSizes (PDEVICE_EXTENSION PrimaryExtension);


/*
 * Host and Guest port IO helpers.
 */
DECLINLINE(void) VBoxHGSMIHostWrite(PVBOXVIDEO_COMMON pCommon, ULONG data)
{
    VBoxVideoCmnPortWriteUlong(pCommon->IOPortHost, data);
}

DECLINLINE(ULONG) VBoxHGSMIHostRead(PVBOXVIDEO_COMMON pCommon)
{
    return VBoxVideoCmnPortReadUlong(pCommon->IOPortHost);
}

DECLINLINE(void) VBoxHGSMIGuestWrite(PVBOXVIDEO_COMMON pCommon, ULONG data)
{
    VBoxVideoCmnPortWriteUlong(pCommon->IOPortGuest, data);
}

DECLINLINE(ULONG) VBoxHGSMIGuestRead(PVBOXVIDEO_COMMON pCommon)
{
    return VBoxVideoCmnPortReadUlong(pCommon->IOPortGuest);
}


BOOLEAN VBoxHGSMIIsSupported (void);

typedef int FNHGSMIFILLVIEWINFO (void *pvData, VBVAINFOVIEW *pInfo);
typedef FNHGSMIFILLVIEWINFO *PFNHGSMIFILLVIEWINFO;

int VBoxHGSMISendViewInfo(PVBOXVIDEO_COMMON pCommon, uint32_t u32Count, PFNHGSMIFILLVIEWINFO pfnFill, void *pvData);

VOID VBoxSetupDisplaysHGSMI (PVBOXVIDEO_COMMON pCommon,
                             ULONG AdapterMemorySize, uint32_t fCaps);
BOOLEAN vboxUpdatePointerShape (PVBOXVIDEO_COMMON pCommon,
                                PVIDEO_POINTER_ATTRIBUTES pointerAttr,
                                uint32_t cbLength);

void VBoxFreeDisplaysHGSMI(PVBOXVIDEO_COMMON pCommon);
#ifndef VBOX_WITH_WDDM
DECLCALLBACK(void) hgsmiHostCmdComplete (HVBOXVIDEOHGSMI hHGSMI, struct _VBVAHOSTCMD * pCmd);
DECLCALLBACK(int) hgsmiHostCmdRequest (HVBOXVIDEOHGSMI hHGSMI, uint8_t u8Channel, uint32_t iDisplay, struct _VBVAHOSTCMD ** ppCmd);
#endif


int vboxVBVAChannelDisplayEnable(PVBOXVIDEO_COMMON pCommon,
        int iDisplay, /* negative would mean this is a miniport handler */
        uint8_t u8Channel);

void hgsmiProcessHostCommandQueue(PVBOXVIDEO_COMMON pCommon);

void HGSMIClearIrq (PVBOXVIDEO_COMMON pCommon);

} /* extern "C" */

#endif /* VBOXVIDEO_H */
