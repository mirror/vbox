/* $Id$ */
/** @file
 * VirtualBox Support Driver - VBoxKeyboard IOService.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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

#define LOG_GROUP LOG_GROUP_SUP_DRV

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/hidsystem/IOHIDUsageTables.h>

#include <iprt/asm.h>
#include <iprt/semaphore.h>
#include <iprt/err.h>
#include <VBox/log.h>

#define superservice IOService
#define superclient  IOUserClient

#define VBOX_KEYBOARD_SERVICE_REQ_TIMEOUT   (1000)

/** The number of IOService class instances. */
static bool volatile        g_fInstantiated     = 0;

class org_virtualbox_VBoxKeyboard : public IOService
{
    OSDeclareDefaultStructors(org_virtualbox_VBoxKeyboard);

    protected:

        RTSEMMUTEX  lock;

    public:

        virtual bool start(IOService* provider);
        virtual void stop(IOService* provider);
        virtual bool terminate(IOOptionBits fOptions);

        IOReturn         syncKeyboardDevicesLeds(void);
};

OSDefineMetaClassAndStructors(org_virtualbox_VBoxKeyboard, IOService);

class org_virtualbox_VBoxKeyboardClient : public IOUserClient
{
    OSDeclareDefaultStructors(org_virtualbox_VBoxKeyboardClient);

    protected:

        org_virtualbox_VBoxKeyboard     *pServiceProvider;

        virtual IOReturn externalMethod(uint32_t iCmd, IOExternalMethodArguments *pArgs, IOExternalMethodDispatch *pDispatchFn, OSObject *pTarget, void *pReference);

    public:

        virtual bool     start(IOService* provider);
        virtual IOReturn clientClose(void);
};

OSDefineMetaClassAndStructors(org_virtualbox_VBoxKeyboardClient, IOUserClient);

bool org_virtualbox_VBoxKeyboard::start(IOService* pProvider)
{
    if (!superservice::start(pProvider))
        return false;

    /* Low level initialization should be performed only once */
    if (!ASMAtomicCmpXchgBool(&g_fInstantiated, true, false))
    {
        superservice::stop(pProvider);
        return false;
    }

    if (RTSemMutexCreate((PRTSEMMUTEX)&lock) != 0)
    {
        Log2(("Unable to create mutex\n"));

        superservice::stop(pProvider);
        return false;
    }

    registerService();

    return true;
}

void org_virtualbox_VBoxKeyboard::stop(IOService* pProvider)
{
    Log2(("Stopping %s.\n", getName()));

    if (RTSemMutexDestroy(lock) != 0)
    {
        Log2(("Unable to destroy mutex\n"));
    }

    superservice::stop(pProvider);

    ASMAtomicWriteBool(&g_fInstantiated, false);
}

bool org_virtualbox_VBoxKeyboard::terminate(IOOptionBits fOptions)
{
    return superservice::terminate(fOptions);
}

IOReturn org_virtualbox_VBoxKeyboard::syncKeyboardDevicesLeds(void)
{
    OSDictionary *pMatchingDictionary;

    Log2(("org_virtualbox_VBoxKeyboardClient::syncKeyboardDevicesLeds\n"));

    pMatchingDictionary = IOService::serviceMatching("IOHIDKeyboard");
    if (pMatchingDictionary)
    {
        OSIterator    *pIter;
        IOHIKeyboard  *pKeyboard;
        int rc;

        rc = RTSemMutexRequest(lock, VBOX_KEYBOARD_SERVICE_REQ_TIMEOUT);
        if (RT_SUCCESS(rc))
        {
            pIter = IOService::getMatchingServices(pMatchingDictionary);
            if (pIter)
            {
                unsigned fLeds;
                unsigned fNewFlags;
                unsigned fDeviceFlags;

                while ((pKeyboard = (IOHIKeyboard *)pIter->getNextObject()))
                {
                    fLeds = pKeyboard->getLEDStatus();

                    /* Take care about CAPSLOCK */
                    pKeyboard->setAlphaLock((fLeds & kHIDUsage_LED_CapsLock) ? true : false);
                    fDeviceFlags = pKeyboard->deviceFlags();
                    fNewFlags = ((fLeds & kHIDUsage_LED_CapsLock)) ? fDeviceFlags | NX_ALPHASHIFTMASK : fDeviceFlags & ~NX_ALPHASHIFTMASK;
                    pKeyboard->setDeviceFlags(fNewFlags);

                    /* Take care about NUMLOCK */
                    pKeyboard->setNumLock  ((fLeds & kHIDUsage_LED_NumLock ) ? true : false);
                    fDeviceFlags = pKeyboard->deviceFlags();
                    fNewFlags = ((fLeds & kHIDUsage_LED_NumLock )) ? fDeviceFlags | NX_NUMERICPADMASK : fDeviceFlags & ~NX_NUMERICPADMASK;
                    pKeyboard->setDeviceFlags(fNewFlags);
                }

                pIter->release();
            }

            RTSemMutexRelease(lock);
        }

        pMatchingDictionary->release();
    }

    return 0;
}

bool org_virtualbox_VBoxKeyboardClient::start(IOService *pProvider)
{
    pServiceProvider = OSDynamicCast(org_virtualbox_VBoxKeyboard, pProvider);
    if (pServiceProvider)
    {
        superclient::start(pProvider);
        Log2(("Started: %s\n", getName()));
        return true;
    }

    Log2(("Failed to start: %s\n", getName()));

    return false;
}

IOReturn org_virtualbox_VBoxKeyboardClient::clientClose(void)
{
    if (pServiceProvider)
    {
        if (pServiceProvider->isOpen(this))
            pServiceProvider->close(this);
        else
            return kIOReturnError;
    }

    if (superclient::terminate())
    {
        Log2(("Terminated successfully\n"));
        return kIOReturnSuccess;
    }
    else
    {
        Log2(("Failed to terminate\n"));
        return kIOReturnError;
    }

}

IOReturn org_virtualbox_VBoxKeyboardClient::externalMethod(uint32_t iCmd, IOExternalMethodArguments *pArgs,
    IOExternalMethodDispatch *pDispatchFn, OSObject *pTarget, void *pReference)
{
    IOExternalMethodDispatch *pStaticFnList;
    uint32_t                  cStaticFnList;
    IOReturn                  rc;

    switch (iCmd)
    {
        case 0:
            rc = pServiceProvider->syncKeyboardDevicesLeds();
            break;

        default:
            rc = kIOReturnBadArgument;
    }

    /* Do not call parent class externalMethod()! */
    return rc;
}
