# $Id
## @file
# Makefile for building the VirtualBox keyboard handler library.
#

# Note that for reasons of convenience, VirtualBox builds this library
# from inside its own build system rather than using this file.

# Copyright (C) 2007-2008 Sun Microsystems, Inc.
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

VBoxKeyboard.so : keyboard-new.o
	gcc -shared -o VBoxKeyboard.so keyboard-new.o -lX11
