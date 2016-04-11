# $Id$
## @file
# For converting biosorg_check_<addr> lines in a wlink mapfile
# to kmk_expr checks.
#

#
# Copyright (C) 2012-2016 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#


/biosorg_check_/!b end  
s/\(.*\)/\L\1/g
s/f000:\(....\). *biosorg_check_0\(\1\)h *//
/^$/b end
p
q 1
:end

