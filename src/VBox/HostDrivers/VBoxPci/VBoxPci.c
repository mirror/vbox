/* $Id $ */
/** @file
 * VBoxPci - PCI card passthrough support (Host), Common Code.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_rawpci     VBoxPci - host PCI support
 *
 * This is a kernel module that works as host proxy between guest and 
 * PCI hardware.
 *
 */

#define LOG_GROUP LOG_GROUP_PCI_RAW
#include "VBoxPciInternal.h"

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/string.h>

#include <VBox/sup.h>
#include <iprt/assert.h>
#include <iprt/spinlock.h>
#include <iprt/uuid.h>
#include <VBox/version.h>

