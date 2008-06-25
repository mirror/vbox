/** @file
 *
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef __VRDPUSB__H
#define __VRDPUSB__H

#include <endian.h>
#include <byteswap.h>

#ifndef cpu_to_le16
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(_le16) (_le16)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define cpu_to_le16(_le16) bswap_16(_le16)
#else
#error Unsupported host byte order!
#endif
#endif /* cpu_to_le16 */

//#include <asm/byteorder.h>
//#define cpu_to_le16 __cpu_to_le16

#define uint8_t uint8
#define uint16_t uint16
#define uint32_t uint32
#define uint64_t long long
#define bool int
#define false 0
#define true 1

#define OPSTATIC
#ifdef RDPUSB_DEBUG
#define LogFlow(a) printf a
#define Log(a) printf a
#define Log2(a) printf a
#else
#define LogFlow(a)
#define Log(a)
#define Log2(a)
#endif

#define LogRel(a) printf a

/* Runtime wrappers. */
#define RTMemAlloc xmalloc
#define RTMemRealloc xrealloc
#define RTMemFree xfree

#define _1K 1024

#define RT_LE2H_U16(_le16) (cpu_to_le16 (_le16))

#define VINF_SUCCESS 0
#define VERR_NO_MEMORY     (-8)
#define VERR_NOT_SUPPORTED (-37)
#define VERR_ACCESS_DENIED (-38)
#define VERR_VUSB_USBFS_PERMISSION (-2005)

#define VBOX_SUCCESS(_rc) ((_rc) >= 0)

#define RTFILE int
#define RTCRITSECT void *

#define Assert(_expr) do {                            \
    if (!(_expr))                                     \
    {                                                 \
        Log(("Assertion failed: {%s}!!!\n", #_expr)); \
    }                                                 \
} while (0)

#define AssertMsgFailed(_msg) do {                    \
        Log(("Assertion failed msg:!!!\n"));          \
	Log(_msg);                                    \
} while (0)

#define AssertReturn(_expr, _retval) do {             \
    if (!(_expr))                                     \
    {                                                 \
        Log(("Assertion failed: {%s}, returning 0x%08X!!!\n", #_expr, _retval)); \
	return (_retval);                             \
    }                                                 \
} while (0)

#define AssertRC(_rc) Assert(VBOX_SUCCESS(_rc))

#define RT_FAILURE(_rc) (!VBOX_SUCCESS(_rc))

#define NOREF(_a) ((void)_a)

static inline int RTCritSectInit (RTCRITSECT *pCritSect)
{
    return VINF_SUCCESS;
}

static inline int RTCritSectDelete (RTCRITSECT *pCritSect)
{
    return VINF_SUCCESS;
}

static inline int RTCritSectEnter (RTCRITSECT *pCritSect)
{
    return VINF_SUCCESS;
}

static inline int RTCritSectLeave (RTCRITSECT *pCritSect)
{
    return VINF_SUCCESS;
}

static inline void *RTMemDupEx (const void *pvSrc, size_t cbSrc, size_t cbExtra)
{
    void *p = RTMemAlloc (cbSrc + cbExtra);

    if (p)
    {
         memcpy (p, pvSrc, cbSrc);
         memset ((char *)p + cbSrc, 0, cbExtra);
    }

    return p;
}

static inline void *RTMemAllocZ (size_t cb)
{
    void *p = RTMemAlloc (cb);

    if (p)
    {
         memset (p, 0, cb);
    }

    return p;
}

static inline int RTStrToUInt32Ex (const char *pszValue, char **ppszNext, unsigned uBase, uint32_t *pu32)
{
    *pu32 = strtoul (pszValue, ppszNext, uBase);
    return VINF_SUCCESS;
}

#define PRTSTREAM FILE *

static inline int RTStrmOpen (const char *pszFileName, const char *pszMode, PRTSTREAM *ppStream)
{
    *ppStream = fopen (pszFileName, pszMode);

    if (*ppStream)
    {
        return VINF_SUCCESS;
    }

    return VERR_NOT_SUPPORTED;
}

static inline int RTStrmClose (PRTSTREAM pStream)
{
    fclose (pStream);
    return VINF_SUCCESS;
}

static inline int RTStrmGetLine (PRTSTREAM pStream, char *pszString, size_t cchString)
{
    if (fgets (pszString, cchString, pStream))
    {
        return VINF_SUCCESS;
    }

    return VERR_NOT_SUPPORTED;
}

static inline char *RTStrStripL (const char *psz)
{
    while (isspace (*psz))
        psz++;
    return (char *)psz;
}

#define NIL_RTFILE -1

#define RTFILE_O_READWRITE 0x00000003
#define RTFILE_O_OPEN      0x00000000
#define RTFILE_O_DENY_NONE 0x00000000

static inline int RTFileOpen (RTFILE *pFile, const char *pszFileName, unsigned fOpen)
{
    Assert (fOpen == RTFILE_O_READWRITE);

    *pFile = open (pszFileName, O_RDWR, 00600);

    if (*pFile != -1)
    {
        return VINF_SUCCESS;
    }

    return VERR_ACCESS_DENIED;
}

static inline int RTFileClose (RTFILE file)
{
    close (file);
    return VINF_SUCCESS;
}

static inline uint64_t RTTimeMilliTS (void)
{
     struct timeval tv;
     gettimeofday (&tv, NULL);
     return (uint64_t)tv.tv_sec * (uint64_t)(1000)
            + (uint64_t)(tv.tv_usec / 1000);
}

#define VRDP_USB_STATUS_SUCCESS         0
#define VRDP_USB_STATUS_ACCESS_DENIED   1
#define VRDP_USB_STATUS_DEVICE_REMOVED  2

#define VRDP_USB_REAP_FLAG_CONTINUED (0)
#define VRDP_USB_REAP_FLAG_LAST      (1)

#define VRDP_USB_CAPS_FLAG_ASYNC    (0)
#define VRDP_USB_CAPS_FLAG_POLL     (1)

#pragma pack(1)

#include "vusb.h"

typedef struct VUSBDEV
{
        char* pszName;
	int request_detach;
} VUSBDEV, *PVUSBDEV;

typedef struct usb_proxy {
    /* Note: the backend code assumes that the dev member is the first in the structure. */
    VUSBDEV Dev;
    /* 'union' because backend accesses the file handle as priv.File. */
    union {
        void *pv;
	int File;
    } Backend;

    struct usb_proxy *next;
    struct usb_proxy *prev;

    uint32_t devid;

    PVUSBURB urbs;

    int iActiveCfg;
    int cIgnoreSetConfigs;
} *PUSBPROXYDEV;

typedef struct vusb_setup {
	uint8_t  bmRequestType;
	uint8_t  bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} VUSBSETUP, *PVUSBSETUP;
#pragma pack()


static inline void vusbDevUnplugged(PVUSBDEV dev)
{
	dev->request_detach = 1;
}

int dev2fd (PUSBPROXYDEV pProxyDev);


typedef struct USBPROXYBACK
{
    /** Name of the backend. */
    const char *pszName;

    int  (* pfnOpen)(PUSBPROXYDEV pProxyDev, const char *pszAddress, void *pvBackend);
    void (* pfnClose)(PUSBPROXYDEV pProxyDev);
    int  (* pfnReset)(PUSBPROXYDEV pProxyDev);
    int  (* pfnSetConfig)(PUSBPROXYDEV pProxyDev, int iCfg);
    int  (* pfnClaimInterface)(PUSBPROXYDEV pProxyDev, int iIf);
    int  (* pfnReleaseInterface)(PUSBPROXYDEV pProxyDev, int iIf);
    int  (* pfnSetInterface)(PUSBPROXYDEV pProxyDev, int iIf, int setting);
    bool (* pfnClearHaltedEndpoint)(PUSBPROXYDEV  pDev, unsigned int iEp);
    int  (* pfnUrbQueue)(PVUSBURB urb);
    void (* pfnUrbCancel)(PVUSBURB pUrb);
    PVUSBURB (* pfnUrbReap)(PUSBPROXYDEV pProxyDev, unsigned cMillies);
    uint32_t uDummy;
} USBPROXYBACK;

typedef USBPROXYBACK *PUSBPROXYBACK;

extern const USBPROXYBACK g_USBProxyDeviceHost;

static inline int op_usbproxy_back_open(struct usb_proxy *p, const char *pszAddress)
{
     return g_USBProxyDeviceHost.pfnOpen (p, pszAddress, NULL);
}

static inline void op_usbproxy_back_close(PUSBPROXYDEV pDev)
{
     return g_USBProxyDeviceHost.pfnClose (pDev);
}

static inline int op_usbproxy_back_reset(PUSBPROXYDEV pDev)
{
    return g_USBProxyDeviceHost.pfnReset (pDev);
}

static inline int op_usbproxy_back_set_config(PUSBPROXYDEV pDev, int cfg)
{
    return g_USBProxyDeviceHost.pfnSetConfig (pDev, cfg);
}

static inline int op_usbproxy_back_claim_interface(PUSBPROXYDEV pDev, int ifnum)
{
    return g_USBProxyDeviceHost.pfnClaimInterface (pDev, ifnum);
}

static inline int op_usbproxy_back_release_interface(PUSBPROXYDEV pDev, int ifnum)
{
    return g_USBProxyDeviceHost.pfnReleaseInterface (pDev, ifnum);
}

static inline int op_usbproxy_back_interface_setting(PUSBPROXYDEV pDev, int ifnum, int setting)
{
    return g_USBProxyDeviceHost.pfnSetInterface (pDev, ifnum, setting);
}

static inline int op_usbproxy_back_queue_urb(PVUSBURB pUrb)
{
    return g_USBProxyDeviceHost.pfnUrbQueue(pUrb);
}

static inline PVUSBURB op_usbproxy_back_reap_urb(PUSBPROXYDEV pDev, unsigned cMillies)
{
    return g_USBProxyDeviceHost.pfnUrbReap (pDev, cMillies);
}

static inline bool op_usbproxy_back_clear_halted_ep(PUSBPROXYDEV pDev, unsigned EndPoint)
{
    return g_USBProxyDeviceHost.pfnClearHaltedEndpoint (pDev, EndPoint);
}

static inline void op_usbproxy_back_cancel_urb(PVUSBURB pUrb)
{
    return g_USBProxyDeviceHost.pfnUrbCancel (pUrb);
}

#endif /* __VRDPUSB__H  */
