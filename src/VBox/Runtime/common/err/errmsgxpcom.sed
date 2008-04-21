# $Id$
## @file
# IPRT - SED script for converting XPCOM errors
#

#
# Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#

# no comments
/\*/b skip
# we want NS_ERROR_* defines, but not all!
/\#define NS_ERROR_SEVERITY_/b skip
/\#define NS_ERROR_BASE/b skip
/\#define NS_ERROR_MODULE_/b skip
/\#define NS_ERROR_GET_/b skip
/\#define NS_ERROR_GENERATE/b skip
/\#define NS_ERROR_/b nserror
d
b end

:skip
# Everything else is deleted!
d
b end


#
# A good error definition
#
:nserror
{
    # remove DOS <CR>.
    s/\r//g
    # remove '#define'
    s/\#define //
    # output C array entry
    s/\([a-zA-Z0-9_]*\)[\t ]*\(.*\)[\t ]*$/{ "\1", \1 }, /
}
b end

# next expression
:end
