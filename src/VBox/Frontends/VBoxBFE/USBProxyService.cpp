/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implemenation of USBProxyService class
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "USBProxyService.h"
#include "Logging.h"

#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>



/** @todo add the required locking. */

/**
 * Initialize data members.
 */
USBProxyService::USBProxyService (HostUSB *aHost)
    : mHost (aHost), mThread (NIL_RTTHREAD), mTerminate (false), mDevices (), mLastError (VINF_SUCCESS)
{
    LogFlowMember (("USBProxyService::USBProxyService: aHost=%p\n", aHost));
}


/**
 * Empty destructor.
 */
USBProxyService::~USBProxyService()
{
    LogFlowMember (("USBProxyService::~USBProxyService: \n"));
    Assert (mThread == NIL_RTTHREAD);
    mDevices.clear();
    mTerminate = true;
    mHost = NULL;
}


bool USBProxyService::isActive (void)
{
    return mThread != NIL_RTTHREAD;
}


int USBProxyService::getLastError (void)
{
    return mLastError;
}


int USBProxyService::start (void)
{
    int rc = VINF_SUCCESS;
    if (mThread == NIL_RTTHREAD)
    {
        /*
         * Force update before starting the poller thread.
         */
        wait (0);
        processChanges ();

        /*
         * Create the poller thread which will look for changes.
         */
        mTerminate = false;
        rc = RTThreadCreate (&mThread, USBProxyService::serviceThread, this,
                             0, RTTHREADTYPE_INFREQUENT_POLLER, RTTHREADFLAGS_WAITABLE, "USBPROXY");
        AssertRC (rc);
        if (VBOX_SUCCESS (rc))
            LogFlow (("USBProxyService::start: started mThread=%RTthrd\n", mThread));
        else
        {
            mThread = NIL_RTTHREAD;
            mLastError = rc;
        }
    }
    else
        LogFlow (("USBProxyService::start: already running, mThread=%RTthrd\n", mThread));
    return rc;
}


int USBProxyService::stop (void)
{
    int rc = VINF_SUCCESS;
    if (mThread != NIL_RTTHREAD)
    {
        /*
         * Mark the thread for termination and kick it.
         */
        ASMAtomicXchgSize (&mTerminate, true);
        rc = interruptWait();
        AssertRC (rc);

        /*
         * Wait for the thread to finish and then update the state.
         */
        rc = RTThreadWait (mThread, 60000, NULL);
        if (rc == VERR_INVALID_HANDLE)
            rc = VINF_SUCCESS;
        if (VBOX_SUCCESS (rc))
        {
            LogFlowMember (("USBProxyService::stop: stopped mThread=%RTthrd\n", mThread));
            mThread = NIL_RTTHREAD;
            mTerminate = false;
        }
        else
        {
            AssertRC (rc);
            mLastError = rc;
        }
    }
    else
        LogFlowMember (("USBProxyService::stop: not active\n"));

    return rc;
}


/**
 * Sort a list of USB devices.
 *
 * @returns Pointer to the head of the sorted doubly linked list.
 * @param   aDevices        Head pointer (can be both singly and doubly linked list).
 */
static PUSBDEVICE sortDevices (PUSBDEVICE pDevices)
{
    PUSBDEVICE pHead = NULL;
    PUSBDEVICE pTail = NULL;
    while (pDevices)
    {
        /* unlink head */
        PUSBDEVICE pDev = pDevices;
        pDevices = pDev->pNext;
        if (pDevices)
            pDevices->pPrev = NULL;

        /* find location. */
        PUSBDEVICE pCur = pTail;
        while (     pCur
               &&   HostUSBDevice::compare (pCur, pDev) > 0)
            pCur = pCur->pPrev;

        /* insert (after pCur) */
        pDev->pPrev = pCur;
        if (pCur)
        {
            pDev->pNext = pCur->pNext;
            pCur->pNext = pDev;
            if (pDev->pNext)
                pDev->pNext->pPrev = pDev;
            else
                pTail = pDev;
        }
        else
        {
            pDev->pNext = pHead;
            if (pHead)
                pHead->pPrev = pDev;
            else
                pTail = pDev;
            pHead = pDev;
        }
    }

    return pHead;
}


void USBProxyService::processChanges (void)
{
    LogFlowMember (("USBProxyService::processChanges: \n"));

    /*
     * Get the sorted list of USB devices.
     */
    PUSBDEVICE pDevices = getDevices();
    if (pDevices)
    {
        pDevices = sortDevices (pDevices);

        /*
         * Compare previous list with the previous list of devices
         * and merge in any changes while notifying Host.
         */
        HostUSBDeviceList::iterator It = this->mDevices.begin();
        while (     It != mDevices.end()
               ||   pDevices)
        {
            /*
             * Compare.
             */
            HostUSBDevice *DevPtr = 0; /* shut up gcc */
            int iDiff;
            if (It == mDevices.end())
                iDiff = 1;
            else
            {
                DevPtr = *It;
                if (!pDevices)
                    iDiff = -1;
                else
                    iDiff = DevPtr->compare (pDevices);
            }
            if (!iDiff)
            {
                /*
                 * Device still there, update the state and move on.
                 */
                if (DevPtr->updateState (pDevices))
                    mHost->onUSBDeviceStateChanged (DevPtr);
                It++;
                PUSBDEVICE pFree = pDevices;
                pDevices = pDevices->pNext; /* treated as singly linked */
                freeDevice (pFree);
                /** @todo detect status changes! */
            }
            else
            {
                if (iDiff > 0)
                {
                    /*
                     * Head of pDevices was attached.
                     */
                    PUSBDEVICE pNew = pDevices;
                    pDevices = pDevices->pNext;
                    pNew->pPrev = pNew->pNext = NULL;

                    HostUSBDevice *NewObj = new HostUSBDevice;
                    NewObj->init (pNew, this);
                    Log (("USBProxyService::processChanges: attached %p/%p:{.idVendor=%#06x, .idProduct=%#06x, .pszProduct=\"%s\", .pszManufacturer=\"%s\"}\n",
                          NewObj, pNew, pNew->idVendor, pNew->idProduct,
                          pNew->pszProduct, pNew->pszManufacturer));

                    mDevices.insert (It, NewObj);
                    mHost->onUSBDeviceAttached (NewObj);
                }
                else
                {
                    /*
                     * DevPtr was detached.
                     */
                    It = mDevices.erase (It);
                    mHost->onUSBDeviceDetached (DevPtr);
                    Log (("USBProxyService::processChanges: detached %p\n", (HostUSBDevice *)DevPtr)); /** @todo add details .*/
                }
            }
        } /* while */
    }
    else
    {
        /* All devices were detached */
        HostUSBDeviceList::iterator It = this->mDevices.begin();
        while (It != mDevices.end())
        {
            HostUSBDevice *DevPtr = *It;
            /*
             * DevPtr was detached.
             */
            It = mDevices.erase (It);
            mHost->onUSBDeviceDetached (DevPtr);
            Log (("USBProxyService::processChanges: detached %p\n", (HostUSBDevice *)DevPtr)); /** @todo add details .*/
        }
    }

    LogFlowMember (("USBProxyService::processChanges: returns void\n"));
}


/*static*/ DECLCALLBACK (int) USBProxyService::serviceThread (RTTHREAD Thread, void *pvUser)
{
    USBProxyService *pThis = (USBProxyService *)pvUser;
    LogFlow (("USBProxyService::serviceThread: pThis=%p\n", pThis));

    /*
     * Processing loop.
     */
    for (;;)
    {
        pThis->wait (RT_INDEFINITE_WAIT);
        if (pThis->mTerminate)
            break;
        pThis->processChanges();
    }

    LogFlow (("USBProxyService::serviceThread: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

/*static*/ void USBProxyService::freeInterfaceMembers (PUSBINTERFACE pIf, unsigned cIfs)
{
    while (cIfs-- > 0)
    {
        RTMemFree (pIf->paEndpoints);
        pIf->paEndpoints = NULL;
        RTStrFree ((char *)pIf->pszDriver);
        pIf->pszDriver = NULL;
        RTStrFree ((char *)pIf->pszInterface);
        pIf->pszInterface = NULL;

        freeInterfaceMembers(pIf->paAlts, pIf->cAlts);
        RTMemFree(pIf->paAlts);
        pIf->paAlts = NULL;
        pIf->cAlts = 0;

        /* next */
        pIf++;
    }
}

/*static*/ void USBProxyService::freeDevice (PUSBDEVICE pDevice)
{
    PUSBCONFIG pCfg = pDevice->paConfigurations;
    unsigned cCfgs = pDevice->bNumConfigurations;
    while (cCfgs-- > 0)
    {
        freeInterfaceMembers (pCfg->paInterfaces, pCfg->bNumInterfaces);
        RTMemFree (pCfg->paInterfaces);
        pCfg->paInterfaces = NULL;
        pCfg->bNumInterfaces = 0;

        RTStrFree ((char *)pCfg->pszConfiguration);
        pCfg->pszConfiguration = NULL;

        /* next */
        pCfg++;
    }
    RTMemFree (pDevice->paConfigurations);
    pDevice->paConfigurations = NULL;

    RTStrFree ((char *)pDevice->pszManufacturer);
    pDevice->pszManufacturer = NULL;
    RTStrFree ((char *)pDevice->pszProduct);
    pDevice->pszProduct = NULL;
    RTStrFree ((char *)pDevice->pszSerialNumber);
    pDevice->pszSerialNumber = NULL;

    RTStrFree ((char *)pDevice->pszAddress);
    pDevice->pszAddress = NULL;

    RTMemFree (pDevice);

}


/* static */ uint64_t USBProxyService::calcSerialHash (const char *aSerial)
{
    if (!aSerial)
        aSerial = "";

    register const uint8_t *pu8 = (const uint8_t *)aSerial;
    register uint64_t u64 = 14695981039346656037ULL;
    for (;;)
    {
        register uint8_t u8 = *pu8;
        if (!u8)
            break;
        u64 = (u64 * 1099511628211ULL) ^ u8;
        pu8++;
    }

    return u64;
}



/* Stubs which the host specific classes overrides: */


int USBProxyService::wait (unsigned aMillies)
{
    return RTThreadSleep (250);
}


int USBProxyService::interruptWait (void)
{
    return VERR_NOT_IMPLEMENTED;
}


PUSBDEVICE USBProxyService::getDevices (void)
{
    return NULL;
}


int USBProxyService::captureDevice (HostUSBDevice *pDevice)
{
    return VERR_NOT_IMPLEMENTED;
}


int USBProxyService::holdDevice (HostUSBDevice *pDevice)
{
    return VERR_NOT_IMPLEMENTED;
}


int USBProxyService::releaseDevice (HostUSBDevice *pDevice)
{
    return VERR_NOT_IMPLEMENTED;
}


int USBProxyService::resetDevice (HostUSBDevice *pDevice)
{
    return VERR_NOT_IMPLEMENTED;
}

