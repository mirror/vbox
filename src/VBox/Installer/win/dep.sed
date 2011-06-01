# $Id$
## @file
# Generate dependencies from .wxs and .wxi sources.
#

#
# Copyright (C) 2006-2011 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# drop all lines not including a src property.
/Source=\"/!d
# extract the file spec
s/^.*Source="\([^"]*\).*$/\1 /
# convert to unix slashes
s/\\/\//g
# $(env.PATH_OUT stuff.
s/(env\./(/g
# pretty
s/$/\\/
s/^/\t/

