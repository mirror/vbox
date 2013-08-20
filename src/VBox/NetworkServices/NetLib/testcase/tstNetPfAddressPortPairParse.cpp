/* $Id$ */
/** @file
 * NetLib - test for Port Fort Forward String
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/string.h>

#include <iprt/test.h>

#include "../VBoxPortForwardString.h"


int main()
{
    PORTFORWARDRULE PortForwardRule;
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstNetPfAddressPortPairParse", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    /* XXX: It's used mostly for debugging but it'd be better to convert it in real test. */

    netPfStrToPf(":tcp:[]:20022:[10.0.2.15]:22", 0, &PortForwardRule);
    netPfStrToPf("test_tcp:tcp:[]:20022:[10.0.2.15]:22", 0, &PortForwardRule);  
    netPfStrToPf("test_with_no_proto::[]:20022:[10.0.2.15]:22", 0, &PortForwardRule);
    netPfStrToPf("test::[]12:20022:[10.0.2.15]:22", 0, &PortForwardRule);    
    netPfStrToPf("test:3123:[]:20022:[10.0.2.15]:22", 0, &PortForwardRule);
    netPfStrToPf("test::[]10.0.2.2:20022:[10.0.2.15]:22", 0, &PortForwardRule);
    netPfStrToPf("test::[]:aa20022:123[10.0.2.15]:22", 0, &PortForwardRule);

    return RTTestSummaryAndDestroy(hTest);
}
