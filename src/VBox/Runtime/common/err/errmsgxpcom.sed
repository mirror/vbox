# $Id$
## @file
# innotek Portable Runtime - SED script for converting XPCOM errors
#

#
#  Copyright (C) 2006-2007 innotek GmbH
# 
#  This file is part of VirtualBox Open Source Edition (OSE), as
#  available from http://www.virtualbox.org. This file is free software;
#  you can redistribute it and/or modify it under the terms of the GNU
#  General Public License as published by the Free Software Foundation,
#  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
#  distribution. VirtualBox OSE is distributed in the hope that it will
#  be useful, but WITHOUT ANY WARRANTY of any kind.

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
