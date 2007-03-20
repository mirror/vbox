#
# Makefile for VBoxBFE (a basic frontend which doesn't make use of Main).
#

#
# Copyright (C) 2006 InnoTek Systemberatung GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License as published by the Free Software Foundation,
# in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
# distribution. VirtualBox OSE is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY of any kind.
#
# If you received this file as part of a commercial VirtualBox
# distribution, then only the terms of your commercial VirtualBox
# license agreement apply instead of the previous paragraph.
#

DEPTH = ../../../..
include $(PATH_KBUILD)/header.kmk

PROGRAMS = VBoxBFE

#
# VBoxBFE
#
VBoxBFE_TEMPLATE = VBOXR3NPEXE
ifneq ($(BUILD_TARGET),l4)
VBoxBFE_SDKS = LIBSDL
VBoxBFE_DEFS = USE_SDL
endif
#ifdef VBOX_WITH_SECURELABEL
#VBoxBFE_DEFS += VBOX_SECURELABEL
#endif
ifdef VBOX_WITH_VRDP
VBoxBFE_DEFS += VBOX_VRDP
endif
ifneq ($(BUILD_TARGET).$(VBOX_WITHOUT_COM),win.)
VBoxBFE_DEFS += VBOXBFE_WITHOUT_COM
endif
ifdef VBOX_WITHOUT_LINUX_COMPILER_H
VBoxBFE_DEFS += VBOX_WITHOUT_LINUX_COMPILER_H
endif
VBoxBFE_DEFS.l4 = _GNU_SOURCE
VBoxBFE_DEFS.linux = _GNU_SOURCE VBOXBFE_WITH_X11
VBoxBFE_DEFS.win.x86 = _WIN32_WINNT=0x0500

VBoxBFE_SOURCES = \
	VBoxBFE.cpp \
	VMMDevInterface.cpp \
	DisplayImpl.cpp \
	MouseImpl.cpp \
	KeyboardImpl.cpp \
	StatusImpl.cpp \
	MachineDebuggerImpl.cpp

# USB Support
ifneq ($(filter-out os2 darwin,$(BUILD_TARGET)),)
VBoxBFE_DEFS += VBOXBFE_WITH_USB
VBoxBFE_SOURCES += \
	HostUSBImpl.cpp \
	HostUSBDeviceImpl.cpp \
	USBProxyService.cpp
endif

VBoxBFE_SOURCES.win = \
	SDLConsole.cpp \
	SDLFramebuffer.cpp
VBoxBFE_SOURCES.os2 = \
	SDLConsole.cpp \
	SDLFramebuffer.cpp
VBoxBFE_SOURCES.linux = \
	SDLConsole.cpp \
	SDLFramebuffer.cpp \
	USBProxyServiceLinux.cpp
VBoxBFE_SOURCES.l4    = \
	L4Console.cpp \
	L4Framebuffer.cpp \
	EmulCpp.cpp \
	USBProxyServiceLinux.cpp
VBoxBFE_SOURCES.darwin = \
	SDLConsole.cpp \
	SDLFramebuffer.cpp

VBoxBFE_INCS = \
	$(PATH_VBoxBFE) \
	$(PATH_BIN)/sdk/include
VBoxBFE_INCS.linux = \
	$(PATH_BIN)/sdk/include \
	$(VBOX_XPCOM_INCS)

VBoxBFE_LIBS = \
	$(LIB_RUNTIME) \
	$(LIB_VMM)
ifndef VBOX_WITHOUT_COM
VBoxBFE_LIBS.win = \
	$(PATH_TOOL_$(VBOX_VCC_TOOL)_ATLMFC_LIB)/atls.lib
endif
VBoxBFE_LIBS.linux = \
	$(LIB_RUNTIME) \
	$(LIB_XCURSOR)
VBoxBFE_LIBS.l4 = \
	$(L4_LIBDIR)/libl4con-idl.a \
	$(L4_LIBDIR)/libdm_generic.a \
	$(L4_LIBDIR)/libdm_mem.a \
	$(L4_LIBDIR)/libnames.a \
	$(L4_LIBDIR)/libconstream-server.a \
	$(L4_LIBDIR)/libl4sys.a
VBoxBFE_LIBS.darwin = \
	$(LIB_REM) \
	$(LIB_SDK_LIBSDL_SDLMAIN)
VBoxBFE_LDFLAGS.darwin = -framework Foundation -framework AppKit

VBoxBFE_LIBPATH.linux = \
	$(LIBPATH_XPCOM) \
	$(VBOX_LIBPATH_X11)


VBoxBFE_CXXFLAGS.win = \
	-EHsc
VBoxBFE_CXXFLAGS.linux =  \
	-DNDEBUG -DTRIMMED -O -Wall -fno-rtti -fno-exceptions \
	-Wno-non-virtual-dtor -Wno-long-long -fshort-wchar -pthread -pipe
VBoxBFE_CXXFLAGS.l4 += -fno-rtti -nostdinc -Wno-non-virtual-dtor \
	$(addprefix -I,$(VBOX_L4_GCC3_INCS) $(L4_INCDIR))
VBoxBFE_CFLAGS.linux += -O ## @todo what's this good for?


include $(PATH_KBUILD)/footer.kmk

