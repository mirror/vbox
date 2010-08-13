/*
 * VirtualBox Guest Additions installer test module for Linux
 *
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
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
