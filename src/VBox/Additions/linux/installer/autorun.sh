#!/bin/sh
#
# Sun VirtualBox
# VirtualBox Guest Additions installation script for Linux

#
# Copyrign Microsystems, Inc.
#
# Sun Microsystems, Inc.
# All rights reserved
#

PATH=$PATH:/bin:/sbin:/usr/sbin

# XXX only Linux so far
# XXX add support for the Solaris pkg
if test "`uname -s`" != "Linux"; then
  echo "Linux not detected."
  exit 1
fi

# 32-bit or 64-bit?
path=`dirname $0`
case `uname -m` in
  i[3456789]86|x86)
    arch='x86'
    ;;
  x86_64|amd64|AMD64)
    arch='amd64'
    ;;
  *)
    echo "Unknown architecture `uname -m`."
    exit 1
    ;;
esac

# execute the installer
if test -f "$path/VBoxLinuxAdditions-$arch.run"; then
  exec gksu /bin/sh "$path/VBoxLinuxAdditions-$arch.run"
fi

# else: unknown failure
echo "Linux guest additions installer not found -- try to start them manually."
exit 1
