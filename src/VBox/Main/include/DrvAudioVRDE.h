/* $Id$ */
/** @file
 * VirtualBox driver interface to VRDE backend.
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_DrvAudioVRDE_h
#define MAIN_INCLUDED_DrvAudioVRDE_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/com/ptr.h>
#include <VBox/com/string.h>

#include <VBox/RemoteDesktop/VRDE.h>

#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmifs.h>

#include "AudioDriver.h"

using namespace com;

class Console;

class AudioVRDE : public AudioDriver
{

public:

    AudioVRDE(Console *pConsole);
    virtual ~AudioVRDE(void);

public:

    static const PDMDRVREG DrvReg;

public:

    void onVRDEClientConnect(uint32_t uClientID);
    void onVRDEClientDisconnect(uint32_t uClientID);
    int onVRDEControl(bool fEnable, uint32_t uFlags);
    int onVRDEInputBegin(void *pvContext, PVRDEAUDIOINBEGIN pVRDEAudioBegin);
    int onVRDEInputData(void *pvContext, const void *pvData, uint32_t cbData);
    int onVRDEInputEnd(void *pvContext);
    int onVRDEInputIntercept(bool fIntercept);

public:

    static DECLCALLBACK(int) drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(void) drvDestruct(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(int) drvAttach(PPDMDRVINS pDrvIns, uint32_t fFlags);
    static DECLCALLBACK(void) drvDetach(PPDMDRVINS pDrvIns, uint32_t fFlags);

private:

    int configureDriver(PCFGMNODE pLunCfg);

    /** Pointer to the associated VRDE audio driver. */
    struct DRVAUDIOVRDE *mpDrv;
};

#endif /* !MAIN_INCLUDED_DrvAudioVRDE_h */

