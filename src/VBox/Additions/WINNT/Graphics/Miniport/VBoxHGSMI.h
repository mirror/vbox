/** @file
 *
 * VirtualBox Video miniport driver for NT/2k/XP
 * HGSMI related functions.
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

#ifndef ___VBoxHGSMI_h_
#define ___VBoxHGSMI_h_
typedef void* HVBOXVIDEOHGSMI;

/* Complete host commands addressed to the display */
typedef DECLCALLBACK(void) FNVBOXVIDEOHGSMICOMPLETION(HVBOXVIDEOHGSMI hHGSMI, struct _VBVAHOSTCMD * pCmd);
typedef FNVBOXVIDEOHGSMICOMPLETION *PFNVBOXVIDEOHGSMICOMPLETION;

/* request the host commands addressed to the display */
typedef DECLCALLBACK(int) FNVBOXVIDEOHGSMICOMMANDS(HVBOXVIDEOHGSMI hHGSMI, uint8_t u8Channel, struct _VBVAHOSTCMD ** ppCmd);
typedef FNVBOXVIDEOHGSMICOMMANDS *PFNVBOXVIDEOHGSMICOMMANDS;

/* post guest command (offset) to the host */
typedef DECLCALLBACK(void) FNVBOXVIDEOHGSMIPOSTCOMMAND(HVBOXVIDEOHGSMI hHGSMI, HGSMIOFFSET offCmd);
typedef FNVBOXVIDEOHGSMIPOSTCOMMAND *PFNVBOXVIDEOHGSMIPOSTCOMMAND;

/* Video Port API dynamically picked up at runtime for binary backwards compatibility with older NT versions */
#if 0
typedef VP_STATUS (*PFNWAITFORSINGLEOBJECT) (IN PVOID  HwDeviceExtension, IN PVOID  Object, IN PLARGE_INTEGER  Timeout  OPTIONAL);

typedef LONG (*PFNSETEVENT) (IN PVOID  HwDeviceExtension, IN PEVENT  pEvent);
typedef VOID (*PFNCLEAREVENT) (IN PVOID  HwDeviceExtension, IN PEVENT  pEvent);
typedef VP_STATUS (*PFNCREATEEVENT) (IN PVOID  HwDeviceExtension, IN ULONG  EventFlag, IN PVOID  Unused, OUT PEVENT  *ppEvent);
typedef VP_STATUS (*PFNDELETEEVENT) (IN PVOID  HwDeviceExtension, IN PEVENT  pEvent);

typedef VP_STATUS (*PFNCREATESPINLOCK) (IN PVOID  HwDeviceExtension, OUT PSPIN_LOCK  *SpinLock);
typedef VP_STATUS (*PFNDELETESPINLOCK) (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock);

typedef VOID (*PFNACQUIRESPINLOCK) (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock, OUT PUCHAR  OldIrql);
typedef VOID (*PFNRELEASESPINLOCK) (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock, IN UCHAR  NewIrql);
typedef VOID (*PFNACQUIRESPINLOCKATDPCLEVEL) (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock);
typedef VOID (*PFNRELEASESPINLOCKFROMDPCLEVEL) (IN PVOID  HwDeviceExtension, IN PSPIN_LOCK  SpinLock);
#endif

typedef long VBOXVP_STATUS;
typedef struct _VIDEO_PORT_EVENT *VBOXPEVENT;
typedef struct _VIDEO_PORT_SPIN_LOCK *VBOXPSPIN_LOCK;
typedef union _LARGE_INTEGER *VBOXPLARGE_INTEGER;

#define VBOXNOTIFICATION_EVENT 0x00000001UL

#define VBOXNO_ERROR           0x00000000UL

typedef VBOXVP_STATUS (*PFNWAITFORSINGLEOBJECT) (void*  HwDeviceExtension, void*  Object, VBOXPLARGE_INTEGER  Timeout);

typedef long (*PFNSETEVENT) (void* HwDeviceExtension, VBOXPEVENT  pEvent);
typedef void (*PFNCLEAREVENT) (void*  HwDeviceExtension, VBOXPEVENT  pEvent);
typedef VBOXVP_STATUS (*PFNCREATEEVENT) (void*  HwDeviceExtension, unsigned long  EventFlag, void*  Unused, VBOXPEVENT  *ppEvent);
typedef VBOXVP_STATUS (*PFNDELETEEVENT) (void*  HwDeviceExtension, VBOXPEVENT  pEvent);

typedef VBOXVP_STATUS (*PFNCREATESPINLOCK) (void*  HwDeviceExtension, VBOXPSPIN_LOCK  *SpinLock);
typedef VBOXVP_STATUS (*PFNDELETESPINLOCK) (void*  HwDeviceExtension, VBOXPSPIN_LOCK  SpinLock);

typedef void (*PFNACQUIRESPINLOCK) (void*  HwDeviceExtension, VBOXPSPIN_LOCK  SpinLock, unsigned char * OldIrql);
typedef void (*PFNRELEASESPINLOCK) (void*  HwDeviceExtension, VBOXPSPIN_LOCK  SpinLock, unsigned char  NewIrql);
typedef void (*PFNACQUIRESPINLOCKATDPCLEVEL) (void*  HwDeviceExtension, VBOXPSPIN_LOCK  SpinLock);
typedef void (*PFNRELEASESPINLOCKFROMDPCLEVEL) (void*  HwDeviceExtension, VBOXPSPIN_LOCK  SpinLock);


/* pfn*SpinLock* functions are available */
#define VBOXVIDEOPORTPROCS_SPINLOCK 0x00000001
/* pfn*Event and pfnWaitForSingleObject functions are available */
#define VBOXVIDEOPORTPROCS_EVENT    0x00000002

typedef struct VBOXVIDEOPORTPROCS
{
    /* ored VBOXVIDEOPORTPROCS_xxx constants describing the supported functyionality */
    uint32_t fSupportedTypes;

    PFNWAITFORSINGLEOBJECT pfnWaitForSingleObject;

    PFNSETEVENT pfnSetEvent;
    PFNCLEAREVENT pfnClearEvent;
    PFNCREATEEVENT pfnCreateEvent;
    PFNDELETEEVENT pfnDeleteEvent;

    PFNCREATESPINLOCK pfnCreateSpinLock;
    PFNDELETESPINLOCK pfnDeleteSpinLock;
    PFNACQUIRESPINLOCK pfnAcquireSpinLock;
    PFNRELEASESPINLOCK pfnReleaseSpinLock;
    PFNACQUIRESPINLOCKATDPCLEVEL pfnAcquireSpinLockAtDpcLevel;
    PFNRELEASESPINLOCKFROMDPCLEVEL pfnReleaseSpinLockFromDpcLevel;
} VBOXVIDEOPORTPROCS;

#endif /* #ifndef ___VBoxHGSMI_h_ */
