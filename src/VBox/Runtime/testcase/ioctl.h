/* $Id$ */
/** @file
 * VBox L4/OSS audio - header for Linux IoCtls.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBOX_L4_OSS_IOCTL
#define ___VBOX_L4_OSS_IOCTL

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

#endif /* !___VBOX_L4_OSS_IOCTL */

