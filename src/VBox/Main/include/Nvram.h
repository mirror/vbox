/* $Id$ */

/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_Nvram_h
#define MAIN_INCLUDED_Nvram_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/com/ptr.h>
#include <VBox/vmm/pdmdrv.h>

class Console;
struct NVRAM;

class Nvram
{
public:
    Nvram(Console *console);
    virtual ~Nvram();

    Console *getParent(void) { return mParent; }

    static const PDMDRVREG DrvReg;

private:
    static DECLCALLBACK(void *) drvNvram_QueryInterface(PPDMIBASE pInterface, const char *pszIID);
    static DECLCALLBACK(int)    drvNvram_Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(void)   drvNvram_Destruct(PPDMDRVINS pDrvIns);

    /** Pointer to the parent object. */
    Console * const mParent;
    /** Pointer to the driver instance data.
     *  Can be NULL during init and termination. */
    struct NVRAM *mpDrv;
};

#endif /* !MAIN_INCLUDED_Nvram_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
