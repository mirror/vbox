#
# Makefile for VBoxSDL (a simple frontend based on SDL).
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

PROGRAMS = VBoxSDL tstSDL


#
# VBoxSDL
#
ifneq ($(filter win linux,$(BUILD_TARGET)),)	## @todo Convert these two to the other template too!
 VBoxSDL_TEMPLATE  = VBOXR3NPEXE
 VBoxSDL_CXXFLAGS.win = \
	-EHsc
 VBoxSDL_CXXFLAGS.linux = \
	-DNDEBUG -DTRIMMED -DVBOX_WITH_XPCOM -O -Wall -fno-rtti -fno-exceptions \
	-Wno-non-virtual-dtor -Wno-long-long -fshort-wchar -pthread -pipe
else
 VBoxSDL_TEMPLATE  = VBOXMAINCLIENT
endif
VBoxSDL_SDKS       = LIBSDL
VBoxSDL_SOURCES    = \
	VBoxSDL.cpp \
	Framebuffer.cpp \
	Helper.cpp
ifndef VBOX_OSE
 VBoxSDL_SOURCES.win= \
	win32/WinUI.cpp \
	win32/TitlebarButtons.cpp \
	win32/VBoxSDL.rc
endif
VBoxSDL_DEFS       =
ifdef VBOX_WITH_SECURELABEL
 VBoxSDL_DEFS     += VBOX_SECURELABEL
endif
ifdef VBOX_WITH_VRDP
 VBoxSDL_DEFS     += VBOX_VRDP
endif
VBoxSDL_DEFS.linux = _GNU_SOURCE
ifdef VBOX_OPENGL
 #VBoxSDL_DEFS.linux += VBOX_OPENGL
endif
ifndef VBOX_OSE
 VBoxSDL_DEFS.win  = VBOX_WIN32_UI
endif
VBoxSDL_DEFS.win.x86 = _WIN32_WINNT=0x0500
VBoxSDL_DEFS.win.amd64 = _WIN32_WINNT=0x0510
VBoxSDL_INCS = \
	$(PATH_BIN)/sdk/include
ifdef VBOX_WITH_XPCOM
 VBoxSDL_INCS = \
	$(PATH_BIN)/sdk/include \
	$(VBOX_XPCOM_INCS) \
	$(PATH_VBoxSDL)
 VBoxSDL_LIBS = \
    $(LIB_SDK_LIBSDL_SDLMAIN) \
	$(PATH_LIB)/VBoxXPCOM$(VBOX_SUFF_LIB) \
	$(LIB_RUNTIME) \
	$(PATH_LIB)/VBoxXPCOMGlue$(VBOX_SUFF_LIB) \
	$(PATH_BIN)/VBoxXPCOM$(VBOX_SUFF_DLL)
 VBoxSDL_LIBS.linux = \
	$(LIB_XCURSOR)
 VBoxSDL_LDFLAGS.darwin = \
    -framework Foundation -framework AppKit
else # !VBOX_WITH_XPCOM
 VBoxSDL_INCS.win = \
	$(PATH_BIN)/sdk/include \
	$(PATH_TOOL_$(VBOX_VCC_TOOL)_ATLMFC_INC) \
	$(PATH_VBoxSDL)
 VBoxSDL_LIBS.win = \
	$(PATH_LIB)/VBoxCOM$(VBOX_SUFF_LIB) \
	$(PATH_TOOL_$(VBOX_VCC_TOOL)_ATLMFC_LIB)/atls.lib \
	$(LIB_RUNTIME)
endif # !VBOX_WITH_XPCOM
ifdef VBOX_OPENGL
 #VBoxSDL_LIBS.linux += GL
endif
VBoxSDL_LIBPATH.linux = \
	$(VBOX_LIBPATH_X11)

VBoxSDL_CLEAN = $(PATH_VBoxSDL)/Ico64x01.h
Framebuffer.cpp_DEPS = $(PATH_VBoxSDL)/Ico64x01.h


#
# tstSDL
#
tstSDL_TEMPLATE = VBOXR3NPEXE
tstSDL_SDKS       = LIBSDL
tstSDL_INST       = $(INST_TESTCASE)
tstSDL_SOURCES    = \
	VBoxSDLTest.cpp
tstSDL_DEFS       = IN_RING3 IN_RT_R3 _GNU_SOURCE
tstSDL_DEFS.win.x86 = _WIN32_WINNT=0x0500
ifdef VBOX_OPENGL
tstSDL_DEFS.linux = VBOX_OPENGL
endif
tstSDL_INCS = \
	$(PATH_tstSDL)

tstSDL_LIBS = \
	$(LIB_RUNTIME)
ifneq ($(filter-out win,$(BUILD_TARGET)),) # someone PLEASE explain why we don't use sdl main!
tstSDL_LIBS += \
	$(LIB_SDK_LIBSDL_SDLMAIN)
endif
ifdef VBOX_OPENGL
tstSDL_LIBS.linux += GL
endif
tstSDL_LIBPATH.linux = \
	$(VBOX_LIBPATH_X11)
tstSDL_LDFLAGS.darwin = \
	-framework Foundation -framework AppKit

tstSDL_CXXFLAGS.win = \
	-EHsc
tstSDL_CXXFLAGS.linux = \
	-DNDEBUG -DTRIMMED -O -Wall -fno-rtti -fno-exceptions \
	-Wno-non-virtual-dtor -Wno-long-long -fshort-wchar -pthread -pipe


# generate rules
include $(PATH_KBUILD)/footer.kmk


# Convert the pnm-file to a byte array.
$(PATH_VBoxSDL)/Ico64x01.h: ico64x01.pnm $(VBOX_BIN2C) | $(call DIRDEP,$(PATH_VBoxSDL))
	$(call MSG_L1,bin2c $< => $@)
	$(QUIET)$(VBOX_BIN2C) Ico64x01 $< $@

