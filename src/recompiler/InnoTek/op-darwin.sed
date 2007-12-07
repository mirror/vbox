# $Id$
## @file
#
# SED script for transforming op.S (i386 ELF from GNU/linux) into
# something that the Darwin (Mac OS X) assembler can make use of.
#

#
# Copyright (C) 2006-2007 innotek GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# This is a generic transformation, only encountered with i386-elf-gcc-3.4.3 so far.
s/^\([[:blank:]]*\)\//\1#/

# Darwin doesn't grok .type and .size, remove those lines.
/^[[:blank:]]*\.type[[:blank:]]/d
/^[[:blank:]]*\.size[[:blank:]]/d

# Darwin names the .rodata section '.const'.
s/^[[:blank:]]*\.section[[:blank:]][[:blank:]]*\.rodata[[:blank:]]*$/\t.const/

# Darwin doesn't grok the the .note.GNU-stack section, remove that line.
/^[[:blank:]]*\.section[[:blank:]][[:blank:]]*\.note\.GNU-stack,/d

# .zero seems to be similar to .spaces...
s/^\([[:blank:]]*\)\.zero[[:blank:]][[:blank:]]*\([0-9][0-9]*\)/\1.space \2/

# It looks like if .align is taking a byte count on linux and a power of 
# two on Darwin, translate to power of two.
s/\.align 128/\.align 7/
s/\.align 64/\.align 6/
s/\.align 32/\.align 5/
s/\.align 16/\.align 4/
s/\.align 8/\.align 3/
s/\.align 4/\.align 2/
s/\.align 2/\.align 1/


# Darwin uses underscore prefixed names like the DOS based i386 OSes
# linux does. So, all global symbols needs to be translated.
s/^[[:blank:]]*\.globl[[:blank:]][[:blank:]]*\([^\t\n ]*\)[[:blank:]]*$/#define \1 _\1\n.globl \1/

# special hack for __op_labelN
s/__op_label\([0-9]\)/___op_label\1/g

