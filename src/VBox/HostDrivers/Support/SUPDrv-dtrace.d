/* $Id$ */
/** @file
 * SUPDrv - Static dtrace probes
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

provider vboxdrv
{
    probe supdrv__session__create(void *pvSession, int fUser);
    probe supdrv__session__close(void *pvSession);
    probe supdrv__ioctl__entry(void *pvSession, unsigned int uIOCtl, void *pvReqHdr);
    probe supdrv__ioctl__return(void *pvSession, unsigned int uIOCtl, void *pvReqHdr, int rc, int rcReq);
};

#pragma D attributes Evolving/Evolving/Common provider vboxdrv provider
#pragma D attributes Private/Private/Unknown  provider vboxdrv module
#pragma D attributes Private/Private/Unknown  provider vboxdrv function
#pragma D attributes Evolving/Evolving/Common provider vboxdrv name
#pragma D attributes Evolving/Evolving/Common provider vboxdrv args



