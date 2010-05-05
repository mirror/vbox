OS=$(shell uname -s)
ARCH=$(shell uname -p)

ifeq ($(OS), Darwin)
 VBOX_SDK=/Users/nike/work/shared/ws/out/darwin.x86/debug/dist/sdk
 VBOX_BIN=/Users/nike/work/shared/ws/out/darwin.x86/debug/dist/VirtualBox.app/Contents/MacOS/
 JAVA_OPT= -d32
endif

ifeq ($(OS), Linux)
 VBOX_SDK=/home/nike/work/ws/out/linux.amd64/debug/bin/sdk
 VBOX_BIN=/home/nike/work/ws/out/linux.amd64/debug/bin
endif

VBOX_JAR=$(VBOX_SDK)/bindings/xpcom/java/vboxjxpcom.jar

all: testvb

testvb:
	javac -cp $(VBOX_JAR) TestVBox.java
	java $(JAVA_OPT) -cp  $(VBOX_JAR):. -Dvbox.home=$(VBOX_BIN) TestVBox
