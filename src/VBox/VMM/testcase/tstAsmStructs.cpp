/* $Id$ */
/** @file
 * Testcase for checking offsets in the assembly structures shared with C/C++.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/cpum.h>
#include "CPUMInternal.h"
#include <VBox/trpm.h>
#include "TRPMInternal.h"
#include "../VMMSwitcher/VMMSwitcher.h"
#include "VMMInternal.h"
#include <VBox/vm.h>

#include "tstHelp.h"
#include <stdio.h>


int main()
{
    int rc = 0;
    printf("tstAsmStructs: TESTING\n");

#ifdef IN_RING3
# include "tstAsmStructsHC.h"
#else
# include "tstAsmStructsGC.h"
#endif

    if (rc)
        printf("tstAsmStructs: FAILURE - %d errors \n", rc);
    else
        printf("tstAsmStructs: SUCCESS\n");
    return rc;
}
