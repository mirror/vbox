/* $Id$ */
/** @file
 * VMware SVGA device
 */
/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Graphics_DevVGA_SVGA_h
#define VBOX_INCLUDED_SRC_Graphics_DevVGA_SVGA_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifndef VBOX_WITH_VMSVGA
# error "VBOX_WITH_VMSVGA is not defined"
#endif

#include <VBox/vmm/pdmthread.h>

#include "vmsvga/svga3d_reg.h"

/** Default FIFO size. */
#define VMSVGA_FIFO_SIZE                _2M
/** The old FIFO size. */
#define VMSVGA_FIFO_SIZE_OLD            _128K

/** Default scratch region size. */
#define VMSVGA_SCRATCH_SIZE             0x100
/** Surface memory available to the guest. */
#define VMSVGA_SURFACE_SIZE             (512*1024*1024)
/** Maximum GMR pages. */
#define VMSVGA_MAX_GMR_PAGES            0x100000
/** Maximum nr of GMR ids. */
#define VMSVGA_MAX_GMR_IDS              _8K
/** Maximum number of GMR descriptors.  */
#define VMSVGA_MAX_GMR_DESC_LOOP_COUNT  VMSVGA_MAX_GMR_PAGES

#define VMSVGA_VAL_UNINITIALIZED        (unsigned)-1

/** For validating X and width values.
 * The code assumes it's at least an order of magnitude less than UINT32_MAX. */
#define VMSVGA_MAX_X                    _1M
/** For validating Y and height values.
 * The code assumes it's at least an order of magnitude less than UINT32_MAX. */
#define VMSVGA_MAX_Y                    _1M

/* u32ActionFlags */
#define VMSVGA_ACTION_CHANGEMODE_BIT    0
#define VMSVGA_ACTION_CHANGEMODE        RT_BIT(VMSVGA_ACTION_CHANGEMODE_BIT)


#ifdef DEBUG
/* Enable to log FIFO register accesses. */
//# define DEBUG_FIFO_ACCESS
/* Enable to log GMR page accesses. */
//# define DEBUG_GMR_ACCESS
#endif

#define VMSVGA_FIFO_EXTCMD_NONE                         0
#define VMSVGA_FIFO_EXTCMD_TERMINATE                    1
#define VMSVGA_FIFO_EXTCMD_SAVESTATE                    2
#define VMSVGA_FIFO_EXTCMD_LOADSTATE                    3
#define VMSVGA_FIFO_EXTCMD_RESET                        4
#define VMSVGA_FIFO_EXTCMD_UPDATE_SURFACE_HEAP_BUFFERS  5

/** Size of the region to backup when switching into svga mode. */
#define VMSVGA_VGA_FB_BACKUP_SIZE                       _512K

/** @def VMSVGA_WITH_VGA_FB_BACKUP
 * Enables correct VGA MMIO read/write handling when VMSVGA is enabled.  It
 * is SLOW and probably not entirely right, but it helps with getting 3dmark
 * output and other stuff. */
#define VMSVGA_WITH_VGA_FB_BACKUP                       1

/** @def VMSVGA_WITH_VGA_FB_BACKUP_AND_IN_RING3
 * defined(VMSVGA_WITH_VGA_FB_BACKUP) && defined(IN_RING3)  */
#if (defined(VMSVGA_WITH_VGA_FB_BACKUP) && defined(IN_RING3)) || defined(DOXYGEN_RUNNING)
# define VMSVGA_WITH_VGA_FB_BACKUP_AND_IN_RING3         1
#else
# undef  VMSVGA_WITH_VGA_FB_BACKUP_AND_IN_RING3
#endif

/** @def VMSVGA_WITH_VGA_FB_BACKUP_AND_IN_RZ
 * defined(VMSVGA_WITH_VGA_FB_BACKUP) && !defined(IN_RING3)  */
#if (defined(VMSVGA_WITH_VGA_FB_BACKUP) && !defined(IN_RING3)) || defined(DOXYGEN_RUNNING)
# define VMSVGA_WITH_VGA_FB_BACKUP_AND_IN_RZ            1
#else
# undef  VMSVGA_WITH_VGA_FB_BACKUP_AND_IN_RZ
#endif


typedef struct
{
    PSSMHANDLE      pSSM;
    uint32_t        uVersion;
    uint32_t        uPass;
} VMSVGA_STATE_LOAD;
typedef VMSVGA_STATE_LOAD *PVMSVGA_STATE_LOAD;

/** Host screen viewport.
 * (4th quadrant with negated Y values - usual Windows and X11 world view.) */
typedef struct VMSVGAVIEWPORT
{
    uint32_t        x;                  /**< x coordinate (left). */
    uint32_t        y;                  /**< y coordinate (top). */
    uint32_t        cx;                 /**< width. */
    uint32_t        cy;                 /**< height. */
    /** Right side coordinate (exclusive). Same as x + cx. */
    uint32_t        xRight;
    /** First quadrant low y coordinate.
     * Same as y + cy - 1 in window coordinates. */
    uint32_t        yLowWC;
    /** First quadrant high y coordinate (exclusive) - yLowWC + cy.
     * Same as y - 1 in window coordinates. */
    uint32_t        yHighWC;
    /** Alignment padding. */
    uint32_t        uAlignment;
} VMSVGAVIEWPORT;

/**
 * Screen object state.
 */
typedef struct VMSVGASCREENOBJECT
{
    /** SVGA_SCREEN_* flags. */
    uint32_t    fuScreen;
    /** The screen object id. */
    uint32_t    idScreen;
    /** The screen dimensions. */
    int32_t     xOrigin;
    int32_t     yOrigin;
    uint32_t    cWidth;
    uint32_t    cHeight;
    /** Offset of the screen buffer in the guest VRAM. */
    uint32_t    offVRAM;
    /** Scanline pitch. */
    uint32_t    cbPitch;
    /** Bits per pixel. */
    uint32_t    cBpp;
    bool        fDefined;
    bool        fModified;
} VMSVGASCREENOBJECT;

/** Pointer to the private VMSVGA ring-3 state structure.
 * @todo Still not entirely satisfired with the type name, but better than
 *       the previous lower/upper case only distinction. */
typedef struct VMSVGAR3STATE *PVMSVGAR3STATE;
/** Pointer to the private (implementation specific) VMSVGA3d state. */
typedef struct VMSVGA3DSTATE *PVMSVGA3DSTATE;


/**
 * The VMSVGA device state.
 *
 * This instantatiated as VGASTATE::svga.
 */
typedef struct VMSVGAState
{
    /** Guest physical address of the FIFO memory range. */
    RTGCPHYS                    GCPhysFIFO;
    /** Size in bytes of the FIFO memory range.
     * This may be smaller than cbFIFOConfig after restoring an old VM state.  */
    uint32_t                    cbFIFO;
    /** The configured FIFO size. */
    uint32_t                    cbFIFOConfig;
    /** SVGA id. */
    uint32_t                    u32SVGAId;
    /** SVGA extensions enabled or not. */
    uint32_t                    fEnabled;
    /** SVGA memory area configured status. */
    uint32_t                    fConfigured;
    /** Device is busy handling FIFO requests (VMSVGA_BUSY_F_FIFO,
     *  VMSVGA_BUSY_F_EMT_FORCE). */
    uint32_t volatile           fBusy;
#define VMSVGA_BUSY_F_FIFO          RT_BIT_32(0) /**< The normal true/false busy FIFO bit. */
#define VMSVGA_BUSY_F_EMT_FORCE     RT_BIT_32(1) /**< Bit preventing race status flickering when EMT kicks the FIFO thread. */
    /** Traces (dirty page detection) enabled or not. */
    uint32_t                    fTraces;
    /** Guest OS identifier. */
    uint32_t                    u32GuestId;
    /** Scratch region size (VMSVGAState::au32ScratchRegion). */
    uint32_t                    cScratchRegion;
    /** Irq status. */
    uint32_t                    u32IrqStatus;
    /** Irq mask. */
    uint32_t                    u32IrqMask;
    /** Pitch lock. */
    uint32_t                    u32PitchLock;
    /** Current GMR id. (SVGA_REG_GMR_ID) */
    uint32_t                    u32CurrentGMRId;
    /** Register caps. */
    uint32_t                    u32RegCaps;
    uint32_t                    Padding0; /* Used to be I/O port base address. */
    /** Port io index register. */
    uint32_t                    u32IndexReg;
    /** FIFO request semaphore. */
    SUPSEMEVENT                 hFIFORequestSem;
    /** The last seen SVGA_FIFO_CURSOR_COUNT value.
     * Used by the FIFO thread and its watchdog. */
    uint32_t                    uLastCursorUpdateCount;
    /** Indicates that the FIFO thread is sleeping and might need waking up. */
    bool volatile               fFIFOThreadSleeping;
    /** The legacy GFB mode registers. If used, they correspond to screen 0. */
    /** True when the guest modifies the GFB mode registers. */
    bool                        fGFBRegisters;
    bool                        afPadding[2];
    uint32_t                    uWidth;
    uint32_t                    uHeight;
    uint32_t                    uBpp;
    uint32_t                    cbScanline;
    /** Maximum width supported. */
    uint32_t                    u32MaxWidth;
    /** Maximum height supported. */
    uint32_t                    u32MaxHeight;
    /** Viewport rectangle, i.e. what's currently visible of the target host
     *  window.  This is usually (0,0)(uWidth,uHeight), but if the window is
     *  shrunk and scrolling applied, both the origin and size may differ.  */
    VMSVGAVIEWPORT              viewport;
    /** Action flags */
    uint32_t                    u32ActionFlags;
    /** SVGA 3d extensions enabled or not. */
    bool                        f3DEnabled;
    /** VRAM page monitoring enabled or not. */
    bool                        fVRAMTracking;
    /** External command to be executed in the FIFO thread. */
    uint8_t volatile            u8FIFOExtCommand;
    /** Set by vmsvgaR3RunExtCmdOnFifoThread when it temporarily resumes the FIFO
     * thread and does not want it do anything but the command. */
    bool volatile               fFifoExtCommandWakeup;
#ifdef DEBUG_GMR_ACCESS
    /** GMR debug access handler type handle. */
    PGMPHYSHANDLERTYPE          hGmrAccessHandlerType;
#endif
#if defined(VMSVGA_USE_FIFO_ACCESS_HANDLER) || defined(DEBUG_FIFO_ACCESS)
    /** FIFO debug access handler type handle. */
    PGMPHYSHANDLERTYPE          hFifoAccessHandlerType;
#elif defined(DEBUG_GMR_ACCESS)
    uint32_t                    uPadding1;
#endif
    /** Number of GMRs. */
    uint32_t                    cGMR;
    uint32_t                    uScreenOffset; /* Used only for loading older saved states. */

    /** Scratch array.
     * Putting this at the end since it's big it probably not . */
    uint32_t                    au32ScratchRegion[VMSVGA_SCRATCH_SIZE];

    STAMCOUNTER                 StatRegBitsPerPixelWr;
    STAMCOUNTER                 StatRegBusyWr;
    STAMCOUNTER                 StatRegCursorXxxxWr;
    STAMCOUNTER                 StatRegDepthWr;
    STAMCOUNTER                 StatRegDisplayHeightWr;
    STAMCOUNTER                 StatRegDisplayIdWr;
    STAMCOUNTER                 StatRegDisplayIsPrimaryWr;
    STAMCOUNTER                 StatRegDisplayPositionXWr;
    STAMCOUNTER                 StatRegDisplayPositionYWr;
    STAMCOUNTER                 StatRegDisplayWidthWr;
    STAMCOUNTER                 StatRegEnableWr;
    STAMCOUNTER                 StatRegGmrIdWr;
    STAMCOUNTER                 StatRegGuestIdWr;
    STAMCOUNTER                 StatRegHeightWr;
    STAMCOUNTER                 StatRegIdWr;
    STAMCOUNTER                 StatRegIrqMaskWr;
    STAMCOUNTER                 StatRegNumDisplaysWr;
    STAMCOUNTER                 StatRegNumGuestDisplaysWr;
    STAMCOUNTER                 StatRegPaletteWr;
    STAMCOUNTER                 StatRegPitchLockWr;
    STAMCOUNTER                 StatRegPseudoColorWr;
    STAMCOUNTER                 StatRegReadOnlyWr;
    STAMCOUNTER                 StatRegScratchWr;
    STAMCOUNTER                 StatRegSyncWr;
    STAMCOUNTER                 StatRegTopWr;
    STAMCOUNTER                 StatRegTracesWr;
    STAMCOUNTER                 StatRegUnknownWr;
    STAMCOUNTER                 StatRegWidthWr;

    STAMCOUNTER                 StatRegBitsPerPixelRd;
    STAMCOUNTER                 StatRegBlueMaskRd;
    STAMCOUNTER                 StatRegBusyRd;
    STAMCOUNTER                 StatRegBytesPerLineRd;
    STAMCOUNTER                 StatRegCapabilitesRd;
    STAMCOUNTER                 StatRegConfigDoneRd;
    STAMCOUNTER                 StatRegCursorXxxxRd;
    STAMCOUNTER                 StatRegDepthRd;
    STAMCOUNTER                 StatRegDisplayHeightRd;
    STAMCOUNTER                 StatRegDisplayIdRd;
    STAMCOUNTER                 StatRegDisplayIsPrimaryRd;
    STAMCOUNTER                 StatRegDisplayPositionXRd;
    STAMCOUNTER                 StatRegDisplayPositionYRd;
    STAMCOUNTER                 StatRegDisplayWidthRd;
    STAMCOUNTER                 StatRegEnableRd;
    STAMCOUNTER                 StatRegFbOffsetRd;
    STAMCOUNTER                 StatRegFbSizeRd;
    STAMCOUNTER                 StatRegFbStartRd;
    STAMCOUNTER                 StatRegGmrIdRd;
    STAMCOUNTER                 StatRegGmrMaxDescriptorLengthRd;
    STAMCOUNTER                 StatRegGmrMaxIdsRd;
    STAMCOUNTER                 StatRegGmrsMaxPagesRd;
    STAMCOUNTER                 StatRegGreenMaskRd;
    STAMCOUNTER                 StatRegGuestIdRd;
    STAMCOUNTER                 StatRegHeightRd;
    STAMCOUNTER                 StatRegHostBitsPerPixelRd;
    STAMCOUNTER                 StatRegIdRd;
    STAMCOUNTER                 StatRegIrqMaskRd;
    STAMCOUNTER                 StatRegMaxHeightRd;
    STAMCOUNTER                 StatRegMaxWidthRd;
    STAMCOUNTER                 StatRegMemorySizeRd;
    STAMCOUNTER                 StatRegMemRegsRd;
    STAMCOUNTER                 StatRegMemSizeRd;
    STAMCOUNTER                 StatRegMemStartRd;
    STAMCOUNTER                 StatRegNumDisplaysRd;
    STAMCOUNTER                 StatRegNumGuestDisplaysRd;
    STAMCOUNTER                 StatRegPaletteRd;
    STAMCOUNTER                 StatRegPitchLockRd;
    STAMCOUNTER                 StatRegPsuedoColorRd;
    STAMCOUNTER                 StatRegRedMaskRd;
    STAMCOUNTER                 StatRegScratchRd;
    STAMCOUNTER                 StatRegScratchSizeRd;
    STAMCOUNTER                 StatRegSyncRd;
    STAMCOUNTER                 StatRegTopRd;
    STAMCOUNTER                 StatRegTracesRd;
    STAMCOUNTER                 StatRegUnknownRd;
    STAMCOUNTER                 StatRegVramSizeRd;
    STAMCOUNTER                 StatRegWidthRd;
    STAMCOUNTER                 StatRegWriteOnlyRd;
} VMSVGAState, VMSVGASTATE;


/**
 * The VMSVGA device state for ring-3
 *
 * This instantatiated as VGASTATER3::svga.
 */
typedef struct VMSVGASTATER3
{
    /** The R3 FIFO pointer. */
    R3PTRTYPE(uint32_t *)       pau32FIFO;
    /** R3 Opaque pointer to svga state. */
    R3PTRTYPE(PVMSVGAR3STATE)   pSvgaR3State;
    /** R3 Opaque pointer to 3d state. */
    R3PTRTYPE(PVMSVGA3DSTATE)   p3dState;
    /** The separate VGA frame buffer in svga mode.
     * Unlike the the boch-based VGA device implementation, VMSVGA seems to have a
     * separate frame buffer for VGA and allows concurrent use of both.  The SVGA
     * SDK is making use of this to do VGA text output while testing other things in
     * SVGA mode, displaying the result by switching back to VGA text mode.  So,
     * when entering SVGA mode we copy the first part of the frame buffer here and
     * direct VGA accesses here instead.  It is copied back when leaving SVGA mode. */
    R3PTRTYPE(uint8_t *)        pbVgaFrameBufferR3;
    /** R3 Opaque pointer to an external fifo cmd parameter. */
    R3PTRTYPE(void * volatile)  pvFIFOExtCmdParam;

    /** FIFO external command semaphore. */
    R3PTRTYPE(RTSEMEVENT)       hFIFOExtCmdSem;
    /** FIFO IO Thread. */
    R3PTRTYPE(PPDMTHREAD)       pFIFOIOThread;
} VMSVGASTATER3;


/**
 * The VMSVGA device state for ring-0
 *
 * This instantatiated as VGASTATER0::svga.
 */
typedef struct VMSVGASTATER0
{
    /** The R0 FIFO pointer.
     * @note This only points to the _first_ _page_ of the FIFO!  */
    R0PTRTYPE(uint32_t *)       pau32FIFO;
} VMSVGASTATER0;


typedef struct VGAState   *PVGASTATE;
typedef struct VGASTATER3 *PVGASTATER3;
typedef struct VGASTATER0 *PVGASTATER0;
typedef struct VGASTATERC *PVGASTATERC;
typedef CTX_SUFF(PVGASTATE) PVGASTATECC;

DECLCALLBACK(int) vmsvgaR3PciIORegionFifoMapUnmap(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                                  RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType);
DECLCALLBACK(VBOXSTRICTRC) vmsvgaIORead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb);
DECLCALLBACK(VBOXSTRICTRC) vmsvgaIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb);

DECLCALLBACK(void) vmsvgaR3PortSetViewport(PPDMIDISPLAYPORT pInterface, uint32_t uScreenId,
                                         uint32_t x, uint32_t y, uint32_t cx, uint32_t cy);

int vmsvgaR3Init(PPDMDEVINS pDevIns);
int vmsvgaR3Reset(PPDMDEVINS pDevIns);
int vmsvgaR3Destruct(PPDMDEVINS pDevIns);
int vmsvgaR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
int vmsvgaR3LoadDone(PPDMDEVINS pDevIns);
int vmsvgaR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
DECLCALLBACK(void) vmsvgaR3PowerOn(PPDMDEVINS pDevIns);
DECLCALLBACK(void) vmsvgaR3PowerOff(PPDMDEVINS pDevIns);
void vmsvgaR3FifoWatchdogTimer(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC);

#ifdef IN_RING3
VMSVGASCREENOBJECT *vmsvgaR3GetScreenObject(PVGASTATECC pThisCC, uint32_t idScreen);
int vmsvgaR3UpdateScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, int x, int y, int w, int h);
#endif

int vmsvgaR3GmrTransfer(PVGASTATE pThis, PVGASTATECC pThisCC, const SVGA3dTransferType enmTransferType,
                        uint8_t *pbHstBuf, uint32_t cbHstBuf, uint32_t offHst, int32_t cbHstPitch,
                        SVGAGuestPtr gstPtr, uint32_t offGst, int32_t cbGstPitch,
                        uint32_t cbWidth, uint32_t cHeight);

void vmsvgaR3ClipCopyBox(const SVGA3dSize *pSizeSrc, const SVGA3dSize *pSizeDest, SVGA3dCopyBox *pBox);
void vmsvgaR3ClipBox(const SVGA3dSize *pSize, SVGA3dBox *pBox);
void vmsvgaR3ClipRect(SVGASignedRect const *pBound, SVGASignedRect *pRect);

#endif /* !VBOX_INCLUDED_SRC_Graphics_DevVGA_SVGA_h */

