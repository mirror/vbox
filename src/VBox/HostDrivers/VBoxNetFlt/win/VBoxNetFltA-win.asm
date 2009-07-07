; $Id$
;; @file
; VBoxNetFlt - Unwind Wrappers for 64-bit NT.
;

;
; Copyright (C) 2008 Sun Microsystems, Inc.
;
; Sun Microsystems, Inc. confidential
; All rights reserved
;

%include "iprt/ntwrap.mac"

NtWrapDyn2DrvFunctionWithAllRegParams netfltNtWrap, vboxNetFltPortRetain
NtWrapDyn2DrvFunctionWithAllRegParams netfltNtWrap, vboxNetFltPortRelease
NtWrapDyn2DrvFunctionWithAllRegParams netfltNtWrap, vboxNetFltPortDisconnectAndRelease
NtWrapDyn2DrvFunctionWithAllRegParams netfltNtWrap, vboxNetFltPortSetActive
NtWrapDyn2DrvFunctionWithAllRegParams netfltNtWrap, vboxNetFltPortWaitForIdle
NtWrapDyn2DrvFunctionWithAllRegParams netfltNtWrap, vboxNetFltPortGetMacAddress
NtWrapDyn2DrvFunctionWithAllRegParams netfltNtWrap, vboxNetFltPortIsHostMac
NtWrapDyn2DrvFunctionWithAllRegParams netfltNtWrap, vboxNetFltPortIsPromiscuous
NtWrapDyn2DrvFunctionWithAllRegParams netfltNtWrap, vboxNetFltPortXmit

