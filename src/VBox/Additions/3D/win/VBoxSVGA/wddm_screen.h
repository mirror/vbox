/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - VMSVGA hardware driver.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBoxGaDriver.h>

#include "vmw_screen.h"

struct vmw_winsys_screen_wddm
{
    struct vmw_winsys_screen base;

    const WDDMGalliumDriverEnv *pEnv;
    VBOXGAHWINFOSVGA HwInfo;
};
