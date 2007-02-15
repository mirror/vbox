#
# Makefile for the VBox Main module.
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

DEPTH = ../../..
include $(PATH_KBUILD)/header.kmk

ifdef VRDP_MC
DEFS       += VRDP_MC
endif

SUBDIRS_AFTER    = testcase

LIBRARIES        =
DLLS             = VBoxXML VBoxC
PROGRAMS         = VBoxSVC
ifeq ($(BUILD_TARGET),win)
 LIBRARIES       = VBoxCOM
else
 ifndef VBOX_WITH_XPCOM
  $(error "VBox: VBOX_WITH_XPCOM isn't defined")
 endif
 LIBRARIES       = VBoxXPCOM
 DEFS           += VBOX_WITH_XPCOM
 DEFS           += VBOX_WITH_SYS_V_IPC_SESSION_WATCHER
 DEFS           += VBOX_WITH_UNIXY_TAP_NETWORKING
endif
#PROGRAMS         = tstCFGLdr

ifeq ($(BUILD_TARGET),win)
 OTHERS          = $(PATH_BIN)/comregister.cmd
 IDLFILE         = $(PATH_BIN)/sdk/idl/VirtualBox.idl
 OTHER_CLEAN     = \
	$(PATH_BIN)/comregister.cmd \
 	$(PATH_BIN)/sdk/include/VirtualBox.h \
 	$(IDLFILE) \
 	$(PATH_BIN)/sdk/lib/VirtualBox.tlb \
 	$(PATH_BIN)/sdk/lib/VirtualBox_i.c \
 	$(PATH_VBoxCOM)/VirtualBox.h \
 	$(PATH_VBoxCOM)/VirtualBox_i.c \
 	$(PATH_VBoxCOM)/VirtualBox.tlb
else
 INSTALLS        = xpcom-components
 IDLFILE         = $(PATH_BIN)/sdk/idl/VirtualBox_XPCOM.idl
 IDLTYPELIB      = $(PATH_BIN)/VirtualBox_XPCOM.xpt
 IDLHEADER       = $(PATH_BIN)/sdk/include/VirtualBox_XPCOM.h
 OTHERS          = $(IDLTYPELIB)
 OTHER_CLEAN     = \
 	$(IDLFILE) \
 	$(IDLHEADER) \
 	$(IDLTYPELIB)

endif



#
# The Main API documentation
#
docs: $(PATH_TARGET)/docs.Main

$(PATH_TARGET)/docs.Main: \
		Doxyfile.Main \
		idl/doxygen.xsl \
		idl/VirtualBox.xidl \
		| $(call DIRDEP, $(PATH_TARGET)) \
		  $(call DIRDEP, $(PATH_OUT)/docs/Main)
	$(RM) -f $(wildcard $(PATH_OUT)/docs/Main/html/*)
	$(VBOX_XSLTPROC) -o $(PATH_TARGET)/VirtualBox.idl idl/doxygen.xsl idl/VirtualBox.xidl
	PATH_OUT="$(PATH_OUT)" PATH_TARGET="$(PATH_TARGET)" PATH_CHM="$(subst /,\\,$(PATH_BIN)/VirtualBoxAPI.chm)" doxygen Doxyfile.Main
	-$(EXEC_X86_WIN32) $(VBOX_PATH_HTML_HELP_WORKSHOP)/hhc.exe $(subst /,\\,$(PATH_OUT)/docs/Main/html/index.hhp)
	$(APPEND) $(PATH_TARGET)/docs.Main

$(call DIRDEP, $(PATH_OUT)/docs/Main):
	$(MKDIR) -p $@


#
# VBoxSVC
#
VBoxSVC_TEMPLATE = VBOXMAINEXE
VBoxSVC_DEFS = CFGLDR_HAVE_COM IN_CFGLDR_R3
ifdef VBOX_WITH_VRDP
VBoxSVC_DEFS += VBOX_VRDP IN_VRDP_R3
endif
ifdef VBOX_WITH_HGCM
VBoxSVC_DEFS += VBOX_HGCM
endif
ifdef VBOX_MAIN_RELEASE_LOG
VBoxSVC_DEFS += VBOX_MAIN_RELEASE_LOG LOG_ENABLED
endif
ifdef VBOX_WITH_USB
VBoxSVC_DEFS += VBOX_WITH_USB
endif
ifdef VBOX_WITH_ALSA
VBoxSVC_DEFS += VBOX_WITH_ALSA
endif
ifdef NO_COMPILER_H
VBoxSVC_DEFS += NO_COMPILER_H
endif
VBoxSVC_DEFS.win += VBOX_COM_OUTOFPROC_MODULE
VBoxSVC_DEFS.win.x86 += _WIN32_WINNT=0x0400
VBoxSVC_DEFS.win.amd64 += _WIN32_WINNT=0x0510
VBoxSVC_INCS = \
	include \
	$(PATH_VBoxSVC) \
	$(PATH_VBoxCOM) \
	$(PATH_BIN)/sdk/include
ifneq ($(BUILD_TARGET),win)
VBoxSVC_INCS += \
	$(VBOX_XPCOM_INCS)
endif
ifdef VBOX_WITH_XPCOM
VBoxSVC_LIBS += \
	$(PATH_BIN)/VBoxDD$(VBOX_SUFF_DLL) \
	$(PATH_BIN)/VBoxXML$(VBOX_SUFF_DLL) \
	$(TARGET_VBoxXPCOM)
VBoxSVC_LIBS += \
	$(PATH_LIB)/VBoxXPCOMGlue$(VBOX_SUFF_LIB) \
	$(PATH_BIN)/VBoxXPCOM$(VBOX_SUFF_DLL)
VBoxSVC_LIBS.darwin = \
	$(LIB_REM)
else
VBoxSVC_LIBS = \
	$(PATH_LIB)/VBoxDD$(VBOX_SUFF_LIB) \
	$(PATH_LIB)/VBoxXML.lib \
	$(PATH_LIB)/VBoxCOM$(VBOX_SUFF_LIB)
endif
VBoxSVC_SOURCES = \
	Logging.cpp \
	Matching.cpp \
	USBDeviceFilterImpl.cpp \
	USBProxyService.cpp \
	VirtualBoxBase.cpp \
	VirtualBoxXMLUtil.cpp \
	VirtualBoxErrorInfoImpl.cpp \
	VirtualBoxImpl.cpp \
	MachineImpl.cpp \
	SnapshotImpl.cpp \
	HardDiskImpl.cpp \
	HardDiskAttachmentImpl.cpp \
	ProgressImpl.cpp \
	DVDDriveImpl.cpp \
	DVDImageImpl.cpp \
	FloppyDriveImpl.cpp \
	FloppyImageImpl.cpp \
	HostImpl.cpp \
	HostDVDDriveImpl.cpp \
	HostFloppyDriveImpl.cpp \
	HostUSBDeviceImpl.cpp \
	GuestOSTypeImpl.cpp \
	NetworkAdapterImpl.cpp \
	USBControllerImpl.cpp \
	AudioAdapterImpl.cpp \
	SharedFolderImpl.cpp \
	SystemPropertiesImpl.cpp \
	BIOSSettingsImpl.cpp

ifdef VBOX_WITH_VRDP
VBoxSVC_SOURCES += VRDPServerImpl.cpp
endif

VBoxSVC_SOURCES.win = \
	HostNetworkInterfaceImpl.cpp \
	win32/svcmain.cpp \
	win32/svchlp.cpp \
	win32/VBoxSVC.rc

ifdef VBOX_WITH_XPCOM
VBoxSVC_SOURCES += \
	linux/server.cpp
endif

ifdef VBOX_WITH_USB
VBoxSVC_SOURCES.linux += linux/USBProxyServiceLinux.cpp
VBoxSVC_SOURCES.win   += win32/USBProxyServiceWin32.cpp
endif

ifeq ($(BUILD_TYPE),debug)
VBoxSVC_LDFLAGS.linux += -rdynamic # for backtrace_symbols()
endif

#
# VBoxC
#
VBoxC_TEMPLATE = VBOXMAIN
VBoxC_DEFS = IN_RING3 IN_CFGLDR_R3
VBoxC_DEFS += VBOX_COM_INPROC
ifdef VBOX_WITH_VRDP
VBoxC_DEFS += VBOX_VRDP IN_VRDP_R3
endif
ifdef VBOX_WITH_HGCM
VBoxC_DEFS += VBOX_HGCM
endif
ifdef VBOX_MAIN_RELEASE_LOG
VBoxC_DEFS += VBOX_MAIN_RELEASE_LOG LOG_ENABLED
endif
ifdef VBOX_WITH_USB
VBoxC_DEFS += VBOX_WITH_USB
endif
ifdef VBOX_WITH_ALSA
VBoxC_DEFS += VBOX_WITH_ALSA
endif
VBoxC_DEFS.win.x86 += _WIN32_WINNT=0x0400
VBoxC_DEFS.win.amd64 += _WIN32_WINNT=0x0510
VBoxC_INCS          = \
	include \
	$(PATH_VBoxC) \
	$(PATH_VBoxCOM) \
	$(PATH_BIN)/sdk/include
ifneq ($(BUILD_TARGET),win)
VBoxC_INCS          += \
	$(VBOX_XPCOM_INCS)
endif
ifdef VBOX_USE_VCC80
VBoxC_LDFLAGS.win = /MANIFEST
endif
ifdef VBOX_WITH_XPCOM
VBoxC_LIBS = \
	$(TARGET_VBoxXPCOM) \
	$(PATH_BIN)/VBoxXPCOM$(VBOX_SUFF_DLL) \
	$(PATH_LIB)/VBoxXPCOMGlue$(VBOX_SUFF_LIB)
else
VBoxC_LIBS = \
	$(PATH_LIB)/VBoxCOM$(VBOX_SUFF_LIB)
endif
VBoxC_LIBS += \
	$(LIB_VMM) \
	$(LIB_REM)
VBoxC_SOURCES = \
	Logging.cpp \
	VBoxDll.cpp \
	USBDeviceImpl.cpp \
	RemoteUSBDeviceImpl.cpp \
	VirtualBoxBase.cpp \
	VirtualBoxErrorInfoImpl.cpp \
	ProgressImpl.cpp \
	SharedFolderImpl.cpp \
	SessionImpl.cpp \
	ConsoleImpl.cpp \
	ConsoleVRDPServer.cpp \
	GuestImpl.cpp \
	KeyboardImpl.cpp \
	MouseImpl.cpp \
	DisplayImpl.cpp \
	FramebufferImpl.cpp \
	MachineDebuggerImpl.cpp \
	VBoxDriversRegister.cpp \
	AudioSnifferInterface.cpp \
	VMMDevInterface.cpp
VBoxC_SOURCES.win = \
	win32/dllmain.cpp \
	win32/VBoxC.def \
	win32/VBoxC.rc
ifdef VBOX_WITH_XPCOM
VBoxC_SOURCES += \
	linux/module.cpp
endif

ifdef VBOX_WITH_HGCM
VBoxC_SOURCES += \
	hgcm/HGCMObjects.cpp \
	hgcm/HGCMThread.cpp \
	hgcm/HGCM.cpp
endif

ifdef VBOX_WITH_USB
VBoxC_SOURCES += \
	RemoteUSBBackend.cpp
endif


#
# VBoxXML
#
VBoxXML_TEMPLATE   = VBOXMAIN
VBoxXML_SDKS       = VBOX_XALAN VBOX_XERCES
## @todo Why do we need this? Why isn't it in the template?!?
VBoxXML_CXXFLAGS.win = -EHsc
VBoxXML_DEFS       = \
	CFGLDR_HAVE_COM IN_RING3 IN_CFGLDR_R3
VBoxXML_INCS       = \
	include \
	$(PATH_TARGET)
VBoxXML_SOURCES    = \
	xml/cfgldr.cpp \
	xml/cfgldrhlp.cpp
ifdef VBOX_WITH_XPCOM
VBoxXML_LIBS       = \
	$(TARGET_VBoxXPCOM) \
	$(PATH_BIN)/VBoxXPCOM$(VBOX_SUFF_DLL)
endif

## @todo (dmik): temporarily embed SettingsConverter.xsl into VBoxXML,
#                later it should go to VBoxSVC as well
xml/cfgldr.cpp_DEPS = $(PATH_TARGET)/SettingsConverter_xsl.h
OTHER_CLEAN += $(PATH_TARGET)/SettingsConverter_xsl.h
$(PATH_TARGET)/SettingsConverter_xsl.h: xml/SettingsConverter.xsl $(VBOX_BIN2C) | $(call DIRDEP,$(PATH_TARGET))
	$(call MSG_L1,bin2c $< => $@)
	$(QUIET)$(VBOX_BIN2C) SettingsConverter_xsl $< $@


#
# tstCFGLdr - broken.
#
tstCFGLdr_TEMPLATE = VBOXR3
tstCFGLdr_SDKS     = VBOX_XALAN VBOX_XERCES
tstCFGLdr_DEFS     = IN_CFGLDR_R3 STANDALONE_TEST
## @todo these makes uncessary noice. fix the template usage!
tstCFGLdr_CXXFLAGS.win = -EHsc
tstCFGLdr_SOURCES  = \
	xml/cfgldr.cpp \
	xml/cfgldrhlp.cpp
tstCFGLdr_INCS     = \
	include \
	$(PATH_TARGET)
ifdef VBOX_WITH_XPCOM
tstCFGLdr_LIBS     = \
	$(PATH_LIB)/VBoxXPCOM$(VBOX_SUFF_LIB)
endif
tstCFGLdr_LIBS    += \
	$(LIB_RUNTIME)


#
# VBoxCOM - COM Abstraction Layer library
#
VBoxCOM_COMMON_SOURCES = \
	glue/com.cpp \
	glue/string.cpp \
	glue/EventQueue.cpp \
	glue/ErrorInfo.cpp

VBoxCOM_TEMPLATE = VBOXMAIN
VBoxCOM_DEFS.x86 = _WIN32_WINNT=0x0500
VBoxCOM_DEFS.amd64 = _WIN32_WINNT=0x0510
VBoxCOM_SOURCES = \
	$(VBoxCOM_COMMON_SOURCES) \
	$(PATH_VBoxCOM)/VirtualBox_i.c
VBoxCOM_INCS = \
	$(PATH_BIN)/sdk/include


#
# VBoxXPCOM
#
VBoxXPCOM_TEMPLATE = VBOXMAIN
VBoxXPCOM_SOURCES = \
	$(VBoxCOM_COMMON_SOURCES) \
	linux/helpers.cpp
VBoxXPCOM_INCS = \
	$(PATH_BIN)/sdk/include \
	include \
	$(VBOX_XPCOM_INCS)


#
# Generate _DEPS on all sources which depends on generated headers.
#
ifeq ($(BUILD_TARGET),win)
PREREQS = $(PATH_VBoxCOM)/VirtualBox_i.c
else
PREREQS = $(IDLTYPELIB) $(IDLHEADER)
endif

# brute force way to ensure the prereqs are generated before anything is compiled.
define def_src_deps
$(src)_DEPS = $(PREREQS)
endef
$(foreach src,\
	$(VBoxC_SOURCES)   $(VBoxC_SOURCES.$(BUILD_TARGET)) \
	$(VBoxSVC_SOURCES) $(VBoxSVC_SOURCES.$(BUILD_TARGET)) \
	$(VBoxCOM_COMMON_SOURCES) $(VBoxCOM_SOURCES) $(VBoxXPCOM_SOURCES) \
	,$(eval $(value def_src_deps)))


#
# Embed XML Schema files to VBoxSVC
#
VBOX_XML_ENTITIES        = $(PATH_TARGET)/VirtualBoxXMLUtil_entities.h
VBOX_XML_ENTITIES_COMMON = $(PATH_TARGET)/VirtualBoxXMLUtil_common_entities.h

VBOX_XML_SCHEMA_COMMON = xml/VirtualBox-settings-common.xsd
VBOX_XML_SCHEMA.darwin = xml/VirtualBox-settings-macosx.xsd
VBOX_XML_SCHEMA.linux  = xml/VirtualBox-settings-linux.xsd
VBOX_XML_SCHEMA.win    = xml/VirtualBox-settings-windows.xsd

VirtualBoxXMLUtil.cpp_DEPS = $(VBOX_XML_ENTITIES) $(VBOX_XML_ENTITIES_COMMON)

$(VBOX_XML_ENTITIES_COMMON): $(VBOX_XML_SCHEMA_COMMON) $(VBOX_BIN2C)
	$(call MSG_L1,bin2c $< => $@)
	$(QUIET)$(VBOX_BIN2C) VirtualBox_settings_common_xsd $< $@

$(VBOX_XML_ENTITIES): $(VBOX_XML_SCHEMA.$(BUILD_TARGET)) $(VBOX_BIN2C)
	$(call MSG_L1,bin2c $< => $@)
	$(QUIET)$(VBOX_BIN2C) VirtualBox_settings_xsd $< $@

OTHER_CLEAN     += $(VBOX_XML_ENTITIES) $(VBOX_XML_ENTITIES_COMMON)


#
# Generate SchemaDefs.h from XML Schema
#
VBOX_XML_SCHEMADEFS_H   = $(PATH_TARGET)/SchemaDefs.h
VBOX_XML_SCHEMADEFS_XSL = xml/SchemaDefs.xsl

VBoxC_DEPS      += $(VBOX_XML_SCHEMADEFS_H)
VBoxC_INCS      += $(PATH_TARGET)
VBoxSVC_DEPS    += $(VBOX_XML_SCHEMADEFS_H)
VBoxSVC_INCS    += $(PATH_TARGET)

$(VBOX_XML_SCHEMADEFS_H): $(VBOX_XML_SCHEMADEFS_XSL) $(VBOX_XML_SCHEMA.$(BUILD_TARGET)) $(VBOX_XML_SCHEMA_COMMON)
	$(call MSG_L1,xsltproc $@)
	$(QUIET)$(VBOX_XSLTPROC) -o $@ $(filter-out $(VBOX_XML_SCHEMA_COMMON),$^)

OTHER_CLEAN     += $(VBOX_XML_SCHEMADEFS_H)
.NOTPARALLEL:      $(VBOX_XML_SCHEMADEFS_H)



#
# Creates the symlinks necessary for making XPCOM work.
#
xpcom-components_INST = bin/components/
xpcom-components_SYMLINKS = \
	VBoxC$(SUFF_DLL)=>../VBoxC$(SUFF_DLL) \
	VBoxXPCOMIPCC$(SUFF_DLL)=>../VBoxXPCOMIPCC$(SUFF_DLL) \
	VirtualBox_XPCOM.xpt=>../VirtualBox_XPCOM.xpt \
	VBoxXPCOMBase.xpt=>../VBoxXPCOMBase.xpt


# generate
include $(PATH_KBUILD)/footer.kmk


#
# Additions rules.
#

XIDLFILE = idl/VirtualBox.xidl

ifneq ($(BUILD_TARGET),win)

linux/helpers.cpp: $(IDLTYPELIB)

$(IDLFILE): idl/xpidl.xsl $(XIDLFILE)
	$(call MSG_L1,xsltproc $<,=> $@)
	$(QUIET)$(MKDIR) -p $(@D)
	$(QUIET)$(VBOX_XSLTPROC) -o $@ $^

$(IDLTYPELIB): $(IDLFILE)
	$(call MSG_L1,xpidl $<,=> $@)
	$(QUIET)$(VBOX_XPIDL) -m typelib -I $(VBOX_PATH_XPCOM_IDL) -e $@ $<

$(IDLHEADER): $(IDLFILE)
	$(call MSG_L1,xpidl $<,=> $@)
	$(QUIET)$(VBOX_XPIDL) -m header  -I $(VBOX_PATH_XPCOM_IDL) -e $@ $<

$(PATH_BIN)/components/.keep:
	$(MKDIR) -p $(@D)
	$(QUIET)echo "keep" > $@

else

MAIN_IDL ?= $(EXEC_X86_WIN32) $(firstword $(wildcard \
	$(PATH_SDK_WINPSDK_BIN)/Midl.Exe\
	$(PATH_SDK_WINPSDK)/Bin/Midl.Exe\
	$(PATH_DEVTOOLS)/win.x86/bin/midl.exe\
	) Sorry_Cannot_Find_The_Midl_Compiler_In_The_PSDK)
IDL_DEFS = /nologo
ifdef VBOX_WITH_VRDP
 IDL_DEFS += /D VBOX_VRDP
endif

$(IDLFILE): idl/midl.xsl $(XIDLFILE)
	$(call MSG_L1,xsltproc $<,=> $@)
	$(QUIET)$(MKDIR) -p $(PATH_BIN)/sdk/idl
	$(QUIET)$(VBOX_XSLTPROC) -o $@ $^

## @todo r=bird: last changes to this rule showed incorrect dependencies here as it broke testcase (see testboxwin2).
# This is kind of obvious when looking at the rule, because it's (a) not specifing all it output
# and (b) generating more stuff *after* the maintarget has been completed.
#
# What needs to be done is to not depend on _i.c in the object subdir, but on all the final outputs.
$(PATH_VBoxCOM)/VirtualBox_i.c: $(IDLFILE) | $(call DIRDEP,$(PATH_VBoxCOM))
	$(MAIN_IDL) $(IDL_DEFS) \
		/out $(subst /,\\,$(PATH_VBoxCOM)) \
		/cpp_cmd $(subst /,\\,$(TOOL_$(VBOX_VCC_TOOL)_CC)) \
		/I $(subst /,\\,$(PATH_SDK_WINPSDK_INC)) \
		/I idl \
		$(subst /,\\,$<)
	$(MKDIR) -p $(PATH_BIN)/sdk/include
	$(CP) $(PATH_VBoxCOM)/VirtualBox.h $(PATH_BIN)/sdk/include
	$(MKDIR) -p $(PATH_BIN)/sdk/lib
	$(CP) $(PATH_VBoxCOM)/VirtualBox.tlb $(PATH_BIN)/sdk/lib
	$(CP) $(PATH_VBoxCOM)/VirtualBox_i.c $(PATH_BIN)/sdk/lib

endif

# bird: anyone still using this? If not it's a gonner.
## Hack to work around wine's trouble with midl.exe and rc.exe (seems to be msvcrt.dll related)
#gen: $(PREREQS)


#
# Translation stuff
#
VBoxSVC_HEADERS = \
	include/collection.h \
	include/MachineImpl.h \
	include/HostDVDDriveImpl.h \
	include/HostFloppyDriveImpl.h

VBoxC_HEADERS = \
	include/ConsoleImpl.h

VBoxSVC_TRANSLATIONS = \
	nls/VBoxSVC_de.ts

VBoxC_TRANSLATIONS = \
	nls/VBoxC_de.ts

updatenls:
	$(VBOX_LUPDATE) $(VBoxSVC_SOURCES) $(VBoxSVC_HEADERS) -ts $(VBoxSVC_TRANSLATIONS)
	$(VBOX_LUPDATE) $(VBoxC_SOURCES) $(VBoxC_HEADERS) -ts $(VBoxC_TRANSLATIONS)

ifdef VBOX_WITH_XPCOM

testidl: $(IDLFILE) $(IDLTYPELIB)
testidlhdr: $(IDLHEADER)

else

testidl: $(IDLFILE) $(PATH_VBoxCOM)/VirtualBox_i.c

endif

$(PATH_BIN)/comregister.cmd: win32/comregister.cmd
	$(CP) $< $@


# Hot fix when dependencies goes bad again...
#.NOTPARALLEL: $(TARGET_VBoxC) $(TARGET_VBoxSVC) $(TARGET_VBoxXPCOM) $(TARGET_VBoxCOM) $(IDLHEADER) $(IDLFILE) $(PATH_VBoxCOM)/VirtualBox_i.c

