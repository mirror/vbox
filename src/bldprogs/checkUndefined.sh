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

target=$1
symbols=$2
static=$3

if [ $# -lt 2 -o $# -gt 3 -o ! -r $target -o ! -r $symbols ]
then
  if [ ! -r $target ]
  then
    echoerr "$0: $target not readable"
  elif [ ! -r $symbols ]
  then
    echoerr "$0: $symbols not readable"
  else
    echoerr "$0: Wrong number of arguments"
  fi
  args_ok="no"
fi

if [ $# -eq 3 -a ! "$static" = "--static" ]
then
  args_ok="no"
fi

if [ "$args_ok" = "no" ]
then
  echoerr "Usage: $0 <object> <allowed undefined symbols> [--static]"
  exit 1
fi

command="-T"
if [ "$static" = "--static" ]
then
  command="-t"
fi

undefined=`objdump $command $target | grep '*UND*' | grep -v -f $symbols | sed -e 's/^.*[ 	]\(.*\)/\1/'`
num_undef=`echo $undefined | wc -w`

if [ $num_undef -ne 0 ]
then
  echoerr "$0: following symbols not defined in $symbols:"
  echoerr "$undefined"
fi
# Return code
[ $num_undef -eq 0 ]
