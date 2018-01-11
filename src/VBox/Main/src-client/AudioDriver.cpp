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

    /* First check if the LUN already exists. */
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
 * Configures the audio driver (to CFGM) and attaches it to the audio chain.
 *
 * @returns IPRT status code. 
 * @param   pThis               Audio driver to detach. 
 * @param   pCfg                Audio driver configuration to use for the audio driver to attach.
 */
/* static */
DECLCALLBACK(int) AudioDriver::Attach(AudioDriver *pThis, AudioDriverCfg *pCfg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    Console::SafeVMPtrQuiet ptrVM(pThis->mpConsole);
    Assert(ptrVM.isOk());

    int vrc = VINF_SUCCESS;

    if (pThis->mfAttached) /* Already attached? Bail out. */
        return VINF_SUCCESS;

    if (pCfg->uLUN == UINT8_MAX) /* No LUN assigned / configured yet? Retrieve it. */
        pCfg->uLUN = pThis->getFreeLUN();

    LogFunc(("strDevice=%s, uInst=%u, uLUN=%u\n", pCfg->strDev.c_str(), pCfg->uInst, pCfg->uLUN));
   
    vrc = pThis->Configure(pCfg, true /* Attach */);
    if (RT_SUCCESS(vrc))
        vrc = PDMR3DriverAttach(ptrVM.rawUVM(), pCfg->strDev.c_str(), pCfg->uInst, pCfg->uLUN, 0 /* fFlags */, NULL /* ppBase */);

    if (RT_SUCCESS(vrc))
    {           
        pThis->mfAttached = true;    
    }
    else                
        LogRel(("VRDE: Failed to attach audio driver, rc=%Rrc\n", vrc));

    return vrc;
}

/**
 * Detaches an already attached audio driver from the audio chain.
 *
 * @returns IPRT status code. 
 * @param   pThis               Audio driver to detach.
 */
/* static */
DECLCALLBACK(int) AudioDriver::Detach(AudioDriver *pThis)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    Console::SafeVMPtrQuiet ptrVM(pThis->mpConsole);
    Assert(ptrVM.isOk());

    if (!pThis->mfAttached) /* Not attached? Bail out. */
        return VINF_SUCCESS;

    int vrc = VINF_SUCCESS;

    AudioDriverCfg *pCfg = &pThis->mCfg;

    LogFunc(("strDevice=%s, uInst=%u, uLUN=%u\n", pCfg->strDev.c_str(), pCfg->uInst, pCfg->uLUN));
   
    vrc = PDMR3DriverDetach(ptrVM.rawUVM(), pCfg->strDev.c_str(), pCfg->uInst, pCfg->uLUN, "AUDIO",
                            0 /* iOccurrence */, 0 /* fFlags */);
    if (RT_SUCCESS(vrc))
        vrc = pThis->Configure(pCfg, false /* Detach */);

    if (RT_SUCCESS(vrc))
    {
        pCfg->uLUN = UINT8_MAX;

        pThis->mfAttached = false;
    }
    else
        LogRel(("VRDE: Failed to detach audio driver, rc=%Rrc\n", vrc));

    return vrc;
}

/**
 * Configures the audio driver via CFGM.
 *
 * @returns VBox status code.
 * @param   strDevice           The PDM device name.
 * @param   uInstance           The PDM device instance.
 * @param   uLUN                The PDM LUN number of the driver.
 * @param   fAttach             Whether to attach or detach the driver configuration to CFGM.
 *
 * @thread EMT
 */
int AudioDriver::Configure(AudioDriverCfg *pCfg, bool fAttach)
{
    if (pCfg->strDev.isEmpty()) /* No audio device configured. Bail out. */
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    Console::SafeVMPtrQuiet ptrVM(mpConsole);
    Assert(ptrVM.isOk());

    /* Apply configuration. */
    mCfg = *pCfg;

    PUVM pUVM = ptrVM.rawUVM();
    AssertPtr(pUVM);

    PCFGMNODE pRoot   = CFGMR3GetRootU(pUVM);
    AssertPtr(pRoot);
    PCFGMNODE pDev0   = CFGMR3GetChildF(pRoot, "Devices/%s/%u/", mCfg.strDev.c_str(), mCfg.uInst);
    AssertPtr(pDev0);

    PCFGMNODE pDevLun = CFGMR3GetChildF(pDev0, "LUN#%u/", mCfg.uLUN);

    if (fAttach)
    {
        if (!pDevLun)
        {         
            LogRel2(("VRDE: Configuring audio driver\n"));

            PCFGMNODE pLunL0;
            CFGMR3InsertNodeF(pDev0, &pLunL0, "LUN#%RU8", mCfg.uLUN);
            CFGMR3InsertString(pLunL0, "Driver", "AUDIO");

            PCFGMNODE pLunCfg;
            CFGMR3InsertNode(pLunL0,   "Config", &pLunCfg);
                CFGMR3InsertString (pLunCfg, "DriverName",    "AudioVRDE");
                CFGMR3InsertInteger(pLunCfg, "InputEnabled",  0); /* Play safe by default. */
                CFGMR3InsertInteger(pLunCfg, "OutputEnabled", 1);

            PCFGMNODE pLunL1;
            CFGMR3InsertNode(pLunL0, "AttachedDriver", &pLunL1);
                CFGMR3InsertString(pLunL1, "Driver", "AudioVRDE");

                CFGMR3InsertNode(pLunL1, "Config", &pLunCfg);

                /* Call the (virtual) method for driver-specific configuration. */
                configureDriver(pLunCfg);
        }
    }
    else /* Detach */
    {
        if (pDevLun)
        {
            LogRel2(("VRDE: Unconfiguring audio driver\n"));
            CFGMR3RemoveNode(pDevLun);
        }
    }

    AssertRC(rc);
    return rc;
}

