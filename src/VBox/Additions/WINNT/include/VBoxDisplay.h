/** @file
 *
 * VBoxDisplay - private windows additions display header
 *
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
 *
 */
#ifndef __VBoxDisplay_h__
#define __VBoxDisplay_h__

#define VBOXESC_SETVISIBLEREGION            0xABCD9001


#define IOCTL_VIDEO_VBOX_SETVISIBLEREGION \
    CTL_CODE(FILE_DEVICE_VIDEO, 0xA01, METHOD_BUFFERED, FILE_ANY_ACCESS)


#endif /* __VBoxDisplay_h__ */
