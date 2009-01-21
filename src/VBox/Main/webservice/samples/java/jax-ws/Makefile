#
# Copyright (C) 2008 Sun Microsystems, Inc.
#
# Sun Microsystems, Inc. confidential
# All rights reserved
#

JAVA16=java
JAVA15=/usr/lib/jvm/java-1.5.0-sun/bin/java
JAVAC = javac
JAVAC15 = javac -target 1.5
JAVAC16 = javac -target 1.6
MKDIR = mkdir
RM = rm

DEST16 = ./gen16
DEST15 = ./gen15

VBOXWS15 = ../lib/vboxws_java15.jar
VBOXWS16 = ../lib/vboxws_java16.jar

JAXWS=./jaxws-ri
JAXWSLIBS=$(JAXWS)/lib/jaxws-api.jar:$(JAXWS)/lib/jaxb-api.jar:$(JAXWS)/lib/jsr181-api.jar:$(JAXWS)/lib/jaxws-rt.jar

all: run16

metric: metric16

$(DEST16)/clienttest.class: clienttest.java
	$(MKDIR) -p $(DEST16)
	$(JAVAC16) -d $(DEST16) -cp $(VBOXWS16) $<

$(DEST15)/clienttest.class: clienttest.java
	$(MKDIR) -p $(DEST15)
	$(JAVAC15) -d $(DEST15) -cp $(VBOXWS15):$(JAXWSLIBS) $<

run16: $(DEST16)/clienttest.class
	$(JAVA16) -cp $(VBOXWS16):$(DEST16) clienttest show vms

run15: $(DEST15)/clienttest.class
	$(JAVA15) -cp $(VBOXWS15):$(JAXWSLIBS):$(DEST15) clienttest show vms

$(DEST16)/metrictest.class: metrictest.java
	$(MKDIR) -p $(DEST16)
	$(JAVAC16) -d $(DEST16) -cp $(VBOXWS16) $<

$(DEST15)/metrictest.class: metrictest.java
	$(MKDIR) -p $(DEST15)
	$(JAVAC15) -d $(DEST15) -cp $(VBOXWS15):$(JAXWSLIBS) $<

metric16: $(DEST16)/metrictest.class
	-$(JAVA16) -cp $(VBOXWS16):$(DEST16) metrictest

metric15: $(DEST15)/metrictest.class
	-$(JAVA15) -cp $(VBOXWS15):$(JAXWSLIBS):$(DEST15) metrictest

clean:
	$(RM) -rf $(DEST15)  $(DEST16) 
