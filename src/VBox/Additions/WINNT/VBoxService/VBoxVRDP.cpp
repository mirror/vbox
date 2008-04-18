/** @file
 *
 * VBoxVRDP - VBox VRDP connection notification
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

/* 0x0501 for SPI_SETDROPSHADOW */
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include "VBoxService.h"
#include "VBoxVRDP.h"
#include <VBox/VBoxDev.h>
#include <VBoxGuestInternal.h>
#include <iprt/assert.h>
#include "helpers.h"


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

typedef struct _VBoxExperienceParameter
{
    const char *name;
    UINT  uActionSet;
    UINT  uActionGet;
    uint32_t level;                    /* The parameter remain enabled at this or higher level. */
    int   type;
    void  *pvDisable;
    UINT  cbSavedValue;
    char  achSavedValue[2 * MAX_PATH]; /* Large enough to save the bitmap path. */
} VBoxExperienceParameter;

#define SPI_(l, a) #a, SPI_SET##a, SPI_GET##a, VRDP_EXPERIENCE_LEVEL_##l

static VBoxExperienceParameter parameters[] =
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

static void vboxExperienceSet (uint32_t level)
{
    int i;
    for (i = 0; i < ELEMENTS(parameters); i++)
    {
        if (parameters[i].level > level)
        {
            /*
             * The parameter has to be disabled.
             */
            dprintf(("Saving %s\n", parameters[i].name));
            
            /* Save the current value. */
            switch (parameters[i].type)
            {
                case VBOX_SPI_STRING:
                {
                    /* The 2nd parameter is size in characters of the buffer.
                     * The 3rd parameter points to the buffer. 
                     */
                    SystemParametersInfo (parameters[i].uActionGet,
                                          MAX_PATH,
                                          parameters[i].achSavedValue,
                                          0);
                } break;

                case VBOX_SPI_BOOL:
                case VBOX_SPI_BOOL_PTR:
                {
                    /* The 3rd parameter points to BOOL. */
                    SystemParametersInfo (parameters[i].uActionGet,
                                          0,
                                          parameters[i].achSavedValue,
                                          0);
                } break;

                case VBOX_SPI_PTR:
                {
                    /* The 3rd parameter points to the structure. 
                     * The cbSize member of this structure must be set.
                     * The uiParam parameter must alos be set.
                     */
                    if (parameters[i].cbSavedValue > sizeof (parameters[i].achSavedValue))
                    {
                        dprintf(("not enough space %d > %d\n", parameters[i].cbSavedValue, sizeof (parameters[i].achSavedValue)));
                        break;
                    }
                    
                    *(UINT *)&parameters[i].achSavedValue[0] = parameters[i].cbSavedValue;
                    
                    SystemParametersInfo (parameters[i].uActionGet,
                                          parameters[i].cbSavedValue,
                                          parameters[i].achSavedValue,
                                          0);
                } break;

                default:
                    break;
            }
            
            dprintf(("Disabling %s\n", parameters[i].name));
            
            /* Disable the feature. */
            switch (parameters[i].type)
            {
                case VBOX_SPI_STRING:
                {
                    /* The 3rd parameter points to the string. */
                    SystemParametersInfo (parameters[i].uActionSet,
                                          0,
                                          parameters[i].pvDisable,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_BOOL:
                {
                    /* The 2nd parameter is BOOL. */
                    SystemParametersInfo (parameters[i].uActionSet,
                                          FALSE,
                                          NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_BOOL_PTR:
                {
                    /* The 3rd parameter is NULL to disable. */
                    SystemParametersInfo (parameters[i].uActionSet,
                                          0,
                                          NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_PTR:
                {
                    /* The 3rd parameter points to the structure. */
                    SystemParametersInfo (parameters[i].uActionSet,
                                          0,
                                          parameters[i].pvDisable,
                                          SPIF_SENDCHANGE);
                } break;

                default:
                    break;
            }
        }
    }
}

static void vboxExperienceRestore (uint32_t level)
{
    int i;
    for (i = 0; i < ELEMENTS(parameters); i++)
    {
        if (parameters[i].level > level)
        {
            dprintf(("Restoring %s\n", parameters[i].name));
            
            /* Restore the feature. */
            switch (parameters[i].type)
            {
                case VBOX_SPI_STRING:
                {
                    /* The 3rd parameter points to the string. */
                    SystemParametersInfo (parameters[i].uActionSet,
                                          0,
                                          parameters[i].achSavedValue,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_BOOL:
                {
                    /* The 2nd parameter is BOOL. */
                    SystemParametersInfo (parameters[i].uActionSet,
                                          *(BOOL *)&parameters[i].achSavedValue[0],
                                          NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_BOOL_PTR:
                {
                    /* The 3rd parameter is NULL to disable. */
                    BOOL fSaved = *(BOOL *)&parameters[i].achSavedValue[0];

                    SystemParametersInfo (parameters[i].uActionSet,
                                          0,
                                          fSaved? &fSaved: NULL,
                                          SPIF_SENDCHANGE);
                } break;

                case VBOX_SPI_PTR:
                {
                    /* The 3rd parameter points to the structure. */
                    SystemParametersInfo (parameters[i].uActionSet,
                                          0,
                                          parameters[i].achSavedValue,
                                          SPIF_SENDCHANGE);
                } break;

                default:
                    break;
            }
        }
    }
}


typedef struct _VBOXVRDPCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    uint32_t level;
    BOOL fSavedThemeEnabled;
    
    HMODULE hModule;

    HRESULT (* pfnEnableTheming)(BOOL fEnable);
    BOOL (* pfnIsThemeActive)(VOID);
} VBOXVRDPCONTEXT;


static VBOXVRDPCONTEXT gCtx = {0};


int VBoxVRDPInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    dprintf(("VBoxVRDPInit\n"));

    gCtx.pEnv      = pEnv;
    gCtx.level     = VRDP_EXPERIENCE_LEVEL_FULL;
    gCtx.fSavedThemeEnabled = FALSE;

    gCtx.hModule = LoadLibrary("UxTheme");
    
    if (gCtx.hModule)
    {
        *(uintptr_t *)&gCtx.pfnEnableTheming = (uintptr_t)GetProcAddress(gCtx.hModule, "EnableTheming");
        *(uintptr_t *)&gCtx.pfnIsThemeActive = (uintptr_t)GetProcAddress(gCtx.hModule, "IsThemeActive");
    }
    else
    {
        gCtx.pfnEnableTheming = 0;
    }
    
    *pfStartThread = true;
    *ppInstance = &gCtx;
    return VINF_SUCCESS;
}


void VBoxVRDPDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    dprintf(("VBoxVRDPDestroy\n"));
    VBOXVRDPCONTEXT *pCtx = (VBOXVRDPCONTEXT *)pInstance;
    vboxExperienceRestore (pCtx->level);
    if (gCtx.hModule)
        FreeLibrary(gCtx.hModule);
    gCtx.hModule = 0;
    return;
}

/**
 * Thread function to wait for and process mode change requests
 */
unsigned __stdcall VBoxVRDPThread(void *pInstance)
{
    VBOXVRDPCONTEXT *pCtx = (VBOXVRDPCONTEXT *)pInstance;
    HANDLE gVBoxDriver = pCtx->pEnv->hDriver;
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;
    
    maskInfo.u32OrMask = VMMDEV_EVENT_VRDP;
    maskInfo.u32NotMask = 0;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxVRDPThread: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxVRDPThread: DeviceIOControl(CtlMask) failed\n"));
        return 0;
    }

    do
    {
        /* wait for the event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn   = 5000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_VRDP;
        if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            dprintf(("VBoxVRDPThread: DeviceIOControl succeded\n"));

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            dprintf(("VBoxVRDPThread: checking event\n"));

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
                                     IOCTL_VBOXGUEST_VMMREQUEST,
                                     &vrdpChangeRequest,
                                     sizeof(VMMDevVRDPChangeRequest),
                                     &vrdpChangeRequest,
                                     sizeof(VMMDevVRDPChangeRequest),
                                     &cbReturned, NULL))
                {
                    dprintf(("VBoxVRDPThread: u8VRDPActive = %d, level %d\n", vrdpChangeRequest.u8VRDPActive, vrdpChangeRequest.u32VRDPExperienceLevel));
                    
                    if (vrdpChangeRequest.u8VRDPActive)
                    {
                        pCtx->level = vrdpChangeRequest.u32VRDPExperienceLevel;
                        vboxExperienceSet (pCtx->level);
    
                        if (pCtx->level == VRDP_EXPERIENCE_LEVEL_ZERO
                            && pCtx->pfnEnableTheming
                            && pCtx->pfnIsThemeActive)
                        {
                            pCtx->fSavedThemeEnabled = pCtx->pfnIsThemeActive ();
                            
                            dprintf(("VBoxVRDPThread: pCtx->fSavedThemeEnabled = %d\n", pCtx->fSavedThemeEnabled));
                            
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
                                dprintf(("VBoxVRDPThread: enabling theme rc = 0x%08X\n", hrc));
                                pCtx->fSavedThemeEnabled = FALSE;
                            }
                        }
                        
                        vboxExperienceRestore (pCtx->level);
                        
                        pCtx->level = VRDP_EXPERIENCE_LEVEL_FULL;
                    }
                }
                else
                {
                    dprintf(("VBoxService: error from DeviceIoControl IOCTL_VBOXGUEST_VMMREQUEST\n"));

                    /* sleep a bit to not eat too much CPU in case the above call always fails */
                    if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 10) == WAIT_OBJECT_0)
                    {
                        fTerminate = true;
                        break;
                    }
                }
            }
        } 
        else
        {
            dprintf(("VBoxService: error from DeviceIoControl IOCTL_VBOXGUEST_WAITEVENT\n"));

            /* sleep a bit to not eat too much CPU in case the above call always fails */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 10) == WAIT_OBJECT_0)
            {
                fTerminate = true;
                break;
            }
        }
    } while (!fTerminate);

    maskInfo.u32OrMask = 0;
    maskInfo.u32NotMask = VMMDEV_EVENT_VRDP;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxVRDPThread: DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxVRDPThread: DeviceIOControl(CtlMask) failed\n"));
    }

    dprintf(("VBoxVRDPThread: finished VRDP change request thread\n"));
    return 0;
}


