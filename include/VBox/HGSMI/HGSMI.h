/** @file
 *
 * VBox Host Guest Shared Memory Interface (HGSMI).
 * Host/Guest shared part.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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


#ifndef __HGSMI_h__
#define __HGSMI_h__

#include <iprt/assert.h>
#include <iprt/types.h>

#include <VBox/HGSMI/HGSMIChannels.h>

/* HGSMI uses 32 bit offsets and sizes. */
typedef uint32_t HGSMISIZE;
typedef uint32_t HGSMIOFFSET;

#define HGSMIOFFSET_VOID ((HGSMIOFFSET)~0)

/*
 * Basic mechanism for the HGSMI is to prepare and pass data buffer to the host and the guest.
 * Data inside these buffers are opaque for the HGSMI and are interpreted by higher levels.
 *
 * Every shared memory buffer passed between the guest/host has the following structure:
 *
 * HGSMIBUFFERHEADER header;
 * uint8_t data[header.u32BufferSize];
 * HGSMIBUFFERTAIL tail;
 *
 * Note: Offset of the 'header' in the memory is used for virtual hardware IO.
 *
 * Buffers are verifyed using the offset and the content of the header and the tail,
 * which are constant during a call.
 *
 * Invalid buffers are ignored.
 *
 * Actual 'data' is not verifyed, as it is expected that the data can be changed by the
 * called function.
 *
 * Since only the offset of the buffer is passed in a IO operation, the header and tail
 * must contain:
 *     * size of data in this buffer;
 *     * checksum for buffer verification.
 *
 * For segmented transfers:
 *     * the sequence identifier;
 *     * offset of the current segment in the sequence;
 *     * total bytes in the transfer.
 *
 * Additionally contains:
 *     * the channel ID;
 *     * the channel information.
 */


/* Describes a shared memory area buffer.
 * Used for calculations with offsets and for buffers verification.
 */
typedef struct _HGSMIAREA
{
    uint8_t     *pu8Base; /* The starting address of the area. Corresponds to offset 'offBase'. */
    HGSMIOFFSET  offBase; /* The starting offset of the area. */
    HGSMIOFFSET  offLast; /* The last valid offset:
                           * offBase + cbArea - 1 - (sizeof (header) + sizeof (tail)).
                           */
    HGSMISIZE    cbArea;  /* Size of the area. */
} HGSMIAREA;


/* The buffer description flags. */
#define HGSMI_BUFFER_HEADER_F_SEQ_MASK     0x03 /* Buffer sequence type mask. */
#define HGSMI_BUFFER_HEADER_F_SEQ_SINGLE   0x00 /* Single buffer, not a part of a sequence. */
#define HGSMI_BUFFER_HEADER_F_SEQ_START    0x01 /* The first buffer in a sequence. */
#define HGSMI_BUFFER_HEADER_F_SEQ_CONTINUE 0x02 /* A middle buffer in a sequence. */
#define HGSMI_BUFFER_HEADER_F_SEQ_END      0x03 /* The last buffer in a sequence. */


#pragma pack(1)
/* 16 bytes buffer header. */
typedef struct _HGSMIBUFFERHEADER
{
    uint32_t    u32DataSize;            /* Size of data that follows the header. */

    uint8_t     u8Flags;                /* The buffer description: HGSMI_BUFFER_HEADER_F_* */

    uint8_t     u8Channel;              /* The channel the data must be routed to. */
    uint16_t    u16ChannelInfo;         /* Opaque to the HGSMI, used by the channel. */

    union {
        uint8_t au8Union[8];            /* Opaque placeholder to make the union 8 bytes. */

        struct
        {                               /* HGSMI_BUFFER_HEADER_F_SEQ_SINGLE */
            uint32_t u32Reserved1;      /* A reserved field, initialize to 0. */
            uint32_t u32Reserved2;      /* A reserved field, initialize to 0. */
        } Buffer;

        struct
        {                               /* HGSMI_BUFFER_HEADER_F_SEQ_START */
            uint32_t u32SequenceNumber; /* The sequence number, the same for all buffers in the sequence. */
            uint32_t u32SequenceSize;   /* The total size of the sequence. */
        } SequenceStart;

        struct
        {                               /* HGSMI_BUFFER_HEADER_F_SEQ_CONTINUE and HGSMI_BUFFER_HEADER_F_SEQ_END */
            uint32_t u32SequenceNumber; /* The sequence number, the same for all buffers in the sequence. */
            uint32_t u32SequenceOffset; /* Data offset in the entire sequence. */
        } SequenceContinue;
    } u;

} HGSMIBUFFERHEADER;

/* 8 bytes buffer tail. */
typedef struct _HGSMIBUFFERTAIL
{
    uint32_t    u32Reserved;        /* Reserved, must be initialized to 0. */
    uint32_t    u32Checksum;        /* Verifyer for the buffer header and offset and for first 4 bytes of the tail. */
} HGSMIBUFFERTAIL;
#pragma pack()

AssertCompile(sizeof (HGSMIBUFFERHEADER) == 16);
AssertCompile(sizeof (HGSMIBUFFERTAIL) == 8);


#pragma pack(1)
typedef struct _HGSMIHEAP
{
    RTHEAPSIMPLE  heap;            /* Heap instance. */
    HGSMIAREA     area;            /* Description. */
    int           cRefs;           /* Number of heap allocations. */
} HGSMIHEAP;
#pragma pack()

#pragma pack(1)
/* The size of the array of channels. Array indexes are uint8_t. Note: the value must not be changed. */
#define HGSMI_NUMBER_OF_CHANNELS 0x100

/* Channel handler called when the guest submits a buffer. */
typedef DECLCALLBACK(int) FNHGSMICHANNELHANDLER(void *pvHandler, uint16_t u16ChannelInfo, void *pvBuffer, HGSMISIZE cbBuffer);
typedef FNHGSMICHANNELHANDLER *PFNHGSMICHANNELHANDLER;

/* Information about a handler: pfn + context. */
typedef struct _HGSMICHANNELHANDLER
{
    PFNHGSMICHANNELHANDLER pfnHandler;
    void *pvHandler;
} HGSMICHANNELHANDLER;

/* Channel description. */
typedef struct _HGSMICHANNEL
{
    HGSMICHANNELHANDLER handler;       /* The channel handler. */
    const char *pszName;               /* NULL for hardcoded channels or RTStrDup'ed name. */
    uint8_t u8Channel;                 /* The channel id, equal to the channel index in the array. */
    uint8_t u8Flags;                   /* HGSMI_CH_F_* */
} HGSMICHANNEL;

typedef struct _HGSMICHANNELINFO
{
    HGSMICHANNEL Channels[HGSMI_NUMBER_OF_CHANNELS]; /* Channel handlers indexed by the channel id.
                                                      * The array is accessed under the instance lock.
                                                      */
}  HGSMICHANNELINFO;
#pragma pack()


RT_C_DECLS_BEGIN

DECLINLINE(HGSMISIZE) HGSMIBufferMinimumSize (void)
{
    return sizeof (HGSMIBUFFERHEADER) + sizeof (HGSMIBUFFERTAIL);
}

DECLINLINE(uint8_t *) HGSMIBufferData (const HGSMIBUFFERHEADER *pHeader)
{
    return (uint8_t *)pHeader + sizeof (HGSMIBUFFERHEADER);
}

DECLINLINE(HGSMIBUFFERTAIL *) HGSMIBufferTail (const HGSMIBUFFERHEADER *pHeader)
{
    return (HGSMIBUFFERTAIL *)(HGSMIBufferData (pHeader) + pHeader->u32DataSize);
}

DECLINLINE(HGSMIBUFFERHEADER *) HGSMIBufferHeaderFromData (const void *pvData)
{
    return (HGSMIBUFFERHEADER *)((uint8_t *)pvData - sizeof (HGSMIBUFFERHEADER));
}

DECLINLINE(HGSMISIZE) HGSMIBufferRequiredSize (uint32_t u32DataSize)
{
    return HGSMIBufferMinimumSize () + u32DataSize;
}

DECLINLINE(HGSMIOFFSET) HGSMIPointerToOffset (const HGSMIAREA *pArea,
                                              const HGSMIBUFFERHEADER *pHeader)
{
    return pArea->offBase + (HGSMIOFFSET)((uint8_t *)pHeader - pArea->pu8Base);
}

DECLINLINE(HGSMIBUFFERHEADER *) HGSMIOffsetToPointer (const HGSMIAREA *pArea,
                                                      HGSMIOFFSET offBuffer)
{
    return (HGSMIBUFFERHEADER *)(pArea->pu8Base + (offBuffer - pArea->offBase));
}

DECLINLINE(uint8_t *) HGSMIBufferDataFromOffset (const HGSMIAREA *pArea, HGSMIOFFSET offBuffer)
{
    HGSMIBUFFERHEADER *pHeader = HGSMIOffsetToPointer (pArea, offBuffer);
    Assert(pHeader);
    if(pHeader)
        return HGSMIBufferData(pHeader);
    return NULL;
}

HGSMICHANNEL *HGSMIChannelFindById (HGSMICHANNELINFO * pChannelInfo, uint8_t u8Channel);

uint32_t HGSMIChecksum (HGSMIOFFSET offBuffer,
                        const HGSMIBUFFERHEADER *pHeader,
                        const HGSMIBUFFERTAIL *pTail);

int HGSMIAreaInitialize (HGSMIAREA *pArea,
                         void *pvBase,
                         HGSMISIZE cbArea,
                         HGSMIOFFSET offBase);

void HGSMIAreaClear (HGSMIAREA *pArea);

HGSMIOFFSET HGSMIBufferInitializeSingle (const HGSMIAREA *pArea,
                                         HGSMIBUFFERHEADER *pHeader,
                                         HGSMISIZE cbBuffer,
                                         uint8_t u8Channel,
                                         uint16_t u16ChannelInfo);

int HGSMIHeapSetup (HGSMIHEAP *pHeap,
                    void *pvBase,
                    HGSMISIZE cbArea,
                    HGSMIOFFSET offBase);

int HGSMIHeapRelocate (HGSMIHEAP *pHeap,
                    void *pvBase,
                    uint32_t offHeapHandle,
                    uintptr_t offDelta,
                    HGSMISIZE cbArea,
                    HGSMIOFFSET offBase
                    );

void HGSMIHeapSetupUnitialized (HGSMIHEAP *pHeap);
bool HGSMIHeapIsItialized (HGSMIHEAP *pHeap);

void HGSMIHeapDestroy (HGSMIHEAP *pHeap);

void *HGSMIHeapAlloc (HGSMIHEAP *pHeap,
                      HGSMISIZE cbData,
                      uint8_t u8Channel,
                      uint16_t u16ChannelInfo);

HGSMIOFFSET HGSMIHeapBufferOffset (HGSMIHEAP *pHeap,
                                   void *pvData);

void HGSMIHeapFree (HGSMIHEAP *pHeap,
                    void *pvData);

DECLINLINE(HGSMIOFFSET) HGSMIHeapOffset(HGSMIHEAP *pHeap)
{
    return pHeap->area.offBase;
}

/* needed for heap relocation */
DECLINLINE(HGSMIOFFSET) HGSMIHeapHandleLocationOffset(HGSMIHEAP *pHeap)
{
    return pHeap->heap != NIL_RTHEAPSIMPLE ? (uint32_t)(pHeap->area.pu8Base - (uint8_t*)pHeap->heap) : HGSMIOFFSET_VOID;
}

DECLINLINE(HGSMISIZE) HGSMIHeapSize(HGSMIHEAP *pHeap)
{
    return pHeap->area.cbArea;
}

int HGSMIChannelRegister (HGSMICHANNELINFO * pChannelInfo,
                                 uint8_t u8Channel,
                                 const char *pszName,
                                 PFNHGSMICHANNELHANDLER pfnChannelHandler,
                                 void *pvChannelHandler,
                                 HGSMICHANNELHANDLER *pOldHandler);

int HGSMIBufferProcess (HGSMIAREA *pArea,
                         HGSMICHANNELINFO * pChannelInfo,
                         HGSMIOFFSET offBuffer);
RT_C_DECLS_END

#endif /* __HGSMI_h__*/
