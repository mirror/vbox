#
# Copyright (C) 2010-2011 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

HOSTOS=linux
VBOX_BIN = ../../../..
VBOX_SDK = ../../..
JACOB_DIR =
CLASSPATH = .

ifeq ($(HOSTOS),win)
 JACOB_JAR=$(JACOB_DIR)/jacob.jar
 VBOX_JAR=$(VBOX_SDK)/bindings/mscom/java/vboxjmscom.jar
 SEP=\;
 JAVA_COM_ARGS += -Djava.library.path=$(JACOB_DIR)
 CLASSPATH += $(JACOB_JAR)$(SEP)
else
 VBOX_JAR=$(VBOX_SDK)/bindings/xpcom/java/vboxjxpcom.jar
 SEP=:
 JAVA_COM_ARGS += -Dvbox.home=$(VBOX_BIN)
endif

VBOX_JAR_WS=$(VBOX_SDK)/bindings/webservice/java/jax-ws/vboxjws.jar


all: ws/TestVBox.class com/TestVBox.class

test: ws/test-TestVBox com/test-TestVBox

com/TestVBox.class:
	@mkdir com 2>/dev/null || true
	javac -g -d com -cp $(VBOX_JAR)$(SEP)$(CLASSPATH) TestVBox.java

com/test-TestVBox: com/TestVBox.class
	java $(JAVA_COM_ARGS) -cp com$(SEP)$(VBOX_JAR)$(SEP)$(CLASSPATH) TestVBox

ws/TestVBox.class:
	@mkdir ws 2>/dev/null || true
	javac -d ws -cp $(VBOX_JAR_WS)$(SEP)$(CLASSPATH) TestVBox.java

ws/test-TestVBox: ws/TestVBox.class
	java $(JAVA_WS_ARGS) -cp ws$(SEP)$(VBOX_JAR_WS)$(SEP)$(CLASSPATH) TestVBox -w -url http://localhost:18083/
