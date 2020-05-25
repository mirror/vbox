/* $Id$ */
/** @file
 * DBGF - Internal header file.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_DBGFInternal_h
#define VMM_INCLUDED_SRC_include_DBGFInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/cdefs.h>
#ifdef IN_RING3
# include <VBox/dis.h>
#endif
#include <VBox/types.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>
#include <iprt/string.h>
#include <iprt/avl.h>
#include <iprt/dbg.h>
#include <iprt/tracelog.h>
#include <VBox/vmm/dbgf.h>



/** @defgroup grp_dbgf_int   Internals
 * @ingroup grp_dbgf
 * @internal
 * @{
 */

/** The maximum tracer instance (total) size, ring-0/raw-mode capable tracers. */
#define DBGF_MAX_TRACER_INSTANCE_SIZE    _512M
/** The maximum tracers instance (total) size, ring-3 only tracers. */
#define DBGF_MAX_TRACER_INSTANCE_SIZE_R3 _1G
/** Event ringbuffer header size. */
#define DBGF_TRACER_EVT_HDR_SZ           (32)
/** Event ringbuffer payload size. */
#define DBGF_TRACER_EVT_PAYLOAD_SZ       (32)
/** Event ringbuffer entry size. */
#define DBGF_TRACER_EVT_SZ               (DBGF_TRACER_EVT_HDR_SZ + DBGF_TRACER_EVT_PAYLOAD_SZ)



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Event entry types.
 */
typedef enum DBGFTRACEREVT
{
    /** Invalid type. */
    DBGFTRACEREVT_INVALID = 0,
    /** Register event source event. */
    DBGFTRACEREVT_SRC_REGISTER,
    /** Deregister event source event. */
    DBGFTRACEREVT_SRC_DEREGISTER,
    /** MMIO map region event. */
    DBGFTRACEREVT_MMIO_MAP,
    /** MMIO unmap region event. */
    DBGFTRACEREVT_MMIO_UNMAP,
    /** MMIO read event. */
    DBGFTRACEREVT_MMIO_READ,
    /** MMIO write event. */
    DBGFTRACEREVT_MMIO_WRITE,
    /** MMIO fill event. */
    DBGFTRACEREVT_MMIO_FILL,
    /** I/O port map event. */
    DBGFTRACEREVT_IOPORT_MAP,
    /** I/O port unmap event. */
    DBGFTRACEREVT_IOPORT_UNMAP,
    /** I/O port read event. */
    DBGFTRACEREVT_IOPORT_READ,
    /** I/O port write event. */
    DBGFTRACEREVT_IOPORT_WRITE,
    /** IRQ event. */
    DBGFTRACEREVT_IRQ,
    /** I/O APIC MSI event. */
    DBGFTRACEREVT_IOAPIC_MSI,
    /** Read from guest physical memory. */
    DBGFTRACEREVT_GCPHYS_READ,
    /** Write to guest physical memory. */
    DBGFTRACEREVT_GCPHYS_WRITE,
    /** 32bit hack. */
    DBGFTRACEREVT_32BIT_HACK
} DBGFTRACEREVT;
/** Pointer to a trace event entry type. */
typedef DBGFTRACEREVT *PDBGFTRACEREVT;


/**
 * MMIO region map event.
 */
typedef struct DBGFTRACEREVTMMIOMAP
{
    /** Unique region handle for the event source. */
    uint64_t                                hMmioRegion;
    /** The base guest physical address of the MMIO region. */
    RTGCPHYS                                GCPhysMmioBase;
    /** Padding to 32byte. */
    uint64_t                                au64Pad0[2];
} DBGFTRACEREVTMMIOMAP;
/** Pointer to a MMIO map event. */
typedef DBGFTRACEREVTMMIOMAP *PDBGFTRACEREVTMMIOMAP;
/** Pointer to a const MMIO map event. */
typedef const DBGFTRACEREVTMMIOMAP *PCDBGFTRACEREVTMMIOMAP;

AssertCompileSize(DBGFTRACEREVTMMIOMAP, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * MMIO region unmap event.
 */
typedef struct DBGFTRACEREVTMMIOUNMAP
{
    /** Unique region handle for the event source. */
    uint64_t                                hMmioRegion;
    /** Padding to 32byte. */
    uint64_t                                au64Pad0[3];
} DBGFTRACEREVTMMIOUNMAP;
/** Pointer to a MMIO map event. */
typedef DBGFTRACEREVTMMIOUNMAP *PDBGFTRACEREVTMMIOUNMAP;
/** Pointer to a const MMIO map event. */
typedef const DBGFTRACEREVTMMIOUNMAP *PCDBGFTRACEREVTMMIOUNMAP;

AssertCompileSize(DBGFTRACEREVTMMIOUNMAP, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * MMIO event.
 */
typedef struct DBGFTRACEREVTMMIO
{
    /** Unique region handle for the event source. */
    uint64_t                                hMmioRegion;
    /** Offset into the region the access happened. */
    RTGCPHYS                                offMmio;
    /** Number of bytes transfered (the direction is in the event header). */
    uint64_t                                cbXfer;
    /** The value transfered. */
    uint64_t                                u64Val;
} DBGFTRACEREVTMMIO;
/** Pointer to a MMIO event. */
typedef DBGFTRACEREVTMMIO *PDBGFTRACEREVTMMIO;
/** Pointer to a const MMIO event. */
typedef const DBGFTRACEREVTMMIO *PCDBGFTRACEREVTMMIO;

AssertCompileSize(DBGFTRACEREVTMMIO, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * MMIO fill event.
 */
typedef struct DBGFTRACEREVTMMIOFILL
{
    /** Unique region handle for the event source. */
    uint64_t                                hMmioRegion;
    /** Offset into the region the access happened. */
    RTGCPHYS                                offMmio;
    /** Item size in bytes. */
    uint32_t                                cbItem;
    /** Amount of items being filled. */
    uint32_t                                cItems;
    /** The fill value. */
    uint32_t                                u32Item;
    /** Padding to 32bytes. */
    uint32_t                                u32Pad0;
} DBGFTRACEREVTMMIOFILL;
/** Pointer to a MMIO event. */
typedef DBGFTRACEREVTMMIOFILL *PDBGFTRACEREVTMMIOFILL;
/** Pointer to a const MMIO event. */
typedef const DBGFTRACEREVTMMIOFILL *PCDBGFTRACEREVTMMIOFILL;

AssertCompileSize(DBGFTRACEREVTMMIOFILL, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * I/O port region map event.
 */
typedef struct DBGFTRACEREVTIOPORTMAP
{
    /** Unique I/O port region handle for the event source. */
    uint64_t                                hIoPorts;
    /** The base I/O port for the region. */
    RTIOPORT                                IoPortBase;
    /** Padding to 32byte. */
    uint16_t                                au16Pad0[11];
} DBGFTRACEREVTIOPORTMAP;
/** Pointer to a MMIO map event. */
typedef DBGFTRACEREVTIOPORTMAP *PDBGFTRACEREVTIOPORTMAP;
/** Pointer to a const MMIO map event. */
typedef const DBGFTRACEREVTIOPORTMAP *PCDBGFTRACEREVTIOPORTMAP;

AssertCompileSize(DBGFTRACEREVTIOPORTMAP, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * MMIO region unmap event.
 */
typedef struct DBGFTRACEREVTIOPORTUNMAP
{
    /** Unique region handle for the event source. */
    uint64_t                                hIoPorts;
    /** Padding to 32byte. */
    uint64_t                                au64Pad0[3];
} DBGFTRACEREVTIOPORTUNMAP;
/** Pointer to a MMIO map event. */
typedef DBGFTRACEREVTIOPORTUNMAP *PDBGFTRACEREVTIOPORTUNMAP;
/** Pointer to a const MMIO map event. */
typedef const DBGFTRACEREVTIOPORTUNMAP *PCDBGFTRACEREVTIOPORTUNMAP;

AssertCompileSize(DBGFTRACEREVTIOPORTUNMAP, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * I/O port event.
 */
typedef struct DBGFTRACEREVTIOPORT
{
    /** Unique region handle for the event source. */
    uint64_t                                hIoPorts;
    /** Offset into the I/O port region. */
    RTIOPORT                                offPort;
    /** 8 byte alignment. */
    uint8_t                                 abPad0[6];
    /** Number of bytes transfered (the direction is in the event header). */
    uint64_t                                cbXfer;
    /** The value transfered. */
    uint32_t                                u32Val;
    /** Padding to 32bytes. */
    uint8_t                                 abPad1[4];
} DBGFTRACEREVTIOPORT;
/** Pointer to a MMIO event. */
typedef DBGFTRACEREVTIOPORT *PDBGFTRACEREVTIOPORT;
/** Pointer to a const MMIO event. */
typedef const DBGFTRACEREVTIOPORT *PCDBGFTRACEREVTIOPORT;

AssertCompileSize(DBGFTRACEREVTIOPORT, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * IRQ event.
 */
typedef struct DBGFTRACEREVTIRQ
{
    /** The IRQ line. */
    int32_t                                 iIrq;
    /** IRQ level flags. */
    int32_t                                 fIrqLvl;
    /** Padding to 32bytes. */
    uint32_t                                au32Pad0[6];
} DBGFTRACEREVTIRQ;
/** Pointer to a MMIO event. */
typedef DBGFTRACEREVTIRQ *PDBGFTRACEREVTIRQ;
/** Pointer to a const MMIO event. */
typedef const DBGFTRACEREVTIRQ *PCDBGFTRACEREVTIRQ;

AssertCompileSize(DBGFTRACEREVTIRQ, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * I/O APIC MSI event.
 */
typedef struct DBGFTRACEREVTIOAPICMSI
{
    /** The guest physical address being written. */
    RTGCPHYS                                GCPhys;
    /** The value being written. */
    uint32_t                                u32Val;
    /** Padding to 32bytes. */
    uint32_t                                au32Pad0[5];
} DBGFTRACEREVTIOAPICMSI;
/** Pointer to a MMIO event. */
typedef DBGFTRACEREVTIOAPICMSI *PDBGFTRACEREVTIOAPICMSI;
/** Pointer to a const MMIO event. */
typedef const DBGFTRACEREVTIOAPICMSI *PCDBGFTRACEREVTIOAPICMSI;

AssertCompileSize(DBGFTRACEREVTIOAPICMSI, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * Guest physical memory transfer.
 */
typedef struct DBGFTRACEREVTGCPHYS
{
    /** Guest physical address of the access. */
    RTGCPHYS                                GCPhys;
    /** Number of bytes transfered (the direction is in the event header).
     * If the number is small enough to fit into the remaining space of the entry
     * it is stored here, otherwise it will be stored in the next entry (and following
     * entries). */
    uint64_t                                cbXfer;
    /** Guest data being transfered. */
    uint8_t                                 abData[16];
} DBGFTRACEREVTGCPHYS;
/** Pointer to a guest physical memory transfer event. */
typedef DBGFTRACEREVTGCPHYS *PDBGFTRACEREVTGCPHYS;
/** Pointer to a const uest physical memory transfer event. */
typedef const DBGFTRACEREVTGCPHYS *PCDBGFTRACEREVTGCPHYS;

AssertCompileSize(DBGFTRACEREVTGCPHYS, DBGF_TRACER_EVT_PAYLOAD_SZ);


/**
 * A trace event header in the shared ring buffer.
 */
typedef struct DBGFTRACEREVTHDR
{
    /** Event ID. */
    volatile uint64_t                       idEvt;
    /** The previous event ID this one links to,
     * DBGF_TRACER_EVT_HDR_ID_INVALID if it links to no other event. */
    uint64_t                                idEvtPrev;
    /** Event source. */
    DBGFTRACEREVTSRC                        hEvtSrc;
    /** The event entry type. */
    DBGFTRACEREVT                           enmEvt;
    /** Flags for this event. */
    uint32_t                                fFlags;
} DBGFTRACEREVTHDR;
/** Pointer to a trace event header. */
typedef DBGFTRACEREVTHDR *PDBGFTRACEREVTHDR;
/** Pointer to a const trace event header. */
typedef const DBGFTRACEREVTHDR *PCDBGFTRACEREVTHDR;

AssertCompileSize(DBGFTRACEREVTHDR, DBGF_TRACER_EVT_HDR_SZ);

/** Invalid event ID, this is always set by the flush thread after processing one entry
 * so the producers know when they are about to overwrite not yet processed entries in the ring buffer. */
#define DBGF_TRACER_EVT_HDR_ID_INVALID      UINT64_C(0xffffffffffffffff)

/** The event came from R0. */
#define DBGF_TRACER_EVT_HDR_F_R0            RT_BIT(0)

/** Default event header tracer flags. */
#ifdef IN_RING0
# define DBGF_TRACER_EVT_HDR_F_DEFAULT      DBGF_TRACER_EVT_HDR_F_R0
#else
# define DBGF_TRACER_EVT_HDR_F_DEFAULT      (0)
#endif


/**
 * Tracer instance data, shared structure.
 */
typedef struct DBGFTRACERSHARED
{
    /** The global event ID counter, monotonically increasing.
     * Accessed by all threads causing a trace event. */
    volatile uint64_t                       idEvt;
    /** The SUP event semaphore for poking the flush thread. */
    SUPSEMEVENT                             hSupSemEvtFlush;
    /** Ring buffer size. */
    size_t                                  cbRingBuf;
    /** Flag whether there are events in the ring buffer to get processed. */
    volatile bool                           fEvtsWaiting;
    /** Flag whether the flush thread is actively running or was kicked. */
    volatile bool                           fFlushThrdActive;
    /** Padding to a 64byte alignment. */
    uint8_t                                 abAlignment0[32];
} DBGFTRACERSHARED;
/** Pointer to the shared tarcer instance data. */
typedef DBGFTRACERSHARED *PDBGFTRACERSHARED;

AssertCompileSizeAlignment(DBGFTRACERSHARED, 64);


/**
 * Guest memory read/write data aggregation.
 */
typedef struct DBGFTRACERGCPHYSRWAGG
{
    /** The event ID which started the aggregation (used for the group ID when writing out the event). */
    uint64_t                                idEvtStart;
    /** The previous event ID used to link all the chunks together. */
    uint64_t                                idEvtPrev;
    /** Number of bytes being transfered. */
    size_t                                  cbXfer;
    /** Amount of data left to aggregate before it can be written. */
    size_t                                  cbLeft;
    /** Amount of bytes allocated. */
    size_t                                  cbBufMax;
    /** Offset into the buffer to write next. */
    size_t                                  offBuf;
    /** Pointer to the allocated buffer. */
    uint8_t                                 *pbBuf;
} DBGFTRACERGCPHYSRWAGG;
/** Pointer to a guest memory read/write data aggregation structure. */
typedef DBGFTRACERGCPHYSRWAGG *PDBGFTRACERGCPHYSRWAGG;


/**
 * Tracer instance data, ring-3
 */
typedef struct DBGFTRACERINSR3
{
    /** Pointer to the next instance.
     * (Head is pointed to by PDM::pTracerInstances.) */
    R3PTRTYPE(struct DBGFTRACERINSR3 *)     pNextR3;
    /** R3 pointer to the VM this instance was created for. */
    PVMR3                                   pVMR3;
    /** Tracer instance number. */
    uint32_t                                idTracer;
    /** Flag whether the tracer has the R0 part enabled. */
    bool                                    fR0Enabled;
    /** Flag whether the tracer flush thread should shut down. */
    volatile bool                           fShutdown;
    /** Padding. */
    bool                                    afPad0[6];
    /** Next event source ID to return for a source registration. */
    volatile DBGFTRACEREVTSRC               hEvtSrcNext;
    /** Pointer to the shared tracer instance data. */
    R3PTRTYPE(PDBGFTRACERSHARED)            pSharedR3;
    /** The I/O thread writing the log from the shared event ringbuffer. */
    RTTHREAD                                hThrdFlush;
    /** Pointer to the start of the ring buffer. */
    R3PTRTYPE(uint8_t *)                    pbRingBufR3;
    /** The last processed event ID. */
    uint64_t                                idEvtLast;
    /** The trace log writer handle. */
    RTTRACELOGWR                            hTraceLog;
    /** Guest memory data aggregation structures to track
     * currently pending guest memory reads/writes. */
    DBGFTRACERGCPHYSRWAGG                   aGstMemRwData[10];
} DBGFTRACERINSR3;
/** Pointer to a tarcer instance - Ring-3 Ptr. */
typedef R3PTRTYPE(DBGFTRACERINSR3 *) PDBGFTRACERINSR3;


/**
 * Private tracer instance data, ring-0
 */
typedef struct DBGFTRACERINSR0
{
    /** Pointer to the VM this instance was created for. */
    R0PTRTYPE(PGVM)                         pGVM;
    /** The tracer instance memory. */
    RTR0MEMOBJ                              hMemObj;
    /** The ring-3 mapping object. */
    RTR0MEMOBJ                              hMapObj;
    /** Pointer to the shared tracer instance data. */
    R0PTRTYPE(PDBGFTRACERSHARED)            pSharedR0;
    /** Size of the ring buffer in bytes, kept here so R3 can not manipulate the ring buffer
     * size afterwards to trick R0 into doing something harmful. */
    size_t                                  cbRingBuf;
    /** Pointer to the start of the ring buffer. */
    R0PTRTYPE(uint8_t *)                    pbRingBufR0;
} DBGFTRACERINSR0;
/** Pointer to a VM - Ring-0 Ptr. */
typedef R0PTRTYPE(DBGFTRACERINSR0 *) PDBGFTRACERINSR0;


/**
 * Private device instance data, raw-mode
 */
typedef struct DBGFTRACERINSRC
{
    /** Pointer to the VM this instance was created for. */
    RGPTRTYPE(PVM)                          pVMRC;
} DBGFTRACERINSRC;


#ifdef IN_RING3
DECLHIDDEN(int) dbgfTracerR3EvtPostSingle(PVMCC pVM, PDBGFTRACERINSCC pThisCC, DBGFTRACEREVTSRC hEvtSrc,
                                          DBGFTRACEREVT enmTraceEvt, const void *pvEvtDesc, size_t cbEvtDesc,
                                          uint64_t *pidEvt);
#endif

/** VMM Debugger Command. */
typedef enum DBGFCMD
{
    /** No command.
     * This is assigned to the field by the emulation thread after
     * a command has been completed. */
    DBGFCMD_NO_COMMAND = 0,
    /** Halt the VM. */
    DBGFCMD_HALT,
    /** Resume execution. */
    DBGFCMD_GO,
    /** Single step execution - stepping into calls. */
    DBGFCMD_SINGLE_STEP,
    /** Detaches the debugger.
     * Disabling all breakpoints, watch points and the like. */
    DBGFCMD_DETACH_DEBUGGER,
    /** Detached the debugger.
     * The isn't a command as such, it's just that it's necessary for the
     * detaching protocol to be racefree. */
    DBGFCMD_DETACHED_DEBUGGER
} DBGFCMD;

/**
 * VMM Debugger Command.
 */
typedef union DBGFCMDDATA
{
    uint32_t    uDummy;
} DBGFCMDDATA;
/** Pointer to DBGF Command Data. */
typedef DBGFCMDDATA *PDBGFCMDDATA;

/**
 * Info type.
 */
typedef enum DBGFINFOTYPE
{
    /** Invalid. */
    DBGFINFOTYPE_INVALID = 0,
    /** Device owner. */
    DBGFINFOTYPE_DEV,
    /** Driver owner. */
    DBGFINFOTYPE_DRV,
    /** Internal owner. */
    DBGFINFOTYPE_INT,
    /** External owner. */
    DBGFINFOTYPE_EXT,
    /** Device owner. */
    DBGFINFOTYPE_DEV_ARGV,
    /** Driver owner. */
    DBGFINFOTYPE_DRV_ARGV,
    /** USB device  owner. */
    DBGFINFOTYPE_USB_ARGV,
    /** Internal owner, argv. */
    DBGFINFOTYPE_INT_ARGV,
    /** External owner. */
    DBGFINFOTYPE_EXT_ARGV
} DBGFINFOTYPE;


/** Pointer to info structure. */
typedef struct DBGFINFO *PDBGFINFO;

#ifdef IN_RING3
/**
 * Info structure.
 */
typedef struct DBGFINFO
{
    /** The flags. */
    uint32_t        fFlags;
    /** Owner type. */
    DBGFINFOTYPE    enmType;
    /** Per type data. */
    union
    {
        /** DBGFINFOTYPE_DEV */
        struct
        {
            /** Device info handler function. */
            PFNDBGFHANDLERDEV   pfnHandler;
            /** The device instance. */
            PPDMDEVINS          pDevIns;
        } Dev;

        /** DBGFINFOTYPE_DRV */
        struct
        {
            /** Driver info handler function. */
            PFNDBGFHANDLERDRV   pfnHandler;
            /** The driver instance. */
            PPDMDRVINS          pDrvIns;
        } Drv;

        /** DBGFINFOTYPE_INT */
        struct
        {
            /** Internal info handler function. */
            PFNDBGFHANDLERINT   pfnHandler;
        } Int;

        /** DBGFINFOTYPE_EXT */
        struct
        {
            /** External info handler function. */
            PFNDBGFHANDLEREXT   pfnHandler;
            /** The user argument. */
            void               *pvUser;
        } Ext;

        /** DBGFINFOTYPE_DEV_ARGV */
        struct
        {
            /** Device info handler function. */
            PFNDBGFINFOARGVDEV  pfnHandler;
            /** The device instance. */
            PPDMDEVINS          pDevIns;
        } DevArgv;

        /** DBGFINFOTYPE_DRV_ARGV */
        struct
        {
            /** Driver info handler function. */
            PFNDBGFINFOARGVDRV  pfnHandler;
            /** The driver instance. */
            PPDMDRVINS          pDrvIns;
        } DrvArgv;

        /** DBGFINFOTYPE_USB_ARGV */
        struct
        {
            /** Driver info handler function. */
            PFNDBGFINFOARGVUSB  pfnHandler;
            /** The driver instance. */
            PPDMUSBINS          pUsbIns;
        } UsbArgv;

        /** DBGFINFOTYPE_INT_ARGV */
        struct
        {
            /** Internal info handler function. */
            PFNDBGFINFOARGVINT  pfnHandler;
        } IntArgv;

        /** DBGFINFOTYPE_EXT_ARGV */
        struct
        {
            /** External info handler function. */
            PFNDBGFINFOARGVEXT  pfnHandler;
            /** The user argument. */
            void               *pvUser;
        } ExtArgv;
    } u;

    /** Pointer to the description. */
    const char     *pszDesc;
    /** Pointer to the next info structure. */
    PDBGFINFO       pNext;
    /** The identifier name length. */
    size_t          cchName;
    /** The identifier name. (Extends 'beyond' the struct as usual.) */
    char            szName[1];
} DBGFINFO;
#endif /* IN_RING3 */


#ifdef IN_RING3
/**
 * Guest OS digger instance.
 */
typedef struct DBGFOS
{
    /** Pointer to the registration record. */
    PCDBGFOSREG                 pReg;
    /** Pointer to the next OS we've registered. */
    struct DBGFOS              *pNext;
    /** List of EMT interface wrappers. */
    struct DBGFOSEMTWRAPPER    *pWrapperHead;
    /** The instance data (variable size). */
    uint8_t                     abData[16];
} DBGFOS;
#endif
/** Pointer to guest OS digger instance. */
typedef struct DBGFOS *PDBGFOS;
/** Pointer to const guest OS digger instance. */
typedef struct DBGFOS const *PCDBGFOS;


/**
 * Breakpoint search optimization.
 */
typedef struct DBGFBPSEARCHOPT
{
    /** Where to start searching for hits.
     * (First enabled is #DBGF::aBreakpoints[iStartSearch]). */
    uint32_t volatile       iStartSearch;
    /** The number of aBreakpoints entries to search.
     * (Last enabled is #DBGF::aBreakpoints[iStartSearch + cToSearch - 1])  */
    uint32_t volatile       cToSearch;
} DBGFBPSEARCHOPT;
/** Pointer to a breakpoint search optimziation structure. */
typedef DBGFBPSEARCHOPT *PDBGFBPSEARCHOPT;



/**
 * DBGF Data (part of VM)
 */
typedef struct DBGF
{
    /** Bitmap of enabled hardware interrupt breakpoints. */
    uint32_t                    bmHardIntBreakpoints[256 / 32];
    /** Bitmap of enabled software interrupt breakpoints. */
    uint32_t                    bmSoftIntBreakpoints[256 / 32];
    /** Bitmap of selected events.
     * This includes non-selectable events too for simplicity, we maintain the
     * state for some of these, as it may come in handy. */
    uint64_t                    bmSelectedEvents[(DBGFEVENT_END + 63) / 64];

    /** Enabled hardware interrupt breakpoints. */
    uint32_t                    cHardIntBreakpoints;
    /** Enabled software interrupt breakpoints. */
    uint32_t                    cSoftIntBreakpoints;

    /** The number of selected events. */
    uint32_t                    cSelectedEvents;

    /** The number of enabled hardware breakpoints. */
    uint8_t                     cEnabledHwBreakpoints;
    /** The number of enabled hardware I/O breakpoints. */
    uint8_t                     cEnabledHwIoBreakpoints;
    /** The number of enabled INT3 breakpoints. */
    uint8_t                     cEnabledInt3Breakpoints;
    uint8_t                     abPadding; /**< Unused padding space up for grabs. */
    uint32_t                    uPadding;

    /** Debugger Attached flag.
     * Set if a debugger is attached, elsewise it's clear.
     */
    bool volatile               fAttached;

    /** Stopped in the Hypervisor.
     * Set if we're stopped on a trace, breakpoint or assertion inside
     * the hypervisor and have to restrict the available operations.
     */
    bool volatile               fStoppedInHyper;

    /**
     * Ping-Pong construct where the Ping side is the VMM and the Pong side
     * the Debugger.
     */
    RTPINGPONG                  PingPong;
    RTHCUINTPTR                 uPtrPadding; /**< Alignment padding. */

    /** The Event to the debugger.
     * The VMM will ping the debugger when the event is ready. The event is
     * either a response to a command or to a break/watch point issued
     * previously.
     */
    DBGFEVENT                   DbgEvent;

    /** The Command to the VMM.
     * Operated in an atomic fashion since the VMM will poll on this.
     * This means that a the command data must be written before this member
     * is set. The VMM will reset this member to the no-command state
     * when it have processed it.
     */
    DBGFCMD volatile            enmVMMCmd;
    /** The Command data.
     * Not all commands take data. */
    DBGFCMDDATA                 VMMCmdData;

    /** Stepping filtering. */
    struct
    {
        /** The CPU doing the stepping.
         * Set to NIL_VMCPUID when filtering is inactive */
        VMCPUID                 idCpu;
        /** The specified flags. */
        uint32_t                fFlags;
        /** The effective PC address to stop at, if given. */
        RTGCPTR                 AddrPc;
        /** The lowest effective stack address to stop at.
         * Together with cbStackPop, this forms a range of effective stack pointer
         * addresses that we stop for.   */
        RTGCPTR                 AddrStackPop;
        /** The size of the stack stop area starting at AddrStackPop. */
        RTGCPTR                 cbStackPop;
        /** Maximum number of steps. */
        uint32_t                cMaxSteps;

        /** Number of steps made thus far. */
        uint32_t                cSteps;
        /** Current call counting balance for step-over handling. */
        uint32_t                uCallDepth;

        uint32_t                u32Padding; /**< Alignment padding. */

    } SteppingFilter;

    uint32_t                    u32Padding[2]; /**< Alignment padding. */

    /** Array of hardware breakpoints. (0..3)
     * This is shared among all the CPUs because life is much simpler that way. */
    DBGFBP                      aHwBreakpoints[4];
    /** Array of int 3 and REM breakpoints. (4..)
     * @remark This is currently a fixed size array for reasons of simplicity. */
    DBGFBP                      aBreakpoints[32];

    /** MMIO breakpoint search optimizations. */
    DBGFBPSEARCHOPT             Mmio;
    /** I/O port breakpoint search optimizations. */
    DBGFBPSEARCHOPT             PortIo;
    /** INT3 breakpoint search optimizations. */
    DBGFBPSEARCHOPT             Int3;

    /**
     * Bug check data.
     * @note This will not be reset on reset.
     */
    struct
    {
        /** The ID of the CPU reporting it. */
        VMCPUID                 idCpu;
        /** The event associated with the bug check (gives source).
         * This is set to DBGFEVENT_END if no BSOD data here. */
        DBGFEVENTTYPE           enmEvent;
        /** The total reset count at the time (VMGetResetCount). */
        uint32_t                uResetNo;
        /** Explicit padding. */
        uint32_t                uPadding;
        /** When it was reported (TMVirtualGet). */
        uint64_t                uTimestamp;
        /** The bug check number.
         * @note This is really just 32-bit wide, see KeBugCheckEx.  */
        uint64_t                uBugCheck;
        /** The bug check parameters. */
        uint64_t                auParameters[4];
    } BugCheck;
} DBGF;
AssertCompileMemberAlignment(DBGF, DbgEvent, 8);
AssertCompileMemberAlignment(DBGF, aHwBreakpoints, 8);
AssertCompileMemberAlignment(DBGF, bmHardIntBreakpoints, 8);
/** Pointer to DBGF Data. */
typedef DBGF *PDBGF;


/**
 * Event state (for DBGFCPU::aEvents).
 */
typedef enum DBGFEVENTSTATE
{
    /** Invalid event stack entry. */
    DBGFEVENTSTATE_INVALID = 0,
    /** The current event stack entry. */
    DBGFEVENTSTATE_CURRENT,
    /** Event that should be ignored but hasn't yet actually been ignored. */
    DBGFEVENTSTATE_IGNORE,
    /** Event that has been ignored but may be restored to IGNORE should another
     * debug event fire before the instruction is completed. */
    DBGFEVENTSTATE_RESTORABLE,
    /** End of valid events.   */
    DBGFEVENTSTATE_END,
    /** Make sure we've got a 32-bit type. */
    DBGFEVENTSTATE_32BIT_HACK = 0x7fffffff
} DBGFEVENTSTATE;


/** Converts a DBGFCPU pointer into a VM pointer. */
#define DBGFCPU_2_VM(pDbgfCpu) ((PVM)((uint8_t *)(pDbgfCpu) + (pDbgfCpu)->offVM))

/**
 * The per CPU data for DBGF.
 */
typedef struct DBGFCPU
{
    /** The offset into the VM structure.
     * @see DBGFCPU_2_VM(). */
    uint32_t                offVM;

    /** Current active breakpoint (id).
     * This is ~0U if not active. It is set when a execution engine
     * encounters a breakpoint and returns VINF_EM_DBG_BREAKPOINT. This is
     * currently not used for REM breakpoints because of the lazy coupling
     * between VBox and REM.
     *
     * @todo drop this in favor of aEvents!  */
    uint32_t                iActiveBp;
    /** Set if we're singlestepping in raw mode.
     * This is checked and cleared in the \#DB handler. */
    bool                    fSingleSteppingRaw;

    /** Alignment padding. */
    bool                    afPadding[3];

    /** The number of events on the stack (aEvents).
     * The pending event is the last one (aEvents[cEvents - 1]), but only when
     * enmState is DBGFEVENTSTATE_CURRENT. */
    uint32_t                cEvents;
    /** Events - current, ignoring and ignored.
     *
     * We maintain a stack of events in order to try avoid ending up in an infinit
     * loop when resuming after an event fired.  There are cases where we may end
     * generating additional events before the instruction can be executed
     * successfully.  Like for instance an XCHG on MMIO with separate read and write
     * breakpoints, or a MOVSB instruction working on breakpointed MMIO as both
     * source and destination.
     *
     * So, when resuming after dropping into the debugger for an event, we convert
     * the DBGFEVENTSTATE_CURRENT event into a DBGFEVENTSTATE_IGNORE event, leaving
     * cEvents unchanged.  If the event is reported again, we will ignore it and
     * tell the reporter to continue executing.  The event change to the
     * DBGFEVENTSTATE_RESTORABLE state.
     *
     * Currently, the event reporter has to figure out that it is a nested event and
     * tell DBGF to restore DBGFEVENTSTATE_RESTORABLE events (and keep
     * DBGFEVENTSTATE_IGNORE, should they happen out of order for some weird
     * reason).
     */
    struct
    {
        /** The event details. */
        DBGFEVENT           Event;
        /** The RIP at which this happend (for validating ignoring). */
        uint64_t            rip;
        /** The event state. */
        DBGFEVENTSTATE      enmState;
        /** Alignment padding. */
        uint32_t            u32Alignment;
    } aEvents[3];
} DBGFCPU;
AssertCompileMemberAlignment(DBGFCPU, aEvents, 8);
AssertCompileMemberSizeAlignment(DBGFCPU, aEvents[0], 8);
/** Pointer to DBGFCPU data. */
typedef DBGFCPU *PDBGFCPU;

struct DBGFOSEMTWRAPPER;

/**
 * DBGF data kept in the ring-0 GVM.
 */
typedef struct DBGFR0PERVM
{
    /** Pointer to the tracer instance if enabled. */
    R0PTRTYPE(struct DBGFTRACERINSR0 *) pTracerR0;
} DBGFR0PERVM;

/**
 * The DBGF data kept in the UVM.
 */
typedef struct DBGFUSERPERVM
{
    /** The address space database lock. */
    RTSEMRW                     hAsDbLock;
    /** The address space handle database.      (Protected by hAsDbLock.) */
    R3PTRTYPE(AVLPVTREE)        AsHandleTree;
    /** The address space process id database.  (Protected by hAsDbLock.) */
    R3PTRTYPE(AVLU32TREE)       AsPidTree;
    /** The address space name database.        (Protected by hAsDbLock.) */
    R3PTRTYPE(RTSTRSPACE)       AsNameSpace;
    /** Special address space aliases.          (Protected by hAsDbLock.) */
    RTDBGAS volatile            ahAsAliases[DBGF_AS_COUNT];
    /** For lazily populating the aliased address spaces. */
    bool volatile               afAsAliasPopuplated[DBGF_AS_COUNT];
    /** Alignment padding. */
    bool                        afAlignment1[2];
    /** Debug configuration. */
    R3PTRTYPE(RTDBGCFG)         hDbgCfg;

    /** The register database lock. */
    RTSEMRW                     hRegDbLock;
    /** String space for looking up registers.  (Protected by hRegDbLock.) */
    R3PTRTYPE(RTSTRSPACE)       RegSpace;
    /** String space holding the register sets. (Protected by hRegDbLock.)  */
    R3PTRTYPE(RTSTRSPACE)       RegSetSpace;
    /** The number of registers (aliases, sub-fields and the special CPU
     * register aliases (eg AH) are not counted). */
    uint32_t                    cRegs;
    /** For early initialization by . */
    bool volatile               fRegDbInitialized;
    /** Alignment padding. */
    bool                        afAlignment2[3];

    /** Critical section protecting the Guest OS Digger data, the info handlers
     * and the plugins.  These share to give the best possible plugin unload
     * race protection. */
    RTCRITSECTRW                CritSect;
    /** Head of the LIFO of loaded DBGF plugins. */
    R3PTRTYPE(struct DBGFPLUGIN *) pPlugInHead;
    /** The current Guest OS digger. */
    R3PTRTYPE(PDBGFOS)          pCurOS;
    /** The head of the Guest OS digger instances. */
    R3PTRTYPE(PDBGFOS)          pOSHead;
    /** List of registered info handlers. */
    R3PTRTYPE(PDBGFINFO)        pInfoFirst;

    /** The configured tracer. */
    PDBGFTRACERINSR3            pTracerR3;

    /** The type database lock. */
    RTSEMRW                     hTypeDbLock;
    /** String space for looking up types.  (Protected by hTypeDbLock.) */
    R3PTRTYPE(RTSTRSPACE)       TypeSpace;
    /** For early initialization by . */
    bool volatile               fTypeDbInitialized;
    /** Alignment padding. */
    bool                        afAlignment3[3];

} DBGFUSERPERVM;
typedef DBGFUSERPERVM *PDBGFUSERPERVM;
typedef DBGFUSERPERVM const *PCDBGFUSERPERVM;

/**
 * The per-CPU DBGF data kept in the UVM.
 */
typedef struct DBGFUSERPERVMCPU
{
    /** The guest register set for this CPU.  Can be NULL. */
    R3PTRTYPE(struct DBGFREGSET *) pGuestRegSet;
    /** The hypervisor register set for this CPU.  Can be NULL. */
    R3PTRTYPE(struct DBGFREGSET *) pHyperRegSet;
} DBGFUSERPERVMCPU;


#ifdef IN_RING3
int  dbgfR3AsInit(PUVM pUVM);
void dbgfR3AsTerm(PUVM pUVM);
void dbgfR3AsRelocate(PUVM pUVM, RTGCUINTPTR offDelta);
int  dbgfR3BpInit(PVM pVM);
int  dbgfR3InfoInit(PUVM pUVM);
int  dbgfR3InfoTerm(PUVM pUVM);
int  dbgfR3OSInit(PUVM pUVM);
void dbgfR3OSTermPart1(PUVM pUVM);
void dbgfR3OSTermPart2(PUVM pUVM);
int  dbgfR3OSStackUnwindAssist(PUVM pUVM, VMCPUID idCpu, PDBGFSTACKFRAME pFrame, PRTDBGUNWINDSTATE pState,
                               PCCPUMCTX pInitialCtx, RTDBGAS hAs, uint64_t *puScratch);
int  dbgfR3RegInit(PUVM pUVM);
void dbgfR3RegTerm(PUVM pUVM);
int  dbgfR3TraceInit(PVM pVM);
void dbgfR3TraceRelocate(PVM pVM);
void dbgfR3TraceTerm(PVM pVM);
DECLHIDDEN(int)  dbgfR3TypeInit(PUVM pUVM);
DECLHIDDEN(void) dbgfR3TypeTerm(PUVM pUVM);
int  dbgfR3PlugInInit(PUVM pUVM);
void dbgfR3PlugInTerm(PUVM pUVM);
int  dbgfR3BugCheckInit(PVM pVM);
DECLHIDDEN(int) dbgfR3TracerInit(PVM pVM);
DECLHIDDEN(void) dbgfR3TracerTerm(PVM pVM);

/**
 * DBGF disassembler state (substate of DISSTATE).
 */
typedef struct DBGFDISSTATE
{
    /** Pointer to the current instruction. */
    PCDISOPCODE     pCurInstr;
    /** Size of the instruction in bytes. */
    uint32_t        cbInstr;
    /** Parameters.  */
    DISOPPARAM      Param1;
    DISOPPARAM      Param2;
    DISOPPARAM      Param3;
    DISOPPARAM      Param4;
} DBGFDISSTATE;
/** Pointer to a DBGF disassembler state. */
typedef DBGFDISSTATE *PDBGFDISSTATE;

DECLHIDDEN(int) dbgfR3DisasInstrStateEx(PUVM pUVM, VMCPUID idCpu, PDBGFADDRESS pAddr, uint32_t fFlags,
                                        char *pszOutput, uint32_t cbOutput, PDBGFDISSTATE pDisState);

#endif /* IN_RING3 */

/** @} */

#endif /* !VMM_INCLUDED_SRC_include_DBGFInternal_h */
