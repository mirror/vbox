/** @file
 *
 * VirtualBox Driver interface to Audio Sniffer device
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

#ifndef ____H_AUDIOSNIFFERINTERFACE
#define ____H_AUDIOSNIFFERINTERFACE

#include <VBox/com/ptr.h>
#include <VBox/pdmdrv.h>

class Console;

class AudioSniffer
{
public:
    AudioSniffer(Console *console);
    virtual ~AudioSniffer();

    static const PDMDRVREG DrvReg;

    /** Pointer to the associated Audio Sniffer driver. */
    struct DRVAUDIOSNIFFER *mpDrv;

    Console *getParent(void) { return mParent; }

    PPDMIAUDIOSNIFFERPORT getAudioSnifferPort(void);

private:
    static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);
    static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
    static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);

    ComObjPtr <Console, ComWeakRef> mParent;
};

#endif /* ____H_AUDIOSNIFFERINTERFACE */
