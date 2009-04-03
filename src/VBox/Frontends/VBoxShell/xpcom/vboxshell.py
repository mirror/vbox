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
#  export VBOX_PROGRAM_PATH=/opt/VirtualBox-2.0.0/
# To allow Python find modules
#  export PYTHONPATH=../:$VBOX_PROGRAM_PATH
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

ctx = {'mgr':mgr, 'vb':vbox, 'ifaces':xpcom.components.interfaces, 
       'remote':False, 'perf':PerfCollector(vbox)}

interpret(ctx)

del vbox
