## @file
# innotek Portable Runtime - SED script for converting VBox/err.h to .mac.
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

# Handle text inside the markers.
/SED-START/,/SED-END/{

# if (#define) goto defines
/^[[:space:]]*#[[:space:]]*define/b defines

}

# Everything else is deleted!
d
b end

##
# Convert the defines
:defines
s/^[[:space:]]*#[[:space:]]*define[[:space:]]*\([a-zA-Z0-9_]*\)[[:space:]]*\(.*\)[[:space:]]*$/%define \1    \2/
b end

# next expression
:end
