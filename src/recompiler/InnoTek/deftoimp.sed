# $Id: deftoimp.sed 17147 2007-01-09 09:05:45Z bird $
## @file
# VBox Runtime - SED script for windows .def file stubs file.
#

#
#
# Copyright (C) 2006 InnoTek Systemberatung GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License as published by the Free Software Foundation,
# in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
# distribution. VirtualBox OSE is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY of any kind.
#
# If you received this file as part of a commercial VirtualBox
# distribution, then only the terms of your commercial VirtualBox
# license agreement apply instead of the previous paragraph.
#

s/;.*$//g
s/^[[:space:]][[:space:]]*//g
s/[[:space:]][[:space:]]*$//g
/^$/d

# Handle text after EXPORTS
/EXPORTS/,//{
s/^EXPORTS$//
/^$/b end

s/^\(.*\)$/EXPORT\nvoid \1(void);\nvoid \1(void){}/
b end
}
d
b end


# next expression
:end

