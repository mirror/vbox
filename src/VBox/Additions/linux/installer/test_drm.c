/*
 * VirtualBox Guest Additions installer test module for Linux
 *
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <linux/init.h>
#include <linux/module.h>
#include "drm/drmP.h"


MODULE_DESCRIPTION("VirtualBox Guest Additions drm test module");
MODULE_AUTHOR("Oracle Corporation");
MODULE_LICENSE("GPL");

static int init(void)
{
    return 0;
}

static void fini(void) {}

module_init(init);
module_exit(fini);
