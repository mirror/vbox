/* $Id$ */
/** @file
 * VMware SVGA device.
 *
 * Logging levels guidelines for this and related files:
 *  - Log() for normal bits.
 *  - LogFlow() for more info.
 *  - Log2 for hex dump of cursor data.
 *  - Log3 for hex dump of shader code.
 *  - Log4 for hex dumps of 3D data.
 *  - Log5 for info about GMR pages.
 *  - LogRel for the usual important stuff.
 *  - LogRel2 for cursor.
 *  - LogRel3 for 3D performance data.
 *  - LogRel4 for HW accelerated graphics output.
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


/** @page pg_dev_vmsvga     VMSVGA - VMware SVGA II Device Emulation
 *
 * This device emulation was contributed by trivirt AG.  It offers an
 * alternative to our Bochs based VGA graphics and 3d emulations.  This is
 * valuable for Xorg based guests, as there is driver support shipping with Xorg
 * since it forked from XFree86.
 *
 *
 * @section sec_dev_vmsvga_sdk  The VMware SDK
 *
 * This is officially deprecated now, however it's still quite useful,
 * especially for getting the old features working:
 * http://vmware-svga.sourceforge.net/
 *
 * They currently point developers at the following resources.
 *  - http://cgit.freedesktop.org/xorg/driver/xf86-video-vmware/
 *  - http://cgit.freedesktop.org/mesa/mesa/tree/src/gallium/drivers/svga/
 *  - http://cgit.freedesktop.org/mesa/vmwgfx/
 *
 * @subsection subsec_dev_vmsvga_sdk_results  Test results
 *
 * Test results:
 *  - 2dmark.img:
 *       + todo
 *  - backdoor-tclo.img:
 *       + todo
 *  - blit-cube.img:
 *       + todo
 *  - bunnies.img:
 *       + todo
 *  - cube.img:
 *       + todo
 *  - cubemark.img:
 *       + todo
 *  - dynamic-vertex-stress.img:
 *       + todo
 *  - dynamic-vertex.img:
 *       + todo
 *  - fence-stress.img:
 *       + todo
 *  - gmr-test.img:
 *       + todo
 *  - half-float-test.img:
 *       + todo
 *  - noscreen-cursor.img:
 *       - The CURSOR I/O and FIFO registers are not implemented, so the mouse
 *         cursor doesn't show. (Hacking the GUI a little, would make the cursor
 *         visible though.)
 *       - Cursor animation via the palette doesn't work.
 *       - During debugging, it turns out that the framebuffer content seems to
 *         be halfways ignore or something (memset(fb, 0xcc, lots)).
 *       - Trouble with way to small FIFO and the 256x256 cursor fails. Need to
 *         grow it 0x10 fold (128KB -> 2MB like in WS10).
 *  - null.img:
 *       + todo
 *  - pong.img:
 *       + todo
 *  - presentReadback.img:
 *       + todo
 *  - resolution-set.img:
 *       + todo
 *  - rt-gamma-test.img:
 *       + todo
 *  - screen-annotation.img:
 *       + todo
 *  - screen-cursor.img:
 *       + todo
 *  - screen-dma-coalesce.img:
 *       + todo
 *  - screen-gmr-discontig.img:
 *       + todo
 *  - screen-gmr-remap.img:
 *       + todo
 *  - screen-multimon.img:
 *       + todo
 *  - screen-present-clip.img:
 *       + todo
 *  - screen-render-test.img:
 *       + todo
 *  - screen-simple.img:
 *       + todo
 *  - screen-text.img:
 *       + todo
 *  - simple-shaders.img:
 *       + todo
 *  - simple_blit.img:
 *       + todo
 *  - tiny-2d-updates.img:
 *       + todo
 *  - video-formats.img:
 *       + todo
 *  - video-sync.img:
 *       + todo
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VMSVGA
#define VMSVGA_USE_EMT_HALT_CODE
#include <VBox/vmm/pdmdev.h>
#include <VBox/version.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/vmm/pgm.h>
#ifdef VMSVGA_USE_EMT_HALT_CODE
# include <VBox/vmm/vmapi.h>
# include <VBox/vmm/vmcpuset.h>
#endif
#include <VBox/sup.h>

#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>
#ifdef IN_RING3
# include <iprt/ctype.h>
# include <iprt/mem.h>
# ifdef VBOX_STRICT
#  include <iprt/time.h>
# endif
#endif

#include <VBox/AssertGuest.h>
#include <VBox/VMMDev.h>
#include <VBoxVideo.h>
#include <VBox/bioslogo.h>

/* should go BEFORE any other DevVGA include to make all DevVGA.h config defines be visible */
#include "DevVGA.h"

#include "DevVGA-SVGA.h"
#include "vmsvga/svga_escape.h"
#include "vmsvga/svga_overlay.h"
#include "vmsvga/svga3d_caps.h"
#ifdef VBOX_WITH_VMSVGA3D
# include "DevVGA-SVGA3d.h"
# ifdef RT_OS_DARWIN
#  include "DevVGA-SVGA3d-cocoa.h"
# endif
# ifdef RT_OS_LINUX
#  ifdef IN_RING3
#include "DevVGA-SVGA3d-glLdr.h"
#  endif
# endif
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/**
 * Macro for checking if a fixed FIFO register is valid according to the
 * current FIFO configuration.
 *
 * @returns true / false.
 * @param   a_iIndex        The fifo register index (like SVGA_FIFO_CAPABILITIES).
 * @param   a_offFifoMin    A valid SVGA_FIFO_MIN value.
 */
#define VMSVGA_IS_VALID_FIFO_REG(a_iIndex, a_offFifoMin) ( ((a_iIndex) + 1) * sizeof(uint32_t) <= (a_offFifoMin) )


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * 64-bit GMR descriptor.
 */
typedef struct
{
   RTGCPHYS GCPhys;
   uint64_t numPages;
} VMSVGAGMRDESCRIPTOR, *PVMSVGAGMRDESCRIPTOR;

/**
 * GMR slot
 */
typedef struct
{
    uint32_t                    cMaxPages;
    uint32_t                    cbTotal;
    uint32_t                    numDescriptors;
    PVMSVGAGMRDESCRIPTOR        paDesc;
} GMR, *PGMR;

#ifdef IN_RING3
/**
 * Internal SVGA ring-3 only state.
 */
typedef struct VMSVGAR3STATE
{
    GMR                    *paGMR; // [VMSVGAState::cGMR]
    struct
    {
        SVGAGuestPtr RT_UNTRUSTED_GUEST         ptr;
        uint32_t RT_UNTRUSTED_GUEST             bytesPerLine;
        SVGAGMRImageFormat RT_UNTRUSTED_GUEST   format;
    } GMRFB;
    struct
    {
        bool                fActive;
        uint32_t            xHotspot;
        uint32_t            yHotspot;
        uint32_t            width;
        uint32_t            height;
        uint32_t            cbData;
        void               *pData;
    } Cursor;
    SVGAColorBGRX           colorAnnotation;

# ifdef VMSVGA_USE_EMT_HALT_CODE
    /** Number of EMTs in BusyDelayedEmts (quicker than scanning the set). */
    uint32_t volatile       cBusyDelayedEmts;
    /** Set of EMTs that are   */
    VMCPUSET                BusyDelayedEmts;
# else
    /** Number of EMTs waiting on hBusyDelayedEmts. */
    uint32_t volatile       cBusyDelayedEmts;
    /** Semaphore that EMTs wait on when reading SVGA_REG_BUSY and the FIFO is
     *  busy (ugly).  */
    RTSEMEVENTMULTI         hBusyDelayedEmts;
# endif

    /** Information obout screens. */
    VMSVGASCREENOBJECT      aScreens[64];

    /** Tracks how much time we waste reading SVGA_REG_BUSY with a busy FIFO. */
    STAMPROFILE             StatBusyDelayEmts;

    STAMPROFILE             StatR3Cmd3dPresentProf;
    STAMPROFILE             StatR3Cmd3dDrawPrimitivesProf;
    STAMPROFILE             StatR3Cmd3dSurfaceDmaProf;
    STAMPROFILE             StatR3Cmd3dBlitSurfaceToScreenProf;
    STAMCOUNTER             StatR3CmdDefineGmr2;
    STAMCOUNTER             StatR3CmdDefineGmr2Free;
    STAMCOUNTER             StatR3CmdDefineGmr2Modify;
    STAMCOUNTER             StatR3CmdRemapGmr2;
    STAMCOUNTER             StatR3CmdRemapGmr2Modify;
    STAMCOUNTER             StatR3CmdInvalidCmd;
    STAMCOUNTER             StatR3CmdFence;
    STAMCOUNTER             StatR3CmdUpdate;
    STAMCOUNTER             StatR3CmdUpdateVerbose;
    STAMCOUNTER             StatR3CmdDefineCursor;
    STAMCOUNTER             StatR3CmdDefineAlphaCursor;
    STAMCOUNTER             StatR3CmdMoveCursor;
    STAMCOUNTER             StatR3CmdDisplayCursor;
    STAMCOUNTER             StatR3CmdRectFill;
    STAMCOUNTER             StatR3CmdRectCopy;
    STAMCOUNTER             StatR3CmdRectRopCopy;
    STAMCOUNTER             StatR3CmdEscape;
    STAMCOUNTER             StatR3CmdDefineScreen;
    STAMCOUNTER             StatR3CmdDestroyScreen;
    STAMCOUNTER             StatR3CmdDefineGmrFb;
    STAMCOUNTER             StatR3CmdBlitGmrFbToScreen;
    STAMCOUNTER             StatR3CmdBlitScreentoGmrFb;
    STAMCOUNTER             StatR3CmdAnnotationFill;
    STAMCOUNTER             StatR3CmdAnnotationCopy;
    STAMCOUNTER             StatR3Cmd3dSurfaceDefine;
    STAMCOUNTER             StatR3Cmd3dSurfaceDefineV2;
    STAMCOUNTER             StatR3Cmd3dSurfaceDestroy;
    STAMCOUNTER             StatR3Cmd3dSurfaceCopy;
    STAMCOUNTER             StatR3Cmd3dSurfaceStretchBlt;
    STAMCOUNTER             StatR3Cmd3dSurfaceDma;
    STAMCOUNTER             StatR3Cmd3dSurfaceScreen;
    STAMCOUNTER             StatR3Cmd3dContextDefine;
    STAMCOUNTER             StatR3Cmd3dContextDestroy;
    STAMCOUNTER             StatR3Cmd3dSetTransform;
    STAMCOUNTER             StatR3Cmd3dSetZRange;
    STAMCOUNTER             StatR3Cmd3dSetRenderState;
    STAMCOUNTER             StatR3Cmd3dSetRenderTarget;
    STAMCOUNTER             StatR3Cmd3dSetTextureState;
    STAMCOUNTER             StatR3Cmd3dSetMaterial;
    STAMCOUNTER             StatR3Cmd3dSetLightData;
    STAMCOUNTER             StatR3Cmd3dSetLightEnable;
    STAMCOUNTER             StatR3Cmd3dSetViewPort;
    STAMCOUNTER             StatR3Cmd3dSetClipPlane;
    STAMCOUNTER             StatR3Cmd3dClear;
    STAMCOUNTER             StatR3Cmd3dPresent;
    STAMCOUNTER             StatR3Cmd3dPresentReadBack;
    STAMCOUNTER             StatR3Cmd3dShaderDefine;
    STAMCOUNTER             StatR3Cmd3dShaderDestroy;
    STAMCOUNTER             StatR3Cmd3dSetShader;
    STAMCOUNTER             StatR3Cmd3dSetShaderConst;
    STAMCOUNTER             StatR3Cmd3dDrawPrimitives;
    STAMCOUNTER             StatR3Cmd3dSetScissorRect;
    STAMCOUNTER             StatR3Cmd3dBeginQuery;
    STAMCOUNTER             StatR3Cmd3dEndQuery;
    STAMCOUNTER             StatR3Cmd3dWaitForQuery;
    STAMCOUNTER             StatR3Cmd3dGenerateMipmaps;
    STAMCOUNTER             StatR3Cmd3dActivateSurface;
    STAMCOUNTER             StatR3Cmd3dDeactivateSurface;

    STAMCOUNTER             StatR3RegConfigDoneWr;
    STAMCOUNTER             StatR3RegGmrDescriptorWr;
    STAMCOUNTER             StatR3RegGmrDescriptorWrErrors;
    STAMCOUNTER             StatR3RegGmrDescriptorWrFree;

    STAMCOUNTER             StatFifoCommands;
    STAMCOUNTER             StatFifoErrors;
    STAMCOUNTER             StatFifoUnkCmds;
    STAMCOUNTER             StatFifoTodoTimeout;
    STAMCOUNTER             StatFifoTodoWoken;
    STAMPROFILE             StatFifoStalls;
    STAMPROFILE             StatFifoExtendedSleep;
# ifdef VMSVGA_USE_FIFO_ACCESS_HANDLER
    STAMCOUNTER             StatFifoAccessHandler;
# endif
    STAMCOUNTER             StatFifoCursorFetchAgain;
    STAMCOUNTER             StatFifoCursorNoChange;
    STAMCOUNTER             StatFifoCursorPosition;
    STAMCOUNTER             StatFifoCursorVisiblity;
    STAMCOUNTER             StatFifoWatchdogWakeUps;
} VMSVGAR3STATE, *PVMSVGAR3STATE;
#endif /* IN_RING3 */


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef IN_RING3
# if defined(VMSVGA_USE_FIFO_ACCESS_HANDLER) || defined(DEBUG_FIFO_ACCESS)
static FNPGMPHYSHANDLER vmsvgaR3FifoAccessHandler;
# endif
# ifdef DEBUG_GMR_ACCESS
static FNPGMPHYSHANDLER vmsvgaR3GmrAccessHandler;
# endif
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef IN_RING3

/**
 * SSM descriptor table for the VMSVGAGMRDESCRIPTOR structure.
 */
static SSMFIELD const g_aVMSVGAGMRDESCRIPTORFields[] =
{
    SSMFIELD_ENTRY_GCPHYS(      VMSVGAGMRDESCRIPTOR,     GCPhys),
    SSMFIELD_ENTRY(             VMSVGAGMRDESCRIPTOR,     numPages),
    SSMFIELD_ENTRY_TERM()
};

/**
 * SSM descriptor table for the GMR structure.
 */
static SSMFIELD const g_aGMRFields[] =
{
    SSMFIELD_ENTRY(             GMR, cMaxPages),
    SSMFIELD_ENTRY(             GMR, cbTotal),
    SSMFIELD_ENTRY(             GMR, numDescriptors),
    SSMFIELD_ENTRY_IGN_HCPTR(   GMR, paDesc),
    SSMFIELD_ENTRY_TERM()
};

/**
 * SSM descriptor table for the VMSVGASCREENOBJECT structure.
 */
static SSMFIELD const g_aVMSVGASCREENOBJECTFields[] =
{
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, fuScreen),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, idScreen),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, xOrigin),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, yOrigin),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, cWidth),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, cHeight),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, offVRAM),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, cbPitch),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, cBpp),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, fDefined),
    SSMFIELD_ENTRY(             VMSVGASCREENOBJECT, fModified),
    SSMFIELD_ENTRY_TERM()
};

/**
 * SSM descriptor table for the VMSVGAR3STATE structure.
 */
static SSMFIELD const g_aVMSVGAR3STATEFields[] =
{
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, paGMR),
    SSMFIELD_ENTRY(             VMSVGAR3STATE, GMRFB),
    SSMFIELD_ENTRY(             VMSVGAR3STATE, Cursor.fActive),
    SSMFIELD_ENTRY(             VMSVGAR3STATE, Cursor.xHotspot),
    SSMFIELD_ENTRY(             VMSVGAR3STATE, Cursor.yHotspot),
    SSMFIELD_ENTRY(             VMSVGAR3STATE, Cursor.width),
    SSMFIELD_ENTRY(             VMSVGAR3STATE, Cursor.height),
    SSMFIELD_ENTRY(             VMSVGAR3STATE, Cursor.cbData),
    SSMFIELD_ENTRY_IGN_HCPTR(   VMSVGAR3STATE, Cursor.pData),
    SSMFIELD_ENTRY(             VMSVGAR3STATE, colorAnnotation),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, cBusyDelayedEmts),
#ifdef VMSVGA_USE_EMT_HALT_CODE
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, BusyDelayedEmts),
#else
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, hBusyDelayedEmts),
#endif
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatBusyDelayEmts),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dPresentProf),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dDrawPrimitivesProf),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSurfaceDmaProf),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dBlitSurfaceToScreenProf),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdDefineGmr2),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdDefineGmr2Free),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdDefineGmr2Modify),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdRemapGmr2),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdRemapGmr2Modify),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdInvalidCmd),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdFence),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdUpdate),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdUpdateVerbose),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdDefineCursor),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdDefineAlphaCursor),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdMoveCursor),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdDisplayCursor),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdRectFill),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdRectCopy),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdRectRopCopy),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdEscape),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdDefineScreen),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdDestroyScreen),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdDefineGmrFb),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdBlitGmrFbToScreen),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdBlitScreentoGmrFb),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdAnnotationFill),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3CmdAnnotationCopy),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSurfaceDefine),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSurfaceDefineV2),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSurfaceDestroy),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSurfaceCopy),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSurfaceStretchBlt),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSurfaceDma),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSurfaceScreen),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dContextDefine),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dContextDestroy),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetTransform),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetZRange),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetRenderState),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetRenderTarget),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetTextureState),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetMaterial),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetLightData),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetLightEnable),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetViewPort),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetClipPlane),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dClear),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dPresent),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dPresentReadBack),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dShaderDefine),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dShaderDestroy),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetShader),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetShaderConst),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dDrawPrimitives),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dSetScissorRect),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dBeginQuery),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dEndQuery),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dWaitForQuery),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dGenerateMipmaps),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dActivateSurface),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3Cmd3dDeactivateSurface),

    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3RegConfigDoneWr),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3RegGmrDescriptorWr),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3RegGmrDescriptorWrErrors),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatR3RegGmrDescriptorWrFree),

    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoCommands),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoErrors),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoUnkCmds),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoTodoTimeout),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoTodoWoken),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoStalls),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoExtendedSleep),
# if defined(VMSVGA_USE_FIFO_ACCESS_HANDLER) || defined(DEBUG_FIFO_ACCESS)
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoAccessHandler),
# endif
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoCursorFetchAgain),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoCursorNoChange),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoCursorPosition),
    SSMFIELD_ENTRY_IGNORE(      VMSVGAR3STATE, StatFifoCursorVisiblity),

    SSMFIELD_ENTRY_TERM()
};

/**
 * SSM descriptor table for the VGAState.svga structure.
 */
static SSMFIELD const g_aVGAStateSVGAFields[] =
{
    SSMFIELD_ENTRY_IGN_GCPHYS(      VMSVGAState, GCPhysFIFO),
    SSMFIELD_ENTRY_IGNORE(          VMSVGAState, cbFIFO),
    SSMFIELD_ENTRY_IGNORE(          VMSVGAState, cbFIFOConfig),
    SSMFIELD_ENTRY(                 VMSVGAState, u32SVGAId),
    SSMFIELD_ENTRY(                 VMSVGAState, fEnabled),
    SSMFIELD_ENTRY(                 VMSVGAState, fConfigured),
    SSMFIELD_ENTRY(                 VMSVGAState, fBusy),
    SSMFIELD_ENTRY(                 VMSVGAState, fTraces),
    SSMFIELD_ENTRY(                 VMSVGAState, u32GuestId),
    SSMFIELD_ENTRY(                 VMSVGAState, cScratchRegion),
    SSMFIELD_ENTRY(                 VMSVGAState, au32ScratchRegion),
    SSMFIELD_ENTRY(                 VMSVGAState, u32IrqStatus),
    SSMFIELD_ENTRY(                 VMSVGAState, u32IrqMask),
    SSMFIELD_ENTRY(                 VMSVGAState, u32PitchLock),
    SSMFIELD_ENTRY(                 VMSVGAState, u32CurrentGMRId),
    SSMFIELD_ENTRY(                 VMSVGAState, u32RegCaps),
    SSMFIELD_ENTRY(                 VMSVGAState, u32IndexReg),
    SSMFIELD_ENTRY_IGNORE(          VMSVGAState, hFIFORequestSem),
    SSMFIELD_ENTRY_IGNORE(          VMSVGAState, uLastCursorUpdateCount),
    SSMFIELD_ENTRY_IGNORE(          VMSVGAState, fFIFOThreadSleeping),
    SSMFIELD_ENTRY_VER(             VMSVGAState, fGFBRegisters, VGA_SAVEDSTATE_VERSION_VMSVGA_SCREENS),
    SSMFIELD_ENTRY(                 VMSVGAState, uWidth),
    SSMFIELD_ENTRY(                 VMSVGAState, uHeight),
    SSMFIELD_ENTRY(                 VMSVGAState, uBpp),
    SSMFIELD_ENTRY(                 VMSVGAState, cbScanline),
    SSMFIELD_ENTRY_VER(             VMSVGAState, uScreenOffset, VGA_SAVEDSTATE_VERSION_VMSVGA),
    SSMFIELD_ENTRY_VER(             VMSVGAState, uCursorX, VGA_SAVEDSTATE_VERSION_VMSVGA_CURSOR),
    SSMFIELD_ENTRY_VER(             VMSVGAState, uCursorY, VGA_SAVEDSTATE_VERSION_VMSVGA_CURSOR),
    SSMFIELD_ENTRY_VER(             VMSVGAState, uCursorID, VGA_SAVEDSTATE_VERSION_VMSVGA_CURSOR),
    SSMFIELD_ENTRY_VER(             VMSVGAState, uCursorOn, VGA_SAVEDSTATE_VERSION_VMSVGA_CURSOR),
    SSMFIELD_ENTRY(                 VMSVGAState, u32MaxWidth),
    SSMFIELD_ENTRY(                 VMSVGAState, u32MaxHeight),
    SSMFIELD_ENTRY(                 VMSVGAState, u32ActionFlags),
    SSMFIELD_ENTRY(                 VMSVGAState, f3DEnabled),
    SSMFIELD_ENTRY(                 VMSVGAState, fVRAMTracking),
    SSMFIELD_ENTRY_IGNORE(          VMSVGAState, u8FIFOExtCommand),
    SSMFIELD_ENTRY_IGNORE(          VMSVGAState, fFifoExtCommandWakeup),
    SSMFIELD_ENTRY_IGNORE(          VMSVGAState, cGMR),
    SSMFIELD_ENTRY_TERM()
};
#endif /* IN_RING3 */


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef IN_RING3
static void vmsvgaR3SetTraces(PPDMDEVINS pDevIns, PVGASTATE pThis, bool fTraces);
static int vmsvgaR3LoadExecFifo(PCPDMDEVHLPR3 pHlp, PVGASTATE pThis, PVGASTATECC pThisCC, PSSMHANDLE pSSM,
                                uint32_t uVersion, uint32_t uPass);
static int vmsvgaR3SaveExecFifo(PCPDMDEVHLPR3 pHlp, PVGASTATECC pThisCC, PSSMHANDLE pSSM);
# ifdef VBOX_WITH_VMSVGA3D
static void vmsvgaR3GmrFree(PVGASTATECC pThisCC, uint32_t idGMR);
# endif /* VBOX_WITH_VMSVGA3D */
#endif /* IN_RING3 */



#ifdef IN_RING3
VMSVGASCREENOBJECT *vmsvgaR3GetScreenObject(PVGASTATECC pThisCC, uint32_t idScreen)
{
    PVMSVGAR3STATE pSVGAState = pThisCC->svga.pSvgaR3State;
    if (   idScreen < (uint32_t)RT_ELEMENTS(pSVGAState->aScreens)
        && pSVGAState
        && pSVGAState->aScreens[idScreen].fDefined)
    {
        return &pSVGAState->aScreens[idScreen];
    }
    return NULL;
}

void vmsvgaR3ResetScreens(PVGASTATECC pThisCC)
{
# ifdef VBOX_WITH_VMSVGA3D
    for (uint32_t idScreen = 0; idScreen < (uint32_t)RT_ELEMENTS(pThisCC->svga.pSvgaR3State->aScreens); ++idScreen)
    {
        VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, idScreen);
        if (pScreen)
            vmsvga3dDestroyScreen(pThisCC, pScreen);
    }
# else
    RT_NOREF(pThisCC);
# endif
}
#endif /* IN_RING3 */

#ifdef LOG_ENABLED

/**
 * Index register string name lookup
 *
 * @returns Index register string or "UNKNOWN"
 * @param   pThis       The shared VGA/VMSVGA state.
 * @param   idxReg      The index register.
 */
static const char *vmsvgaIndexToString(PVGASTATE pThis, uint32_t idxReg)
{
    switch (idxReg)
    {
        case SVGA_REG_ID:                       return "SVGA_REG_ID";
        case SVGA_REG_ENABLE:                   return "SVGA_REG_ENABLE";
        case SVGA_REG_WIDTH:                    return "SVGA_REG_WIDTH";
        case SVGA_REG_HEIGHT:                   return "SVGA_REG_HEIGHT";
        case SVGA_REG_MAX_WIDTH:                return "SVGA_REG_MAX_WIDTH";
        case SVGA_REG_MAX_HEIGHT:               return "SVGA_REG_MAX_HEIGHT";
        case SVGA_REG_DEPTH:                    return "SVGA_REG_DEPTH";
        case SVGA_REG_BITS_PER_PIXEL:           return "SVGA_REG_BITS_PER_PIXEL"; /* Current bpp in the guest */
        case SVGA_REG_HOST_BITS_PER_PIXEL:      return "SVGA_REG_HOST_BITS_PER_PIXEL"; /* (Deprecated) */
        case SVGA_REG_PSEUDOCOLOR:              return "SVGA_REG_PSEUDOCOLOR";
        case SVGA_REG_RED_MASK:                 return "SVGA_REG_RED_MASK";
        case SVGA_REG_GREEN_MASK:               return "SVGA_REG_GREEN_MASK";
        case SVGA_REG_BLUE_MASK:                return "SVGA_REG_BLUE_MASK";
        case SVGA_REG_BYTES_PER_LINE:           return "SVGA_REG_BYTES_PER_LINE";
        case SVGA_REG_VRAM_SIZE:                return "SVGA_REG_VRAM_SIZE"; /* VRAM size */
        case SVGA_REG_FB_START:                 return "SVGA_REG_FB_START"; /* Frame buffer physical address. */
        case SVGA_REG_FB_OFFSET:                return "SVGA_REG_FB_OFFSET"; /* Offset of the frame buffer in VRAM */
        case SVGA_REG_FB_SIZE:                  return "SVGA_REG_FB_SIZE"; /* Frame buffer size */
        case SVGA_REG_CAPABILITIES:             return "SVGA_REG_CAPABILITIES";
        case SVGA_REG_MEM_START:                return "SVGA_REG_MEM_START"; /* FIFO start */
        case SVGA_REG_MEM_SIZE:                 return "SVGA_REG_MEM_SIZE"; /* FIFO size */
        case SVGA_REG_CONFIG_DONE:              return "SVGA_REG_CONFIG_DONE"; /* Set when memory area configured */
        case SVGA_REG_SYNC:                     return "SVGA_REG_SYNC"; /* See "FIFO Synchronization Registers" */
        case SVGA_REG_BUSY:                     return "SVGA_REG_BUSY"; /* See "FIFO Synchronization Registers" */
        case SVGA_REG_GUEST_ID:                 return "SVGA_REG_GUEST_ID"; /* Set guest OS identifier */
        case SVGA_REG_SCRATCH_SIZE:             return "SVGA_REG_SCRATCH_SIZE"; /* Number of scratch registers */
        case SVGA_REG_MEM_REGS:                 return "SVGA_REG_MEM_REGS"; /* Number of FIFO registers */
        case SVGA_REG_PITCHLOCK:                return "SVGA_REG_PITCHLOCK"; /* Fixed pitch for all modes */
        case SVGA_REG_IRQMASK:                  return "SVGA_REG_IRQMASK"; /* Interrupt mask */
        case SVGA_REG_GMR_ID:                   return "SVGA_REG_GMR_ID";
        case SVGA_REG_GMR_DESCRIPTOR:           return "SVGA_REG_GMR_DESCRIPTOR";
        case SVGA_REG_GMR_MAX_IDS:              return "SVGA_REG_GMR_MAX_IDS";
        case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:return "SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH";
        case SVGA_REG_TRACES:                   return "SVGA_REG_TRACES"; /* Enable trace-based updates even when FIFO is on */
        case SVGA_REG_GMRS_MAX_PAGES:           return "SVGA_REG_GMRS_MAX_PAGES"; /* Maximum number of 4KB pages for all GMRs */
        case SVGA_REG_MEMORY_SIZE:              return "SVGA_REG_MEMORY_SIZE"; /* Total dedicated device memory excluding FIFO */
        case SVGA_REG_TOP:                      return "SVGA_REG_TOP"; /* Must be 1 more than the last register */
        case SVGA_PALETTE_BASE:                 return "SVGA_PALETTE_BASE"; /* Base of SVGA color map */
        case SVGA_REG_CURSOR_ID:                return "SVGA_REG_CURSOR_ID";
        case SVGA_REG_CURSOR_X:                 return "SVGA_REG_CURSOR_X";
        case SVGA_REG_CURSOR_Y:                 return "SVGA_REG_CURSOR_Y";
        case SVGA_REG_CURSOR_ON:                return "SVGA_REG_CURSOR_ON";
        case SVGA_REG_NUM_GUEST_DISPLAYS:       return "SVGA_REG_NUM_GUEST_DISPLAYS"; /* Number of guest displays in X/Y direction */
        case SVGA_REG_DISPLAY_ID:               return "SVGA_REG_DISPLAY_ID";  /* Display ID for the following display attributes */
        case SVGA_REG_DISPLAY_IS_PRIMARY:       return "SVGA_REG_DISPLAY_IS_PRIMARY"; /* Whether this is a primary display */
        case SVGA_REG_DISPLAY_POSITION_X:       return "SVGA_REG_DISPLAY_POSITION_X"; /* The display position x */
        case SVGA_REG_DISPLAY_POSITION_Y:       return "SVGA_REG_DISPLAY_POSITION_Y"; /* The display position y */
        case SVGA_REG_DISPLAY_WIDTH:            return "SVGA_REG_DISPLAY_WIDTH";    /* The display's width */
        case SVGA_REG_DISPLAY_HEIGHT:           return "SVGA_REG_DISPLAY_HEIGHT";   /* The display's height */
        case SVGA_REG_NUM_DISPLAYS:             return "SVGA_REG_NUM_DISPLAYS";     /* (Deprecated) */

        default:
            if (idxReg - (uint32_t)SVGA_SCRATCH_BASE < pThis->svga.cScratchRegion)
                return "SVGA_SCRATCH_BASE reg";
            if (idxReg - (uint32_t)SVGA_PALETTE_BASE < (uint32_t)SVGA_NUM_PALETTE_REGS)
                return "SVGA_PALETTE_BASE reg";
            return "UNKNOWN";
    }
}

#ifdef IN_RING3
/**
 * FIFO command name lookup
 *
 * @returns FIFO command string or "UNKNOWN"
 * @param   u32Cmd      FIFO command
 */
static const char *vmsvgaR3FifoCmdToString(uint32_t u32Cmd)
{
    switch (u32Cmd)
    {
        case SVGA_CMD_INVALID_CMD:              return "SVGA_CMD_INVALID_CMD";
        case SVGA_CMD_UPDATE:                   return "SVGA_CMD_UPDATE";
        case SVGA_CMD_RECT_COPY:                return "SVGA_CMD_RECT_COPY";
        case SVGA_CMD_RECT_ROP_COPY:            return "SVGA_CMD_RECT_ROP_COPY";
        case SVGA_CMD_DEFINE_CURSOR:            return "SVGA_CMD_DEFINE_CURSOR";
        case SVGA_CMD_DISPLAY_CURSOR:           return "SVGA_CMD_DISPLAY_CURSOR";
        case SVGA_CMD_MOVE_CURSOR:              return "SVGA_CMD_MOVE_CURSOR";
        case SVGA_CMD_DEFINE_ALPHA_CURSOR:      return "SVGA_CMD_DEFINE_ALPHA_CURSOR";
        case SVGA_CMD_UPDATE_VERBOSE:           return "SVGA_CMD_UPDATE_VERBOSE";
        case SVGA_CMD_FRONT_ROP_FILL:           return "SVGA_CMD_FRONT_ROP_FILL";
        case SVGA_CMD_FENCE:                    return "SVGA_CMD_FENCE";
        case SVGA_CMD_ESCAPE:                   return "SVGA_CMD_ESCAPE";
        case SVGA_CMD_DEFINE_SCREEN:            return "SVGA_CMD_DEFINE_SCREEN";
        case SVGA_CMD_DESTROY_SCREEN:           return "SVGA_CMD_DESTROY_SCREEN";
        case SVGA_CMD_DEFINE_GMRFB:             return "SVGA_CMD_DEFINE_GMRFB";
        case SVGA_CMD_BLIT_GMRFB_TO_SCREEN:     return "SVGA_CMD_BLIT_GMRFB_TO_SCREEN";
        case SVGA_CMD_BLIT_SCREEN_TO_GMRFB:     return "SVGA_CMD_BLIT_SCREEN_TO_GMRFB";
        case SVGA_CMD_ANNOTATION_FILL:          return "SVGA_CMD_ANNOTATION_FILL";
        case SVGA_CMD_ANNOTATION_COPY:          return "SVGA_CMD_ANNOTATION_COPY";
        case SVGA_CMD_DEFINE_GMR2:              return "SVGA_CMD_DEFINE_GMR2";
        case SVGA_CMD_REMAP_GMR2:               return "SVGA_CMD_REMAP_GMR2";
        case SVGA_3D_CMD_SURFACE_DEFINE:        return "SVGA_3D_CMD_SURFACE_DEFINE";
        case SVGA_3D_CMD_SURFACE_DESTROY:       return "SVGA_3D_CMD_SURFACE_DESTROY";
        case SVGA_3D_CMD_SURFACE_COPY:          return "SVGA_3D_CMD_SURFACE_COPY";
        case SVGA_3D_CMD_SURFACE_STRETCHBLT:    return "SVGA_3D_CMD_SURFACE_STRETCHBLT";
        case SVGA_3D_CMD_SURFACE_DMA:           return "SVGA_3D_CMD_SURFACE_DMA";
        case SVGA_3D_CMD_CONTEXT_DEFINE:        return "SVGA_3D_CMD_CONTEXT_DEFINE";
        case SVGA_3D_CMD_CONTEXT_DESTROY:       return "SVGA_3D_CMD_CONTEXT_DESTROY";
        case SVGA_3D_CMD_SETTRANSFORM:          return "SVGA_3D_CMD_SETTRANSFORM";
        case SVGA_3D_CMD_SETZRANGE:             return "SVGA_3D_CMD_SETZRANGE";
        case SVGA_3D_CMD_SETRENDERSTATE:        return "SVGA_3D_CMD_SETRENDERSTATE";
        case SVGA_3D_CMD_SETRENDERTARGET:       return "SVGA_3D_CMD_SETRENDERTARGET";
        case SVGA_3D_CMD_SETTEXTURESTATE:       return "SVGA_3D_CMD_SETTEXTURESTATE";
        case SVGA_3D_CMD_SETMATERIAL:           return "SVGA_3D_CMD_SETMATERIAL";
        case SVGA_3D_CMD_SETLIGHTDATA:          return "SVGA_3D_CMD_SETLIGHTDATA";
        case SVGA_3D_CMD_SETLIGHTENABLED:       return "SVGA_3D_CMD_SETLIGHTENABLED";
        case SVGA_3D_CMD_SETVIEWPORT:           return "SVGA_3D_CMD_SETVIEWPORT";
        case SVGA_3D_CMD_SETCLIPPLANE:          return "SVGA_3D_CMD_SETCLIPPLANE";
        case SVGA_3D_CMD_CLEAR:                 return "SVGA_3D_CMD_CLEAR";
        case SVGA_3D_CMD_PRESENT:               return "SVGA_3D_CMD_PRESENT";
        case SVGA_3D_CMD_SHADER_DEFINE:         return "SVGA_3D_CMD_SHADER_DEFINE";
        case SVGA_3D_CMD_SHADER_DESTROY:        return "SVGA_3D_CMD_SHADER_DESTROY";
        case SVGA_3D_CMD_SET_SHADER:            return "SVGA_3D_CMD_SET_SHADER";
        case SVGA_3D_CMD_SET_SHADER_CONST:      return "SVGA_3D_CMD_SET_SHADER_CONST";
        case SVGA_3D_CMD_DRAW_PRIMITIVES:       return "SVGA_3D_CMD_DRAW_PRIMITIVES";
        case SVGA_3D_CMD_SETSCISSORRECT:        return "SVGA_3D_CMD_SETSCISSORRECT";
        case SVGA_3D_CMD_BEGIN_QUERY:           return "SVGA_3D_CMD_BEGIN_QUERY";
        case SVGA_3D_CMD_END_QUERY:             return "SVGA_3D_CMD_END_QUERY";
        case SVGA_3D_CMD_WAIT_FOR_QUERY:        return "SVGA_3D_CMD_WAIT_FOR_QUERY";
        case SVGA_3D_CMD_PRESENT_READBACK:      return "SVGA_3D_CMD_PRESENT_READBACK";
        case SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN:return "SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN";
        case SVGA_3D_CMD_SURFACE_DEFINE_V2:     return "SVGA_3D_CMD_SURFACE_DEFINE_V2";
        case SVGA_3D_CMD_GENERATE_MIPMAPS:      return "SVGA_3D_CMD_GENERATE_MIPMAPS";
        case SVGA_3D_CMD_ACTIVATE_SURFACE:      return "SVGA_3D_CMD_ACTIVATE_SURFACE";
        case SVGA_3D_CMD_DEACTIVATE_SURFACE:    return "SVGA_3D_CMD_DEACTIVATE_SURFACE";
        default:                                return "UNKNOWN";
    }
}
# endif /* IN_RING3 */

#endif /* LOG_ENABLED */
#ifdef IN_RING3

/**
 * @interface_method_impl{PDMIDISPLAYPORT,pfnSetViewport}
 */
DECLCALLBACK(void) vmsvgaR3PortSetViewport(PPDMIDISPLAYPORT pInterface, uint32_t idScreen, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy)
{
    PVGASTATECC pThisCC = RT_FROM_MEMBER(pInterface, VGASTATECC, IPort);
    PVGASTATE   pThis   = PDMDEVINS_2_DATA(pThisCC->pDevIns, PVGASTATE);

    Log(("vmsvgaPortSetViewPort: screen %d (%d,%d)(%d,%d)\n", idScreen, x, y, cx, cy));
    VMSVGAVIEWPORT const OldViewport = pThis->svga.viewport;

    /** @todo Test how it interacts with multiple screen objects. */
    VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, idScreen);
    uint32_t const uWidth = pScreen ? pScreen->cWidth : 0;
    uint32_t const uHeight = pScreen ? pScreen->cHeight : 0;

    if (x < uWidth)
    {
        pThis->svga.viewport.x      = x;
        pThis->svga.viewport.cx     = RT_MIN(cx, uWidth - x);
        pThis->svga.viewport.xRight = x + pThis->svga.viewport.cx;
    }
    else
    {
        pThis->svga.viewport.x      = uWidth;
        pThis->svga.viewport.cx     = 0;
        pThis->svga.viewport.xRight = uWidth;
    }
    if (y < uHeight)
    {
        pThis->svga.viewport.y       = y;
        pThis->svga.viewport.cy      = RT_MIN(cy, uHeight - y);
        pThis->svga.viewport.yLowWC  = uHeight - y - pThis->svga.viewport.cy;
        pThis->svga.viewport.yHighWC = uHeight - y;
    }
    else
    {
        pThis->svga.viewport.y       = uHeight;
        pThis->svga.viewport.cy      = 0;
        pThis->svga.viewport.yLowWC  = 0;
        pThis->svga.viewport.yHighWC = 0;
    }

# ifdef VBOX_WITH_VMSVGA3D
    /*
     * Now inform the 3D backend.
     */
    if (pThis->svga.f3DEnabled)
        vmsvga3dUpdateHostScreenViewport(pThisCC, idScreen, &OldViewport);
# else
    RT_NOREF(OldViewport);
# endif
}


/**
 * Updating screen information in API
 *
 * @param   pThis       The The shared VGA/VMSVGA instance data.
 * @param   pThisCC     The VGA/VMSVGA state for ring-3.
 */
void vmsvgaR3VBVAResize(PVGASTATE pThis, PVGASTATECC pThisCC)
{
    int rc;

    PVMSVGAR3STATE pSVGAState = pThisCC->svga.pSvgaR3State;

    for (unsigned iScreen = 0; iScreen < RT_ELEMENTS(pSVGAState->aScreens); ++iScreen)
    {
        VMSVGASCREENOBJECT *pScreen = &pSVGAState->aScreens[iScreen];
        if (!pScreen->fModified)
            continue;

        pScreen->fModified = false;

        VBVAINFOVIEW view;
        RT_ZERO(view);
        view.u32ViewIndex     = pScreen->idScreen;
        // view.u32ViewOffset    = 0;
        view.u32ViewSize      = pThis->vram_size;
        view.u32MaxScreenSize = pThis->vram_size;

        VBVAINFOSCREEN screen;
        RT_ZERO(screen);
        screen.u32ViewIndex   = pScreen->idScreen;

        if (pScreen->fDefined)
        {
            if (   pScreen->cWidth  == VMSVGA_VAL_UNINITIALIZED
                || pScreen->cHeight == VMSVGA_VAL_UNINITIALIZED
                || pScreen->cBpp    == VMSVGA_VAL_UNINITIALIZED)
            {
                Assert(pThis->svga.fGFBRegisters);
                continue;
            }

            screen.i32OriginX      = pScreen->xOrigin;
            screen.i32OriginY      = pScreen->yOrigin;
            screen.u32StartOffset  = pScreen->offVRAM;
            screen.u32LineSize     = pScreen->cbPitch;
            screen.u32Width        = pScreen->cWidth;
            screen.u32Height       = pScreen->cHeight;
            screen.u16BitsPerPixel = pScreen->cBpp;
            if (!(pScreen->fuScreen & SVGA_SCREEN_DEACTIVATE))
                screen.u16Flags    = VBVA_SCREEN_F_ACTIVE;
            if (pScreen->fuScreen & SVGA_SCREEN_BLANKING)
                screen.u16Flags   |= VBVA_SCREEN_F_BLANK2;
        }
        else
        {
            /* Screen is destroyed. */
            screen.u16Flags        = VBVA_SCREEN_F_DISABLED;
        }

        rc = pThisCC->pDrv->pfnVBVAResize(pThisCC->pDrv, &view, &screen, pThisCC->pbVRam, /*fResetInputMapping=*/ true);
        AssertRC(rc);
    }
}


/**
 * @interface_method_impl{PDMIDISPLAYPORT,pfnReportMonitorPositions}
 *
 * Used to update screen offsets (positions) since appearently vmwgfx fails to
 * pass correct offsets thru FIFO.
 */
DECLCALLBACK(void) vmsvgaR3PortReportMonitorPositions(PPDMIDISPLAYPORT pInterface, uint32_t cPositions, PCRTPOINT paPositions)
{
    PVGASTATECC         pThisCC    = RT_FROM_MEMBER(pInterface, VGASTATECC, IPort);
    PVGASTATE           pThis      = PDMDEVINS_2_DATA(pThisCC->pDevIns, PVGASTATE);
    PVMSVGAR3STATE      pSVGAState = pThisCC->svga.pSvgaR3State;

    AssertReturnVoid(pSVGAState);

    /* We assume cPositions is the # of outputs Xserver reports and paPositions is (-1, -1) for disabled monitors. */
    cPositions = RT_MIN(cPositions, RT_ELEMENTS(pSVGAState->aScreens));
    for (uint32_t i = 0; i < cPositions; ++i)
    {
        if (   pSVGAState->aScreens[i].xOrigin == paPositions[i].x
            && pSVGAState->aScreens[i].yOrigin == paPositions[i].y)
            continue;

        if (pSVGAState->aScreens[i].xOrigin == -1)
            continue;
        if (pSVGAState->aScreens[i].yOrigin == -1)
            continue;

        pSVGAState->aScreens[i].xOrigin = paPositions[i].x;
        pSVGAState->aScreens[i].yOrigin = paPositions[i].y;
        pSVGAState->aScreens[i].fModified = true;
    }

    vmsvgaR3VBVAResize(pThis, pThisCC);
}

#endif /* IN_RING3 */

/**
 * Read port register
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared VGA/VMSVGA state.
 * @param   pu32        Where to store the read value
 */
static int vmsvgaReadPort(PPDMDEVINS pDevIns, PVGASTATE pThis, uint32_t *pu32)
{
#ifdef IN_RING3
    PVGASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
#endif
    int rc = VINF_SUCCESS;
    *pu32 = 0;

    /* Rough index register validation. */
    uint32_t idxReg = pThis->svga.u32IndexReg;
#if !defined(IN_RING3) && defined(VBOX_STRICT)
    ASSERT_GUEST_MSG_RETURN(idxReg < SVGA_SCRATCH_BASE + pThis->svga.cScratchRegion, ("idxReg=%#x\n", idxReg),
                            VINF_IOM_R3_IOPORT_READ);
#else
    ASSERT_GUEST_MSG_STMT_RETURN(idxReg < SVGA_SCRATCH_BASE + pThis->svga.cScratchRegion, ("idxReg=%#x\n", idxReg),
                                 STAM_REL_COUNTER_INC(&pThis->svga.StatRegUnknownRd),
                                 VINF_SUCCESS);
#endif
    RT_UNTRUSTED_VALIDATED_FENCE();

    /* We must adjust the register number if we're in SVGA_ID_0 mode because the PALETTE range moved. */
    if (   idxReg >= SVGA_REG_CAPABILITIES
        && pThis->svga.u32SVGAId == SVGA_ID_0)
    {
        idxReg += SVGA_PALETTE_BASE - SVGA_REG_CAPABILITIES;
        Log(("vmsvgaWritePort: SVGA_ID_0 reg adj %#x -> %#x\n", pThis->svga.u32IndexReg, idxReg));
    }

    switch (idxReg)
    {
        case SVGA_REG_ID:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegIdRd);
            *pu32 = pThis->svga.u32SVGAId;
            break;

        case SVGA_REG_ENABLE:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegEnableRd);
            *pu32 = pThis->svga.fEnabled;
            break;

        case SVGA_REG_WIDTH:
        {
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegWidthRd);
            if (    pThis->svga.fEnabled
                &&  pThis->svga.uWidth != VMSVGA_VAL_UNINITIALIZED)
                *pu32 = pThis->svga.uWidth;
            else
            {
#ifndef IN_RING3
                rc = VINF_IOM_R3_IOPORT_READ;
#else
                *pu32 = pThisCC->pDrv->cx;
#endif
            }
            break;
        }

        case SVGA_REG_HEIGHT:
        {
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegHeightRd);
            if (    pThis->svga.fEnabled
                &&  pThis->svga.uHeight != VMSVGA_VAL_UNINITIALIZED)
                *pu32 = pThis->svga.uHeight;
            else
            {
#ifndef IN_RING3
                rc = VINF_IOM_R3_IOPORT_READ;
#else
                *pu32 = pThisCC->pDrv->cy;
#endif
            }
            break;
        }

        case SVGA_REG_MAX_WIDTH:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegMaxWidthRd);
            *pu32 = pThis->svga.u32MaxWidth;
            break;

        case SVGA_REG_MAX_HEIGHT:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegMaxHeightRd);
            *pu32 = pThis->svga.u32MaxHeight;
            break;

        case SVGA_REG_DEPTH:
            /* This returns the color depth of the current mode. */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDepthRd);
            switch (pThis->svga.uBpp)
            {
                case 15:
                case 16:
                case 24:
                    *pu32 = pThis->svga.uBpp;
                    break;

                default:
                case 32:
                    *pu32 = 24; /* The upper 8 bits are either alpha bits or not used. */
                    break;
            }
            break;

        case SVGA_REG_HOST_BITS_PER_PIXEL: /* (Deprecated) */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegHostBitsPerPixelRd);
            *pu32 = pThis->svga.uHostBpp;
            break;

        case SVGA_REG_BITS_PER_PIXEL:      /* Current bpp in the guest */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegBitsPerPixelRd);
            *pu32 = pThis->svga.uBpp;
            break;

        case SVGA_REG_PSEUDOCOLOR:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegPsuedoColorRd);
            *pu32 = pThis->svga.uBpp == 8; /* See section 6 "Pseudocolor" in svga_interface.txt. */
            break;

        case SVGA_REG_RED_MASK:
        case SVGA_REG_GREEN_MASK:
        case SVGA_REG_BLUE_MASK:
        {
            uint32_t uBpp;

            if (pThis->svga.fEnabled)
                uBpp = pThis->svga.uBpp;
            else
                uBpp = pThis->svga.uHostBpp;

            uint32_t u32RedMask, u32GreenMask, u32BlueMask;
            switch (uBpp)
            {
                case 8:
                    u32RedMask   = 0x07;
                    u32GreenMask = 0x38;
                    u32BlueMask  = 0xc0;
                    break;

                case 15:
                    u32RedMask   = 0x0000001f;
                    u32GreenMask = 0x000003e0;
                    u32BlueMask  = 0x00007c00;
                    break;

                case 16:
                    u32RedMask   = 0x0000001f;
                    u32GreenMask = 0x000007e0;
                    u32BlueMask  = 0x0000f800;
                    break;

                case 24:
                case 32:
                default:
                    u32RedMask   = 0x00ff0000;
                    u32GreenMask = 0x0000ff00;
                    u32BlueMask  = 0x000000ff;
                    break;
            }
            switch (idxReg)
            {
                case SVGA_REG_RED_MASK:
                    STAM_REL_COUNTER_INC(&pThis->svga.StatRegRedMaskRd);
                    *pu32 = u32RedMask;
                    break;

                case SVGA_REG_GREEN_MASK:
                    STAM_REL_COUNTER_INC(&pThis->svga.StatRegGreenMaskRd);
                    *pu32 = u32GreenMask;
                    break;

                case SVGA_REG_BLUE_MASK:
                    STAM_REL_COUNTER_INC(&pThis->svga.StatRegBlueMaskRd);
                    *pu32 = u32BlueMask;
                    break;
            }
            break;
        }

        case SVGA_REG_BYTES_PER_LINE:
        {
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegBytesPerLineRd);
            if (    pThis->svga.fEnabled
                &&  pThis->svga.cbScanline)
                *pu32 = pThis->svga.cbScanline;
            else
            {
#ifndef IN_RING3
                rc = VINF_IOM_R3_IOPORT_READ;
#else
                *pu32 = pThisCC->pDrv->cbScanline;
#endif
            }
            break;
        }

        case SVGA_REG_VRAM_SIZE:            /* VRAM size */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegVramSizeRd);
            *pu32 = pThis->vram_size;
            break;

        case SVGA_REG_FB_START:             /* Frame buffer physical address. */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegFbStartRd);
            Assert(pThis->GCPhysVRAM <= 0xffffffff);
            *pu32 = pThis->GCPhysVRAM;
            break;

        case SVGA_REG_FB_OFFSET:            /* Offset of the frame buffer in VRAM */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegFbOffsetRd);
            /* Always zero in our case. */
            *pu32 = 0;
            break;

        case SVGA_REG_FB_SIZE:              /* Frame buffer size */
        {
#ifndef IN_RING3
            rc = VINF_IOM_R3_IOPORT_READ;
#else
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegFbSizeRd);

            /* VMWare testcases want at least 4 MB in case the hardware is disabled. */
            if (    pThis->svga.fEnabled
                &&  pThis->svga.uHeight != VMSVGA_VAL_UNINITIALIZED)
            {
                /* Hardware enabled; return real framebuffer size .*/
                *pu32 = (uint32_t)pThis->svga.uHeight * pThis->svga.cbScanline;
            }
            else
                *pu32 = RT_MAX(0x100000, (uint32_t)pThisCC->pDrv->cy * pThisCC->pDrv->cbScanline);

            *pu32 = RT_MIN(pThis->vram_size, *pu32);
            Log(("h=%d w=%d bpp=%d\n", pThisCC->pDrv->cy, pThisCC->pDrv->cx, pThisCC->pDrv->cBits));
#endif
            break;
        }

        case SVGA_REG_CAPABILITIES:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegCapabilitesRd);
            *pu32 = pThis->svga.u32RegCaps;
            break;

        case SVGA_REG_MEM_START:           /* FIFO start */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegMemStartRd);
            Assert(pThis->svga.GCPhysFIFO <= 0xffffffff);
            *pu32 = pThis->svga.GCPhysFIFO;
            break;

        case SVGA_REG_MEM_SIZE:            /* FIFO size */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegMemSizeRd);
            *pu32 = pThis->svga.cbFIFO;
            break;

        case SVGA_REG_CONFIG_DONE:         /* Set when memory area configured */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegConfigDoneRd);
            *pu32 = pThis->svga.fConfigured;
            break;

        case SVGA_REG_SYNC:                /* See "FIFO Synchronization Registers" */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegSyncRd);
            *pu32 = 0;
            break;

        case SVGA_REG_BUSY:                /* See "FIFO Synchronization Registers" */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegBusyRd);
            if (pThis->svga.fBusy)
            {
#ifndef IN_RING3
                /* Go to ring-3 and halt the CPU. */
                rc = VINF_IOM_R3_IOPORT_READ;
                RT_NOREF(pDevIns);
                break;
#else
# if defined(VMSVGA_USE_EMT_HALT_CODE)
                /* The guest is basically doing a HLT via the device here, but with
                   a special wake up condition on FIFO completion. */
                PVMSVGAR3STATE pSVGAState = pThisCC->svga.pSvgaR3State;
                STAM_REL_PROFILE_START(&pSVGAState->StatBusyDelayEmts, EmtDelay);
                PVM         pVM   = PDMDevHlpGetVM(pDevIns);
                VMCPUID     idCpu = PDMDevHlpGetCurrentCpuId(pDevIns);
                VMCPUSET_ATOMIC_ADD(&pSVGAState->BusyDelayedEmts, idCpu);
                ASMAtomicIncU32(&pSVGAState->cBusyDelayedEmts);
                if (pThis->svga.fBusy)
                {
                    PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect); /* hack around lock order issue. */
                    rc = VMR3WaitForDeviceReady(pVM, idCpu);
                    PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_IGNORED);
                }
                ASMAtomicDecU32(&pSVGAState->cBusyDelayedEmts);
                VMCPUSET_ATOMIC_DEL(&pSVGAState->BusyDelayedEmts, idCpu);
# else

                /* Delay the EMT a bit so the FIFO and others can get some work done.
                   This used to be a crude 50 ms sleep. The current code tries to be
                   more efficient, but the consept is still very crude. */
                PVMSVGAR3STATE pSVGAState = pThisCC->svga.pSvgaR3State;
                STAM_REL_PROFILE_START(&pSVGAState->StatBusyDelayEmts, EmtDelay);
                RTThreadYield();
                if (pThis->svga.fBusy)
                {
                    uint32_t cRefs = ASMAtomicIncU32(&pSVGAState->cBusyDelayedEmts);

                    if (pThis->svga.fBusy && cRefs == 1)
                        RTSemEventMultiReset(pSVGAState->hBusyDelayedEmts);
                    if (pThis->svga.fBusy)
                    {
                        /** @todo If this code is going to stay, we need to call into the halt/wait
                         *        code in VMEmt.cpp here, otherwise all kind of EMT interaction will
                         *        suffer when the guest is polling on a busy FIFO. */
                        uint64_t cNsMaxWait = TMVirtualSyncGetNsToDeadline(PDMDevHlpGetVM(pDevIns));
                        if (cNsMaxWait >= RT_NS_100US)
                            RTSemEventMultiWaitEx(pSVGAState->hBusyDelayedEmts,
                                                  RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_RELATIVE | RTSEMWAIT_FLAGS_NORESUME,
                                                  RT_MIN(cNsMaxWait, RT_NS_10MS));
                    }

                    ASMAtomicDecU32(&pSVGAState->cBusyDelayedEmts);
                }
                STAM_REL_PROFILE_STOP(&pSVGAState->StatBusyDelayEmts, EmtDelay);
# endif
                *pu32 = pThis->svga.fBusy != 0;
#endif
            }
            else
                *pu32 = false;
            break;

        case SVGA_REG_GUEST_ID:            /* Set guest OS identifier */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegGuestIdRd);
            *pu32 = pThis->svga.u32GuestId;
            break;

        case SVGA_REG_SCRATCH_SIZE:        /* Number of scratch registers */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegScratchSizeRd);
            *pu32 = pThis->svga.cScratchRegion;
            break;

        case SVGA_REG_MEM_REGS:            /* Number of FIFO registers */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegMemRegsRd);
            *pu32 = SVGA_FIFO_NUM_REGS;
            break;

        case SVGA_REG_PITCHLOCK:           /* Fixed pitch for all modes */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegPitchLockRd);
            *pu32 = pThis->svga.u32PitchLock;
            break;

        case SVGA_REG_IRQMASK:             /* Interrupt mask */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegIrqMaskRd);
            *pu32 = pThis->svga.u32IrqMask;
            break;

        /* See "Guest memory regions" below. */
        case SVGA_REG_GMR_ID:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegGmrIdRd);
            *pu32 = pThis->svga.u32CurrentGMRId;
            break;

        case SVGA_REG_GMR_DESCRIPTOR:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegWriteOnlyRd);
            /* Write only */
            *pu32 = 0;
            break;

        case SVGA_REG_GMR_MAX_IDS:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegGmrMaxIdsRd);
            *pu32 = pThis->svga.cGMR;
            break;

        case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegGmrMaxDescriptorLengthRd);
            *pu32 = VMSVGA_MAX_GMR_PAGES;
            break;

        case SVGA_REG_TRACES:            /* Enable trace-based updates even when FIFO is on */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegTracesRd);
            *pu32 = pThis->svga.fTraces;
            break;

        case SVGA_REG_GMRS_MAX_PAGES:    /* Maximum number of 4KB pages for all GMRs */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegGmrsMaxPagesRd);
            *pu32 = VMSVGA_MAX_GMR_PAGES;
            break;

        case SVGA_REG_MEMORY_SIZE:       /* Total dedicated device memory excluding FIFO */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegMemorySizeRd);
            *pu32 = VMSVGA_SURFACE_SIZE;
            break;

        case SVGA_REG_TOP:               /* Must be 1 more than the last register */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegTopRd);
            break;

        /* Mouse cursor support. */
        case SVGA_REG_CURSOR_ID:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegCursorIdRd);
            *pu32 = pThis->svga.uCursorID;
            break;

        case SVGA_REG_CURSOR_X:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegCursorXRd);
            *pu32 = pThis->svga.uCursorX;
            break;

        case SVGA_REG_CURSOR_Y:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegCursorYRd);
            *pu32 = pThis->svga.uCursorY;
            break;

        case SVGA_REG_CURSOR_ON:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegCursorOnRd);
            *pu32 = pThis->svga.uCursorOn;
            break;

        /* Legacy multi-monitor support */
        case SVGA_REG_NUM_GUEST_DISPLAYS:/* Number of guest displays in X/Y direction */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegNumGuestDisplaysRd);
            *pu32 = 1;
            break;

        case SVGA_REG_DISPLAY_ID:        /* Display ID for the following display attributes */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayIdRd);
            *pu32 = 0;
            break;

        case SVGA_REG_DISPLAY_IS_PRIMARY:/* Whether this is a primary display */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayIsPrimaryRd);
            *pu32 = 0;
            break;

        case SVGA_REG_DISPLAY_POSITION_X:/* The display position x */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayPositionXRd);
            *pu32 = 0;
            break;

        case SVGA_REG_DISPLAY_POSITION_Y:/* The display position y */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayPositionYRd);
            *pu32 = 0;
            break;

        case SVGA_REG_DISPLAY_WIDTH:     /* The display's width */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayWidthRd);
            *pu32 = pThis->svga.uWidth;
            break;

        case SVGA_REG_DISPLAY_HEIGHT:    /* The display's height */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayHeightRd);
            *pu32 = pThis->svga.uHeight;
            break;

        case SVGA_REG_NUM_DISPLAYS:        /* (Deprecated) */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegNumDisplaysRd);
            /* We must return something sensible here otherwise the Linux driver
               will take a legacy code path without 3d support.  This number also
               limits how many screens Linux guests will allow. */
            *pu32 = pThis->cMonitors;
            break;

        default:
        {
            uint32_t offReg;
            if ((offReg = idxReg - SVGA_SCRATCH_BASE) < pThis->svga.cScratchRegion)
            {
                RT_UNTRUSTED_VALIDATED_FENCE();
                *pu32 = pThis->svga.au32ScratchRegion[offReg];
                STAM_REL_COUNTER_INC(&pThis->svga.StatRegScratchRd);
            }
            else if ((offReg = idxReg - SVGA_PALETTE_BASE) < (uint32_t)SVGA_NUM_PALETTE_REGS)
            {
                /* Note! Using last_palette rather than palette here to preserve the VGA one. */
                STAM_REL_COUNTER_INC(&pThis->svga.StatRegPaletteRd);
                RT_UNTRUSTED_VALIDATED_FENCE();
                uint32_t u32 = pThis->last_palette[offReg / 3];
                switch (offReg % 3)
                {
                    case 0: *pu32 = (u32 >> 16) & 0xff; break; /* red */
                    case 1: *pu32 = (u32 >>  8) & 0xff; break; /* green */
                    case 2: *pu32 =  u32        & 0xff; break; /* blue */
                }
            }
            else
            {
#if !defined(IN_RING3) && defined(VBOX_STRICT)
                rc = VINF_IOM_R3_IOPORT_READ;
#else
                STAM_REL_COUNTER_INC(&pThis->svga.StatRegUnknownRd);

                /* Do not assert. The guest might be reading all registers. */
                LogFunc(("Unknown reg=%#x\n", idxReg));
#endif
            }
            break;
        }
    }
    Log(("vmsvgaReadPort index=%s (%d) val=%#x rc=%x\n", vmsvgaIndexToString(pThis, idxReg), idxReg, *pu32, rc));
    return rc;
}

#ifdef IN_RING3
/**
 * Apply the current resolution settings to change the video mode.
 *
 * @returns VBox status code.
 * @param   pThis       The shared VGA state.
 * @param   pThisCC     The ring-3 VGA state.
 */
static int vmsvgaR3ChangeMode(PVGASTATE pThis, PVGASTATECC pThisCC)
{
    /* Always do changemode on FIFO thread. */
    Assert(RTThreadSelf() == pThisCC->svga.pFIFOIOThread->Thread);

    PVMSVGAR3STATE pSVGAState = pThisCC->svga.pSvgaR3State;

    pThisCC->pDrv->pfnLFBModeChange(pThisCC->pDrv, true);

    if (pThis->svga.fGFBRegisters)
    {
        /* "For backwards compatibility, when the GFB mode registers (WIDTH,
         * HEIGHT, PITCHLOCK, BITS_PER_PIXEL) are modified, the SVGA device
         * deletes all screens other than screen #0, and redefines screen
         * #0 according to the specified mode. Drivers that use
         * SVGA_CMD_DEFINE_SCREEN should destroy or redefine screen #0."
         */

        VMSVGASCREENOBJECT *pScreen = &pSVGAState->aScreens[0];
        pScreen->fDefined  = true;
        pScreen->fModified = true;
        pScreen->fuScreen  = SVGA_SCREEN_MUST_BE_SET | SVGA_SCREEN_IS_PRIMARY;
        pScreen->idScreen  = 0;
        pScreen->xOrigin   = 0;
        pScreen->yOrigin   = 0;
        pScreen->offVRAM   = 0;
        pScreen->cbPitch   = pThis->svga.cbScanline;
        pScreen->cWidth    = pThis->svga.uWidth;
        pScreen->cHeight   = pThis->svga.uHeight;
        pScreen->cBpp      = pThis->svga.uBpp;

        for (unsigned iScreen = 1; iScreen < RT_ELEMENTS(pSVGAState->aScreens); ++iScreen)
        {
            /* Delete screen. */
            pScreen = &pSVGAState->aScreens[iScreen];
            if (pScreen->fDefined)
            {
                pScreen->fModified = true;
                pScreen->fDefined = false;
            }
        }
    }
    else
    {
        /* "If Screen Objects are supported, they can be used to fully
         * replace the functionality provided by the framebuffer registers
         * (SVGA_REG_WIDTH, HEIGHT, etc.) and by SVGA_CAP_DISPLAY_TOPOLOGY."
         */
        pThis->svga.uWidth  = VMSVGA_VAL_UNINITIALIZED;
        pThis->svga.uHeight = VMSVGA_VAL_UNINITIALIZED;
        pThis->svga.uBpp    = pThis->svga.uHostBpp;
    }

    vmsvgaR3VBVAResize(pThis, pThisCC);

    /* Last stuff. For the VGA device screenshot. */
    pThis->last_bpp        = pSVGAState->aScreens[0].cBpp;
    pThis->last_scr_width  = pSVGAState->aScreens[0].cWidth;
    pThis->last_scr_height = pSVGAState->aScreens[0].cHeight;
    pThis->last_width      = pSVGAState->aScreens[0].cWidth;
    pThis->last_height     = pSVGAState->aScreens[0].cHeight;

    /* vmsvgaPortSetViewPort not called after state load; set sensible defaults. */
    if (    pThis->svga.viewport.cx == 0
        &&  pThis->svga.viewport.cy == 0)
    {
        pThis->svga.viewport.cx      = pSVGAState->aScreens[0].cWidth;
        pThis->svga.viewport.xRight  = pSVGAState->aScreens[0].cWidth;
        pThis->svga.viewport.cy      = pSVGAState->aScreens[0].cHeight;
        pThis->svga.viewport.yHighWC = pSVGAState->aScreens[0].cHeight;
        pThis->svga.viewport.yLowWC  = 0;
    }

    return VINF_SUCCESS;
}

int vmsvgaR3UpdateScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, int x, int y, int w, int h)
{
    VBVACMDHDR cmd;
    cmd.x = (int16_t)(pScreen->xOrigin + x);
    cmd.y = (int16_t)(pScreen->yOrigin + y);
    cmd.w = (uint16_t)w;
    cmd.h = (uint16_t)h;

    pThisCC->pDrv->pfnVBVAUpdateBegin(pThisCC->pDrv, pScreen->idScreen);
    pThisCC->pDrv->pfnVBVAUpdateProcess(pThisCC->pDrv, pScreen->idScreen, &cmd, sizeof(cmd));
    pThisCC->pDrv->pfnVBVAUpdateEnd(pThisCC->pDrv, pScreen->idScreen,
                                    pScreen->xOrigin + x, pScreen->yOrigin + y, w, h);

    return VINF_SUCCESS;
}

#endif /* IN_RING3 */
#if defined(IN_RING0) || defined(IN_RING3)

/**
 * Safely updates the SVGA_FIFO_BUSY register (in shared memory).
 *
 * @param   pThis       The shared VGA/VMSVGA instance data.
 * @param   pThisCC     The VGA/VMSVGA state for the current context.
 * @param   fState      The busy state.
 */
DECLINLINE(void) vmsvgaHCSafeFifoBusyRegUpdate(PVGASTATE pThis, PVGASTATECC pThisCC, bool fState)
{
    ASMAtomicWriteU32(&pThisCC->svga.pau32FIFO[SVGA_FIFO_BUSY], fState);

    if (RT_UNLIKELY(fState != (pThis->svga.fBusy != 0)))
    {
        /* Race / unfortunately scheduling. Highly unlikly. */
        uint32_t cLoops = 64;
        do
        {
            ASMNopPause();
            fState = (pThis->svga.fBusy != 0);
            ASMAtomicWriteU32(&pThisCC->svga.pau32FIFO[SVGA_FIFO_BUSY], fState != 0);
        } while (cLoops-- > 0 && fState != (pThis->svga.fBusy != 0));
    }
}


/**
 * Update the scanline pitch in response to the guest changing mode
 * width/bpp.
 *
 * @param   pThis       The shared VGA/VMSVGA state.
 * @param   pThisCC     The VGA/VMSVGA state for the current context.
 */
DECLINLINE(void) vmsvgaHCUpdatePitch(PVGASTATE pThis, PVGASTATECC pThisCC)
{
    uint32_t RT_UNTRUSTED_VOLATILE_GUEST *pFIFO = pThisCC->svga.pau32FIFO;
    uint32_t uFifoPitchLock = pFIFO[SVGA_FIFO_PITCHLOCK];
    uint32_t uRegPitchLock  = pThis->svga.u32PitchLock;
    uint32_t uFifoMin       = pFIFO[SVGA_FIFO_MIN];

    /* The SVGA_FIFO_PITCHLOCK register is only valid if SVGA_FIFO_MIN points past
     * it. If SVGA_FIFO_MIN is small, there may well be data at the SVGA_FIFO_PITCHLOCK
     * location but it has a different meaning.
     */
    if ((uFifoMin / sizeof(uint32_t)) <= SVGA_FIFO_PITCHLOCK)
        uFifoPitchLock = 0;

    /* Sanitize values. */
    if ((uFifoPitchLock < 200) || (uFifoPitchLock > 32768))
        uFifoPitchLock = 0;
    if ((uRegPitchLock  < 200) || (uRegPitchLock  > 32768))
        uRegPitchLock = 0;

    /* Prefer the register value to the FIFO value.*/
    if (uRegPitchLock)
        pThis->svga.cbScanline = uRegPitchLock;
    else if (uFifoPitchLock)
        pThis->svga.cbScanline = uFifoPitchLock;
    else
        pThis->svga.cbScanline = pThis->svga.uWidth * (RT_ALIGN(pThis->svga.uBpp, 8) / 8);

    if ((uFifoMin / sizeof(uint32_t)) <= SVGA_FIFO_PITCHLOCK)
        pThis->svga.u32PitchLock = pThis->svga.cbScanline;
}

#endif /* IN_RING0 || IN_RING3 */

#ifdef IN_RING3

/**
 * Sends cursor position and visibility information from legacy
 * SVGA registers to the front-end.
 */
static void vmsvgaR3RegUpdateCursor(PVGASTATECC pThisCC, PVGASTATE pThis, uint32_t uCursorOn)
{
    /*
     * Writing the X/Y/ID registers does not trigger changes; only writing the
     * SVGA_REG_CURSOR_ON register does. That minimizes the overhead.
     * We boldly assume that guests aren't stupid and aren't writing the CURSOR_ON
     * register if they don't have to.
     */
    uint32_t x, y, idScreen;
    uint32_t fFlags = VBVA_CURSOR_VALID_DATA;

    x = pThis->svga.uCursorX;
    y = pThis->svga.uCursorY;
    idScreen = SVGA_ID_INVALID; /* The old register interface is single screen only. */

    /* The original values for SVGA_REG_CURSOR_ON were off (0) and on (1); later, the values
     * were extended as follows:
     *
     *   SVGA_CURSOR_ON_HIDE               0
     *   SVGA_CURSOR_ON_SHOW               1
     *   SVGA_CURSOR_ON_REMOVE_FROM_FB     2 - cursor on but not in the framebuffer
     *   SVGA_CURSOR_ON_RESTORE_TO_FB      3 - cursor on, possibly in the framebuffer
     *
     * Since we never draw the cursor into the guest's framebuffer, we do not need to
     * distinguish between the non-zero values but still remember them.
     */
    if (RT_BOOL(pThis->svga.uCursorOn) != RT_BOOL(uCursorOn))
    {
        LogRel2(("vmsvgaR3RegUpdateCursor: uCursorOn %d prev CursorOn %d (%d,%d)\n", uCursorOn, pThis->svga.uCursorOn, x, y));
        pThisCC->pDrv->pfnVBVAMousePointerShape(pThisCC->pDrv, RT_BOOL(uCursorOn), false, 0, 0, 0, 0, NULL);
    }
    pThis->svga.uCursorOn = uCursorOn;
    pThisCC->pDrv->pfnVBVAReportCursorPosition(pThisCC->pDrv, fFlags, idScreen, x, y);
}


/**
 * Copy a rectangle of pixels within guest VRAM.
 */
static void vmsvgaR3RectCopy(PVGASTATECC pThisCC, VMSVGASCREENOBJECT const *pScreen, uint32_t srcX, uint32_t srcY,
                             uint32_t dstX, uint32_t dstY, uint32_t width, uint32_t height, unsigned cbFrameBuffer)
{
    if (!width || !height)
        return; /* Nothing to do, don't even bother. */

    /*
     * The guest VRAM (aka GFB) is considered to be a bitmap in the format
     * corresponding to the current display mode.
     */
    uint32_t const  cbPixel = RT_ALIGN(pScreen->cBpp, 8) / 8;
    uint32_t const  cbScanline = pScreen->cbPitch ? pScreen->cbPitch : width * cbPixel;
    uint8_t const   *pSrc;
    uint8_t         *pDst;
    unsigned const  cbRectWidth = width * cbPixel;
    unsigned        uMaxOffset;

    uMaxOffset = (RT_MAX(srcY, dstY) + height) * cbScanline + (RT_MAX(srcX, dstX) + width) * cbPixel;
    if (uMaxOffset >= cbFrameBuffer)
    {
        Log(("Max offset (%u) too big for framebuffer (%u bytes), ignoring!\n", uMaxOffset, cbFrameBuffer));
        return; /* Just don't listen to a bad guest. */
    }

    pSrc = pDst = pThisCC->pbVRam;
    pSrc += srcY * cbScanline + srcX * cbPixel;
    pDst += dstY * cbScanline + dstX * cbPixel;

    if (srcY >= dstY)
    {
        /* Source below destination, copy top to bottom. */
        for (; height > 0; height--)
        {
            memmove(pDst, pSrc, cbRectWidth);
            pSrc += cbScanline;
            pDst += cbScanline;
        }
    }
    else
    {
        /* Source above destination, copy bottom to top. */
        pSrc += cbScanline * (height - 1);
        pDst += cbScanline * (height - 1);
        for (; height > 0; height--)
        {
            memmove(pDst, pSrc, cbRectWidth);
            pSrc -= cbScanline;
            pDst -= cbScanline;
        }
    }
}

#endif /* IN_RING3 */


/**
 * Write port register
 *
 * @returns Strict VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared VGA/VMSVGA state.
 * @param   pThisCC     The VGA/VMSVGA state for the current context.
 * @param   u32         Value to write
 */
static VBOXSTRICTRC vmsvgaWritePort(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC, uint32_t u32)
{
#ifdef IN_RING3
    PVMSVGAR3STATE pSVGAState = pThisCC->svga.pSvgaR3State;
#endif
    VBOXSTRICTRC rc = VINF_SUCCESS;
    RT_NOREF(pThisCC);

    /* Rough index register validation. */
    uint32_t idxReg = pThis->svga.u32IndexReg;
#if !defined(IN_RING3) && defined(VBOX_STRICT)
    ASSERT_GUEST_MSG_RETURN(idxReg < SVGA_SCRATCH_BASE + pThis->svga.cScratchRegion, ("idxReg=%#x\n", idxReg),
                            VINF_IOM_R3_IOPORT_WRITE);
#else
    ASSERT_GUEST_MSG_STMT_RETURN(idxReg < SVGA_SCRATCH_BASE + pThis->svga.cScratchRegion, ("idxReg=%#x\n", idxReg),
                                 STAM_REL_COUNTER_INC(&pThis->svga.StatRegUnknownWr),
                                 VINF_SUCCESS);
#endif
    RT_UNTRUSTED_VALIDATED_FENCE();

    /* We must adjust the register number if we're in SVGA_ID_0 mode because the PALETTE range moved. */
    if (   idxReg >= SVGA_REG_CAPABILITIES
        && pThis->svga.u32SVGAId == SVGA_ID_0)
    {
        idxReg += SVGA_PALETTE_BASE - SVGA_REG_CAPABILITIES;
        Log(("vmsvgaWritePort: SVGA_ID_0 reg adj %#x -> %#x\n", pThis->svga.u32IndexReg, idxReg));
    }
    Log(("vmsvgaWritePort index=%s (%d) val=%#x\n", vmsvgaIndexToString(pThis, idxReg), idxReg, u32));
    /* Check if the guest uses legacy registers. See vmsvgaR3ChangeMode */
    switch (idxReg)
    {
        case SVGA_REG_WIDTH:
        case SVGA_REG_HEIGHT:
        case SVGA_REG_PITCHLOCK:
        case SVGA_REG_BITS_PER_PIXEL:
            pThis->svga.fGFBRegisters = true;
            break;
        default:
            break;
    }

    switch (idxReg)
    {
        case SVGA_REG_ID:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegIdWr);
            if (   u32 == SVGA_ID_0
                || u32 == SVGA_ID_1
                || u32 == SVGA_ID_2)
                pThis->svga.u32SVGAId = u32;
            else
                PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "Trying to set SVGA_REG_ID to %#x (%d)\n", u32, u32);
            break;

        case SVGA_REG_ENABLE:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegEnableWr);
#ifdef IN_RING3
            if (    (u32 & SVGA_REG_ENABLE_ENABLE)
                &&  pThis->svga.fEnabled == false)
            {
                /* Make a backup copy of the first 512kb in order to save font data etc. */
                /** @todo should probably swap here, rather than copy + zero */
                memcpy(pThisCC->svga.pbVgaFrameBufferR3, pThisCC->pbVRam, VMSVGA_VGA_FB_BACKUP_SIZE);
                memset(pThisCC->pbVRam, 0, VMSVGA_VGA_FB_BACKUP_SIZE);
            }

            pThis->svga.fEnabled = u32;
            if (pThis->svga.fEnabled)
            {
                if (    pThis->svga.uWidth  == VMSVGA_VAL_UNINITIALIZED
                    &&  pThis->svga.uHeight == VMSVGA_VAL_UNINITIALIZED)
                {
                    /* Keep the current mode. */
                    pThis->svga.uWidth  = pThisCC->pDrv->cx;
                    pThis->svga.uHeight = pThisCC->pDrv->cy;
                    pThis->svga.uBpp    = (pThisCC->pDrv->cBits + 7) & ~7;
                }

                if (    pThis->svga.uWidth  != VMSVGA_VAL_UNINITIALIZED
                    &&  pThis->svga.uHeight != VMSVGA_VAL_UNINITIALIZED)
                    ASMAtomicOrU32(&pThis->svga.u32ActionFlags, VMSVGA_ACTION_CHANGEMODE);
# ifdef LOG_ENABLED
                uint32_t *pFIFO = pThisCC->svga.pau32FIFO;
                Log(("configured=%d busy=%d\n", pThis->svga.fConfigured, pFIFO[SVGA_FIFO_BUSY]));
                Log(("next %x stop %x\n", pFIFO[SVGA_FIFO_NEXT_CMD], pFIFO[SVGA_FIFO_STOP]));
# endif

                /* Disable or enable dirty page tracking according to the current fTraces value. */
                vmsvgaR3SetTraces(pDevIns, pThis, !!pThis->svga.fTraces);

                /* bird: Whatever this is was added to make screenshot work, ask sunlover should explain... */
                for (uint32_t idScreen = 0; idScreen < pThis->cMonitors; ++idScreen)
                    pThisCC->pDrv->pfnVBVAEnable(pThisCC->pDrv, idScreen, NULL /*pHostFlags*/);

                /* Make the cursor visible again as needed. */
                if (pSVGAState->Cursor.fActive)
                    pThisCC->pDrv->pfnVBVAMousePointerShape(pThisCC->pDrv, true /*fVisible*/, false, 0, 0, 0, 0, NULL);
            }
            else
            {
                /* Make sure the cursor is off. */
                if (pSVGAState->Cursor.fActive)
                    pThisCC->pDrv->pfnVBVAMousePointerShape(pThisCC->pDrv, false /*fVisible*/, false, 0, 0, 0, 0, NULL);

                /* Restore the text mode backup. */
                memcpy(pThisCC->pbVRam, pThisCC->svga.pbVgaFrameBufferR3, VMSVGA_VGA_FB_BACKUP_SIZE);

                pThisCC->pDrv->pfnLFBModeChange(pThisCC->pDrv, false);

                /* Enable dirty page tracking again when going into legacy mode. */
                vmsvgaR3SetTraces(pDevIns, pThis, true);

                /* bird: Whatever this is was added to make screenshot work, ask sunlover should explain... */
                for (uint32_t idScreen = 0; idScreen < pThis->cMonitors; ++idScreen)
                    pThisCC->pDrv->pfnVBVADisable(pThisCC->pDrv, idScreen);

                /* Clear the pitch lock. */
                pThis->svga.u32PitchLock = 0;
            }
#else  /* !IN_RING3 */
            rc = VINF_IOM_R3_IOPORT_WRITE;
#endif /* !IN_RING3 */
            break;

        case SVGA_REG_WIDTH:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegWidthWr);
            if (pThis->svga.uWidth != u32)
            {
#if defined(IN_RING3) || defined(IN_RING0)
                pThis->svga.uWidth = u32;
                vmsvgaHCUpdatePitch(pThis, pThisCC);
                if (pThis->svga.fEnabled)
                    ASMAtomicOrU32(&pThis->svga.u32ActionFlags, VMSVGA_ACTION_CHANGEMODE);
#else
                rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
            }
            /* else: nop */
            break;

        case SVGA_REG_HEIGHT:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegHeightWr);
            if (pThis->svga.uHeight != u32)
            {
                pThis->svga.uHeight = u32;
                if (pThis->svga.fEnabled)
                    ASMAtomicOrU32(&pThis->svga.u32ActionFlags, VMSVGA_ACTION_CHANGEMODE);
            }
            /* else: nop */
            break;

        case SVGA_REG_DEPTH:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDepthWr);
            /** @todo read-only?? */
            break;

        case SVGA_REG_BITS_PER_PIXEL:      /* Current bpp in the guest */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegBitsPerPixelWr);
            if (pThis->svga.uBpp != u32)
            {
#if defined(IN_RING3) || defined(IN_RING0)
                pThis->svga.uBpp = u32;
                vmsvgaHCUpdatePitch(pThis, pThisCC);
                if (pThis->svga.fEnabled)
                    ASMAtomicOrU32(&pThis->svga.u32ActionFlags, VMSVGA_ACTION_CHANGEMODE);
#else
                rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
            }
            /* else: nop */
            break;

        case SVGA_REG_PSEUDOCOLOR:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegPseudoColorWr);
            break;

        case SVGA_REG_CONFIG_DONE:         /* Set when memory area configured */
#ifdef IN_RING3
            STAM_REL_COUNTER_INC(&pSVGAState->StatR3RegConfigDoneWr);
            pThis->svga.fConfigured = u32;
            /* Disabling the FIFO enables tracing (dirty page detection) by default. */
            if (!pThis->svga.fConfigured)
                pThis->svga.fTraces = true;
            vmsvgaR3SetTraces(pDevIns, pThis, !!pThis->svga.fTraces);
#else
            rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
            break;

        case SVGA_REG_SYNC:                /* See "FIFO Synchronization Registers" */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegSyncWr);
            if (    pThis->svga.fEnabled
                &&  pThis->svga.fConfigured)
            {
#if defined(IN_RING3) || defined(IN_RING0)
                Log(("SVGA_REG_SYNC: SVGA_FIFO_BUSY=%d\n", pThisCC->svga.pau32FIFO[SVGA_FIFO_BUSY]));
                /*
                 * The VMSVGA_BUSY_F_EMT_FORCE flag makes sure we will check if the FIFO is empty
                 * at least once; VMSVGA_BUSY_F_FIFO alone does not ensure that.
                 */
                ASMAtomicWriteU32(&pThis->svga.fBusy, VMSVGA_BUSY_F_EMT_FORCE | VMSVGA_BUSY_F_FIFO);
                if (VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_BUSY, pThisCC->svga.pau32FIFO[SVGA_FIFO_MIN]))
                    vmsvgaHCSafeFifoBusyRegUpdate(pThis, pThisCC, true);

                /* Kick the FIFO thread to start processing commands again. */
                PDMDevHlpSUPSemEventSignal(pDevIns, pThis->svga.hFIFORequestSem);
#else
                rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
            }
            /* else nothing to do. */
            else
                Log(("Sync ignored enabled=%d configured=%d\n", pThis->svga.fEnabled, pThis->svga.fConfigured));

            break;

        case SVGA_REG_BUSY:                /* See "FIFO Synchronization Registers" (read-only) */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegBusyWr);
            break;

        case SVGA_REG_GUEST_ID:            /* Set guest OS identifier */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegGuestIdWr);
            pThis->svga.u32GuestId = u32;
            break;

        case SVGA_REG_PITCHLOCK:           /* Fixed pitch for all modes */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegPitchLockWr);
            pThis->svga.u32PitchLock = u32;
            /* Should this also update the FIFO pitch lock? Unclear. */
            break;

        case SVGA_REG_IRQMASK:             /* Interrupt mask */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegIrqMaskWr);
            pThis->svga.u32IrqMask = u32;

            /* Irq pending after the above change? */
            if (pThis->svga.u32IrqStatus & u32)
            {
                Log(("SVGA_REG_IRQMASK: Trigger interrupt with status %x\n", pThis->svga.u32IrqStatus));
                PDMDevHlpPCISetIrqNoWait(pDevIns, 0, 1);
            }
            else
                PDMDevHlpPCISetIrqNoWait(pDevIns, 0, 0);
            break;

        /* Mouse cursor support */
        case SVGA_REG_CURSOR_ID:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegCursorIdWr);
            pThis->svga.uCursorID = u32;
            break;

        case SVGA_REG_CURSOR_X:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegCursorXWr);
            pThis->svga.uCursorX = u32;
            break;

        case SVGA_REG_CURSOR_Y:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegCursorYWr);
            pThis->svga.uCursorY = u32;
            break;

        case SVGA_REG_CURSOR_ON:
#ifdef IN_RING3
            /* The cursor is only updated when SVGA_REG_CURSOR_ON is written. */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegCursorOnWr);
            vmsvgaR3RegUpdateCursor(pThisCC, pThis, u32);
#else
            rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
            break;

        /* Legacy multi-monitor support */
        case SVGA_REG_NUM_GUEST_DISPLAYS:/* Number of guest displays in X/Y direction */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegNumGuestDisplaysWr);
            break;
        case SVGA_REG_DISPLAY_ID:        /* Display ID for the following display attributes */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayIdWr);
            break;
        case SVGA_REG_DISPLAY_IS_PRIMARY:/* Whether this is a primary display */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayIsPrimaryWr);
            break;
        case SVGA_REG_DISPLAY_POSITION_X:/* The display position x */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayPositionXWr);
            break;
        case SVGA_REG_DISPLAY_POSITION_Y:/* The display position y */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayPositionYWr);
            break;
        case SVGA_REG_DISPLAY_WIDTH:     /* The display's width */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayWidthWr);
            break;
        case SVGA_REG_DISPLAY_HEIGHT:    /* The display's height */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegDisplayHeightWr);
            break;
#ifdef VBOX_WITH_VMSVGA3D
        /* See "Guest memory regions" below. */
        case SVGA_REG_GMR_ID:
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegGmrIdWr);
            pThis->svga.u32CurrentGMRId = u32;
            break;

        case SVGA_REG_GMR_DESCRIPTOR:
# ifndef IN_RING3
            rc = VINF_IOM_R3_IOPORT_WRITE;
            break;
# else /* IN_RING3 */
        {
            STAM_REL_COUNTER_INC(&pSVGAState->StatR3RegGmrDescriptorWr);

            /* Validate current GMR id. */
            uint32_t idGMR = pThis->svga.u32CurrentGMRId;
            AssertBreak(idGMR < pThis->svga.cGMR);
            RT_UNTRUSTED_VALIDATED_FENCE();

            /* Free the old GMR if present. */
            vmsvgaR3GmrFree(pThisCC, idGMR);

            /* Just undefine the GMR? */
            RTGCPHYS GCPhys = (RTGCPHYS)u32 << PAGE_SHIFT;
            if (GCPhys == 0)
            {
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3RegGmrDescriptorWrFree);
                break;
            }


            /* Never cross a page boundary automatically. */
            const uint32_t          cMaxPages   = RT_MIN(VMSVGA_MAX_GMR_PAGES, UINT32_MAX / X86_PAGE_SIZE);
            uint32_t                cPagesTotal = 0;
            uint32_t                iDesc       = 0;
            PVMSVGAGMRDESCRIPTOR    paDescs     = NULL;
            uint32_t                cLoops      = 0;
            RTGCPHYS                GCPhysBase  = GCPhys;
            while (PHYS_PAGE_ADDRESS(GCPhys) == PHYS_PAGE_ADDRESS(GCPhysBase))
            {
                /* Read descriptor. */
                SVGAGuestMemDescriptor desc;
                rc = PDMDevHlpPhysRead(pDevIns, GCPhys, &desc, sizeof(desc));
                AssertRCBreak(VBOXSTRICTRC_VAL(rc));

                if (desc.numPages != 0)
                {
                    AssertBreakStmt(desc.numPages <= cMaxPages, rc = VERR_OUT_OF_RANGE);
                    cPagesTotal += desc.numPages;
                    AssertBreakStmt(cPagesTotal   <= cMaxPages, rc = VERR_OUT_OF_RANGE);

                    if ((iDesc & 15) == 0)
                    {
                        void *pvNew = RTMemRealloc(paDescs, (iDesc + 16) * sizeof(VMSVGAGMRDESCRIPTOR));
                        AssertBreakStmt(pvNew, rc = VERR_NO_MEMORY);
                        paDescs = (PVMSVGAGMRDESCRIPTOR)pvNew;
                    }

                    paDescs[iDesc].GCPhys     = (RTGCPHYS)desc.ppn << PAGE_SHIFT;
                    paDescs[iDesc++].numPages = desc.numPages;

                    /* Continue with the next descriptor. */
                    GCPhys += sizeof(desc);
                }
                else if (desc.ppn == 0)
                    break;  /* terminator */
                else /* Pointer to the next physical page of descriptors. */
                    GCPhys = GCPhysBase = (RTGCPHYS)desc.ppn << PAGE_SHIFT;

                cLoops++;
                AssertBreakStmt(cLoops < VMSVGA_MAX_GMR_DESC_LOOP_COUNT, rc = VERR_OUT_OF_RANGE);
            }

            AssertStmt(iDesc > 0 || RT_FAILURE_NP(rc), rc = VERR_OUT_OF_RANGE);
            if (RT_SUCCESS(rc))
            {
                /* Commit the GMR. */
                pSVGAState->paGMR[idGMR].paDesc         = paDescs;
                pSVGAState->paGMR[idGMR].numDescriptors = iDesc;
                pSVGAState->paGMR[idGMR].cMaxPages      = cPagesTotal;
                pSVGAState->paGMR[idGMR].cbTotal        = cPagesTotal * PAGE_SIZE;
                Assert((pSVGAState->paGMR[idGMR].cbTotal >> PAGE_SHIFT) == cPagesTotal);
                Log(("Defined new gmr %x numDescriptors=%d cbTotal=%x (%#x pages)\n",
                     idGMR, iDesc, pSVGAState->paGMR[idGMR].cbTotal, cPagesTotal));
            }
            else
            {
                RTMemFree(paDescs);
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3RegGmrDescriptorWrErrors);
            }
            break;
        }
# endif /* IN_RING3 */
#endif // VBOX_WITH_VMSVGA3D

        case SVGA_REG_TRACES:            /* Enable trace-based updates even when FIFO is on */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegTracesWr);
            if (pThis->svga.fTraces == u32)
                break; /* nothing to do */

#ifdef IN_RING3
            vmsvgaR3SetTraces(pDevIns, pThis, !!u32);
#else
            rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
            break;

        case SVGA_REG_TOP:               /* Must be 1 more than the last register */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegTopWr);
            break;

        case SVGA_REG_NUM_DISPLAYS:        /* (Deprecated) */
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegNumDisplaysWr);
            Log(("Write to deprecated register %x - val %x ignored\n", idxReg, u32));
            break;

        case SVGA_REG_FB_START:
        case SVGA_REG_MEM_START:
        case SVGA_REG_HOST_BITS_PER_PIXEL:
        case SVGA_REG_MAX_WIDTH:
        case SVGA_REG_MAX_HEIGHT:
        case SVGA_REG_VRAM_SIZE:
        case SVGA_REG_FB_SIZE:
        case SVGA_REG_CAPABILITIES:
        case SVGA_REG_MEM_SIZE:
        case SVGA_REG_SCRATCH_SIZE:        /* Number of scratch registers */
        case SVGA_REG_MEM_REGS:            /* Number of FIFO registers */
        case SVGA_REG_BYTES_PER_LINE:
        case SVGA_REG_FB_OFFSET:
        case SVGA_REG_RED_MASK:
        case SVGA_REG_GREEN_MASK:
        case SVGA_REG_BLUE_MASK:
        case SVGA_REG_GMRS_MAX_PAGES:    /* Maximum number of 4KB pages for all GMRs */
        case SVGA_REG_MEMORY_SIZE:       /* Total dedicated device memory excluding FIFO */
        case SVGA_REG_GMR_MAX_IDS:
        case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:
            /* Read only - ignore. */
            Log(("Write to R/O register %x - val %x ignored\n", idxReg, u32));
            STAM_REL_COUNTER_INC(&pThis->svga.StatRegReadOnlyWr);
            break;

        default:
        {
            uint32_t offReg;
            if ((offReg = idxReg - SVGA_SCRATCH_BASE) < pThis->svga.cScratchRegion)
            {
                RT_UNTRUSTED_VALIDATED_FENCE();
                pThis->svga.au32ScratchRegion[offReg] = u32;
                STAM_REL_COUNTER_INC(&pThis->svga.StatRegScratchWr);
            }
            else if ((offReg = idxReg - SVGA_PALETTE_BASE) < (uint32_t)SVGA_NUM_PALETTE_REGS)
            {
                /* Note! Using last_palette rather than palette here to preserve the VGA one.
                         Btw, see rgb_to_pixel32.  */
                STAM_REL_COUNTER_INC(&pThis->svga.StatRegPaletteWr);
                u32 &= 0xff;
                RT_UNTRUSTED_VALIDATED_FENCE();
                uint32_t uRgb = pThis->last_palette[offReg / 3];
                switch (offReg % 3)
                {
                    case 0: uRgb = (uRgb & UINT32_C(0x0000ffff)) | (u32 << 16); break; /* red */
                    case 1: uRgb = (uRgb & UINT32_C(0x00ff00ff)) | (u32 <<  8); break; /* green */
                    case 2: uRgb = (uRgb & UINT32_C(0x00ffff00)) |  u32       ; break; /* blue */
                }
                pThis->last_palette[offReg / 3] = uRgb;
            }
            else
            {
#if !defined(IN_RING3) && defined(VBOX_STRICT)
                rc = VINF_IOM_R3_IOPORT_WRITE;
#else
                STAM_REL_COUNTER_INC(&pThis->svga.StatRegUnknownWr);
                AssertMsgFailed(("reg=%#x u32=%#x\n", idxReg, u32));
#endif
            }
            break;
        }
    }
    return rc;
}

/**
 * @callback_method_impl{FNIOMIOPORTNEWIN}
 */
DECLCALLBACK(VBOXSTRICTRC) vmsvgaIORead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb)
{
    PVGASTATE   pThis = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    RT_NOREF_PV(pvUser);

    /* Only dword accesses. */
    if (cb == 4)
    {
        switch (offPort)
        {
            case SVGA_INDEX_PORT:
                *pu32 = pThis->svga.u32IndexReg;
                break;

            case SVGA_VALUE_PORT:
                return vmsvgaReadPort(pDevIns, pThis, pu32);

            case SVGA_BIOS_PORT:
                Log(("Ignoring BIOS port read\n"));
                *pu32 = 0;
                break;

            case SVGA_IRQSTATUS_PORT:
                LogFlow(("vmsvgaIORead: SVGA_IRQSTATUS_PORT %x\n", pThis->svga.u32IrqStatus));
                *pu32 = pThis->svga.u32IrqStatus;
                break;

            default:
                ASSERT_GUEST_MSG_FAILED(("vmsvgaIORead: Unknown register %u was read from.\n", offPort));
                *pu32 = UINT32_MAX;
                break;
        }
    }
    else
    {
        Log(("Ignoring non-dword I/O port read at %x cb=%d\n", offPort, cb));
        *pu32 = UINT32_MAX;
    }
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNIOMIOPORTNEWOUT}
 */
DECLCALLBACK(VBOXSTRICTRC) vmsvgaIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb)
{
    PVGASTATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATECC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
    RT_NOREF_PV(pvUser);

    /* Only dword accesses. */
    if (cb == 4)
        switch (offPort)
        {
            case SVGA_INDEX_PORT:
                pThis->svga.u32IndexReg = u32;
                break;

            case SVGA_VALUE_PORT:
                return vmsvgaWritePort(pDevIns, pThis, pThisCC, u32);

            case SVGA_BIOS_PORT:
                Log(("Ignoring BIOS port write (val=%x)\n", u32));
                break;

            case SVGA_IRQSTATUS_PORT:
                Log(("vmsvgaIOWrite SVGA_IRQSTATUS_PORT %x: status %x -> %x\n", u32, pThis->svga.u32IrqStatus, pThis->svga.u32IrqStatus & ~u32));
                ASMAtomicAndU32(&pThis->svga.u32IrqStatus, ~u32);
                /* Clear the irq in case all events have been cleared. */
                if (!(pThis->svga.u32IrqStatus & pThis->svga.u32IrqMask))
                {
                    Log(("vmsvgaIOWrite SVGA_IRQSTATUS_PORT: clearing IRQ\n"));
                    PDMDevHlpPCISetIrqNoWait(pDevIns, 0, 0);
                }
                break;

            default:
                ASSERT_GUEST_MSG_FAILED(("vmsvgaIOWrite: Unknown register %u was written to, value %#x LB %u.\n", offPort, u32, cb));
                break;
        }
    else
        Log(("Ignoring non-dword write at %x val=%x cb=%d\n", offPort, u32, cb));

    return VINF_SUCCESS;
}

#ifdef IN_RING3

# ifdef DEBUG_FIFO_ACCESS
/**
 * Handle FIFO memory access.
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pThis           The shared VGA/VMSVGA instance data.
 * @param   GCPhys          The access physical address.
 * @param   fWriteAccess    Read or write access
 */
static int vmsvgaR3DebugFifoAccess(PVM pVM, PVGASTATE pThis, RTGCPHYS GCPhys, bool fWriteAccess)
{
    RT_NOREF(pVM);
    RTGCPHYS GCPhysOffset = GCPhys - pThis->svga.GCPhysFIFO;
    uint32_t *pFIFO = pThisCC->svga.pau32FIFO;

    switch (GCPhysOffset >> 2)
    {
    case SVGA_FIFO_MIN:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_MIN = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_MAX:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_MAX = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_NEXT_CMD:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_NEXT_CMD = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_STOP:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_STOP = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_CAPABILITIES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_CAPABILITIES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_FLAGS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_FLAGS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_FENCE:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_FENCE = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_HWVERSION:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_HWVERSION = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_PITCHLOCK:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_PITCHLOCK = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_CURSOR_ON:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_CURSOR_ON = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_CURSOR_X:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_CURSOR_X = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_CURSOR_Y:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_CURSOR_Y = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_CURSOR_COUNT:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_CURSOR_COUNT = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_CURSOR_LAST_UPDATED:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_CURSOR_LAST_UPDATED = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_RESERVED:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_RESERVED = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_CURSOR_SCREEN_ID:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_CURSOR_SCREEN_ID = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_DEAD:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_DEAD = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_HWVERSION_REVISED:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_HWVERSION_REVISED = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_3D:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_3D = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_LIGHTS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_LIGHTS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_TEXTURES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_TEXTURES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_CLIP_PLANES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_CLIP_PLANES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_VERTEX_SHADER_VERSION:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_VERTEX_SHADER_VERSION = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_VERTEX_SHADER:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_VERTEX_SHADER = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_FRAGMENT_SHADER:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_FRAGMENT_SHADER = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_RENDER_TARGETS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_RENDER_TARGETS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_S23E8_TEXTURES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_S23E8_TEXTURES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_S10E5_TEXTURES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_S10E5_TEXTURES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_D16_BUFFER_FORMAT:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_D16_BUFFER_FORMAT = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_QUERY_TYPES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_QUERY_TYPES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_POINT_SIZE:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_POINT_SIZE = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_SHADER_TEXTURES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_SHADER_TEXTURES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_VOLUME_EXTENT:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_VOLUME_EXTENT = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_VERTEX_INDEX:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_VERTEX_INDEX = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_TEXTURE_OPS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_TEXTURE_OPS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_R5G6B5:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_R5G6B5 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_ALPHA8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_ALPHA8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_Z_D16:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_Z_D16 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_DXT1:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_DXT1 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_DXT2:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_DXT2 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_DXT3:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_DXT3 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_DXT4:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_DXT4 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_DXT5:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_DXT5 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_CxV8U8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_CxV8U8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_R_S10E5:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_R_S10E5 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_R_S23E8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_R_S23E8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_V16U16:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_V16U16 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_G16R16:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_G16R16 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_UYVY:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_UYVY = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_YUY2:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_YUY2 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MULTISAMPLE_NONMASKABLESAMPLES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MULTISAMPLE_NONMASKABLESAMPLES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MULTISAMPLE_MASKABLESAMPLES:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MULTISAMPLE_MASKABLESAMPLES = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_ALPHATOCOVERAGE:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_ALPHATOCOVERAGE = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SUPERSAMPLE:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SUPERSAMPLE = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_AUTOGENMIPMAPS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_AUTOGENMIPMAPS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_NV12:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_NV12 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_AYUV:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_AYUV = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_CONTEXT_IDS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_CONTEXT_IDS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_MAX_SURFACE_IDS:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_MAX_SURFACE_IDS = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_Z_DF16:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_Z_DF16 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_Z_DF24:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_Z_DF24 = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_BC4_UNORM:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_BC4_UNORM = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS + SVGA3D_DEVCAP_SURFACEFMT_BC5_UNORM:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS SVGA3D_DEVCAP_SURFACEFMT_BC5_UNORM = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_3D_CAPS_LAST:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_3D_CAPS_LAST = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_GUEST_3D_HWVERSION:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_GUEST_3D_HWVERSION = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_FENCE_GOAL:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_FENCE_GOAL = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    case SVGA_FIFO_BUSY:
        Log(("vmsvgaFIFOAccess [0x%x]: %s SVGA_FIFO_BUSY = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", pFIFO[GCPhysOffset >> 2]));
        break;
    default:
        Log(("vmsvgaFIFOAccess [0x%x]: %s access at offset %x = %x\n", GCPhysOffset >> 2, (fWriteAccess) ? "WRITE" : "READ", GCPhysOffset, pFIFO[GCPhysOffset >> 2]));
        break;
    }

    return VINF_EM_RAW_EMULATE_INSTR;
}
# endif /* DEBUG_FIFO_ACCESS */

# if defined(VMSVGA_USE_FIFO_ACCESS_HANDLER) || defined(DEBUG_FIFO_ACCESS)
/**
 * HC access handler for the FIFO.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   pVCpu           The cross context CPU structure for the calling EMT.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   enmOrigin       Who is making the access.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(VBOXSTRICTRC)
vmsvgaR3FifoAccessHandler(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf,
                          PGMACCESSTYPE enmAccessType, PGMACCESSORIGIN enmOrigin, void *pvUser)
{
    NOREF(pVCpu); NOREF(pvPhys); NOREF(pvBuf); NOREF(cbBuf); NOREF(enmOrigin); NOREF(enmAccessType); NOREF(GCPhys);
    PVGASTATE pThis = (PVGASTATE)pvUser;
    AssertPtr(pThis);

# ifdef VMSVGA_USE_FIFO_ACCESS_HANDLER
    /*
     * Wake up the FIFO thread as it might have work to do now.
     */
    int rc = PDMDevHlpSUPSemEventSignal(pDevIns, pThis->svga.hFIFORequestSem);
    AssertLogRelRC(rc);
# endif

# ifdef DEBUG_FIFO_ACCESS
    /*
     * When in debug-fifo-access mode, we do not disable the access handler,
     * but leave it on as we wish to catch all access.
     */
    Assert(GCPhys >= pThis->svga.GCPhysFIFO);
    rc = vmsvgaR3DebugFifoAccess(pVM, pThis, GCPhys, enmAccessType == PGMACCESSTYPE_WRITE);
# elif defined(VMSVGA_USE_FIFO_ACCESS_HANDLER)
    /*
     * Temporarily disable the access handler now that we've kicked the FIFO thread.
     */
    STAM_REL_COUNTER_INC(&pThisCC->svga.pSvgaR3State->StatFifoAccessHandler);
    rc = PGMHandlerPhysicalPageTempOff(pVM, pThis->svga.GCPhysFIFO, pThis->svga.GCPhysFIFO);
# endif
    if (RT_SUCCESS(rc))
        return VINF_PGM_HANDLER_DO_DEFAULT;
    AssertMsg(rc <= VINF_SUCCESS, ("rc=%Rrc\n", rc));
    return rc;
}
# endif /* VMSVGA_USE_FIFO_ACCESS_HANDLER || DEBUG_FIFO_ACCESS */

#endif /* IN_RING3 */

#ifdef DEBUG_GMR_ACCESS
# ifdef IN_RING3

/**
 * HC access handler for the FIFO.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   pVCpu           The cross context CPU structure for the calling EMT.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   enmOrigin       Who is making the access.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(VBOXSTRICTRC)
vmsvgaR3GmrAccessHandler(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf,
                         PGMACCESSTYPE enmAccessType, PGMACCESSORIGIN enmOrigin, void *pvUser)
{
    PVGASTATE   pThis = (PVGASTATE)pvUser;
    Assert(pThis);
    PVMSVGAR3STATE pSVGAState = pThisCC->svga.pSvgaR3State;
    NOREF(pVCpu); NOREF(pvPhys); NOREF(pvBuf); NOREF(cbBuf); NOREF(enmAccessType); NOREF(enmOrigin);

    Log(("vmsvgaR3GmrAccessHandler: GMR access to page %RGp\n", GCPhys));

    for (uint32_t i = 0; i < pThis->svga.cGMR; ++i)
    {
        PGMR pGMR = &pSVGAState->paGMR[i];

        if (pGMR->numDescriptors)
        {
            for (uint32_t j = 0; j < pGMR->numDescriptors; j++)
            {
                if (    GCPhys >= pGMR->paDesc[j].GCPhys
                    &&  GCPhys < pGMR->paDesc[j].GCPhys + pGMR->paDesc[j].numPages * PAGE_SIZE)
                {
                    /*
                     * Turn off the write handler for this particular page and make it R/W.
                     * Then return telling the caller to restart the guest instruction.
                     */
                    int rc = PGMHandlerPhysicalPageTempOff(pVM, pGMR->paDesc[j].GCPhys, GCPhys);
                    AssertRC(rc);
                    return VINF_PGM_HANDLER_DO_DEFAULT;
                }
            }
        }
    }

    return VINF_PGM_HANDLER_DO_DEFAULT;
}

/** Callback handler for VMR3ReqCallWaitU */
static DECLCALLBACK(int) vmsvgaR3RegisterGmr(PPDMDEVINS pDevIns, uint32_t gmrId)
{
    PVGASTATE       pThis = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;
    PGMR pGMR = &pSVGAState->paGMR[gmrId];
    int rc;

    for (uint32_t i = 0; i < pGMR->numDescriptors; i++)
    {
        rc = PGMHandlerPhysicalRegister(PDMDevHlpGetVM(pDevIns),
                                        pGMR->paDesc[i].GCPhys, pGMR->paDesc[i].GCPhys + pGMR->paDesc[i].numPages * PAGE_SIZE - 1,
                                        pThis->svga.hGmrAccessHandlerType, pThis, NIL_RTR0PTR, NIL_RTRCPTR, "VMSVGA GMR");
        AssertRC(rc);
    }
    return VINF_SUCCESS;
}

/** Callback handler for VMR3ReqCallWaitU */
static DECLCALLBACK(int) vmsvgaR3DeregisterGmr(PPDMDEVINS pDevIns, uint32_t gmrId)
{
    PVGASTATE       pThis = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;
    PGMR pGMR = &pSVGAState->paGMR[gmrId];

    for (uint32_t i = 0; i < pGMR->numDescriptors; i++)
    {
        int rc = PGMHandlerPhysicalDeregister(PDMDevHlpGetVM(pDevIns), pGMR->paDesc[i].GCPhys);
        AssertRC(rc);
    }
    return VINF_SUCCESS;
}

/** Callback handler for VMR3ReqCallWaitU */
static DECLCALLBACK(int) vmsvgaR3ResetGmrHandlers(PVGASTATE pThis)
{
    PVMSVGAR3STATE pSVGAState = pThisCC->svga.pSvgaR3State;

    for (uint32_t i = 0; i < pThis->svga.cGMR; ++i)
    {
        PGMR pGMR = &pSVGAState->paGMR[i];

        if (pGMR->numDescriptors)
        {
            for (uint32_t j = 0; j < pGMR->numDescriptors; j++)
            {
                int rc = PGMHandlerPhysicalReset(PDMDevHlpGetVM(pDevIns), pGMR->paDesc[j].GCPhys);
                AssertRC(rc);
            }
        }
    }
    return VINF_SUCCESS;
}

# endif /* IN_RING3 */
#endif /* DEBUG_GMR_ACCESS */

/* -=-=-=-=-=- Ring 3 -=-=-=-=-=- */

#ifdef IN_RING3


/**
 * Common worker for changing the pointer shape.
 *
 * @param   pThisCC             The VGA/VMSVGA state for ring-3.
 * @param   pSVGAState          The VMSVGA ring-3 instance data.
 * @param   fAlpha              Whether there is alpha or not.
 * @param   xHot                Hotspot x coordinate.
 * @param   yHot                Hotspot y coordinate.
 * @param   cx                  Width.
 * @param   cy                  Height.
 * @param   pbData              Heap copy of the cursor data.  Consumed.
 * @param   cbData              The size of the data.
 */
static void vmsvgaR3InstallNewCursor(PVGASTATECC pThisCC, PVMSVGAR3STATE pSVGAState, bool fAlpha,
                                     uint32_t xHot, uint32_t yHot, uint32_t cx, uint32_t cy, uint8_t *pbData, uint32_t cbData)
{
    LogRel2(("vmsvgaR3InstallNewCursor: cx=%d cy=%d xHot=%d yHot=%d fAlpha=%d cbData=%#x\n", cx, cy, xHot, yHot, fAlpha, cbData));
# ifdef LOG_ENABLED
    if (LogIs2Enabled())
    {
        uint32_t cbAndLine = RT_ALIGN(cx, 8) / 8;
        if (!fAlpha)
        {
            Log2(("VMSVGA Cursor AND mask (%d,%d):\n", cx, cy));
            for (uint32_t y = 0; y < cy; y++)
            {
                Log2(("%3u:", y));
                uint8_t const *pbLine = &pbData[y * cbAndLine];
                for (uint32_t x = 0; x < cx; x += 8)
                {
                    uint8_t   b = pbLine[x / 8];
                    char      szByte[12];
                    szByte[0] = b & 0x80 ? '*' : ' '; /* most significant bit first */
                    szByte[1] = b & 0x40 ? '*' : ' ';
                    szByte[2] = b & 0x20 ? '*' : ' ';
                    szByte[3] = b & 0x10 ? '*' : ' ';
                    szByte[4] = b & 0x08 ? '*' : ' ';
                    szByte[5] = b & 0x04 ? '*' : ' ';
                    szByte[6] = b & 0x02 ? '*' : ' ';
                    szByte[7] = b & 0x01 ? '*' : ' ';
                    szByte[8] = '\0';
                    Log2(("%s", szByte));
                }
                Log2(("\n"));
            }
        }

        Log2(("VMSVGA Cursor XOR mask (%d,%d):\n", cx, cy));
        uint32_t const *pu32Xor = (uint32_t const *)&pbData[RT_ALIGN_32(cbAndLine * cy, 4)];
        for (uint32_t y = 0; y < cy; y++)
        {
            Log2(("%3u:", y));
            uint32_t const *pu32Line = &pu32Xor[y * cx];
            for (uint32_t x = 0; x < cx; x++)
                Log2((" %08x", pu32Line[x]));
            Log2(("\n"));
        }
    }
# endif

    int rc = pThisCC->pDrv->pfnVBVAMousePointerShape(pThisCC->pDrv, true /*fVisible*/, fAlpha, xHot, yHot, cx, cy, pbData);
    AssertRC(rc);

    if (pSVGAState->Cursor.fActive)
        RTMemFreeZ(pSVGAState->Cursor.pData, pSVGAState->Cursor.cbData);

    pSVGAState->Cursor.fActive  = true;
    pSVGAState->Cursor.xHotspot = xHot;
    pSVGAState->Cursor.yHotspot = yHot;
    pSVGAState->Cursor.width    = cx;
    pSVGAState->Cursor.height   = cy;
    pSVGAState->Cursor.cbData   = cbData;
    pSVGAState->Cursor.pData    = pbData;
}


/**
 * Handles the SVGA_CMD_DEFINE_CURSOR command.
 *
 * @param   pThis               The shared VGA/VMSVGA state.
 * @param   pThisCC             The VGA/VMSVGA state for ring-3.
 * @param   pSVGAState          The VMSVGA ring-3 instance data.
 * @param   pCursor             The cursor.
 * @param   pbSrcAndMask        The AND mask.
 * @param   cbSrcAndLine        The scanline length of the AND mask.
 * @param   pbSrcXorMask        The XOR mask.
 * @param   cbSrcXorLine        The scanline length of the XOR mask.
 */
static void vmsvgaR3CmdDefineCursor(PVGASTATE pThis, PVGASTATECC pThisCC, PVMSVGAR3STATE pSVGAState,
                                    SVGAFifoCmdDefineCursor const *pCursor,
                                    uint8_t const *pbSrcAndMask, uint32_t cbSrcAndLine,
                                    uint8_t const *pbSrcXorMask, uint32_t cbSrcXorLine)
{
    uint32_t const cx = pCursor->width;
    uint32_t const cy = pCursor->height;

    /*
     * Convert the input to 1-bit AND mask and a 32-bit BRGA XOR mask.
     * The AND data uses 8-bit aligned scanlines.
     * The XOR data must be starting on a 32-bit boundrary.
     */
    uint32_t cbDstAndLine = RT_ALIGN_32(cx, 8) / 8;
    uint32_t cbDstAndMask = cbDstAndLine          * cy;
    uint32_t cbDstXorMask = cx * sizeof(uint32_t) * cy;
    uint32_t cbCopy = RT_ALIGN_32(cbDstAndMask, 4) + cbDstXorMask;

    uint8_t *pbCopy = (uint8_t *)RTMemAlloc(cbCopy);
    AssertReturnVoid(pbCopy);

    /* Convert the AND mask. */
    uint8_t       *pbDst     = pbCopy;
    uint8_t const *pbSrc     = pbSrcAndMask;
    switch (pCursor->andMaskDepth)
    {
        case 1:
            if (cbSrcAndLine == cbDstAndLine)
                memcpy(pbDst, pbSrc, cbSrcAndLine * cy);
            else
            {
                Assert(cbSrcAndLine > cbDstAndLine); /* lines are dword alined in source, but only byte in destination. */
                for (uint32_t y = 0; y < cy; y++)
                {
                    memcpy(pbDst, pbSrc, cbDstAndLine);
                    pbDst += cbDstAndLine;
                    pbSrc += cbSrcAndLine;
                }
            }
            break;
        /* Should take the XOR mask into account for the multi-bit AND mask. */
        case 8:
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; )
                {
                    uint8_t bDst = 0;
                    uint8_t fBit = 0x80;
                    do
                    {
                        uintptr_t const idxPal = pbSrc[x] * 3;
                        if (((   pThis->last_palette[idxPal]
                              | (pThis->last_palette[idxPal] >>  8)
                              | (pThis->last_palette[idxPal] >> 16)) & 0xff) > 0xfc)
                            bDst |= fBit;
                        fBit >>= 1;
                        x++;
                    } while (x < cx && (x & 7));
                    pbDst[(x - 1) / 8] = bDst;
                }
                pbDst += cbDstAndLine;
                pbSrc += cbSrcAndLine;
            }
            break;
        case 15:
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; )
                {
                    uint8_t bDst = 0;
                    uint8_t fBit = 0x80;
                    do
                    {
                        if ((pbSrc[x * 2] | (pbSrc[x * 2 + 1] & 0x7f)) >= 0xfc)
                            bDst |= fBit;
                        fBit >>= 1;
                        x++;
                    } while (x < cx && (x & 7));
                    pbDst[(x - 1) / 8] = bDst;
                }
                pbDst += cbDstAndLine;
                pbSrc += cbSrcAndLine;
            }
            break;
        case 16:
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; )
                {
                    uint8_t bDst = 0;
                    uint8_t fBit = 0x80;
                    do
                    {
                        if ((pbSrc[x * 2] | pbSrc[x * 2 + 1]) >= 0xfc)
                            bDst |= fBit;
                        fBit >>= 1;
                        x++;
                    } while (x < cx && (x & 7));
                    pbDst[(x - 1) / 8] = bDst;
                }
                pbDst += cbDstAndLine;
                pbSrc += cbSrcAndLine;
            }
            break;
        case 24:
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; )
                {
                    uint8_t bDst = 0;
                    uint8_t fBit = 0x80;
                    do
                    {
                        if ((pbSrc[x * 3] | pbSrc[x * 3 + 1] | pbSrc[x * 3 + 2]) >= 0xfc)
                            bDst |= fBit;
                        fBit >>= 1;
                        x++;
                    } while (x < cx && (x & 7));
                    pbDst[(x - 1) / 8] = bDst;
                }
                pbDst += cbDstAndLine;
                pbSrc += cbSrcAndLine;
            }
            break;
        case 32:
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; )
                {
                    uint8_t bDst = 0;
                    uint8_t fBit = 0x80;
                    do
                    {
                        if ((pbSrc[x * 4] | pbSrc[x * 4 + 1] | pbSrc[x * 4 + 2] | pbSrc[x * 4 + 3]) >= 0xfc)
                            bDst |= fBit;
                        fBit >>= 1;
                        x++;
                    } while (x < cx && (x & 7));
                    pbDst[(x - 1) / 8] = bDst;
                }
                pbDst += cbDstAndLine;
                pbSrc += cbSrcAndLine;
            }
            break;
        default:
            RTMemFreeZ(pbCopy, cbCopy);
            AssertFailedReturnVoid();
    }

    /* Convert the XOR mask. */
    uint32_t *pu32Dst = (uint32_t *)(pbCopy + RT_ALIGN_32(cbDstAndMask, 4));
    pbSrc  = pbSrcXorMask;
    switch (pCursor->xorMaskDepth)
    {
        case 1:
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; )
                {
                    /* most significant bit is the left most one. */
                    uint8_t bSrc = pbSrc[x / 8];
                    do
                    {
                        *pu32Dst++ = bSrc & 0x80 ? UINT32_C(0x00ffffff) : 0;
                        bSrc <<= 1;
                        x++;
                    } while ((x & 7) && x < cx);
                }
                pbSrc += cbSrcXorLine;
            }
            break;
        case 8:
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; x++)
                {
                    uint32_t u = pThis->last_palette[pbSrc[x]];
                    *pu32Dst++ = u;//RT_MAKE_U32_FROM_U8(RT_BYTE1(u), RT_BYTE2(u), RT_BYTE3(u), 0);
                }
                pbSrc += cbSrcXorLine;
            }
            break;
        case 15: /* Src: RGB-5-5-5 */
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; x++)
                {
                    uint32_t const uValue = RT_MAKE_U16(pbSrc[x * 2], pbSrc[x * 2 + 1]);
                    *pu32Dst++ = RT_MAKE_U32_FROM_U8(( uValue        & 0x1f) << 3,
                                                     ((uValue >>  5) & 0x1f) << 3,
                                                     ((uValue >> 10) & 0x1f) << 3, 0);
                }
                pbSrc += cbSrcXorLine;
            }
            break;
        case 16: /* Src: RGB-5-6-5 */
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; x++)
                {
                    uint32_t const uValue = RT_MAKE_U16(pbSrc[x * 2], pbSrc[x * 2 + 1]);
                    *pu32Dst++ = RT_MAKE_U32_FROM_U8(( uValue        & 0x1f) << 3,
                                                     ((uValue >>  5) & 0x3f) << 2,
                                                     ((uValue >> 11) & 0x1f) << 3, 0);
                }
                pbSrc += cbSrcXorLine;
            }
            break;
        case 24:
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; x++)
                    *pu32Dst++ = RT_MAKE_U32_FROM_U8(pbSrc[x*3], pbSrc[x*3 + 1], pbSrc[x*3 + 2], 0);
                pbSrc += cbSrcXorLine;
            }
            break;
        case 32:
            for (uint32_t y = 0; y < cy; y++)
            {
                for (uint32_t x = 0; x < cx; x++)
                    *pu32Dst++ = RT_MAKE_U32_FROM_U8(pbSrc[x*4], pbSrc[x*4 + 1], pbSrc[x*4 + 2], 0);
                pbSrc += cbSrcXorLine;
            }
            break;
        default:
            RTMemFreeZ(pbCopy, cbCopy);
            AssertFailedReturnVoid();
    }

    /*
     * Pass it to the frontend/whatever.
     */
    vmsvgaR3InstallNewCursor(pThisCC, pSVGAState, false /*fAlpha*/, pCursor->hotspotX, pCursor->hotspotY, cx, cy, pbCopy, cbCopy);
}


/**
 * Worker for vmsvgaR3FifoThread that handles an external command.
 *
 * @param   pDevIns         The device instance.
 * @param   pThis           The shared VGA/VMSVGA instance data.
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 */
static void vmsvgaR3FifoHandleExtCmd(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC)
{
    uint8_t uExtCmd = pThis->svga.u8FIFOExtCommand;
    switch (pThis->svga.u8FIFOExtCommand)
    {
        case VMSVGA_FIFO_EXTCMD_RESET:
            Log(("vmsvgaR3FifoLoop: reset the fifo thread.\n"));
            Assert(pThisCC->svga.pvFIFOExtCmdParam == NULL);
# ifdef VBOX_WITH_VMSVGA3D
            if (pThis->svga.f3DEnabled)
            {
                /* The 3d subsystem must be reset from the fifo thread. */
                vmsvgaR3ResetScreens(pThisCC); /** @todo Also destroy screens on PowerOff. */
                vmsvga3dReset(pThisCC);
            }
# endif
            break;

        case VMSVGA_FIFO_EXTCMD_TERMINATE:
            Log(("vmsvgaR3FifoLoop: terminate the fifo thread.\n"));
            Assert(pThisCC->svga.pvFIFOExtCmdParam == NULL);
# ifdef VBOX_WITH_VMSVGA3D
            if (pThis->svga.f3DEnabled)
            {
                /* The 3d subsystem must be shut down from the fifo thread. */
                vmsvga3dTerminate(pThisCC);
            }
# endif
            break;

        case VMSVGA_FIFO_EXTCMD_SAVESTATE:
        {
            Log(("vmsvgaR3FifoLoop: VMSVGA_FIFO_EXTCMD_SAVESTATE.\n"));
            PSSMHANDLE pSSM = (PSSMHANDLE)pThisCC->svga.pvFIFOExtCmdParam;
            AssertLogRelMsgBreak(RT_VALID_PTR(pSSM), ("pSSM=%p\n", pSSM));
            vmsvgaR3SaveExecFifo(pDevIns->pHlpR3, pThisCC, pSSM);
# ifdef VBOX_WITH_VMSVGA3D
            if (pThis->svga.f3DEnabled)
                vmsvga3dSaveExec(pDevIns, pThisCC, pSSM);
# endif
            break;
        }

        case VMSVGA_FIFO_EXTCMD_LOADSTATE:
        {
            Log(("vmsvgaR3FifoLoop: VMSVGA_FIFO_EXTCMD_LOADSTATE.\n"));
            PVMSVGA_STATE_LOAD pLoadState = (PVMSVGA_STATE_LOAD)pThisCC->svga.pvFIFOExtCmdParam;
            AssertLogRelMsgBreak(RT_VALID_PTR(pLoadState), ("pLoadState=%p\n", pLoadState));
            vmsvgaR3LoadExecFifo(pDevIns->pHlpR3, pThis, pThisCC, pLoadState->pSSM, pLoadState->uVersion, pLoadState->uPass);
# ifdef VBOX_WITH_VMSVGA3D
            if (pThis->svga.f3DEnabled)
                vmsvga3dLoadExec(pDevIns, pThis, pThisCC, pLoadState->pSSM, pLoadState->uVersion, pLoadState->uPass);
# endif
            break;
        }

        case VMSVGA_FIFO_EXTCMD_UPDATE_SURFACE_HEAP_BUFFERS:
        {
# ifdef VBOX_WITH_VMSVGA3D
            uint32_t sid = (uint32_t)(uintptr_t)pThisCC->svga.pvFIFOExtCmdParam;
            Log(("vmsvgaR3FifoLoop: VMSVGA_FIFO_EXTCMD_UPDATE_SURFACE_HEAP_BUFFERS sid=%#x\n", sid));
            vmsvga3dUpdateHeapBuffersForSurfaces(pThisCC, sid);
# endif
            break;
        }


        default:
            AssertLogRelMsgFailed(("uExtCmd=%#x pvFIFOExtCmdParam=%p\n", uExtCmd, pThisCC->svga.pvFIFOExtCmdParam));
            break;
    }

    /*
     * Signal the end of the external command.
     */
    pThisCC->svga.pvFIFOExtCmdParam = NULL;
    pThis->svga.u8FIFOExtCommand  = VMSVGA_FIFO_EXTCMD_NONE;
    ASMMemoryFence(); /* paranoia^2 */
    int rc = RTSemEventSignal(pThisCC->svga.hFIFOExtCmdSem);
    AssertLogRelRC(rc);
}

/**
 * Worker for vmsvgaR3Destruct, vmsvgaR3Reset, vmsvgaR3Save and vmsvgaR3Load for
 * doing a job on the FIFO thread (even when it's officially suspended).
 *
 * @returns VBox status code (fully asserted).
 * @param   pDevIns         The device instance.
 * @param   pThis           The shared VGA/VMSVGA instance data.
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 * @param   uExtCmd         The command to execute on the FIFO thread.
 * @param   pvParam         Pointer to command parameters.
 * @param   cMsWait         The time to wait for the command, given in
 *                          milliseconds.
 */
static int vmsvgaR3RunExtCmdOnFifoThread(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC,
                                         uint8_t uExtCmd, void *pvParam, RTMSINTERVAL cMsWait)
{
    Assert(cMsWait >= RT_MS_1SEC * 5);
    AssertLogRelMsg(pThis->svga.u8FIFOExtCommand == VMSVGA_FIFO_EXTCMD_NONE,
                    ("old=%d new=%d\n", pThis->svga.u8FIFOExtCommand, uExtCmd));

    int rc;
    PPDMTHREAD      pThread  = pThisCC->svga.pFIFOIOThread;
    PDMTHREADSTATE  enmState = pThread->enmState;
    if (enmState == PDMTHREADSTATE_SUSPENDED)
    {
        /*
         * The thread is suspended, we have to temporarily wake it up so it can
         * perform the task.
         * (We ASSUME not racing code here, both wrt thread state and ext commands.)
         */
        Log(("vmsvgaR3RunExtCmdOnFifoThread: uExtCmd=%d enmState=SUSPENDED\n", uExtCmd));
        /* Post the request. */
        pThis->svga.fFifoExtCommandWakeup = true;
        pThisCC->svga.pvFIFOExtCmdParam     = pvParam;
        pThis->svga.u8FIFOExtCommand      = uExtCmd;
        ASMMemoryFence(); /* paranoia^3 */

        /* Resume the thread. */
        rc = PDMDevHlpThreadResume(pDevIns, pThread);
        AssertLogRelRC(rc);
        if (RT_SUCCESS(rc))
        {
            /* Wait. Take care in case the semaphore was already posted (same as below). */
            rc = RTSemEventWait(pThisCC->svga.hFIFOExtCmdSem, cMsWait);
            if (   rc == VINF_SUCCESS
                && pThis->svga.u8FIFOExtCommand == uExtCmd)
                rc = RTSemEventWait(pThisCC->svga.hFIFOExtCmdSem, cMsWait);
            AssertLogRelMsg(pThis->svga.u8FIFOExtCommand != uExtCmd || RT_FAILURE_NP(rc),
                            ("%#x %Rrc\n", pThis->svga.u8FIFOExtCommand, rc));

            /* suspend the thread */
            pThis->svga.fFifoExtCommandWakeup = false;
            int rc2 = PDMDevHlpThreadSuspend(pDevIns, pThread);
            AssertLogRelRC(rc2);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
        }
        pThis->svga.fFifoExtCommandWakeup = false;
        pThisCC->svga.pvFIFOExtCmdParam     = NULL;
    }
    else if (enmState == PDMTHREADSTATE_RUNNING)
    {
        /*
         * The thread is running, should only happen during reset and vmsvga3dsfc.
         * We ASSUME not racing code here, both wrt thread state and ext commands.
         */
        Log(("vmsvgaR3RunExtCmdOnFifoThread: uExtCmd=%d enmState=RUNNING\n", uExtCmd));
        Assert(uExtCmd == VMSVGA_FIFO_EXTCMD_RESET || uExtCmd == VMSVGA_FIFO_EXTCMD_UPDATE_SURFACE_HEAP_BUFFERS);

        /* Post the request. */
        pThisCC->svga.pvFIFOExtCmdParam = pvParam;
        pThis->svga.u8FIFOExtCommand  = uExtCmd;
        ASMMemoryFence(); /* paranoia^2 */
        rc = PDMDevHlpSUPSemEventSignal(pDevIns, pThis->svga.hFIFORequestSem);
        AssertLogRelRC(rc);

        /* Wait. Take care in case the semaphore was already posted (same as above). */
        rc = RTSemEventWait(pThisCC->svga.hFIFOExtCmdSem, cMsWait);
        if (   rc == VINF_SUCCESS
            && pThis->svga.u8FIFOExtCommand == uExtCmd)
            rc = RTSemEventWait(pThisCC->svga.hFIFOExtCmdSem, cMsWait); /* it was already posted, retry the wait. */
        AssertLogRelMsg(pThis->svga.u8FIFOExtCommand != uExtCmd || RT_FAILURE_NP(rc),
                        ("%#x %Rrc\n", pThis->svga.u8FIFOExtCommand, rc));

        pThisCC->svga.pvFIFOExtCmdParam = NULL;
    }
    else
    {
        /*
         * Something is wrong with the thread!
         */
        AssertLogRelMsgFailed(("uExtCmd=%d enmState=%d\n", uExtCmd, enmState));
        rc = VERR_INVALID_STATE;
    }
    return rc;
}


/**
 * Marks the FIFO non-busy, notifying any waiting EMTs.
 *
 * @param   pDevIns         The device instance.
 * @param   pThis           The shared VGA/VMSVGA instance data.
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 * @param   pSVGAState      Pointer to the ring-3 only SVGA state data.
 * @param   offFifoMin      The start byte offset of the command FIFO.
 */
static void vmsvgaR3FifoSetNotBusy(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC, PVMSVGAR3STATE pSVGAState, uint32_t offFifoMin)
{
    ASMAtomicAndU32(&pThis->svga.fBusy, ~(VMSVGA_BUSY_F_FIFO | VMSVGA_BUSY_F_EMT_FORCE));
    if (VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_BUSY, offFifoMin))
        vmsvgaHCSafeFifoBusyRegUpdate(pThis, pThisCC, pThis->svga.fBusy != 0);

    /* Wake up any waiting EMTs. */
    if (pSVGAState->cBusyDelayedEmts > 0)
    {
# ifdef VMSVGA_USE_EMT_HALT_CODE
        PVM pVM = PDMDevHlpGetVM(pDevIns);
        VMCPUID idCpu = VMCpuSetFindLastPresentInternal(&pSVGAState->BusyDelayedEmts);
        if (idCpu != NIL_VMCPUID)
        {
            VMR3NotifyCpuDeviceReady(pVM, idCpu);
            while (idCpu-- > 0)
                if (VMCPUSET_IS_PRESENT(&pSVGAState->BusyDelayedEmts, idCpu))
                    VMR3NotifyCpuDeviceReady(pVM, idCpu);
        }
# else
        int rc2 = RTSemEventMultiSignal(pSVGAState->hBusyDelayedEmts);
        AssertRC(rc2);
# endif
    }
}

/**
 * Reads (more) payload into the command buffer.
 *
 * @returns pbBounceBuf on success
 * @retval  (void *)1 if the thread was requested to stop.
 * @retval  NULL on FIFO error.
 *
 * @param   cbPayloadReq    The number of bytes of payload requested.
 * @param   pFIFO           The FIFO.
 * @param   offCurrentCmd   The FIFO byte offset of the current command.
 * @param   offFifoMin      The start byte offset of the command FIFO.
 * @param   offFifoMax      The end byte offset of the command FIFO.
 * @param   pbBounceBuf     The bounch buffer. Same size as the entire FIFO, so
 *                          always sufficient size.
 * @param   pcbAlreadyRead  How much payload we've already read into the bounce
 *                          buffer. (We will NEVER re-read anything.)
 * @param   pThread         The calling PDM thread handle.
 * @param   pThis           The shared VGA/VMSVGA instance data.
 * @param   pSVGAState      Pointer to the ring-3 only SVGA state data. For
 *                          statistics collection.
 * @param   pDevIns         The device instance.
 */
static void *vmsvgaR3FifoGetCmdPayload(uint32_t cbPayloadReq, uint32_t RT_UNTRUSTED_VOLATILE_GUEST *pFIFO,
                                       uint32_t offCurrentCmd, uint32_t offFifoMin, uint32_t offFifoMax,
                                       uint8_t *pbBounceBuf, uint32_t *pcbAlreadyRead,
                                       PPDMTHREAD pThread, PVGASTATE pThis, PVMSVGAR3STATE pSVGAState, PPDMDEVINS pDevIns)
{
    Assert(pbBounceBuf);
    Assert(pcbAlreadyRead);
    Assert(offFifoMin < offFifoMax);
    Assert(offCurrentCmd >= offFifoMin && offCurrentCmd < offFifoMax);
    Assert(offFifoMax <= pThis->svga.cbFIFO);

    /*
     * Check if the requested payload size has already been satisfied                                                                                                .
     *                                                                                                                                                       .
     * When called to read more, the caller is responsible for making sure the                                                                               .
     * new command size (cbRequsted) never is smaller than what has already                                                                                  .
     * been read.
     */
    uint32_t cbAlreadyRead = *pcbAlreadyRead;
    if (cbPayloadReq <= cbAlreadyRead)
    {
        AssertLogRelReturn(cbPayloadReq == cbAlreadyRead, NULL);
        return pbBounceBuf;
    }

    /*
     * Commands bigger than the fifo buffer are invalid.
     */
    uint32_t const cbFifoCmd = offFifoMax - offFifoMin;
    AssertMsgReturnStmt(cbPayloadReq <= cbFifoCmd, ("cbPayloadReq=%#x cbFifoCmd=%#x\n", cbPayloadReq, cbFifoCmd),
                        STAM_REL_COUNTER_INC(&pSVGAState->StatFifoErrors),
                        NULL);

    /*
     * Move offCurrentCmd past the command dword.
     */
    offCurrentCmd += sizeof(uint32_t);
    if (offCurrentCmd >= offFifoMax)
        offCurrentCmd = offFifoMin;

    /*
     * Do we have sufficient payload data available already?
     * The host should not read beyond [SVGA_FIFO_NEXT_CMD], therefore '>=' in the condition below.
     */
    uint32_t cbAfter, cbBefore;
    uint32_t offNextCmd = pFIFO[SVGA_FIFO_NEXT_CMD];
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    if (offNextCmd >= offCurrentCmd)
    {
        if (RT_LIKELY(offNextCmd < offFifoMax))
            cbAfter = offNextCmd - offCurrentCmd;
        else
        {
            STAM_REL_COUNTER_INC(&pSVGAState->StatFifoErrors);
            LogRelMax(16, ("vmsvgaR3FifoGetCmdPayload: Invalid offNextCmd=%#x (offFifoMin=%#x offFifoMax=%#x)\n",
                           offNextCmd, offFifoMin, offFifoMax));
            cbAfter = offFifoMax - offCurrentCmd;
        }
        cbBefore = 0;
    }
    else
    {
        cbAfter  = offFifoMax - offCurrentCmd;
        if (offNextCmd >= offFifoMin)
            cbBefore = offNextCmd - offFifoMin;
        else
        {
            STAM_REL_COUNTER_INC(&pSVGAState->StatFifoErrors);
            LogRelMax(16, ("vmsvgaR3FifoGetCmdPayload: Invalid offNextCmd=%#x (offFifoMin=%#x offFifoMax=%#x)\n",
                           offNextCmd, offFifoMin, offFifoMax));
            cbBefore = 0;
        }
    }
    if (cbAfter + cbBefore < cbPayloadReq)
    {
        /*
         * Insufficient, must wait for it to arrive.
         */
/** @todo Should clear the busy flag here to maybe encourage the guest to wake us up. */
        STAM_REL_PROFILE_START(&pSVGAState->StatFifoStalls, Stall);
        for (uint32_t i = 0;; i++)
        {
            if (pThread->enmState != PDMTHREADSTATE_RUNNING)
            {
                STAM_REL_PROFILE_STOP(&pSVGAState->StatFifoStalls, Stall);
                return (void *)(uintptr_t)1;
            }
            Log(("Guest still copying (%x vs %x) current %x next %x stop %x loop %u; sleep a bit\n",
                 cbPayloadReq, cbAfter + cbBefore, offCurrentCmd, offNextCmd, pFIFO[SVGA_FIFO_STOP], i));

            PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pThis->svga.hFIFORequestSem, i < 16 ? 1 : 2);

            offNextCmd = pFIFO[SVGA_FIFO_NEXT_CMD];
            RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
            if (offNextCmd >= offCurrentCmd)
            {
                cbAfter = RT_MIN(offNextCmd, offFifoMax) - offCurrentCmd;
                cbBefore = 0;
            }
            else
            {
                cbAfter  = offFifoMax - offCurrentCmd;
                cbBefore = RT_MAX(offNextCmd, offFifoMin) - offFifoMin;
            }

            if (cbAfter + cbBefore >= cbPayloadReq)
                break;
        }
        STAM_REL_PROFILE_STOP(&pSVGAState->StatFifoStalls, Stall);
    }

    /*
     * Copy out the memory and update what pcbAlreadyRead points to.
     */
    if (cbAfter >= cbPayloadReq)
        memcpy(pbBounceBuf + cbAlreadyRead,
               (uint8_t *)pFIFO + offCurrentCmd + cbAlreadyRead,
               cbPayloadReq - cbAlreadyRead);
    else
    {
        LogFlow(("Split data buffer at %x (%u-%u)\n", offCurrentCmd, cbAfter, cbBefore));
        if (cbAlreadyRead < cbAfter)
        {
            memcpy(pbBounceBuf + cbAlreadyRead,
                   (uint8_t *)pFIFO + offCurrentCmd + cbAlreadyRead,
                   cbAfter - cbAlreadyRead);
            cbAlreadyRead = cbAfter;
        }
        memcpy(pbBounceBuf + cbAlreadyRead,
               (uint8_t *)pFIFO + offFifoMin + cbAlreadyRead - cbAfter,
               cbPayloadReq - cbAlreadyRead);
    }
    *pcbAlreadyRead = cbPayloadReq;
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    return pbBounceBuf;
}


/**
 * Sends cursor position and visibility information from the FIFO to the front-end.
 * @returns SVGA_FIFO_CURSOR_COUNT value used.
 */
static uint32_t
vmsvgaR3FifoUpdateCursor(PVGASTATECC pThisCC, PVMSVGAR3STATE  pSVGAState, uint32_t RT_UNTRUSTED_VOLATILE_GUEST *pFIFO,
                         uint32_t offFifoMin,  uint32_t uCursorUpdateCount,
                         uint32_t *pxLast, uint32_t *pyLast, uint32_t *pfLastVisible)
{
    /*
     * Check if the cursor update counter has changed and try get a stable
     * set of values if it has.  This is race-prone, especially consindering
     * the screen ID, but little we can do about that.
     */
    uint32_t x, y, fVisible, idScreen;
    for (uint32_t i = 0; ; i++)
    {
        x        = pFIFO[SVGA_FIFO_CURSOR_X];
        y        = pFIFO[SVGA_FIFO_CURSOR_Y];
        fVisible = pFIFO[SVGA_FIFO_CURSOR_ON];
        idScreen = VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_CURSOR_SCREEN_ID, offFifoMin)
                 ? pFIFO[SVGA_FIFO_CURSOR_SCREEN_ID] : SVGA_ID_INVALID;
        if (   uCursorUpdateCount == pFIFO[SVGA_FIFO_CURSOR_COUNT]
            || i > 3)
            break;
        if (i == 0)
            STAM_REL_COUNTER_INC(&pSVGAState->StatFifoCursorFetchAgain);
        ASMNopPause();
        uCursorUpdateCount = pFIFO[SVGA_FIFO_CURSOR_COUNT];
    }

    /*
     * Check if anything has changed, as calling into pDrv is not light-weight.
     */
    if (   *pxLast == x
        && *pyLast == y
        && (idScreen != SVGA_ID_INVALID || *pfLastVisible == fVisible))
        STAM_REL_COUNTER_INC(&pSVGAState->StatFifoCursorNoChange);
    else
    {
        /*
         * Detected changes.
         *
         * We handle global, not per-screen visibility information by sending
         * pfnVBVAMousePointerShape without shape data.
         */
        *pxLast = x;
        *pyLast = y;
        uint32_t fFlags = VBVA_CURSOR_VALID_DATA;
        if (idScreen != SVGA_ID_INVALID)
            fFlags |= VBVA_CURSOR_SCREEN_RELATIVE;
        else if (*pfLastVisible != fVisible)
        {
            LogRel2(("vmsvgaR3FifoUpdateCursor: fVisible %d fLastVisible %d (%d,%d)\n", fVisible, *pfLastVisible, x, y));
            *pfLastVisible = fVisible;
            pThisCC->pDrv->pfnVBVAMousePointerShape(pThisCC->pDrv, RT_BOOL(fVisible), false, 0, 0, 0, 0, NULL);
            STAM_REL_COUNTER_INC(&pSVGAState->StatFifoCursorVisiblity);
        }
        pThisCC->pDrv->pfnVBVAReportCursorPosition(pThisCC->pDrv, fFlags, idScreen, x, y);
        STAM_REL_COUNTER_INC(&pSVGAState->StatFifoCursorPosition);
    }

    /*
     * Update done.  Signal this to the guest.
     */
    pFIFO[SVGA_FIFO_CURSOR_LAST_UPDATED] = uCursorUpdateCount;

    return uCursorUpdateCount;
}


/**
 * Checks if there is work to be done, either cursor updating or FIFO commands.
 *
 * @returns true if pending work, false if not.
 * @param   pFIFO               The FIFO to examine.
 * @param   uLastCursorCount    The last cursor update counter value.
 */
DECLINLINE(bool) vmsvgaR3FifoHasWork(uint32_t RT_UNTRUSTED_VOLATILE_GUEST * const pFIFO, uint32_t uLastCursorCount)
{
    if (pFIFO[SVGA_FIFO_NEXT_CMD] != pFIFO[SVGA_FIFO_STOP])
        return true;

    if (   uLastCursorCount != pFIFO[SVGA_FIFO_CURSOR_COUNT]
        && VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_CURSOR_LAST_UPDATED, pFIFO[SVGA_FIFO_MIN]))
        return true;

    return false;
}


/**
 * Called by the VGA refresh timer to wake up the FIFO thread when needed.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared VGA/VMSVGA instance data.
 * @param   pThisCC     The VGA/VMSVGA state for ring-3.
 */
void vmsvgaR3FifoWatchdogTimer(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC)
{
    /* Caller already checked pThis->svga.fFIFOThreadSleeping, so we only have
       to recheck it before doing the signalling. */
    uint32_t RT_UNTRUSTED_VOLATILE_GUEST * const pFIFO = pThisCC->svga.pau32FIFO;
    AssertReturnVoid(pFIFO);
    if (   vmsvgaR3FifoHasWork(pFIFO, ASMAtomicReadU32(&pThis->svga.uLastCursorUpdateCount))
        && pThis->svga.fFIFOThreadSleeping)
    {
        int rc = PDMDevHlpSUPSemEventSignal(pDevIns, pThis->svga.hFIFORequestSem);
        AssertRC(rc);
        STAM_REL_COUNTER_INC(&pThisCC->svga.pSvgaR3State->StatFifoWatchdogWakeUps);
    }
}


/**
 * Called by the FIFO thread to process pending actions.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared VGA/VMSVGA instance data.
 * @param   pThisCC     The VGA/VMSVGA state for ring-3.
 */
void vmsvgaR3FifoPendingActions(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC)
{
    RT_NOREF(pDevIns);

    /* Currently just mode changes. */
    if (ASMBitTestAndClear(&pThis->svga.u32ActionFlags, VMSVGA_ACTION_CHANGEMODE_BIT))
    {
        vmsvgaR3ChangeMode(pThis, pThisCC);
# ifdef VBOX_WITH_VMSVGA3D
        if (pThisCC->svga.p3dState != NULL)
            vmsvga3dChangeMode(pThisCC);
# endif
    }
}


/*
 * These two macros are put outside vmsvgaR3FifoLoop because doxygen gets confused,
 * even the latest version, and thinks we're documenting vmsvgaR3FifoLoop. Sigh.
 */
/** @def VMSVGAFIFO_GET_CMD_BUFFER_BREAK
 * Macro for shortening calls to vmsvgaR3FifoGetCmdPayload.
 *
 * Will break out of the switch on failure.
 * Will restart and quit the loop if the thread was requested to stop.
 *
 * @param   a_PtrVar        Request variable pointer.
 * @param   a_Type          Request typedef (not pointer) for casting.
 * @param   a_cbPayloadReq  How much payload to fetch.
 * @remarks Accesses a bunch of variables in the current scope!
 */
# define VMSVGAFIFO_GET_CMD_BUFFER_BREAK(a_PtrVar, a_Type, a_cbPayloadReq) \
            if (1) { \
                (a_PtrVar) = (a_Type *)vmsvgaR3FifoGetCmdPayload((a_cbPayloadReq), pFIFO, offCurrentCmd, offFifoMin, offFifoMax, \
                                                                 pbBounceBuf, &cbPayload, pThread, pThis, pSVGAState, pDevIns); \
                if (RT_UNLIKELY((uintptr_t)(a_PtrVar) < 2)) { if ((uintptr_t)(a_PtrVar) == 1) continue; break; } \
                RT_UNTRUSTED_NONVOLATILE_COPY_FENCE(); \
            } else do {} while (0)
/* @def VMSVGAFIFO_GET_MORE_CMD_BUFFER_BREAK
 * Macro for shortening calls to vmsvgaR3FifoGetCmdPayload for refetching the
 * buffer after figuring out the actual command size.
 *
 * Will break out of the switch on failure.
 *
 * @param   a_PtrVar        Request variable pointer.
 * @param   a_Type          Request typedef (not pointer) for casting.
 * @param   a_cbPayloadReq  How much payload to fetch.
 * @remarks Accesses a bunch of variables in the current scope!
 */
# define VMSVGAFIFO_GET_MORE_CMD_BUFFER_BREAK(a_PtrVar, a_Type, a_cbPayloadReq) \
            if (1) { \
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(a_PtrVar, a_Type, a_cbPayloadReq); \
            } else do {} while (0)

/**
 * @callback_method_impl{PFNPDMTHREADDEV, The async FIFO handling thread.}
 */
static DECLCALLBACK(int) vmsvgaR3FifoLoop(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PVGASTATE       pThis      = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATER3     pThisCC    = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;
    int             rc;

# ifdef RT_OS_LINUX
    XInitThreads();
# endif

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    /*
     * Special mode where we only execute an external command and the go back
     * to being suspended.  Currently, all ext cmds ends up here, with the reset
     * one also being eligble for runtime execution further down as well.
     */
    if (pThis->svga.fFifoExtCommandWakeup)
    {
        vmsvgaR3FifoHandleExtCmd(pDevIns, pThis, pThisCC);
        while (pThread->enmState == PDMTHREADSTATE_RUNNING)
            if (pThis->svga.u8FIFOExtCommand == VMSVGA_FIFO_EXTCMD_NONE)
                PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pThis->svga.hFIFORequestSem, RT_MS_1MIN);
            else
                vmsvgaR3FifoHandleExtCmd(pDevIns, pThis, pThisCC);
        return VINF_SUCCESS;
    }


    /*
     * Signal the semaphore to make sure we don't wait for 250ms after a
     * suspend & resume scenario (see vmsvgaR3FifoGetCmdPayload).
     */
    PDMDevHlpSUPSemEventSignal(pDevIns, pThis->svga.hFIFORequestSem);

    /*
     * Allocate a bounce buffer for command we get from the FIFO.
     * (All code must return via the end of the function to free this buffer.)
     */
    uint8_t *pbBounceBuf = (uint8_t *)RTMemAllocZ(pThis->svga.cbFIFO);
    AssertReturn(pbBounceBuf, VERR_NO_MEMORY);

    /*
     * Polling/sleep interval config.
     *
     * We wait for an a short interval if the guest has recently given us work
     * to do, but the interval increases the longer we're kept idle.  Once we've
     * reached the refresh timer interval, we'll switch to extended waits,
     * depending on it or the guest to kick us into action when needed.
     *
     * Should the refresh time go fishing, we'll just continue increasing the
     * sleep length till we reaches the 250 ms max after about 16 seconds.
     */
    RTMSINTERVAL const  cMsMinSleep           = 16;
    RTMSINTERVAL const  cMsIncSleep           = 2;
    RTMSINTERVAL const  cMsMaxSleep           = 250;
    RTMSINTERVAL const  cMsExtendedSleep      = 15 * RT_MS_1SEC; /* Regular paranoia dictates that this cannot be indefinite. */
    RTMSINTERVAL        cMsSleep              = cMsMaxSleep;

    /*
     * Cursor update state (SVGA_FIFO_CAP_CURSOR_BYPASS_3).
     *
     * Initialize with values that will detect an update from the guest.
     * Make sure that if the guest never updates the cursor position, then the device does not report it.
     * The guest has to change the value of uLastCursorUpdateCount, when the cursor position is actually updated.
     * xLastCursor, yLastCursor and fLastCursorVisible are set to report the first update.
     */
    uint32_t RT_UNTRUSTED_VOLATILE_GUEST * const pFIFO = pThisCC->svga.pau32FIFO;
    pThis->svga.uLastCursorUpdateCount = pFIFO[SVGA_FIFO_CURSOR_COUNT];
    uint32_t xLastCursor        = ~pFIFO[SVGA_FIFO_CURSOR_X];
    uint32_t yLastCursor        = ~pFIFO[SVGA_FIFO_CURSOR_Y];
    uint32_t fLastCursorVisible = ~pFIFO[SVGA_FIFO_CURSOR_ON];

    /*
     * The FIFO loop.
     */
    LogFlow(("vmsvgaR3FifoLoop: started loop\n"));
    bool fBadOrDisabledFifo = false;
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
# if defined(RT_OS_DARWIN) && defined(VBOX_WITH_VMSVGA3D)
        /*
         * Should service the run loop every so often.
         */
        if (pThis->svga.f3DEnabled)
            vmsvga3dCocoaServiceRunLoop();
# endif

        /* First check any pending actions. */
        vmsvgaR3FifoPendingActions(pDevIns, pThis, pThisCC);

        /*
         * Unless there's already work pending, go to sleep for a short while.
         * (See polling/sleep interval config above.)
         */
        if (   fBadOrDisabledFifo
            || !vmsvgaR3FifoHasWork(pFIFO, pThis->svga.uLastCursorUpdateCount))
        {
            ASMAtomicWriteBool(&pThis->svga.fFIFOThreadSleeping, true);
            Assert(pThis->cMilliesRefreshInterval > 0);
            if (cMsSleep < pThis->cMilliesRefreshInterval)
                rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pThis->svga.hFIFORequestSem, cMsSleep);
            else
            {
# ifdef VMSVGA_USE_FIFO_ACCESS_HANDLER
                int rc2 = PGMHandlerPhysicalReset(PDMDevHlpGetVM(pDevIns), pThis->svga.GCPhysFIFO);
                AssertRC(rc2); /* No break. Racing EMTs unmapping and remapping the region. */
# endif
                if (   !fBadOrDisabledFifo
                    && vmsvgaR3FifoHasWork(pFIFO, pThis->svga.uLastCursorUpdateCount))
                    rc = VINF_SUCCESS;
                else
                {
                    STAM_REL_PROFILE_START(&pSVGAState->StatFifoExtendedSleep, Acc);
                    rc = PDMDevHlpSUPSemEventWaitNoResume(pDevIns, pThis->svga.hFIFORequestSem, cMsExtendedSleep);
                    STAM_REL_PROFILE_STOP(&pSVGAState->StatFifoExtendedSleep, Acc);
                }
            }
            ASMAtomicWriteBool(&pThis->svga.fFIFOThreadSleeping, false);
            AssertBreak(RT_SUCCESS(rc) || rc == VERR_TIMEOUT || rc == VERR_INTERRUPTED);
            if (pThread->enmState != PDMTHREADSTATE_RUNNING)
            {
                LogFlow(("vmsvgaR3FifoLoop: thread state %x\n", pThread->enmState));
                break;
            }
        }
        else
            rc = VINF_SUCCESS;
        fBadOrDisabledFifo = false;
        if (rc == VERR_TIMEOUT)
        {
            if (!vmsvgaR3FifoHasWork(pFIFO, pThis->svga.uLastCursorUpdateCount))
            {
                cMsSleep = RT_MIN(cMsSleep + cMsIncSleep, cMsMaxSleep);
                continue;
            }
            STAM_REL_COUNTER_INC(&pSVGAState->StatFifoTodoTimeout);

            Log(("vmsvgaR3FifoLoop: timeout\n"));
        }
        else if (vmsvgaR3FifoHasWork(pFIFO, pThis->svga.uLastCursorUpdateCount))
            STAM_REL_COUNTER_INC(&pSVGAState->StatFifoTodoWoken);
        cMsSleep = cMsMinSleep;

        Log(("vmsvgaR3FifoLoop: enabled=%d configured=%d busy=%d\n", pThis->svga.fEnabled, pThis->svga.fConfigured, pFIFO[SVGA_FIFO_BUSY]));
        Log(("vmsvgaR3FifoLoop: min  %x max  %x\n", pFIFO[SVGA_FIFO_MIN], pFIFO[SVGA_FIFO_MAX]));
        Log(("vmsvgaR3FifoLoop: next %x stop %x\n", pFIFO[SVGA_FIFO_NEXT_CMD], pFIFO[SVGA_FIFO_STOP]));

        /*
         * Handle external commands (currently only reset).
         */
        if (pThis->svga.u8FIFOExtCommand != VMSVGA_FIFO_EXTCMD_NONE)
        {
            vmsvgaR3FifoHandleExtCmd(pDevIns, pThis, pThisCC);
            continue;
        }

        /*
         * The device must be enabled and configured.
         */
        if (   !pThis->svga.fEnabled
            || !pThis->svga.fConfigured)
        {
            vmsvgaR3FifoSetNotBusy(pDevIns, pThis, pThisCC, pSVGAState, pFIFO[SVGA_FIFO_MIN]);
            fBadOrDisabledFifo = true;
            cMsSleep           = cMsMaxSleep; /* cheat */
            continue;
        }

        /*
         * Get and check the min/max values.  We ASSUME that they will remain
         * unchanged while we process requests.  A further ASSUMPTION is that
         * the guest won't mess with SVGA_FIFO_NEXT_CMD while we're busy, so
         * we don't read it back while in the loop.
         */
        uint32_t const offFifoMin    = pFIFO[SVGA_FIFO_MIN];
        uint32_t const offFifoMax    = pFIFO[SVGA_FIFO_MAX];
        uint32_t       offCurrentCmd = pFIFO[SVGA_FIFO_STOP];
        RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
        if (RT_UNLIKELY(   !VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_STOP, offFifoMin)
                        || offFifoMax <= offFifoMin
                        || offFifoMax > pThis->svga.cbFIFO
                        || (offFifoMax & 3) != 0
                        || (offFifoMin & 3) != 0
                        || offCurrentCmd < offFifoMin
                        || offCurrentCmd > offFifoMax))
        {
            STAM_REL_COUNTER_INC(&pSVGAState->StatFifoErrors);
            LogRelMax(8, ("vmsvgaR3FifoLoop: Bad fifo: min=%#x stop=%#x max=%#x\n", offFifoMin, offCurrentCmd, offFifoMax));
            vmsvgaR3FifoSetNotBusy(pDevIns, pThis, pThisCC, pSVGAState, offFifoMin);
            fBadOrDisabledFifo = true;
            continue;
        }
        RT_UNTRUSTED_VALIDATED_FENCE();
        if (RT_UNLIKELY(offCurrentCmd & 3))
        {
            STAM_REL_COUNTER_INC(&pSVGAState->StatFifoErrors);
            LogRelMax(8, ("vmsvgaR3FifoLoop: Misaligned offCurrentCmd=%#x?\n", offCurrentCmd));
            offCurrentCmd &= ~UINT32_C(3);
        }

        /*
         * Update the cursor position before we start on the FIFO commands.
         */
        /** @todo do we need to check whether the guest disabled the SVGA_FIFO_CAP_CURSOR_BYPASS_3 capability here? */
        if (VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_CURSOR_LAST_UPDATED, offFifoMin))
        {
            uint32_t const uCursorUpdateCount = pFIFO[SVGA_FIFO_CURSOR_COUNT];
            if (uCursorUpdateCount == pThis->svga.uLastCursorUpdateCount)
            { /* halfways likely */ }
            else
            {
                uint32_t const uNewCount = vmsvgaR3FifoUpdateCursor(pThisCC, pSVGAState, pFIFO, offFifoMin, uCursorUpdateCount,
                                                                    &xLastCursor, &yLastCursor, &fLastCursorVisible);
                ASMAtomicWriteU32(&pThis->svga.uLastCursorUpdateCount, uNewCount);
            }
        }

        /*
         * Mark the FIFO as busy.
         */
        ASMAtomicWriteU32(&pThis->svga.fBusy, VMSVGA_BUSY_F_FIFO);  // Clears VMSVGA_BUSY_F_EMT_FORCE!
        if (VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_BUSY, offFifoMin))
            ASMAtomicWriteU32(&pFIFO[SVGA_FIFO_BUSY], true);

        /*
         * Execute all queued FIFO commands.
         * Quit if pending external command or changes in the thread state.
         */
        bool fDone = false;
        while (   !(fDone = (pFIFO[SVGA_FIFO_NEXT_CMD] == offCurrentCmd))
               && pThread->enmState == PDMTHREADSTATE_RUNNING)
        {
            uint32_t cbPayload = 0;
            uint32_t u32IrqStatus = 0;

            Assert(offCurrentCmd < offFifoMax && offCurrentCmd >= offFifoMin);

            /* First check any pending actions. */
            vmsvgaR3FifoPendingActions(pDevIns, pThis, pThisCC);

            /* Check for pending external commands (reset). */
            if (pThis->svga.u8FIFOExtCommand != VMSVGA_FIFO_EXTCMD_NONE)
                break;

            /*
             * Process the command.
             */
            SVGAFifoCmdId const enmCmdId = (SVGAFifoCmdId)pFIFO[offCurrentCmd / sizeof(uint32_t)];
            RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
            LogFlow(("vmsvgaR3FifoLoop: FIFO command (iCmd=0x%x) %s 0x%x\n",
                     offCurrentCmd / sizeof(uint32_t), vmsvgaR3FifoCmdToString(enmCmdId), enmCmdId));
            switch (enmCmdId)
            {
            case SVGA_CMD_INVALID_CMD:
                /* Nothing to do. */
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdInvalidCmd);
                break;

            case SVGA_CMD_FENCE:
            {
                SVGAFifoCmdFence *pCmdFence;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmdFence, SVGAFifoCmdFence, sizeof(*pCmdFence));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdFence);
                if (VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_FENCE, offFifoMin))
                {
                    Log(("vmsvgaR3FifoLoop: SVGA_CMD_FENCE %x\n", pCmdFence->fence));
                    pFIFO[SVGA_FIFO_FENCE] = pCmdFence->fence;

                    if (pThis->svga.u32IrqMask & SVGA_IRQFLAG_ANY_FENCE)
                    {
                        Log(("vmsvgaR3FifoLoop: any fence irq\n"));
                        u32IrqStatus |= SVGA_IRQFLAG_ANY_FENCE;
                    }
                    else
                    if (    VMSVGA_IS_VALID_FIFO_REG(SVGA_FIFO_FENCE_GOAL, offFifoMin)
                        &&  (pThis->svga.u32IrqMask & SVGA_IRQFLAG_FENCE_GOAL)
                        &&  pFIFO[SVGA_FIFO_FENCE_GOAL] == pCmdFence->fence)
                    {
                        Log(("vmsvgaR3FifoLoop: fence goal reached irq (fence=%x)\n", pCmdFence->fence));
                        u32IrqStatus |= SVGA_IRQFLAG_FENCE_GOAL;
                    }
                }
                else
                    Log(("SVGA_CMD_FENCE is bogus when offFifoMin is %#x!\n", offFifoMin));
                break;
            }
            case SVGA_CMD_UPDATE:
            case SVGA_CMD_UPDATE_VERBOSE:
            {
                SVGAFifoCmdUpdate *pUpdate;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pUpdate, SVGAFifoCmdUpdate, sizeof(*pUpdate));
                if (enmCmdId == SVGA_CMD_UPDATE)
                    STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdUpdate);
                else
                    STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdUpdateVerbose);
                Log(("vmsvgaR3FifoLoop: UPDATE (%d,%d)(%d,%d)\n", pUpdate->x, pUpdate->y, pUpdate->width, pUpdate->height));
                /** @todo Multiple screens? */
                VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, 0);
                if (!pScreen) /* Can happen if screen is not defined (aScreens[idScreen].fDefined == false) yet. */
                    break;
                vmsvgaR3UpdateScreen(pThisCC, pScreen, pUpdate->x, pUpdate->y, pUpdate->width, pUpdate->height);
                break;
            }

            case SVGA_CMD_DEFINE_CURSOR:
            {
                /* Followed by bitmap data. */
                SVGAFifoCmdDefineCursor *pCursor;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCursor, SVGAFifoCmdDefineCursor, sizeof(*pCursor));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdDefineCursor);

                Log(("vmsvgaR3FifoLoop: CURSOR id=%d size (%d,%d) hotspot (%d,%d) andMaskDepth=%d xorMaskDepth=%d\n",
                     pCursor->id, pCursor->width, pCursor->height, pCursor->hotspotX, pCursor->hotspotY,
                     pCursor->andMaskDepth, pCursor->xorMaskDepth));
                AssertBreak(pCursor->height < 2048 && pCursor->width < 2048);
                AssertBreak(pCursor->andMaskDepth <= 32);
                AssertBreak(pCursor->xorMaskDepth <= 32);
                RT_UNTRUSTED_VALIDATED_FENCE();

                uint32_t cbAndLine = RT_ALIGN_32(pCursor->width * (pCursor->andMaskDepth + (pCursor->andMaskDepth == 15)), 32) / 8;
                uint32_t cbAndMask = cbAndLine * pCursor->height;
                uint32_t cbXorLine = RT_ALIGN_32(pCursor->width * (pCursor->xorMaskDepth + (pCursor->xorMaskDepth == 15)), 32) / 8;
                uint32_t cbXorMask = cbXorLine * pCursor->height;
                VMSVGAFIFO_GET_MORE_CMD_BUFFER_BREAK(pCursor, SVGAFifoCmdDefineCursor, sizeof(*pCursor) + cbAndMask + cbXorMask);

                vmsvgaR3CmdDefineCursor(pThis, pThisCC, pSVGAState, pCursor, (uint8_t const *)(pCursor + 1), cbAndLine,
                                        (uint8_t const *)(pCursor + 1) + cbAndMask, cbXorLine);
                break;
            }

            case SVGA_CMD_DEFINE_ALPHA_CURSOR:
            {
                /* Followed by bitmap data. */
                uint32_t cbCursorShape, cbAndMask;
                uint8_t *pCursorCopy;
                uint32_t cbCmd;

                SVGAFifoCmdDefineAlphaCursor *pCursor;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCursor, SVGAFifoCmdDefineAlphaCursor, sizeof(*pCursor));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdDefineAlphaCursor);

                Log(("vmsvgaR3FifoLoop: ALPHA_CURSOR id=%d size (%d,%d) hotspot (%d,%d)\n", pCursor->id, pCursor->width, pCursor->height, pCursor->hotspotX, pCursor->hotspotY));

                /* Check against a reasonable upper limit to prevent integer overflows in the sanity checks below. */
                AssertBreak(pCursor->height < 2048 && pCursor->width < 2048);
                RT_UNTRUSTED_VALIDATED_FENCE();

                /* Refetch the bitmap data as well. */
                cbCmd = sizeof(SVGAFifoCmdDefineAlphaCursor) + pCursor->width * pCursor->height * sizeof(uint32_t) /* 32-bit BRGA format */;
                VMSVGAFIFO_GET_MORE_CMD_BUFFER_BREAK(pCursor, SVGAFifoCmdDefineAlphaCursor, cbCmd);
                /** @todo Would be more efficient to copy the data straight into pCursorCopy (memcpy below). */

                /* The mouse pointer interface always expects an AND mask followed by the color data (XOR mask). */
                cbAndMask     = (pCursor->width + 7) / 8 * pCursor->height;                         /* size of the AND mask */
                cbAndMask     = ((cbAndMask + 3) & ~3);                                             /* + gap for alignment */
                cbCursorShape = cbAndMask + pCursor->width * sizeof(uint32_t) * pCursor->height;    /* + size of the XOR mask (32-bit BRGA format) */

                pCursorCopy = (uint8_t *)RTMemAlloc(cbCursorShape);
                AssertPtrBreak(pCursorCopy);

                /* Transparency is defined by the alpha bytes, so make the whole bitmap visible. */
                memset(pCursorCopy, 0xff, cbAndMask);
                /* Colour data */
                memcpy(pCursorCopy + cbAndMask, (pCursor + 1), pCursor->width * pCursor->height * sizeof(uint32_t));

                vmsvgaR3InstallNewCursor(pThisCC, pSVGAState, true /*fAlpha*/, pCursor->hotspotX, pCursor->hotspotY,
                                         pCursor->width, pCursor->height, pCursorCopy, cbCursorShape);
                break;
            }

            case SVGA_CMD_MOVE_CURSOR:
            {
                /* Deprecated; there should be no driver which *requires* this command. However, if
                 * we do ecncounter this command, it might be useful to not get the FIFO completely out of
                 * alignment.
                 * May be issued by guest if SVGA_CAP_CURSOR_BYPASS is missing.
                 */
                SVGAFifoCmdMoveCursor *pMoveCursor;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pMoveCursor, SVGAFifoCmdMoveCursor, sizeof(*pMoveCursor));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdMoveCursor);

                Log(("vmsvgaR3FifoLoop: MOVE CURSOR to %d,%d\n", pMoveCursor->pos.x, pMoveCursor->pos.y));
                LogRelMax(4, ("Unsupported SVGA_CMD_MOVE_CURSOR command ignored.\n"));
                break;
            }

            case SVGA_CMD_DISPLAY_CURSOR:
            {
                /* Deprecated; there should be no driver which *requires* this command. However, if
                 * we do ecncounter this command, it might be useful to not get the FIFO completely out of
                 * alignment.
                 * May be issued by guest if SVGA_CAP_CURSOR_BYPASS is missing.
                 */
                SVGAFifoCmdDisplayCursor *pDisplayCursor;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pDisplayCursor, SVGAFifoCmdDisplayCursor, sizeof(*pDisplayCursor));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdDisplayCursor);

                Log(("vmsvgaR3FifoLoop: DISPLAY CURSOR id=%d state=%d\n", pDisplayCursor->id, pDisplayCursor->state));
                LogRelMax(4, ("Unsupported SVGA_CMD_DISPLAY_CURSOR command ignored.\n"));
                break;
            }

            case SVGA_CMD_RECT_FILL:
            {
                SVGAFifoCmdRectFill *pRectFill;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pRectFill, SVGAFifoCmdRectFill, sizeof(*pRectFill));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdRectFill);

                Log(("vmsvgaR3FifoLoop: RECT FILL %08X @ %d,%d (%dx%d)\n", pRectFill->pixel, pRectFill->destX, pRectFill->destY, pRectFill->width, pRectFill->height));
                LogRelMax(4, ("Unsupported SVGA_CMD_RECT_FILL command ignored.\n"));
                break;
            }

            case SVGA_CMD_RECT_COPY:
            {
                SVGAFifoCmdRectCopy *pRectCopy;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pRectCopy, SVGAFifoCmdRectCopy, sizeof(*pRectCopy));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdRectCopy);

                Log(("vmsvgaR3FifoLoop: RECT COPY %d,%d -> %d,%d (%dx%d)\n", pRectCopy->srcX, pRectCopy->srcY, pRectCopy->destX, pRectCopy->destY, pRectCopy->width, pRectCopy->height));
                VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, 0);
                AssertPtrBreak(pScreen);

                /* Check that arguments aren't complete junk. A precise check is done in vmsvgaR3RectCopy(). */
                AssertBreak(pRectCopy->srcX < pThis->svga.u32MaxWidth);
                AssertBreak(pRectCopy->destX < pThis->svga.u32MaxWidth);
                AssertBreak(pRectCopy->width < pThis->svga.u32MaxWidth);
                AssertBreak(pRectCopy->srcY < pThis->svga.u32MaxHeight);
                AssertBreak(pRectCopy->destY < pThis->svga.u32MaxHeight);
                AssertBreak(pRectCopy->height < pThis->svga.u32MaxHeight);

                vmsvgaR3RectCopy(pThisCC, pScreen, pRectCopy->srcX, pRectCopy->srcY, pRectCopy->destX, pRectCopy->destY,
                                 pRectCopy->width, pRectCopy->height, pThis->vram_size);
                vmsvgaR3UpdateScreen(pThisCC, pScreen, pRectCopy->destX, pRectCopy->destY, pRectCopy->width, pRectCopy->height);
                break;
            }

            case SVGA_CMD_RECT_ROP_COPY:
            {
                SVGAFifoCmdRectRopCopy *pRRCopy;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pRRCopy, SVGAFifoCmdRectRopCopy, sizeof(*pRRCopy));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdRectRopCopy);

                Log(("vmsvgaR3FifoLoop: RECT ROP COPY %d,%d -> %d,%d (%dx%d) ROP %X\n", pRRCopy->srcX, pRRCopy->srcY, pRRCopy->destX, pRRCopy->destY, pRRCopy->width, pRRCopy->height, pRRCopy->rop));
                if (pRRCopy->rop != SVGA_ROP_COPY)
                {
                    /* We only support the plain copy ROP which makes SVGA_CMD_RECT_ROP_COPY exactly the same
                     * as SVGA_CMD_RECT_COPY. XFree86 4.1.0 and 4.2.0 drivers (driver version 10.4.0 and 10.7.0,
                     * respectively) issue SVGA_CMD_RECT_ROP_COPY when SVGA_CAP_RECT_COPY is present even when
                     * SVGA_CAP_RASTER_OP is not. However, the ROP will always be SVGA_ROP_COPY.
                     */
                    LogRelMax(4, ("RECT ROP COPY %d,%d -> %d,%d (%dx%d) ROP %X unsupported\n", pRRCopy->srcX, pRRCopy->srcY, pRRCopy->destX, pRRCopy->destY, pRRCopy->width, pRRCopy->height, pRRCopy->rop));
                    break;
                }

                VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, 0);
                AssertPtrBreak(pScreen);

                /* Check that arguments aren't complete junk. A precise check is done in vmsvgaR3RectCopy(). */
                AssertBreak(pRRCopy->srcX < pThis->svga.u32MaxWidth);
                AssertBreak(pRRCopy->destX < pThis->svga.u32MaxWidth);
                AssertBreak(pRRCopy->width < pThis->svga.u32MaxWidth);
                AssertBreak(pRRCopy->srcY < pThis->svga.u32MaxHeight);
                AssertBreak(pRRCopy->destY < pThis->svga.u32MaxHeight);
                AssertBreak(pRRCopy->height < pThis->svga.u32MaxHeight);

                vmsvgaR3RectCopy(pThisCC, pScreen, pRRCopy->srcX, pRRCopy->srcY, pRRCopy->destX, pRRCopy->destY,
                                 pRRCopy->width, pRRCopy->height, pThis->vram_size);
                vmsvgaR3UpdateScreen(pThisCC, pScreen, pRRCopy->destX, pRRCopy->destY, pRRCopy->width, pRRCopy->height);
                break;
            }

            case SVGA_CMD_ESCAPE:
            {
                /* Followed by nsize bytes of data. */
                SVGAFifoCmdEscape *pEscape;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pEscape, SVGAFifoCmdEscape, sizeof(*pEscape));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdEscape);

                /* Refetch the command buffer with the variable data; undo size increase (ugly) */
                AssertBreak(pEscape->size < pThis->svga.cbFIFO);
                RT_UNTRUSTED_VALIDATED_FENCE();
                uint32_t cbCmd = sizeof(SVGAFifoCmdEscape) + pEscape->size;
                VMSVGAFIFO_GET_MORE_CMD_BUFFER_BREAK(pEscape, SVGAFifoCmdEscape, cbCmd);

                if (pEscape->nsid == SVGA_ESCAPE_NSID_VMWARE)
                {
                    AssertBreak(pEscape->size >= sizeof(uint32_t));
                    RT_UNTRUSTED_VALIDATED_FENCE();
                    uint32_t cmd = *(uint32_t *)(pEscape + 1);
                    Log(("vmsvgaR3FifoLoop: ESCAPE (%x %x) VMWARE cmd=%x\n", pEscape->nsid, pEscape->size, cmd));

                    switch (cmd)
                    {
                        case SVGA_ESCAPE_VMWARE_VIDEO_SET_REGS:
                        {
                            SVGAEscapeVideoSetRegs *pVideoCmd = (SVGAEscapeVideoSetRegs *)(pEscape + 1);
                            AssertBreak(pEscape->size >= sizeof(pVideoCmd->header));
                            uint32_t cRegs = (pEscape->size - sizeof(pVideoCmd->header)) / sizeof(pVideoCmd->items[0]);

                            Log(("SVGA_ESCAPE_VMWARE_VIDEO_SET_REGS: stream %x\n", pVideoCmd->header.streamId));
                            for (uint32_t iReg = 0; iReg < cRegs; iReg++)
                                Log(("SVGA_ESCAPE_VMWARE_VIDEO_SET_REGS: reg %x val %x\n", pVideoCmd->items[iReg].registerId, pVideoCmd->items[iReg].value));

                            RT_NOREF_PV(pVideoCmd);
                            break;

                        }

                        case SVGA_ESCAPE_VMWARE_VIDEO_FLUSH:
                        {
                            SVGAEscapeVideoFlush *pVideoCmd = (SVGAEscapeVideoFlush *)(pEscape + 1);
                            AssertBreak(pEscape->size >= sizeof(*pVideoCmd));
                            Log(("SVGA_ESCAPE_VMWARE_VIDEO_FLUSH: stream %x\n", pVideoCmd->streamId));
                            RT_NOREF_PV(pVideoCmd);
                            break;
                        }

                        default:
                            Log(("SVGA_CMD_ESCAPE: Unknown vmware escape: %x\n", cmd));
                            break;
                    }
                }
                else
                    Log(("vmsvgaR3FifoLoop: ESCAPE %x %x\n", pEscape->nsid, pEscape->size));

                break;
            }
# ifdef VBOX_WITH_VMSVGA3D
            case SVGA_CMD_DEFINE_GMR2:
            {
                SVGAFifoCmdDefineGMR2 *pCmd;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdDefineGMR2, sizeof(*pCmd));
                Log(("vmsvgaR3FifoLoop: SVGA_CMD_DEFINE_GMR2 id=%x %x pages\n", pCmd->gmrId, pCmd->numPages));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdDefineGmr2);

                /* Validate current GMR id. */
                AssertBreak(pCmd->gmrId < pThis->svga.cGMR);
                AssertBreak(pCmd->numPages <= VMSVGA_MAX_GMR_PAGES);
                RT_UNTRUSTED_VALIDATED_FENCE();

                if (!pCmd->numPages)
                {
                    STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdDefineGmr2Free);
                    vmsvgaR3GmrFree(pThisCC, pCmd->gmrId);
                }
                else
                {
                    PGMR pGMR = &pSVGAState->paGMR[pCmd->gmrId];
                    if (pGMR->cMaxPages)
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdDefineGmr2Modify);

                    /* Not sure if we should always free the descriptor, but for simplicity
                       we do so if the new size is smaller than the current. */
                    /** @todo always free the descriptor in SVGA_CMD_DEFINE_GMR2? */
                    if (pGMR->cbTotal / X86_PAGE_SIZE > pGMR->cMaxPages)
                        vmsvgaR3GmrFree(pThisCC, pCmd->gmrId);

                    pGMR->cMaxPages = pCmd->numPages;
                    /* The rest is done by the REMAP_GMR2 command. */
                }
                break;
            }

            case SVGA_CMD_REMAP_GMR2:
            {
                /* Followed by page descriptors or guest ptr. */
                SVGAFifoCmdRemapGMR2 *pCmd;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdRemapGMR2, sizeof(*pCmd));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdRemapGmr2);

                Log(("vmsvgaR3FifoLoop: SVGA_CMD_REMAP_GMR2 id=%x flags=%x offset=%x npages=%x\n", pCmd->gmrId, pCmd->flags, pCmd->offsetPages, pCmd->numPages));
                AssertBreak(pCmd->gmrId < pThis->svga.cGMR);
                RT_UNTRUSTED_VALIDATED_FENCE();

                /* Calculate the size of what comes after next and fetch it. */
                uint32_t cbCmd = sizeof(SVGAFifoCmdRemapGMR2);
                if (pCmd->flags & SVGA_REMAP_GMR2_VIA_GMR)
                    cbCmd += sizeof(SVGAGuestPtr);
                else
                {
                    uint32_t const cbPageDesc = (pCmd->flags & SVGA_REMAP_GMR2_PPN64) ? sizeof(uint64_t) : sizeof(uint32_t);
                    if (pCmd->flags & SVGA_REMAP_GMR2_SINGLE_PPN)
                    {
                        cbCmd         += cbPageDesc;
                        pCmd->numPages = 1;
                    }
                    else
                    {
                        AssertBreak(pCmd->numPages <= pThis->svga.cbFIFO / cbPageDesc);
                        cbCmd += cbPageDesc * pCmd->numPages;
                    }
                }
                VMSVGAFIFO_GET_MORE_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdRemapGMR2, cbCmd);

                /* Validate current GMR id and size. */
                AssertBreak(pCmd->gmrId < pThis->svga.cGMR);
                RT_UNTRUSTED_VALIDATED_FENCE();
                PGMR pGMR = &pSVGAState->paGMR[pCmd->gmrId];
                AssertBreak(   (uint64_t)pCmd->offsetPages + pCmd->numPages
                            <= RT_MIN(pGMR->cMaxPages, RT_MIN(VMSVGA_MAX_GMR_PAGES, UINT32_MAX / X86_PAGE_SIZE)));
                AssertBreak(!pCmd->offsetPages || pGMR->paDesc); /** @todo */

                if (pCmd->numPages == 0)
                    break;

                /** @todo Move to a separate function vmsvgaGMRRemap() */

                /* Calc new total page count so we can use it instead of cMaxPages for allocations below. */
                uint32_t const cNewTotalPages = RT_MAX(pGMR->cbTotal >> X86_PAGE_SHIFT, pCmd->offsetPages + pCmd->numPages);

                /*
                 * We flatten the existing descriptors into a page array, overwrite the
                 * pages specified in this command and then recompress the descriptor.
                 */
                /** @todo Optimize the GMR remap algorithm! */

                /* Save the old page descriptors as an array of page frame numbers (address >> X86_PAGE_SHIFT) */
                uint64_t *paNewPage64 = NULL;
                if (pGMR->paDesc)
                {
                    STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdRemapGmr2Modify);

                    paNewPage64 = (uint64_t *)RTMemAllocZ(cNewTotalPages * sizeof(uint64_t));
                    AssertPtrBreak(paNewPage64);

                    uint32_t idxPage = 0;
                    for (uint32_t i = 0; i < pGMR->numDescriptors; i++)
                        for (uint32_t j = 0; j < pGMR->paDesc[i].numPages; j++)
                            paNewPage64[idxPage++] = (pGMR->paDesc[i].GCPhys + j * X86_PAGE_SIZE) >> X86_PAGE_SHIFT;
                    AssertBreakStmt(idxPage == pGMR->cbTotal >> X86_PAGE_SHIFT, RTMemFree(paNewPage64));
                    RT_UNTRUSTED_VALIDATED_FENCE();
                }

                /* Free the old GMR if present. */
                if (pGMR->paDesc)
                    RTMemFree(pGMR->paDesc);

                /* Allocate the maximum amount possible (everything non-continuous) */
                PVMSVGAGMRDESCRIPTOR paDescs;
                pGMR->paDesc = paDescs = (PVMSVGAGMRDESCRIPTOR)RTMemAllocZ(cNewTotalPages * sizeof(VMSVGAGMRDESCRIPTOR));
                AssertBreakStmt(paDescs, RTMemFree(paNewPage64));

                if (pCmd->flags & SVGA_REMAP_GMR2_VIA_GMR)
                {
                    /** @todo */
                    AssertFailed();
                    pGMR->numDescriptors = 0;
                }
                else
                {
                    uint32_t  *paPages32 = (uint32_t *)(pCmd + 1);
                    uint64_t  *paPages64 = (uint64_t *)(pCmd + 1);
                    bool       fGCPhys64 = RT_BOOL(pCmd->flags & SVGA_REMAP_GMR2_PPN64);

                    if (paNewPage64)
                    {
                        /* Overwrite the old page array with the new page values. */
                        if (fGCPhys64)
                            for (uint32_t i = pCmd->offsetPages; i < pCmd->offsetPages + pCmd->numPages; i++)
                                paNewPage64[i] = paPages64[i - pCmd->offsetPages];
                        else
                            for (uint32_t i = pCmd->offsetPages; i < pCmd->offsetPages + pCmd->numPages; i++)
                                paNewPage64[i] = paPages32[i - pCmd->offsetPages];

                        /* Use the updated page array instead of the command data. */
                        fGCPhys64      = true;
                        paPages64      = paNewPage64;
                        pCmd->numPages = cNewTotalPages;
                    }

                    /* The first page. */
                    /** @todo The 0x00000FFFFFFFFFFF mask limits to 44 bits and should not be
                     *        applied to paNewPage64. */
                    RTGCPHYS GCPhys;
                    if (fGCPhys64)
                        GCPhys = (paPages64[0] << X86_PAGE_SHIFT) & UINT64_C(0x00000FFFFFFFFFFF); /* Seeing rubbish in the top bits with certain linux guests. */
                    else
                        GCPhys = (RTGCPHYS)paPages32[0] << PAGE_SHIFT;
                    paDescs[0].GCPhys    = GCPhys;
                    paDescs[0].numPages  = 1;

                    /* Subsequent pages. */
                    uint32_t iDescriptor = 0;
                    for (uint32_t i = 1; i < pCmd->numPages; i++)
                    {
                        if (pCmd->flags & SVGA_REMAP_GMR2_PPN64)
                            GCPhys = (paPages64[i] << X86_PAGE_SHIFT) & UINT64_C(0x00000FFFFFFFFFFF); /* Seeing rubbish in the top bits with certain linux guests. */
                        else
                            GCPhys = (RTGCPHYS)paPages32[i] << X86_PAGE_SHIFT;

                        /* Continuous physical memory? */
                        if (GCPhys == paDescs[iDescriptor].GCPhys + paDescs[iDescriptor].numPages * X86_PAGE_SIZE)
                        {
                            Assert(paDescs[iDescriptor].numPages);
                            paDescs[iDescriptor].numPages++;
                            Log5Func(("Page %x GCPhys=%RGp successor\n", i, GCPhys));
                        }
                        else
                        {
                            iDescriptor++;
                            paDescs[iDescriptor].GCPhys   = GCPhys;
                            paDescs[iDescriptor].numPages = 1;
                            Log5Func(("Page %x GCPhys=%RGp\n", i, paDescs[iDescriptor].GCPhys));
                        }
                    }

                    pGMR->cbTotal = cNewTotalPages << X86_PAGE_SHIFT;
                    Log5Func(("Nr of descriptors %x; cbTotal=%#x\n", iDescriptor + 1, cNewTotalPages));
                    pGMR->numDescriptors = iDescriptor + 1;
                }

                if (paNewPage64)
                    RTMemFree(paNewPage64);

#  ifdef DEBUG_GMR_ACCESS
                VMR3ReqCallWaitU(PDMDevHlpGetUVM(pDevIns), VMCPUID_ANY, (PFNRT)vmsvgaR3RegisterGmr, 2, pDevIns, pCmd->gmrId);
#  endif
                break;
            }
# endif // VBOX_WITH_VMSVGA3D
            case SVGA_CMD_DEFINE_SCREEN:
            {
                /* The size of this command is specified by the guest and depends on capabilities. */
                Assert(pFIFO[SVGA_FIFO_CAPABILITIES] & SVGA_FIFO_CAP_SCREEN_OBJECT_2);

                SVGAFifoCmdDefineScreen *pCmd;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdDefineScreen, sizeof(pCmd->screen.structSize));
                AssertBreak(pCmd->screen.structSize < pThis->svga.cbFIFO);
                RT_UNTRUSTED_VALIDATED_FENCE();

                RT_BZERO(&pCmd->screen.id, sizeof(*pCmd) - RT_OFFSETOF(SVGAFifoCmdDefineScreen, screen.id));
                VMSVGAFIFO_GET_MORE_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdDefineScreen, RT_MAX(sizeof(pCmd->screen.structSize), pCmd->screen.structSize));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdDefineScreen);

                LogFunc(("SVGA_CMD_DEFINE_SCREEN id=%x flags=%x size=(%d,%d) root=(%d,%d) %d:0x%x 0x%x\n",
                         pCmd->screen.id, pCmd->screen.flags, pCmd->screen.size.width, pCmd->screen.size.height, pCmd->screen.root.x, pCmd->screen.root.y,
                         pCmd->screen.backingStore.ptr.gmrId, pCmd->screen.backingStore.ptr.offset, pCmd->screen.backingStore.pitch));

                uint32_t const idScreen = pCmd->screen.id;
                AssertBreak(idScreen < RT_ELEMENTS(pThisCC->svga.pSvgaR3State->aScreens));

                uint32_t const uWidth = pCmd->screen.size.width;
                AssertBreak(uWidth <= pThis->svga.u32MaxWidth);

                uint32_t const uHeight = pCmd->screen.size.height;
                AssertBreak(uHeight <= pThis->svga.u32MaxHeight);

                uint32_t const cbWidth = uWidth * ((32 + 7) / 8); /** @todo 32? */
                uint32_t const cbPitch = pCmd->screen.backingStore.pitch ? pCmd->screen.backingStore.pitch : cbWidth;
                AssertBreak(cbWidth <= cbPitch);

                uint32_t const uScreenOffset = pCmd->screen.backingStore.ptr.offset;
                AssertBreak(uScreenOffset < pThis->vram_size);

                uint32_t const cbVram = pThis->vram_size - uScreenOffset;
                /* If we have a not zero pitch, then height can't exceed the available VRAM. */
                AssertBreak(   (uHeight == 0 && cbPitch == 0)
                            || (cbPitch > 0 && uHeight <= cbVram / cbPitch));
                RT_UNTRUSTED_VALIDATED_FENCE();

                VMSVGASCREENOBJECT *pScreen = &pThisCC->svga.pSvgaR3State->aScreens[idScreen];

                bool const fBlank = RT_BOOL(pCmd->screen.flags & (SVGA_SCREEN_DEACTIVATE | SVGA_SCREEN_BLANKING));

                pScreen->fDefined  = true;
                pScreen->fModified = true;
                pScreen->fuScreen  = pCmd->screen.flags;
                pScreen->idScreen  = idScreen;
                if (!fBlank)
                {
                    AssertBreak(uWidth > 0 && uHeight > 0);

                    pScreen->xOrigin = pCmd->screen.root.x;
                    pScreen->yOrigin = pCmd->screen.root.y;
                    pScreen->cWidth  = uWidth;
                    pScreen->cHeight = uHeight;
                    pScreen->offVRAM = uScreenOffset;
                    pScreen->cbPitch = cbPitch;
                    pScreen->cBpp    = 32;
                }
                else
                {
                    /* Keep old values. */
                }

                pThis->svga.fGFBRegisters = false;
                vmsvgaR3ChangeMode(pThis, pThisCC);

# ifdef VBOX_WITH_VMSVGA3D
                vmsvga3dDefineScreen(pThisCC, pScreen);
# endif
                break;
            }

            case SVGA_CMD_DESTROY_SCREEN:
            {
                SVGAFifoCmdDestroyScreen *pCmd;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdDestroyScreen, sizeof(*pCmd));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdDestroyScreen);

                Log(("vmsvgaR3FifoLoop: SVGA_CMD_DESTROY_SCREEN id=%x\n", pCmd->screenId));

                uint32_t const idScreen = pCmd->screenId;
                AssertBreak(idScreen < RT_ELEMENTS(pThisCC->svga.pSvgaR3State->aScreens));
                RT_UNTRUSTED_VALIDATED_FENCE();

                VMSVGASCREENOBJECT *pScreen = &pThisCC->svga.pSvgaR3State->aScreens[idScreen];
                pScreen->fModified = true;
                pScreen->fDefined  = false;
                pScreen->idScreen  = idScreen;

# ifdef VBOX_WITH_VMSVGA3D
                vmsvga3dDestroyScreen(pThisCC, pScreen);
# endif
                vmsvgaR3ChangeMode(pThis, pThisCC);
                break;
            }

            case SVGA_CMD_DEFINE_GMRFB:
            {
                SVGAFifoCmdDefineGMRFB *pCmd;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdDefineGMRFB, sizeof(*pCmd));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdDefineGmrFb);

                Log(("vmsvgaR3FifoLoop: SVGA_CMD_DEFINE_GMRFB gmr=%x offset=%x bytesPerLine=%x bpp=%d color depth=%d\n", pCmd->ptr.gmrId, pCmd->ptr.offset, pCmd->bytesPerLine, pCmd->format.s.bitsPerPixel, pCmd->format.s.colorDepth));
                pSVGAState->GMRFB.ptr          = pCmd->ptr;
                pSVGAState->GMRFB.bytesPerLine = pCmd->bytesPerLine;
                pSVGAState->GMRFB.format       = pCmd->format;
                break;
            }

            case SVGA_CMD_BLIT_GMRFB_TO_SCREEN:
            {
                SVGAFifoCmdBlitGMRFBToScreen *pCmd;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdBlitGMRFBToScreen, sizeof(*pCmd));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdBlitGmrFbToScreen);

                LogFunc(("SVGA_CMD_BLIT_GMRFB_TO_SCREEN src=(%d,%d) dest id=%d (%d,%d)(%d,%d)\n",
                         pCmd->srcOrigin.x, pCmd->srcOrigin.y, pCmd->destScreenId, pCmd->destRect.left, pCmd->destRect.top, pCmd->destRect.right, pCmd->destRect.bottom));

                AssertBreak(pCmd->destScreenId < RT_ELEMENTS(pThisCC->svga.pSvgaR3State->aScreens));
                RT_UNTRUSTED_VALIDATED_FENCE();

                VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, pCmd->destScreenId);
                AssertPtrBreak(pScreen);

                /** @todo Support GMRFB.format.s.bitsPerPixel != pThis->svga.uBpp   */
                AssertBreak(pSVGAState->GMRFB.format.s.bitsPerPixel == pScreen->cBpp);

                /* Clip destRect to the screen dimensions. */
                SVGASignedRect screenRect;
                screenRect.left   = 0;
                screenRect.top    = 0;
                screenRect.right  = pScreen->cWidth;
                screenRect.bottom = pScreen->cHeight;
                SVGASignedRect clipRect = pCmd->destRect;
                vmsvgaR3ClipRect(&screenRect, &clipRect);
                RT_UNTRUSTED_VALIDATED_FENCE();

                uint32_t const width  = clipRect.right - clipRect.left;
                uint32_t const height = clipRect.bottom - clipRect.top;

                if (   width == 0
                    || height == 0)
                    break;  /* Nothing to do. */

                int32_t const srcx = pCmd->srcOrigin.x + (clipRect.left - pCmd->destRect.left);
                int32_t const srcy = pCmd->srcOrigin.y + (clipRect.top - pCmd->destRect.top);

                /* Copy the defined by GMRFB image to the screen 0 VRAM area.
                 * Prepare parameters for vmsvgaR3GmrTransfer.
                 */
                AssertBreak(pScreen->offVRAM < pThis->vram_size); /* Paranoia. Ensured by SVGA_CMD_DEFINE_SCREEN. */

                /* Destination: host buffer which describes the screen 0 VRAM.
                 * Important are pbHstBuf and cbHstBuf. offHst and cbHstPitch are verified by vmsvgaR3GmrTransfer.
                 */
                uint8_t * const pbHstBuf = (uint8_t *)pThisCC->pbVRam + pScreen->offVRAM;
                uint32_t const cbScanline = pScreen->cbPitch ? pScreen->cbPitch :
                                                               width * (RT_ALIGN(pScreen->cBpp, 8) / 8);
                uint32_t cbHstBuf = cbScanline * pScreen->cHeight;
                if (cbHstBuf > pThis->vram_size - pScreen->offVRAM)
                   cbHstBuf = pThis->vram_size - pScreen->offVRAM; /* Paranoia. */
                uint32_t const offHst =   (clipRect.left * RT_ALIGN(pScreen->cBpp, 8)) / 8
                                        + cbScanline * clipRect.top;
                int32_t const cbHstPitch = cbScanline;

                /* Source: GMRFB. vmsvgaR3GmrTransfer ensures that no memory outside the GMR is read. */
                SVGAGuestPtr const gstPtr = pSVGAState->GMRFB.ptr;
                uint32_t const offGst =  (srcx * RT_ALIGN(pSVGAState->GMRFB.format.s.bitsPerPixel, 8)) / 8
                                       + pSVGAState->GMRFB.bytesPerLine * srcy;
                int32_t const cbGstPitch = pSVGAState->GMRFB.bytesPerLine;

                rc = vmsvgaR3GmrTransfer(pThis, pThisCC, SVGA3D_WRITE_HOST_VRAM,
                                         pbHstBuf, cbHstBuf, offHst, cbHstPitch,
                                         gstPtr, offGst, cbGstPitch,
                                         (width * RT_ALIGN(pScreen->cBpp, 8)) / 8, height);
                AssertRC(rc);
                vmsvgaR3UpdateScreen(pThisCC, pScreen, clipRect.left, clipRect.top, width, height);
                break;
            }

            case SVGA_CMD_BLIT_SCREEN_TO_GMRFB:
            {
                SVGAFifoCmdBlitScreenToGMRFB *pCmd;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdBlitScreenToGMRFB, sizeof(*pCmd));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdBlitScreentoGmrFb);

                /* Note! This can fetch 3d render results as well!! */
                LogFunc(("SVGA_CMD_BLIT_SCREEN_TO_GMRFB dest=(%d,%d) src id=%d (%d,%d)(%d,%d)\n",
                         pCmd->destOrigin.x, pCmd->destOrigin.y, pCmd->srcScreenId, pCmd->srcRect.left, pCmd->srcRect.top, pCmd->srcRect.right, pCmd->srcRect.bottom));

                AssertBreak(pCmd->srcScreenId < RT_ELEMENTS(pThisCC->svga.pSvgaR3State->aScreens));
                RT_UNTRUSTED_VALIDATED_FENCE();

                VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, pCmd->srcScreenId);
                AssertPtrBreak(pScreen);

                /** @todo Support GMRFB.format.s.bitsPerPixel != pThis->svga.uBpp?   */
                AssertBreak(pSVGAState->GMRFB.format.s.bitsPerPixel == pScreen->cBpp);

                /* Clip destRect to the screen dimensions. */
                SVGASignedRect screenRect;
                screenRect.left   = 0;
                screenRect.top    = 0;
                screenRect.right  = pScreen->cWidth;
                screenRect.bottom = pScreen->cHeight;
                SVGASignedRect clipRect = pCmd->srcRect;
                vmsvgaR3ClipRect(&screenRect, &clipRect);
                RT_UNTRUSTED_VALIDATED_FENCE();

                uint32_t const width  = clipRect.right - clipRect.left;
                uint32_t const height = clipRect.bottom - clipRect.top;

                if (   width == 0
                    || height == 0)
                    break;  /* Nothing to do. */

                int32_t const dstx = pCmd->destOrigin.x + (clipRect.left - pCmd->srcRect.left);
                int32_t const dsty = pCmd->destOrigin.y + (clipRect.top - pCmd->srcRect.top);

                /* Copy the defined by GMRFB image to the screen 0 VRAM area.
                 * Prepare parameters for vmsvgaR3GmrTransfer.
                 */
                AssertBreak(pScreen->offVRAM < pThis->vram_size); /* Paranoia. Ensured by SVGA_CMD_DEFINE_SCREEN. */

                /* Source: host buffer which describes the screen 0 VRAM.
                 * Important are pbHstBuf and cbHstBuf. offHst and cbHstPitch are verified by vmsvgaR3GmrTransfer.
                 */
                uint8_t * const pbHstBuf = (uint8_t *)pThisCC->pbVRam + pScreen->offVRAM;
                uint32_t const cbScanline = pScreen->cbPitch ? pScreen->cbPitch :
                                                               width * (RT_ALIGN(pScreen->cBpp, 8) / 8);
                uint32_t cbHstBuf = cbScanline * pScreen->cHeight;
                if (cbHstBuf > pThis->vram_size - pScreen->offVRAM)
                   cbHstBuf = pThis->vram_size - pScreen->offVRAM; /* Paranoia. */
                uint32_t const offHst =   (clipRect.left * RT_ALIGN(pScreen->cBpp, 8)) / 8
                                        + cbScanline * clipRect.top;
                int32_t const cbHstPitch = cbScanline;

                /* Destination: GMRFB. vmsvgaR3GmrTransfer ensures that no memory outside the GMR is read. */
                SVGAGuestPtr const gstPtr = pSVGAState->GMRFB.ptr;
                uint32_t const offGst =  (dstx * RT_ALIGN(pSVGAState->GMRFB.format.s.bitsPerPixel, 8)) / 8
                                       + pSVGAState->GMRFB.bytesPerLine * dsty;
                int32_t const cbGstPitch = pSVGAState->GMRFB.bytesPerLine;

                rc = vmsvgaR3GmrTransfer(pThis, pThisCC, SVGA3D_READ_HOST_VRAM,
                                         pbHstBuf, cbHstBuf, offHst, cbHstPitch,
                                         gstPtr, offGst, cbGstPitch,
                                         (width * RT_ALIGN(pScreen->cBpp, 8)) / 8, height);
                AssertRC(rc);
                break;
            }

            case SVGA_CMD_ANNOTATION_FILL:
            {
                SVGAFifoCmdAnnotationFill *pCmd;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdAnnotationFill, sizeof(*pCmd));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdAnnotationFill);

                Log(("vmsvgaR3FifoLoop: SVGA_CMD_ANNOTATION_FILL red=%x green=%x blue=%x\n", pCmd->color.s.r, pCmd->color.s.g, pCmd->color.s.b));
                pSVGAState->colorAnnotation = pCmd->color;
                break;
            }

            case SVGA_CMD_ANNOTATION_COPY:
            {
                SVGAFifoCmdAnnotationCopy *pCmd;
                VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pCmd, SVGAFifoCmdAnnotationCopy, sizeof(*pCmd));
                STAM_REL_COUNTER_INC(&pSVGAState->StatR3CmdAnnotationCopy);

                Log(("vmsvgaR3FifoLoop: SVGA_CMD_ANNOTATION_COPY\n"));
                AssertFailed();
                break;
            }

            default:
# ifdef VBOX_WITH_VMSVGA3D
                if (    (int)enmCmdId >= SVGA_3D_CMD_BASE
                    &&  (int)enmCmdId <  SVGA_3D_CMD_MAX)
                {
                    RT_UNTRUSTED_VALIDATED_FENCE();

                    /* All 3d commands start with a common header, which defines the size of the command. */
                    SVGA3dCmdHeader *pHdr;
                    VMSVGAFIFO_GET_CMD_BUFFER_BREAK(pHdr, SVGA3dCmdHeader, sizeof(*pHdr));
                    AssertBreak(pHdr->size < pThis->svga.cbFIFO);
                    uint32_t cbCmd = sizeof(SVGA3dCmdHeader) + pHdr->size;
                    VMSVGAFIFO_GET_MORE_CMD_BUFFER_BREAK(pHdr, SVGA3dCmdHeader, cbCmd);

                    if (RT_LIKELY(pThis->svga.f3DEnabled))
                    { /* likely */ }
                    else
                    {
                        LogRelMax(8, ("VMSVGA3d: 3D disabled, command %d skipped\n", enmCmdId));
                        break;
                    }

/** @def VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK
 * Check that the 3D command has at least a_cbMin of payload bytes after the
 * header.  Will break out of the switch if it doesn't.
 */
#  define VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(a_cbMin) \
     if (1) { \
          AssertMsgBreak(pHdr->size >= (a_cbMin), ("size=%#x a_cbMin=%#zx\n", pHdr->size, (size_t)(a_cbMin))); \
          RT_UNTRUSTED_VALIDATED_FENCE(); \
     } else do {} while (0)
                    switch ((int)enmCmdId)
                    {
                    case SVGA_3D_CMD_SURFACE_DEFINE:
                    {
                        uint32_t                cMipLevels;
                        SVGA3dCmdDefineSurface *pCmd = (SVGA3dCmdDefineSurface *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSurfaceDefine);

                        cMipLevels = (pHdr->size - sizeof(*pCmd)) / sizeof(SVGA3dSize);
                        rc = vmsvga3dSurfaceDefine(pThisCC, pCmd->sid, (uint32_t)pCmd->surfaceFlags, pCmd->format, pCmd->face, 0,
                                                   SVGA3D_TEX_FILTER_NONE, cMipLevels, (SVGA3dSize *)(pCmd + 1));
#  ifdef DEBUG_GMR_ACCESS
                        VMR3ReqCallWaitU(PDMDevHlpGetUVM(pDevIns), VMCPUID_ANY, (PFNRT)vmsvgaR3ResetGmrHandlers, 1, pThis);
#  endif
                        break;
                    }

                    case SVGA_3D_CMD_SURFACE_DEFINE_V2:
                    {
                        uint32_t                   cMipLevels;
                        SVGA3dCmdDefineSurface_v2 *pCmd = (SVGA3dCmdDefineSurface_v2 *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSurfaceDefineV2);

                        cMipLevels = (pHdr->size - sizeof(*pCmd)) / sizeof(SVGA3dSize);
                        rc = vmsvga3dSurfaceDefine(pThisCC, pCmd->sid, pCmd->surfaceFlags, pCmd->format, pCmd->face,
                                                   pCmd->multisampleCount, pCmd->autogenFilter,
                                                   cMipLevels, (SVGA3dSize *)(pCmd + 1));
                        break;
                    }

                    case SVGA_3D_CMD_SURFACE_DESTROY:
                    {
                        SVGA3dCmdDestroySurface *pCmd = (SVGA3dCmdDestroySurface *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSurfaceDestroy);
                        rc = vmsvga3dSurfaceDestroy(pThisCC, pCmd->sid);
                        break;
                    }

                    case SVGA_3D_CMD_SURFACE_COPY:
                    {
                        uint32_t              cCopyBoxes;
                        SVGA3dCmdSurfaceCopy *pCmd = (SVGA3dCmdSurfaceCopy *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSurfaceCopy);

                        cCopyBoxes = (pHdr->size - sizeof(pCmd)) / sizeof(SVGA3dCopyBox);
                        rc = vmsvga3dSurfaceCopy(pThisCC, pCmd->dest, pCmd->src, cCopyBoxes, (SVGA3dCopyBox *)(pCmd + 1));
                        break;
                    }

                    case SVGA_3D_CMD_SURFACE_STRETCHBLT:
                    {
                        SVGA3dCmdSurfaceStretchBlt *pCmd = (SVGA3dCmdSurfaceStretchBlt *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSurfaceStretchBlt);

                        rc = vmsvga3dSurfaceStretchBlt(pThis, pThisCC, &pCmd->dest, &pCmd->boxDest,
                                                       &pCmd->src, &pCmd->boxSrc, pCmd->mode);
                        break;
                    }

                    case SVGA_3D_CMD_SURFACE_DMA:
                    {
                        uint32_t             cCopyBoxes;
                        SVGA3dCmdSurfaceDMA *pCmd = (SVGA3dCmdSurfaceDMA *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSurfaceDma);

                        uint64_t u64NanoTS = 0;
                        if (LogRelIs3Enabled())
                            u64NanoTS = RTTimeNanoTS();
                        cCopyBoxes = (pHdr->size - sizeof(*pCmd)) / sizeof(SVGA3dCopyBox);
                        STAM_PROFILE_START(&pSVGAState->StatR3Cmd3dSurfaceDmaProf, a);
                        rc = vmsvga3dSurfaceDMA(pThis, pThisCC, pCmd->guest, pCmd->host, pCmd->transfer,
                                                cCopyBoxes, (SVGA3dCopyBox *)(pCmd + 1));
                        STAM_PROFILE_STOP(&pSVGAState->StatR3Cmd3dSurfaceDmaProf, a);
                        if (LogRelIs3Enabled())
                        {
                            if (cCopyBoxes)
                            {
                                SVGA3dCopyBox *pFirstBox = (SVGA3dCopyBox *)(pCmd + 1);
                                LogRel3(("VMSVGA: SURFACE_DMA: %d us %d boxes %d,%d %dx%d%s\n",
                                    (RTTimeNanoTS() - u64NanoTS) / 1000ULL, cCopyBoxes,
                                    pFirstBox->x, pFirstBox->y, pFirstBox->w, pFirstBox->h,
                                    pCmd->transfer == SVGA3D_READ_HOST_VRAM ? " readback!!!" : ""));
                            }
                        }
                        break;
                    }

                    case SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN:
                    {
                        uint32_t                      cRects;
                        SVGA3dCmdBlitSurfaceToScreen *pCmd = (SVGA3dCmdBlitSurfaceToScreen *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSurfaceScreen);

                        static uint64_t u64FrameStartNanoTS = 0;
                        static uint64_t u64ElapsedPerSecNano = 0;
                        static int cFrames = 0;
                        uint64_t u64NanoTS = 0;
                        if (LogRelIs3Enabled())
                            u64NanoTS = RTTimeNanoTS();
                        cRects = (pHdr->size - sizeof(*pCmd)) / sizeof(SVGASignedRect);
                        STAM_REL_PROFILE_START(&pSVGAState->StatR3Cmd3dBlitSurfaceToScreenProf, a);
                        rc = vmsvga3dSurfaceBlitToScreen(pThis, pThisCC, pCmd->destScreenId, pCmd->destRect, pCmd->srcImage,
                                                         pCmd->srcRect, cRects, (SVGASignedRect *)(pCmd + 1));
                        STAM_REL_PROFILE_STOP(&pSVGAState->StatR3Cmd3dBlitSurfaceToScreenProf, a);
                        if (LogRelIs3Enabled())
                        {
                            uint64_t u64ElapsedNano = RTTimeNanoTS() - u64NanoTS;
                            u64ElapsedPerSecNano += u64ElapsedNano;

                            SVGASignedRect *pFirstRect = cRects ? (SVGASignedRect *)(pCmd + 1) : &pCmd->destRect;
                            LogRel3(("VMSVGA: SURFACE_TO_SCREEN: %d us %d rects %d,%d %dx%d\n",
                                (u64ElapsedNano) / 1000ULL, cRects,
                                pFirstRect->left, pFirstRect->top,
                                pFirstRect->right - pFirstRect->left, pFirstRect->bottom - pFirstRect->top));

                            ++cFrames;
                            if (u64NanoTS - u64FrameStartNanoTS >= UINT64_C(1000000000))
                            {
                                LogRel3(("VMSVGA: SURFACE_TO_SCREEN: FPS %d, elapsed %llu us\n",
                                         cFrames, u64ElapsedPerSecNano / 1000ULL));
                                u64FrameStartNanoTS = u64NanoTS;
                                cFrames = 0;
                                u64ElapsedPerSecNano = 0;
                            }
                        }
                        break;
                    }

                    case SVGA_3D_CMD_CONTEXT_DEFINE:
                    {
                        SVGA3dCmdDefineContext *pCmd = (SVGA3dCmdDefineContext *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dContextDefine);

                        rc = vmsvga3dContextDefine(pThisCC, pCmd->cid);
                        break;
                    }

                    case SVGA_3D_CMD_CONTEXT_DESTROY:
                    {
                        SVGA3dCmdDestroyContext *pCmd = (SVGA3dCmdDestroyContext *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dContextDestroy);

                        rc = vmsvga3dContextDestroy(pThisCC, pCmd->cid);
                        break;
                    }

                    case SVGA_3D_CMD_SETTRANSFORM:
                    {
                        SVGA3dCmdSetTransform *pCmd = (SVGA3dCmdSetTransform *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetTransform);

                        rc = vmsvga3dSetTransform(pThisCC, pCmd->cid, pCmd->type, pCmd->matrix);
                        break;
                    }

                    case SVGA_3D_CMD_SETZRANGE:
                    {
                        SVGA3dCmdSetZRange *pCmd = (SVGA3dCmdSetZRange *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetZRange);

                        rc = vmsvga3dSetZRange(pThisCC, pCmd->cid, pCmd->zRange);
                        break;
                    }

                    case SVGA_3D_CMD_SETRENDERSTATE:
                    {
                        uint32_t                 cRenderStates;
                        SVGA3dCmdSetRenderState *pCmd = (SVGA3dCmdSetRenderState *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetRenderState);

                        cRenderStates = (pHdr->size - sizeof(*pCmd)) / sizeof(SVGA3dRenderState);
                        rc = vmsvga3dSetRenderState(pThisCC, pCmd->cid, cRenderStates, (SVGA3dRenderState *)(pCmd + 1));
                        break;
                    }

                    case SVGA_3D_CMD_SETRENDERTARGET:
                    {
                        SVGA3dCmdSetRenderTarget *pCmd = (SVGA3dCmdSetRenderTarget *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetRenderTarget);

                        rc = vmsvga3dSetRenderTarget(pThisCC, pCmd->cid, pCmd->type, pCmd->target);
                        break;
                    }

                    case SVGA_3D_CMD_SETTEXTURESTATE:
                    {
                        uint32_t                  cTextureStates;
                        SVGA3dCmdSetTextureState *pCmd = (SVGA3dCmdSetTextureState *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetTextureState);

                        cTextureStates = (pHdr->size - sizeof(*pCmd)) / sizeof(SVGA3dTextureState);
                        rc = vmsvga3dSetTextureState(pThisCC, pCmd->cid, cTextureStates, (SVGA3dTextureState *)(pCmd + 1));
                        break;
                    }

                    case SVGA_3D_CMD_SETMATERIAL:
                    {
                        SVGA3dCmdSetMaterial *pCmd = (SVGA3dCmdSetMaterial *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetMaterial);

                        rc = vmsvga3dSetMaterial(pThisCC, pCmd->cid, pCmd->face, &pCmd->material);
                        break;
                    }

                    case SVGA_3D_CMD_SETLIGHTDATA:
                    {
                        SVGA3dCmdSetLightData *pCmd = (SVGA3dCmdSetLightData *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetLightData);

                        rc = vmsvga3dSetLightData(pThisCC, pCmd->cid, pCmd->index, &pCmd->data);
                        break;
                    }

                    case SVGA_3D_CMD_SETLIGHTENABLED:
                    {
                        SVGA3dCmdSetLightEnabled *pCmd = (SVGA3dCmdSetLightEnabled *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetLightEnable);

                        rc = vmsvga3dSetLightEnabled(pThisCC, pCmd->cid, pCmd->index, pCmd->enabled);
                        break;
                    }

                    case SVGA_3D_CMD_SETVIEWPORT:
                    {
                        SVGA3dCmdSetViewport *pCmd = (SVGA3dCmdSetViewport *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetViewPort);

                        rc = vmsvga3dSetViewPort(pThisCC, pCmd->cid, &pCmd->rect);
                        break;
                    }

                    case SVGA_3D_CMD_SETCLIPPLANE:
                    {
                        SVGA3dCmdSetClipPlane *pCmd = (SVGA3dCmdSetClipPlane *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetClipPlane);

                        rc = vmsvga3dSetClipPlane(pThisCC, pCmd->cid, pCmd->index, pCmd->plane);
                        break;
                    }

                    case SVGA_3D_CMD_CLEAR:
                    {
                        SVGA3dCmdClear  *pCmd = (SVGA3dCmdClear *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dClear);

                        uint32_t cRects = (pHdr->size - sizeof(*pCmd)) / sizeof(SVGA3dRect);
                        rc = vmsvga3dCommandClear(pThisCC, pCmd->cid, pCmd->clearFlag, pCmd->color, pCmd->depth, pCmd->stencil, cRects, (SVGA3dRect *)(pCmd + 1));
                        break;
                    }

                    case SVGA_3D_CMD_PRESENT:
                    case SVGA_3D_CMD_PRESENT_READBACK: /** @todo SVGA_3D_CMD_PRESENT_READBACK isn't quite the same as present... */
                    {
                        SVGA3dCmdPresent *pCmd = (SVGA3dCmdPresent *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        if ((unsigned)enmCmdId == SVGA_3D_CMD_PRESENT)
                            STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dPresent);
                        else
                            STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dPresentReadBack);

                        uint32_t cRects = (pHdr->size - sizeof(*pCmd)) / sizeof(SVGA3dCopyRect);

                        STAM_PROFILE_START(&pSVGAState->StatR3Cmd3dPresentProf, a);
                        rc = vmsvga3dCommandPresent(pThis, pThisCC, pCmd->sid, cRects, (SVGA3dCopyRect *)(pCmd + 1));
                        STAM_PROFILE_STOP(&pSVGAState->StatR3Cmd3dPresentProf, a);
                        break;
                    }

                    case SVGA_3D_CMD_SHADER_DEFINE:
                    {
                        SVGA3dCmdDefineShader *pCmd = (SVGA3dCmdDefineShader *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dShaderDefine);

                        uint32_t cbData = (pHdr->size - sizeof(*pCmd));
                        rc = vmsvga3dShaderDefine(pThisCC, pCmd->cid, pCmd->shid, pCmd->type, cbData, (uint32_t *)(pCmd + 1));
                        break;
                    }

                    case SVGA_3D_CMD_SHADER_DESTROY:
                    {
                        SVGA3dCmdDestroyShader *pCmd = (SVGA3dCmdDestroyShader *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dShaderDestroy);

                        rc = vmsvga3dShaderDestroy(pThisCC, pCmd->cid, pCmd->shid, pCmd->type);
                        break;
                    }

                    case SVGA_3D_CMD_SET_SHADER:
                    {
                        SVGA3dCmdSetShader *pCmd = (SVGA3dCmdSetShader *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetShader);

                        rc = vmsvga3dShaderSet(pThisCC, NULL, pCmd->cid, pCmd->type, pCmd->shid);
                        break;
                    }

                    case SVGA_3D_CMD_SET_SHADER_CONST:
                    {
                        SVGA3dCmdSetShaderConst *pCmd = (SVGA3dCmdSetShaderConst *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetShaderConst);

                        uint32_t cRegisters = (pHdr->size - sizeof(*pCmd)) / sizeof(pCmd->values) + 1;
                        rc = vmsvga3dShaderSetConst(pThisCC, pCmd->cid, pCmd->reg, pCmd->type, pCmd->ctype, cRegisters, pCmd->values);
                        break;
                    }

                    case SVGA_3D_CMD_DRAW_PRIMITIVES:
                    {
                        SVGA3dCmdDrawPrimitives *pCmd = (SVGA3dCmdDrawPrimitives *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dDrawPrimitives);

                        AssertBreak(pCmd->numRanges <= SVGA3D_MAX_DRAW_PRIMITIVE_RANGES);
                        AssertBreak(pCmd->numVertexDecls <= SVGA3D_MAX_VERTEX_ARRAYS);
                        uint32_t const cbRangesAndVertexDecls = pCmd->numVertexDecls * sizeof(SVGA3dVertexDecl)
                                                              + pCmd->numRanges * sizeof(SVGA3dPrimitiveRange);
                        ASSERT_GUEST_BREAK(cbRangesAndVertexDecls <= pHdr->size - sizeof(*pCmd));

                        uint32_t cVertexDivisor = (pHdr->size - sizeof(*pCmd) - cbRangesAndVertexDecls) / sizeof(uint32_t);
                        AssertBreak(!cVertexDivisor || cVertexDivisor == pCmd->numVertexDecls);

                        RT_UNTRUSTED_VALIDATED_FENCE();

                        SVGA3dVertexDecl     *pVertexDecl    = (SVGA3dVertexDecl *)(pCmd + 1);
                        SVGA3dPrimitiveRange *pNumRange      = (SVGA3dPrimitiveRange *)&pVertexDecl[pCmd->numVertexDecls];
                        SVGA3dVertexDivisor  *pVertexDivisor = cVertexDivisor ? (SVGA3dVertexDivisor *)&pNumRange[pCmd->numRanges] : NULL;

                        STAM_PROFILE_START(&pSVGAState->StatR3Cmd3dDrawPrimitivesProf, a);
                        rc = vmsvga3dDrawPrimitives(pThisCC, pCmd->cid, pCmd->numVertexDecls, pVertexDecl, pCmd->numRanges,
                                                    pNumRange, cVertexDivisor, pVertexDivisor);
                        STAM_PROFILE_STOP(&pSVGAState->StatR3Cmd3dDrawPrimitivesProf, a);
                        break;
                    }

                    case SVGA_3D_CMD_SETSCISSORRECT:
                    {
                        SVGA3dCmdSetScissorRect *pCmd = (SVGA3dCmdSetScissorRect *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dSetScissorRect);

                        rc = vmsvga3dSetScissorRect(pThisCC, pCmd->cid, &pCmd->rect);
                        break;
                    }

                    case SVGA_3D_CMD_BEGIN_QUERY:
                    {
                        SVGA3dCmdBeginQuery *pCmd = (SVGA3dCmdBeginQuery *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dBeginQuery);

                        rc = vmsvga3dQueryBegin(pThisCC, pCmd->cid, pCmd->type);
                        break;
                    }

                    case SVGA_3D_CMD_END_QUERY:
                    {
                        SVGA3dCmdEndQuery *pCmd = (SVGA3dCmdEndQuery *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dEndQuery);

                        rc = vmsvga3dQueryEnd(pThisCC, pCmd->cid, pCmd->type, pCmd->guestResult);
                        break;
                    }

                    case SVGA_3D_CMD_WAIT_FOR_QUERY:
                    {
                        SVGA3dCmdWaitForQuery *pCmd = (SVGA3dCmdWaitForQuery *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dWaitForQuery);

                        rc = vmsvga3dQueryWait(pThis, pThisCC, pCmd->cid, pCmd->type, pCmd->guestResult);
                        break;
                    }

                    case SVGA_3D_CMD_GENERATE_MIPMAPS:
                    {
                        SVGA3dCmdGenerateMipmaps *pCmd = (SVGA3dCmdGenerateMipmaps *)(pHdr + 1);
                        VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK(sizeof(*pCmd));
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dGenerateMipmaps);

                        rc = vmsvga3dGenerateMipmaps(pThisCC, pCmd->sid, pCmd->filter);
                        break;
                    }

                    case SVGA_3D_CMD_ACTIVATE_SURFACE:
                        /* context id + surface id? */
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dActivateSurface);
                        break;
                    case SVGA_3D_CMD_DEACTIVATE_SURFACE:
                        /* context id + surface id? */
                        STAM_REL_COUNTER_INC(&pSVGAState->StatR3Cmd3dDeactivateSurface);
                        break;

                    default:
                        STAM_REL_COUNTER_INC(&pSVGAState->StatFifoUnkCmds);
                        AssertMsgFailed(("enmCmdId=%d\n", enmCmdId));
                        break;
                    }
                }
                else
# endif // VBOX_WITH_VMSVGA3D
                {
                    STAM_REL_COUNTER_INC(&pSVGAState->StatFifoUnkCmds);
                    AssertMsgFailed(("enmCmdId=%d\n", enmCmdId));
                }
            }

            /* Go to the next slot */
            Assert(cbPayload + sizeof(uint32_t) <= offFifoMax - offFifoMin);
            offCurrentCmd += RT_ALIGN_32(cbPayload + sizeof(uint32_t), sizeof(uint32_t));
            if (offCurrentCmd >= offFifoMax)
            {
                offCurrentCmd -= offFifoMax - offFifoMin;
                Assert(offCurrentCmd >= offFifoMin);
                Assert(offCurrentCmd <  offFifoMax);
            }
            ASMAtomicWriteU32(&pFIFO[SVGA_FIFO_STOP], offCurrentCmd);
            STAM_REL_COUNTER_INC(&pSVGAState->StatFifoCommands);

            /*
             * Raise IRQ if required.  Must enter the critical section here
             * before making final decisions here, otherwise cubebench and
             * others may end up waiting forever.
             */
            if (   u32IrqStatus
                || (pThis->svga.u32IrqMask & SVGA_IRQFLAG_FIFO_PROGRESS))
            {
                int rc2 = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VERR_IGNORED);
                AssertRC(rc2);

                /* FIFO progress might trigger an interrupt. */
                if (pThis->svga.u32IrqMask & SVGA_IRQFLAG_FIFO_PROGRESS)
                {
                    Log(("vmsvgaR3FifoLoop: fifo progress irq\n"));
                    u32IrqStatus |= SVGA_IRQFLAG_FIFO_PROGRESS;
                }

                /* Unmasked IRQ pending? */
                if (pThis->svga.u32IrqMask & u32IrqStatus)
                {
                    Log(("vmsvgaR3FifoLoop: Trigger interrupt with status %x\n", u32IrqStatus));
                    ASMAtomicOrU32(&pThis->svga.u32IrqStatus, u32IrqStatus);
                    PDMDevHlpPCISetIrq(pDevIns, 0, 1);
                }

                PDMDevHlpCritSectLeave(pDevIns, &pThis->CritSect);
            }
        }

        /* If really done, clear the busy flag. */
        if (fDone)
        {
            Log(("vmsvgaR3FifoLoop: emptied the FIFO next=%x stop=%x\n", pFIFO[SVGA_FIFO_NEXT_CMD], offCurrentCmd));
            vmsvgaR3FifoSetNotBusy(pDevIns, pThis, pThisCC, pSVGAState, offFifoMin);
        }
    }

    /*
     * Free the bounce buffer. (There are no returns above!)
     */
    RTMemFree(pbBounceBuf);

    return VINF_SUCCESS;
}

#undef VMSVGAFIFO_CHECK_3D_CMD_MIN_SIZE_BREAK
#undef VMSVGAFIFO_GET_MORE_CMD_BUFFER_BREAK
#undef VMSVGAFIFO_GET_CMD_BUFFER_BREAK

#ifdef VBOX_WITH_VMSVGA3D
/**
 * Free the specified GMR
 *
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 * @param   idGMR           GMR id
 */
static void vmsvgaR3GmrFree(PVGASTATECC pThisCC, uint32_t idGMR)
{
    PVMSVGAR3STATE pSVGAState = pThisCC->svga.pSvgaR3State;

    /* Free the old descriptor if present. */
    PGMR pGMR = &pSVGAState->paGMR[idGMR];
    if (   pGMR->numDescriptors
        || pGMR->paDesc /* needed till we implement SVGA_REMAP_GMR2_VIA_GMR */)
    {
# ifdef DEBUG_GMR_ACCESS
        VMR3ReqCallWaitU(PDMDevHlpGetUVM(pThisCC->pDevIns), VMCPUID_ANY, (PFNRT)vmsvgaR3DeregisterGmr, 2, pDevIns, idGMR);
# endif

        Assert(pGMR->paDesc);
        RTMemFree(pGMR->paDesc);
        pGMR->paDesc         = NULL;
        pGMR->numDescriptors = 0;
        pGMR->cbTotal        = 0;
        pGMR->cMaxPages      = 0;
    }
    Assert(!pGMR->cMaxPages);
    Assert(!pGMR->cbTotal);
}
#endif /* VBOX_WITH_VMSVGA3D */

/**
 * Copy between a GMR and a host memory buffer.
 *
 * @returns VBox status code.
 * @param   pThis           The shared VGA/VMSVGA instance data.
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 * @param   enmTransferType Transfer type (read/write)
 * @param   pbHstBuf        Host buffer pointer (valid)
 * @param   cbHstBuf        Size of host buffer (valid)
 * @param   offHst          Host buffer offset of the first scanline
 * @param   cbHstPitch      Destination buffer pitch
 * @param   gstPtr          GMR description
 * @param   offGst          Guest buffer offset of the first scanline
 * @param   cbGstPitch      Guest buffer pitch
 * @param   cbWidth         Width in bytes to copy
 * @param   cHeight         Number of scanllines to copy
 */
int vmsvgaR3GmrTransfer(PVGASTATE pThis, PVGASTATECC pThisCC, const SVGA3dTransferType enmTransferType,
                        uint8_t *pbHstBuf, uint32_t cbHstBuf, uint32_t offHst, int32_t cbHstPitch,
                        SVGAGuestPtr gstPtr, uint32_t offGst, int32_t cbGstPitch,
                        uint32_t cbWidth, uint32_t cHeight)
{
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;
    PPDMDEVINS      pDevIns = pThisCC->pDevIns; /* simpler */
    int             rc;

    LogFunc(("%s host %p size=%d offset %d pitch=%d; guest gmr=%#x:%#x offset=%d pitch=%d cbWidth=%d cHeight=%d\n",
             enmTransferType == SVGA3D_READ_HOST_VRAM ? "WRITE" : "READ", /* GMR op: READ host VRAM means WRITE GMR */
             pbHstBuf, cbHstBuf, offHst, cbHstPitch,
             gstPtr.gmrId, gstPtr.offset, offGst, cbGstPitch, cbWidth, cHeight));
    AssertReturn(cbWidth && cHeight, VERR_INVALID_PARAMETER);

    PGMR pGMR;
    uint32_t cbGmr; /* The GMR size in bytes. */
    if (gstPtr.gmrId == SVGA_GMR_FRAMEBUFFER)
    {
        pGMR = NULL;
        cbGmr = pThis->vram_size;
    }
    else
    {
        AssertReturn(gstPtr.gmrId < pThis->svga.cGMR, VERR_INVALID_PARAMETER);
        RT_UNTRUSTED_VALIDATED_FENCE();
        pGMR = &pSVGAState->paGMR[gstPtr.gmrId];
        cbGmr = pGMR->cbTotal;
    }

    /*
     * GMR
     */
    /* Calculate GMR offset of the data to be copied. */
    AssertMsgReturn(gstPtr.offset < cbGmr,
                    ("gmr=%#x:%#x offGst=%#x cbGstPitch=%#x cHeight=%#x cbWidth=%#x cbGmr=%#x\n",
                     gstPtr.gmrId, gstPtr.offset, offGst, cbGstPitch, cHeight, cbWidth, cbGmr),
                    VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();
    AssertMsgReturn(offGst < cbGmr - gstPtr.offset,
                    ("gmr=%#x:%#x offGst=%#x cbGstPitch=%#x cHeight=%#x cbWidth=%#x cbGmr=%#x\n",
                     gstPtr.gmrId, gstPtr.offset, offGst, cbGstPitch, cHeight, cbWidth, cbGmr),
                    VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();
    uint32_t const offGmr = offGst + gstPtr.offset; /* Offset in the GMR, where the first scanline is located. */

    /* Verify that cbWidth is less than scanline and fits into the GMR. */
    uint32_t const cbGmrScanline = cbGstPitch > 0 ? cbGstPitch : -cbGstPitch;
    AssertMsgReturn(cbGmrScanline != 0,
                    ("gmr=%#x:%#x offGst=%#x cbGstPitch=%#x cHeight=%#x cbWidth=%#x cbGmr=%#x\n",
                     gstPtr.gmrId, gstPtr.offset, offGst, cbGstPitch, cHeight, cbWidth, cbGmr),
                    VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();
    AssertMsgReturn(cbWidth <= cbGmrScanline,
                    ("gmr=%#x:%#x offGst=%#x cbGstPitch=%#x cHeight=%#x cbWidth=%#x cbGmr=%#x\n",
                     gstPtr.gmrId, gstPtr.offset, offGst, cbGstPitch, cHeight, cbWidth, cbGmr),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(cbWidth <= cbGmr - offGmr,
                    ("gmr=%#x:%#x offGst=%#x cbGstPitch=%#x cHeight=%#x cbWidth=%#x cbGmr=%#x\n",
                     gstPtr.gmrId, gstPtr.offset, offGst, cbGstPitch, cHeight, cbWidth, cbGmr),
                    VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();

    /* How many bytes are available for the data in the GMR. */
    uint32_t const cbGmrLeft = cbGstPitch > 0 ? cbGmr - offGmr : offGmr + cbWidth;

    /* How many scanlines would fit into the available data. */
    uint32_t cGmrScanlines = cbGmrLeft / cbGmrScanline;
    uint32_t const cbGmrLastScanline = cbGmrLeft - cGmrScanlines * cbGmrScanline; /* Slack space. */
    if (cbWidth <= cbGmrLastScanline)
        ++cGmrScanlines;

    if (cHeight > cGmrScanlines)
        cHeight = cGmrScanlines;

    AssertMsgReturn(cHeight > 0,
                    ("gmr=%#x:%#x offGst=%#x cbGstPitch=%#x cHeight=%#x cbWidth=%#x cbGmr=%#x\n",
                     gstPtr.gmrId, gstPtr.offset, offGst, cbGstPitch, cHeight, cbWidth, cbGmr),
                    VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();

    /*
     * Host buffer.
     */
    AssertMsgReturn(offHst < cbHstBuf,
                    ("buffer=%p size %d offHst=%d cbHstPitch=%d cHeight=%d cbWidth=%d\n",
                     pbHstBuf, cbHstBuf, offHst, cbHstPitch, cHeight, cbWidth),
                    VERR_INVALID_PARAMETER);

    /* Verify that cbWidth is less than scanline and fits into the buffer. */
    uint32_t const cbHstScanline = cbHstPitch > 0 ? cbHstPitch : -cbHstPitch;
    AssertMsgReturn(cbHstScanline != 0,
                    ("buffer=%p size %d offHst=%d cbHstPitch=%d cHeight=%d cbWidth=%d\n",
                     pbHstBuf, cbHstBuf, offHst, cbHstPitch, cHeight, cbWidth),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(cbWidth <= cbHstScanline,
                    ("buffer=%p size %d offHst=%d cbHstPitch=%d cHeight=%d cbWidth=%d\n",
                     pbHstBuf, cbHstBuf, offHst, cbHstPitch, cHeight, cbWidth),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(cbWidth <= cbHstBuf - offHst,
                    ("buffer=%p size %d offHst=%d cbHstPitch=%d cHeight=%d cbWidth=%d\n",
                     pbHstBuf, cbHstBuf, offHst, cbHstPitch, cHeight, cbWidth),
                    VERR_INVALID_PARAMETER);

    /* How many bytes are available for the data in the buffer. */
    uint32_t const cbHstLeft = cbHstPitch > 0 ? cbHstBuf - offHst : offHst + cbWidth;

    /* How many scanlines would fit into the available data. */
    uint32_t cHstScanlines = cbHstLeft / cbHstScanline;
    uint32_t const cbHstLastScanline = cbHstLeft - cHstScanlines * cbHstScanline; /* Slack space. */
    if (cbWidth <= cbHstLastScanline)
        ++cHstScanlines;

    if (cHeight > cHstScanlines)
        cHeight = cHstScanlines;

    AssertMsgReturn(cHeight > 0,
                    ("buffer=%p size %d offHst=%d cbHstPitch=%d cHeight=%d cbWidth=%d\n",
                     pbHstBuf, cbHstBuf, offHst, cbHstPitch, cHeight, cbWidth),
                    VERR_INVALID_PARAMETER);

    uint8_t *pbHst = pbHstBuf + offHst;

    /* Shortcut for the framebuffer. */
    if (gstPtr.gmrId == SVGA_GMR_FRAMEBUFFER)
    {
        uint8_t *pbGst = pThisCC->pbVRam + offGmr;

        uint8_t const *pbSrc;
        int32_t cbSrcPitch;
        uint8_t *pbDst;
        int32_t cbDstPitch;

        if (enmTransferType == SVGA3D_READ_HOST_VRAM)
        {
            pbSrc      = pbHst;
            cbSrcPitch = cbHstPitch;
            pbDst      = pbGst;
            cbDstPitch = cbGstPitch;
        }
        else
        {
            pbSrc      = pbGst;
            cbSrcPitch = cbGstPitch;
            pbDst      = pbHst;
            cbDstPitch = cbHstPitch;
        }

        if (   cbWidth == (uint32_t)cbGstPitch
            && cbGstPitch == cbHstPitch)
        {
            /* Entire scanlines, positive pitch. */
            memcpy(pbDst, pbSrc, cbWidth * cHeight);
        }
        else
        {
            for (uint32_t i = 0; i < cHeight; ++i)
            {
                memcpy(pbDst, pbSrc, cbWidth);

                pbDst += cbDstPitch;
                pbSrc += cbSrcPitch;
            }
        }
        return VINF_SUCCESS;
    }

    AssertPtrReturn(pGMR, VERR_INVALID_PARAMETER);
    AssertReturn(pGMR->numDescriptors > 0, VERR_INVALID_PARAMETER);

    PVMSVGAGMRDESCRIPTOR const paDesc = pGMR->paDesc; /* Local copy of the pointer. */
    uint32_t iDesc = 0;                               /* Index in the descriptor array. */
    uint32_t offDesc = 0;                             /* GMR offset of the current descriptor. */
    uint32_t offGmrScanline = offGmr;                 /* GMR offset of the scanline which is being copied. */
    uint8_t *pbHstScanline = pbHst;                   /* Host address of the scanline which is being copied. */
    for (uint32_t i = 0; i < cHeight; ++i)
    {
        uint32_t cbCurrentWidth = cbWidth;
        uint32_t offGmrCurrent  = offGmrScanline;
        uint8_t *pbCurrentHost  = pbHstScanline;

        /* Find the right descriptor */
        while (offDesc + paDesc[iDesc].numPages * PAGE_SIZE <= offGmrCurrent)
        {
            offDesc += paDesc[iDesc].numPages * PAGE_SIZE;
            AssertReturn(offDesc < pGMR->cbTotal, VERR_INTERNAL_ERROR); /* overflow protection */
            ++iDesc;
            AssertReturn(iDesc < pGMR->numDescriptors, VERR_INTERNAL_ERROR);
        }

        while (cbCurrentWidth)
        {
            uint32_t cbToCopy;

            if (offGmrCurrent + cbCurrentWidth <= offDesc + paDesc[iDesc].numPages * PAGE_SIZE)
            {
                cbToCopy = cbCurrentWidth;
            }
            else
            {
                cbToCopy = (offDesc + paDesc[iDesc].numPages * PAGE_SIZE - offGmrCurrent);
                AssertReturn(cbToCopy <= cbCurrentWidth, VERR_INVALID_PARAMETER);
            }

            RTGCPHYS const GCPhys = paDesc[iDesc].GCPhys + offGmrCurrent - offDesc;

            Log5Func(("%s phys=%RGp\n", (enmTransferType == SVGA3D_WRITE_HOST_VRAM) ? "READ" : "WRITE", GCPhys));

            if (enmTransferType == SVGA3D_WRITE_HOST_VRAM)
                rc = PDMDevHlpPhysRead(pDevIns, GCPhys, pbCurrentHost, cbToCopy);
            else
                rc = PDMDevHlpPhysWrite(pDevIns, GCPhys, pbCurrentHost, cbToCopy);
            AssertRCBreak(rc);

            cbCurrentWidth -= cbToCopy;
            offGmrCurrent  += cbToCopy;
            pbCurrentHost  += cbToCopy;

            /* Go to the next descriptor if there's anything left. */
            if (cbCurrentWidth)
            {
                offDesc += paDesc[iDesc].numPages * PAGE_SIZE;
                AssertReturn(offDesc < pGMR->cbTotal, VERR_INTERNAL_ERROR);
                ++iDesc;
                AssertReturn(iDesc < pGMR->numDescriptors, VERR_INTERNAL_ERROR);
            }
        }

        offGmrScanline += cbGstPitch;
        pbHstScanline  += cbHstPitch;
    }

    return VINF_SUCCESS;
}


/**
 * Unsigned coordinates in pBox. Clip to [0; pSizeSrc), [0; pSizeDest).
 *
 * @param   pSizeSrc    Source surface dimensions.
 * @param   pSizeDest   Destination surface dimensions.
 * @param   pBox        Coordinates to be clipped.
 */
void vmsvgaR3ClipCopyBox(const SVGA3dSize *pSizeSrc, const SVGA3dSize *pSizeDest, SVGA3dCopyBox *pBox)
{
    /* Src x, w */
    if (pBox->srcx > pSizeSrc->width)
        pBox->srcx = pSizeSrc->width;
    if (pBox->w > pSizeSrc->width - pBox->srcx)
        pBox->w = pSizeSrc->width - pBox->srcx;

    /* Src y, h */
    if (pBox->srcy > pSizeSrc->height)
        pBox->srcy = pSizeSrc->height;
    if (pBox->h > pSizeSrc->height - pBox->srcy)
        pBox->h = pSizeSrc->height - pBox->srcy;

    /* Src z, d */
    if (pBox->srcz > pSizeSrc->depth)
        pBox->srcz = pSizeSrc->depth;
    if (pBox->d > pSizeSrc->depth - pBox->srcz)
        pBox->d = pSizeSrc->depth - pBox->srcz;

    /* Dest x, w */
    if (pBox->x > pSizeDest->width)
        pBox->x = pSizeDest->width;
    if (pBox->w > pSizeDest->width - pBox->x)
        pBox->w = pSizeDest->width - pBox->x;

    /* Dest y, h */
    if (pBox->y > pSizeDest->height)
        pBox->y = pSizeDest->height;
    if (pBox->h > pSizeDest->height - pBox->y)
        pBox->h = pSizeDest->height - pBox->y;

    /* Dest z, d */
    if (pBox->z > pSizeDest->depth)
        pBox->z = pSizeDest->depth;
    if (pBox->d > pSizeDest->depth - pBox->z)
        pBox->d = pSizeDest->depth - pBox->z;
}

/**
 * Unsigned coordinates in pBox. Clip to [0; pSize).
 *
 * @param   pSize   Source surface dimensions.
 * @param   pBox    Coordinates to be clipped.
 */
void vmsvgaR3ClipBox(const SVGA3dSize *pSize, SVGA3dBox *pBox)
{
    /* x, w */
    if (pBox->x > pSize->width)
        pBox->x = pSize->width;
    if (pBox->w > pSize->width - pBox->x)
        pBox->w = pSize->width - pBox->x;

    /* y, h */
    if (pBox->y > pSize->height)
        pBox->y = pSize->height;
    if (pBox->h > pSize->height - pBox->y)
        pBox->h = pSize->height - pBox->y;

    /* z, d */
    if (pBox->z > pSize->depth)
        pBox->z = pSize->depth;
    if (pBox->d > pSize->depth - pBox->z)
        pBox->d = pSize->depth - pBox->z;
}

/**
 * Clip.
 *
 * @param   pBound  Bounding rectangle.
 * @param   pRect   Rectangle to be clipped.
 */
void vmsvgaR3ClipRect(SVGASignedRect const *pBound, SVGASignedRect *pRect)
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;

    /* Right order. */
    Assert(pBound->left <= pBound->right && pBound->top <= pBound->bottom);
    if (pRect->left < pRect->right)
    {
        left = pRect->left;
        right = pRect->right;
    }
    else
    {
        left = pRect->right;
        right = pRect->left;
    }
    if (pRect->top < pRect->bottom)
    {
        top = pRect->top;
        bottom = pRect->bottom;
    }
    else
    {
        top = pRect->bottom;
        bottom = pRect->top;
    }

    if (left < pBound->left)
        left = pBound->left;
    if (right < pBound->left)
        right = pBound->left;

    if (left > pBound->right)
        left = pBound->right;
    if (right > pBound->right)
        right = pBound->right;

    if (top < pBound->top)
        top = pBound->top;
    if (bottom < pBound->top)
        bottom = pBound->top;

    if (top > pBound->bottom)
        top = pBound->bottom;
    if (bottom > pBound->bottom)
        bottom = pBound->bottom;

    pRect->left = left;
    pRect->right = right;
    pRect->top = top;
    pRect->bottom = bottom;
}

/**
 * @callback_method_impl{PFNPDMTHREADWAKEUPDEV,
 * Unblock the FIFO I/O thread so it can respond to a state change.}
 */
static DECLCALLBACK(int) vmsvgaR3FifoLoopWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    RT_NOREF(pDevIns);
    PVGASTATE pThis = (PVGASTATE)pThread->pvUser;
    Log(("vmsvgaR3FifoLoopWakeUp\n"));
    return PDMDevHlpSUPSemEventSignal(pDevIns, pThis->svga.hFIFORequestSem);
}

/**
 * Enables or disables dirty page tracking for the framebuffer
 *
 * @param   pDevIns         The device instance.
 * @param   pThis           The shared VGA/VMSVGA instance data.
 * @param   fTraces         Enable/disable traces
 */
static void vmsvgaR3SetTraces(PPDMDEVINS pDevIns, PVGASTATE pThis, bool fTraces)
{
    if (    (!pThis->svga.fConfigured || !pThis->svga.fEnabled)
        &&  !fTraces)
    {
        //Assert(pThis->svga.fTraces);
        Log(("vmsvgaR3SetTraces: *not* allowed to disable dirty page tracking when the device is in legacy mode.\n"));
        return;
    }

    pThis->svga.fTraces = fTraces;
    if (pThis->svga.fTraces)
    {
        unsigned cbFrameBuffer = pThis->vram_size;

        Log(("vmsvgaR3SetTraces: enable dirty page handling for the frame buffer only (%x bytes)\n", 0));
        /** @todo How does this work with screens? */
        if (pThis->svga.uHeight != VMSVGA_VAL_UNINITIALIZED)
        {
# ifndef DEBUG_bird /* BB-10.3.1 triggers this as it initializes everything to zero. Better just ignore it. */
            Assert(pThis->svga.cbScanline);
# endif
            /* Hardware enabled; return real framebuffer size .*/
            cbFrameBuffer = (uint32_t)pThis->svga.uHeight * pThis->svga.cbScanline;
            cbFrameBuffer = RT_ALIGN(cbFrameBuffer, PAGE_SIZE);
        }

        if (!pThis->svga.fVRAMTracking)
        {
            Log(("vmsvgaR3SetTraces: enable frame buffer dirty page tracking. (%x bytes; vram %x)\n", cbFrameBuffer, pThis->vram_size));
            vgaR3RegisterVRAMHandler(pDevIns, pThis, cbFrameBuffer);
            pThis->svga.fVRAMTracking = true;
        }
    }
    else
    {
        if (pThis->svga.fVRAMTracking)
        {
            Log(("vmsvgaR3SetTraces: disable frame buffer dirty page tracking\n"));
            vgaR3UnregisterVRAMHandler(pDevIns, pThis);
            pThis->svga.fVRAMTracking = false;
        }
    }
}

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
DECLCALLBACK(int) vmsvgaR3PciIORegionFifoMapUnmap(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                                  RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    PVGASTATE   pThis = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    int         rc;
    RT_NOREF(pPciDev);
    Assert(pPciDev == pDevIns->apPciDevs[0]);

    Log(("vmsvgaR3PciIORegionFifoMapUnmap: iRegion=%d GCPhysAddress=%RGp cb=%RGp enmType=%d\n", iRegion, GCPhysAddress, cb, enmType));
    AssertReturn(   iRegion == pThis->pciRegions.iFIFO
                 && (   enmType == PCI_ADDRESS_SPACE_MEM
                     || (enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH /* got wrong in 6.1.0RC1 */ && pThis->fStateLoaded))
                 , VERR_INTERNAL_ERROR);
    if (GCPhysAddress != NIL_RTGCPHYS)
    {
        /*
         * Mapping the FIFO RAM.
         */
        AssertLogRelMsg(cb == pThis->svga.cbFIFO, ("cb=%#RGp cbFIFO=%#x\n", cb, pThis->svga.cbFIFO));
        rc = PDMDevHlpMmio2Map(pDevIns, pThis->hMmio2VmSvgaFifo, GCPhysAddress);
        AssertRC(rc);

# if defined(VMSVGA_USE_FIFO_ACCESS_HANDLER) || defined(DEBUG_FIFO_ACCESS)
        if (RT_SUCCESS(rc))
        {
            rc = PGMHandlerPhysicalRegister(PDMDevHlpGetVM(pDevIns), GCPhysAddress,
#  ifdef DEBUG_FIFO_ACCESS
                                            GCPhysAddress + (pThis->svga.cbFIFO - 1),
#  else
                                            GCPhysAddress + PAGE_SIZE - 1,
#  endif
                                            pThis->svga.hFifoAccessHandlerType, pThis, NIL_RTR0PTR, NIL_RTRCPTR,
                                            "VMSVGA FIFO");
            AssertRC(rc);
        }
# endif
        if (RT_SUCCESS(rc))
        {
            pThis->svga.GCPhysFIFO = GCPhysAddress;
            Log(("vmsvgaR3IORegionMap: GCPhysFIFO=%RGp cbFIFO=%#x\n", GCPhysAddress, pThis->svga.cbFIFO));
        }
        rc = VINF_PCI_MAPPING_DONE; /* caller only cares about this status, so it is okay that we overwrite erros here. */
    }
    else
    {
        Assert(pThis->svga.GCPhysFIFO);
# if defined(VMSVGA_USE_FIFO_ACCESS_HANDLER) || defined(DEBUG_FIFO_ACCESS)
        rc = PGMHandlerPhysicalDeregister(PDMDevHlpGetVM(pDevIns), pThis->svga.GCPhysFIFO);
        AssertRC(rc);
# else
        rc = VINF_SUCCESS;
# endif
        pThis->svga.GCPhysFIFO = 0;
    }
    return rc;
}

# ifdef VBOX_WITH_VMSVGA3D

/**
 * Used by vmsvga3dInfoSurfaceWorker to make the FIFO thread to save one or all
 * surfaces to VMSVGA3DMIPMAPLEVEL::pSurfaceData heap buffers.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The The shared VGA/VMSVGA instance data.
 * @param   pThisCC     The VGA/VMSVGA state for ring-3.
 * @param   sid         Either UINT32_MAX or the ID of a specific surface.  If
 *                      UINT32_MAX is used, all surfaces are processed.
 */
void vmsvgaR33dSurfaceUpdateHeapBuffersOnFifoThread(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC, uint32_t sid)
{
    vmsvgaR3RunExtCmdOnFifoThread(pDevIns, pThis, pThisCC, VMSVGA_FIFO_EXTCMD_UPDATE_SURFACE_HEAP_BUFFERS, (void *)(uintptr_t)sid,
                                  sid == UINT32_MAX ? 10 * RT_MS_1SEC : RT_MS_1MIN);
}


/**
 * @callback_method_impl{FNDBGFHANDLERDEV, "vmsvga3dsfc"}
 */
DECLCALLBACK(void) vmsvgaR3Info3dSurface(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /* There might be a specific surface ID at the start of the
       arguments, if not show all surfaces. */
    uint32_t sid = UINT32_MAX;
    if (pszArgs)
        pszArgs = RTStrStripL(pszArgs);
    if (pszArgs && RT_C_IS_DIGIT(*pszArgs))
        sid = RTStrToUInt32(pszArgs);

    /* Verbose or terse display, we default to verbose. */
    bool fVerbose = true;
    if (RTStrIStr(pszArgs, "terse"))
        fVerbose = false;

    /* The size of the ascii art (x direction, y is 3/4 of x). */
    uint32_t cxAscii = 80;
    if (RTStrIStr(pszArgs, "gigantic"))
        cxAscii = 300;
    else if (RTStrIStr(pszArgs, "huge"))
        cxAscii = 180;
    else if (RTStrIStr(pszArgs, "big"))
        cxAscii = 132;
    else if (RTStrIStr(pszArgs, "normal"))
        cxAscii = 80;
    else if (RTStrIStr(pszArgs, "medium"))
        cxAscii = 64;
    else if (RTStrIStr(pszArgs, "small"))
        cxAscii = 48;
    else if (RTStrIStr(pszArgs, "tiny"))
        cxAscii = 24;

    /* Y invert the image when producing the ASCII art. */
    bool fInvY = false;
    if (RTStrIStr(pszArgs, "invy"))
        fInvY = true;

    vmsvga3dInfoSurfaceWorker(pDevIns, PDMDEVINS_2_DATA(pDevIns, PVGASTATE), PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC),
                              pHlp, sid, fVerbose, cxAscii, fInvY, NULL);
}


/**
 * @callback_method_impl{FNDBGFHANDLERDEV, "vmsvga3dsurf"}
 */
DECLCALLBACK(void) vmsvgaR3Info3dSurfaceBmp(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /* pszArg = "sid[>dir]"
     * Writes %dir%/info-S-sidI.bmp, where S - sequential bitmap number, I - decimal surface id.
     */
    char *pszBitmapPath = NULL;
    uint32_t sid = UINT32_MAX;
    if (pszArgs)
        pszArgs = RTStrStripL(pszArgs);
    if (pszArgs && RT_C_IS_DIGIT(*pszArgs))
        RTStrToUInt32Ex(pszArgs, &pszBitmapPath, 0, &sid);
    if (   pszBitmapPath
        && *pszBitmapPath == '>')
        ++pszBitmapPath;

    const bool fVerbose = true;
    const uint32_t cxAscii = 0; /* No ASCII */
    const bool fInvY = false;   /* Do not invert. */
    vmsvga3dInfoSurfaceWorker(pDevIns, PDMDEVINS_2_DATA(pDevIns, PVGASTATE), PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC),
                              pHlp, sid, fVerbose, cxAscii, fInvY, pszBitmapPath);
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV, "vmsvga3dctx"}
 */
DECLCALLBACK(void) vmsvgaR3Info3dContext(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /* There might be a specific surface ID at the start of the
       arguments, if not show all contexts. */
    uint32_t sid = UINT32_MAX;
    if (pszArgs)
        pszArgs = RTStrStripL(pszArgs);
    if (pszArgs && RT_C_IS_DIGIT(*pszArgs))
        sid = RTStrToUInt32(pszArgs);

    /* Verbose or terse display, we default to verbose. */
    bool fVerbose = true;
    if (RTStrIStr(pszArgs, "terse"))
        fVerbose = false;

    vmsvga3dInfoContextWorker(PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC), pHlp, sid, fVerbose);
}
# endif /* VBOX_WITH_VMSVGA3D */

/**
 * @callback_method_impl{FNDBGFHANDLERDEV, "vmsvga"}
 */
static DECLCALLBACK(void) vmsvgaR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVGASTATE       pThis      = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATECC     pThisCC    = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;
    uint32_t RT_UNTRUSTED_VOLATILE_GUEST *pFIFO = pThisCC->svga.pau32FIFO;
    RT_NOREF(pszArgs);

    pHlp->pfnPrintf(pHlp, "Extension enabled:  %RTbool\n", pThis->svga.fEnabled);
    pHlp->pfnPrintf(pHlp, "Configured:         %RTbool\n", pThis->svga.fConfigured);
    pHlp->pfnPrintf(pHlp, "Base I/O port:      %#x\n",
                    pThis->hIoPortVmSvga != NIL_IOMIOPORTHANDLE
                    ? PDMDevHlpIoPortGetMappingAddress(pDevIns, pThis->hIoPortVmSvga) : UINT32_MAX);
    pHlp->pfnPrintf(pHlp, "FIFO address:       %RGp\n", pThis->svga.GCPhysFIFO);
    pHlp->pfnPrintf(pHlp, "FIFO size:          %u (%#x)\n", pThis->svga.cbFIFO, pThis->svga.cbFIFO);
    pHlp->pfnPrintf(pHlp, "FIFO external cmd:  %#x\n", pThis->svga.u8FIFOExtCommand);
    pHlp->pfnPrintf(pHlp, "FIFO extcmd wakeup: %u\n", pThis->svga.fFifoExtCommandWakeup);
    pHlp->pfnPrintf(pHlp, "FIFO min/max:       %u/%u\n", pFIFO[SVGA_FIFO_MIN], pFIFO[SVGA_FIFO_MAX]);
    pHlp->pfnPrintf(pHlp, "Busy:               %#x\n", pThis->svga.fBusy);
    pHlp->pfnPrintf(pHlp, "Traces:             %RTbool (effective: %RTbool)\n", pThis->svga.fTraces, pThis->svga.fVRAMTracking);
    pHlp->pfnPrintf(pHlp, "Guest ID:           %#x (%d)\n", pThis->svga.u32GuestId, pThis->svga.u32GuestId);
    pHlp->pfnPrintf(pHlp, "IRQ status:         %#x\n", pThis->svga.u32IrqStatus);
    pHlp->pfnPrintf(pHlp, "IRQ mask:           %#x\n", pThis->svga.u32IrqMask);
    pHlp->pfnPrintf(pHlp, "Pitch lock:         %#x (FIFO:%#x)\n", pThis->svga.u32PitchLock, pFIFO[SVGA_FIFO_PITCHLOCK]);
    pHlp->pfnPrintf(pHlp, "Current GMR ID:     %#x\n", pThis->svga.u32CurrentGMRId);
    pHlp->pfnPrintf(pHlp, "Capabilites reg:    %#x\n", pThis->svga.u32RegCaps);
    pHlp->pfnPrintf(pHlp, "Index reg:          %#x\n", pThis->svga.u32IndexReg);
    pHlp->pfnPrintf(pHlp, "Action flags:       %#x\n", pThis->svga.u32ActionFlags);
    pHlp->pfnPrintf(pHlp, "Max display size:   %ux%u\n", pThis->svga.u32MaxWidth, pThis->svga.u32MaxHeight);
    pHlp->pfnPrintf(pHlp, "Display size:       %ux%u %ubpp\n", pThis->svga.uWidth, pThis->svga.uHeight, pThis->svga.uBpp);
    pHlp->pfnPrintf(pHlp, "Scanline:           %u (%#x)\n", pThis->svga.cbScanline, pThis->svga.cbScanline);
    pHlp->pfnPrintf(pHlp, "Viewport position:  %ux%u\n", pThis->svga.viewport.x, pThis->svga.viewport.y);
    pHlp->pfnPrintf(pHlp, "Viewport size:      %ux%u\n", pThis->svga.viewport.cx, pThis->svga.viewport.cy);

    pHlp->pfnPrintf(pHlp, "Cursor active:      %RTbool\n", pSVGAState->Cursor.fActive);
    pHlp->pfnPrintf(pHlp, "Cursor hotspot:     %ux%u\n", pSVGAState->Cursor.xHotspot, pSVGAState->Cursor.yHotspot);
    pHlp->pfnPrintf(pHlp, "Cursor size:        %ux%u\n", pSVGAState->Cursor.width, pSVGAState->Cursor.height);
    pHlp->pfnPrintf(pHlp, "Cursor byte size:   %u (%#x)\n", pSVGAState->Cursor.cbData, pSVGAState->Cursor.cbData);

    pHlp->pfnPrintf(pHlp, "FIFO cursor:        state %u, screen %d\n", pFIFO[SVGA_FIFO_CURSOR_ON], pFIFO[SVGA_FIFO_CURSOR_SCREEN_ID]);
    pHlp->pfnPrintf(pHlp, "FIFO cursor at:     %u,%u\n", pFIFO[SVGA_FIFO_CURSOR_X], pFIFO[SVGA_FIFO_CURSOR_Y]);

    pHlp->pfnPrintf(pHlp, "Legacy cursor:      ID %u, state %u\n", pThis->svga.uCursorID, pThis->svga.uCursorOn);
    pHlp->pfnPrintf(pHlp, "Legacy cursor at:   %u,%u\n", pThis->svga.uCursorX, pThis->svga.uCursorY);

# ifdef VBOX_WITH_VMSVGA3D
    pHlp->pfnPrintf(pHlp, "3D enabled:         %RTbool\n", pThis->svga.f3DEnabled);
# endif
    if (pThisCC->pDrv)
    {
        pHlp->pfnPrintf(pHlp, "Driver mode:        %ux%u %ubpp\n", pThisCC->pDrv->cx, pThisCC->pDrv->cy, pThisCC->pDrv->cBits);
        pHlp->pfnPrintf(pHlp, "Driver pitch:       %u (%#x)\n", pThisCC->pDrv->cbScanline, pThisCC->pDrv->cbScanline);
    }

    /* Dump screen information. */
    for (unsigned iScreen = 0; iScreen < RT_ELEMENTS(pSVGAState->aScreens); ++iScreen)
    {
        VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, iScreen);
        if (pScreen)
        {
            pHlp->pfnPrintf(pHlp, "Screen %u defined (ID %u):\n", iScreen, pScreen->idScreen);
            pHlp->pfnPrintf(pHlp, "  %u x %u x %ubpp @ %u, %u\n", pScreen->cWidth, pScreen->cHeight,
                            pScreen->cBpp, pScreen->xOrigin, pScreen->yOrigin);
            pHlp->pfnPrintf(pHlp, "  Pitch %u bytes, VRAM offset %X\n", pScreen->cbPitch, pScreen->offVRAM);
            pHlp->pfnPrintf(pHlp, "  Flags %X", pScreen->fuScreen);
            if (pScreen->fuScreen != SVGA_SCREEN_MUST_BE_SET)
            {
                pHlp->pfnPrintf(pHlp, " (");
                if (pScreen->fuScreen & SVGA_SCREEN_IS_PRIMARY)
                    pHlp->pfnPrintf(pHlp, " IS_PRIMARY");
                if (pScreen->fuScreen & SVGA_SCREEN_FULLSCREEN_HINT)
                    pHlp->pfnPrintf(pHlp, " FULLSCREEN_HINT");
                if (pScreen->fuScreen & SVGA_SCREEN_DEACTIVATE)
                    pHlp->pfnPrintf(pHlp, " DEACTIVATE");
                if (pScreen->fuScreen & SVGA_SCREEN_BLANKING)
                    pHlp->pfnPrintf(pHlp, " BLANKING");
                pHlp->pfnPrintf(pHlp, " )");
            }
            pHlp->pfnPrintf(pHlp, ", %smodified\n", pScreen->fModified ? "" : "not ");
        }
    }

}

/**
 * Portion of VMSVGA state which must be loaded oin the FIFO thread.
 */
static int vmsvgaR3LoadExecFifo(PCPDMDEVHLPR3 pHlp, PVGASTATE pThis, PVGASTATECC pThisCC,
                                PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    RT_NOREF(uPass);

    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;
    int rc;

    if (uVersion >= VGA_SAVEDSTATE_VERSION_VMSVGA_SCREENS)
    {
        uint32_t cScreens = 0;
        rc = pHlp->pfnSSMGetU32(pSSM, &cScreens);
        AssertRCReturn(rc, rc);
        AssertLogRelMsgReturn(cScreens <= _64K, /* big enough */
                              ("cScreens=%#x\n", cScreens),
                              VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

        for (uint32_t i = 0; i < cScreens; ++i)
        {
            VMSVGASCREENOBJECT screen;
            RT_ZERO(screen);

            rc = pHlp->pfnSSMGetStructEx(pSSM, &screen, sizeof(screen), 0, g_aVMSVGASCREENOBJECTFields, NULL);
            AssertLogRelRCReturn(rc, rc);

            if (screen.idScreen < RT_ELEMENTS(pSVGAState->aScreens))
            {
                VMSVGASCREENOBJECT *pScreen = &pSVGAState->aScreens[screen.idScreen];
                *pScreen = screen;
                pScreen->fModified = true;
            }
            else
            {
                LogRel(("VGA: ignored screen object %d\n", screen.idScreen));
            }
        }
    }
    else
    {
        /* Try to setup at least the first screen. */
        VMSVGASCREENOBJECT *pScreen = &pSVGAState->aScreens[0];
        pScreen->fDefined  = true;
        pScreen->fModified = true;
        pScreen->fuScreen  = SVGA_SCREEN_MUST_BE_SET | SVGA_SCREEN_IS_PRIMARY;
        pScreen->idScreen  = 0;
        pScreen->xOrigin   = 0;
        pScreen->yOrigin   = 0;
        pScreen->offVRAM   = pThis->svga.uScreenOffset;
        pScreen->cbPitch   = pThis->svga.cbScanline;
        pScreen->cWidth    = pThis->svga.uWidth;
        pScreen->cHeight   = pThis->svga.uHeight;
        pScreen->cBpp      = pThis->svga.uBpp;
    }

    return VINF_SUCCESS;
}

/**
 * @copydoc FNSSMDEVLOADEXEC
 */
int vmsvgaR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    RT_NOREF(uPass);
    PVGASTATE       pThis      = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATECC     pThisCC    = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;
    PCPDMDEVHLPR3   pHlp       = pDevIns->pHlpR3;
    int             rc;

    /* Load our part of the VGAState */
    rc = pHlp->pfnSSMGetStructEx(pSSM, &pThis->svga, sizeof(pThis->svga), 0, g_aVGAStateSVGAFields, NULL);
    AssertRCReturn(rc, rc);

    /* Load the VGA framebuffer. */
    AssertCompile(VMSVGA_VGA_FB_BACKUP_SIZE >= _32K);
    uint32_t cbVgaFramebuffer = _32K;
    if (uVersion >= VGA_SAVEDSTATE_VERSION_VMSVGA_VGA_FB_FIX)
    {
        rc = pHlp->pfnSSMGetU32(pSSM, &cbVgaFramebuffer);
        AssertRCReturn(rc, rc);
        AssertLogRelMsgReturn(cbVgaFramebuffer <= _4M && cbVgaFramebuffer >= _32K && RT_IS_POWER_OF_TWO(cbVgaFramebuffer),
                              ("cbVgaFramebuffer=%#x - expected 32KB..4MB, power of two\n", cbVgaFramebuffer),
                              VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
        AssertCompile(VMSVGA_VGA_FB_BACKUP_SIZE <= _4M);
        AssertCompile(RT_IS_POWER_OF_TWO(VMSVGA_VGA_FB_BACKUP_SIZE));
    }
    rc = pHlp->pfnSSMGetMem(pSSM, pThisCC->svga.pbVgaFrameBufferR3, RT_MIN(cbVgaFramebuffer, VMSVGA_VGA_FB_BACKUP_SIZE));
    AssertRCReturn(rc, rc);
    if (cbVgaFramebuffer > VMSVGA_VGA_FB_BACKUP_SIZE)
        pHlp->pfnSSMSkip(pSSM, cbVgaFramebuffer - VMSVGA_VGA_FB_BACKUP_SIZE);
    else if (cbVgaFramebuffer < VMSVGA_VGA_FB_BACKUP_SIZE)
        RT_BZERO(&pThisCC->svga.pbVgaFrameBufferR3[cbVgaFramebuffer], VMSVGA_VGA_FB_BACKUP_SIZE - cbVgaFramebuffer);

    /* Load the VMSVGA state. */
    rc = pHlp->pfnSSMGetStructEx(pSSM, pSVGAState, sizeof(*pSVGAState), 0, g_aVMSVGAR3STATEFields, NULL);
    AssertRCReturn(rc, rc);

    /* Load the active cursor bitmaps. */
    if (pSVGAState->Cursor.fActive)
    {
        pSVGAState->Cursor.pData = RTMemAlloc(pSVGAState->Cursor.cbData);
        AssertReturn(pSVGAState->Cursor.pData, VERR_NO_MEMORY);

        rc = pHlp->pfnSSMGetMem(pSSM, pSVGAState->Cursor.pData, pSVGAState->Cursor.cbData);
        AssertRCReturn(rc, rc);
    }

    /* Load the GMR state. */
    uint32_t cGMR = 256; /* Hardcoded in previous saved state versions. */
    if (uVersion >= VGA_SAVEDSTATE_VERSION_VMSVGA_GMR_COUNT)
    {
        rc = pHlp->pfnSSMGetU32(pSSM, &cGMR);
        AssertRCReturn(rc, rc);
        /* Numbers of GMRs was never less than 256. 1MB is a large arbitrary limit. */
        AssertLogRelMsgReturn(cGMR <= _1M && cGMR >= 256,
                              ("cGMR=%#x - expected 256B..1MB\n", cGMR),
                              VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
    }

    if (pThis->svga.cGMR != cGMR)
    {
        /* Reallocate GMR array. */
        Assert(pSVGAState->paGMR != NULL);
        RTMemFree(pSVGAState->paGMR);
        pSVGAState->paGMR = (PGMR)RTMemAllocZ(cGMR * sizeof(GMR));
        AssertReturn(pSVGAState->paGMR, VERR_NO_MEMORY);
        pThis->svga.cGMR = cGMR;
    }

    for (uint32_t i = 0; i < cGMR; ++i)
    {
        PGMR pGMR = &pSVGAState->paGMR[i];

        rc = pHlp->pfnSSMGetStructEx(pSSM, pGMR, sizeof(*pGMR), 0, g_aGMRFields, NULL);
        AssertRCReturn(rc, rc);

        if (pGMR->numDescriptors)
        {
            Assert(pGMR->cMaxPages || pGMR->cbTotal);
            pGMR->paDesc = (PVMSVGAGMRDESCRIPTOR)RTMemAllocZ(pGMR->numDescriptors * sizeof(VMSVGAGMRDESCRIPTOR));
            AssertReturn(pGMR->paDesc, VERR_NO_MEMORY);

            for (uint32_t j = 0; j < pGMR->numDescriptors; ++j)
            {
                rc = pHlp->pfnSSMGetStructEx(pSSM, &pGMR->paDesc[j], sizeof(pGMR->paDesc[j]), 0, g_aVMSVGAGMRDESCRIPTORFields, NULL);
                AssertRCReturn(rc, rc);
            }
        }
    }

#  ifdef RT_OS_DARWIN  /** @todo r=bird: this is normally done on the EMT, so for DARWIN we do that when loading saved state too now. See DevVGA-SVGA3d-shared.h. */
    vmsvga3dPowerOn(pDevIns, pThis, PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC));
#  endif

    VMSVGA_STATE_LOAD LoadState;
    LoadState.pSSM     = pSSM;
    LoadState.uVersion = uVersion;
    LoadState.uPass    = uPass;
    rc = vmsvgaR3RunExtCmdOnFifoThread(pDevIns, pThis, pThisCC, VMSVGA_FIFO_EXTCMD_LOADSTATE, &LoadState, RT_INDEFINITE_WAIT);
    AssertLogRelRCReturn(rc, rc);

    return VINF_SUCCESS;
}

/**
 * Reinit the video mode after the state has been loaded.
 */
int vmsvgaR3LoadDone(PPDMDEVINS pDevIns)
{
    PVGASTATE       pThis      = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATECC     pThisCC    = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;

    /* Set the active cursor. */
    if (pSVGAState->Cursor.fActive)
    {
        /* We don't store the alpha flag, but we can take a guess that if
         * the old register interface was used, the cursor was B&W.
         */
        bool    fAlpha = pThis->svga.uCursorOn ? false : true;

        int rc = pThisCC->pDrv->pfnVBVAMousePointerShape(pThisCC->pDrv,
                                                         true /*fVisible*/,
                                                         fAlpha,
                                                         pSVGAState->Cursor.xHotspot,
                                                         pSVGAState->Cursor.yHotspot,
                                                         pSVGAState->Cursor.width,
                                                         pSVGAState->Cursor.height,
                                                         pSVGAState->Cursor.pData);
        AssertRC(rc);

        if (pThis->svga.uCursorOn)
            pThisCC->pDrv->pfnVBVAReportCursorPosition(pThisCC->pDrv, VBVA_CURSOR_VALID_DATA, SVGA_ID_INVALID, pThis->svga.uCursorX, pThis->svga.uCursorY);
    }

    /* If the VRAM handler should not be registered, we have to explicitly
     * unregister it here!
     */
    if (!pThis->svga.fVRAMTracking)
    {
        vgaR3UnregisterVRAMHandler(pDevIns, pThis);
    }

    /* Let the FIFO thread deal with changing the mode. */
    ASMAtomicOrU32(&pThis->svga.u32ActionFlags, VMSVGA_ACTION_CHANGEMODE);

    return VINF_SUCCESS;
}

/**
 * Portion of SVGA state which must be saved in the FIFO thread.
 */
static int vmsvgaR3SaveExecFifo(PCPDMDEVHLPR3 pHlp, PVGASTATECC pThisCC, PSSMHANDLE pSSM)
{
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;
    int             rc;

    /* Save the screen objects. */
    /* Count defined screen object. */
    uint32_t cScreens = 0;
    for (uint32_t i = 0; i < RT_ELEMENTS(pSVGAState->aScreens); ++i)
    {
         if (pSVGAState->aScreens[i].fDefined)
             ++cScreens;
    }

    rc = pHlp->pfnSSMPutU32(pSSM, cScreens);
    AssertLogRelRCReturn(rc, rc);

    for (uint32_t i = 0; i < cScreens; ++i)
    {
        VMSVGASCREENOBJECT *pScreen = &pSVGAState->aScreens[i];

        rc = pHlp->pfnSSMPutStructEx(pSSM, pScreen, sizeof(*pScreen), 0, g_aVMSVGASCREENOBJECTFields, NULL);
        AssertLogRelRCReturn(rc, rc);
    }
    return VINF_SUCCESS;
}

/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
int vmsvgaR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVGASTATE       pThis      = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATECC     pThisCC    = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;
    PCPDMDEVHLPR3   pHlp       = pDevIns->pHlpR3;
    int             rc;

    /* Save our part of the VGAState */
    rc = pHlp->pfnSSMPutStructEx(pSSM, &pThis->svga, sizeof(pThis->svga), 0, g_aVGAStateSVGAFields, NULL);
    AssertLogRelRCReturn(rc, rc);

    /* Save the framebuffer backup. */
    rc = pHlp->pfnSSMPutU32(pSSM, VMSVGA_VGA_FB_BACKUP_SIZE);
    rc = pHlp->pfnSSMPutMem(pSSM, pThisCC->svga.pbVgaFrameBufferR3, VMSVGA_VGA_FB_BACKUP_SIZE);
    AssertLogRelRCReturn(rc, rc);

    /* Save the VMSVGA state. */
    rc = pHlp->pfnSSMPutStructEx(pSSM, pSVGAState, sizeof(*pSVGAState), 0, g_aVMSVGAR3STATEFields, NULL);
    AssertLogRelRCReturn(rc, rc);

    /* Save the active cursor bitmaps. */
    if (pSVGAState->Cursor.fActive)
    {
        rc = pHlp->pfnSSMPutMem(pSSM, pSVGAState->Cursor.pData, pSVGAState->Cursor.cbData);
        AssertLogRelRCReturn(rc, rc);
    }

    /* Save the GMR state */
    rc = pHlp->pfnSSMPutU32(pSSM, pThis->svga.cGMR);
    AssertLogRelRCReturn(rc, rc);
    for (uint32_t i = 0; i < pThis->svga.cGMR; ++i)
    {
        PGMR pGMR = &pSVGAState->paGMR[i];

        rc = pHlp->pfnSSMPutStructEx(pSSM, pGMR, sizeof(*pGMR), 0, g_aGMRFields, NULL);
        AssertLogRelRCReturn(rc, rc);

        for (uint32_t j = 0; j < pGMR->numDescriptors; ++j)
        {
            rc = pHlp->pfnSSMPutStructEx(pSSM, &pGMR->paDesc[j], sizeof(pGMR->paDesc[j]), 0, g_aVMSVGAGMRDESCRIPTORFields, NULL);
            AssertLogRelRCReturn(rc, rc);
        }
    }

    /*
     * Must save some state (3D in particular) in the FIFO thread.
     */
    rc = vmsvgaR3RunExtCmdOnFifoThread(pDevIns, pThis, pThisCC, VMSVGA_FIFO_EXTCMD_SAVESTATE, pSSM, RT_INDEFINITE_WAIT);
    AssertLogRelRCReturn(rc, rc);

    return VINF_SUCCESS;
}

/**
 * Destructor for PVMSVGAR3STATE structure.
 *
 * @param   pThis          The shared VGA/VMSVGA instance data.
 * @param   pSVGAState     Pointer to the structure. It is not deallocated.
 */
static void vmsvgaR3StateTerm(PVGASTATE pThis, PVMSVGAR3STATE pSVGAState)
{
# ifndef VMSVGA_USE_EMT_HALT_CODE
    if (pSVGAState->hBusyDelayedEmts != NIL_RTSEMEVENTMULTI)
    {
        RTSemEventMultiDestroy(pSVGAState->hBusyDelayedEmts);
        pSVGAState->hBusyDelayedEmts = NIL_RTSEMEVENT;
    }
# endif

    if (pSVGAState->Cursor.fActive)
    {
        RTMemFreeZ(pSVGAState->Cursor.pData, pSVGAState->Cursor.cbData);
        pSVGAState->Cursor.pData = NULL;
        pSVGAState->Cursor.fActive = false;
    }

    if (pSVGAState->paGMR)
    {
        for (unsigned i = 0; i < pThis->svga.cGMR; ++i)
            if (pSVGAState->paGMR[i].paDesc)
                RTMemFree(pSVGAState->paGMR[i].paDesc);

        RTMemFree(pSVGAState->paGMR);
        pSVGAState->paGMR = NULL;
    }
}

/**
 * Constructor for PVMSVGAR3STATE structure.
 *
 * @returns VBox status code.
 * @param   pThis          The shared VGA/VMSVGA instance data.
 * @param   pSVGAState     Pointer to the structure. It is already allocated.
 */
static int vmsvgaR3StateInit(PVGASTATE pThis, PVMSVGAR3STATE pSVGAState)
{
    int rc = VINF_SUCCESS;
    RT_ZERO(*pSVGAState);

    pSVGAState->paGMR = (PGMR)RTMemAllocZ(pThis->svga.cGMR * sizeof(GMR));
    AssertReturn(pSVGAState->paGMR, VERR_NO_MEMORY);

# ifndef VMSVGA_USE_EMT_HALT_CODE
    /* Create semaphore for delaying EMTs wait for the FIFO to stop being busy. */
    rc = RTSemEventMultiCreate(&pSVGAState->hBusyDelayedEmts);
    AssertRCReturn(rc, rc);
# endif

    return rc;
}

/**
 * Initializes the host capabilities: registers and FIFO.
 *
 * @returns VBox status code.
 * @param   pThis     The shared VGA/VMSVGA instance data.
 * @param   pThisCC   The VGA/VMSVGA state for ring-3.
 */
static void vmsvgaR3InitCaps(PVGASTATE pThis, PVGASTATECC pThisCC)
{
    /* Register caps. */
    pThis->svga.u32RegCaps = SVGA_CAP_GMR
                           | SVGA_CAP_GMR2
                           | SVGA_CAP_CURSOR
                           | SVGA_CAP_CURSOR_BYPASS
                           | SVGA_CAP_CURSOR_BYPASS_2
                           | SVGA_CAP_EXTENDED_FIFO
                           | SVGA_CAP_IRQMASK
                           | SVGA_CAP_PITCHLOCK
                           | SVGA_CAP_RECT_COPY
                           | SVGA_CAP_TRACES
                           | SVGA_CAP_SCREEN_OBJECT_2
                           | SVGA_CAP_ALPHA_CURSOR;
# ifdef VBOX_WITH_VMSVGA3D
    pThis->svga.u32RegCaps |= SVGA_CAP_3D;
# endif

    /* Clear the FIFO. */
    RT_BZERO(pThisCC->svga.pau32FIFO, pThis->svga.cbFIFO);

    /* Setup FIFO capabilities. */
    pThisCC->svga.pau32FIFO[SVGA_FIFO_CAPABILITIES] = SVGA_FIFO_CAP_FENCE
                                                | SVGA_FIFO_CAP_CURSOR_BYPASS_3
                                                | SVGA_FIFO_CAP_GMR2
                                                | SVGA_FIFO_CAP_3D_HWVERSION_REVISED
                                                | SVGA_FIFO_CAP_SCREEN_OBJECT_2
                                                | SVGA_FIFO_CAP_RESERVE
                                                | SVGA_FIFO_CAP_PITCHLOCK;

    /* Valid with SVGA_FIFO_CAP_SCREEN_OBJECT_2 */
    pThisCC->svga.pau32FIFO[SVGA_FIFO_CURSOR_SCREEN_ID] = SVGA_ID_INVALID;
}

# ifdef VBOX_WITH_VMSVGA3D
/** Names for the vmsvga 3d capabilities, prefixed with format type hint char. */
static const char * const g_apszVmSvgaDevCapNames[] =
{
    "x3D",                           /* = 0 */
    "xMAX_LIGHTS",
    "xMAX_TEXTURES",
    "xMAX_CLIP_PLANES",
    "xVERTEX_SHADER_VERSION",
    "xVERTEX_SHADER",
    "xFRAGMENT_SHADER_VERSION",
    "xFRAGMENT_SHADER",
    "xMAX_RENDER_TARGETS",
    "xS23E8_TEXTURES",
    "xS10E5_TEXTURES",
    "xMAX_FIXED_VERTEXBLEND",
    "xD16_BUFFER_FORMAT",
    "xD24S8_BUFFER_FORMAT",
    "xD24X8_BUFFER_FORMAT",
    "xQUERY_TYPES",
    "xTEXTURE_GRADIENT_SAMPLING",
    "rMAX_POINT_SIZE",
    "xMAX_SHADER_TEXTURES",
    "xMAX_TEXTURE_WIDTH",
    "xMAX_TEXTURE_HEIGHT",
    "xMAX_VOLUME_EXTENT",
    "xMAX_TEXTURE_REPEAT",
    "xMAX_TEXTURE_ASPECT_RATIO",
    "xMAX_TEXTURE_ANISOTROPY",
    "xMAX_PRIMITIVE_COUNT",
    "xMAX_VERTEX_INDEX",
    "xMAX_VERTEX_SHADER_INSTRUCTIONS",
    "xMAX_FRAGMENT_SHADER_INSTRUCTIONS",
    "xMAX_VERTEX_SHADER_TEMPS",
    "xMAX_FRAGMENT_SHADER_TEMPS",
    "xTEXTURE_OPS",
    "xSURFACEFMT_X8R8G8B8",
    "xSURFACEFMT_A8R8G8B8",
    "xSURFACEFMT_A2R10G10B10",
    "xSURFACEFMT_X1R5G5B5",
    "xSURFACEFMT_A1R5G5B5",
    "xSURFACEFMT_A4R4G4B4",
    "xSURFACEFMT_R5G6B5",
    "xSURFACEFMT_LUMINANCE16",
    "xSURFACEFMT_LUMINANCE8_ALPHA8",
    "xSURFACEFMT_ALPHA8",
    "xSURFACEFMT_LUMINANCE8",
    "xSURFACEFMT_Z_D16",
    "xSURFACEFMT_Z_D24S8",
    "xSURFACEFMT_Z_D24X8",
    "xSURFACEFMT_DXT1",
    "xSURFACEFMT_DXT2",
    "xSURFACEFMT_DXT3",
    "xSURFACEFMT_DXT4",
    "xSURFACEFMT_DXT5",
    "xSURFACEFMT_BUMPX8L8V8U8",
    "xSURFACEFMT_A2W10V10U10",
    "xSURFACEFMT_BUMPU8V8",
    "xSURFACEFMT_Q8W8V8U8",
    "xSURFACEFMT_CxV8U8",
    "xSURFACEFMT_R_S10E5",
    "xSURFACEFMT_R_S23E8",
    "xSURFACEFMT_RG_S10E5",
    "xSURFACEFMT_RG_S23E8",
    "xSURFACEFMT_ARGB_S10E5",
    "xSURFACEFMT_ARGB_S23E8",
    "xMISSING62",
    "xMAX_VERTEX_SHADER_TEXTURES",
    "xMAX_SIMULTANEOUS_RENDER_TARGETS",
    "xSURFACEFMT_V16U16",
    "xSURFACEFMT_G16R16",
    "xSURFACEFMT_A16B16G16R16",
    "xSURFACEFMT_UYVY",
    "xSURFACEFMT_YUY2",
    "xMULTISAMPLE_NONMASKABLESAMPLES",
    "xMULTISAMPLE_MASKABLESAMPLES",
    "xALPHATOCOVERAGE",
    "xSUPERSAMPLE",
    "xAUTOGENMIPMAPS",
    "xSURFACEFMT_NV12",
    "xSURFACEFMT_AYUV",
    "xMAX_CONTEXT_IDS",
    "xMAX_SURFACE_IDS",
    "xSURFACEFMT_Z_DF16",
    "xSURFACEFMT_Z_DF24",
    "xSURFACEFMT_Z_D24S8_INT",
    "xSURFACEFMT_BC4_UNORM",
    "xSURFACEFMT_BC5_UNORM", /* 83 */
};

/**
 * Initializes the host 3D capabilities in FIFO.
 *
 * @returns VBox status code.
 * @param   pThis     The shared VGA/VMSVGA instance data.
 * @param   pThisCC   The VGA/VMSVGA state for ring-3.
 */
static void vmsvgaR3InitFifo3DCaps(PVGASTATECC pThisCC)
{
    /** @todo Probably query the capabilities once and cache in a memory buffer. */
    bool              fSavedBuffering = RTLogRelSetBuffering(true);
    SVGA3dCapsRecord *pCaps;
    SVGA3dCapPair    *pData;
    uint32_t          idxCap  = 0;

    /* 3d hardware version; latest and greatest */
    pThisCC->svga.pau32FIFO[SVGA_FIFO_3D_HWVERSION_REVISED] = SVGA3D_HWVERSION_CURRENT;
    pThisCC->svga.pau32FIFO[SVGA_FIFO_3D_HWVERSION]         = SVGA3D_HWVERSION_CURRENT;

    pCaps = (SVGA3dCapsRecord *)&pThisCC->svga.pau32FIFO[SVGA_FIFO_3D_CAPS];
    pCaps->header.type   = SVGA3DCAPS_RECORD_DEVCAPS;
    pData = (SVGA3dCapPair *)&pCaps->data;

    /* Fill out all 3d capabilities. */
    for (unsigned i = 0; i < SVGA3D_DEVCAP_MAX; i++)
    {
        uint32_t val = 0;

        int rc = vmsvga3dQueryCaps(pThisCC, i, &val);
        if (RT_SUCCESS(rc))
        {
            pData[idxCap][0] = i;
            pData[idxCap][1] = val;
            idxCap++;
            if (g_apszVmSvgaDevCapNames[i][0] == 'x')
                LogRel(("VMSVGA3d: cap[%u]=%#010x {%s}\n", i, val, &g_apszVmSvgaDevCapNames[i][1]));
            else
                LogRel(("VMSVGA3d: cap[%u]=%d.%04u {%s}\n", i, (int)*(float *)&val, (unsigned)(*(float *)&val * 10000) % 10000,
                        &g_apszVmSvgaDevCapNames[i][1]));
        }
        else
            LogRel(("VMSVGA3d: cap[%u]=failed rc=%Rrc! {%s}\n", i, rc, &g_apszVmSvgaDevCapNames[i][1]));
    }
    pCaps->header.length = (sizeof(pCaps->header) + idxCap * sizeof(SVGA3dCapPair)) / sizeof(uint32_t);
    pCaps = (SVGA3dCapsRecord *)((uint32_t *)pCaps + pCaps->header.length);

    /* Mark end of record array. */
    pCaps->header.length = 0;

    RTLogRelSetBuffering(fSavedBuffering);
}

# endif

/**
 * Resets the SVGA hardware state
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 */
int vmsvgaR3Reset(PPDMDEVINS pDevIns)
{
    PVGASTATE       pThis      = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATECC     pThisCC    = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
    PVMSVGAR3STATE  pSVGAState = pThisCC->svga.pSvgaR3State;

    /* Reset before init? */
    if (!pSVGAState)
        return VINF_SUCCESS;

    Log(("vmsvgaR3Reset\n"));

    /* Reset the FIFO processing as well as the 3d state (if we have one). */
    pThisCC->svga.pau32FIFO[SVGA_FIFO_NEXT_CMD] = pThisCC->svga.pau32FIFO[SVGA_FIFO_STOP] = 0; /** @todo should probably let the FIFO thread do this ... */
    int rc = vmsvgaR3RunExtCmdOnFifoThread(pDevIns, pThis, pThisCC, VMSVGA_FIFO_EXTCMD_RESET, NULL /*pvParam*/, 10000 /*ms*/);

    /* Reset other stuff. */
    pThis->svga.cScratchRegion = VMSVGA_SCRATCH_SIZE;
    RT_ZERO(pThis->svga.au32ScratchRegion);

    vmsvgaR3StateTerm(pThis, pThisCC->svga.pSvgaR3State);
    vmsvgaR3StateInit(pThis, pThisCC->svga.pSvgaR3State);

    RT_BZERO(pThisCC->svga.pbVgaFrameBufferR3, VMSVGA_VGA_FB_BACKUP_SIZE);

    /* Initialize FIFO and register capabilities. */
    vmsvgaR3InitCaps(pThis, pThisCC);

# ifdef VBOX_WITH_VMSVGA3D
    if (pThis->svga.f3DEnabled)
        vmsvgaR3InitFifo3DCaps(pThisCC);
# endif

    /* VRAM tracking is enabled by default during bootup. */
    pThis->svga.fVRAMTracking = true;
    pThis->svga.fEnabled      = false;

    /* Invalidate current settings. */
    pThis->svga.uWidth       = VMSVGA_VAL_UNINITIALIZED;
    pThis->svga.uHeight      = VMSVGA_VAL_UNINITIALIZED;
    pThis->svga.uBpp         = pThis->svga.uHostBpp;
    pThis->svga.cbScanline   = 0;
    pThis->svga.u32PitchLock = 0;

    return rc;
}

/**
 * Cleans up the SVGA hardware state
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 */
int vmsvgaR3Destruct(PPDMDEVINS pDevIns)
{
    PVGASTATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATECC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);

    /*
     * Ask the FIFO thread to terminate the 3d state and then terminate it.
     */
    if (pThisCC->svga.pFIFOIOThread)
    {
        int rc = vmsvgaR3RunExtCmdOnFifoThread(pDevIns, pThis, pThisCC,  VMSVGA_FIFO_EXTCMD_TERMINATE,
                                               NULL /*pvParam*/, 30000 /*ms*/);
        AssertLogRelRC(rc);

        rc = PDMDevHlpThreadDestroy(pDevIns, pThisCC->svga.pFIFOIOThread, NULL);
        AssertLogRelRC(rc);
        pThisCC->svga.pFIFOIOThread = NULL;
    }

    /*
     * Destroy the special SVGA state.
     */
    if (pThisCC->svga.pSvgaR3State)
    {
        vmsvgaR3StateTerm(pThis, pThisCC->svga.pSvgaR3State);

        RTMemFree(pThisCC->svga.pSvgaR3State);
        pThisCC->svga.pSvgaR3State = NULL;
    }

    /*
     * Free our resources residing in the VGA state.
     */
    if (pThisCC->svga.pbVgaFrameBufferR3)
    {
        RTMemFree(pThisCC->svga.pbVgaFrameBufferR3);
        pThisCC->svga.pbVgaFrameBufferR3 = NULL;
    }
    if (pThisCC->svga.hFIFOExtCmdSem != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(pThisCC->svga.hFIFOExtCmdSem);
        pThisCC->svga.hFIFOExtCmdSem = NIL_RTSEMEVENT;
    }
    if (pThis->svga.hFIFORequestSem != NIL_SUPSEMEVENT)
    {
        PDMDevHlpSUPSemEventClose(pDevIns, pThis->svga.hFIFORequestSem);
        pThis->svga.hFIFORequestSem = NIL_SUPSEMEVENT;
    }

    return VINF_SUCCESS;
}

/**
 * Initialize the SVGA hardware state
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 */
int vmsvgaR3Init(PPDMDEVINS pDevIns)
{
    PVGASTATE       pThis   = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATECC     pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
    PVMSVGAR3STATE  pSVGAState;
    int             rc;

    pThis->svga.cScratchRegion = VMSVGA_SCRATCH_SIZE;
    memset(pThis->svga.au32ScratchRegion, 0, sizeof(pThis->svga.au32ScratchRegion));

    pThis->svga.cGMR = VMSVGA_MAX_GMR_IDS;

    /* Necessary for creating a backup of the text mode frame buffer when switching into svga mode. */
    pThisCC->svga.pbVgaFrameBufferR3 = (uint8_t *)RTMemAllocZ(VMSVGA_VGA_FB_BACKUP_SIZE);
    AssertReturn(pThisCC->svga.pbVgaFrameBufferR3, VERR_NO_MEMORY);

    /* Create event semaphore. */
    rc = PDMDevHlpSUPSemEventCreate(pDevIns, &pThis->svga.hFIFORequestSem);
    AssertRCReturn(rc, rc);

    /* Create event semaphore. */
    rc = RTSemEventCreate(&pThisCC->svga.hFIFOExtCmdSem);
    AssertRCReturn(rc, rc);

    pThisCC->svga.pSvgaR3State = (PVMSVGAR3STATE)RTMemAlloc(sizeof(VMSVGAR3STATE));
    AssertReturn(pThisCC->svga.pSvgaR3State, VERR_NO_MEMORY);

    rc = vmsvgaR3StateInit(pThis, pThisCC->svga.pSvgaR3State);
    AssertMsgRCReturn(rc, ("Failed to create pSvgaR3State.\n"), rc);

    pSVGAState = pThisCC->svga.pSvgaR3State;

    /* Initialize FIFO and register capabilities. */
    vmsvgaR3InitCaps(pThis, pThisCC);

# ifdef VBOX_WITH_VMSVGA3D
    if (pThis->svga.f3DEnabled)
    {
        rc = vmsvga3dInit(pDevIns, pThis, pThisCC);
        if (RT_FAILURE(rc))
        {
            LogRel(("VMSVGA3d: 3D support disabled! (vmsvga3dInit -> %Rrc)\n", rc));
            pThis->svga.f3DEnabled = false;
        }
    }
# endif
    /* VRAM tracking is enabled by default during bootup. */
    pThis->svga.fVRAMTracking = true;

    /* Set up the host bpp. This value is as a default for the programmable
     * bpp value. On old implementations, SVGA_REG_HOST_BITS_PER_PIXEL did not
     * exist and SVGA_REG_BITS_PER_PIXEL was read-only, returning what was later
     * separated as SVGA_REG_HOST_BITS_PER_PIXEL.
     *
     * NB: The driver cBits value is currently constant for the lifetime of the
     * VM. If that changes, the host bpp logic might need revisiting.
     */
    pThis->svga.uHostBpp = (pThisCC->pDrv->cBits + 7) & ~7;

    /* Invalidate current settings. */
    pThis->svga.uWidth     = VMSVGA_VAL_UNINITIALIZED;
    pThis->svga.uHeight    = VMSVGA_VAL_UNINITIALIZED;
    pThis->svga.uBpp       = pThis->svga.uHostBpp;
    pThis->svga.cbScanline = 0;

    pThis->svga.u32MaxWidth  = VBE_DISPI_MAX_YRES;
    pThis->svga.u32MaxHeight = VBE_DISPI_MAX_XRES;
    while (pThis->svga.u32MaxWidth * pThis->svga.u32MaxHeight * 4 /* 32 bpp */ > pThis->vram_size)
    {
        pThis->svga.u32MaxWidth  -= 256;
        pThis->svga.u32MaxHeight -= 256;
    }
    Log(("VMSVGA: Maximum size (%d,%d)\n", pThis->svga.u32MaxWidth, pThis->svga.u32MaxHeight));

# ifdef DEBUG_GMR_ACCESS
    /* Register the GMR access handler type. */
    rc = PGMR3HandlerPhysicalTypeRegister(PDMDevHlpGetVM(pDevIns), PGMPHYSHANDLERKIND_WRITE,
                                          vmsvgaR3GmrAccessHandler,
                                          NULL, NULL, NULL,
                                          NULL, NULL, NULL,
                                          "VMSVGA GMR", &pThis->svga.hGmrAccessHandlerType);
    AssertRCReturn(rc, rc);
# endif

# if defined(VMSVGA_USE_FIFO_ACCESS_HANDLER) || defined(DEBUG_FIFO_ACCESS)
    /* Register the FIFO access handler type.  In addition to
       debugging FIFO access, this is also used to facilitate
       extended fifo thread sleeps. */
    rc = PGMR3HandlerPhysicalTypeRegister(PDMDevHlpGetVM(pDevIns),
#  ifdef DEBUG_FIFO_ACCESS
                                          PGMPHYSHANDLERKIND_ALL,
#  else
                                          PGMPHYSHANDLERKIND_WRITE,
#  endif
                                          vmsvgaR3FifoAccessHandler,
                                          NULL, NULL, NULL,
                                          NULL, NULL, NULL,
                                          "VMSVGA FIFO", &pThis->svga.hFifoAccessHandlerType);
    AssertRCReturn(rc, rc);
# endif

    /* Create the async IO thread. */
    rc = PDMDevHlpThreadCreate(pDevIns, &pThisCC->svga.pFIFOIOThread, pThis, vmsvgaR3FifoLoop, vmsvgaR3FifoLoopWakeUp, 0,
                               RTTHREADTYPE_IO, "VMSVGA FIFO");
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("%s: Async IO Thread creation for FIFO handling failed rc=%d\n", __FUNCTION__, rc));
        return rc;
    }

    /*
     * Statistics.
     */
# define REG_CNT(a_pvSample, a_pszName, a_pszDesc) \
        PDMDevHlpSTAMRegister(pDevIns, (a_pvSample), STAMTYPE_COUNTER, a_pszName, STAMUNIT_OCCURENCES, a_pszDesc)
# define REG_PRF(a_pvSample, a_pszName, a_pszDesc) \
        PDMDevHlpSTAMRegister(pDevIns, (a_pvSample), STAMTYPE_PROFILE, a_pszName, STAMUNIT_TICKS_PER_CALL, a_pszDesc)
# ifdef VBOX_WITH_STATISTICS
    REG_PRF(&pSVGAState->StatR3Cmd3dDrawPrimitivesProf,   "VMSVGA/Cmd/3dDrawPrimitivesProf",       "Profiling of SVGA_3D_CMD_DRAW_PRIMITIVES.");
    REG_PRF(&pSVGAState->StatR3Cmd3dPresentProf,          "VMSVGA/Cmd/3dPresentProfBoth",          "Profiling of SVGA_3D_CMD_PRESENT and SVGA_3D_CMD_PRESENT_READBACK.");
    REG_PRF(&pSVGAState->StatR3Cmd3dSurfaceDmaProf,       "VMSVGA/Cmd/3dSurfaceDmaProf",           "Profiling of SVGA_3D_CMD_SURFACE_DMA.");
# endif
    REG_PRF(&pSVGAState->StatR3Cmd3dBlitSurfaceToScreenProf, "VMSVGA/Cmd/3dBlitSurfaceToScreenProf", "Profiling of SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN.");
    REG_CNT(&pSVGAState->StatR3Cmd3dActivateSurface,      "VMSVGA/Cmd/3dActivateSurface",          "SVGA_3D_CMD_ACTIVATE_SURFACE");
    REG_CNT(&pSVGAState->StatR3Cmd3dBeginQuery,           "VMSVGA/Cmd/3dBeginQuery",               "SVGA_3D_CMD_BEGIN_QUERY");
    REG_CNT(&pSVGAState->StatR3Cmd3dClear,                "VMSVGA/Cmd/3dClear",                    "SVGA_3D_CMD_CLEAR");
    REG_CNT(&pSVGAState->StatR3Cmd3dContextDefine,        "VMSVGA/Cmd/3dContextDefine",            "SVGA_3D_CMD_CONTEXT_DEFINE");
    REG_CNT(&pSVGAState->StatR3Cmd3dContextDestroy,       "VMSVGA/Cmd/3dContextDestroy",           "SVGA_3D_CMD_CONTEXT_DESTROY");
    REG_CNT(&pSVGAState->StatR3Cmd3dDeactivateSurface,    "VMSVGA/Cmd/3dDeactivateSurface",        "SVGA_3D_CMD_DEACTIVATE_SURFACE");
    REG_CNT(&pSVGAState->StatR3Cmd3dDrawPrimitives,       "VMSVGA/Cmd/3dDrawPrimitives",           "SVGA_3D_CMD_DRAW_PRIMITIVES");
    REG_CNT(&pSVGAState->StatR3Cmd3dEndQuery,             "VMSVGA/Cmd/3dEndQuery",                 "SVGA_3D_CMD_END_QUERY");
    REG_CNT(&pSVGAState->StatR3Cmd3dGenerateMipmaps,      "VMSVGA/Cmd/3dGenerateMipmaps",          "SVGA_3D_CMD_GENERATE_MIPMAPS");
    REG_CNT(&pSVGAState->StatR3Cmd3dPresent,              "VMSVGA/Cmd/3dPresent",                  "SVGA_3D_CMD_PRESENT");
    REG_CNT(&pSVGAState->StatR3Cmd3dPresentReadBack,      "VMSVGA/Cmd/3dPresentReadBack",          "SVGA_3D_CMD_PRESENT_READBACK");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetClipPlane,         "VMSVGA/Cmd/3dSetClipPlane",             "SVGA_3D_CMD_SETCLIPPLANE");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetLightData,         "VMSVGA/Cmd/3dSetLightData",             "SVGA_3D_CMD_SETLIGHTDATA");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetLightEnable,       "VMSVGA/Cmd/3dSetLightEnable",           "SVGA_3D_CMD_SETLIGHTENABLE");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetMaterial,          "VMSVGA/Cmd/3dSetMaterial",              "SVGA_3D_CMD_SETMATERIAL");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetRenderState,       "VMSVGA/Cmd/3dSetRenderState",           "SVGA_3D_CMD_SETRENDERSTATE");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetRenderTarget,      "VMSVGA/Cmd/3dSetRenderTarget",          "SVGA_3D_CMD_SETRENDERTARGET");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetScissorRect,       "VMSVGA/Cmd/3dSetScissorRect",           "SVGA_3D_CMD_SETSCISSORRECT");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetShader,            "VMSVGA/Cmd/3dSetShader",                "SVGA_3D_CMD_SET_SHADER");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetShaderConst,       "VMSVGA/Cmd/3dSetShaderConst",           "SVGA_3D_CMD_SET_SHADER_CONST");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetTextureState,      "VMSVGA/Cmd/3dSetTextureState",          "SVGA_3D_CMD_SETTEXTURESTATE");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetTransform,         "VMSVGA/Cmd/3dSetTransform",             "SVGA_3D_CMD_SETTRANSFORM");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetViewPort,          "VMSVGA/Cmd/3dSetViewPort",              "SVGA_3D_CMD_SETVIEWPORT");
    REG_CNT(&pSVGAState->StatR3Cmd3dSetZRange,            "VMSVGA/Cmd/3dSetZRange",                "SVGA_3D_CMD_SETZRANGE");
    REG_CNT(&pSVGAState->StatR3Cmd3dShaderDefine,         "VMSVGA/Cmd/3dShaderDefine",             "SVGA_3D_CMD_SHADER_DEFINE");
    REG_CNT(&pSVGAState->StatR3Cmd3dShaderDestroy,        "VMSVGA/Cmd/3dShaderDestroy",            "SVGA_3D_CMD_SHADER_DESTROY");
    REG_CNT(&pSVGAState->StatR3Cmd3dSurfaceCopy,          "VMSVGA/Cmd/3dSurfaceCopy",              "SVGA_3D_CMD_SURFACE_COPY");
    REG_CNT(&pSVGAState->StatR3Cmd3dSurfaceDefine,        "VMSVGA/Cmd/3dSurfaceDefine",            "SVGA_3D_CMD_SURFACE_DEFINE");
    REG_CNT(&pSVGAState->StatR3Cmd3dSurfaceDefineV2,      "VMSVGA/Cmd/3dSurfaceDefineV2",          "SVGA_3D_CMD_SURFACE_DEFINE_V2");
    REG_CNT(&pSVGAState->StatR3Cmd3dSurfaceDestroy,       "VMSVGA/Cmd/3dSurfaceDestroy",           "SVGA_3D_CMD_SURFACE_DESTROY");
    REG_CNT(&pSVGAState->StatR3Cmd3dSurfaceDma,           "VMSVGA/Cmd/3dSurfaceDma",               "SVGA_3D_CMD_SURFACE_DMA");
    REG_CNT(&pSVGAState->StatR3Cmd3dSurfaceScreen,        "VMSVGA/Cmd/3dSurfaceScreen",            "SVGA_3D_CMD_SURFACE_SCREEN");
    REG_CNT(&pSVGAState->StatR3Cmd3dSurfaceStretchBlt,    "VMSVGA/Cmd/3dSurfaceStretchBlt",        "SVGA_3D_CMD_SURFACE_STRETCHBLT");
    REG_CNT(&pSVGAState->StatR3Cmd3dWaitForQuery,         "VMSVGA/Cmd/3dWaitForQuery",             "SVGA_3D_CMD_WAIT_FOR_QUERY");
    REG_CNT(&pSVGAState->StatR3CmdAnnotationCopy,         "VMSVGA/Cmd/AnnotationCopy",             "SVGA_CMD_ANNOTATION_COPY");
    REG_CNT(&pSVGAState->StatR3CmdAnnotationFill,         "VMSVGA/Cmd/AnnotationFill",             "SVGA_CMD_ANNOTATION_FILL");
    REG_CNT(&pSVGAState->StatR3CmdBlitGmrFbToScreen,      "VMSVGA/Cmd/BlitGmrFbToScreen",          "SVGA_CMD_BLIT_GMRFB_TO_SCREEN");
    REG_CNT(&pSVGAState->StatR3CmdBlitScreentoGmrFb,      "VMSVGA/Cmd/BlitScreentoGmrFb",          "SVGA_CMD_BLIT_SCREEN_TO_GMRFB");
    REG_CNT(&pSVGAState->StatR3CmdDefineAlphaCursor,      "VMSVGA/Cmd/DefineAlphaCursor",          "SVGA_CMD_DEFINE_ALPHA_CURSOR");
    REG_CNT(&pSVGAState->StatR3CmdDefineCursor,           "VMSVGA/Cmd/DefineCursor",               "SVGA_CMD_DEFINE_CURSOR");
    REG_CNT(&pSVGAState->StatR3CmdMoveCursor,             "VMSVGA/Cmd/MoveCursor",                 "SVGA_CMD_MOVE_CURSOR");
    REG_CNT(&pSVGAState->StatR3CmdDisplayCursor,          "VMSVGA/Cmd/DisplayCursor",              "SVGA_CMD_DISPLAY_CURSOR");
    REG_CNT(&pSVGAState->StatR3CmdRectFill,               "VMSVGA/Cmd/RectFill",                   "SVGA_CMD_RECT_FILL");
    REG_CNT(&pSVGAState->StatR3CmdRectCopy,               "VMSVGA/Cmd/RectCopy",                   "SVGA_CMD_RECT_COPY");
    REG_CNT(&pSVGAState->StatR3CmdRectRopCopy,            "VMSVGA/Cmd/RectRopCopy",                "SVGA_CMD_RECT_ROP_COPY");
    REG_CNT(&pSVGAState->StatR3CmdDefineGmr2,             "VMSVGA/Cmd/DefineGmr2",                 "SVGA_CMD_DEFINE_GMR2");
    REG_CNT(&pSVGAState->StatR3CmdDefineGmr2Free,         "VMSVGA/Cmd/DefineGmr2/Free",            "Number of SVGA_CMD_DEFINE_GMR2 commands that only frees.");
    REG_CNT(&pSVGAState->StatR3CmdDefineGmr2Modify,       "VMSVGA/Cmd/DefineGmr2/Modify",          "Number of SVGA_CMD_DEFINE_GMR2 commands that redefines a non-free GMR.");
    REG_CNT(&pSVGAState->StatR3CmdDefineGmrFb,            "VMSVGA/Cmd/DefineGmrFb",                "SVGA_CMD_DEFINE_GMRFB");
    REG_CNT(&pSVGAState->StatR3CmdDefineScreen,           "VMSVGA/Cmd/DefineScreen",               "SVGA_CMD_DEFINE_SCREEN");
    REG_CNT(&pSVGAState->StatR3CmdDestroyScreen,          "VMSVGA/Cmd/DestroyScreen",              "SVGA_CMD_DESTROY_SCREEN");
    REG_CNT(&pSVGAState->StatR3CmdEscape,                 "VMSVGA/Cmd/Escape",                     "SVGA_CMD_ESCAPE");
    REG_CNT(&pSVGAState->StatR3CmdFence,                  "VMSVGA/Cmd/Fence",                      "SVGA_CMD_FENCE");
    REG_CNT(&pSVGAState->StatR3CmdInvalidCmd,             "VMSVGA/Cmd/InvalidCmd",                 "SVGA_CMD_INVALID_CMD");
    REG_CNT(&pSVGAState->StatR3CmdRemapGmr2,              "VMSVGA/Cmd/RemapGmr2",                  "SVGA_CMD_REMAP_GMR2");
    REG_CNT(&pSVGAState->StatR3CmdRemapGmr2Modify,        "VMSVGA/Cmd/RemapGmr2/Modify",           "Number of SVGA_CMD_REMAP_GMR2 commands that modifies rather than complete the definition of a GMR.");
    REG_CNT(&pSVGAState->StatR3CmdUpdate,                 "VMSVGA/Cmd/Update",                     "SVGA_CMD_UPDATE");
    REG_CNT(&pSVGAState->StatR3CmdUpdateVerbose,          "VMSVGA/Cmd/UpdateVerbose",              "SVGA_CMD_UPDATE_VERBOSE");

    REG_CNT(&pSVGAState->StatR3RegConfigDoneWr,           "VMSVGA/Reg/ConfigDoneWrite",            "SVGA_REG_CONFIG_DONE writes");
    REG_CNT(&pSVGAState->StatR3RegGmrDescriptorWr,        "VMSVGA/Reg/GmrDescriptorWrite",         "SVGA_REG_GMR_DESCRIPTOR writes");
    REG_CNT(&pSVGAState->StatR3RegGmrDescriptorWrErrors,  "VMSVGA/Reg/GmrDescriptorWrite/Errors",  "Number of erroneous SVGA_REG_GMR_DESCRIPTOR commands.");
    REG_CNT(&pSVGAState->StatR3RegGmrDescriptorWrFree,    "VMSVGA/Reg/GmrDescriptorWrite/Free",    "Number of SVGA_REG_GMR_DESCRIPTOR commands only freeing the GMR.");
    REG_CNT(&pThis->svga.StatRegBitsPerPixelWr,           "VMSVGA/Reg/BitsPerPixelWrite",          "SVGA_REG_BITS_PER_PIXEL writes.");
    REG_CNT(&pThis->svga.StatRegBusyWr,                   "VMSVGA/Reg/BusyWrite",                  "SVGA_REG_BUSY writes.");
    REG_CNT(&pThis->svga.StatRegCursorXWr,                "VMSVGA/Reg/CursorXWrite",               "SVGA_REG_CURSOR_X writes.");
    REG_CNT(&pThis->svga.StatRegCursorYWr,                "VMSVGA/Reg/CursorYWrite",               "SVGA_REG_CURSOR_Y writes.");
    REG_CNT(&pThis->svga.StatRegCursorIdWr,               "VMSVGA/Reg/CursorIdWrite",              "SVGA_REG_CURSOR_ID writes.");
    REG_CNT(&pThis->svga.StatRegCursorOnWr,               "VMSVGA/Reg/CursorOnWrite",              "SVGA_REG_CURSOR_ON writes.");
    REG_CNT(&pThis->svga.StatRegDepthWr,                  "VMSVGA/Reg/DepthWrite",                 "SVGA_REG_DEPTH writes.");
    REG_CNT(&pThis->svga.StatRegDisplayHeightWr,          "VMSVGA/Reg/DisplayHeightWrite",         "SVGA_REG_DISPLAY_HEIGHT writes.");
    REG_CNT(&pThis->svga.StatRegDisplayIdWr,              "VMSVGA/Reg/DisplayIdWrite",             "SVGA_REG_DISPLAY_ID writes.");
    REG_CNT(&pThis->svga.StatRegDisplayIsPrimaryWr,       "VMSVGA/Reg/DisplayIsPrimaryWrite",      "SVGA_REG_DISPLAY_IS_PRIMARY writes.");
    REG_CNT(&pThis->svga.StatRegDisplayPositionXWr,       "VMSVGA/Reg/DisplayPositionXWrite",      "SVGA_REG_DISPLAY_POSITION_X writes.");
    REG_CNT(&pThis->svga.StatRegDisplayPositionYWr,       "VMSVGA/Reg/DisplayPositionYWrite",      "SVGA_REG_DISPLAY_POSITION_Y writes.");
    REG_CNT(&pThis->svga.StatRegDisplayWidthWr,           "VMSVGA/Reg/DisplayWidthWrite",          "SVGA_REG_DISPLAY_WIDTH writes.");
    REG_CNT(&pThis->svga.StatRegEnableWr,                 "VMSVGA/Reg/EnableWrite",                "SVGA_REG_ENABLE writes.");
    REG_CNT(&pThis->svga.StatRegGmrIdWr,                  "VMSVGA/Reg/GmrIdWrite",                 "SVGA_REG_GMR_ID writes.");
    REG_CNT(&pThis->svga.StatRegGuestIdWr,                "VMSVGA/Reg/GuestIdWrite",               "SVGA_REG_GUEST_ID writes.");
    REG_CNT(&pThis->svga.StatRegHeightWr,                 "VMSVGA/Reg/HeightWrite",                "SVGA_REG_HEIGHT writes.");
    REG_CNT(&pThis->svga.StatRegIdWr,                     "VMSVGA/Reg/IdWrite",                    "SVGA_REG_ID writes.");
    REG_CNT(&pThis->svga.StatRegIrqMaskWr,                "VMSVGA/Reg/IrqMaskWrite",               "SVGA_REG_IRQMASK writes.");
    REG_CNT(&pThis->svga.StatRegNumDisplaysWr,            "VMSVGA/Reg/NumDisplaysWrite",           "SVGA_REG_NUM_DISPLAYS writes.");
    REG_CNT(&pThis->svga.StatRegNumGuestDisplaysWr,       "VMSVGA/Reg/NumGuestDisplaysWrite",      "SVGA_REG_NUM_GUEST_DISPLAYS writes.");
    REG_CNT(&pThis->svga.StatRegPaletteWr,                "VMSVGA/Reg/PaletteWrite",               "SVGA_PALETTE_XXXX writes.");
    REG_CNT(&pThis->svga.StatRegPitchLockWr,              "VMSVGA/Reg/PitchLockWrite",             "SVGA_REG_PITCHLOCK writes.");
    REG_CNT(&pThis->svga.StatRegPseudoColorWr,            "VMSVGA/Reg/PseudoColorWrite",           "SVGA_REG_PSEUDOCOLOR writes.");
    REG_CNT(&pThis->svga.StatRegReadOnlyWr,               "VMSVGA/Reg/ReadOnlyWrite",              "Read-only SVGA_REG_XXXX writes.");
    REG_CNT(&pThis->svga.StatRegScratchWr,                "VMSVGA/Reg/ScratchWrite",               "SVGA_REG_SCRATCH_XXXX writes.");
    REG_CNT(&pThis->svga.StatRegSyncWr,                   "VMSVGA/Reg/SyncWrite",                  "SVGA_REG_SYNC writes.");
    REG_CNT(&pThis->svga.StatRegTopWr,                    "VMSVGA/Reg/TopWrite",                   "SVGA_REG_TOP writes.");
    REG_CNT(&pThis->svga.StatRegTracesWr,                 "VMSVGA/Reg/TracesWrite",                "SVGA_REG_TRACES writes.");
    REG_CNT(&pThis->svga.StatRegUnknownWr,                "VMSVGA/Reg/UnknownWrite",               "Writes to unknown register.");
    REG_CNT(&pThis->svga.StatRegWidthWr,                  "VMSVGA/Reg/WidthWrite",                 "SVGA_REG_WIDTH writes.");

    REG_CNT(&pThis->svga.StatRegBitsPerPixelRd,           "VMSVGA/Reg/BitsPerPixelRead",           "SVGA_REG_BITS_PER_PIXEL reads.");
    REG_CNT(&pThis->svga.StatRegBlueMaskRd,               "VMSVGA/Reg/BlueMaskRead",               "SVGA_REG_BLUE_MASK reads.");
    REG_CNT(&pThis->svga.StatRegBusyRd,                   "VMSVGA/Reg/BusyRead",                   "SVGA_REG_BUSY reads.");
    REG_CNT(&pThis->svga.StatRegBytesPerLineRd,           "VMSVGA/Reg/BytesPerLineRead",           "SVGA_REG_BYTES_PER_LINE reads.");
    REG_CNT(&pThis->svga.StatRegCapabilitesRd,            "VMSVGA/Reg/CapabilitesRead",            "SVGA_REG_CAPABILITIES reads.");
    REG_CNT(&pThis->svga.StatRegConfigDoneRd,             "VMSVGA/Reg/ConfigDoneRead",             "SVGA_REG_CONFIG_DONE reads.");
    REG_CNT(&pThis->svga.StatRegCursorXRd,                "VMSVGA/Reg/CursorXRead",                "SVGA_REG_CURSOR_X reads.");
    REG_CNT(&pThis->svga.StatRegCursorYRd,                "VMSVGA/Reg/CursorYRead",                "SVGA_REG_CURSOR_Y reads.");
    REG_CNT(&pThis->svga.StatRegCursorIdRd,               "VMSVGA/Reg/CursorIdRead",               "SVGA_REG_CURSOR_ID reads.");
    REG_CNT(&pThis->svga.StatRegCursorOnRd,               "VMSVGA/Reg/CursorOnRead",               "SVGA_REG_CURSOR_ON reads.");
    REG_CNT(&pThis->svga.StatRegDepthRd,                  "VMSVGA/Reg/DepthRead",                  "SVGA_REG_DEPTH reads.");
    REG_CNT(&pThis->svga.StatRegDisplayHeightRd,          "VMSVGA/Reg/DisplayHeightRead",          "SVGA_REG_DISPLAY_HEIGHT reads.");
    REG_CNT(&pThis->svga.StatRegDisplayIdRd,              "VMSVGA/Reg/DisplayIdRead",              "SVGA_REG_DISPLAY_ID reads.");
    REG_CNT(&pThis->svga.StatRegDisplayIsPrimaryRd,       "VMSVGA/Reg/DisplayIsPrimaryRead",       "SVGA_REG_DISPLAY_IS_PRIMARY reads.");
    REG_CNT(&pThis->svga.StatRegDisplayPositionXRd,       "VMSVGA/Reg/DisplayPositionXRead",       "SVGA_REG_DISPLAY_POSITION_X reads.");
    REG_CNT(&pThis->svga.StatRegDisplayPositionYRd,       "VMSVGA/Reg/DisplayPositionYRead",       "SVGA_REG_DISPLAY_POSITION_Y reads.");
    REG_CNT(&pThis->svga.StatRegDisplayWidthRd,           "VMSVGA/Reg/DisplayWidthRead",           "SVGA_REG_DISPLAY_WIDTH reads.");
    REG_CNT(&pThis->svga.StatRegEnableRd,                 "VMSVGA/Reg/EnableRead",                 "SVGA_REG_ENABLE reads.");
    REG_CNT(&pThis->svga.StatRegFbOffsetRd,               "VMSVGA/Reg/FbOffsetRead",               "SVGA_REG_FB_OFFSET reads.");
    REG_CNT(&pThis->svga.StatRegFbSizeRd,                 "VMSVGA/Reg/FbSizeRead",                 "SVGA_REG_FB_SIZE reads.");
    REG_CNT(&pThis->svga.StatRegFbStartRd,                "VMSVGA/Reg/FbStartRead",                "SVGA_REG_FB_START reads.");
    REG_CNT(&pThis->svga.StatRegGmrIdRd,                  "VMSVGA/Reg/GmrIdRead",                  "SVGA_REG_GMR_ID reads.");
    REG_CNT(&pThis->svga.StatRegGmrMaxDescriptorLengthRd, "VMSVGA/Reg/GmrMaxDescriptorLengthRead", "SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH reads.");
    REG_CNT(&pThis->svga.StatRegGmrMaxIdsRd,              "VMSVGA/Reg/GmrMaxIdsRead",              "SVGA_REG_GMR_MAX_IDS reads.");
    REG_CNT(&pThis->svga.StatRegGmrsMaxPagesRd,           "VMSVGA/Reg/GmrsMaxPagesRead",           "SVGA_REG_GMRS_MAX_PAGES reads.");
    REG_CNT(&pThis->svga.StatRegGreenMaskRd,              "VMSVGA/Reg/GreenMaskRead",              "SVGA_REG_GREEN_MASK reads.");
    REG_CNT(&pThis->svga.StatRegGuestIdRd,                "VMSVGA/Reg/GuestIdRead",                "SVGA_REG_GUEST_ID reads.");
    REG_CNT(&pThis->svga.StatRegHeightRd,                 "VMSVGA/Reg/HeightRead",                 "SVGA_REG_HEIGHT reads.");
    REG_CNT(&pThis->svga.StatRegHostBitsPerPixelRd,       "VMSVGA/Reg/HostBitsPerPixelRead",       "SVGA_REG_HOST_BITS_PER_PIXEL reads.");
    REG_CNT(&pThis->svga.StatRegIdRd,                     "VMSVGA/Reg/IdRead",                     "SVGA_REG_ID reads.");
    REG_CNT(&pThis->svga.StatRegIrqMaskRd,                "VMSVGA/Reg/IrqMaskRead",                "SVGA_REG_IRQ_MASK reads.");
    REG_CNT(&pThis->svga.StatRegMaxHeightRd,              "VMSVGA/Reg/MaxHeightRead",              "SVGA_REG_MAX_HEIGHT reads.");
    REG_CNT(&pThis->svga.StatRegMaxWidthRd,               "VMSVGA/Reg/MaxWidthRead",               "SVGA_REG_MAX_WIDTH reads.");
    REG_CNT(&pThis->svga.StatRegMemorySizeRd,             "VMSVGA/Reg/MemorySizeRead",             "SVGA_REG_MEMORY_SIZE reads.");
    REG_CNT(&pThis->svga.StatRegMemRegsRd,                "VMSVGA/Reg/MemRegsRead",                "SVGA_REG_MEM_REGS reads.");
    REG_CNT(&pThis->svga.StatRegMemSizeRd,                "VMSVGA/Reg/MemSizeRead",                "SVGA_REG_MEM_SIZE reads.");
    REG_CNT(&pThis->svga.StatRegMemStartRd,               "VMSVGA/Reg/MemStartRead",               "SVGA_REG_MEM_START reads.");
    REG_CNT(&pThis->svga.StatRegNumDisplaysRd,            "VMSVGA/Reg/NumDisplaysRead",            "SVGA_REG_NUM_DISPLAYS reads.");
    REG_CNT(&pThis->svga.StatRegNumGuestDisplaysRd,       "VMSVGA/Reg/NumGuestDisplaysRead",       "SVGA_REG_NUM_GUEST_DISPLAYS reads.");
    REG_CNT(&pThis->svga.StatRegPaletteRd,                "VMSVGA/Reg/PaletteRead",                "SVGA_REG_PLAETTE_XXXX reads.");
    REG_CNT(&pThis->svga.StatRegPitchLockRd,              "VMSVGA/Reg/PitchLockRead",              "SVGA_REG_PITCHLOCK reads.");
    REG_CNT(&pThis->svga.StatRegPsuedoColorRd,            "VMSVGA/Reg/PsuedoColorRead",            "SVGA_REG_PSEUDOCOLOR reads.");
    REG_CNT(&pThis->svga.StatRegRedMaskRd,                "VMSVGA/Reg/RedMaskRead",                "SVGA_REG_RED_MASK reads.");
    REG_CNT(&pThis->svga.StatRegScratchRd,                "VMSVGA/Reg/ScratchRead",                "SVGA_REG_SCRATCH reads.");
    REG_CNT(&pThis->svga.StatRegScratchSizeRd,            "VMSVGA/Reg/ScratchSizeRead",            "SVGA_REG_SCRATCH_SIZE reads.");
    REG_CNT(&pThis->svga.StatRegSyncRd,                   "VMSVGA/Reg/SyncRead",                   "SVGA_REG_SYNC reads.");
    REG_CNT(&pThis->svga.StatRegTopRd,                    "VMSVGA/Reg/TopRead",                    "SVGA_REG_TOP reads.");
    REG_CNT(&pThis->svga.StatRegTracesRd,                 "VMSVGA/Reg/TracesRead",                 "SVGA_REG_TRACES reads.");
    REG_CNT(&pThis->svga.StatRegUnknownRd,                "VMSVGA/Reg/UnknownRead",                "SVGA_REG_UNKNOWN reads.");
    REG_CNT(&pThis->svga.StatRegVramSizeRd,               "VMSVGA/Reg/VramSizeRead",               "SVGA_REG_VRAM_SIZE reads.");
    REG_CNT(&pThis->svga.StatRegWidthRd,                  "VMSVGA/Reg/WidthRead",                  "SVGA_REG_WIDTH reads.");
    REG_CNT(&pThis->svga.StatRegWriteOnlyRd,              "VMSVGA/Reg/WriteOnlyRead",              "Write-only SVGA_REG_XXXX reads.");

    REG_PRF(&pSVGAState->StatBusyDelayEmts,               "VMSVGA/EmtDelayOnBusyFifo",             "Time we've delayed EMTs because of busy FIFO thread.");
    REG_CNT(&pSVGAState->StatFifoCommands,                "VMSVGA/FifoCommands",                   "FIFO command counter.");
    REG_CNT(&pSVGAState->StatFifoErrors,                  "VMSVGA/FifoErrors",                     "FIFO error counter.");
    REG_CNT(&pSVGAState->StatFifoUnkCmds,                 "VMSVGA/FifoUnknownCommands",            "FIFO unknown command counter.");
    REG_CNT(&pSVGAState->StatFifoTodoTimeout,             "VMSVGA/FifoTodoTimeout",                "Number of times we discovered pending work after a wait timeout.");
    REG_CNT(&pSVGAState->StatFifoTodoWoken,               "VMSVGA/FifoTodoWoken",                  "Number of times we discovered pending work after being woken up.");
    REG_PRF(&pSVGAState->StatFifoStalls,                  "VMSVGA/FifoStalls",                     "Profiling of FIFO stalls (waiting for guest to finish copying data).");
    REG_PRF(&pSVGAState->StatFifoExtendedSleep,           "VMSVGA/FifoExtendedSleep",              "Profiling FIFO sleeps relying on the refresh timer and/or access handler.");
# if defined(VMSVGA_USE_FIFO_ACCESS_HANDLER) || defined(DEBUG_FIFO_ACCESS)
    REG_CNT(&pSVGAState->StatFifoAccessHandler,           "VMSVGA/FifoAccessHandler",              "Number of times the FIFO access handler triggered.");
# endif
    REG_CNT(&pSVGAState->StatFifoCursorFetchAgain,        "VMSVGA/FifoCursorFetchAgain",           "Times the cursor update counter changed while reading.");
    REG_CNT(&pSVGAState->StatFifoCursorNoChange,          "VMSVGA/FifoCursorNoChange",             "No cursor position change event though the update counter was modified.");
    REG_CNT(&pSVGAState->StatFifoCursorPosition,          "VMSVGA/FifoCursorPosition",             "Cursor position and visibility changes.");
    REG_CNT(&pSVGAState->StatFifoCursorVisiblity,         "VMSVGA/FifoCursorVisiblity",            "Cursor visibility changes.");
    REG_CNT(&pSVGAState->StatFifoWatchdogWakeUps,         "VMSVGA/FifoWatchdogWakeUps",            "Number of times the FIFO refresh poller/watchdog woke up the FIFO thread.");

# undef REG_CNT
# undef REG_PRF

    /*
     * Info handlers.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "vmsvga", "Basic VMSVGA device state details", vmsvgaR3Info);
# ifdef VBOX_WITH_VMSVGA3D
    PDMDevHlpDBGFInfoRegister(pDevIns, "vmsvga3dctx", "VMSVGA 3d context details. Accepts 'terse'.", vmsvgaR3Info3dContext);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vmsvga3dsfc",
                              "VMSVGA 3d surface details. "
                              "Accepts 'terse', 'invy', and one of 'tiny', 'medium', 'normal', 'big', 'huge', or 'gigantic'.",
                              vmsvgaR3Info3dSurface);
    PDMDevHlpDBGFInfoRegister(pDevIns, "vmsvga3dsurf",
                              "VMSVGA 3d surface details and bitmap: "
                              "sid[>dir]",
                              vmsvgaR3Info3dSurfaceBmp);
# endif

    return VINF_SUCCESS;
}

/**
 * Power On notification.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance data.
 *
 * @remarks Caller enters the device critical section.
 */
DECLCALLBACK(void) vmsvgaR3PowerOn(PPDMDEVINS pDevIns)
{
# ifdef VBOX_WITH_VMSVGA3D
    PVGASTATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PVGASTATE);
    PVGASTATECC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
    if (pThis->svga.f3DEnabled)
    {
        int rc = vmsvga3dPowerOn(pDevIns, pThis, pThisCC);

        if (RT_SUCCESS(rc))
        {
            /* Initialize FIFO 3D capabilities. */
            vmsvgaR3InitFifo3DCaps(pThisCC);
        }
    }
# else  /* !VBOX_WITH_VMSVGA3D */
    RT_NOREF(pDevIns);
# endif /* !VBOX_WITH_VMSVGA3D */
}

#endif /* IN_RING3 */
