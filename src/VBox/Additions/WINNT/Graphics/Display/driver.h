/******************************Module*Header*******************************\
*
*                           *******************
*                           * GDI SAMPLE CODE *
*                           *******************
*
* Module Name: driver.h
*
* contains prototypes for the frame buffer driver.
*
* Copyright (c) 1992-1998 Microsoft Corporation
\**************************************************************************/

#include "stddef.h"
#include <stdarg.h>
#include "windef.h"
#include "wingdi.h"
#include "winddi.h"
#include "devioctl.h"
#include "ntddvdeo.h"
#include "debug.h"

#include "../Miniport/vboxioctl.h"

#include <VBox/VBoxVideo.h>

/* Forward declaration. */
struct _PDEV;
typedef struct _PDEV PDEV;
typedef PDEV *PPDEV;

typedef struct _VBOXDISPLAYINFO
{
    VBOXVIDEOINFOHDR         hdrLink;
    VBOXVIDEOINFOLINK        link;
    VBOXVIDEOINFOHDR         hdrScreen;
    VBOXVIDEOINFOSCREEN      screen;
    VBOXVIDEOINFOHDR         hdrHostEvents;
    VBOXVIDEOINFOHOSTEVENTS  hostEvents;
    VBOXVIDEOINFOHDR         hdrEnd;
} VBOXDISPLAYINFO;

#include "vbvavrdp.h"
#include "vrdpbmp.h"

/* Saved screen bits information. */
typedef struct _SSB
{
    ULONG ident;   /* 1 based index in the stack = the handle returned by DrvSaveScreenBits (SS_SAVE) */
    BYTE *pBuffer; /* Buffer where screen bits are saved. */
} SSB;

/* VRAM
 *  |           |          |           |           |
 * 0+framebuffer+ddraw heap+VBVA buffer+displayinfo=cScreenSize
 */
typedef struct _VRAMLAYOUT
{
    ULONG cbVRAM;
    
    ULONG offFrameBuffer;
    ULONG cbFrameBuffer;
    
    ULONG offDDRAWHeap; //@todo
    ULONG cbDDRAWHeap;
    
    ULONG offVBVABuffer;
    ULONG cbVBVABuffer;
    
    ULONG offDisplayInformation;
    ULONG cbDisplayInformation;
} VRAMLAYOUT;

typedef struct
{
    PPDEV   ppdev;
} VBOXSURF, *PVBOXSURF;

struct  _PDEV
{
    HANDLE  hDriver;                    // Handle to \Device\Screen
    HDEV    hdevEng;                    // Engine's handle to PDEV
    HSURF   hsurfScreenBitmap;          // Engine's handle to VRAM screen bitmap surface
    SURFOBJ *psoScreenBitmap;           // VRAM screen bitmap surface
    HSURF   hsurfScreen;                // Engine's handle to VRAM screen device surface
    ULONG   ulBitmapType;
    HPALETTE hpalDefault;               // Handle to the default palette for device.
    PBYTE   pjScreen;                   // This is pointer to base screen address
    ULONG   cxScreen;                   // Visible screen width
    ULONG   cyScreen;                   // Visible screen height
    POINTL  ptlOrg;                     // Where this display is anchored in
                                        //   the virtual desktop.
    POINTL  ptlDevOrg;                  // Device origin for DualView (0,0 for primary view).
    ULONG   ulMode;                     // Mode the mini-port driver is in.
    LONG    lDeltaScreen;               // Distance from one scan to the next.
    
    PVOID   pOffscreenList;             // linked list of DCI offscreen surfaces.
    FLONG   flRed;                      // For bitfields device, Red Mask
    FLONG   flGreen;                    // For bitfields device, Green Mask
    FLONG   flBlue;                     // For bitfields device, Blue Mask
    ULONG   cPaletteShift;              // number of bits the 8-8-8 palette must
                                        // be shifted by to fit in the hardware
                                        // palette.
    ULONG   ulBitCount;                 // # of bits per pel 8,16,24,32 are only supported.
    POINTL  ptlHotSpot;                 // adjustment for pointer hot spot
    VIDEO_POINTER_CAPABILITIES PointerCapabilities; // HW pointer abilities
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes; // hardware pointer attributes
    DWORD   cjPointerAttributes;        // Size of buffer allocated
    BOOL    fHwCursorActive;            // Are we currently using the hw cursor
    PALETTEENTRY *pPal;                 // If this is pal managed, this is the pal
    BOOL    bSupportDCI;                // Does the miniport support DCI?
    FLONG   flHooks;
    
    VBVAENABLERESULT vbva;
    uint32_t         u32VRDPResetFlag;
    BOOL             fHwBufferOverflow;
    VBVARECORD       *pRecord;
    VRDPBC           cache;

    ULONG cSSB;                 // Number of active saved screen bits records in the following array.
    SSB aSSB[4];                // LIFO type stack for saved screen areas.

    VBOXDISPLAYINFO *pInfo;
    BOOLEAN bVBoxVideoSupported;
    ULONG iDevice;
    VRAMLAYOUT layout;

    PVBOXSURF   pdsurfScreen;

#ifdef VBOX_WITH_DDRAW
    BOOL             bDdExclusiveMode;
    DWORD            dwNewDDSurfaceOffset;
    DWORD            cHeaps;
    VIDEOMEMORY*     pvmList;
    struct {
        DWORD bLocked;
        RECTL rArea;
    } ddLock;
#endif /* VBOX_WITH_DDRAW */
};

#ifdef VBOX_WITH_OPENGL 
typedef struct 
{ 
    DWORD dwVersion; 
    DWORD dwDriverVersion; 
    WCHAR szDriverName[256]; 
} OPENGL_INFO, *POPENGL_INFO; 
#endif 

/* The global semaphore handle for all driver instances. */
extern HSEMAPHORE ghsemHwBuffer;

extern BOOL  g_bOnNT40;

DWORD getAvailableModes(HANDLE, PVIDEO_MODE_INFORMATION *, DWORD *);
BOOL bInitPDEV(PPDEV, PDEVMODEW, GDIINFO *, DEVINFO *);
BOOL bInitSURF(PPDEV, BOOL);
BOOL bInitPaletteInfo(PPDEV, DEVINFO *);
BOOL bInitPointer(PPDEV, DEVINFO *);
BOOL bInit256ColorPalette(PPDEV);
BOOL bInitNotificationThread(PPDEV);
VOID vStopNotificationThread (PPDEV);
VOID vDisablePalette(PPDEV);
VOID vDisableSURF(PPDEV);

#define MAX_CLUT_SIZE (sizeof(VIDEO_CLUT) + (sizeof(ULONG) * 256))

//
// Determines the size of the DriverExtra information in the DEVMODE
// structure passed to and from the display driver.
//

#define DRIVER_EXTRA_SIZE 0

#define DLL_NAME                L"VBoxDisp"   // Name of the DLL in UNICODE
#define STANDARD_DEBUG_PREFIX   "VBOXDISP: "  // All debug output is prefixed
#define ALLOC_TAG               'bvDD'        // Four byte tag (characters in
                                              // reverse order) used for memory
                                              // allocations

// VBOX
typedef struct _CLIPRECTS {
    ULONG  c;
    RECTL  arcl[64];
} CLIPRECTS;

typedef struct _VRDPCLIPRECTS
{
    RECTL rclDstOrig; /* Original bounding rectancle. */
    RECTL rclDst;     /* Bounding rectangle of all rects. */
    CLIPRECTS rects;  /* Rectangles to update. */
} VRDPCLIPRECTS;


BOOL vboxVbvaEnable (PPDEV ppdev);
void vboxVbvaDisable (PPDEV ppdev);

BOOL vboxHwBufferBeginUpdate (PPDEV ppdev);
void vboxHwBufferEndUpdate (PPDEV ppdev);

BOOL vboxWrite (PPDEV ppdev, const void *pv, uint32_t cb);

BOOL vboxOrderSupported (PPDEV ppdev, unsigned code);

void VBoxProcessDisplayInfo(PPDEV ppdev);
void VBoxUpdateDisplayInfo (PPDEV ppdev);

void drvLoadEng (void);

BOOL bIsScreenSurface (SURFOBJ *pso);

__inline SURFOBJ *getSurfObj (SURFOBJ *pso)
{
    if (pso)
    {
        PPDEV ppdev = (PPDEV)pso->dhpdev;
    
        if (ppdev)
        {
            if (ppdev->psoScreenBitmap && pso->hsurf == ppdev->hsurfScreen)
            {
                /* Convert the device PSO to the bitmap PSO which can be passed to Eng*. */
                pso = ppdev->psoScreenBitmap;
            }
        }
    }
    
    return pso;
}

#define CONV_SURF(_pso) getSurfObj (_pso)

__inline int format2BytesPerPixel(const SURFOBJ *pso)
{
    switch (pso->iBitmapFormat)
    {
        case BMF_16BPP: return 2;
        case BMF_24BPP: return 3;
        case BMF_32BPP: return 4;
    }
    
    return 0;
}

#ifdef VBOX_VBVA_ADJUST_RECT
void vrdpAdjustRect (SURFOBJ *pso, RECTL *prcl);
BOOL vbvaFindChangedRect (SURFOBJ *psoDest, SURFOBJ *psoSrc, RECTL *prclDest, POINTL *pptlSrc);
#endif /* VBOX_VBVA_ADJUST_RECT */

void vrdpReportDirtyRect (PPDEV ppdev, RECTL *prcl);
void vbvaReportDirtyRect (PPDEV ppdev, RECTL *prcl);

#define VRDP_TEXT_MAX_GLYPH_SIZE 0x100
#define VRDP_TEXT_MAX_GLYPHS     0xfe
 
BOOL vboxReportText (PPDEV ppdev,
                     VRDPCLIPRECTS *pClipRects,
                     STROBJ   *pstro,
                     FONTOBJ  *pfo,
                     RECTL    *prclOpaque,
                     ULONG    ulForeRGB,
                     ULONG    ulBackRGB
                    );

BOOL vrdpReportOrderGeneric (PPDEV ppdev,
                             const VRDPCLIPRECTS *pClipRects,
                             const void *pvOrder,
                             unsigned cbOrder,
                             unsigned code);


#include <iprt/assert.h>

#define VBVA_ASSERT(expr) \
    do { \
        if (!(expr)) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertMsg2("!!!\n"); \
        } \
    } while (0)

#ifdef STAT_sunlover
extern ULONG gStatCopyBitsOffscreenToScreen;
extern ULONG gStatCopyBitsScreenToScreen;
extern ULONG gStatBitBltOffscreenToScreen;
extern ULONG gStatBitBltScreenToScreen;
extern ULONG gStatUnchangedOffscreenToScreen;
extern ULONG gStatUnchangedOffscreenToScreenCRC;
extern ULONG gStatNonTransientEngineBitmaps;
extern ULONG gStatTransientEngineBitmaps;
extern ULONG gStatUnchangedBitmapsCRC;
extern ULONG gStatUnchangedBitmapsDeviceCRC;
extern ULONG gStatBitmapsCRC;
extern ULONG gStatBitBltScreenPattern;
extern ULONG gStatBitBltScreenSquare;
extern ULONG gStatBitBltScreenPatternReported;
extern ULONG gStatBitBltScreenSquareReported;
extern ULONG gStatCopyBitsScreenSquare;

extern ULONG gStatEnablePDEV;
extern ULONG gStatCompletePDEV;
extern ULONG gStatDisablePDEV;
extern ULONG gStatEnableSurface;
extern ULONG gStatDisableSurface;
extern ULONG gStatAssertMode;
extern ULONG gStatDisableDriver;
extern ULONG gStatCreateDeviceBitmap;
extern ULONG gStatDeleteDeviceBitmap;
extern ULONG gStatDitherColor;
extern ULONG gStatStrokePath;
extern ULONG gStatFillPath;
extern ULONG gStatStrokeAndFillPath;
extern ULONG gStatPaint;
extern ULONG gStatBitBlt;
extern ULONG gStatCopyBits;
extern ULONG gStatStretchBlt;
extern ULONG gStatSetPalette;
extern ULONG gStatTextOut;
extern ULONG gStatSetPointerShape;
extern ULONG gStatMovePointer;
extern ULONG gStatLineTo;
extern ULONG gStatSynchronize;
extern ULONG gStatGetModes;
extern ULONG gStatGradientFill;
extern ULONG gStatStretchBltROP;
extern ULONG gStatPlgBlt;
extern ULONG gStatAlphaBlend;
extern ULONG gStatTransparentBlt;

void statPrint (void);

#define STATDRVENTRY(a, b) do { if (bIsScreenSurface (b)) gStat##a++; } while (0)
#define STATPRINT do { statPrint (); } while (0)
#else
#define STATDRVENTRY(a, b)
#define STATPRINT
#endif /* STAT_sunlover */
