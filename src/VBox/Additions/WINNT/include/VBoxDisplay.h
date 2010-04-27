/** @file
 *
 * VBoxDisplay - private windows additions display header
 *
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef __VBoxDisplay_h__
#define __VBoxDisplay_h__

#define VBOXESC_SETVISIBLEREGION            0xABCD9001
#define VBOXESC_ISVRDPACTIVE                0xABCD9002
#ifdef VBOXWDDM
# define VBOXESC_REINITVIDEOMODES           0xABCD9003

typedef struct
{
    ULONG Id;
    DWORD Width;
    DWORD Height;
    DWORD BitsPerPixel;
} VBOXWDDM_RECOMMENDVIDPN_SCREEN_INFO, *PVBOXWDDM_RECOMMENDVIDPN_SCREEN_INFO;

typedef struct
{
    uint32_t cScreenInfos;
    VBOXWDDM_RECOMMENDVIDPN_SCREEN_INFO aScreenInfos[1];
} VBOXWDDM_RECOMMENDVIDPN, *PVBOXWDDM_RECOMMENDVIDPN;

#define VBOXWDDM_RECOMMENDVIDPN_SIZE(_c) (RT_OFFSETOF(VBOXWDDM_RECOMMENDVIDPN, aScreenInfos[_c]))
#endif /* #ifdef VBOXWDDM */

typedef struct VBOXDISPIFESCAPE
{
    int escapeCode;
} VBOXDISPIFESCAPE, *PVBOXDISPIFESCAPE;

#define VBOXDISPIFESCAPE_DATA_OFFSET() ((sizeof (VBOXDISPIFESCAPE) + 7) & ~7)
#define VBOXDISPIFESCAPE_DATA(_pHead, _t) ( (_t*)(((uint8_t*)(_pHead)) + VBOXDISPIFESCAPE_DATA_OFFSET()))
#define VBOXDISPIFESCAPE_DATA_SIZE(_s) ( (_s) < VBOXDISPIFESCAPE_DATA_OFFSET() ? 0 : (_s) - VBOXDISPIFESCAPE_DATA_OFFSET() )
#define VBOXDISPIFESCAPE_SIZE(_cbData) ((_cbData) ? VBOXDISPIFESCAPE_DATA_OFFSET() + (_cbData) : sizeof (VBOXDISPIFESCAPE))

#define IOCTL_VIDEO_VBOX_SETVISIBLEREGION \
    CTL_CODE(FILE_DEVICE_VIDEO, 0xA01, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif /* __VBoxDisplay_h__ */
