/* $Id$ */
/** @file
 * VBoxVRDP - VBox VRDP connection notification
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* 0x0501 for SPI_SETDROPSHADOW */
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxVRDP.h"

#include <VBox/VMMDev.h>
#ifdef DEBUG
# define LOG_ENABLED
# define LOG_GROUP LOG_GROUP_DEFAULT
#endif
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/ldr.h>



/* The guest receives VRDP_ACTIVE/VRDP_INACTIVE notifications.
 *
 * When VRDP_ACTIVE is received, the guest asks host about the experience level.
 * The experience level is an integer value, different values disable some GUI effects.
 *
 * On VRDP_INACTIVE the original values are restored.
 *
 * Note: that this is not controlled from the client, that is a per VM settings.
 *
 * Note: theming is disabled separately by EnableTheming.
 */

#define VBOX_SPI_STRING   0
#define VBOX_SPI_BOOL_PTR 1
#define VBOX_SPI_BOOL     2
#define VBOX_SPI_PTR      3

static ANIMATIONINFO animationInfoDisable =
{
    sizeof (ANIMATIONINFO),
    FALSE
};

typedef struct _VBOXVRDPEXPPARAM
{
    const char *name;
    UINT  uActionSet;
    UINT  uActionGet;
    uint32_t level;                    /* The parameter remain enabled at this or higher level. */
    int   type;
    void  *pvDisable;
    UINT  cbSavedValue;
    char  achSavedValue[2 * MAX_PATH]; /* Large enough to save the bitmap path. */
} VBOXVRDPEXPPARAM, *PVBOXVRDPEXPPARAM;

typedef struct _VBOXVRDPCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    uint32_t level;
    BOOL fSavedThemeEnabled;

    RTLDRMOD hModUxTheme;

    HRESULT (* pfnEnableTheming)(BOOL fEnable);
    BOOL (* pfnIsThemeActive)(VOID);
} VBOXVRDPCONTEXT, *PVBOXVRDPCONTEXT;

static VBOXVRDPCONTEXT g_Ctx = { 0 };

#define SPI_(l, a) #a, SPI_SET##a, SPI_GET##a, VRDP_EXPERIENCE_LEVEL_##l

static VBOXVRDPEXPPARAM s_aSPIParams[] =
{
    { SPI_(MEDIUM, DESKWALLPAPER),           VBOX_SPI_STRING,   "" },
    { SPI_(FULL,   DROPSHADOW),              VBOX_SPI_BOOL_PTR,       },
    { SPI_(HIGH,   FONTSMOOTHING),           VBOX_SPI_BOOL,           },
    { SPI_(FULL,   MENUFADE),                VBOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   COMBOBOXANIMATION),       VBOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   CURSORSHADOW),            VBOX_SPI_BOOL_PTR,       },
    { SPI_(HIGH,   GRADIENTCAPTIONS),        VBOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   LISTBOXSMOOTHSCROLLING),  VBOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   MENUANIMATION),           VBOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   SELECTIONFADE),           VBOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   TOOLTIPANIMATION),        VBOX_SPI_BOOL_PTR,       },
    { SPI_(FULL,   ANIMATION),               VBOX_SPI_PTR,      &animationInfoDisable, sizeof (ANIMATIONINFO) },
    { SPI_(MEDIUM, DRAGFULLWINDOWS),         VBOX_SPI_BOOL,           }
};

#undef SPI_

static void vboxExperienceSet(uint32_t uLevel)
{
    for (size_t i = 0; i < RT_ELEMENTS(s_aSPIParams); i++)
    {
        PVBOXVRDPEXPPARAM pParam = &s_aSPIParams[i];
        if (pParam->level > uLevel)
        {
            /*
             * The parameter has to be disabled.
             */
            LogFlowFunc(("Saving %s\n", pParam->name));

            /* Save the current value. */
            switch (pParam->type)
            {
                case VBOX_SPI_STRING:
                {
                    /* The 2nd parameter is size in characters of the buffer.
                     * The 3rd parameter points to the buffer.
                     */
                    SystemParametersInfo (pParam->uActionGet,
                                          MAX_PATH,
                                          pParam->achSavedValue,
                                          0);
                } break;

                case VBOX_SPI_BOOL:
                case VBOX_SPI_BOOL_PTR:
                {
                    /* The 3rd parameter points to BOOL. */
                    SystemParametersInfo (pParam->uActionGet,
                                          0,
                                          pParam->achSavedValue,
                                          0);
                } break;

                case VBOX_SPI_PTR:
                {
                    /* The 3rd parameter points to the structure.
                     * The cbSize member of this structure must be set.
                     * The uiParam parameter must alos be set.
                     */
                    if (pParam->cbSavedValue > sizeof (pParam->achSavedValue))
                    {
                        LogFlowFunc(("Not enough space %d > %d\n", pParam->cbSavedValue, sizeof (pParam->achSavedValue)));
                        break;
                    }

                    *(UINT *)&pParam->achSavedValue[0] = pParam->cbSavedValue;

                    SystemParametersInfo (s_aSPIParams[i].uActionGet,
                                          s_aSPIParams[i].cbSavedValue,
                                          s_aSPIParams[i].achSavedValue,
                                          0);
                } break;

                default:
                    break;
            }

            LogFlowFunc(("Disabling %s\n", pParam->name));

            /* Disable the feature. */
            switch (pParam->type)
            {
                case VBOX_SPI_STRING:
                {
                    /* The 3rd parameter points to the string. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          pParam->pvDisable,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_BOOL:
                {
                    /* The 2nd parameter is BOOL. */
                    SystemParametersInfo (pParam->uActionSet,
                                          FALSE,
                                          NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_BOOL_PTR:
                {
                    /* The 3rd parameter is NULL to disable. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_PTR:
                {
                    /* The 3rd parameter points to the structure. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          pParam->pvDisable,
                                          SPIF_SENDCHANGE);
                } break;

                default:
                    break;
            }
        }
    }
}

static void vboxExperienceRestore(uint32_t uLevel)
{
    int i;
    for (i = 0; i < RT_ELEMENTS(s_aSPIParams); i++)
    {
        PVBOXVRDPEXPPARAM pParam = &s_aSPIParams[i];
        if (pParam->level > uLevel)
        {
            LogFlowFunc(("Restoring %s\n", pParam->name));

            /* Restore the feature. */
            switch (pParam->type)
            {
                case VBOX_SPI_STRING:
                {
                    /* The 3rd parameter points to the string. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          pParam->achSavedValue,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_BOOL:
                {
                    /* The 2nd parameter is BOOL. */
                    SystemParametersInfo (pParam->uActionSet,
                                          *(BOOL *)&pParam->achSavedValue[0],
                                          NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_BOOL_PTR:
                {
                    /* The 3rd parameter is NULL to disable. */
                    BOOL fSaved = *(BOOL *)&pParam->achSavedValue[0];

                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          fSaved? &fSaved: NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_PTR:
                {
                    /* The 3rd parameter points to the structure. */
                    SystemParametersInfo (pParam->uActionSet,
                                          0,
                                          pParam->achSavedValue,
                                          SPIF_SENDCHANGE);
                } break;

                default:
                    break;
            }
        }
    }
}

static DECLCALLBACK(int) VBoxVRDPInit(const PVBOXSERVICEENV pEnv, void **ppInstance)
{
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);
    AssertPtrReturn(ppInstance, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    PVBOXVRDPCONTEXT pCtx = &g_Ctx; /* Only one instance at the moment. */
    AssertPtr(pCtx);

    pCtx->pEnv               = pEnv;
    pCtx->level              = VRDP_EXPERIENCE_LEVEL_FULL;
    pCtx->fSavedThemeEnabled = FALSE;

    int rc = RTLdrLoadSystem("UxTheme.dll", false /*fNoUnload*/, &g_Ctx.hModUxTheme);
    if (RT_SUCCESS(rc))
    {
        *(PFNRT *)&pCtx->pfnEnableTheming = RTLdrGetFunction(g_Ctx.hModUxTheme, "EnableTheming");
        *(PFNRT *)&pCtx->pfnIsThemeActive = RTLdrGetFunction(g_Ctx.hModUxTheme, "IsThemeActive");

        *ppInstance = &g_Ctx;
    }
    else
    {
        g_Ctx.hModUxTheme = NIL_RTLDRMOD;
        g_Ctx.pfnEnableTheming = NULL;
        g_Ctx.pfnIsThemeActive = NULL;
    }

    /* Tell the caller that the service does not work but it is OK to continue. */
    if (RT_FAILURE(rc))
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(void) VBoxVRDPDestroy(void *pInstance)
{
    AssertPtrReturnVoid(pInstance);

    LogFlowFuncEnter();

    PVBOXVRDPCONTEXT pCtx = (PVBOXVRDPCONTEXT)pInstance;

    vboxExperienceRestore (pCtx->level);
    if (pCtx->hModUxTheme != NIL_RTLDRMOD)
    {
        RTLdrClose(g_Ctx.hModUxTheme);
        pCtx->hModUxTheme = NIL_RTLDRMOD;
    }

    return;
}

/**
 * Thread function to wait for and process mode change requests
 */
static DECLCALLBACK(int) VBoxVRDPWorker(void *pInstance, bool volatile *pfShutdown)
{
    AssertPtrReturn(pInstance, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    PVBOXVRDPCONTEXT pCtx = (PVBOXVRDPCONTEXT)pInstance;
    AssertPtr(pCtx);

    HANDLE gVBoxDriver = pCtx->pEnv->hDriver;
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;

    maskInfo.u32OrMask = VMMDEV_EVENT_VRDP;
    maskInfo.u32NotMask = 0;
    if (!DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        DWORD dwErr = GetLastError();
        LogFlowFunc(("DeviceIOControl(CtlMask) failed with %ld, exiting\n", dwErr));
        return RTErrConvertFromWin32(dwErr);
    }

    int rc = VINF_SUCCESS;

    for (;;)
    {
        /* wait for the event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn   = 5000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_VRDP;
        if (DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            /* are we supposed to stop? */
            if (*pfShutdown)
                break;

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_VRDP)
            {
                /* Call the host to get VRDP status and the experience level. */
                VMMDevVRDPChangeRequest vrdpChangeRequest = {0};

                vrdpChangeRequest.header.size            = sizeof(VMMDevVRDPChangeRequest);
                vrdpChangeRequest.header.version         = VMMDEV_REQUEST_HEADER_VERSION;
                vrdpChangeRequest.header.requestType     = VMMDevReq_GetVRDPChangeRequest;
                vrdpChangeRequest.u8VRDPActive           = 0;
                vrdpChangeRequest.u32VRDPExperienceLevel = 0;

                if (DeviceIoControl (gVBoxDriver,
                                     VBOXGUEST_IOCTL_VMMREQUEST(sizeof(VMMDevVRDPChangeRequest)),
                                     &vrdpChangeRequest,
                                     sizeof(VMMDevVRDPChangeRequest),
                                     &vrdpChangeRequest,
                                     sizeof(VMMDevVRDPChangeRequest),
                                     &cbReturned, NULL))
                {
                    LogFlowFunc(("u8VRDPActive = %d, level %d\n", vrdpChangeRequest.u8VRDPActive, vrdpChangeRequest.u32VRDPExperienceLevel));

                    if (vrdpChangeRequest.u8VRDPActive)
                    {
                        pCtx->level = vrdpChangeRequest.u32VRDPExperienceLevel;
                        vboxExperienceSet (pCtx->level);

                        if (pCtx->level == VRDP_EXPERIENCE_LEVEL_ZERO
                            && pCtx->pfnEnableTheming
                            && pCtx->pfnIsThemeActive)
                        {
                            pCtx->fSavedThemeEnabled = pCtx->pfnIsThemeActive ();

                            LogFlowFunc(("pCtx->fSavedThemeEnabled = %d\n", pCtx->fSavedThemeEnabled));

                            if (pCtx->fSavedThemeEnabled)
                            {
                                pCtx->pfnEnableTheming (FALSE);
                            }
                        }
                    }
                    else
                    {
                        if (pCtx->level == VRDP_EXPERIENCE_LEVEL_ZERO
                            && pCtx->pfnEnableTheming
                            && pCtx->pfnIsThemeActive)
                        {
                            if (pCtx->fSavedThemeEnabled)
                            {
                                /* @todo the call returns S_OK but theming remains disabled. */
                                HRESULT hrc = pCtx->pfnEnableTheming (TRUE);
                                LogFlowFunc(("enabling theme rc = 0x%08X\n", hrc));
                                pCtx->fSavedThemeEnabled = FALSE;
                            }
                        }

                        vboxExperienceRestore (pCtx->level);

                        pCtx->level = VRDP_EXPERIENCE_LEVEL_FULL;
                    }
                }
                else
                {
                    /* sleep a bit to not eat too much CPU in case the above call always fails */
                    RTThreadSleep(10);

                    if (*pfShutdown)
                        break;
                }
            }
        }
        else
        {
            /* sleep a bit to not eat too much CPU in case the above call always fails */
            RTThreadSleep(50);

            if (*pfShutdown)
                break;
        }
    }

    maskInfo.u32OrMask  = 0;
    maskInfo.u32NotMask = VMMDEV_EVENT_VRDP;
    if (!DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
        LogFlowFunc(("DeviceIOControl(CtlMask) failed\n"));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * The service description.
 */
VBOXSERVICEDESC g_SvcDescVRDP =
{
    /* pszName. */
    "VRDP",
    /* pszDescription. */
    "VRDP Connection Notification",
    /* methods */
    VBoxVRDPInit,
    VBoxVRDPWorker,
    NULL /* pfnStop */,
    VBoxVRDPDestroy
};

