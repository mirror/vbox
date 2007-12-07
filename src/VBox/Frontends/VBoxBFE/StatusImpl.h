/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of VMStatus class
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

#ifndef ____H_STATUSIMPL
#define ____H_STATUSIMPL

#include <VBox/pdm.h>

class VMStatus
{
public:

    static const PDMDRVREG  DrvReg;

private:

    static DECLCALLBACK(void)   drvUnitChanged(PPDMILEDCONNECTORS pInterface, unsigned iLUN);
    static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);
    static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
    static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);
};

extern VMStatus *gStatus;

#endif // ____H_STATUSIMPL
