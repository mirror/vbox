/** @file
 * Version management
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef __VBox_version_h__
#define __VBox_version_h__

#include <version-generated.h>

/** Combined version number. */
#define VBOX_VERSION            (VBOX_VERSION_MAJOR << 16 | VBOX_VERSION_MINOR)
/** Get minor version from combined version */
#define VBOX_GET_VERSION_MINOR(uVer)    (uVer & 0xffff)
/** Get major version from combined version. */
#define VBOX_GET_VERSION_MAJOR(uVer)    (uVer >> 16)


#endif

