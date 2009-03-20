# $Id$
## @file
# IPRT - SED script for converting */err.h.
#

# Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

# Handle text inside the markers.
/SED-START/,/SED-END/{

# if (#define) goto defines
/^[[:space:]]*#[[:space:]]*define/b defines

# if (/**) goto description
/\/\*\*/b description

}

# Everything else is deleted!
d
b end


##
# Convert the defines
:defines
s/^[[:space:]]*#[[:space:]]*define[[:space:]]*\([[:alnum:]_]*\)[[:space:]]*\(.*\)[[:space:]]*$/    "\1",\n     \1 }, /
b end

##
# Convert descriptive comments. /** desc */
:description
# arg! how to do N until end of comment?
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
/\*\//!N
# anything with @{ and @} is skipped
/@[\{\}]/d

# Fix double spaces
s/[[:space:]][[:space:]]/ /g

# Fix \# sequences (doxygen needs them, we don't).
s/\\#/#/g

# insert punctuation.
s/\([^.[:space:]]\)[[:space:]]*\*\//\1. \*\//

# convert /** short. more
s/[[:space:]]*\/\*\*[[:space:]]*/  { NULL, \"/
s/  { NULL, \"\([^.!?"]*[.!?][.!?]*\)/  { \"\1\",\n    \"\1/

# terminate the string
s/[[:space:]]*\*\//\"\,/
s/[[:space:]]*[[:space:]]\*[[:space:]][[:space:]]*/ /g
b end


# next expression
:end
