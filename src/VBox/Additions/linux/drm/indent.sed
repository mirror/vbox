# Oracle VM VirtualBox
# VirtualBox to Linux kernel coding style conversion script.

#
# Copyright (C) 2017 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

# This script is for converting code inside the vboxvideo module to Linux
# kernel coding style.  It assumes correct VirtualBox coding style, will break
# break if the coding style is wrong (e.g. tab instead of spaces) and is not
# indended to be a generic solution: for example, identifiers will be
# translated case by case, not algorithmically.  It also assumes that any
# flexibility in either coding style will be used where possible to make the
# code conform to both at once.

# Replace up to six leading groups of four spaces with tabs.
s/^                        /\t\t\t\t\t\t/g
s/^                    /\t\t\t\t\t/g
s/^                /\t\t\t\t/g
s/^            /\t\t\t/g
s/^        /\t\t/g
s/^    /\t/g
# Remove any spaces left after the tabs.  This also limits maximum indentation.
s/^\(\t\t*\) */\1/g
# And move braces.  This must be the last expression as it jumps to the next
# line.
/..*$/ {
  N
  s/^\([\t ][\t ]*\)} *\n[\t ]*else/\1} else/g
  t continue_else
  b try_brace
:continue_else
  N
:try_brace
  s/^\([\t ].*\)\n[\t ][\t ]*{/\1 {/g
  t done_brace
  P
  D
:done_brace
  p
  d
}
