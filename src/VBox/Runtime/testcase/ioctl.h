/* $Id$ */
/** @file
 * VBox L4/OSS audio - header for Linux IoCtls.
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

#ifndef __VBOX_L4_OSS_IOCTL
#define __VBOX_L4_OSS_IOCTL

#define IOCPARM_MASK   0x3fff          /* parameters must be < 16383 bytes */
#define IOC_VOID       0U << 30      /* no parameters */
#define IOC_IN        1U << 30       /* copy out parameters */
#define IOC_OUT         2U << 30     /* copy in parameters */
#define IOC_INOUT      (IOC_IN|IOC_OUT)
/* the 0x20000000 is so we can distinguish new ioctl's from old */
#define _IO(x,y)       ((int)(IOC_VOID|(x<<8)|y))
#define _IOR(x,y,t)    ((int)(IOC_OUT|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y))
#define _IOW(x,y,t)    ((int)(IOC_IN|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y))
/* this should be _IORW, but stdio got there first */
#define _IOWR(x,y,t)   ((int)(IOC_INOUT|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y))
#define _IOC_SIZE(x)   ((x>>16)&IOCPARM_MASK)
#define _IOC_DIR(x)    (x & 0xf0000000)
#define _IOC_NONE      IOC_VOID
#define _IOC_READ      IOC_OUT
#define _IOC_WRITE     IOC_IN

#endif /* __VBOX_L4_OSS_IOCTL */

