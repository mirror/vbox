/* $Id$ */
/** @file
 * VirtualBox Video miniport driver for NT/2k/XP - HGSMI related functions.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


#include "VBoxVideo.h"
#include "Helper.h"

#include <iprt/asm.h>
#include <iprt/log.h>
#include <iprt/thread.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/VBoxVideo.h>

#include <VBoxDisplay.h>

#include "vboxioctl.h"

#define MEM_TAG 'HVBV'

void HGSMINotifyHostCmdComplete (PDEVICE_EXTENSION PrimaryExtension, HGSMIOFFSET offt)
{
    VBoxHGSMIHostWrite(PrimaryExtension, offt);
}

void HGSMIClearIrq (PDEVICE_EXTENSION PrimaryExtension)
{
    VBoxHGSMIHostWrite(PrimaryExtension, HGSMIOFFSET_VOID);
}

static void HGSMIHostCmdComplete (PDEVICE_EXTENSION PrimaryExtension, void * pvMem)
{
	HGSMIOFFSET offMem = HGSMIPointerToOffset (&PrimaryExtension->u.primary.areaHostHeap, HGSMIBufferHeaderFromData (pvMem));
	Assert(offMem != HGSMIOFFSET_VOID);
	if(offMem != HGSMIOFFSET_VOID)
	{
		HGSMINotifyHostCmdComplete (PrimaryExtension, offMem);
	}
}

static void hgsmiHostCmdProcess(PDEVICE_EXTENSION PrimaryExtension, HGSMIOFFSET offBuffer)
{
	int rc = HGSMIBufferProcess (&PrimaryExtension->u.primary.areaHostHeap,
								&PrimaryExtension->u.primary.channels,
								offBuffer);
	Assert(!RT_FAILURE(rc));
	if(RT_FAILURE(rc))
	{
		/* failure means the command was not submitted to the handler for some reason
		 * it's our responsibility to notify its completion in this case */
		HGSMINotifyHostCmdComplete(PrimaryExtension, offBuffer);
	}
	/* if the cmd succeeded it's responsibility of the callback to complete it */
}

static HGSMIOFFSET hgsmiGetHostBuffer (PDEVICE_EXTENSION PrimaryExtension)
{
    return VBoxHGSMIHostRead(PrimaryExtension);
}

static void hgsmiHostCommandQueryProcess (PDEVICE_EXTENSION PrimaryExtension)
{
	HGSMIOFFSET offset = hgsmiGetHostBuffer (PrimaryExtension);
	Assert(offset != HGSMIOFFSET_VOID);
	if(offset != HGSMIOFFSET_VOID)
	{
		hgsmiHostCmdProcess(PrimaryExtension, offset);
	}
}

#define VBOX_HGSMI_LOCK(_pe, _plock, _dpc, _pold) \
    do { \
        if(_dpc) { \
            (_pe)->u.primary.VideoPortProcs.pfnAcquireSpinLockAtDpcLevel(_pe, _plock); \
        } \
        else {\
            (_pe)->u.primary.VideoPortProcs.pfnAcquireSpinLock(_pe, _plock, _pold); \
        }\
    } while(0)

#define VBOX_HGSMI_UNLOCK(_pe, _plock, _dpc, _pold) \
    do { \
        if(_dpc) { \
            (_pe)->u.primary.VideoPortProcs.pfnReleaseSpinLockFromDpcLevel(_pe, _plock); \
        } \
        else {\
            (_pe)->u.primary.VideoPortProcs.pfnReleaseSpinLock(_pe, _plock, _pold); \
        }\
    } while(0)

VOID VBoxVideoHGSMIDpc(
    IN PVOID  HwDeviceExtension,
    IN PVOID  Context
    )
{
    PDEVICE_EXTENSION PrimaryExtension = (PDEVICE_EXTENSION)HwDeviceExtension;
    uint32_t flags = (uint32_t)Context;
    bool bProcessing = false;
    UCHAR OldIrql;
    /* we check if another thread is processing the queue and exit if so */
    do
    {
        bool bLock = false;
        if(!(PrimaryExtension->u.primary.pHostFlags->u32HostFlags & HGSMIHOSTFLAGS_COMMANDS_PENDING))
        {
            if(!bProcessing)
            {
                break;
            }
            VBOX_HGSMI_LOCK(PrimaryExtension, PrimaryExtension->u.primary.pSynchLock, flags, &OldIrql);
            if(!(PrimaryExtension->u.primary.pHostFlags->u32HostFlags & HGSMIHOSTFLAGS_COMMANDS_PENDING))
            {
            	Assert(PrimaryExtension->u.primary.bHostCmdProcessing);
                PrimaryExtension->u.primary.bHostCmdProcessing = false;
                VBOX_HGSMI_UNLOCK(PrimaryExtension, PrimaryExtension->u.primary.pSynchLock, flags, OldIrql);
                break;
            }
            VBOX_HGSMI_UNLOCK(PrimaryExtension, PrimaryExtension->u.primary.pSynchLock, flags, OldIrql);
        }
        else
        {
            if(!bProcessing)
            {
                VBOX_HGSMI_LOCK(PrimaryExtension, PrimaryExtension->u.primary.pSynchLock, flags, &OldIrql);
                if(!(PrimaryExtension->u.primary.pHostFlags->u32HostFlags & HGSMIHOSTFLAGS_COMMANDS_PENDING)
                        || PrimaryExtension->u.primary.bHostCmdProcessing)
                {
                    VBOX_HGSMI_UNLOCK(PrimaryExtension, PrimaryExtension->u.primary.pSynchLock, flags, OldIrql);
                    break;
                }
                Assert(!PrimaryExtension->u.primary.bHostCmdProcessing);
                PrimaryExtension->u.primary.bHostCmdProcessing = true;
                VBOX_HGSMI_UNLOCK(PrimaryExtension, PrimaryExtension->u.primary.pSynchLock, flags, OldIrql);
                bProcessing = true;
            }
        }

        Assert(bProcessing);
        Assert(PrimaryExtension->u.primary.bHostCmdProcessing);
        Assert((PrimaryExtension->u.primary.pHostFlags->u32HostFlags & HGSMIHOSTFLAGS_COMMANDS_PENDING) != 0);
        bProcessing = true;

        hgsmiHostCommandQueryProcess (PrimaryExtension);
    } while(true/*!PrimaryExtension->u.primary.bPollingStop*/);
}

/* Detect whether HGSMI is supported by the host. */
BOOLEAN VBoxHGSMIIsSupported (PDEVICE_EXTENSION PrimaryExtension)
{
    USHORT DispiId;

    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
    VideoPortWritePortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, VBE_DISPI_ID_HGSMI);

    DispiId = VideoPortReadPortUshort((PUSHORT)VBE_DISPI_IOPORT_DATA);

    return (DispiId == VBE_DISPI_ID_HGSMI);
}

typedef int FNHGSMICALLINIT (PDEVICE_EXTENSION PrimaryExtension, void *pvContext, void *pvData);
typedef FNHGSMICALLINIT *PFNHGSMICALLINIT;

typedef int FNHGSMICALLFINALIZE (PDEVICE_EXTENSION PrimaryExtension, void *pvContext, void *pvData);
typedef FNHGSMICALLFINALIZE *PFNHGSMICALLFINALIZE;

static int vboxCallChannel (PDEVICE_EXTENSION PrimaryExtension,
						 uint8_t u8Ch,
                         uint16_t u16Op,
                         HGSMISIZE cbData,
                         PFNHGSMICALLINIT pfnInit,
                         PFNHGSMICALLFINALIZE pfnFinalize,
                         void *pvContext)
{
    int rc = VINF_SUCCESS;

    /* Allocate the IO buffer. */
    if (PrimaryExtension->pPrimary != PrimaryExtension)
    {
        dprintf(("VBoxVideo::vboxCallChannel: not primary extension %p!!!\n", PrimaryExtension));
        return VERR_INVALID_PARAMETER;
    }

    void *p = HGSMIHeapAlloc (&PrimaryExtension->u.primary.hgsmiAdapterHeap, cbData, u8Ch, u16Op);

    if (!p)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        /* Prepare data to be sent to the host. */
        if (pfnInit)
        {
            rc = pfnInit (PrimaryExtension, pvContext, p);
        }

        if (RT_SUCCESS (rc))
        {
            /* Initialize the buffer and get the offset for port IO. */
            HGSMIOFFSET offBuffer = HGSMIHeapBufferOffset (&PrimaryExtension->u.primary.hgsmiAdapterHeap,
                                                           p);

            /* Submit the buffer to the host. */
            VBoxHGSMIGuestWrite(PrimaryExtension, offBuffer);

            if (pfnFinalize)
            {
                rc = pfnFinalize (PrimaryExtension, pvContext, p);
            }
        }
        else
        {
            AssertFailed ();
            rc = VERR_INTERNAL_ERROR;
        }

        /* Free the IO buffer. */
        HGSMIHeapFree (&PrimaryExtension->u.primary.hgsmiAdapterHeap, p);
    }

    return rc;
}

static int vboxCallVBVA (PDEVICE_EXTENSION PrimaryExtension,
                         uint16_t u16Op,
                         HGSMISIZE cbData,
                         PFNHGSMICALLINIT pfnInit,
                         PFNHGSMICALLFINALIZE pfnFinalize,
                         void *pvContext)
{
	return vboxCallChannel (PrimaryExtension,
			HGSMI_CH_VBVA,
	                          u16Op,
	                          cbData,
	                          pfnInit,
	                          pfnFinalize,
	                         pvContext);
}

typedef struct _QUERYCONFCTX
{
    uint32_t u32Index;
    ULONG *pulValue;
} QUERYCONFCTX;

static int vbvaInitQueryConf (PDEVICE_EXTENSION PrimaryExtension, void *pvContext, void *pvData)
{
    NOREF (PrimaryExtension);

    QUERYCONFCTX *pCtx = (QUERYCONFCTX *)pvContext;
    VBVACONF32 *p = (VBVACONF32 *)pvData;

    p->u32Index = pCtx->u32Index;
    p->u32Value = 0;

    return VINF_SUCCESS;
}

static int vbvaFinalizeQueryConf (PDEVICE_EXTENSION PrimaryExtension, void *pvContext, void *pvData)
{
    NOREF (PrimaryExtension);

    QUERYCONFCTX *pCtx = (QUERYCONFCTX *)pvContext;
    VBVACONF32 *p = (VBVACONF32 *)pvData;

    if (pCtx->pulValue)
    {
        *pCtx->pulValue = p->u32Value;
    }

    dprintf(("VBoxVideo::vboxQueryConf: u32Value = %d\n", p->u32Value));
    return VINF_SUCCESS;
}

static int vboxQueryConfHGSMI (PDEVICE_EXTENSION PrimaryExtension, uint32_t u32Index, ULONG *pulValue)
{
    dprintf(("VBoxVideo::vboxQueryConf: u32Index = %d\n", u32Index));

    QUERYCONFCTX context;

    context.u32Index = u32Index;
    context.pulValue = pulValue;

    int rc = vboxCallVBVA (PrimaryExtension,
                           VBVA_QUERY_CONF32,
                           sizeof (VBVACONF32),
                           vbvaInitQueryConf,
                           vbvaFinalizeQueryConf,
                           &context);

    dprintf(("VBoxVideo::vboxQueryConf: rc = %d\n", rc));

    return rc;
}

static int vbvaInitInfoDisplay (PDEVICE_EXTENSION PrimaryExtension, void *pvContext, void *pvData)
{
    NOREF (pvContext);
    VBVAINFOVIEW *p = (VBVAINFOVIEW *)pvData;

    int i;
    PDEVICE_EXTENSION Extension;

    for (i = 0, Extension = PrimaryExtension;
         i < PrimaryExtension->u.primary.cDisplays && Extension;
         i++, Extension = Extension->pNext)
    {
        p[i].u32ViewIndex     = Extension->iDevice;
        p[i].u32ViewOffset    = Extension->ulFrameBufferOffset;
        p[i].u32ViewSize      = PrimaryExtension->u.primary.ulMaxFrameBufferSize;

        /* How much VRAM should be reserved for the guest drivers to use VBVA. */
        const uint32_t cbReservedVRAM = VBVA_DISPLAY_INFORMATION_SIZE + VBVA_MIN_BUFFER_SIZE;

        p[i].u32MaxScreenSize = p[i].u32ViewSize > cbReservedVRAM?
                                    p[i].u32ViewSize - cbReservedVRAM:
                                    0;
    }

    if (i == PrimaryExtension->u.primary.cDisplays && Extension == NULL)
    {
        return VINF_SUCCESS;
    }

    AssertFailed ();
    return VERR_INTERNAL_ERROR;
}

static int vbvaInitInfoHeap (PDEVICE_EXTENSION PrimaryExtension, void *pvContext, void *pvData)
{
    NOREF (pvContext);
    VBVAINFOHEAP *p = (VBVAINFOHEAP *)pvData;

    p->u32HeapOffset = PrimaryExtension->u.primary.cbVRAM
                       - PrimaryExtension->u.primary.cbMiniportHeap
                       - VBVA_ADAPTER_INFORMATION_SIZE;
    p->u32HeapSize = PrimaryExtension->u.primary.cbMiniportHeap;

    return VINF_SUCCESS;
}

static int hgsmiInitFlagsLocation (PDEVICE_EXTENSION PrimaryExtension, void *pvContext, void *pvData)
{
    NOREF (pvContext);
    HGSMIBUFFERLOCATION *p = (HGSMIBUFFERLOCATION *)pvData;

    p->offLocation = PrimaryExtension->u.primary.cbVRAM - sizeof (HGSMIHOSTFLAGS);
    p->cbLocation = sizeof (HGSMIHOSTFLAGS);

    return VINF_SUCCESS;
}


static int vboxSetupAdapterInfoHGSMI (PDEVICE_EXTENSION PrimaryExtension)
{
    dprintf(("VBoxVideo::vboxSetupAdapterInfo\n"));

    /* setup the flags first to ensure they are initialized by the time the host heap is ready */
    int rc = vboxCallChannel(PrimaryExtension,
            HGSMI_CH_HGSMI,
            HGSMI_CC_HOST_FLAGS_LOCATION,
                       sizeof (HGSMIBUFFERLOCATION),
                       hgsmiInitFlagsLocation,
                       NULL,
                       NULL);
    AssertRC(rc);
    if(RT_SUCCESS (rc))
    {
        rc = vboxCallVBVA (PrimaryExtension,
                               VBVA_INFO_VIEW,
                               sizeof (VBVAINFOVIEW) * PrimaryExtension->u.primary.cDisplays,
                               vbvaInitInfoDisplay,
                               NULL,
                               NULL);
        AssertRC(rc);
        if (RT_SUCCESS (rc))
        {
            /* Report the host heap location. */
            rc = vboxCallVBVA (PrimaryExtension,
                               VBVA_INFO_HEAP,
                               sizeof (VBVAINFOHEAP),
                               vbvaInitInfoHeap,
                               NULL,
                               NULL);
            AssertRC(rc);
        }
    }


    dprintf(("VBoxVideo::vboxSetupAdapterInfo finished rc = %d\n", rc));

    return rc;
}

VP_STATUS vboxWaitForSingleObjectVoid(IN PVOID  HwDeviceExtension, IN PVOID  Object, IN PLARGE_INTEGER  Timeout  OPTIONAL)
{
    return ERROR_INVALID_FUNCTION;
}

LONG vboxSetEventVoid(IN PVOID  HwDeviceExtension, IN PEVENT  pEvent)
{
    return 0;
}

VOID vboxClearEventVoid (IN PVOID  HwDeviceExtension, IN PEVENT  pEvent)
{
}

VP_STATUS vboxCreateEventVoid(IN PVOID  HwDeviceExtension, IN ULONG  EventFlag, IN PVOID  Unused, OUT PEVENT  *ppEvent)
{
    return ERROR_INVALID_FUNCTION;
}

VP_STATUS vboxDeleteEventVoid(IN PVOID  HwDeviceExtension, IN PEVENT  pEvent)
{
    return ERROR_INVALID_FUNCTION;
}

VP_STATUS vboxCreateSpinLockVoid (IN PVOID  HwDeviceExtension, OUT PSPIN_LOCK  *SpinLock)
{
    return ERROR_INVALID_FUNCTION;
}

VP_STATUS vboxDeleteSpinLockVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock)
{
    return ERROR_INVALID_FUNCTION;
}

VOID vboxAcquireSpinLockVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock, OUT PUCHAR  OldIrql)
{
}

VOID vboxReleaseSpinLockVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock, IN UCHAR  NewIrql)
{
}

VOID vboxAcquireSpinLockAtDpcLevelVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock)
{
}

VOID vboxReleaseSpinLockFromDpcLevelVoid (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock)
{
}

void VBoxSetupVideoPortFunctions(PDEVICE_EXTENSION PrimaryExtension, VBOXVIDEOPORTPROCS *pCallbacks, PVIDEO_PORT_CONFIG_INFO pConfigInfo)
{
    memset(pCallbacks, 0, sizeof(VBOXVIDEOPORTPROCS));

    if (vboxQueryWinVersion() <= WINNT4)
    {
        /* VideoPortGetProcAddress is available for >= win2k */
        return;
    }

    pCallbacks->pfnWaitForSingleObject = (PFNWAITFORSINGLEOBJECT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortWaitForSingleObject");
    Assert(pCallbacks->pfnWaitForSingleObject);

    pCallbacks->pfnSetEvent = (PFNSETEVENT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortSetEvent");
    Assert(pCallbacks->pfnSetEvent);

    pCallbacks->pfnClearEvent = (PFNCLEAREVENT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortClearEvent");
    Assert(pCallbacks->pfnClearEvent);

    pCallbacks->pfnCreateEvent = (PFNCREATEEVENT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortCreateEvent");
    Assert(pCallbacks->pfnCreateEvent);

    pCallbacks->pfnDeleteEvent = (PFNDELETEEVENT)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortDeleteEvent");
    Assert(pCallbacks->pfnDeleteEvent);

    if(pCallbacks->pfnWaitForSingleObject
            && pCallbacks->pfnSetEvent
            && pCallbacks->pfnClearEvent
            && pCallbacks->pfnCreateEvent
            && pCallbacks->pfnDeleteEvent)
    {
        pCallbacks->fSupportedTypes |= VBOXVIDEOPORTPROCS_EVENT;
    }
    else
    {
        pCallbacks->pfnWaitForSingleObject = vboxWaitForSingleObjectVoid;
        pCallbacks->pfnSetEvent = vboxSetEventVoid;
        pCallbacks->pfnClearEvent = vboxClearEventVoid;
        pCallbacks->pfnCreateEvent = vboxCreateEventVoid;
        pCallbacks->pfnDeleteEvent = vboxDeleteEventVoid;
    }

    pCallbacks->pfnCreateSpinLock = (PFNCREATESPINLOCK)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortCreateSpinLock");
    Assert(pCallbacks->pfnCreateSpinLock);

    pCallbacks->pfnDeleteSpinLock = (PFNDELETESPINLOCK)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortDeleteSpinLock");
    Assert(pCallbacks->pfnDeleteSpinLock);

    pCallbacks->pfnAcquireSpinLock = (PFNACQUIRESPINLOCK)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortAcquireSpinLock");
    Assert(pCallbacks->pfnAcquireSpinLock);

    pCallbacks->pfnReleaseSpinLock = (PFNRELEASESPINLOCK)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortReleaseSpinLock");
    Assert(pCallbacks->pfnReleaseSpinLock);

    pCallbacks->pfnAcquireSpinLockAtDpcLevel = (PFNACQUIRESPINLOCKATDPCLEVEL)(pConfigInfo->VideoPortGetProcAddress)
            (PrimaryExtension,
             (PUCHAR)"VideoPortAcquireSpinLockAtDpcLevel");
    Assert(pCallbacks->pfnAcquireSpinLockAtDpcLevel);

    pCallbacks->pfnReleaseSpinLockFromDpcLevel = (PFNRELEASESPINLOCKFROMDPCLEVEL)(pConfigInfo->VideoPortGetProcAddress)
                (PrimaryExtension,
                 (PUCHAR)"VideoPortReleaseSpinLockFromDpcLevel");
    Assert(pCallbacks->pfnReleaseSpinLockFromDpcLevel);

    if(pCallbacks->pfnCreateSpinLock
            && pCallbacks->pfnDeleteSpinLock
            && pCallbacks->pfnAcquireSpinLock
            && pCallbacks->pfnReleaseSpinLock
            && pCallbacks->pfnAcquireSpinLockAtDpcLevel
            && pCallbacks->pfnReleaseSpinLockFromDpcLevel)
    {
        pCallbacks->fSupportedTypes |= VBOXVIDEOPORTPROCS_SPINLOCK;
    }
    else
    {
        pCallbacks->pfnCreateSpinLock = vboxCreateSpinLockVoid;
        pCallbacks->pfnDeleteSpinLock = vboxDeleteSpinLockVoid;
        pCallbacks->pfnAcquireSpinLock = vboxAcquireSpinLockVoid;
        pCallbacks->pfnReleaseSpinLock = vboxReleaseSpinLockVoid;
        pCallbacks->pfnAcquireSpinLockAtDpcLevel = vboxAcquireSpinLockAtDpcLevelVoid;
        pCallbacks->pfnReleaseSpinLockFromDpcLevel = vboxReleaseSpinLockFromDpcLevelVoid;
    }

    Assert(pCallbacks->fSupportedTypes & VBOXVIDEOPORTPROCS_EVENT);
    Assert(pCallbacks->fSupportedTypes & VBOXVIDEOPORTPROCS_SPINLOCK);
}

/**
 * Helper function to register secondary displays (DualView). Note that this will not
 * be available on pre-XP versions, and some editions on XP will fail because they are
 * intentionally crippled.
 *
 * HGSMI variant is a bit different because it uses only HGSMI interface (VBVA channel)
 * to talk to the host.
 */
VOID VBoxSetupDisplaysHGSMI(PDEVICE_EXTENSION PrimaryExtension, PVIDEO_PORT_CONFIG_INFO pConfigInfo, ULONG AdapterMemorySize)
{
    VP_STATUS rc = NO_ERROR;

    dprintf(("VBoxVideo::VBoxSetupDisplays: PrimaryExtension = %p\n",
             PrimaryExtension));

    /* Preinitialize the primary extension.
     * Note: bVBoxVideoSupported is set to FALSE, because HGSMI is active instead.
     */
    PrimaryExtension->pNext                              = NULL;
    PrimaryExtension->pPrimary                           = PrimaryExtension;
    PrimaryExtension->iDevice                            = 0;
    PrimaryExtension->ulFrameBufferOffset                = 0;
    PrimaryExtension->ulFrameBufferSize                  = 0;
    PrimaryExtension->u.primary.ulVbvaEnabled            = 0;
    PrimaryExtension->u.primary.bVBoxVideoSupported      = FALSE;
    PrimaryExtension->u.primary.cDisplays                = 1;
    PrimaryExtension->u.primary.cbVRAM                   = AdapterMemorySize;
    PrimaryExtension->u.primary.cbMiniportHeap           = 0;
    PrimaryExtension->u.primary.pvMiniportHeap           = NULL;
    PrimaryExtension->u.primary.pvAdapterInformation     = NULL;
    PrimaryExtension->u.primary.ulMaxFrameBufferSize     = 0;
    PrimaryExtension->u.primary.bHGSMI                   = VBoxHGSMIIsSupported (PrimaryExtension);
    VideoPortZeroMemory(&PrimaryExtension->u.primary.areaHostHeap, sizeof(HGSMIAREA));
    VideoPortZeroMemory(&PrimaryExtension->areaDisplay, sizeof(HGSMIAREA));

    if (PrimaryExtension->u.primary.IOPortGuest == 0)
    {
        PrimaryExtension->u.primary.bHGSMI = false;
    }

    if (PrimaryExtension->u.primary.bHGSMI)
    {
        /* Map the adapter information. It will be needed for HGSMI IO. */
        rc = VBoxMapAdapterMemory (PrimaryExtension,
                                   &PrimaryExtension->u.primary.pvAdapterInformation,
                                   PrimaryExtension->u.primary.cbVRAM - VBVA_ADAPTER_INFORMATION_SIZE,
                                   VBVA_ADAPTER_INFORMATION_SIZE
                                  );
        if (rc != NO_ERROR)
        {
            dprintf(("VBoxVideo::VBoxSetupDisplays: VBoxMapAdapterMemory pvAdapterInfoirrmation failed rc = %d\n",
                     rc));

            PrimaryExtension->u.primary.bHGSMI = FALSE;
        }
        else
        {
            /* Setup a HGSMI heap within the adapter information area. */
            rc = HGSMIHeapSetup (&PrimaryExtension->u.primary.hgsmiAdapterHeap,
                                 PrimaryExtension->u.primary.pvAdapterInformation,
                                 VBVA_ADAPTER_INFORMATION_SIZE - sizeof(HGSMIHOSTFLAGS),
                                 PrimaryExtension->u.primary.cbVRAM - VBVA_ADAPTER_INFORMATION_SIZE);

            if (RT_FAILURE (rc))
            {
                dprintf(("VBoxVideo::VBoxSetupDisplays: HGSMIHeapSetup failed rc = %d\n",
                         rc));

                PrimaryExtension->u.primary.bHGSMI = FALSE;
            }
            else
            {
                    PrimaryExtension->u.primary.pHostFlags = (HGSMIHOSTFLAGS*)(((uint8_t*)PrimaryExtension->u.primary.pvAdapterInformation)
                                                            + VBVA_ADAPTER_INFORMATION_SIZE - sizeof(HGSMIHOSTFLAGS));
            }
        }
    }

    /* Setup the host heap and the adapter memory. */
    if (PrimaryExtension->u.primary.bHGSMI)
    {
        /* The miniport heap is used for the host buffers. */
        ULONG cbMiniportHeap = 0;
        vboxQueryConfHGSMI (PrimaryExtension, VBOX_VBVA_CONF32_HOST_HEAP_SIZE, &cbMiniportHeap);

        if (cbMiniportHeap != 0)
        {
            /* Do not allow too big heap. No more than 25% of VRAM is allowed. */
            ULONG cbMiniportHeapMaxSize = AdapterMemorySize / 4;

            if (cbMiniportHeapMaxSize >= VBVA_ADAPTER_INFORMATION_SIZE)
            {
                cbMiniportHeapMaxSize -= VBVA_ADAPTER_INFORMATION_SIZE;
            }

            if (cbMiniportHeap > cbMiniportHeapMaxSize)
            {
                cbMiniportHeap = cbMiniportHeapMaxSize;
            }

            /* Round up to 4096 bytes. */
            PrimaryExtension->u.primary.cbMiniportHeap = (cbMiniportHeap + 0xFFF) & ~0xFFF;

            dprintf(("VBoxVideo::VBoxSetupDisplays: cbMiniportHeap = 0x%08X, PrimaryExtension->u.primary.cbMiniportHeap = 0x%08X, cbMiniportHeapMaxSize = 0x%08X\n",
                     cbMiniportHeap, PrimaryExtension->u.primary.cbMiniportHeap, cbMiniportHeapMaxSize));

            /* Map the heap region.
             *
             * Note: the heap will be used for the host buffers submitted to the guest.
             *       The miniport driver is responsible for reading FIFO and notifying
             *       display drivers.
             */
            rc = VBoxMapAdapterMemory (PrimaryExtension,
                                       &PrimaryExtension->u.primary.pvMiniportHeap,
                                       PrimaryExtension->u.primary.cbVRAM
                                       - VBVA_ADAPTER_INFORMATION_SIZE
                                       - PrimaryExtension->u.primary.cbMiniportHeap,
                                       PrimaryExtension->u.primary.cbMiniportHeap
                                      );
            if (rc != NO_ERROR)
            {
                PrimaryExtension->u.primary.pvMiniportHeap = NULL;
                PrimaryExtension->u.primary.cbMiniportHeap = 0;
                PrimaryExtension->u.primary.bHGSMI = FALSE;
            }
            else
            {
                HGSMIOFFSET offBase = PrimaryExtension->u.primary.cbVRAM
                                      - VBVA_ADAPTER_INFORMATION_SIZE
                                      - PrimaryExtension->u.primary.cbMiniportHeap;

                /* Init the host hap area. Buffers from the host will be placed there. */
                HGSMIAreaInitialize (&PrimaryExtension->u.primary.areaHostHeap,
                                     PrimaryExtension->u.primary.pvMiniportHeap,
                                     PrimaryExtension->u.primary.cbMiniportHeap,
                                     offBase);
            }
        }
        else
        {
            /* Host has not requested a heap. */
            PrimaryExtension->u.primary.pvMiniportHeap = NULL;
            PrimaryExtension->u.primary.cbMiniportHeap = 0;
        }
    }

    /* Check whether the guest supports multimonitors. */
    if (PrimaryExtension->u.primary.bHGSMI)
    {
        typedef VP_STATUS (*PFNCREATESECONDARYDISPLAY)(PVOID, PVOID *, ULONG);
        PFNCREATESECONDARYDISPLAY pfnCreateSecondaryDisplay = NULL;

        /* Dynamically query the VideoPort import to be binary compatible across Windows versions */
        if (vboxQueryWinVersion() > WINNT4)
        {
            /* This bluescreens on NT4, hence the above version check */
            pfnCreateSecondaryDisplay = (PFNCREATESECONDARYDISPLAY)(pConfigInfo->VideoPortGetProcAddress)
                                                                       (PrimaryExtension,
                                                                        (PUCHAR)"VideoPortCreateSecondaryDisplay");
        }

        if (pfnCreateSecondaryDisplay != NULL)
        {
            /* Query the configured number of displays. */
            ULONG cDisplays = 0;
            vboxQueryConfHGSMI (PrimaryExtension, VBOX_VBVA_CONF32_MONITOR_COUNT, &cDisplays);

            dprintf(("VBoxVideo::VBoxSetupDisplays: cDisplays = %d\n",
                     cDisplays));

            if (cDisplays == 0 || cDisplays > VBOX_VIDEO_MAX_SCREENS)
            {
                /* Host reported some bad value. Continue in the 1 screen mode. */
                cDisplays = 1;
            }

            PDEVICE_EXTENSION pPrev = PrimaryExtension;

            ULONG iDisplay;
            for (iDisplay = 1; iDisplay < cDisplays; iDisplay++)
            {
               PDEVICE_EXTENSION SecondaryExtension = NULL;
               rc = pfnCreateSecondaryDisplay (PrimaryExtension, (PVOID*)&SecondaryExtension, VIDEO_DUALVIEW_REMOVABLE);

               dprintf(("VBoxVideo::VBoxSetupDisplays: VideoPortCreateSecondaryDisplay returned %#x, SecondaryExtension = %p\n",
                        rc, SecondaryExtension));

               if (rc != NO_ERROR)
               {
                   break;
               }

               SecondaryExtension->pNext                = NULL;
               SecondaryExtension->pPrimary             = PrimaryExtension;
               SecondaryExtension->iDevice              = iDisplay;
               SecondaryExtension->ulFrameBufferOffset  = 0;
               SecondaryExtension->ulFrameBufferSize    = 0;
               SecondaryExtension->u.secondary.bEnabled = FALSE;

               /* Update the list pointers. */
               pPrev->pNext = SecondaryExtension;
               pPrev = SecondaryExtension;

               /* Take the successfully created display into account. */
               PrimaryExtension->u.primary.cDisplays++;
            }
        }

        /* Failure to create secondary displays is not fatal */
        rc = NO_ERROR;
    }

    /* Now when the number of monitors is known and extensions are created,
     * calculate the layout of framebuffers.
     */
    VBoxComputeFrameBufferSizes (PrimaryExtension);

    if (PrimaryExtension->u.primary.bHGSMI)
    {
        /* Setup the information for the host. */
        rc = vboxSetupAdapterInfoHGSMI (PrimaryExtension);

        if (RT_FAILURE (rc))
        {
            PrimaryExtension->u.primary.bHGSMI = FALSE;
        }
    }

    if (!PrimaryExtension->u.primary.bHGSMI)
    {
        /* Unmap the memory if VBoxVideo is not supported. */
        VBoxUnmapAdapterMemory (PrimaryExtension, &PrimaryExtension->u.primary.pvMiniportHeap);
        VBoxUnmapAdapterMemory (PrimaryExtension, &PrimaryExtension->u.primary.pvAdapterInformation);

        HGSMIHeapDestroy (&PrimaryExtension->u.primary.hgsmiAdapterHeap);
    }

    if (PrimaryExtension->u.primary.bHGSMI)
    {
        PrimaryExtension->u.primary.VideoPortProcs.pfnCreateSpinLock(PrimaryExtension, &PrimaryExtension->u.primary.pSynchLock);
    }

    dprintf(("VBoxVideo::VBoxSetupDisplays: finished\n"));
}


/*
 * Send the pointer shape to the host.
 */
typedef struct _MOUSEPOINTERSHAPECTX
{
    VIDEO_POINTER_ATTRIBUTES *pPointerAttr;
    uint32_t cbData;
    int32_t i32Result;
} MOUSEPOINTERSHAPECTX;

static int vbvaInitMousePointerShape (PDEVICE_EXTENSION PrimaryExtension, void *pvContext, void *pvData)
{
    NOREF (PrimaryExtension);

    MOUSEPOINTERSHAPECTX *pCtx = (MOUSEPOINTERSHAPECTX *)pvContext;
    VBVAMOUSEPOINTERSHAPE *p = (VBVAMOUSEPOINTERSHAPE *)pvData;

    /* Will be updated by the host. */
    p->i32Result = VINF_SUCCESS;

    /* We have our custom flags in the field */
    p->fu32Flags = pCtx->pPointerAttr->Enable & 0x0000FFFF;

    /* Even if pointer is invisible, we have to pass following data,
     * so host could create the pointer with initial status - invisible
     */
    p->u32HotX   = (pCtx->pPointerAttr->Enable >> 16) & 0xFF;
    p->u32HotY   = (pCtx->pPointerAttr->Enable >> 24) & 0xFF;
    p->u32Width  = pCtx->pPointerAttr->Width;
    p->u32Height = pCtx->pPointerAttr->Height;

    if (p->fu32Flags & VBOX_MOUSE_POINTER_SHAPE)
    {
        /* Copy the actual pointer data. */
        memcpy (p->au8Data, pCtx->pPointerAttr->Pixels, pCtx->cbData);
    }

    return VINF_SUCCESS;
}

static int vbvaFinalizeMousePointerShape (PDEVICE_EXTENSION PrimaryExtension, void *pvContext, void *pvData)
{
    NOREF (PrimaryExtension);

    MOUSEPOINTERSHAPECTX *pCtx = (MOUSEPOINTERSHAPECTX *)pvContext;
    VBVAMOUSEPOINTERSHAPE *p = (VBVAMOUSEPOINTERSHAPE *)pvData;

    pCtx->i32Result = p->i32Result;

    return VINF_SUCCESS;
}

BOOLEAN vboxUpdatePointerShape (PDEVICE_EXTENSION PrimaryExtension,
                                PVIDEO_POINTER_ATTRIBUTES pointerAttr,
                                uint32_t cbLength)
{
    uint32_t cbData = 0;

    if (pointerAttr->Enable & VBOX_MOUSE_POINTER_SHAPE)
    {
        /* Size of the pointer data: sizeof (AND mask) + sizeof (XOR_MASK) */
        cbData = ((((pointerAttr->Width + 7) / 8) * pointerAttr->Height + 3) & ~3)
                 + pointerAttr->Width * 4 * pointerAttr->Height;
    }

    dprintf(("vboxUpdatePointerShape: cbData %d, %dx%d\n",
             cbData, pointerAttr->Width, pointerAttr->Height));

    if (cbData > cbLength - sizeof(VIDEO_POINTER_ATTRIBUTES))
    {
        dprintf(("vboxUpdatePointerShape: calculated pointer data size is too big (%d bytes, limit %d)\n",
                 cbData, cbLength - sizeof(VIDEO_POINTER_ATTRIBUTES)));
        return FALSE;
    }

    MOUSEPOINTERSHAPECTX ctx;

    ctx.pPointerAttr = pointerAttr;
    ctx.cbData = cbData;
    ctx.i32Result = VERR_NOT_SUPPORTED;

    int rc = vboxCallVBVA (PrimaryExtension,
                           VBVA_MOUSE_POINTER_SHAPE,
                           sizeof (VBVAMOUSEPOINTERSHAPE) + cbData,
                           vbvaInitMousePointerShape,
                           vbvaFinalizeMousePointerShape,
                           &ctx);

    dprintf(("VBoxVideo::vboxMousePointerShape: rc %d, i32Result = %d\n", rc, ctx.i32Result));

    return RT_SUCCESS(rc) && RT_SUCCESS(ctx.i32Result);
}

typedef struct _VBVAMINIPORT_CHANNELCONTEXT
{
    PFNHGSMICHANNELHANDLER pfnChannelHandler;
	void *pvChannelHandler;
}VBVAMINIPORT_CHANNELCONTEXT;

typedef struct _VBVADISP_CHANNELCONTEXT
{
    struct _VBVAHOSTCMD * pFirstCmd;
    struct _VBVAHOSTCMD * pLastCmd;
    PSPIN_LOCK pSynchLock;
#ifdef DEBUG
    int cCmds;
#endif
    bool bValid;
}VBVADISP_CHANNELCONTEXT;

#ifdef DEBUG
void dbgCheckListLocked(const VBVADISP_CHANNELCONTEXT *pList, struct _VBVAHOSTCMD * pCmd)
{
    int counter = 0;
    for(struct _VBVAHOSTCMD * pCur = pList->pFirstCmd; pCur; pCur=pCur->u.pNext)
    {
    	Assert(pCur != pCmd);
    	if(pCur == pList->pLastCmd)
    	{
    		Assert(pCur->u.pNext == NULL);
    	}
    	if(pCur->u.pNext == NULL)
    	{
    		Assert(pCur == pList->pLastCmd);
    	}
    	counter++;
    }

    Assert(counter == pList->cCmds);
}

void dbgCheckList(PDEVICE_EXTENSION PrimaryExtension, const VBVADISP_CHANNELCONTEXT *pList, struct _VBVAHOSTCMD * pCmd)
{
    UCHAR oldIrql;
    PrimaryExtension->u.primary.VideoPortProcs.pfnAcquireSpinLock(PrimaryExtension,
    		pList->pSynchLock,
            &oldIrql);

    dbgCheckListLocked(pList, pCmd);

    PrimaryExtension->u.primary.VideoPortProcs.pfnReleaseSpinLock(PrimaryExtension,
    		pList->pSynchLock,
            oldIrql);
}

#define DBG_CHECKLIST_LOCKED(_pl, pc) dbgCheckListLocked(_pl, pc)
#define DBG_CHECKLIST(_pe, _pl, pc) dbgCheckList(_pe, _pl, pc)

#else
#define DBG_CHECKLIST_LOCKED(_pl, pc) do{}while(0)
#define DBG_CHECKLIST(_pe, _pl, pc) do{}while(0)
#endif


typedef struct _VBVA_CHANNELCONTEXTS
{
    PDEVICE_EXTENSION PrimaryExtension;
	uint32_t cUsed;
	uint32_t cContexts;
	VBVAMINIPORT_CHANNELCONTEXT mpContext;
	VBVADISP_CHANNELCONTEXT aContexts[1];
}VBVA_CHANNELCONTEXTS;

static int vboxVBVADeleteChannelContexts(PDEVICE_EXTENSION PrimaryExtension, VBVA_CHANNELCONTEXTS * pContext)
{
    VideoPortFreePool(PrimaryExtension,pContext);
	return VINF_SUCCESS;
}

static int vboxVBVACreateChannelContexts(PDEVICE_EXTENSION PrimaryExtension, VBVA_CHANNELCONTEXTS ** ppContext)
{
	uint32_t cDisplays = (uint32_t)PrimaryExtension->u.primary.cDisplays;
	const size_t size = RT_OFFSETOF(VBVA_CHANNELCONTEXTS, aContexts[cDisplays]);
	VBVA_CHANNELCONTEXTS * pContext = (VBVA_CHANNELCONTEXTS*)VideoPortAllocatePool(PrimaryExtension,
	        VpNonPagedPool,
	        size,
	        MEM_TAG);
	if(pContext)
	{
		memset(pContext, 0, size);
		pContext->cContexts = cDisplays;
		pContext->PrimaryExtension = PrimaryExtension;
		*ppContext = pContext;
		return VINF_SUCCESS;
	}
	return VERR_GENERAL_FAILURE;
}

static VBVADISP_CHANNELCONTEXT* vboxVBVAFindHandlerInfo(VBVA_CHANNELCONTEXTS *pCallbacks, int iId)
{
	if(iId < 0)
	{
		return NULL;
	}
	else if(pCallbacks->cContexts > (uint32_t)iId)
	{
		return &pCallbacks->aContexts[iId];
	}
	return NULL;
}

DECLCALLBACK(void) hgsmiHostCmdComplete (HVBOXVIDEOHGSMI hHGSMI, struct _VBVAHOSTCMD * pCmd)
{
    PDEVICE_EXTENSION PrimaryExtension = ((PDEVICE_EXTENSION)hHGSMI)->pPrimary;
    HGSMIHostCmdComplete (PrimaryExtension, pCmd);
}

DECLCALLBACK(int) hgsmiHostCmdRequest (HVBOXVIDEOHGSMI hHGSMI, uint8_t u8Channel, struct _VBVAHOSTCMD ** ppCmd)
{
//    if(display < 0)
//        return VERR_INVALID_PARAMETER;
    if(!ppCmd)
        return VERR_INVALID_PARAMETER;

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hHGSMI;
    PDEVICE_EXTENSION PrimaryExtension = pDevExt->pPrimary;

    /* pick up the host commands */
    VBoxVideoHGSMIDpc(PrimaryExtension, NULL);

    HGSMICHANNEL * pChannel = HGSMIChannelFindById (&PrimaryExtension->u.primary.channels, u8Channel);
    if(pChannel)
    {
        VBVA_CHANNELCONTEXTS * pContexts = (VBVA_CHANNELCONTEXTS *)pChannel->handler.pvHandler;
        VBVADISP_CHANNELCONTEXT *pDispContext = vboxVBVAFindHandlerInfo(pContexts, pDevExt->iDevice);
        Assert(pDispContext);
        if(pDispContext)
        {
            UCHAR oldIrql;
            PrimaryExtension->u.primary.VideoPortProcs.pfnAcquireSpinLock(PrimaryExtension,
                    pDispContext->pSynchLock,
                    &oldIrql);

            DBG_CHECKLIST_LOCKED(pDispContext, NULL);

            *ppCmd = pDispContext->pFirstCmd;
            pDispContext->pFirstCmd = NULL;
            pDispContext->pLastCmd = NULL;
#ifdef DEBUG
            pDispContext->cCmds = 0;
#endif
            PrimaryExtension->u.primary.VideoPortProcs.pfnReleaseSpinLock(PrimaryExtension,
                    pDispContext->pSynchLock,
                    oldIrql);

            DBG_CHECKLIST(PrimaryExtension, pDispContext, NULL);

            return VINF_SUCCESS;
        }
    }

    return VERR_INVALID_PARAMETER;
}

static DECLCALLBACK(int) vboxVBVAChannelGenericHandler(void *pvHandler, uint16_t u16ChannelInfo, void *pvBuffer, HGSMISIZE cbBuffer)
{
    VBVA_CHANNELCONTEXTS *pCallbacks = (VBVA_CHANNELCONTEXTS*)pvHandler;
//    Assert(0);
    Assert(cbBuffer > VBVAHOSTCMD_HDRSIZE);
	if(cbBuffer > VBVAHOSTCMD_HDRSIZE)
	{
	    VBVAHOSTCMD *pHdr = (VBVAHOSTCMD*)pvBuffer;
	    Assert(pHdr->iDstID >= 0);
	    if(pHdr->iDstID >= 0)
	    {
	        VBVADISP_CHANNELCONTEXT* pHandler = vboxVBVAFindHandlerInfo(pCallbacks, pHdr->iDstID);
	        Assert(pHandler && pHandler->bValid);
	        if(pHandler && pHandler->bValid)
	        {
                VBVAHOSTCMD *pFirst = NULL, *pLast = NULL;
                for(VBVAHOSTCMD *pCur = pHdr; pCur; )
                {
                    Assert(!pCur->u.Data);
                    Assert(!pFirst);
                    Assert(!pLast);

                    switch(u16ChannelInfo)
                    {
                        case VBVAHG_DISPLAY_CUSTOM:
                        {
                            if(pLast)
                            {
                                pLast->u.pNext = pCur;
                                pLast = pCur;
                            }
                            else
                            {
                                pFirst = pCur;
                                pLast = pCur;
                            }
                            Assert(!pCur->u.Data);
                            //TODO: use offset here
                            pCur = pCur->u.pNext;
                            Assert(!pCur);
                            Assert(pFirst);
                            Assert(pFirst == pLast);
                            break;
                        }
                        case VBVAHG_EVENT:
                        {
                            VBVAHOSTCMDEVENT *pEventCmd = VBVAHOSTCMD_BODY(pCur, VBVAHOSTCMDEVENT);
                            PEVENT pEvent = (PEVENT)pEventCmd->pEvent;
                            pCallbacks->PrimaryExtension->u.primary.VideoPortProcs.pfnSetEvent(
                                    pCallbacks->PrimaryExtension,
                                    pEvent);
                        }
                        default:
                        {
        					DBG_CHECKLIST(pCallbacks->PrimaryExtension, pHandler, pCur);
                            Assert(u16ChannelInfo==VBVAHG_EVENT);
                            Assert(!pCur->u.Data);
                            //TODO: use offset here
                            if(pLast)
                                pLast->u.pNext = pCur->u.pNext;
                            VBVAHOSTCMD * pNext = pCur->u.pNext;
                            pCur->u.pNext = NULL;
                            HGSMIHostCmdComplete(pCallbacks->PrimaryExtension, pCur);
                            pCur = pNext;
                            Assert(!pCur);
                            Assert(!pFirst);
                            Assert(pFirst == pLast);
                            break;
                        }
                    }
                }

				DBG_CHECKLIST(pCallbacks->PrimaryExtension, pHandler, pFirst);

                /* we do not support lists currently */
                Assert(pFirst == pLast);
                if(pLast)
                {
                	Assert(pLast->u.pNext == NULL);
                }

                if(pFirst)
                {
                	Assert(pLast);
					UCHAR oldIrql;
					pCallbacks->PrimaryExtension->u.primary.VideoPortProcs.pfnAcquireSpinLock(pCallbacks->PrimaryExtension,
							pHandler->pSynchLock,
							&oldIrql);

					DBG_CHECKLIST_LOCKED(pHandler, pFirst);

					if(pHandler->pLastCmd)
					{
						pHandler->pLastCmd->u.pNext = pFirst;
						Assert(pHandler->pFirstCmd);
					}
					else
					{
						Assert(!pHandler->pFirstCmd);
						pHandler->pFirstCmd = pFirst;
					}
					pHandler->pLastCmd = pLast;
#ifdef DEBUG
					pHandler->cCmds++;
#endif
					DBG_CHECKLIST_LOCKED(pHandler, NULL);

					pCallbacks->PrimaryExtension->u.primary.VideoPortProcs.pfnReleaseSpinLock(pCallbacks->PrimaryExtension,
							pHandler->pSynchLock,
							oldIrql);
                }
                else
                {
                	Assert(!pLast);
                }
                return VINF_SUCCESS;
	        }
	    }
	    else
	    {
	        //TODO: impl
//	        HGSMIMINIPORT_CHANNELCONTEXT *pHandler = vboxVideoHGSMIFindHandler;
//	         if(pHandler && pHandler->pfnChannelHandler)
//	         {
//	             pHandler->pfnChannelHandler(pHandler->pvChannelHandler, u16ChannelInfo, pHdr, cbBuffer);
//
//	             return VINF_SUCCESS;
//	         }
	    }
	}
	/* no handlers were found, need to complete the command here */
	HGSMIHostCmdComplete(pCallbacks->PrimaryExtension, pvBuffer);
	return VINF_SUCCESS;
}

static HGSMICHANNELHANDLER g_OldHandler;

int vboxVBVAChannelDisplayEnable(PDEVICE_EXTENSION PrimaryExtension,
		int iDisplay, /* negative would mean this is a miniport handler */
        uint8_t u8Channel)
{
    VBVA_CHANNELCONTEXTS * pContexts;
	HGSMICHANNEL * pChannel = HGSMIChannelFindById (&PrimaryExtension->u.primary.channels, u8Channel);
	if(!pChannel)
	{
		int rc = vboxVBVACreateChannelContexts(PrimaryExtension, &pContexts);
		if(RT_FAILURE(rc))
		{
			return rc;
		}
	}
	else
	{
	    pContexts = (VBVA_CHANNELCONTEXTS *)pChannel->handler.pvHandler;
	}

	VBVADISP_CHANNELCONTEXT *pDispContext = vboxVBVAFindHandlerInfo(pContexts, iDisplay);
	Assert(pDispContext);
	if(pDispContext)
	{
	    Assert(!pDispContext->bValid);
	    Assert(!pDispContext->pFirstCmd);
	    Assert(!pDispContext->pLastCmd);
		if(!pDispContext->bValid)
		{
		    pDispContext->bValid = true;
		    pDispContext->pFirstCmd = NULL;
            pDispContext->pLastCmd= NULL;
#ifdef DEBUG
            pDispContext->cCmds = 0;
#endif

            PrimaryExtension->u.primary.VideoPortProcs.pfnCreateSpinLock(PrimaryExtension, &pDispContext->pSynchLock);

			int rc = VINF_SUCCESS;
			if(!pChannel)
			{
				rc = HGSMIChannelRegister (&PrimaryExtension->u.primary.channels,
				                                 u8Channel,
				                                 "VGA Miniport HGSMI channel",
				                                 vboxVBVAChannelGenericHandler,
				                                 pContexts,
				                                 &g_OldHandler);
			}

			if(RT_SUCCESS(rc))
			{
				pContexts->cUsed++;
				return VINF_SUCCESS;
			}
		}
	}

	if(!pChannel)
	{
	    vboxVBVADeleteChannelContexts(PrimaryExtension, pContexts);
	}

	return VERR_GENERAL_FAILURE;
}

/** @todo Mouse pointer position to be read from VMMDev memory, address of the memory region
 * can be queried from VMMDev via an IOCTL. This VMMDev memory region will contain
 * host information which is needed by the guest.
 *
 * Reading will not cause a switch to the host.
 *
 * Have to take into account:
 *  * synchronization: host must write to the memory only from EMT,
 *    large structures must be read under flag, which tells the host
 *    that the guest is currently reading the memory (OWNER flag?).
 *  * guest writes: may be allocate a page for the host info and make
 *    the page readonly for the guest.
 *  * the information should be available only for additions drivers.
 *  * VMMDev additions driver will inform the host which version of the info it expects,
 *    host must support all versions.
 *
 */


