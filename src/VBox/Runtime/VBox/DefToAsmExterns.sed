# $Id$
## @file
# SED script for generating assembly externs from a VBoxRT windows .def file.
#

#
# Copyright (C) 2006-2018 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

#
# Remove uncertain exports.
#
/not-some-systems/d

#
# Remove comments and space. Skip empty lines.
#
s/;.*$//g
s/^[[:space:]][[:space:]]*//g
s/[[:space:]][[:space:]]*$//g
/^$/d

# Handle text after EXPORTS
/EXPORTS/,//{
s/^EXPORTS$//
/^$/b end


/[[:space:]]DATA$/b data

#
# Function export
#
:code
s/^\(.*\)$/EXTERN_IMP2 \1/
b end


#
# Data export
#
:data
s/^\(.*\)[[:space:]]*DATA$/EXTERN_IMP2 \1/
b end

}
d
b end


# next expression
:end

