/* $Id$ */
/** @file
 * VBoxManageCloudMachine - The cloud machine related commands.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxManage.h"

#include <VBox/log.h>

#include <VBox/com/ErrorInfo.h>
#include <VBox/com/Guid.h>
#include <VBox/com/errorprint.h>


/*
 * VBoxManage cloud machine ...
 *
 * The "cloud" prefix handling is in VBoxManageCloud.cpp, so this
 * function is not static.
 */
RTEXITCODE handleCloudMachine(HandlerArg *a, int iFirst,
                              const ComPtr<ICloudProfile> &pCloudProfile)
{
    RT_NOREF(a, iFirst, pCloudProfile);

    return RTMsgErrorExit(RTEXITCODE_FAILURE,
               "cloud machine - not yet implemented");
}


/*
 * VBoxManage cloud list machines
 * VBoxManage cloud machine list
 *
 * The "cloud list" prefix handling is in VBoxManageCloud.cpp, so this
 * function is not static.
 */
RTEXITCODE listCloudMachines(HandlerArg *a, int iFirst,
                             const ComPtr<ICloudProfile> &pCloudProfile)
{
    RT_NOREF(a, iFirst, pCloudProfile);

    return RTMsgErrorExit(RTEXITCODE_FAILURE,
               "cloud list machines - not yet implemented");
}
