/* $Id$ */
/** @file
 * VirtualBox audio base class for Main audio drivers.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#include "LoggingNew.h"

#include <VBox/log.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pdmdrv.h>

#include "AudioDriver.h"
#include "ConsoleImpl.h"

AudioDriver::AudioDriver(Console *pConsole)
    : mpConsole(pConsole)
    , mfAttached(false)
    , muLUN(UINT8_MAX)
{
}

AudioDriver::~AudioDriver(void)
{
}

/**
 * Returns the next free LUN of the audio device driver
 * chain.
 *
 * @return unsigned             Next free LUN in audio device driver chain.
 */
unsigned AudioDriver::getFreeLUN(void)
{
    Console::SafeVMPtrQuiet ptrVM(mpConsole);
    Assert(ptrVM.isOk());

    PUVM pUVM = ptrVM.rawUVM();
    AssertPtr(pUVM);

    unsigned uLUN = 0;

    PCFGMNODE pDevLUN;
    for (;;)
    {
        pDevLUN = CFGMR3GetChildF(CFGMR3GetRootU(pUVM), "Devices/%s/%u/LUN#%u/", mCfg.strDev.c_str(), mCfg.uInst, uLUN);
        if (!pDevLUN)
            break;
        uLUN++;
    }

    return uLUN;
}


/**
 * Initializes the audio driver with a certain (device) configuration.
 *
 * @note The driver's LUN will be determined on runtime when attaching the
 *       driver to the audio driver chain.
 *
 * @returns VBox status code.
 * @param   pCfg                Audio driver configuration to use.
 */
int AudioDriver::InitializeConfig(AudioDriverCfg *pCfg)
{
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    /* Apply configuration. */
    mCfg = *pCfg;

    return VINF_SUCCESS;
}


/**
 * Attaches the driver via EMT, if configured.
 *
 * @returns IPRT status code.
 * @param   pUVM                The user mode VM handle for talking to EMT.
 * @param   pAutoLock           The callers auto lock instance.  Can be NULL if
 *                              not locked.
 */
int AudioDriver::doAttachDriverViaEmt(PUVM pUVM, util::AutoWriteLock *pAutoLock)
{
    if (!isConfigured())
        return VINF_SUCCESS;

    PVMREQ pReq;
    int vrc = VMR3ReqCallU(pUVM, VMCPUID_ANY, &pReq, 0 /* no wait! */, VMREQFLAGS_VBOX_STATUS,
                           (PFNRT)attachDriverOnEmt, 1, this);
    if (vrc == VERR_TIMEOUT)
    {
        /* Release the lock before a blocking VMR3* call (EMT might wait for it, @bugref{7648})! */
        if (pAutoLock)
            pAutoLock->release();

        vrc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);

        if (pAutoLock)
            pAutoLock->acquire();
    }

    AssertRC(vrc);
    VMR3ReqFree(pReq);

    return vrc;
}


/**
 * Configures the audio driver (to CFGM) and attaches it to the audio chain.
 * Does nothing if the audio driver already is attached.
 *
 * @returns VBox status code.
 * @param   pThis               Audio driver to detach.
 */
/* static */
DECLCALLBACK(int) AudioDriver::attachDriverOnEmt(AudioDriver *pThis)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    Console::SafeVMPtrQuiet ptrVM(pThis->mpConsole);
    Assert(ptrVM.isOk());


    if (pThis->mfAttached) /* Already attached? Bail out. */
    {
        LogFunc(("%s: Already attached\n", pThis->mCfg.strName.c_str()));
        return VINF_SUCCESS;
    }

    AudioDriverCfg *pCfg = &pThis->mCfg;

    unsigned uLUN = pThis->muLUN;
    if (uLUN == UINT8_MAX) /* No LUN assigned / configured yet? Retrieve it. */
        uLUN = pThis->getFreeLUN();

    LogFunc(("strName=%s, strDevice=%s, uInst=%u, uLUN=%u\n",
             pCfg->strName.c_str(), pCfg->strDev.c_str(), pCfg->uInst, uLUN));

    int vrc = pThis->configure(uLUN, true /* Attach */);
    if (RT_SUCCESS(vrc))
        vrc = PDMR3DeviceAttach(ptrVM.rawUVM(), pCfg->strDev.c_str(), pCfg->uInst, uLUN, 0 /* fFlags */,
                                NULL /* ppBase */);
    if (RT_SUCCESS(vrc))
    {
        pThis->muLUN      = uLUN;
        pThis->mfAttached = true;

        LogRel2(("%s: Driver attached (LUN #%u)\n", pCfg->strName.c_str(), pThis->muLUN));
    }
    else
        LogRel(("%s: Failed to attach audio driver, rc=%Rrc\n", pCfg->strName.c_str(), vrc));

    return vrc;
}


/**
 * Detatches the driver via EMT, if configured.
 *
 * @returns IPRT status code.
 * @param   pUVM                The user mode VM handle for talking to EMT.
 * @param   pAutoLock           The callers auto lock instance.  Can be NULL if
 *                              not locked.
 */
int AudioDriver::doDetachDriverViaEmt(PUVM pUVM, util::AutoWriteLock *pAutoLock)
{
    if (!isConfigured())
        return VINF_SUCCESS;

    PVMREQ pReq;
    int vrc = VMR3ReqCallU(pUVM, VMCPUID_ANY, &pReq, 0 /* no wait! */, VMREQFLAGS_VBOX_STATUS,
                           (PFNRT)detachDriverOnEmt, 1, this);
    if (vrc == VERR_TIMEOUT)
    {
        /* Release the lock before a blocking VMR3* call (EMT might wait for it, @bugref{7648})! */
        if (pAutoLock)
            pAutoLock->release();

        vrc = VMR3ReqWait(pReq, RT_INDEFINITE_WAIT);

        if (pAutoLock)
            pAutoLock->acquire();
    }

    AssertRC(vrc);
    VMR3ReqFree(pReq);

    return vrc;
}


/**
 * Detaches an already attached audio driver from the audio chain.
 * Does nothing if the audio driver already is detached or not attached.
 *
 * @returns VBox status code.
 * @param   pThis               Audio driver to detach.
 */
/* static */
DECLCALLBACK(int) AudioDriver::detachDriverOnEmt(AudioDriver *pThis)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (!pThis->mfAttached) /* Not attached? Bail out. */
    {
        LogFunc(("%s: Not attached\n", pThis->mCfg.strName.c_str()));
        return VINF_SUCCESS;
    }

    Console::SafeVMPtrQuiet ptrVM(pThis->mpConsole);
    Assert(ptrVM.isOk());

    Assert(pThis->muLUN != UINT8_MAX);

    AudioDriverCfg *pCfg = &pThis->mCfg;

    LogFunc(("strName=%s, strDevice=%s, uInst=%u, uLUN=%u\n",
             pCfg->strName.c_str(), pCfg->strDev.c_str(), pCfg->uInst, pThis->muLUN));

    int vrc = PDMR3DriverDetach(ptrVM.rawUVM(), pCfg->strDev.c_str(), pCfg->uInst, pThis->muLUN, "AUDIO",
                            0 /* iOccurrence */, 0 /* fFlags */);
    if (RT_SUCCESS(vrc))
        vrc = pThis->configure(pThis->muLUN, false /* Detach */);

    if (RT_SUCCESS(vrc))
    {
        pThis->muLUN      = UINT8_MAX;
        pThis->mfAttached = false;

        LogRel2(("%s: Driver detached\n", pCfg->strName.c_str()));
    }
    else
        LogRel(("%s: Failed to detach audio driver, rc=%Rrc\n", pCfg->strName.c_str(), vrc));

    return vrc;
}

/**
 * Configures the audio driver via CFGM.
 *
 * @returns VBox status code.
 * @param   uLUN                LUN to attach driver to.
 * @param   fAttach             Whether to attach or detach the driver configuration to CFGM.
 *
 * @thread EMT
 */
int AudioDriver::configure(unsigned uLUN, bool fAttach)
{
    int rc = VINF_SUCCESS;

    Console::SafeVMPtrQuiet ptrVM(mpConsole);
    Assert(ptrVM.isOk());

    PUVM pUVM = ptrVM.rawUVM();
    AssertPtr(pUVM);

    PCFGMNODE pRoot   = CFGMR3GetRootU(pUVM);
    AssertPtr(pRoot);
    PCFGMNODE pDev0   = CFGMR3GetChildF(pRoot, "Devices/%s/%u/", mCfg.strDev.c_str(), mCfg.uInst);
    AssertPtr(pDev0);

    PCFGMNODE pDevLun = CFGMR3GetChildF(pDev0, "LUN#%u/", uLUN);

    if (fAttach)
    {
        AssertMsg(uLUN != UINT8_MAX, ("%s: LUN is undefined when it must not\n", mCfg.strName.c_str()));

        if (!pDevLun)
        {
            LogRel2(("%s: Configuring audio driver (to LUN #%RU8)\n", mCfg.strName.c_str(), uLUN));

            PCFGMNODE pLunL0;
            CFGMR3InsertNodeF(pDev0, &pLunL0, "LUN#%RU8", uLUN);
            CFGMR3InsertString(pLunL0, "Driver", "AUDIO");

            PCFGMNODE pLunCfg;
            CFGMR3InsertNode(pLunL0,   "Config", &pLunCfg);
                CFGMR3InsertStringF(pLunCfg, "DriverName",    "%s", mCfg.strName.c_str());
                CFGMR3InsertInteger(pLunCfg, "InputEnabled",  0); /* Play safe by default. */
                CFGMR3InsertInteger(pLunCfg, "OutputEnabled", 1);

            PCFGMNODE pLunL1;
            CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL1);
                CFGMR3InsertStringF(pLunL1, "Driver", "%s", mCfg.strName.c_str());

                CFGMR3InsertNode(pLunL1, "Config", &pLunCfg);

                /* Call the (virtual) method for driver-specific configuration. */
                configureDriver(pLunCfg);
        }
        else
            rc = VERR_ALREADY_EXISTS;
    }
    else /* Detach */
    {
        if (pDevLun)
        {
            LogRel2(("%s: Unconfiguring audio driver\n", mCfg.strName.c_str()));
            CFGMR3RemoveNode(pDevLun);
        }
        else
            rc = VERR_NOT_FOUND;
    }

#ifdef DEBUG_andy
    CFGMR3Dump(pDev0);
#endif

    if (RT_FAILURE(rc))
        LogRel(("%s: %s audio driver failed with rc=%Rrc\n",
                mCfg.strName.c_str(), fAttach ? "Configuring" : "Unconfiguring", rc));

    return rc;
}

