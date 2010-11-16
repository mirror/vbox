/* $Id$ */
/** @file
 * NAT - debug helpers (declarations/defines).
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <VBox/log.h>
/* we've excluded stdio.h */
#define FILE void

int debug_init (void);
void ipstats (PNATState);
void tcpstats (PNATState);
void udpstats (PNATState);
void icmpstats (PNATState);
void mbufstats (PNATState);
void sockstats (PNATState);

#endif
