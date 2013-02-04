/** @file
 *
 * VBoxDisplay - private windows additions display header
 *
 * Copyright (C) 2006-2012 Oracle Corporation
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

#include <iprt/types.h>
#include <iprt/assert.h>

#define VBOXESC_SETVISIBLEREGION            0xABCD9001
#define VBOXESC_ISVRDPACTIVE                0xABCD9002
#ifdef VBOX_WITH_WDDM
# define VBOXESC_REINITVIDEOMODES           0xABCD9003
# define VBOXESC_GETVBOXVIDEOCMCMD          0xABCD9004
# define VBOXESC_DBGPRINT                   0xABCD9005
# define VBOXESC_SCREENLAYOUT               0xABCD9006
# define VBOXESC_SWAPCHAININFO              0xABCD9007
# define VBOXESC_UHGSMI_ALLOCATE            0xABCD9008
# define VBOXESC_UHGSMI_DEALLOCATE          0xABCD9009
# define VBOXESC_UHGSMI_SUBMIT              0xABCD900A
# define VBOXESC_SHRC_ADDREF                0xABCD900B
# define VBOXESC_SHRC_RELEASE               0xABCD900C
# define VBOXESC_DBGDUMPBUF                 0xABCD900D
# define VBOXESC_CRHGSMICTLCON_CALL         0xABCD900E
# define VBOXESC_CRHGSMICTLCON_GETCLIENTID  0xABCD900F
# define VBOXESC_REINITVIDEOMODESBYMASK     0xABCD9010
# define VBOXESC_ADJUSTVIDEOMODES           0xABCD9011
#endif /* #ifdef VBOX_WITH_WDDM */

# define VBOXESC_ISANYX                     0xABCD9200

typedef struct VBOXDISPIFESCAPE
{
    int32_t escapeCode;
    uint32_t u32CmdSpecific;
} VBOXDISPIFESCAPE, *PVBOXDISPIFESCAPE;

/* ensure command body is always 8-byte-aligned*/
AssertCompile((sizeof (VBOXDISPIFESCAPE) & 7) == 0);

#define VBOXDISPIFESCAPE_DATA_OFFSET() ((sizeof (VBOXDISPIFESCAPE) + 7) & ~7)
#define VBOXDISPIFESCAPE_DATA(_pHead, _t) ( (_t*)(((uint8_t*)(_pHead)) + VBOXDISPIFESCAPE_DATA_OFFSET()))
#define VBOXDISPIFESCAPE_DATA_SIZE(_s) ( (_s) < VBOXDISPIFESCAPE_DATA_OFFSET() ? 0 : (_s) - VBOXDISPIFESCAPE_DATA_OFFSET() )
#define VBOXDISPIFESCAPE_SIZE(_cbData) ((_cbData) ? VBOXDISPIFESCAPE_DATA_OFFSET() + (_cbData) : sizeof (VBOXDISPIFESCAPE))

#define IOCTL_VIDEO_VBOX_SETVISIBLEREGION \
    CTL_CODE(FILE_DEVICE_VIDEO, 0xA01, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_VIDEO_VBOX_ISANYX \
    CTL_CODE(FILE_DEVICE_VIDEO, 0xA02, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct VBOXDISPIFESCAPE_ISANYX
{
    VBOXDISPIFESCAPE EscapeHdr;
    uint32_t u32IsAnyX;
} VBOXDISPIFESCAPE_ISANYX, *PVBOXDISPIFESCAPE_ISANYX;

#ifdef VBOX_WITH_WDDM

/* for VBOX_VIDEO_MAX_SCREENS definition */
#include <VBox/Hardware/VBoxVideoVBE.h>

typedef struct
{
    DWORD Id;
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

/* the mode was adjusted */
#define VBOXWDDM_ADJUSTVIDEOMODE_F_ADJUSTED       0x00000001
/* the mode is the currently active one */
#define VBOXWDDM_ADJUSTVIDEOMODE_F_CURRENT        0x00000002
/* the mode is unsupported */
#define VBOXWDDM_ADJUSTVIDEOMODE_F_UNSUPPORTED    0x00000004
/* invalid screen id */
#define VBOXWDDM_ADJUSTVIDEOMODE_F_INVALISCREENID 0x00000008

typedef struct VBOXWDDM_ADJUSTVIDEOMODE
{
    uint32_t fFlags;
    VBOXWDDM_RECOMMENDVIDPN_SCREEN_INFO Mode;
} VBOXWDDM_ADJUSTVIDEOMODE, *PVBOXWDDM_ADJUSTVIDEOMODE;

typedef struct VBOXDISPIFESCAPE_ADJUSTVIDEOMODES
{
    VBOXDISPIFESCAPE EscapeHdr;
    VBOXWDDM_ADJUSTVIDEOMODE aScreenInfos[1];
} VBOXDISPIFESCAPE_ADJUSTVIDEOMODES, *PVBOXDISPIFESCAPE_ADJUSTVIDEOMODES;

#define VBOXWDDM_REINITVIDEOMODESBYMASK_F_RECONNECT_DISPLAYS_ON_CHANGE 0x00000001

#define VBOXWDDM_SCREENMASK_SIZE ((VBOX_VIDEO_MAX_SCREENS + 7) >> 3)

typedef struct VBOXDISPIFESCAPE_REINITVIDEOMODESBYMASK
{
    VBOXDISPIFESCAPE EscapeHdr;
    uint8_t ScreenMask[VBOXWDDM_SCREENMASK_SIZE];
} VBOXDISPIFESCAPE_REINITVIDEOMODESBYMASK, *PVBOXDISPIFESCAPE_REINITVIDEOMODESBYMASK;

#endif /* VBOX_WITH_WDDM */

#endif /* __VBoxDisplay_h__ */
