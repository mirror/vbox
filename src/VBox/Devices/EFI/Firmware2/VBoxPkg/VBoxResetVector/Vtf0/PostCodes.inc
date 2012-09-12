; $Id$
;; @file


;
; Copyright (C) 2012 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

;------------------------------------------------------------------------------
; @file
; Definitions of POST CODES for the reset vector module
;
; Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

%define POSTCODE_16BIT_MODE     0x16
%define POSTCODE_32BIT_MODE     0x32
%define POSTCODE_64BIT_MODE     0x64

%define POSTCODE_BFV_NOT_FOUND  0xb0
%define POSTCODE_BFV_FOUND      0xb1

%define POSTCODE_SEC_NOT_FOUND  0xf0
%define POSTCODE_SEC_FOUND      0xf1

