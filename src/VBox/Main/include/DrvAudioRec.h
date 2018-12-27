/* $Id$ */
/** @file
 * VirtualBox driver interface video recording audio backend.
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_DRVAUDIORECORDING
#define ____H_DRVAUDIORECORDING
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/com/ptr.h>
#include <VBox/settings.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmifs.h>

#include "AudioDriver.h"
#include "Recording.h"

using namespace com;

class Console;

class AudioVideoRec : public AudioDriver
{

public:

    AudioVideoRec(Console *pConsole);
    virtual ~AudioVideoRec(void);

public:

    static const PDMDRVREG DrvReg;

public:

    int applyConfiguration(const settings::RecordingSettings &Settings);

public:

    static DECLCALLBACK(int)  drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(void) drvDestruct(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(int)  drvAttach(PPDMDRVINS pDrvIns, uint32_t fFlags);
    static DECLCALLBACK(void) drvDetach(PPDMDRVINS pDrvIns, uint32_t fFlags);

private:

    int configureDriver(PCFGMNODE pLunCfg);

    /** Pointer to the associated video recording audio driver. */
    struct DRVAUDIORECORDING         *mpDrv;
    /** Capturing configuration used for configuring the driver. */
    struct settings::RecordingSettings mVideoRecCfg;
};

#endif /* !____H_DRVAUDIORECORDING */

