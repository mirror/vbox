#!python
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
#

import sys, os
from win32com import universal
from win32com.client import gencache, DispatchWithEvents, Dispatch
from win32com.client import constants, getevents
import win32com.server.register
import win32com
import pythoncom
import win32api
import traceback

from shellcommon import interpret

class LocalManager:
    def getSessionObject(self, vb):
	return win32com.client.Dispatch("{3C02F46D-C9D2-4f11-A384-53F0CF917214}")

vbox = None
mgr = LocalManager()
try:
	vbox = win32com.client.Dispatch("{B1A7A4F2-47B9-4A1E-82B2-07CCD5323C3F}")
except Exception,e:
    print "COM exception: ",e
    traceback.print_exc()
    sys.exit(1)

# fake constants, while get resolved constants issues for real
# win32com.client.constants doesn't work for some reasons
class DummyInterfaces: pass
class SessionState:pass

DummyInterfaces.SessionState=SessionState()
DummyInterfaces.SessionState.Open = 2

ctx = {'mgr':mgr, 'vb':vbox, 'ifaces':DummyInterfaces(),
#'ifaces':win32com.client.constants, 
       'remote':False, 'type':'mscom' }

interpret(ctx)

del vbox
