/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Windows Specific Code. Protocol edge of ndis filter driver
 */

/*
 * Copyright (C) 2008 Oracle Corporation
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
 * Based in part on Microsoft DDK sample code for Ndis Intermediate Miniport passthru driver sample.
 * Copyright (c) 1993-1999, Microsoft Corporation
 */

#ifndef ___VBoxNetFltPt_win_h___
#define ___VBoxNetFltPt_win_h___

#ifdef VBOXNETADP
# error "No protocol edge"
#endif
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtRegister(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDeregister();
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDoUnbinding(PADAPT pAdapt, bool bOnUnbind);
DECLHIDDEN(VOID) vboxNetFltWinPtFlushReceiveQueue(IN PADAPT pAdapt, IN bool bReturn);
DECLHIDDEN(VOID) vboxNetFltWinPtRequestComplete(IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_REQUEST NdisRequest, IN NDIS_STATUS Status);
DECLHIDDEN(bool) vboxNetFltWinPtCloseAdapter(PADAPT pAdapt, PNDIS_STATUS pStatus);

#ifdef VBOX_NETFLT_ONDEMAND_BIND
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDoBinding(IN PADAPT pAdapt);
#else
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDoBinding(IN PADAPT pAdapt, IN PNDIS_STRING pOurDeviceName, IN PNDIS_STRING pBindToDeviceName);
DECLHIDDEN(NDIS_HANDLE) vboxNetFltWinPtGetHandle();
#endif

#endif
