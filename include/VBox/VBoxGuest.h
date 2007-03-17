/** @file
 * VBoxGuest - VirtualBox Guest Additions interface
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __VBox_VBoxGuest_h__
#define __VBox_VBoxGuest_h__

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <VBox/err.h>
#include <VBox/ostypes.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

#ifdef __WIN__
/** The support service name. */
#define VBOXGUEST_SERVICE_NAME       "VBoxGuest"
/** Win32 Device name. */
#define VBOXGUEST_DEVICE_NAME        "\\\\.\\VBoxGuest"
/** Global name for Win2k+ */
#define VBOXGUEST_DEVICE_NAME_GLOBAL "\\\\.\\Global\\VBoxGuest"
/** Win32 driver name */
#define VBOXGUEST_DEVICE_NAME_NT     L"\\Device\\VBoxGuest"
/** device name */
#define VBOXGUEST_DEVICE_NAME_DOS    L"\\DosDevices\\VBoxGuest"
#else /* !__WIN__ */
#define VBOXGUEST_DEVICE_NAME        "/dev/vboxadd"
#endif

/** VirtualBox vendor ID */
#define VBOX_PCI_VENDORID (0x80ee)

/** VMMDev PCI card identifiers */
#define VMMDEV_VENDORID VBOX_PCI_VENDORID
#define VMMDEV_DEVICEID (0xcafe)

/** VirtualBox graphics card identifiers */
#define VBOX_VENDORID VBOX_PCI_VENDORID
#define VBOX_VESA_VENDORID VBOX_PCI_VENDORID
#define VBOX_DEVICEID (0xbeef)
#define VBOX_VESA_DEVICEID (0xbeef)

/**
 * VBoxGuest port definitions
 * @{
 */

/** guest can (== wants to) handle absolute coordinates */
#define VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE      BIT(0)
/** host can (== wants to) send absolute coordinates */
#define VBOXGUEST_MOUSE_HOST_CAN_ABSOLUTE       BIT(1)
/** guest can *NOT* switch to software cursor and therefore depends on the host cursor */
#define VBOXGUEST_MOUSE_GUEST_NEEDS_HOST_CURSOR BIT(2)
/** host does NOT provide support for drawing the cursor itself (e.g. L4 console) */
#define VBOXGUEST_MOUSE_HOST_CANNOT_HWPOINTER   BIT(3)

/** fictive start address of the hypervisor physical memory for MmMapIoSpace */
#define HYPERVISOR_PHYSICAL_START  0xf8000000

/*
 * VMMDev Generic Request Interface
 */

/** port for generic request interface */
#define PORT_VMMDEV_REQUEST_OFFSET 0

/** Current version of the VMMDev interface.
 *
 * Additions are allowed to work only if
 * additions_major == vmmdev_current && additions_minor <= vmmdev_current.
 * Additions version is reported to host (VMMDev) by VMMDevReq_ReportGuestInfo.
 */
#define VMMDEV_VERSION_MAJOR (0x1)
#define VMMDEV_VERSION_MINOR (0x4)
#define VMMDEV_VERSION ((VMMDEV_VERSION_MAJOR << 16) | VMMDEV_VERSION_MINOR)

/**
 * VMMDev request types.
 * @note when updating this, adjust vmmdevGetRequestSize() as well
 */
typedef enum
{
    VMMDevReq_InvalidRequest             =  0,
    VMMDevReq_GetMouseStatus             =  1,
    VMMDevReq_SetMouseStatus             =  2,
    VMMDevReq_SetPointerShape            =  3,
    /** @todo implement on host side */
    VMMDevReq_GetHostVersion             =  4,
    VMMDevReq_Idle                       =  5,
    VMMDevReq_GetHostTime                = 10,
    VMMDevReq_GetHypervisorInfo          = 20,
    VMMDevReq_SetHypervisorInfo          = 21,
    VMMDevReq_SetPowerStatus             = 30,
    VMMDevReq_AcknowledgeEvents          = 41,
    VMMDevReq_CtlGuestFilterMask         = 42,
    VMMDevReq_ReportGuestInfo            = 50,
    VMMDevReq_GetDisplayChangeRequest    = 51,
    VMMDevReq_VideoModeSupported         = 52,
    VMMDevReq_GetHeightReduction         = 53,
#ifdef VBOX_HGCM
    VMMDevReq_HGCMConnect                = 60,
    VMMDevReq_HGCMDisconnect             = 61,
    VMMDevReq_HGCMCall                   = 62,
#endif
    VMMDevReq_VideoAccelEnable           = 70,
    VMMDevReq_VideoAccelFlush            = 71,
    VMMDevReq_QueryCredentials           = 100,
    VMMDevReq_ReportCredentialsJudgement = 101,
    VMMDevReq_SizeHack                   = 0x7fffffff
} VMMDevRequestType;

/** Version of VMMDevRequestHeader structure. */
#define VMMDEV_REQUEST_HEADER_VERSION (0x10001)

#pragma pack(4)
/** generic VMMDev request header */
typedef struct
{
    /** size of the structure in bytes (including body). Filled by caller */
    uint32_t size;
    /** version of the structure. Filled by caller */
    uint32_t version;
    /** type of the request */
    VMMDevRequestType requestType;
    /** return code. Filled by VMMDev */
    int32_t  rc;
    /** reserved fields */
    uint32_t reserved1;
    uint32_t reserved2;
} VMMDevRequestHeader;

/** mouse status request structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** mouse feature mask */
    uint32_t mouseFeatures;
    /** mouse x position */
    uint32_t pointerXPos;
    /** mouse y position */
    uint32_t pointerYPos;
} VMMDevReqMouseStatus;

/** Note VBOX_MOUSE_POINTER_* flags are used in guest video driver,
 *  values must be <= 0x8000 and must not be changed.
 */

/** pointer is visible */
#define VBOX_MOUSE_POINTER_VISIBLE (0x0001)
/** pointer has alpha channel */
#define VBOX_MOUSE_POINTER_ALPHA   (0x0002)
/** pointerData contains new pointer shape */
#define VBOX_MOUSE_POINTER_SHAPE   (0x0004)

/** mouse pointer shape/visibility change request */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** VBOX_MOUSE_POINTER_* bit flags */
    uint32_t fFlags;
    /** x coordinate of hot spot */
    uint32_t xHot;
    /** y coordinate of hot spot */
    uint32_t yHot;
    /** width of the pointer in pixels */
    uint32_t width;
    /** height of the pointer in scanlines */
    uint32_t height;
    /** Pointer data.
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
     * Preallocate 4 bytes for accessing actual data as p->pointerData
     */
    char pointerData[4];
} VMMDevReqMousePointer;

/** host version request structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** major version */
    uint32_t major;
    /** minor version */
    uint32_t minor;
    /** build number */
    uint32_t build;
} VMMDevReqHostVersion;

/** idle request structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
} VMMDevReqIdle;

/** host time request structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** time in milliseconds since unix epoch. Filled by VMMDev. */
    uint64_t time;
} VMMDevReqHostTime;

/** hypervisor info structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** guest virtual address of proposed hypervisor start */
    RTGCPTR hypervisorStart;
    /** hypervisor size in bytes */
    uint32_t hypervisorSize;
} VMMDevReqHypervisorInfo;

/** system power requests */
typedef enum
{
    VMMDevPowerState_Invalid   = 0,
    VMMDevPowerState_Pause     = 1,
    VMMDevPowerState_PowerOff  = 2,
    VMMDevPowerState_SaveState = 3,
    VMMDevPowerState_SizeHack = 0x7fffffff
} VMMDevPowerState;

/** system power status structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** power state request */
    VMMDevPowerState powerState;
} VMMDevPowerStateRequest;

/** pending events structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** pending event bitmap */
    uint32_t events;
} VMMDevEvents;

/** guest filter mask control */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** mask of events to be added to filter */
    uint32_t u32OrMask;
    /** mask of events to be removed from filter */
    uint32_t u32NotMask;
} VMMDevCtlGuestFilterMask;

/** guest information structure */
typedef struct VBoxGuestInfo
{
    /** The VMMDev interface version expected by additions. */
    uint32_t additionsVersion;
    /** guest OS type */
    OSType osType;
    /** @todo */
} VBoxGuestInfo;

/** guest information structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** Guest information. */
    VBoxGuestInfo guestInfo;
} VMMDevReportGuestInfo;

/** display change request structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** horizontal pixel resolution (0 = do not change) */
    uint32_t xres;
    /** vertical pixel resolution (0 = do not change) */
    uint32_t yres;
    /** bits per pixel (0 = do not change) */
    uint32_t bpp;
    /** Flag that the request is an acknowlegement for the VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST.
     *  Values: 0 - just querying, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST - event acknowledged.
     */
    uint32_t eventAck;
} VMMDevDisplayChangeRequest;

/** video mode supported request structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** horizontal pixel resolution (input) */
    uint32_t width;
    /** vertical pixel resolution (input) */
    uint32_t height;
    /** bits per pixel (input) */
    uint32_t bpp;
    /** supported flag (output) */
    bool fSupported;
} VMMDevVideoModeSupportedRequest;

/** video modes height reduction request structure */
typedef struct
{
    /** header */
    VMMDevRequestHeader header;
    /** height reduction in pixels (output) */
    uint32_t heightReduction;
} VMMDevGetHeightReductionRequest;

#pragma pack()

#ifdef VBOX_HGCM

/** HGCM flags.
 *  @{
 */
#define VBOX_HGCM_REQ_DONE      (0x1)
#define VBOX_HGCM_REQ_CANCELLED (0x2)
/** @} */

#pragma pack(4)
typedef struct _VMMDevHGCMRequestHeader
{
    /** Request header. */
    VMMDevRequestHeader header;

    /** HGCM flags. */
    uint32_t fu32Flags;

    /** Result code. */
    int32_t result;
} VMMDevHGCMRequestHeader;

/** HGCM service location types. */
typedef enum
{
    VMMDevHGCMLoc_Invalid    = 0,
    VMMDevHGCMLoc_LocalHost  = 1,
    VMMDevHGCMLoc_LocalHost_Existing = 2,
    VMMDevHGCMLoc_SizeHack   = 0x7fffffff
} HGCMServiceLocationType;

typedef struct
{
    char achName[128];
} HGCMServiceLocationHost;

typedef struct HGCMSERVICELOCATION
{
    /** Type of the location. */
    HGCMServiceLocationType type;

    union
    {
        HGCMServiceLocationHost host;
    } u;
} HGCMServiceLocation;

typedef struct
{
    /* request header */
    VMMDevHGCMRequestHeader header;

    /** IN: Description of service to connect to. */
    HGCMServiceLocation loc;

    /** OUT: Client identifier assigned by local instance of HGCM. */
    uint32_t u32ClientID;
} VMMDevHGCMConnect;

typedef struct
{
    /* request header */
    VMMDevHGCMRequestHeader header;

    /** IN: Client identifier. */
    uint32_t u32ClientID;
} VMMDevHGCMDisconnect;

typedef enum
{
    VMMDevHGCMParmType_Invalid    = 0,
    VMMDevHGCMParmType_32bit      = 1,
    VMMDevHGCMParmType_64bit      = 2,
    VMMDevHGCMParmType_PhysAddr   = 3,
    VMMDevHGCMParmType_LinAddr    = 4, /**< In and Out */
    VMMDevHGCMParmType_LinAddr_In = 5, /**< In (read) */
    VMMDevHGCMParmType_LinAddr_Out= 6, /**< Out (write) */
    VMMDevHGCMParmType_SizeHack   = 0x7fffffff
} HGCMFunctionParameterType;

typedef struct _HGCMFUNCTIONPARAMETER
{
    HGCMFunctionParameterType type;
    union
    {
        uint32_t   value32;
        uint64_t   value64;
        struct
        {
            uint32_t size;

            union
            {
                RTGCPHYS physAddr;
                RTGCPTR  linearAddr;
            } u;
        } Pointer;
    } u;
} HGCMFunctionParameter;

typedef struct
{
    /* request header */
    VMMDevHGCMRequestHeader header;

    /** IN: Client identifier. */
    uint32_t u32ClientID;
    /** IN: Service function number. */
    uint32_t u32Function;
    /** IN: Number of parameters. */
    uint32_t cParms;
    /** Parameters follow in form: HGCMFunctionParameter aParms[X]; */
} VMMDevHGCMCall;
#pragma pack()

#define VMMDEV_HGCM_CALL_PARMS(a) ((HGCMFunctionParameter *)((char *)a + sizeof (VMMDevHGCMCall)))

#endif /* VBOX_HGCM */


#define VBVA_F_STATUS_ACCEPTED (0x01)
#define VBVA_F_STATUS_ENABLED  (0x02)

#pragma pack(4)

typedef struct _VMMDevVideoAccelEnable
{
    /* request header */
    VMMDevRequestHeader header;

    /** 0 - disable, !0 - enable. */
    uint32_t u32Enable;

    /** The size of VBVAMEMORY::au8RingBuffer expected by driver.
     *  The host will refuse to enable VBVA if the size is not equal to
     *  VBVA_RING_BUFFER_SIZE.
     */
    uint32_t cbRingBuffer;

    /** Guest initializes the status to 0. Host sets appropriate VBVA_F_STATUS_ flags. */
    uint32_t fu32Status;

} VMMDevVideoAccelEnable;

typedef struct _VMMDevVideoAccelFlush
{
    /* request header */
    VMMDevRequestHeader header;

} VMMDevVideoAccelFlush;

#pragma pack()

#pragma pack(1)

/** VBVA command header. */
typedef struct _VBVACMDHDR
{
   /** Coordinates of affected rectangle. */
   int16_t x;
   int16_t y;
   uint16_t w;
   uint16_t h;
} VBVACMDHDR;

/* VBVA order codes. Must be >= 0, because the VRDP server internally
 * uses negative values to mark some operations.
 * Values are important since they are used as an index in the
 * "supported orders" bit mask.
 */
#define VBVA_VRDP_DIRTY_RECT     (0)
#define VBVA_VRDP_SOLIDRECT      (1)
#define VBVA_VRDP_SOLIDBLT       (2)
#define VBVA_VRDP_DSTBLT         (3)
#define VBVA_VRDP_SCREENBLT      (4)
#define VBVA_VRDP_PATBLT_BRUSH   (5)
#define VBVA_VRDP_MEMBLT         (6)
#define VBVA_VRDP_CACHED_BITMAP  (7)
#define VBVA_VRDP_DELETED_BITMAP (8)
#define VBVA_VRDP_LINE           (9)
#define VBVA_VRDP_BOUNDS         (10)
#define VBVA_VRDP_REPEAT         (11)
#define VBVA_VRDP_POLYLINE       (12)
#define VBVA_VRDP_ELLIPSE        (13)
#define VBVA_VRDP_SAVESCREEN     (14)

#define VBVA_VRDP_INDEX_TO_BIT(__index) (1 << (__index))

/* 128 bit bitmap hash. */
typedef uint8_t VRDPBITMAPHASH[16];

typedef struct _VRDPORDERPOINT
{
    int16_t  x;
    int16_t  y;
} VRDPORDERPOINT;

typedef struct _VRDPORDERPOLYPOINTS
{
    uint8_t  c;
    VRDPORDERPOINT a[16];
} VRDPORDERPOLYPOINTS;

typedef struct _VRDPORDERAREA
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
} VRDPORDERAREA;

typedef struct _VRDPORDERBOUNDS
{
    VRDPORDERPOINT pt1;
    VRDPORDERPOINT pt2;
} VRDPORDERBOUNDS;

typedef struct _VRDPORDERREPEAT
{
    VRDPORDERBOUNDS bounds;
} VRDPORDERREPEAT;


/* Header for bitmap bits in VBVA VRDP operations. */
typedef struct _VRDPDATABITS
{
    /* Size of bitmap data without the header. */
    uint32_t cb;
    int16_t  x;
    int16_t  y;
    uint16_t cWidth;
    uint16_t cHeight;
    uint8_t cbPixel;
} VRDPDATABITS;

typedef struct _VRDPORDERSOLIDRECT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    uint32_t rgb;
} VRDPORDERSOLIDRECT;

typedef struct _VRDPORDERSOLIDBLT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    uint32_t rgb;
    uint8_t  rop;
} VRDPORDERSOLIDBLT;

typedef struct _VRDPORDERDSTBLT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    uint8_t  rop;
} VRDPORDERDSTBLT;

typedef struct _VRDPORDERSCREENBLT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    int16_t  xSrc;
    int16_t  ySrc;
    uint8_t  rop;
} VRDPORDERSCREENBLT;

typedef struct _VRDPORDERPATBLTBRUSH
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    int8_t   xSrc;
    int8_t   ySrc;
    uint32_t rgbFG;
    uint32_t rgbBG;
    uint8_t  rop;
    uint8_t  pattern[8];
} VRDPORDERPATBLTBRUSH;

typedef struct _VRDPORDERMEMBLT
{
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    int16_t  xSrc;
    int16_t  ySrc;
    uint8_t  rop;
    VRDPBITMAPHASH hash;
} VRDPORDERMEMBLT;

typedef struct _VRDPORDERCACHEDBITMAP
{
    VRDPBITMAPHASH hash;
    /* VRDPDATABITS and the bitmap data follows. */
} VRDPORDERCACHEDBITMAP;

typedef struct _VRDPORDERDELETEDBITMAP
{
    VRDPBITMAPHASH hash;
} VRDPORDERDELETEDBITMAP;

typedef struct _VRDPORDERLINE
{
    int16_t  x1;
    int16_t  y1;
    int16_t  x2;
    int16_t  y2;
    int16_t  xBounds1;
    int16_t  yBounds1;
    int16_t  xBounds2;
    int16_t  yBounds2;
    uint8_t  mix;
    uint32_t rgb;
} VRDPORDERLINE;

typedef struct _VRDPORDERPOLYLINE
{
    VRDPORDERPOINT ptStart;
    uint8_t  mix;
    uint32_t rgb;
    VRDPORDERPOLYPOINTS points;
} VRDPORDERPOLYLINE;

typedef struct _VRDPORDERELLIPSE
{
    VRDPORDERPOINT pt1;
    VRDPORDERPOINT pt2;
    uint8_t  mix;
    uint8_t  fillMode;
    uint32_t rgb;
} VRDPORDERELLIPSE;

typedef struct _VRDPORDERSAVESCREEN
{
    VRDPORDERPOINT pt1;
    VRDPORDERPOINT pt2;
    uint8_t ident;
    uint8_t restore;
} VRDPORDERSAVESCREEN;
#pragma pack()

/* The VBVA ring buffer is suitable for transferring large (< 2gb) amount of data.
 * For example big bitmaps which do not fit to the buffer.
 *
 * Guest starts writing to the buffer by initializing a record entry in the
 * aRecords queue. VBVA_F_RECORD_PARTIAL indicates that the record is being
 * written. As data is written to the ring buffer, the guest increases off32End
 * for the record.
 *
 * The host reads the aRecords on flushes and processes all completed records.
 * When host encounters situation when only a partial record presents and
 * cbRecord & ~VBVA_F_RECORD_PARTIAL >= VBVA_RING_BUFFER_SIZE - VBVA_RING_BUFFER_THRESHOLD,
 * the host fetched all record data and updates off32Head. After that on each flush
 * the host continues fetching the data until the record is completed.
 *
 */

#define VBVA_RING_BUFFER_SIZE        (_4M - _1K)
#define VBVA_RING_BUFFER_THRESHOLD   (4 * _1K)

#define VBVA_MAX_RECORDS (64)

#define VBVA_F_MODE_ENABLED         (0x00000001)
#define VBVA_F_MODE_VRDP            (0x00000002)
#define VBVA_F_MODE_VRDP_RESET      (0x00000004)
#define VBVA_F_MODE_VRDP_ORDER_MASK (0x00000008)

#define VBVA_F_RECORD_PARTIAL   (0x80000000)

#pragma pack(1)
typedef struct _VBVARECORD
{
    /** The length of the record. Changed by guest. */
    uint32_t cbRecord;
} VBVARECORD;

typedef struct _VBVAMEMORY
{
    /** VBVA_F_MODE_* */
    uint32_t fu32ModeFlags;

    /** The offset where the data start in the buffer. */
    uint32_t off32Data;
    /** The offset where next data must be placed in the buffer. */
    uint32_t off32Free;

    /** The ring buffer for data. */
    uint8_t  au8RingBuffer[VBVA_RING_BUFFER_SIZE];

    /** The queue of record descriptions. */
    VBVARECORD aRecords[VBVA_MAX_RECORDS];
    uint32_t indexRecordFirst;
    uint32_t indexRecordFree;

    /* RDP orders supported by the client. The guest reports only them
     * and falls back to DIRTY rects for not supported ones.
     *
     * (1 << VBVA_VRDP_*)
     */
    uint32_t fu32SupportedOrders;

} VBVAMEMORY;
#pragma pack()

/** @} */


/**
 * VMMDev RAM
 * @{
 */

#pragma pack(1)
/** Layout of VMMDEV RAM region that contains information for guest */
typedef struct
{
    /** size */
    uint32_t u32Size;
    /** version */
    uint32_t u32Version;

    union {
        /** Flag telling that VMMDev set the IRQ and acknowlegment is required */
        struct {
            bool fHaveEvents;
        } V1_04;

        struct {
            /** Pending events flags, set by host. */
            uint32_t u32HostEvents;
            /** Mask of events the guest wants to see, set by guest. */
            uint32_t u32GuestEventMask;
        } V1_03;
    } V;

    VBVAMEMORY vbvaMemory;

} VMMDevMemory;
#pragma pack()

/** Version of VMMDevMemory structure. */
#define VMMDEV_MEMORY_VERSION (1)

/** @} */


/**
 * VMMDev events.
 * @{
 */

/** Host mouse capabilities has been changed. */
#define VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED BIT(0)
/** HGCM event. */
#define VMMDEV_EVENT_HGCM                       BIT(1)
/** A display change request has been issued. */
#define VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST     BIT(2)
/** Credentials are available for judgement. */
#define VMMDEV_EVENT_JUDGE_CREDENTIALS          BIT(3)
/** The guest has been restored. */
#define VMMDEV_EVENT_RESTORED                   BIT(4)

/** @} */


/**
 * VBoxGuest IOCTL codes and structures.
 * @{
 * IOCTL function numbers start from 2048, because MSDN says the
 * second parameter of CTL_CODE macro (function) must be <= 2048 and <= 4095 for IHVs.
 * The IOCTL number algorithm corresponds to CTL_CODE on Windows but for Linux IOCTLs,
 * we also encode the data size, so we need an additional parameter.
 */
#if defined(__WIN__)
#define IOCTL_CODE(DeviceType, Function, Method, Access, DataSize_ignored) \
    ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#else /* unix: */
#define IOCTL_CODE(DeviceType, Function, Method_ignored, Access_ignored, DataSize) \
    ( (3 << 30) | ((DeviceType) << 8) | (Function) | ((DataSize) << 16) )
#define METHOD_BUFFERED        0
#define FILE_WRITE_ACCESS      0x0002
#define FILE_DEVICE_UNKNOWN    0x00000022
#endif

/** IOCTL to VBoxGuest to query the VMMDev IO port region start. */
#define IOCTL_VBOXGUEST_GETVMMDEVPORT IOCTL_CODE(FILE_DEVICE_UNKNOWN, 2048, METHOD_BUFFERED, FILE_WRITE_ACCESS, sizeof(VBoxGuestPortInfo))

#pragma pack(4)
typedef struct _VBoxGuestPortInfo
{
    uint32_t portAddress;
    VMMDevMemory *pVMMDevMemory;
} VBoxGuestPortInfo;

/** IOCTL to VBoxGuest to wait for a VMMDev host notification */
#define IOCTL_VBOXGUEST_WAITEVENT IOCTL_CODE(FILE_DEVICE_UNKNOWN, 2049, METHOD_BUFFERED, FILE_WRITE_ACCESS, sizeof(VBoxGuestWaitEventInfo))

/**
 * Result codes for VBoxGuestWaitEventInfo::u32Result
 * @{
 */
/** Successful completion, an event occured. */
#define VBOXGUEST_WAITEVENT_OK          (0)
/** Successful completion, timed out. */
#define VBOXGUEST_WAITEVENT_TIMEOUT     (1)
/** Wait was interrupted. */
#define VBOXGUEST_WAITEVENT_INTERRUPTED (2)
/** An error occured while processing the request. */
#define VBOXGUEST_WAITEVENT_ERROR       (3)
/** @} */

/** Input and output buffers layout of the IOCTL_VBOXGUEST_WAITEVENT */
typedef struct _VBoxGuestWaitEventInfo
{
    /** timeout in milliseconds */
    uint32_t u32TimeoutIn;
    /** events to wait for */
    uint32_t u32EventMaskIn;
    /** result code */
    uint32_t u32Result;
    /** events occured */
    uint32_t u32EventFlagsOut;
} VBoxGuestWaitEventInfo;

/** IOCTL to VBoxGuest to perform a VMM request */
#define IOCTL_VBOXGUEST_VMMREQUEST IOCTL_CODE(FILE_DEVICE_UNKNOWN, 2050, METHOD_BUFFERED, FILE_WRITE_ACCESS, sizeof(VMMDevRequestHeader))

/** IOCTL to VBoxGuest to control event filter mask */

typedef struct _VBoxGuestFilterMaskInfo
{
    uint32_t u32OrMask;
    uint32_t u32NotMask;
} VBoxGuestFilterMaskInfo;
#pragma pack()

#define IOCTL_VBOXGUEST_CTL_FILTER_MASK IOCTL_CODE(FILE_DEVICE_UNKNOWN, 2051, METHOD_BUFFERED, FILE_WRITE_ACCESS, sizeof (VBoxGuestFilterMaskInfo))

#ifdef VBOX_HGCM
/* These structures are shared between the driver and other binaries,
 * therefore packing must be defined explicitely.
 */
#pragma pack(1)
typedef struct _VBoxGuestHGCMConnectInfo
{
    uint32_t result;          /**< OUT */
    HGCMServiceLocation Loc;  /**< IN */
    uint32_t u32ClientID;     /**< OUT */
} VBoxGuestHGCMConnectInfo;

typedef struct _VBoxGuestHGCMDisconnectInfo
{
    uint32_t result;          /**< OUT */
    uint32_t u32ClientID;     /**< IN */
} VBoxGuestHGCMDisconnectInfo;

typedef struct _VBoxGuestHGCMCallInfo
{
    uint32_t result;          /**< OUT Host HGCM return code.*/
    uint32_t u32ClientID;     /**< IN  The id of the caller. */
    uint32_t u32Function;     /**< IN  Function number. */
    uint32_t cParms;          /**< IN  How many parms. */
    /* Parameters follow in form HGCMFunctionParameter aParms[cParms] */
} VBoxGuestHGCMCallInfo;
#pragma pack()

#define IOCTL_VBOXGUEST_HGCM_CONNECT    IOCTL_CODE(FILE_DEVICE_UNKNOWN, 3072, METHOD_BUFFERED, FILE_WRITE_ACCESS, sizeof(VBoxGuestHGCMConnectInfo))
#define IOCTL_VBOXGUEST_HGCM_DISCONNECT IOCTL_CODE(FILE_DEVICE_UNKNOWN, 3073, METHOD_BUFFERED, FILE_WRITE_ACCESS, sizeof(VBoxGuestHGCMDisconnectInfo))
#define IOCTL_VBOXGUEST_HGCM_CALL       IOCTL_CODE(FILE_DEVICE_UNKNOWN, 3074, METHOD_BUFFERED, FILE_WRITE_ACCESS, sizeof(VBoxGuestHGCMCallInfo))

#define VBOXGUEST_HGCM_CALL_PARMS(a) ((HGCMFunctionParameter *)((uint8_t *)(a) + sizeof (VBoxGuestHGCMCallInfo)))

#endif /* VBOX_HGCM */

/*
 * Credentials request flags and structure
 */

#define VMMDEV_CREDENTIALS_STRLEN           128

/** query from host whether credentials are present */
#define VMMDEV_CREDENTIALS_QUERYPRESENCE     BIT(1)
/** read credentials from host (can be combined with clear) */
#define VMMDEV_CREDENTIALS_READ              BIT(2)
/** clear credentials on host (can be combined with read) */
#define VMMDEV_CREDENTIALS_CLEAR             BIT(3)
/** read credentials for judgement in the guest */
#define VMMDEV_CREDENTIALS_READJUDGE         BIT(8)
/** clear credentials for judegement on the host */
#define VMMDEV_CREDENTIALS_CLEARJUDGE        BIT(9)
/** report credentials acceptance by guest */
#define VMMDEV_CREDENTIALS_JUDGE_OK          BIT(10)
/** report credentials denial by guest */
#define VMMDEV_CREDENTIALS_JUDGE_DENY        BIT(11)
/** report that no judgement could be made by guest */
#define VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT BIT(12)

/** flag telling the guest that credentials are present */
#define VMMDEV_CREDENTIALS_PRESENT           BIT(16)
/** flag telling guest that local logons should be prohibited */
#define VMMDEV_CREDENTIALS_NOLOCALLOGON      BIT(17)

/** credentials request structure */
#pragma pack(4)
typedef struct _VMMDevCredentials
{
    /* request header */
    VMMDevRequestHeader header;
    /* request flags (in/out) */
    uint32_t u32Flags;
    /* user name (UTF-8) (out) */
    char szUserName[VMMDEV_CREDENTIALS_STRLEN];
    /* password (UTF-8) (out) */
    char szPassword[VMMDEV_CREDENTIALS_STRLEN];
    /* domain name (UTF-8) (out) */
    char szDomain[VMMDEV_CREDENTIALS_STRLEN];
} VMMDevCredentials;
#pragma pack()

/** inline helper to determine the request size for the given operation */
DECLINLINE(size_t) vmmdevGetRequestSize(VMMDevRequestType requestType)
{
    switch (requestType)
    {
        case VMMDevReq_GetMouseStatus:
        case VMMDevReq_SetMouseStatus:
            return sizeof(VMMDevReqMouseStatus);
        case VMMDevReq_SetPointerShape:
            return sizeof(VMMDevReqMousePointer);
        case VMMDevReq_GetHostVersion:
            return sizeof(VMMDevReqHostVersion);
        case VMMDevReq_Idle:
            return sizeof(VMMDevReqIdle);
        case VMMDevReq_GetHostTime:
            return sizeof(VMMDevReqHostTime);
        case VMMDevReq_GetHypervisorInfo:
        case VMMDevReq_SetHypervisorInfo:
            return sizeof(VMMDevReqHypervisorInfo);
        case VMMDevReq_SetPowerStatus:
            return sizeof(VMMDevPowerStateRequest);
        case VMMDevReq_AcknowledgeEvents:
            return sizeof(VMMDevEvents);
        case VMMDevReq_ReportGuestInfo:
            return sizeof(VMMDevReportGuestInfo);
        case VMMDevReq_GetDisplayChangeRequest:
            return sizeof(VMMDevDisplayChangeRequest);
        case VMMDevReq_VideoModeSupported:
            return sizeof(VMMDevVideoModeSupportedRequest);
        case VMMDevReq_GetHeightReduction:
            return sizeof(VMMDevGetHeightReductionRequest);
#ifdef VBOX_HGCM
        case VMMDevReq_HGCMConnect:
            return sizeof(VMMDevHGCMConnect);
        case VMMDevReq_HGCMDisconnect:
            return sizeof(VMMDevHGCMDisconnect);
        case VMMDevReq_HGCMCall:
            return sizeof(VMMDevHGCMCall);
#endif
        case VMMDevReq_VideoAccelEnable:
            return sizeof(VMMDevVideoAccelEnable);
        case VMMDevReq_VideoAccelFlush:
            return sizeof(VMMDevVideoAccelFlush);
        case VMMDevReq_QueryCredentials:
            return sizeof(VMMDevCredentials);
        default:
            return 0;
    }
}

/**
 * Initializes a request structure.
 *
 */
DECLINLINE(int) vmmdevInitRequest(VMMDevRequestHeader *req, VMMDevRequestType type)
{
    uint32_t requestSize;
    if (!req)
        return VERR_INVALID_PARAMETER;
    requestSize = (uint32_t)vmmdevGetRequestSize(type);
    if (!requestSize)
        return VERR_INVALID_PARAMETER;
    req->size        = requestSize;
    req->version     = VMMDEV_REQUEST_HEADER_VERSION;
    req->requestType = type;
    req->rc          = VERR_GENERAL_FAILURE;
    req->reserved1   = 0;
    req->reserved2   = 0;
    return VINF_SUCCESS;
}

/** @} */

#endif /* __VBox_VBoxGuest_h__ */
