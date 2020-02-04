; $Id$
;; @file
; DevEFI - firmware binaries.
;

;
; Copyright (C) 2011-2020 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;


;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "iprt/x86.mac"


BEGINCONST
EXPORTEDNAME g_abEfiFirmware32
        incbin "VBoxEFI32.fd"
end_32_firmware:
EXPORTEDNAME g_cbEfiFirmware32
        dd  end_32_firmware - NAME(g_abEfiFirmware32)

ALIGNDATA(64)
EXPORTEDNAME g_abEfiFirmware64
        incbin "VBoxEFI64.fd"
end_64_firmware:
EXPORTEDNAME g_cbEfiFirmware64
        dd  end_64_firmware - NAME(g_abEfiFirmware64)

%ifdef ASM_FORMAT_ELF
size g_abEfiFirmware32 end_32_firmware - NAME(g_abEfiFirmware32)
type g_abEfiFirmware32 object
size g_cbEfiFirmware32 4
type g_cbEfiFirmware32 object

size g_abEfiFirmware64 end_64_firmware - NAME(g_abEfiFirmware64)
type g_abEfiFirmware64 object
size g_cbEfiFirmware64 4
type g_cbEfiFirmware64 object
%endif
