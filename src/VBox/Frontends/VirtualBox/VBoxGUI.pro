#
# VBox GUI: main Qt project file.
#
# NOTE: This file is NOT intended to be opened by Qt Designer!
#
#  Copyright (C) 2006-2007 innotek GmbH
# 
#  This file is part of VirtualBox Open Source Edition (OSE), as
#  available from http://www.virtualbox.org. This file is free software;
#  you can redistribute it and/or modify it under the terms of the GNU
#  General Public License as published by the Free Software Foundation,
#  in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
#  distribution. VirtualBox OSE is distributed in the hope that it will
#  be useful, but WITHOUT ANY WARRANTY of any kind.

TEMPLATE	= app

os2:TARGET		= VirtualBox_2
win32:TARGET	= VirtualBox_w
x11:TARGET	= VirtualBox_x

CONFIG	-= debug
CONFIG	+= qt warn_on release console

DEFINES += VBOX_DEBUG VBOX_CHECK_STATE
#win32:CONFIG += debug

DEPENDPATH += include ../../../include
INCLUDEPATH += include ../../../include

HEADERS += \
	include/COM.h \
	include/CIShared.h \
	include/QIWidgetValidator.h \
	include/VMConfigObject.h \
	include/VMManager.h \
	include/VMNamespace.h \
	include/VBoxGlobal.h \
	include/VBoxSelectorWnd.h \
	include/VBoxConsoleView.h \
	include/VBoxConsoleWnd.h \
	include/VBoxToolBar.h \
	include/VBoxVMListBox.h

SOURCES	+= \
	src/COM.cpp \
	src/main.cpp \
	src/QIWidgetValidator.cpp \
	src/VMConfigObject.cpp \
	src/VMManager.cpp \
	src/VBoxGlobal.cpp \
	src/VBoxSelectorWnd.cpp \
	src/VBoxConsoleView.cpp \
	src/VBoxConsoleWnd.cpp \
	src/VBoxVMListBox.cpp

# Qt Designer project file
# we need it to be separate from the main project file
include( VBoxUI.pro )

TRANSLATIONS = \
	vboxgui_de.ts \
	vboxgui_ru.ts

UI_DIR = .ui
MOC_DIR = .moc
# we use "ob" instead of "obj" because of the MINGW generator bug
# (it replaces "obj" it with "o" in the clean target)
os2:OBJECTS_DIR = .ob/os2
win32:OBJECTS_DIR = .ob/win32
x11:OBJECTS_DIR = .ob/x11

# Qt library dependencies
os2:PRE_TARGETDEPS += $(QTDIR)\lib\libqt-mt.a
#win32:PRE_TARGETDEPS += $(QTDIR)\lib\qt-mt331.lib
