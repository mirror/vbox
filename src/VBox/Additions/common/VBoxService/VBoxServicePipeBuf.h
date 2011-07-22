/* $Id$ */
/** @file
 * VBoxServicePipeBuf - Pipe buffering.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxServicePipeBuf_h
#define ___VBoxServicePipeBuf_h

#include "VBoxServiceInternal.h"

int  VBoxServicePipeBufInit(PVBOXSERVICECTRLEXECPIPEBUF pBuf, uint8_t uId, bool fNeedNotificationPipe);
int  VBoxServicePipeBufRead(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                            uint8_t *pbBuffer, uint32_t cbBuffer, uint32_t *pcbToRead);
int VBoxServicePipeBufPeek(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                           uint8_t *pbBuffer, uint32_t cbBuffer,
                           uint32_t cbOffset,
                           uint32_t *pcbRead, uint32_t *pcbLeft);
int  VBoxServicePipeBufWriteToBuf(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                                  uint8_t *pbData, uint32_t cbData, bool fPendingClose, uint32_t *pcbWritten);
int  VBoxServicePipeBufWriteToPipe(PVBOXSERVICECTRLEXECPIPEBUF pBuf, RTPIPE hPipe,
                                   size_t *pcbWritten, size_t *pcbLeft);
bool VBoxServicePipeBufIsEnabled(PVBOXSERVICECTRLEXECPIPEBUF pBuf);
bool VBoxServicePipeBufIsClosing(PVBOXSERVICECTRLEXECPIPEBUF pBuf);
int  VBoxServicePipeBufSetPID(PVBOXSERVICECTRLEXECPIPEBUF pBuf, uint32_t uPID);
int  VBoxServicePipeBufSetStatus(PVBOXSERVICECTRLEXECPIPEBUF pBuf, bool fEnabled);
int  VBoxServicePipeBufWaitForEvent(PVBOXSERVICECTRLEXECPIPEBUF pBuf, RTMSINTERVAL cMillies);
void VBoxServicePipeBufDestroy(PVBOXSERVICECTRLEXECPIPEBUF pBuf);

#endif  /* !___VBoxServicePipeBuf_h */

