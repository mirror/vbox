# $Id$
## @file
# innotek Portable Runtime - SED script for converting */err.h.
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
s/^[[:space:]]*#[[:space:]]*define[[:space:]]*\([a-zA-Z0-9_]*\)[[:space:]]*\(.*\)[[:space:]]*$/    "\1",\n     \1 }, /
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
