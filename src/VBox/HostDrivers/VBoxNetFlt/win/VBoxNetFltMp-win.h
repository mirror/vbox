/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Windows Specific Code. Miniport edge of ndis filter driver
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */
/*
 * Based in part on Microsoft DDK sample code for Ndis Intermediate Miniport passthru driver sample.
 * Copyright (c) 1993-1999, Microsoft Corporation
 */

#ifndef ___VBoxNetFltMp_win_h___
#define ___VBoxNetFltMp_win_h___

 #ifdef VBOX_NETFLT_ONDEMAND_BIND
  # error "unsupported (VBOX_NETFLT_ONDEMAND_BIND)"
 #else

DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpRegister(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpDeregister();
DECLHIDDEN(NDIS_HANDLE) vboxNetFltWinMpGetHandle();
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpInitializeDevideInstance(PADAPT pAdapt);
DECLHIDDEN(VOID) vboxNetFltWinMpReturnPacket(IN NDIS_HANDLE MiniportAdapterContext, IN PNDIS_PACKET Packet);
DECLHIDDEN(bool) vboxNetFltWinMpDeInitializeDevideInstance(PADAPT pAdapt, PNDIS_STATUS pStatus);
DECLHIDDEN(VOID) vboxNetFltWinMpQueryPNPCapabilities(IN OUT PADAPT pAdapt, OUT PNDIS_STATUS pStatus);
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpReferenceControlDevice();
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpDereferenceControlDevice();

  #ifdef VBOXNETADP
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpDoInitialization(PADAPT pAdapt, NDIS_HANDLE hMiniportAdapter, NDIS_HANDLE hWrapperConfigurationContext);
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinMpDoDeinitialization(PADAPT pAdapt);
  #endif

 #endif

#endif
