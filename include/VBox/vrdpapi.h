/** @file
 * VBox Remote Desktop Protocol:
 * Public APIs.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_vrdpapi_h
#define ___VBox_vrdpapi_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

#ifdef IN_RING0
# error "There are no VRDP APIs available in Ring-0 Host Context!"
#endif
#ifdef IN_GC
# error "There are no VRDP APIs available Guest Context!"
#endif


/** @defgroup grp_vrdp VRDP
 * VirtualBox Remote Desktop Protocol (VRDP) interface that lets to use
 * the VRDP server.
 * @{
 */

/** Default port that VRDP binds to. */
#define VRDP_DEFAULT_PORT (3389)

__BEGIN_DECLS

/* Forward declaration of the VRDP server instance handle. */
#ifdef __cplusplus
class VRDPServer;
typedef class VRDPServer *HVRDPSERVER;
#else
struct VRDPServer;
typedef struct VRDPServer *HVRDPSERVER;
#endif /* __cplusplus */

/* Callback based VRDP server interface declarations. */

#if defined(IN_VRDP)
# define VRDPDECL(type)       DECLEXPORT(type) RTCALL
#else
# define VRDPDECL(type)       DECLIMPORT(type) RTCALL
#endif /* IN_VRDP */

/** The color mouse pointer information. */
typedef struct _VRDPCOLORPOINTER
{
    uint16_t u16HotX;
    uint16_t u16HotY;
    uint16_t u16Width;
    uint16_t u16Height;
    uint16_t u16MaskLen;
    uint16_t u16DataLen;
    /* The mask and the bitmap follow. */
} VRDPCOLORPOINTER;

/** Audio format information packed in a 32 bit value. */
typedef uint32_t VRDPAUDIOFORMAT;

/** Constructs 32 bit value for given frequency, number of channel and bits per sample. */
#define VRDP_AUDIO_FMT_MAKE(freq, c, bps, s) ((((s) & 0x1) << 28) + (((bps) & 0xFF) << 20) + (((c) & 0xF) << 16) + ((freq) & 0xFFFF))

/** Decode frequency. */
#define VRDP_AUDIO_FMT_SAMPLE_FREQ(a) ((a) & 0xFFFF)
/** Decode number of channels. */
#define VRDP_AUDIO_FMT_CHANNELS(a) (((a) >> 16) & 0xF)
/** Decode number signess. */
#define VRDP_AUDIO_FMT_SIGNED(a) (((a) >> 28) & 0x1)
/** Decode number of bits per sample. */
#define VRDP_AUDIO_FMT_BITS_PER_SAMPLE(a) (((a) >> 20) & 0xFF)
/** Decode number of bytes per sample. */
#define VRDP_AUDIO_FMT_BYTES_PER_SAMPLE(a) ((VRDP_AUDIO_FMT_BITS_PER_SAMPLE(a) + 7) / 8)

/*
 * Remote USB protocol.
 */

/* The version of Remote USB Protocol. */
#define VRDP_USB_VERSION (1)

/** USB backend operations. */
#define VRDP_USB_REQ_OPEN              (0)
#define VRDP_USB_REQ_CLOSE             (1)
#define VRDP_USB_REQ_RESET             (2)
#define VRDP_USB_REQ_SET_CONFIG        (3)
#define VRDP_USB_REQ_CLAIM_INTERFACE   (4)
#define VRDP_USB_REQ_RELEASE_INTERFACE (5)
#define VRDP_USB_REQ_INTERFACE_SETTING (6)
#define VRDP_USB_REQ_QUEUE_URB         (7)
#define VRDP_USB_REQ_REAP_URB          (8)
#define VRDP_USB_REQ_CLEAR_HALTED_EP   (9)
#define VRDP_USB_REQ_CANCEL_URB        (10)

/** USB service operations. */
#define VRDP_USB_REQ_DEVICE_LIST       (11)
#define VRDP_USB_REQ_NEGOTIATE         (12)

/** An operation completion status is a byte. */
typedef uint8_t VRDPUSBSTATUS;

/** USB device identifier is an 32 bit value. */
typedef uint32_t VRDPUSBDEVID;

/** Status codes. */
#define VRDP_USB_STATUS_SUCCESS        ((VRDPUSBSTATUS)0)
#define VRDP_USB_STATUS_ACCESS_DENIED  ((VRDPUSBSTATUS)1)
#define VRDP_USB_STATUS_DEVICE_REMOVED ((VRDPUSBSTATUS)2)

/*
 * Data structures to use with VRDPUSBRequest.
 * The *RET* structures always represent the layout of VRDP data.
 * The *PARM* structures normally the same as VRDP layout.
 * However the VRDP_USB_REQ_QUEUE_URB_PARM has a pointer to
 * URB data in place where actual data will be in VRDP layout.
 *
 * Since replies (*RET*) are asynchronous, the 'success'
 * replies are not required for operations which return
 * only the status code (VRDPUSBREQRETHDR only):
 *  VRDP_USB_REQ_OPEN
 *  VRDP_USB_REQ_RESET
 *  VRDP_USB_REQ_SET_CONFIG
 *  VRDP_USB_REQ_CLAIM_INTERFACE
 *  VRDP_USB_REQ_RELEASE_INTERFACE
 *  VRDP_USB_REQ_INTERFACE_SETTING
 *  VRDP_USB_REQ_CLEAR_HALTED_EP
 *
 */

/* VRDP layout has no aligments. */
#pragma pack(1)
/* Common header for all VRDP USB packets. After the reply hdr follows *PARM* or *RET* data. */
typedef struct _VRDPUSBPKTHDR
{
    /* Total length of the reply NOT including the 'length' field. */
    uint32_t length;
    /* The operation code for which the reply was sent by the client. */
    uint8_t code;
} VRDPUSBPKTHDR;

/* Common header for all return structures. */
typedef struct _VRDPUSBREQRETHDR
{
    /* Device status. */
    VRDPUSBSTATUS status;
    /* Device id. */
    VRDPUSBDEVID id;
} VRDPUSBREQRETHDR;


/* VRDP_USB_REQ_OPEN
 */
typedef struct _VRDP_USB_REQ_OPEN_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
} VRDP_USB_REQ_OPEN_PARM;

typedef struct _VRDP_USB_REQ_OPEN_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_OPEN_RET;


/* VRDP_USB_REQ_CLOSE
 */
typedef struct _VRDP_USB_REQ_CLOSE_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
} VRDP_USB_REQ_CLOSE_PARM;

/* The close request has no returned data. */


/* VRDP_USB_REQ_RESET
 */
typedef struct _VRDP_USB_REQ_RESET_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
} VRDP_USB_REQ_RESET_PARM;

typedef struct _VRDP_USB_REQ_RESET_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_RESET_RET;


/* VRDP_USB_REQ_SET_CONFIG
 */
typedef struct _VRDP_USB_REQ_SET_CONFIG_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t configuration;
} VRDP_USB_REQ_SET_CONFIG_PARM;

typedef struct _VRDP_USB_REQ_SET_CONFIG_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_SET_CONFIG_RET;


/* VRDP_USB_REQ_CLAIM_INTERFACE
 */
typedef struct _VRDP_USB_REQ_CLAIM_INTERFACE_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t iface;
} VRDP_USB_REQ_CLAIM_INTERFACE_PARM;

typedef struct _VRDP_USB_REQ_CLAIM_INTERFACE_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_CLAIM_INTERFACE_RET;


/* VRDP_USB_REQ_RELEASE_INTERFACE
 */
typedef struct _VRDP_USB_REQ_RELEASE_INTERFACE_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t iface;
} VRDP_USB_REQ_RELEASE_INTERFACE_PARM;

typedef struct _VRDP_USB_REQ_RELEASE_INTERFACE_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_RELEASE_INTERFACE_RET;


/* VRDP_USB_REQ_INTERFACE_SETTING
 */
typedef struct _VRDP_USB_REQ_INTERFACE_SETTING_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t iface;
    uint8_t setting;
} VRDP_USB_REQ_INTERFACE_SETTING_PARM;

typedef struct _VRDP_USB_REQ_INTERFACE_SETTING_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_INTERFACE_SETTING_RET;


/* VRDP_USB_REQ_QUEUE_URB
 */

#define VRDP_USB_TRANSFER_TYPE_CTRL (0)
#define VRDP_USB_TRANSFER_TYPE_ISOC (1)
#define VRDP_USB_TRANSFER_TYPE_BULK (2)
#define VRDP_USB_TRANSFER_TYPE_INTR (3)
#define VRDP_USB_TRANSFER_TYPE_MSG  (4)

#define VRDP_USB_DIRECTION_SETUP (0)
#define VRDP_USB_DIRECTION_IN    (1)
#define VRDP_USB_DIRECTION_OUT   (2)

typedef struct _VRDP_USB_REQ_QUEUE_URB_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint32_t handle;    /* Distinguishes that particular URB. Later used in CancelURB and returned by ReapURB */
    uint8_t type;
    uint8_t ep;
    uint8_t direction;
    uint32_t urblen;    /* Length of the URB. */
    uint32_t datalen;   /* Length of the data. */
    void *data;         /* In RDP layout the data follow. */
} VRDP_USB_REQ_QUEUE_URB_PARM;

/* The queue URB has no explicit return. The reap URB reply will be
 * eventually the indirect result.
 */


/* VRDP_USB_REQ_REAP_URB
 * Notificationg from server to client that server expects an URB
 * from any device.
 * Only sent if negotiated URB return method is polling.
 * Normally, the client will send URBs back as soon as they are ready.
 */
typedef struct _VRDP_USB_REQ_REAP_URB_PARM
{
    uint8_t code;
} VRDP_USB_REQ_REAP_URB_PARM;


#define VRDP_USB_XFER_OK    (0)
#define VRDP_USB_XFER_STALL (1)
#define VRDP_USB_XFER_DNR   (2)
#define VRDP_USB_XFER_CRC   (3)

#define VRDP_USB_REAP_FLAG_CONTINUED (0x0)
#define VRDP_USB_REAP_FLAG_LAST      (0x1)

#define VRDP_USB_REAP_VALID_FLAGS    (VRDP_USB_REAP_FLAG_LAST)

typedef struct _VRDPUSBREQREAPURBBODY
{
    VRDPUSBDEVID     id;        /* From which device the URB arrives. */
    uint8_t          flags;     /* VRDP_USB_REAP_FLAG_* */
    uint8_t          error;     /* VRDP_USB_XFER_* */
    uint32_t         handle;    /* Handle of returned URB. Not 0. */
    uint32_t         len;       /* Length of data actually transferred. */
    /* Data follow. */
} VRDPUSBREQREAPURBBODY;

typedef struct _VRDP_USB_REQ_REAP_URB_RET
{
    /* The REAP URB has no header, only completed URBs are returned. */
    VRDPUSBREQREAPURBBODY body;
    /* Another body may follow, depending on flags. */
} VRDP_USB_REQ_REAP_URB_RET;


/* VRDP_USB_REQ_CLEAR_HALTED_EP
 */
typedef struct _VRDP_USB_REQ_CLEAR_HALTED_EP_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t ep;
} VRDP_USB_REQ_CLEAR_HALTED_EP_PARM;

typedef struct _VRDP_USB_REQ_CLEAR_HALTED_EP_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_CLEAR_HALTED_EP_RET;


/* VRDP_USB_REQ_CANCEL_URB
 */
typedef struct _VRDP_USB_REQ_CANCEL_URB_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint32_t handle;
} VRDP_USB_REQ_CANCEL_URB_PARM;

/* The cancel URB request has no return. */


/* VRDP_USB_REQ_DEVICE_LIST
 *
 * Server polls USB devices on client by sending this request
 * periodically. Client sends back a list of all devices
 * connected to it. Each device is assigned with an identifier,
 * that is used to distinguish the particular device.
 */
typedef struct _VRDP_USB_REQ_DEVICE_LIST_PARM
{
    uint8_t code;
} VRDP_USB_REQ_DEVICE_LIST_PARM;

/* Data is a list of the following variable length structures. */
typedef struct _VRDPUSBDEVICEDESC
{
    /* Offset of the next structure. 0 if last. */
    uint16_t oNext;

    /* Identifier of the device assigned by client. */
    VRDPUSBDEVID id;

    /** USB version number. */
    uint16_t        bcdUSB;
    /** Device class. */
    uint8_t         bDeviceClass;
    /** Device subclass. */
    uint8_t         bDeviceSubClass;
    /** Device protocol */
    uint8_t         bDeviceProtocol;
    /** Vendor ID. */
    uint16_t        idVendor;
    /** Product ID. */
    uint16_t        idProduct;
    /** Revision, integer part. */
    uint16_t        bcdRev;
    /** Manufacturer string. */
    uint16_t        oManufacturer;
    /** Product string. */
    uint16_t        oProduct;
    /** Serial number string. */
    uint16_t        oSerialNumber;
    /** Physical USB port the device is connected to. */
    uint16_t        idPort;

} VRDPUSBDEVICEDESC;

typedef struct _VRDP_USB_REQ_DEVICE_LIST_RET
{
    VRDPUSBDEVICEDESC body;
    /* Other devices may follow.
     * The list ends with (uint16_t)0,
     * which means that an empty list consists of 2 zero bytes.
     */
} VRDP_USB_REQ_DEVICE_LIST_RET;

typedef struct _VRDPUSBREQNEGOTIATEPARM
{
    uint8_t code;

    /* Remote USB Protocol version. */
    uint32_t version;

} VRDPUSBREQNEGOTIATEPARM;

#define VRDP_USB_CAPS_FLAG_ASYNC    (0x0)
#define VRDP_USB_CAPS_FLAG_POLL     (0x1)

#define VRDP_USB_CAPS_VALID_FLAGS   (VRDP_USB_CAPS_FLAG_POLL)

typedef struct _VRDPUSBREQNEGOTIATERET
{
    uint8_t flags;
} VRDPUSBREQNEGOTIATERET;
#pragma pack()

#define VRDP_CLIPBOARD_FORMAT_NULL         (0x0)
#define VRDP_CLIPBOARD_FORMAT_UNICODE_TEXT (0x1)
#define VRDP_CLIPBOARD_FORMAT_BITMAP       (0x2)
#define VRDP_CLIPBOARD_FORMAT_HTML         (0x4)

#define VRDP_CLIPBOARD_FUNCTION_FORMAT_ANNOUNCE (0)
#define VRDP_CLIPBOARD_FUNCTION_DATA_READ       (1)
#define VRDP_CLIPBOARD_FUNCTION_DATA_WRITE      (2)


/** Indexes of information values. */

/** Whether a client is connected at the moment.
 * uint32_t
 */
#define VRDP_QI_ACTIVE                 (0)

/** How many times a client connected up to current moment.
 * uint32_t
 */
#define VRDP_QI_NUMBER_OF_CLIENTS      (1)

/** When last connection was established.
 * int64_t time in milliseconds since 1970-01-01 00:00:00 UTC
 */
#define VRDP_QI_BEGIN_TIME             (2)

/** When last connection was terminated or current time if connection still active.
 * int64_t time in milliseconds since 1970-01-01 00:00:00 UTC
 */
#define VRDP_QI_END_TIME               (3)

/** How many bytes were sent in last (current) connection.
 * uint64_t
 */
#define VRDP_QI_BYTES_SENT             (4)

/** How many bytes were sent in all connections.
 * uint64_t
 */
#define VRDP_QI_BYTES_SENT_TOTAL       (5)

/** How many bytes were received in last (current) connection.
 * uint64_t
 */
#define VRDP_QI_BYTES_RECEIVED         (6)

/** How many bytes were received in all connections.
 * uint64_t
 */
#define VRDP_QI_BYTES_RECEIVED_TOTAL   (7)

/** Login user name supplied by the client.
 * UTF8 nul terminated string.
 */
#define VRDP_QI_USER                   (8)

/** Login domain supplied by the client.
 * UTF8 nul terminated string.
 */
#define VRDP_QI_DOMAIN                 (9)

/** The client name supplied by the client.
 * UTF8 nul terminated string.
 */
#define VRDP_QI_CLIENT_NAME            (10)

/** IP address of the client.
 * UTF8 nul terminated string.
 */
#define VRDP_QI_CLIENT_IP              (11)

/** The client software version number.
 * uint32_t.
 */
#define VRDP_QI_CLIENT_VERSION         (12)

/** Public key exchange method used when connection was established.
 *  Values: 0 - RDP4 public key exchange scheme.
 *          1 - X509 sertificates were sent to client.
 * uint32_t.
 */
#define VRDP_QI_ENCRYPTION_STYLE       (13)


/** Hints what has been intercepted by the application. */
#define VRDP_CLIENT_INTERCEPT_AUDIO     (0x1)
#define VRDP_CLIENT_INTERCEPT_USB       (0x2)
#define VRDP_CLIENT_INTERCEPT_CLIPBOARD (0x4)


/** The version of the VRDP server interface. */
#define VRDP_INTERFACE_VERSION_1 (1)

/** The header that does not change when the interface changes. */
typedef struct _VRDPINTERFACEHDR
{
    /** The version of the interface. */
    uint64_t u64Version;

    /** The size of the structure. */
    uint64_t u64Size;

} VRDPINTERFACEHDR;

/** The VRDP server entry points. Interface version 1. */
typedef struct _VRDPENTRYPOINTS_1
{
    /** The header. */
    VRDPINTERFACEHDR header;

    /** Destroy the server instance.
     *
     * @param hServer The server instance handle.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(void, VRDPDestroy,(HVRDPSERVER hServer));

    /** The server should start to accept clients connections.
     *
     * @param hServer The server instance handle.
     * @param fEnable Whether to enable or disable client connections.
     *                When is false, all existing clients are disconnected.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDPEnableConnections,(HVRDPSERVER hServer,
                                                   bool fEnable));

    /** The server should disconnect the client.
     *
     * @param hServer     The server instance handle.
     * @param u32ClientId The client identifier.
     * @param fReconnect  Whether to send a "REDIRECT to the same server" packet to the 
     *                    client before disconnecting.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(void, VRDPDisconnect,(HVRDPSERVER hServer,
                                             uint32_t u32ClientId,
                                             bool fReconnect));

    /**
     * Inform the server that the display was resized.
     * The server will query information about display
     * from the application via callbacks.
     *
     * @param hServer Handle of VRDP server instance.
     */
    DECLR3CALLBACKMEMBER(void, VRDPResize,(HVRDPSERVER hServer));

    /**
     * Send a update.
     *
     * @param hServer   Handle of VRDP server instance.
     * @param uScreenId The screen index.
     * @param pvUpdate  Pointer to VBoxGuest.h::VRDPORDERHDR structure with extra data.
     * @param cbUpdate  Size of the update data.
     */
    DECLR3CALLBACKMEMBER(void, VRDPUpdate,(HVRDPSERVER hServer,
                                         unsigned uScreenId,
                                         void *pvUpdate,
                                         uint32_t cbUpdate));

    /**
     * Set the mouse pointer shape.
     *
     * @param hServer  Handle of VRDP server instance.
     * @param pPointer The pointer shape information.
     */
    DECLR3CALLBACKMEMBER(void, VRDPColorPointer,(HVRDPSERVER hServer,
                                               const VRDPCOLORPOINTER *pPointer));

    /**
     * Hide the mouse pointer.
     *
     * @param hServer Handle of VRDP server instance.
     */
    DECLR3CALLBACKMEMBER(void, VRDPHidePointer,(HVRDPSERVER hServer));

    /**
     * Queues the samples to be sent to clients.
     *
     * @param hServer    Handle of VRDP server instance.
     * @param pvSamples  Address of samples to be sent.
     * @param cSamples   Number of samples.
     * @param format     Encoded audio format for these samples.
     *
     * @note Initialized to NULL when the application audio callbacks are NULL.
     */
    DECLR3CALLBACKMEMBER(void, VRDPAudioSamples,(HVRDPSERVER hServer,
                                               const void *pvSamples,
                                               uint32_t cSamples,
                                               VRDPAUDIOFORMAT format));

    /**
     * Sets the sound volume on clients.
     *
     * @param hServer    Handle of VRDP server instance.
     * @param left       0..0xFFFF volume level for left channel.
     * @param right      0..0xFFFF volume level for right channel.
     *
     * @note Initialized to NULL when the application audio callbacks are NULL.
     */
    DECLR3CALLBACKMEMBER(void, VRDPAudioVolume,(HVRDPSERVER hServer,
                                              uint16_t u16Left,
                                              uint16_t u16Right));

    /**
     * Sends a USB request.
     *
     * @param hServer      Handle of VRDP server instance.
     * @param u32ClientId  An identifier that allows the server to find the corresponding client.
     *                     The identifier is always passed by the server as a parameter
     *                     of the FNVRDPUSBCALLBACK. Note that the value is the same as
     *                     in the VRDPSERVERCALLBACK functions.
     * @param pvParm       Function specific parameters buffer.
     * @param cbParm       Size of the buffer.
     *
     * @note Initialized to NULL when the application USB callbacks are NULL.
     */
    DECLR3CALLBACKMEMBER(void, VRDPUSBRequest,(HVRDPSERVER hServer,
                                             uint32_t u32ClientId,
                                             void *pvParm,
                                             uint32_t cbParm));

    /**
     * Called by the application when (VRDP_CLIPBOARD_FUNCTION_*):
     *   - (0) guest announces available clipboard formats;
     *   - (1) guest requests clipboard data;
     *   - (2) guest responds to the client's request for clipboard data.
     *
     * @param hServer     The VRDP server handle.
     * @param u32Function The cause of the call.
     * @param u32Format   Bitmask of announced formats or the format of data.
     * @param pvData      Points to: (1) buffer to be filled with clients data;
     *                               (2) data from the host.
     * @param cbData      Size of 'pvData' buffer in bytes.
     * @param pcbActualRead Size of the copied data in bytes.
     *
     * @note Initialized to NULL when the application clipboard callbacks are NULL.
     */
    DECLR3CALLBACKMEMBER(void, VRDPClipboard,(HVRDPSERVER hServer,
                                            uint32_t u32Function,
                                            uint32_t u32Format,
                                            void *pvData,
                                            uint32_t cbData,
                                            uint32_t *pcbActualRead));

    /**
     * Query various information from the VRDP server.
     *
     * @param hServer   The VRDP server handle.
     * @param index     VRDP_QI_* identifier of information to be returned.
     * @param pvBuffer  Address of memory buffer to which the information must be written.
     * @param cbBuffer  Size of the memory buffer in bytes.
     * @param pcbOut    Size in bytes of returned information value.
     *
     * @remark The caller must check the *pcbOut. 0 there means no information was returned.
     *         A value greater than cbBuffer means that information is too big to fit in the
     *         buffer, in that case no information was placed to the buffer.
     */
    DECLR3CALLBACKMEMBER(void, VRDPQueryInfo,(HVRDPSERVER hServer,
                                            uint32_t index,
                                            void *pvBuffer,
                                            uint32_t cbBuffer,
                                            uint32_t *pcbOut));
} VRDPENTRYPOINTS_1;

#define VRDP_QP_NETWORK_PORT      (1)
#define VRDP_QP_NETWORK_ADDRESS   (2)
#define VRDP_QP_NUMBER_MONITORS   (3)

#pragma pack(1)
/* A framebuffer description. */
typedef struct _VRDPFRAMEBUFFERINFO
{
    const uint8_t *pu8Bits;
    int            xOrigin;
    int            yOrigin;
    unsigned       cWidth;
    unsigned       cHeight;
    unsigned       cBitsPerPixel;
    unsigned       cbLine;
} VRDPFRAMEBUFFERINFO;

#define VRDP_INPUT_SCANCODE 0
#define VRDP_INPUT_POINT    1
#define VRDP_INPUT_CAD      2
#define VRDP_INPUT_RESET    3
#define VRDP_INPUT_SYNCH    4

typedef struct _VRDPINPUTSCANCODE
{
    unsigned uScancode;
} VRDPINPUTSCANCODE;

#define VRDP_INPUT_POINT_BUTTON1    0x01
#define VRDP_INPUT_POINT_BUTTON2    0x02
#define VRDP_INPUT_POINT_BUTTON3    0x04
#define VRDP_INPUT_POINT_WHEEL_UP   0x08
#define VRDP_INPUT_POINT_WHEEL_DOWN 0x10

typedef struct _VRDPINPUTPOINT
{
    int x;
    int y;
    unsigned uButtons;
} VRDPINPUTPOINT;

#define VRDP_INPUT_SYNCH_SCROLL   0x01
#define VRDP_INPUT_SYNCH_NUMLOCK  0x02
#define VRDP_INPUT_SYNCH_CAPITAL  0x04

typedef struct _VRDPINPUTSYNCH
{
    unsigned uLockStatus;
} VRDPINPUTSYNCH;
#pragma pack()

/** The VRDP server callbacks. Interface version 1. */
typedef struct _VRDPCALLBACKS_1
{
    /** The header. */
    VRDPINTERFACEHDR header;

    /**
     * Query various information, on how the VRDP server must operate, from the application.
     *
     * @param pvCallback  The callback specific pointer.
     * @param index       VRDP_QP_* identifier of information to be returned.
     * @param pvBuffer    Address of memory buffer to which the information must be written.
     * @param cbBuffer    Size of the memory buffer in bytes.
     * @param pcbOut      Size in bytes of returned information value.
     *
     * @return IPRT status code. VINF_BUFFER_OVERFLOW if the buffer is too small for the value.
     */
    DECLR3CALLBACKMEMBER(int, VRDPCallbackQueryProperty,(void *pvCallback,
                                                       uint32_t index,
                                                       void *pvBuffer,
                                                       uint32_t cbBuffer,
                                                       uint32_t *pcbOut));

    /* A client is logging in, the application must decide whether
     * to let to connect the client. The server will drop the connection, 
     * when an error code is returned by the callback.
     *
     * @param pvCallback   The callback specific pointer.
     * @param u32ClientId  An unique client identifier generated by the server.
     * @param pszUser      The username.
     * @param pszPassword  The password.
     * @param pszDomain    The domain.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDPCallbackClientLogon,(void *pvCallback,
                                                     uint32_t u32ClientId,
                                                     const char *pszUser,
                                                     const char *pszPassword,
                                                     const char *pszDomain));

    /* The client has been successfully connected.
     *
     * @param pvCallback      The callback specific pointer.
     * @param u32ClientId     An unique client identifier generated by the server.
     */
    DECLR3CALLBACKMEMBER(void, VRDPCallbackClientConnect,(void *pvCallback,
                                                        uint32_t u32ClientId));

    /* The client has been disconnected.
     *
     * @param pvCallback      The callback specific pointer.
     * @param u32ClientId     An unique client identifier generated by the server.
     * @param fu32Intercepted What was intercepted by the client (VRDP_CLIENT_INTERCEPT_*).
     */
    DECLR3CALLBACKMEMBER(void, VRDPCallbackClientDisconnect,(void *pvCallback,
                                                           uint32_t u32ClientId,
                                                           uint32_t fu32Intercepted));
    /* The client supports one of RDP channels.
     *
     * @param pvCallback      The callback specific pointer.
     * @param u32ClientId     An unique client identifier generated by the server.
     * @param fu32Intercept   What the client wants to intercept. One of VRDP_CLIENT_INTERCEPT_* flags.
     * @param ppvIntercept    The value to be passed to the channel specific callback.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDPCallbackIntercept,(void *pvCallback,
                                                   uint32_t u32ClientId,
                                                   uint32_t fu32Intercept,
                                                   void **ppvIntercept));

    /**
     * Called by the server when a reply is received from a client.
     *
     * @param pvCallback   The callback specific pointer.
     * @param ppvIntercept The value returned by VRDPCallbackIntercept for the VRDP_CLIENT_INTERCEPT_USB.
     * @param u32ClientId  Identifies the client that sent the reply.
     * @param u8Code       The operation code VRDP_USB_REQ_*.
     * @param pvRet        Points to data received from the client.
     * @param cbRet        Size of the data in bytes.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDPCallbackUSB,(void *pvCallback,
                                             void *pvIntercept,
                                             uint32_t u32ClientId,
                                             uint8_t u8Code,
                                             const void *pvRet,
                                             uint32_t cbRet));

    /**
     * Called by the server when (VRDP_CLIPBOARD_FUNCTION_*):
     *   - (0) client announces available clipboard formats;
     *   - (1) client requests clipboard data.
     *
     * @param pvCallback   The callback specific pointer.
     * @param ppvIntercept The value returned by VRDPCallbackIntercept for the VRDP_CLIENT_INTERCEPT_CLIPBOARD.
     * @param u32ClientId Identifies the RDP client that sent the reply.
     * @param u32Function The cause of the callback.
     * @param u32Format   Bitmask of reported formats or the format of received data.
     * @param pvData      Reserved.
     * @param cbData      Reserved.
     *
     * @return IPRT status code.
     */
    DECLR3CALLBACKMEMBER(int, VRDPCallbackClipboard,(void *pvCallback,
                                                   void *pvIntercept,
                                                   uint32_t u32ClientId,
                                                   uint32_t u32Function,
                                                   uint32_t u32Format,
                                                   const void *pvData,
                                                   uint32_t cbData));

    /* The framebuffer information is queried.
     *
     * @param pvCallback      The callback specific pointer.
     * @param uScreenId       The framebuffer index.
     * @param pInfo           The information structure to ber filled.
     *
     * @return Whether the framebuffer is available.
     */
    DECLR3CALLBACKMEMBER(bool, VRDPCallbackFramebufferQuery,(void *pvCallback,
                                                           unsigned uScreenId,
                                                           VRDPFRAMEBUFFERINFO *pInfo));

    /* The framebuffer is locked.
     *
     * @param pvCallback      The callback specific pointer.
     * @param uScreenId       The framebuffer index.
     */
    DECLR3CALLBACKMEMBER(void, VRDPCallbackFramebufferLock,(void *pvCallback,
                                                          unsigned uScreenId));

    /* The framebuffer is unlocked.
     *
     * @param pvCallback      The callback specific pointer.
     * @param uScreenId       The framebuffer index.
     */
    DECLR3CALLBACKMEMBER(void, VRDPCallbackFramebufferUnlock,(void *pvCallback,
                                                            unsigned uScreenId));

    /* Input from the client.
     *
     * @param pvCallback      The callback specific pointer.
     * @param pvInput         The input information.
     * @param cbInput         The size of the input information.
     */
    DECLR3CALLBACKMEMBER(void, VRDPCallbackInput,(void *pvCallback,
                                                int type,
                                                const void *pvInput,
                                                unsigned cbInput));

    /* Video mode hint from the client.
     *
     * @param pvCallback      The callback specific pointer.
     * @param cWidth          Requested width.
     * @param cHeight         Requested height.
     * @param cBitsPerPixel   Requested color depth.
     * @param uScreenId       The framebuffer index.
     */
    DECLR3CALLBACKMEMBER(void, VRDPCallbackVideoModeHint,(void *pvCallback,
                                                        unsigned cWidth,
                                                        unsigned cHeight, 
                                                        unsigned cBitsPerPixel,
                                                        unsigned uScreenId));

} VRDPCALLBACKS_1;

/**
 * Create a new VRDP server instance. The instance is fully functional but refuses
 * client connections until the entry point VRDPEnableConnections is called by the application.
 *
 * The caller prepares the callbacks structure. The header.u64Version field
 * must be initialized with the version of the insterface to use.
 * The server will initialize the callbacks table to match the requested interface.
 *
 * @param pCallback     Pointer to the application callbacks which let the server to fetch
 *                      the configuration data and to access the desktop.
 * @param pvCallback    The callback specific pointer to be passed back to the application.
 * @param ppEntryPoints Where to store the pointer to the VRDP entry points structure.
 * @param phServer      Pointer to the created server instance handle.
 *
 * @return IPRT status code.
 */
VRDPDECL(int) VRDPCreateServer (const VRDPINTERFACEHDR *pCallbacks,
                                void *pvCallback,
                                VRDPINTERFACEHDR **ppEntryPoints,
                                HVRDPSERVER *phServer);

typedef DECLCALLBACK(int) FNVRDPCREATESERVER (const VRDPINTERFACEHDR *pCallbacks,
                                              void *pvCallback,
                                              VRDPINTERFACEHDR **ppEntryPoints,
                                              HVRDPSERVER *phServer);
typedef FNVRDPCREATESERVER *PFNVRDPCREATESERVER;

__END_DECLS

/** @} */

#endif

