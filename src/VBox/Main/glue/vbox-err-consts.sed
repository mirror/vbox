# $Id$
## @file
# IPRT - SED script for converting */err.h to a python dictionary.
#

#
# Copyright (C) 2006-2019 Oracle Corporation
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

# Header and footer text:
1i <text>
$a </text>

# Handle text inside the markers.
/SED-START/,/SED-END/{

# if (#define) goto defines
/^[[:space:]]*#[[:space:]]*define/b defines

}

# Everything else is deleted!
:delete
d
b end


##
# Convert the defines
:defines

/^[[:space:]]*#[[:space:]]*define[[:space:]]*\([[:alnum:]_]*\)[[:space:](]*\([-+]*[[:space:]]*[[:digit:]][[:digit:]]*\)[[:space:])]*$/b define_okay
b delete
:define_okay
s/^[[:space:]]*#[[:space:]]*define[[:space:]]*\([[:alnum:]_]*\)[[:space:](]*\([-+]*[[:space:]]*[[:digit:]][[:digit:]]*\)[[:space:])]*$/        '\1': \2,/

b end


# next expression
:end
