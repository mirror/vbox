#
# Makefile for the VirtualBox Qt GUI.
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

ifeq ($(BUILD_TARGET),linux)
XKEYBOARD_NEW := 1
endif

# include qmake project file
include VBoxUI.pro

# Import QDesigner UI sources
VirtualBox_QT_UISRCS := $(FORMS)
# Import translation sources
VirtualBox_QT_TRANSLATIONS := $(TRANSLATIONS)
# Import images
VirtualBox_QT_IMAGES := $(IMAGES)

# reset things to avoid possible conflicts with kBuild
TEMPLATE :=
LANGUAGE :=
FORMS :=
TRANSLATIONS :=
IMAGES :=

DEPTH = ../../../..
include $(PATH_KBUILD)/header.kmk

PROGRAMS = VirtualBox

INSTALLS = VirtualBox.nls
INSTALLS.darwin = VirtualBox.app


VirtualBox_TEMPLATE = VBOXQTGUIEXE
VirtualBox_SDKS.win = WINPSDK DXSDK
VirtualBox_SDKS.linux = LIBSDL


# bird:
# this is a sketch which later can be moved into kBuild core stuff perhaps.
# I might not have got the logic in what causes what to be generated right
# feel free to help.

# QDesigner UI sources are imported from VBoxUI.pro as VirtualBox_QT_UISRC

# Headers containing definitions of classes that use the Q_OBJECT macro
VirtualBox_QT_MOCHDRS = \
	include/QIWidgetValidator.h \
	include/QIHotKeyEdit.h \
	include/QIStatusBar.h \
	include/QIStateIndicator.h \
	include/QIMessageBox.h \
	include/QIRichLabel.h \
	include/VMGlobalSettings.h \
	include/VBoxGlobal.h \
	include/VBoxMediaComboBox.h \
	include/VBoxSelectorWnd.h \
	include/VBoxConsoleWnd.h \
	include/VBoxConsoleView.h \
	include/VBoxProblemReporter.h

# Sources containing local definitions of classes that use the Q_OBJECT macro
VirtualBox_QT_MOCSRCS = src/VBoxSelectorWnd.cpp
ifneq ($(BUILD_TARGET),win)
VirtualBox_QT_MOCSRCS += src/COMDefs.cpp
endif

# UI headers (ui.h) containing local definitions of classes that use the Q_OBJECT macro
VirtualBox_QT_MOCUIHDRS = \
	ui/VBoxVMSettingsDlg.ui.h \
	ui/VBoxSharedFoldersSettings.ui.h
ifeq ($(BUILD_TARGET),win)
VirtualBox_QT_MOCUIHDRS += ui/VBoxVMNetworkSettings.ui.h
endif

# All generated sources. Note: this list MUST be in sync with Qt source
# generation rules defined somewhere below!
VirtualBox_GENSRCS = \
	$(foreach moc,$(notdir $(basename $(VirtualBox_QT_MOCHDRS))), $(PATH_VirtualBox)/moc/moc_$(moc).cpp) \
	$(foreach ui,$(notdir $(basename $(VirtualBox_QT_UISRCS))), $(PATH_VirtualBox)/ui/$(ui).cpp $(PATH_VirtualBox)/moc/moc_$(ui).cpp) \
	$(PATH_VirtualBox)/ui/vbox_image_collection.cpp

# All generated headers. Note: this list MUST be in sync with Qt source
# generation rules defined somewhere below!
VirtualBox_GENHDRS = \
	$(foreach mocui,$(notdir $(basename $(VirtualBox_QT_MOCUIHDRS))), $(PATH_VirtualBox)/moc/$(mocui).moc) \
	$(foreach moc,$(notdir $(basename $(VirtualBox_QT_MOCSRCS))), $(PATH_VirtualBox)/moc/$(moc).moc) \
	$(foreach ui,$(notdir $(basename $(VirtualBox_QT_UISRCS))), $(PATH_VirtualBox)/ui/$(ui).h)


VirtualBox_SOURCES = \
	$(VirtualBox_GENSRCS) \
	src/main.cpp \
	src/COMDefs.cpp \
	src/QIWidgetValidator.cpp \
	src/QIHotKeyEdit.cpp \
	src/QIStateIndicator.cpp \
	src/QIStatusBar.cpp \
	src/QIMessageBox.cpp \
	src/QIRichLabel.cpp \
	src/VMGlobalSettings.cpp \
	src/VBoxGlobal.cpp \
	src/VBoxMediaComboBox.cpp \
	src/VBoxProblemReporter.cpp \
	src/VBoxSelectorWnd.cpp \
	src/VBoxConsoleView.cpp \
	src/VBoxConsoleWnd.cpp \
	src/VBoxVMListBox.cpp \
	src/VBoxFrameBuffer.cpp

VirtualBox_SOURCES.win += \
	src/win32/VirtualBox.rc

VirtualBox_SOURCES.win += \
	src/VBoxFBDDRAW.cpp

ifdef XKEYBOARD_NEW
VirtualBox_SOURCES.linux = \
	src/linux/XKeyboardNew.cpp
else
VirtualBox_SOURCES.linux = \
	src/linux/XKeyboard.cpp \
	src/linux/keyboard.c
endif

VirtualBox_SOURCES.darwin = \
	src/darwin/DarwinKeyboard.cpp \
	src/darwin/DarwinCursor.cpp

VirtualBox_DEFS           = VBOX_GUI_SEPARATE_VM_PROCESS
VirtualBox_DEFS.debug     = VBOX_GUI_DEBUG VBOX_CHECK_STATE # QT_FATAL_ASSERT
VirtualBox_DEFS.darwin    = VBOX_GUI_USE_QIMAGE
VirtualBox_DEFS.linux     = VBOX_GUI_USE_SDL
VirtualBox_DEFS.win       = VBOX_GUI_USE_QIMAGE UNICODE QT_DLL
#ifndef VBOX_OSE
 VirtualBox_DEFS.darwin  += VBOX_WITH_HACKED_QT
#endif
ifneq ($(BUILD_TYPE),release)
 # non-release builds has some extra features.
 VirtualBox_DEFS         += VBOX_GUI_USE_REFRESH_TIMER VBOX_WITH_DEBUGGER_GUI_MENU
 VirtualBox_DEFS.win     += VBOX_GUI_USE_DDRAW
 VirtualBox_DEFS.linux   += VBOX_GUI_USE_QIMAGE
endif
ifdef VBOX_WITH_ALSA
 VirtualBox_DEFS         += VBOX_WITH_ALSA
endif
ifdef VBOX_OSE
 VirtualBox_DEFS         += VBOX_OSE
endif

VirtualBox_INCS           = \
	./include \
	$(PATH_VirtualBox)/ui \
	$(PATH_VirtualBox)/moc \
	$(PATH_VirtualBox)/include \


ifeq ($(BUILD_TYPE),release)
 VirtualBox_LDFLAGS.win  += /SUBSYSTEM:windows
else
 VirtualBox_LDFLAGS.linux+= -rdynamic # for backtrace_symbols()
 ifeq ($(USERNAME),dmik)
  VirtualBox_LDFLAGS.win += /SUBSYSTEM:windows
 else
  VirtualBox_LDFLAGS.win += /SUBSYSTEM:console
 endif
endif
VirtualBox_LDFLAGS.darwin = -framework IOKit
VirtualBox_LIBS.win       = \
	$(PATH_SDK_WINPSDK_LIB)/Htmlhelp.Lib \
	$(PATH_SDK_DXSDK_LIB)/ddraw.lib \
	$(PATH_SDK_DXSDK_LIB)/dxguid.lib

ifdef VBOX_WITH_DEBUGGER_GUI
 ifeq ($(BUILD_TARGET),win)
  VirtualBox_LIBS        += $(PATH_LIB)/VBoxDbg$(VBOX_SUFF_LIB)
 else
  VirtualBox_LIBS        += $(PATH_DLL)/VBoxDbg$(VBOX_SUFF_DLL)
 endif
endif

WRAPPERSFILE        = $(PATH_VirtualBox)/include/COMWrappers.h
WRAPPERSINCFILE     = include/COMDefs.h
WRAPPERSTEMPLATE    = include/COMWrappers.xsl
XIDLFILE            = ../../Main/idl/VirtualBox.xidl


# generated files we need to clean manually
OTHER_CLEAN           = \
	$(VirtualBox_GENSRCS) \
	$(VirtualBox_GENHDRS) \
	$(WRAPPERSFILE)


#
# On Mac OS X (darwin) we need to install icon resources and compusory bundle contents.
#
VirtualBox.app_INST = $(INST_VIRTUALBOX)Contents/
VirtualBox.app_MODE = 644
VirtualBox.app_SOURCES = \
	src/darwin/PkgInfo \
	$(PATH_TARGET)/Info.plist \
	images/VirtualBox.icns=>Resources/virtualbox.icns

$(PATH_TARGET)/Info.plist: src/darwin/Info.plist $(VBOX_VERSION_MK) | $(call DIRDEP,$(PATH_TARGET))
	$(call MSG_L1,Generating $@)
	$(xQUIET)$(RM) -f $@
	$(xQUIET)$(SED) \
		-e 's/@VBOX_VERSION_STRING@/$(VBOX_VERSION_STRING)/g' \
		-e 's/@VBOX_VERSION_MAJOR@/$(VBOX_VERSION_MAJOR)/g' \
		-e 's/@VBOX_VERSION_MINOR@/$(VBOX_VERSION_MINOR)/g' \
		-e 's/@VBOX_VERSION_BUILD@/$(VBOX_VERSION_BUILD)/g' \
		$< > $@

#
# Translation installation
#

VirtualBox.nls_INST = $(INST_BIN)nls/
VirtualBox.nls_SOURCES = $(patsubst %.ts,$(PATH_VirtualBox)/nls/%.qm,$(notdir $(VirtualBox_QT_TRANSLATIONS)))
VirtualBox.nls_MODE = 644



# Commit the magic.
# (note: before custom rules that make usage of generated variables!).
include $(PATH_KBUILD)/footer.kmk



#
# Qt source file generation rules
#

##  Generate a rule to create a MOC source file from a header containing
#   classes that use the Q_OBJECT macro.
#   @param  $mochdr     header file with Q_OBJECT
define def_qt_gen_src_moc

$(eval mocsrc := $(PATH_$(target))/moc/moc_$(notdir $(basename $(mochdr))).cpp)

$(target)_GENSRCS_REAL += $(mocsrc)

$(mocsrc): $(mochdr)
	$$(call MSG_L1,moc $(mochdr),=> $$@)
	$$(QUIET)$(MKDIR) -p $$(@D)
	$$(QUIET)$(VBOX_MOC) $(mochdr) -o $$@

endef

##  Generate a rule to create a MOC include file from a source containing
#   local classes that use the Q_OBJECT macro. This include is then included
#   by that source, so it must be generated before the source gets compiled.
#   @param  $mocsrc     source file with Q_OBJECT
define def_qt_gen_inc_moc

$(eval mocobj := $(PATH_$(target)_$(mocsrc))/$(notdir $(basename $(mocsrc)))$(SUFF_OBJ))
$(eval mocinc := $(PATH_$(target))/moc/$(notdir $(basename $(mocsrc))).moc)

$(target)_GENHDRS_REAL += $(mocinc)

$(mocobj): $$(mocinc)

.NOTPARALLEL: $$(mocinc)
$(mocinc): $(mocsrc)
	$$(call MSG_L1,moc $(mocsrc),=> $$@)
	$$(QUIET)$(MKDIR) -p $$(@D)
	$$(QUIET)$(VBOX_MOC) -i $(mocsrc) -o $$@

endef

##  Generate a rule to create a MOC include file from a UI header (ui.h) containing
#   local classes that use the Q_OBJECT macro. This include is then included
#   by that header, so it must be generated before the UI source gets compiled.
#   @param  $mocuihdr     UI header file with Q_OBJECT
define def_qt_gen_inc_mocuihdr

$(eval uisrc    := $(PATH_$(target))/ui/$(notdir $(basename $(basename $(mocuihdr)))).cpp)
$(eval uiobj    := $(PATH_$(target)_$$(uisrc))/$(notdir $(basename $$(uisrc)))$(SUFF_OBJ))
$(eval mocuiinc := $(PATH_$(target))/moc/$(notdir $(basename $(mocuihdr))).moc)

$(target)_GENHDRS_REAL += $(mocuiinc)

.NOTPARALLEL: $$(mocuiinc)
$(uisrc): $$(mocuiinc)
$(mocuiinc): $(mocuihdr)
	$$(call MSG_L1,moc $(mocuihdr),=> $$@)
	$$(QUIET)$(MKDIR) -p $$(@D)
	$$(QUIET)$(VBOX_MOC) -i $(mocuihdr) -o $$@

endef

##  Generate a rule to create a header and source files from an UI
#   definition source (.ui).
#   @param  $uifile       UI definintion source file
define def_qt_gen_src_ui

$(eval uisrc  := $(PATH_$(target))/ui/$(notdir $(basename $(uifile))).cpp)
$(eval uihdr  := $(PATH_$(target))/ui/$(notdir $(basename $(uifile))).h)
$(eval mocsrc := $(PATH_$(target))/moc/moc_$(notdir $(basename $(uifile))).cpp)

$(target)_GENSRCS_REAL += $(uisrc) $(mocsrc)
$(target)_GENHDRS_REAL += $(uihdr)

.NOTPARALLEL: $$(uihdr)
$(uihdr): $(uifile) | $$(call DIRDEP,$$(dir $$(uihdr)))
	$$(call MSG_L1,uic $(uifile),=> $$@)
	$$(QUIET)$(VBOX_UIC) $(uifile) -o $$@

$(uisrc): $$(uihdr) $(uifile) $(wildcard $(uifile).h) | $$(call DIRDEP,$$(dir $$(uisrc)))
	$$(call MSG_L1,uic $(uifile),=> $$@)
	$$(QUIET)$(VBOX_UIC) $(uifile) -i $(uihdr) -o $$@

$(mocsrc): $$(uihdr) | $$(call DIRDEP,$$(dir $$(mocsrc)))
	$$(call MSG_L1,moc $(uihdr),=> $$@)
	$$(QUIET)$(VBOX_MOC) $(uihdr) -o $$@

endef

##  Generate a rule to create a .qm file from a NLS translation
#   source (.ts).
#   @param  $tsfile       Translation source file
define def_qt_gen_nls

$(eval qmfile  := $(PATH_$(target))/nls/$(notdir $(basename $(tsfile))).qm)

OTHER_CLEAN += $(qmfile)

$(qmfile): $(tsfile) | $$(call DIRDEP,$$(dir $$(qmfile)))
	$$(call MSG_L1,lrelease $(tsfile),=> $$@)
	$$(QUIET)$(VBOX_LRELEASE) $(tsfile) -qm $$@

endef

## Generate rules for generating the Qt source for a target.
# @param   $target  Target name.
define def_qt_gen_src

# moc srcs from hdrs with Q_OBJECT
$(foreach mochdr,$($(target)_QT_MOCHDRS),$(eval $(def_qt_gen_src_moc)))
# moc includes from srcs with Q_OBJECT
$(foreach mocsrc,$($(target)_QT_MOCSRCS),$(eval $(def_qt_gen_inc_moc)))
# moc includes from UI headers with Q_OBJECT
$(foreach mocuihdr,$($(target)_QT_MOCUIHDRS),$(eval $(def_qt_gen_inc_mocuihdr)))
# UI sources
$(foreach uifile,$($(target)_QT_UISRCS),$(eval $(def_qt_gen_src_ui)))
# NLS files
$(foreach tsfile,$($(target)_QT_TRANSLATIONS),$(eval $(def_qt_gen_nls)))
# dirs
$$(call DIRDEP,$(PATH_$(target))/ui/) \
$$(call DIRDEP,$(PATH_$(target))/moc/) \
$$(call DIRDEP,$(PATH_$(target))/nls/):
	$$(call MSG_L2,Creating directory $$@)
	$$(QUIET)$(MKDIR) -p $$@

endef

# Generate Qt source rules.
$(foreach target,VirtualBox,$(eval $(def_qt_gen_src)))


# Generate COM Wrappers
.NOTPARALLEL: $(WRAPPERSFILE) $(WRAPPERSINCFILE)

$(WRAPPERSINCFILE): $(WRAPPERSFILE)

$(WRAPPERSFILE): $(XIDLFILE) $(WRAPPERSTEMPLATE) | $(call DIRDEP,$(PATH_VirtualBox)/include/)
	$(call MSG_L1,xsltproc $<,=> $@)
	$(QUIET)$(VBOX_XSLTPROC) -o $@ $(WRAPPERSTEMPLATE) $<

$(call DIRDEP,$(PATH_VirtualBox)/include/):
	$(call MSG_L2,Creating directory $@)
	$(QUIET)$(MKDIR) -p $@

# this is actually necessary only for Win32 target with disabled dependencies
define def_wrapper_deps
$(src): $(WRAPPERSFILE) $(WRAPPERSINCFILE)
endef

$(foreach src,$(VirtualBox_SOURCES),$(eval $(def_wrapper_deps)))

# static images imported from VBoxUI.pro as VirtualBox_QT_IMAGES

$(PATH_VirtualBox)/ui/vbox_image_collection.txt: VBoxUI.pro $(VirtualBox_QT_IMAGES)
	@echo " \
	$(VirtualBox_QT_IMAGES) \
    " > $@

$(PATH_VirtualBox)/ui/vbox_image_collection.cpp: $(PATH_VirtualBox)/ui/vbox_image_collection.txt
	$(call MSG_L1,uic $<,=> $@)
	$(QUIET)$(VBOX_UIC) -o $@ -embed VBoxGUI -f $<

VirtualBox_GENSRCS += $(PATH_VirtualBox)/ui/vbox_image_collection.cpp
OTHER_CLEAN += $(PATH_VirtualBox)/ui/vbox_image_collection.txt


#
# Hand made dependencies go here.
# Basically, here are dependencies for generated UI source files that
# include generated headers in turn.
#

$(PATH_VirtualBox)/ui/VBoxDiskImageManagerDlg.cpp: \
	$(PATH_VirtualBox)/ui/VBoxNewHDWzd.h

# Make all generated UI sources dependent on all generated headers (since they
# may include them).  This is safer than indifidual dependencies above but
# currently disabled, because will cause all UI sources to be rebuilt one a
# single one changes.
#$(patsubst %,$(PATH_VirtualBox)/ui/%.cpp,$(notdir $(basename $(VirtualBox_QT_UISRCS)))) : $(VirtualBox_GENHDRS)


#
# Custom targets
#

# Update all known NLS translation (.ts) files in the nls/ subdirectory
updatenls: $(VirtualBox_SOURCES) $(VirtualBox_QT_MOCHDRS) $(VirtualBox_GENHDRS)
	$(VBOX_LUPDATE) $^ -ts $(VirtualBox_QT_TRANSLATIONS)


#
# Test targets
#

test:
	@echo ---------------
	@echo $(VirtualBox_GENSRCS) | $(SED) -e "s/ /\n/g"
	@echo ---------------
	@echo $(VirtualBox_GENHDRS) | $(SED) -e "s/ /\n/g"
	@echo ---------------

test2:
	@echo ---------------
	@echo $(OTHER_CLEAN) | $(SED) -e "s/ /\n/g"
	@echo ---------------

testwrappers: $(WRAPPERSFILE)

