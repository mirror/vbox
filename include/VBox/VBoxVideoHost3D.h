/** @file
 *
 * VirtualBox 3D host inter-components interfaces
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
 */
#ifndef ___VBox_VBoxVideoHost3D_h
#define ___VBox_VBoxVideoHost3D_h
#include <iprt/cdefs.h>
#include <VBox/VBoxVideo.h>

/* screen update instance */
typedef struct PDMIDISPLAYCONNECTOR *HVBOXCRCMDCLTSCR;
struct VBVACMDHDR;

typedef struct VBOXCMDVBVA_HDR *PVBOXCMDVBVA_HDR;

typedef DECLCALLBACKPTR(void, PFNVBOXCRCMD_CLTSCR_UPDATE_BEGIN)(HVBOXCRCMDCLTSCR hClt, unsigned u32Screen);
typedef DECLCALLBACKPTR(void, PFNVBOXCRCMD_CLTSCR_UPDATE_END)(HVBOXCRCMDCLTSCR hClt, unsigned uScreenId, int32_t x, int32_t y, uint32_t cx, uint32_t cy);
typedef DECLCALLBACKPTR(void, PFNVBOXCRCMD_CLTSCR_UPDATE_PROCESS)(HVBOXCRCMDCLTSCR hClt, unsigned u32Screen, struct VBVACMDHDR *pCmd, size_t cbCmd);

/*client callbacks to be used by the server
 * when working in the CrCmd mode */
typedef struct VBOXCRCMD_SVRENABLE_INFO
{
    HVBOXCRCMDCLTSCR hCltScr;
    PFNVBOXCRCMD_CLTSCR_UPDATE_BEGIN pfnCltScrUpdateBegin;
    PFNVBOXCRCMD_CLTSCR_UPDATE_PROCESS pfnCltScrUpdateProcess;
    PFNVBOXCRCMD_CLTSCR_UPDATE_END pfnCltScrUpdateEnd;
} VBOXCRCMD_SVRENABLE_INFO;

typedef void * HVBOXCRCMDSVR;

/* enables the CrCmd interface, thus the hgcm interface gets disabled.
 * all subsequent calls will be done in the thread Enable was done,
 * until the Disable is called */
typedef DECLCALLBACKPTR(int, PFNVBOXCRCMD_SVR_ENABLE)(HVBOXCRCMDSVR hSvr, VBOXCRCMD_SVRENABLE_INFO *pInfo);
/* Opposite to Enable (see above) */
typedef DECLCALLBACKPTR(int, PFNVBOXCRCMD_SVR_DISABLE)(HVBOXCRCMDSVR hSvr);
/* process command */
typedef DECLCALLBACKPTR(int, PFNVBOXCRCMD_SVR_CMD)(HVBOXCRCMDSVR hSvr, PVBOXCMDVBVA_HDR pCmd, uint32_t cbCmd);
/* process host control */
typedef DECLCALLBACKPTR(int, PFNVBOXCRCMD_SVR_HOSTCTL)(HVBOXCRCMDSVR hSvr, uint8_t* pCtl, uint32_t cbCmd);
/* process guest control */
typedef DECLCALLBACKPTR(int, PFNVBOXCRCMD_SVR_GUESTCTL)(HVBOXCRCMDSVR hSvr, uint8_t* pCtl, uint32_t cbCmd);
/* process SaveState */
typedef DECLCALLBACKPTR(int, PFNVBOXCRCMD_SVR_SAVESTATE)(HVBOXCRCMDSVR hSvr, PSSMHANDLE pSSM);
/* process LoadState */
typedef DECLCALLBACKPTR(int, PFNVBOXCRCMD_SVR_LOADSTATE)(HVBOXCRCMDSVR hSvr, PSSMHANDLE pSSM, uint32_t u32Version);


typedef struct VBOXCRCMD_SVRINFO
{
    HVBOXCRCMDSVR hSvr;
    PFNVBOXCRCMD_SVR_ENABLE pfnEnable;
    PFNVBOXCRCMD_SVR_DISABLE pfnDisable;
    PFNVBOXCRCMD_SVR_CMD pfnCmd;
    PFNVBOXCRCMD_SVR_HOSTCTL pfnHostCtl;
    PFNVBOXCRCMD_SVR_GUESTCTL pfnGuestCtl;
    PFNVBOXCRCMD_SVR_SAVESTATE pfnSaveState;
    PFNVBOXCRCMD_SVR_LOADSTATE pfnLoadState;
} VBOXCRCMD_SVRINFO;


typedef struct VBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP
{
    VBOXVDMACMD_CHROMIUM_CTL Hdr;
    union
    {
        void *pvVRamBase;
        uint64_t uAlignment;
    };
    uint64_t cbVRam;
    /* out */
    struct VBOXCRCMD_SVRINFO CrCmdServerInfo;
} VBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP, *PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP;

typedef enum
{
    VBOXCRCMDCTL_TYPE_HGCM = 1,
    VBOXCRCMDCTL_TYPE_DISABLE,
    VBOXCRCMDCTL_TYPE_ENABLE,
    VBOXCRCMDCTL_TYPE_32bit = 0x7fffffff
} VBOXCRCMDCTL_TYPE;

typedef struct VBOXCRCMDCTL
{
    VBOXCRCMDCTL_TYPE enmType;
    uint32_t u32Function;
    /* not to be used by clients */
    union
    {
        void(*pfnInternal)();
        void* pvInternal;
    };
} VBOXCRCMDCTL;

typedef struct VBOXVDMAHOST * HVBOXCRCMDCTL_REMAINING_HOST_COMMAND;

typedef DECLCALLBACKPTR(uint8_t*, PFNVBOXCRCMDCTL_REMAINING_HOST_COMMAND)(HVBOXCRCMDCTL_REMAINING_HOST_COMMAND hClient, uint32_t *pcbCtl, int prevCmdRc);

typedef struct VBOXCRCMDCTL_ENABLE
{
    VBOXCRCMDCTL Hdr;
    HVBOXCRCMDCTL_REMAINING_HOST_COMMAND hRHCmd;
    PFNVBOXCRCMDCTL_REMAINING_HOST_COMMAND pfnRHCmd;
} VBOXCRCMDCTL_ENABLE;

#endif /*#ifndef ___VBox_VBoxVideoHost3D_h*/
