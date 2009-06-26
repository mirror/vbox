#! /bin/sh

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
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#

# Compare undefined symbols in a shared or static object against a new-line
# separated list of grep patterns in a text file.
#
# Usage: /bin/sh <script name> <object> <allowed undefined symbols> [--static]
#
# Currently only works for native objects on Linux platforms

echoerr()
{
  echo $* 1>&2
}

hostos=$1
target=$2
symbols=$3
static=$4

if test $# -lt 3 || test $# -gt 4 || test ! -r "$target" || test ! -r "$symbols"; then
  if test ! -r "$target"; then
    echoerr "$0: '$target' not readable"
  elif test ! -r "$symbols"; then
    echoerr "$0: '$symbols' not readable"
  else
    echoerr "$0: Wrong number of arguments"
  fi
  args_ok="no"
fi

if test $# -eq 4 && test "$static" != "--static"; then
  args_ok="no"
fi

if test "$args_ok" = "no"; then
  echoerr "Usage: $0 <object> <allowed undefined symbols> [--static]"
  exit 1
fi

if test "$hostos" = "solaris"; then
    objdumpbin=/usr/sfw/bin/gobjdump
    grepbin=/usr/sfw/bin/ggrep
elif test "$hostos" = "linux"; then
    objdumpbin=`which objdump`
    grepbin=`which grep`
else
    echoerr "$0: '$hostos' not a valid hostos string. supported 'linux' 'solaris'"
    exit 1
fi

command="-T"
if test "$static" = "--static"; then
  command="-t"
fi

if test ! -x "$objdumpbin"; then
    echoerr "$0: '$objdumpbin' not found or not executable."
    exit 1
fi

undefined=`$objdumpbin $command $target | $grepbin '*UND*' | $grepbin -v -f $symbols | sed -e 's/^.*[ 	]\(.*\)/\1/'`
num_undef=`echo $undefined | wc -w`

if test $num_undef -ne 0; then
  echoerr "$0: following symbols not defined in $symbols:"
  echoerr "$undefined"
  exit 1
fi
# Return code
exit 0


