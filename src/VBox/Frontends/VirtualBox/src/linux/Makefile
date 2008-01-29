# $Id
## @file
# Makefile for building the VirtualBox keyboard handler library.
#

# Note that for reasons of convenience, VirtualBox builds this library
# from inside its own build system rather than using this file.

# Copyright (C) 2008 innotek GmbH
#
# This library is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This library is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# Lesser General Public License as published by the Free Software
# Foundation, in version 2.1 as it comes in the "COPYING.LIB" file of the
# VirtualBox OSE distribution.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
             
VBoxKeyboard.so : keyboard-new.o
	gcc -shared -o VBoxKeyboard.so keyboard-new.o -lX11
