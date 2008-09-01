#!/usr/bin/python
#
#################################################################################
# This program is a simple interactive shell for VirtualBox. You can query      #
# information and issue commands from a simple command line.                    #
#                                                                               #
# It also provides you with examples on how to use VirtualBox's Python API.     #
# This shell is even somewhat documented and supports TAB-completion and        #
# history if you have Python readline installed.                                #
#                                                                               #
#                                                Enjoy.                         #
#################################################################################
#
# To make it work, the following variables have to be set.
# Please note the trailing slash in VBOX_PROGRAM_PATH - it's a must.
# 
# This is the place where VirtualBox resides
#  export VBOX_PROGRAM_PATH=/home/nike/work/ws/out/linux.amd64/debug/bin/
# To allow Python find modules
#  export PYTHONPATH=$VBOX_PROGRAM_PATH/sdk/bindings/xpcom/python:$VBOX_PROGRAM_PATH
# To allow library resolution
#  export LD_LIBRARY_PATH=$VBOX_PROGRAM_PATH
#
# Additionally, on 64-bit Solaris, you need to use 64-bit Python from 
# /usr/bin/amd64/python and due to quirks in native modules loading of 
# Python do the following:
#   mkdir $VBOX_PROGRAM_PATH/64
#   ln -s $VBOX_PROGRAM_PATH/VBoxPython.so $VBOX_PROGRAM_PATH/64/VBoxPython.so 
#
# Mac OS X users can get away with just this:
#   export PYTHONPATH=$VBOX_SDK/bindings/xpcom/python:$PYTHONPATH
# , where $VBOX_SDK is is replaced with the place you unzipped the SDK
# or <trunk>/out/darwin.x86/release/dist/sdk.
#  
#

# this one has to be the first XPCOM related import
import xpcom.vboxxpcom
import xpcom
import xpcom.components
import sys, traceback

from shellcommon import interpret

class LocalManager:
    def getSessionObject(self, vb):
        return xpcom.components.classes["@virtualbox.org/Session;1"].createInstance()

vbox = None
mgr = LocalManager()
try:
    vbox = xpcom.components.classes["@virtualbox.org/VirtualBox;1"].createInstance()
except xpcom.Exception, e:
    print "XPCOM exception: ",e
    traceback.print_exc()
    sys.exit(1)

ctx = {'mgr':mgr, 'vb':vbox, 'ifaces':xpcom.components.interfaces, 'remote':False}

interpret(ctx)

del vbox
