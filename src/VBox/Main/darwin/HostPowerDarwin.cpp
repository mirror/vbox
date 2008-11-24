/** @file
 *
 * VirtualBox interface to host's power notification service
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "HostPower.h"
#include "Logging.h"

#include <IOKit/IOMessage.h>

HostPowerServiceDarwin::HostPowerServiceDarwin (VirtualBox *aVirtualBox)
  : HostPowerService (aVirtualBox),
    mThread (NULL),
    mRootPort (MACH_PORT_NULL),
    mNotifyPort (nil),
    mRunLoop (nil)
{
    /* Create the new worker thread. */
    int rc = RTThreadCreate (&mThread, HostPowerServiceDarwin::powerChangeNotificationThread, this, 65536,
                             RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "MainPower");

    if (RT_FAILURE (rc))
        LogFlow (("RTThreadCreate failed with %Rrc\n", rc));
}

HostPowerServiceDarwin::~HostPowerServiceDarwin()
{
    /* Jump out of the run loop. */
    CFRunLoopStop (mRunLoop);
    /* Remove the sleep notification port from the application runloop. */
    CFRunLoopRemoveSource (CFRunLoopGetCurrent(),
                           IONotificationPortGetRunLoopSource (mNotifyPort),
                           kCFRunLoopCommonModes);
    /* Deregister for system sleep notifications. */
    IODeregisterForSystemPower (&mNotifierObject);
    /* IORegisterForSystemPower implicitly opens the Root Power Domain
     * IOService so we close it here. */
    IOServiceClose (mRootPort);
    /* Destroy the notification port allocated by IORegisterForSystemPower */
    IONotificationPortDestroy (mNotifyPort);
}

DECLCALLBACK(int) HostPowerServiceDarwin::powerChangeNotificationThread (RTTHREAD ThreadSelf, void *pInstance)
{
    HostPowerServiceDarwin *pPowerObj = static_cast<HostPowerServiceDarwin *> (pInstance);

//    OSErr retCode = AEInstallEventHandler(kAEMacPowerMgtEvt, kAEMacLowPowerSaveData,
//                                          HostPowerServiceDarwin::lowPowerEventHandler,
//                                          pPowerObj, false);

    /* Register to receive system sleep notifications */
    pPowerObj->mRootPort = IORegisterForSystemPower (pPowerObj, &pPowerObj->mNotifyPort,
                                                     HostPowerServiceDarwin::powerChangeNotificationHandler,
                                                     &pPowerObj->mNotifierObject);
    if (pPowerObj->mRootPort == MACH_PORT_NULL)
    {
        LogFlow (("IORegisterForSystemPower failed\n"));
        return VERR_NOT_SUPPORTED;
    }
    pPowerObj->mRunLoop = CFRunLoopGetCurrent();
    /* Add the notification port to the application runloop */
    CFRunLoopAddSource (pPowerObj->mRunLoop,
                        IONotificationPortGetRunLoopSource (pPowerObj->mNotifyPort),
                        kCFRunLoopCommonModes);
    /* Start the run loop. This blocks. */
    CFRunLoopRun();

    return VINF_SUCCESS;
}

void HostPowerServiceDarwin::powerChangeNotificationHandler (void *pData, io_service_t service, natural_t messageType, void *pMessageArgument)
{
    HostPowerServiceDarwin *pPowerObj = static_cast<HostPowerServiceDarwin *> (pData);
//    printf( "messageType %08lx, arg %08lx\n", (long unsigned int)messageType, (long unsigned int)pMessageArgument );

    switch (messageType)
    {
        case kIOMessageCanSystemSleep:
            {
                /* Idle sleep is about to kick in. This message will not be
                 * sent for forced sleep. Applications have a chance to prevent
                 * sleep by calling IOCancelPowerChange. Most applications
                 * should not prevent idle sleep. Power Management waits up to
                 * 30 seconds for you to either allow or deny idle sleep. If
                 * you don't acknowledge this power change by calling either
                 * IOAllowPowerChange or IOCancelPowerChange, the system will
                 * wait 30 seconds then go to sleep. */
                IOAllowPowerChange (pPowerObj->mRootPort, reinterpret_cast<long> (pMessageArgument));
                break;
            }
        case kIOMessageSystemWillSleep:
            {
                /* The system will go for sleep. */
                pPowerObj->notify (HostPowerEvent_Suspend);
                /* If you do not call IOAllowPowerChange or IOCancelPowerChange to
                 * acknowledge this message, sleep will be delayed by 30 seconds.
                 * NOTE: If you call IOCancelPowerChange to deny sleep it returns
                 * kIOReturnSuccess, however the system WILL still go to sleep. */
                IOAllowPowerChange (pPowerObj->mRootPort, reinterpret_cast<long> (pMessageArgument));
                break;
            }
        case kIOMessageSystemWillPowerOn:
            {
                /* System has started the wake up process. */
                break;
            }
        case kIOMessageSystemHasPoweredOn:
            {
                /* System has finished the wake up process. */
                pPowerObj->notify (HostPowerEvent_Resume);
                break;
            }
        default:
            break;
    }
}

OSErr HostPowerServiceDarwin::lowPowerEventHandler (const AppleEvent *event, AppleEvent *replyEvent, long data)
{
    printf ("low power event\n");
    LogRel (("low power event\n"));
//    HostPowerServiceDarwin *pPowerObj = reinterpret_cast<HostPowerServiceDarwin *> (data);
//    pPowerObj->notify(HostPowerEvent_BatteryLow);
    return noErr;
}

