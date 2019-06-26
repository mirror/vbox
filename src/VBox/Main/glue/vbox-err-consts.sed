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
