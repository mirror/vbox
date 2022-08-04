# $Id$
## @file
# SED script for generating input for genalias from C/C++ and ASM sources.
#

#
# Copyright (C) 2022 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# The contents of this file may alternatively be used under the terms
# of the Common Development and Distribution License Version 1.0
# (CDDL) only, as it comes in the "COPYING.CDDL" file of the
# VirtualBox OSE distribution, in which case the provisions of the
# CDDL are applicable instead of those of the GPL.
#
# You may elect to license modified versions of this file under the
# terms and conditions of either the GPL or the CDDL or both.
#

s/^ *RT_NOCRT_BEGINPROC  *([a-zA-Z_][^ ;]*)([ ;].*|)$/\1=nocrt_\1 _\1=nocrt_\1 \\/p
s/^ *RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL\(([^;)]+)\);.*$/\1=nocrt_\1 _\1=nocrt_\1 \\/p
s/^ *RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL_WITHOUT_UNDERSCORE\(([^;)]+)\);.*$/\1=nocrt_\1 \\/p
