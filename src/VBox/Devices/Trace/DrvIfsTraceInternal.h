/* $Id$ */
/** @file
 * VBox interface callback tracing driver - internal header.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Trace_DrvIfsTraceInternal_h
#define VBOX_INCLUDED_SRC_Trace_DrvIfsTraceInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmserialifs.h>

#include <iprt/types.h>


/**
 * Interface Tracing Driver Instance Data.
 */
typedef struct DRVIFTRACE
{
    /** @name Interfaces exposed by this driver.
     * @{ */
    PDMIBASE             IBase;
    PDMISERIALPORT       ISerialPort;
    PDMISERIALCONNECTOR  ISerialConnector;
    /** @}  */

    /** @name Interfaces exposed by the driver below us.
     * @{ */
    PPDMISERIALCONNECTOR pISerialConBelow;
    /** @} */

    /** @name Interfaces exposed by the driver/device above us.
     * @{ */
    PPDMISERIALPORT      pISerialPortAbove;
    /** @} */

    /** PDM device driver instance pointer. */
    PPDMDRVINS           pDrvIns;
    /** The trace log writer handle. */
    RTTRACELOGWR         hTraceLog;
    /** Path of the trace log file. */
    char                 *pszTraceFilePath;

} DRVIFTRACE;
/** Pointer to a interface tracing driver instance. */
typedef DRVIFTRACE *PDRVIFTRACE;


DECLHIDDEN(void) drvIfsTrace_SerialIfInit(PDRVIFTRACE pThis);

#endif /* !VBOX_INCLUDED_SRC_Trace_DrvIfsTraceInternal_h */
