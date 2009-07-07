; $Id$
;; @file
; VBoxNetFlt - Unwind Wrappers for 64-bit NT.
;

;
; Copyright (C) 2008 Sun Microsystems, Inc.
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
; Clara, CA 95054 USA or visit http://www.sun.com if you need
; additional information or have any questions.
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

