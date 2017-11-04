/* $Id$ */
/** @file
 * MsrSup - SupDrv-specific MSR access.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
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
#include <iprt/ctype.h>
#include <iprt/x86.h>

#include <VBox/sup.h>
#include "VBoxCpuReport.h"


/* Wrappers to mask funny SUPR3 calling conventions on some platforms. */
static int supMsrProberRead(uint32_t uMsr, RTCPUID idCpu, uint64_t *puValue, bool *pfGp)
{
    return SUPR3MsrProberRead(uMsr, idCpu, puValue, pfGp);
}

static int supMsrProberWrite(uint32_t uMsr, RTCPUID idCpu, uint64_t uValue, bool *pfGp)
{
    return SUPR3MsrProberWrite(uMsr, idCpu, uValue, pfGp);
}

static int supMsrProberModify(uint32_t uMsr, RTCPUID idCpu, uint64_t fAndMask, uint64_t fOrMask, PSUPMSRPROBERMODIFYRESULT pResult)
{
    return SUPR3MsrProberModify(uMsr, idCpu, fAndMask, fOrMask, pResult);
}

static int supMsrProberTerm(void)
{
    return VINF_SUCCESS;
}

int SupDrvMsrProberInit(VBMSRFNS *fnsMsr)
{
    int rc = SUPR3Init(NULL);
    if (RT_FAILURE(rc))
    {
        vbCpuRepDebug("warning: Unable to initialize the support library (%Rrc).\n", rc);
        return VERR_NOT_FOUND;
    }

    /* Test if the MSR prober is available, since the interface is optional. The TSC MSR will exist on any supported CPU. */
    uint64_t uValue;
    bool     fGp;
    rc = SUPR3MsrProberRead(MSR_IA32_TSC, NIL_RTCPUID, &uValue, &fGp);
    if (RT_FAILURE(rc))
    {
        vbCpuRepDebug("warning: MSR probing not supported by the support driver (%Rrc).\n", rc);
        return VERR_NOT_SUPPORTED;
    }

    fnsMsr->msrRead       = supMsrProberRead;
    fnsMsr->msrWrite      = supMsrProberWrite;
    fnsMsr->msrModify     = supMsrProberModify;
    fnsMsr->msrProberTerm = supMsrProberTerm;

    return VINF_SUCCESS;
}
