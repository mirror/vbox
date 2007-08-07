# $Id$
## @file
# innotek Portable Runtime - SED script for converting COM errors
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

# we only care about message definitions
\/\/ MessageId: /b messageid
d
b end


# Everything else is deleted!
d
b end


#
# A message ID we care about
#
:messageid
# concatenate the next four lines to the string
N
N
N
N
{
    # remove DOS <CR>.
    s/\r//g
    # remove the message ID
    s/\/\/ MessageId: //g
    # remove the stuff in between
    s/\/\/\n\/\/ MessageText:\n\/\/\n\/\/  //g
    # backslashes have to be escaped
    s/\\/\\\\/g
    # double quotes have to be escaped, too
    s/"/\\"/g
    # output C array entry
    s/\([a-zA-Z0-9_]*\)[\t ]*\n\(.*\)[\t ]*$/{ "\2", "\1", \1 }, /
}
b end

# next expression
:end
