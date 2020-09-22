/* $Id$ */
/** @file
 * VBox audio devices - Mac OS X CoreAudio audio driver, authorization helpers for Mojave+.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>

#include <iprt/errcore.h>
#include <iprt/semaphore.h>

#import <AVFoundation/AVFoundation.h>
#import <AVFoundation/AVMediaFormat.h>
#import <Foundation/NSException.h>


#if MAC_OS_X_VERSION_MIN_REQUIRED < 101400

/* HACK ALERT! It's there in the 10.13 SDK, but only for iOS 7.0+. Deploying CPP trickery to shut up warnings/errors. */
# if MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
#  define AVAuthorizationStatus                 OurAVAuthorizationStatus
#  define AVAuthorizationStatusNotDetermined    OurAVAuthorizationStatusNotDetermined
#  define AVAuthorizationStatusRestricted       OurAVAuthorizationStatusRestricted
#  define AVAuthorizationStatusDenied           OurAVAuthorizationStatusDenied
#  define AVAuthorizationStatusAuthorized       OurAVAuthorizationStatusAuthorized
# endif

/**
 * The authorization status enum.
 *
 * Starting macOS 10.14 we need to request permissions in order to use any audio input device
 * but as we build against an older SDK where this is not available we have to duplicate
 * AVAuthorizationStatus and do everything dynmically during runtime, sigh...
 */
typedef enum AVAuthorizationStatus: NSInteger
{
    AVAuthorizationStatusNotDetermined = 0,
    AVAuthorizationStatusRestricted    = 1,
    AVAuthorizationStatusDenied        = 2,
    AVAuthorizationStatusAuthorized    = 3,
} AVAuthorizationStatus;

#endif


/**
 * Requests camera permissions for Mojave and onwards.
 *
 * @returns VBox status code.
 */
static int coreAudioInputPermissionRequest(void)
{
    __block RTSEMEVENT hEvt = NIL_RTSEMEVENT;
    __block int rc = RTSemEventCreate(&hEvt);
    if (RT_SUCCESS(rc))
    {
        /* Perform auth request. */
        [AVCaptureDevice performSelector: @selector(requestAccessForMediaType: completionHandler:) withObject: (id)AVMediaTypeAudio withObject: (id)^(BOOL granted) {
            if (!granted) {
                LogRel(("CoreAudio: Access denied!\n"));
                rc = VERR_ACCESS_DENIED;
            }
            RTSemEventSignal(hEvt);
        }];

        rc = RTSemEventWait(hEvt, 10 * RT_MS_1SEC);
        RTSemEventDestroy(hEvt);
    }

    return rc;
}

/**
 * Checks permission for capturing devices on Mojave and onwards.
 *
 * @returns VBox status code.
 */
DECLHIDDEN(int) coreAudioInputPermissionCheck(void)
{
    int rc = VINF_SUCCESS;

    if (NSFoundationVersionNumber >= 10.14)
    {
        /*
         * Because we build with an older SDK where the authorization APIs are not available
         * (introduced with Mojave 10.14) we have to resort to resolving the APIs dynamically.
         */
        LogRel(("CoreAudio: macOS 10.14+ detected, checking audio input permissions\n"));

        if ([AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)])
        {
            AVAuthorizationStatus enmAuthSts = (AVAuthorizationStatus)(NSInteger)[AVCaptureDevice performSelector: @selector(authorizationStatusForMediaType:) withObject: (id)AVMediaTypeAudio];
            if (enmAuthSts == AVAuthorizationStatusNotDetermined)
                rc = coreAudioInputPermissionRequest();
            else if (   enmAuthSts == AVAuthorizationStatusRestricted
                     || enmAuthSts == AVAuthorizationStatusDenied)
            {
                LogRel(("CoreAudio: Access denied!\n"));
                rc = VERR_ACCESS_DENIED;
            }
        }
    }

    return rc;
}
