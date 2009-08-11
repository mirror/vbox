/** @file
 * VirtualBox Video interface.
 */

/*
 * Copyright (C) 2006 Sun Microsystems, Inc.
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

#ifndef ___VBox_VBoxVideo_h
#define ___VBox_VBoxVideo_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

/*
 * The last 4096 bytes of the guest VRAM contains the generic info for all
 * DualView chunks: sizes and offsets of chunks. This is filled by miniport.
 *
 * Last 4096 bytes of each chunk contain chunk specific data: framebuffer info,
 * etc. This is used exclusively by the corresponding instance of a display driver.
 *
 * The VRAM layout:
 *     Last 4096 bytes - Adapter information area.
 *     4096 bytes aligned miniport heap (value specified in the config rouded up).
 *     Slack - what left after dividing the VRAM.
 *     4096 bytes aligned framebuffers:
 *       last 4096 bytes of each framebuffer is the display information area.
 *
 * The Virtual Graphics Adapter information in the guest VRAM is stored by the
 * guest video driver using structures prepended by VBOXVIDEOINFOHDR.
 *
 * When the guest driver writes dword 0 to the VBE_DISPI_INDEX_VBOX_VIDEO
 * the host starts to process the info. The first element at the start of
 * the 4096 bytes region should be normally be a LINK that points to
 * actual information chain. That way the guest driver can have some
 * fixed layout of the information memory block and just rewrite
 * the link to point to relevant memory chain.
 *
 * The processing stops at the END element.
 *
 * The host can access the memory only when the port IO is processed.
 * All data that will be needed later must be copied from these 4096 bytes.
 * But other VRAM can be used by host until the mode is disabled.
 *
 * The guest driver writes dword 0xffffffff to the VBE_DISPI_INDEX_VBOX_VIDEO
 * to disable the mode.
 *
 * VBE_DISPI_INDEX_VBOX_VIDEO is used to read the configuration information
 * from the host and issue commands to the host.
 *
 * The guest writes the VBE_DISPI_INDEX_VBOX_VIDEO index register, the the
 * following operations with the VBE data register can be performed:
 *
 * Operation            Result
 * write 16 bit value   NOP
 * read 16 bit value    count of monitors
 * write 32 bit value   sets the vbox command value and the command processed by the host
 * read 32 bit value    result of the last vbox command is returned
 */

#define VBOX_VIDEO_PRIMARY_SCREEN 0
#define VBOX_VIDEO_NO_SCREEN ~0

#define VBOX_VIDEO_MAX_SCREENS 64

/* The size of the information. */
#ifndef VBOX_WITH_HGSMI
#define VBOX_VIDEO_ADAPTER_INFORMATION_SIZE  4096
#define VBOX_VIDEO_DISPLAY_INFORMATION_SIZE  4096
#else
/*
 * The minimum HGSMI heap size is PAGE_SIZE (4096 bytes) and is a restriction of the
 * runtime heapsimple API. Use minimum 2 pages here, because the info area also may
 * contain other data (for example HGSMIHOSTFLAGS structure).
 */
#define VBVA_ADAPTER_INFORMATION_SIZE  (8*_1K)
#define VBVA_DISPLAY_INFORMATION_SIZE  (8*_1K)
#define VBVA_MIN_BUFFER_SIZE           (64*_1K)
#endif /* VBOX_WITH_HGSMI */


/* The value for port IO to let the adapter to interpret the adapter memory. */
#define VBOX_VIDEO_DISABLE_ADAPTER_MEMORY        0xFFFFFFFF

/* The value for port IO to let the adapter to interpret the adapter memory. */
#define VBOX_VIDEO_INTERPRET_ADAPTER_MEMORY      0x00000000

/* The value for port IO to let the adapter to interpret the display memory.
 * The display number is encoded in low 16 bits.
 */
#define VBOX_VIDEO_INTERPRET_DISPLAY_MEMORY_BASE 0x00010000


/* The end of the information. */
#define VBOX_VIDEO_INFO_TYPE_END          0
/* Instructs the host to fetch the next VBOXVIDEOINFOHDR at the given offset of VRAM. */
#define VBOX_VIDEO_INFO_TYPE_LINK         1
/* Information about a display memory position. */
#define VBOX_VIDEO_INFO_TYPE_DISPLAY      2
/* Information about a screen. */
#define VBOX_VIDEO_INFO_TYPE_SCREEN       3
/* Information about host notifications for the driver. */
#define VBOX_VIDEO_INFO_TYPE_HOST_EVENTS  4
/* Information about non-volatile guest VRAM heap. */
#define VBOX_VIDEO_INFO_TYPE_NV_HEAP      5
/* VBVA enable/disable. */
#define VBOX_VIDEO_INFO_TYPE_VBVA_STATUS  6
/* VBVA flush. */
#define VBOX_VIDEO_INFO_TYPE_VBVA_FLUSH   7
/* Query configuration value. */
#define VBOX_VIDEO_INFO_TYPE_QUERY_CONF32 8


#pragma pack(1)
typedef struct _VBOXVIDEOINFOHDR
{
    uint8_t u8Type;
    uint8_t u8Reserved;
    uint16_t u16Length;
} VBOXVIDEOINFOHDR;


typedef struct _VBOXVIDEOINFOLINK
{
    /* Relative offset in VRAM */
    int32_t i32Offset;
} VBOXVIDEOINFOLINK;


/* Resides in adapter info memory. Describes a display VRAM chunk. */
typedef struct _VBOXVIDEOINFODISPLAY
{
    /* Index of the framebuffer assigned by guest. */
    uint32_t u32Index;

    /* Absolute offset in VRAM of the framebuffer to be displayed on the monitor. */
    uint32_t u32Offset;

    /* The size of the memory that can be used for the screen. */
    uint32_t u32FramebufferSize;

    /* The size of the memory that is used for the Display information.
     * The information is at u32Offset + u32FramebufferSize
     */
    uint32_t u32InformationSize;

} VBOXVIDEOINFODISPLAY;


/* Resides in display info area, describes the current video mode. */
#define VBOX_VIDEO_INFO_SCREEN_F_NONE   0x00
#define VBOX_VIDEO_INFO_SCREEN_F_ACTIVE 0x01

typedef struct _VBOXVIDEOINFOSCREEN
{
    /* Physical X origin relative to the primary screen. */
    int32_t xOrigin;

    /* Physical Y origin relative to the primary screen. */
    int32_t yOrigin;

    /* The scan line size in bytes. */
    uint32_t u32LineSize;

    /* Width of the screen. */
    uint16_t u16Width;

    /* Height of the screen. */
    uint16_t u16Height;

    /* Color depth. */
    uint8_t bitsPerPixel;

    /* VBOX_VIDEO_INFO_SCREEN_F_* */
    uint8_t u8Flags;
} VBOXVIDEOINFOSCREEN;

/* The guest initializes the structure to 0. The positions of the structure in the
 * display info area must not be changed, host will update the structure. Guest checks
 * the events and modifies the structure as a response to host.
 */
#define VBOX_VIDEO_INFO_HOST_EVENTS_F_NONE        0x00000000
#define VBOX_VIDEO_INFO_HOST_EVENTS_F_VRDP_RESET  0x00000080

typedef struct _VBOXVIDEOINFOHOSTEVENTS
{
    /* Host events. */
    uint32_t fu32Events;
} VBOXVIDEOINFOHOSTEVENTS;

/* Resides in adapter info memory. Describes the non-volatile VRAM heap. */
typedef struct _VBOXVIDEOINFONVHEAP
{
    /* Absolute offset in VRAM of the start of the heap. */
    uint32_t u32HeapOffset;

    /* The size of the heap. */
    uint32_t u32HeapSize;

} VBOXVIDEOINFONVHEAP;

/* Display information area. */
typedef struct _VBOXVIDEOINFOVBVASTATUS
{
    /* Absolute offset in VRAM of the start of the VBVA QUEUE. 0 to disable VBVA. */
    uint32_t u32QueueOffset;

    /* The size of the VBVA QUEUE. 0 to disable VBVA. */
    uint32_t u32QueueSize;

} VBOXVIDEOINFOVBVASTATUS;

typedef struct _VBOXVIDEOINFOVBVAFLUSH
{
    uint32_t u32DataStart;

    uint32_t u32DataEnd;

} VBOXVIDEOINFOVBVAFLUSH;

#define VBOX_VIDEO_QCI32_MONITOR_COUNT       0
#define VBOX_VIDEO_QCI32_OFFSCREEN_HEAP_SIZE 1

typedef struct _VBOXVIDEOINFOQUERYCONF32
{
    uint32_t u32Index;

    uint32_t u32Value;

} VBOXVIDEOINFOQUERYCONF32;
#pragma pack()

# ifdef VBOX_WITH_VIDEOHWACCEL
#pragma pack(1)

#define VBOXVHWA_VERSION_MAJ 0
#define VBOXVHWA_VERSION_MIN 0
#define VBOXVHWA_VERSION_BLD 1
#define VBOXVHWA_VERSION_RSV 0

typedef enum
{
    VBOXVHWACMD_TYPE_SURF_CANCREATE = 1,
    VBOXVHWACMD_TYPE_SURF_CREATE,
    VBOXVHWACMD_TYPE_SURF_DESTROY,
    VBOXVHWACMD_TYPE_SURF_LOCK,
    VBOXVHWACMD_TYPE_SURF_UNLOCK,
    VBOXVHWACMD_TYPE_SURF_BLT,
    VBOXVHWACMD_TYPE_SURF_FLIP,
    VBOXVHWACMD_TYPE_SURF_OVERLAY_UPDATE,
    VBOXVHWACMD_TYPE_SURF_OVERLAY_SETPOSITION,
    VBOXVHWACMD_TYPE_SURF_COLORKEY_SET,
    VBOXVHWACMD_TYPE_QUERY_INFO1,
    VBOXVHWACMD_TYPE_QUERY_INFO2,
    VBOXVHWACMD_TYPE_ENABLE,
    VBOXVHWACMD_TYPE_DISABLE
} VBOXVHWACMD_TYPE;

/* the command processing was asynch, set by the host to indicate asynch command completion
 * must not be cleared once set, the command completion is performed by issuing a host->guest completion command
 * while keeping this flag unchanged */
#define VBOXVHWACMD_FLAG_HG_ASYNCH               0x00010000
/* asynch completion is performed by issuing the event */
#define VBOXVHWACMD_FLAG_GH_ASYNCH_EVENT         0x00000001
/* issue interrupt on asynch completion */
#define VBOXVHWACMD_FLAG_GH_ASYNCH_IRQ           0x00000002
/* guest does not do any op on completion of this command, the host may copy the command and indicate that it does not need the command anymore
 * by setting the VBOXVHWACMD_FLAG_HG_ASYNCH_RETURNED flag */
#define VBOXVHWACMD_FLAG_GH_ASYNCH_NOCOMPLETION  0x00000004
/* the host has copied the VBOXVHWACMD_FLAG_GH_ASYNCH_NOCOMPLETION command and returned it to the guest */
#define VBOXVHWACMD_FLAG_HG_ASYNCH_RETURNED      0x00020000

typedef struct _VBOXVHWACMD
{
    VBOXVHWACMD_TYPE enmCmd; /* command type */
    volatile int32_t rc; /* command result */
    int32_t iDisplay; /* display index */
    volatile int32_t Flags; /* ored VBOXVHWACMD_FLAG_xxx values */
    uint64_t GuestVBVAReserved1; /* field internally used by the guest VBVA cmd handling, must NOT be modified by clients */
    uint64_t GuestVBVAReserved2; /* field internally used by the guest VBVA cmd handling, must NOT be modified by clients */
    union
    {
        struct _VBOXVHWACMD *pNext;
        uint32_t             offNext;
        uint64_t Data; /* the body is 64-bit aligned */
    } u;
    char body[1];
} VBOXVHWACMD;

#define VBOXVHWACMD_HEADSIZE() (RT_OFFSETOF(VBOXVHWACMD, body))
#define VBOXVHWACMD_SIZE(_tCmd) (VBOXVHWACMD_HEADSIZE() + sizeof(_tCmd))
typedef unsigned int VBOXVHWACMD_LENGTH;
typedef uint64_t VBOXVHWA_SURFHANDLE;
#define VBOXVHWACMD_SURFHANDLE_INVALID 0
#define VBOXVHWACMD_BODY(_p, _t) ((_t*)(_p)->body)
#define VBOXVHWACMD_HEAD(_pb) ((VBOXVHWACMD*)((uint8_t *)(_pb) - RT_OFFSETOF(VBOXVHWACMD, body)))

typedef struct _VBOXVHWA_RECTL
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} VBOXVHWA_RECTL;

typedef struct _VBOXVHWA_COLORKEY
{
    uint32_t low;
    uint32_t high;
} VBOXVHWA_COLORKEY;

typedef struct _VBOXVHWA_PIXELFORMAT
{
    uint32_t flags;
    uint32_t fourCC;
    union
    {
        uint32_t rgbBitCount;
        uint32_t yuvBitCount;
    } c;

    union
    {
        uint32_t rgbRBitMask;
        uint32_t yuvYBitMask;
    } m1;

    union
    {
        uint32_t rgbGBitMask;
        uint32_t yuvUBitMask;
    } m2;

    union
    {
        uint32_t rgbBBitMask;
        uint32_t yuvVBitMask;
    } m3;

    union
    {
        uint32_t rgbABitMask;
    } m4;
} VBOXVHWA_PIXELFORMAT;

typedef struct _VBOXVHWA_SURFACEDESC
{
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitch;
    uint32_t sizeX;
    uint32_t sizeY;
    uint32_t cBackBuffers;
    uint32_t Reserved;
    VBOXVHWA_COLORKEY DstOverlayCK;
    VBOXVHWA_COLORKEY DstBltCK;
    VBOXVHWA_COLORKEY SrcOverlayCK;
    VBOXVHWA_COLORKEY SrcBltCK;
    VBOXVHWA_PIXELFORMAT PixelFormat;
    uint32_t surfCaps;
    uint32_t Reserved2;
} VBOXVHWA_SURFACEDESC;

typedef struct _VBOXVHWA_BLTFX
{
    uint32_t flags;
    uint32_t rop;
    uint32_t rotationOp;
    uint32_t rotation;
    uint32_t fillColor;
    uint32_t Reserved;
    VBOXVHWA_COLORKEY DstCK;
    VBOXVHWA_COLORKEY SrcCK;
} VBOXVHWA_BLTFX;

typedef struct _VBOXVHWA_OVERLAYFX
{
    uint32_t flags;
    uint32_t Reserved1;
    uint32_t fxFlags;
    uint32_t Reserved2;
    VBOXVHWA_COLORKEY DstCK;
    VBOXVHWA_COLORKEY SrcCK;
} VBOXVHWA_OVERLAYFX;

#define VBOXVHWA_CAPS_BLT             0x00000001
#define VBOXVHWA_CAPS_BLTCOLORFILL    0x00000002
#define VBOXVHWA_CAPS_BLTFOURCC       0x00000004
#define VBOXVHWA_CAPS_BLTSTRETCH      0x00000008
#define VBOXVHWA_CAPS_BLTQUEUE        0x00000010

#define VBOXVHWA_CAPS_OVERLAY         0x00000100
#define VBOXVHWA_CAPS_OVERLAYFOURCC   0x00000200
#define VBOXVHWA_CAPS_OVERLAYSTRETCH  0x00000400
#define VBOXVHWA_CAPS_OVERLAYCANTCLIP 0x00000800

#define VBOXVHWA_CAPS_COLORKEY        0x00010000
#define VBOXVHWA_CAPS_COLORKEYHWASSIST         0x00020000


#define VBOXVHWA_SCAPS_FLIP             0x00000001
#define VBOXVHWA_SCAPS_PRIMARYSURFACE   0x00000002
#define VBOXVHWA_SCAPS_OFFSCREENPLAIN   0x00000004
#define VBOXVHWA_SCAPS_OVERLAY          0x00000008
#define VBOXVHWA_SCAPS_VISIBLE          0x00000010
#define VBOXVHWA_SCAPS_VIDEOMEMORY      0x00000020
#define VBOXVHWA_SCAPS_LOCALVIDMEM      0x00000040
#define VBOXVHWA_SCAPS_COMPLEX          0x00000080


#define VBOXVHWA_PF_RGB                 0x00000001
#define VBOXVHWA_PF_RGBTOYUV            0x00000002
#define VBOXVHWA_PF_YUV                 0x00000008
#define VBOXVHWA_PF_FOURCC              0x00000010

#define VBOXVHWA_LOCK_DISCARDCONTENTS   0x00000001

#define VBOXVHWA_CFG_ENABLED            0x00000001

#define VBOXVHWA_SD_BACKBUFFERCOUNT     0x00000001
#define VBOXVHWA_SD_CAPS                0x00000002
#define VBOXVHWA_SD_CKDESTBLT           0x00000004
#define VBOXVHWA_SD_CKDESTOVERLAY       0x00000008
#define VBOXVHWA_SD_CKSRCBLT            0x00000010
#define VBOXVHWA_SD_CKSRCOVERLAY        0x00000020
#define VBOXVHWA_SD_HEIGHT              0x00000040
#define VBOXVHWA_SD_PITCH               0x00000080
#define VBOXVHWA_SD_PIXELFORMAT         0x00000100
//#define VBOXVHWA_SD_REFRESHRATE       0x00000200
#define VBOXVHWA_SD_WIDTH               0x00000400

#define VBOXVHWA_CKEYCAPS_DESTBLT                  0x00000001
#define VBOXVHWA_CKEYCAPS_DESTBLTCLRSPACE          0x00000002
#define VBOXVHWA_CKEYCAPS_DESTBLTCLRSPACEYUV       0x00000004
#define VBOXVHWA_CKEYCAPS_DESTBLTYUV               0x00000008
#define VBOXVHWA_CKEYCAPS_DESTOVERLAY              0x00000010
#define VBOXVHWA_CKEYCAPS_DESTOVERLAYCLRSPACE      0x00000020
#define VBOXVHWA_CKEYCAPS_DESTOVERLAYCLRSPACEYUV   0x00000040
#define VBOXVHWA_CKEYCAPS_DESTOVERLAYONEACTIVE     0x00000080
#define VBOXVHWA_CKEYCAPS_DESTOVERLAYYUV           0x00000100
#define VBOXVHWA_CKEYCAPS_NOCOSTOVERLAY            0x00000200
#define VBOXVHWA_CKEYCAPS_SRCBLT                   0x00000400
#define VBOXVHWA_CKEYCAPS_SRCBLTCLRSPACE           0x00000800
#define VBOXVHWA_CKEYCAPS_SRCBLTCLRSPACEYUV        0x00001000
#define VBOXVHWA_CKEYCAPS_SRCBLTYUV                0x00002000
#define VBOXVHWA_CKEYCAPS_SRCOVERLAY               0x00004000
#define VBOXVHWA_CKEYCAPS_SRCOVERLAYCLRSPACE       0x00008000
#define VBOXVHWA_CKEYCAPS_SRCOVERLAYCLRSPACEYUV    0x00010000
#define VBOXVHWA_CKEYCAPS_SRCOVERLAYONEACTIVE      0x00020000
#define VBOXVHWA_CKEYCAPS_SRCOVERLAYYUV            0x00040000


#define VBOXVHWA_BLT_COLORFILL                      0x00000400
#define VBOXVHWA_BLT_DDFX                           0x00000800
#define VBOXVHWA_BLT_EXTENDED_FLAGS                 0x40000000
#define VBOXVHWA_BLT_EXTENDED_LINEAR_CONTENT        0x00000004
#define VBOXVHWA_BLT_EXTENDED_PRESENTATION_STRETCHFACTOR 0x00000010
#define VBOXVHWA_BLT_KEYDESTOVERRIDE                0x00004000
#define VBOXVHWA_BLT_KEYSRCOVERRIDE                 0x00010000
#define VBOXVHWA_BLT_LAST_PRESENTATION              0x20000000
#define VBOXVHWA_BLT_PRESENTATION                   0x10000000
#define VBOXVHWA_BLT_ROP                            0x00020000


#define VBOXVHWA_OVER_DDFX                          0x00080000
#define VBOXVHWA_OVER_HIDE                          0x00000200
#define VBOXVHWA_OVER_KEYDEST                       0x00000400
#define VBOXVHWA_OVER_KEYDESTOVERRIDE               0x00000800
#define VBOXVHWA_OVER_KEYSRC                        0x00001000
#define VBOXVHWA_OVER_KEYSRCOVERRIDE                0x00002000
#define VBOXVHWA_OVER_SHOW                          0x00004000

#define VBOXVHWA_CKEY_COLORSPACE                    0x00000001
#define VBOXVHWA_CKEY_DESTBLT                       0x00000002
#define VBOXVHWA_CKEY_DESTOVERLAY                   0x00000004
#define VBOXVHWA_CKEY_SRCBLT                        0x00000008
#define VBOXVHWA_CKEY_SRCOVERLAY                    0x00000010

#define VBOXVHWA_BLT_ARITHSTRETCHY                  0x00000001
#define VBOXVHWA_BLT_MIRRORLEFTRIGHT                0x00000002
#define VBOXVHWA_BLT_MIRRORUPDOWN                   0x00000004

#define VBOXVHWA_OVERFX_ARITHSTRETCHY               0x00000001
#define VBOXVHWA_OVERFX_MIRRORLEFTRIGHT             0x00000002
#define VBOXVHWA_OVERFX_MIRRORUPDOWN                0x00000004

#define VBOXVHWA_CAPS2_CANRENDERWINDOWED            0x00080000
#define VBOXVHWA_CAPS2_WIDESURFACES                 0x00001000
#define VBOXVHWA_CAPS2_COPYFOURCC                   0x00008000
//#define VBOXVHWA_CAPS2_FLIPINTERVAL                 0x00200000
//#define VBOXVHWA_CAPS2_FLIPNOVSYNC                  0x00400000


#define VBOXVHWA_OFFSET64_VOID        (~0L)

typedef struct _VBOXVHWA_VERSION
{
    uint32_t maj;
    uint32_t min;
    uint32_t bld;
    uint32_t reserved;
} VBOXVHWA_VERSION;

typedef struct _VBOXVHWACMD_QUERYINFO1
{
    union
    {
        struct
        {
            VBOXVHWA_VERSION guestVersion;
        } in;

        struct
        {
            uint32_t cfgFlags;
            uint32_t caps;

            uint32_t caps2;
            uint32_t colorKeyCaps;

            uint32_t stretchCaps;
            uint32_t surfaceCaps;

            uint32_t numOverlays;
            uint32_t curOverlays;

            uint32_t numFourCC;
            uint32_t reserved;
        } out;
    } u;
} VBOXVHWACMD_QUERYINFO1;

typedef struct _VBOXVHWACMD_QUERYINFO2
{
    uint32_t numFourCC;
    uint32_t FourCC[1];
} VBOXVHWACMD_QUERYINFO2;

#define VBOXVHWAINFO2_SIZE(_cFourCC) RT_OFFSETOF(VBOXVHWAINFO2, FourCC[_cFourCC])

typedef struct _VBOXVHWACMD_SURF_CANCREATE
{
    VBOXVHWA_SURFACEDESC SurfInfo;
    union
    {
        struct
        {
            uint32_t bIsDifferentPixelFormat;
            uint32_t Reserved;
        } in;

        struct
        {
            uint32_t ErrInfo;
        } out;
    } u;
} VBOXVHWACMD_SURF_CANCREATE;

typedef struct _VBOXVHWACMD_SURF_CREATE
{
    VBOXVHWA_SURFACEDESC SurfInfo;
    union
    {
        struct
        {
            uint64_t offSurface;
        } in;

        struct
        {
            VBOXVHWA_SURFHANDLE hSurf;
        } out;
    } u;
} VBOXVHWACMD_SURF_CREATE;

typedef struct _VBOXVHWACMD_SURF_DESTROY
{
    union
    {
        struct
        {
            VBOXVHWA_SURFHANDLE hSurf;
        } in;
    } u;
} VBOXVHWACMD_SURF_DESTROY;

typedef struct _VBOXVHWACMD_SURF_LOCK
{
    union
    {
        struct
        {
            VBOXVHWA_SURFHANDLE hSurf;
            uint64_t offSurface;
            uint32_t flags;
            uint32_t rectValid;
            VBOXVHWA_RECTL rect;
        } in;
    } u;
} VBOXVHWACMD_SURF_LOCK;

typedef struct _VBOXVHWACMD_SURF_UNLOCK
{
    union
    {
        struct
        {
            VBOXVHWA_SURFHANDLE hSurf;
            uint32_t xUpdatedMemValid;
            uint32_t reserved;
            VBOXVHWA_RECTL xUpdatedMemRect;
        } in;
    } u;
} VBOXVHWACMD_SURF_UNLOCK;

typedef struct _VBOXVHWACMD_SURF_BLT
{
    uint64_t DstGuestSurfInfo;
    uint64_t SrcGuestSurfInfo;
    union
    {
        struct
        {
            VBOXVHWA_SURFHANDLE hDstSurf;
            uint64_t offDstSurface;
            VBOXVHWA_RECTL dstRect;
            VBOXVHWA_SURFHANDLE hSrcSurf;
            uint64_t offSrcSurface;
            VBOXVHWA_RECTL srcRect;
            uint32_t flags;
            uint32_t xUpdatedSrcMemValid;
            VBOXVHWA_BLTFX desc;
            VBOXVHWA_RECTL xUpdatedSrcMemRect;
        } in;
    } u;
} VBOXVHWACMD_SURF_BLT;

typedef struct _VBOXVHWACMD_SURF_FLIP
{
    uint64_t TargGuestSurfInfo;
    uint64_t CurrGuestSurfInfo;
    union
    {
        struct
        {
            VBOXVHWA_SURFHANDLE hTargSurf;
            uint64_t offTargSurface;
            VBOXVHWA_SURFHANDLE hCurrSurf;
            uint64_t offCurrSurface;
            uint32_t flags;
            uint32_t xUpdatedTargMemValid;
            VBOXVHWA_RECTL xUpdatedTargMemRect;
        } in;
    } u;
} VBOXVHWACMD_SURF_FLIP;

typedef struct _VBOXVHWACMD_SURF_COLORKEY_SET
{
    union
    {
        struct
        {
            VBOXVHWA_SURFHANDLE hSurf;
            uint64_t offSurface;
            VBOXVHWA_COLORKEY CKey;
            uint32_t flags;
            uint32_t reserved;
        } in;
    } u;
} VBOXVHWACMD_SURF_COLORKEY_SET;

typedef struct _VBOXVHWACMD_SURF_OVERLAY_UPDATE
{
    union
    {
        struct
        {
            VBOXVHWA_SURFHANDLE hDstSurf;
            uint64_t offDstSurface;
            VBOXVHWA_RECTL dstRect;
            VBOXVHWA_SURFHANDLE hSrcSurf;
            uint64_t offSrcSurface;
            VBOXVHWA_RECTL srcRect;
            uint32_t flags;
            uint32_t xUpdatedSrcMemValid;
            VBOXVHWA_OVERLAYFX desc;
            VBOXVHWA_RECTL xUpdatedSrcMemRect;
        } in;
    } u;
}VBOXVHWACMD_SURF_OVERLAY_UPDATE;

typedef struct _VBOXVHWACMD_SURF_OVERLAY_SETPOSITION
{
    union
    {
        struct
        {
            VBOXVHWA_SURFHANDLE hDstSurf;
            uint64_t offDstSurface;
            VBOXVHWA_SURFHANDLE hSrcSurf;
            uint64_t offSrcSurface;
            uint32_t xPos;
            uint32_t yPos;
            uint32_t flags;
            uint32_t reserved;
        } in;
    } u;
} VBOXVHWACMD_SURF_OVERLAY_SETPOSITION;

#pragma pack()
# endif /* #ifdef VBOX_WITH_VIDEOHWACCEL */

#ifdef VBOX_WITH_HGSMI

/* All structures are without alignment. */
#pragma pack(1)

typedef struct _VBVABUFFER
{
    uint32_t u32HostEvents;
    uint32_t u32SupportedOrders;

    /* The offset where the data start in the buffer. */
    uint32_t off32Data;
    /* The offset where next data must be placed in the buffer. */
    uint32_t off32Free;

    /* The queue of record descriptions. */
    VBVARECORD aRecords[VBVA_MAX_RECORDS];
    uint32_t indexRecordFirst;
    uint32_t indexRecordFree;

    /* Space to leave free in the buffer when large partial records are transferred. */
    uint32_t cbPartialWriteThreshold;

    uint32_t cbData;
    uint8_t  au8Data[1]; /* variable size for the rest of the VBVABUFFER area in VRAM. */
} VBVABUFFER;

/* guest->host commands */
#define VBVA_QUERY_CONF32 1
#define VBVA_SET_CONF32   2
#define VBVA_INFO_VIEW    3
#define VBVA_INFO_HEAP    4
#define VBVA_FLUSH        5
#define VBVA_INFO_SCREEN  6
#define VBVA_ENABLE       7
#define VBVA_MOUSE_POINTER_SHAPE 8
#ifdef VBOX_WITH_VIDEOHWACCEL
# define VBVA_VHWA_CMD    9
#endif /* # ifdef VBOX_WITH_VIDEOHWACCEL */

/* host->guest commands */
# define VBVAHG_EVENT 1
# define VBVAHG_DISPLAY_CUSTOM 2

# ifdef VBOX_WITH_VIDEOHWACCEL
#define VBVAHG_DCUSTOM_VHWA_CMDCOMPLETE 1
#pragma pack(1)
typedef struct _VBVAHOSTCMDVHWACMDCOMPLETE
{
    uint32_t offCmd;
}VBVAHOSTCMDVHWACMDCOMPLETE;
#pragma pack()
# endif /* # ifdef VBOX_WITH_VIDEOHWACCEL */

#pragma pack(1)
typedef enum
{
    VBVAHOSTCMD_OP_EVENT = 1,
    VBVAHOSTCMD_OP_CUSTOM
}VBVAHOSTCMD_OP_TYPE;

typedef struct _VBVAHOSTCMDEVENT
{
    uint64_t pEvent;
}VBVAHOSTCMDEVENT;


typedef struct _VBVAHOSTCMD
{
    /* destination ID if >=0 specifies display index, otherwize the command is directed to the miniport */
    int32_t iDstID;
    int32_t customOpCode;
    union
    {
        struct _VBVAHOSTCMD *pNext;
        uint32_t             offNext;
        uint64_t Data; /* the body is 64-bit aligned */
    } u;
    char body[1];
}VBVAHOSTCMD;

#define VBVAHOSTCMD_SIZE(_size) (sizeof(VBVAHOSTCMD) + (_size))
#define VBVAHOSTCMD_BODY(_pCmd, _tBody) ((_tBody*)(_pCmd)->body)
#define VBVAHOSTCMD_HDR(_pBody) ((VBVAHOSTCMD*)(((uint8_t*)_pBody) - RT_OFFSETOF(VBVAHOSTCMD, body)))
#define VBVAHOSTCMD_HDRSIZE (RT_OFFSETOF(VBVAHOSTCMD, body))

#pragma pack()

/* VBVACONF32::u32Index */
#define VBOX_VBVA_CONF32_MONITOR_COUNT  0
#define VBOX_VBVA_CONF32_HOST_HEAP_SIZE 1

typedef struct _VBVACONF32
{
    uint32_t u32Index;
    uint32_t u32Value;
} VBVACONF32;

typedef struct _VBVAINFOVIEW
{
    /* Index of the screen, assigned by the guest. */
    uint32_t u32ViewIndex;

    /* The screen offset in VRAM, the framebuffer starts here. */
    uint32_t u32ViewOffset;

    /* The size of the VRAM memory that can be used for the view. */
    uint32_t u32ViewSize;

    /* The recommended maximum size of the VRAM memory for the screen. */
    uint32_t u32MaxScreenSize;
} VBVAINFOVIEW;

typedef struct _VBVAINFOHEAP
{
    /* Absolute offset in VRAM of the start of the heap. */
    uint32_t u32HeapOffset;

    /* The size of the heap. */
    uint32_t u32HeapSize;

} VBVAINFOHEAP;

typedef struct _VBVAFLUSH
{
    uint32_t u32Reserved;

} VBVAFLUSH;

/* VBVAINFOSCREEN::u8Flags */
#define VBVA_SCREEN_F_NONE   0x0000
#define VBVA_SCREEN_F_ACTIVE 0x0001

typedef struct _VBVAINFOSCREEN
{
    /* Which view contains the screen. */
    uint32_t u32ViewIndex;

    /* Physical X origin relative to the primary screen. */
    int32_t i32OriginX;

    /* Physical Y origin relative to the primary screen. */
    int32_t i32OriginY;

    /* The scan line size in bytes. */
    uint32_t u32LineSize;

    /* Width of the screen. */
    uint32_t u32Width;

    /* Height of the screen. */
    uint32_t u32Height;

    /* Color depth. */
    uint16_t u16BitsPerPixel;

    /* VBVA_SCREEN_F_* */
    uint16_t u16Flags;
} VBVAINFOSCREEN;


/* VBVAENABLE::u32Flags */
#define VBVA_F_NONE    0x00000000
#define VBVA_F_ENABLE  0x00000001
#define VBVA_F_DISABLE 0x00000002

typedef struct _VBVAENABLE
{
    uint32_t u32Flags;
    uint32_t u32Offset;

} VBVAENABLE;

typedef struct _VBVAMOUSEPOINTERSHAPE
{
    /* The host result. */
    uint32_t u32Result;

    /* VBOX_MOUSE_POINTER_* bit flags. */
    uint32_t fu32Flags;

    /* X coordinate of the hot spot. */
    uint32_t u32HotX;

    /* Y coordinate of the hot spot. */
    uint32_t u32HotY;

    /* Width of the pointer in pixels. */
    uint32_t u32Width;

    /* Height of the pointer in scanlines. */
    uint32_t u32Height;

    /* Pointer data.
     *
     ****
     * The data consists of 1 bpp AND mask followed by 32 bpp XOR (color) mask.
     *
     * For pointers without alpha channel the XOR mask pixels are 32 bit values: (lsb)BGR0(msb).
     * For pointers with alpha channel the XOR mask consists of (lsb)BGRA(msb) 32 bit values.
     *
     * Guest driver must create the AND mask for pointers with alpha channel, so if host does not
     * support alpha, the pointer could be displayed as a normal color pointer. The AND mask can
     * be constructed from alpha values. For example alpha value >= 0xf0 means bit 0 in the AND mask.
     *
     * The AND mask is 1 bpp bitmap with byte aligned scanlines. Size of AND mask,
     * therefore, is cbAnd = (width + 7) / 8 * height. The padding bits at the
     * end of any scanline are undefined.
     *
     * The XOR mask follows the AND mask on the next 4 bytes aligned offset:
     * uint8_t *pXor = pAnd + (cbAnd + 3) & ~3
     * Bytes in the gap between the AND and the XOR mask are undefined.
     * XOR mask scanlines have no gap between them and size of XOR mask is:
     * cXor = width * 4 * height.
     ****
     *
     * Preallocate 4 bytes for accessing actual data as p->au8Data.
     */
    uint8_t au8Data[4];

} VBVAMOUSEPOINTERSHAPE;

#pragma pack()

#endif /* VBOX_WITH_HGSMI */

#endif
