#!/usr/bin/python

#########################################################################################
#
# This  Python program implements interactive script access to VBox services
#
# To run this example you need to have:
# - Python ZSI installed (see http://pywebsvcs.sourceforge.net/zsi.html)
# - VirtualBox webservice Python wrappers visible
#   (such as PYTHONPATH=/opt/VirtualBox/sdk/bindings/webservice/python/lib)
# - VirtualBox webservice running (vboxwebsrv) on the local machine, or whatever WSDL says, for interactive sessions disable object watchdog with -t 0
#
######################################################################################### 
from VirtualBox_services import *
from VirtualBox_wrappers import *

import sys, traceback
from shellcommon import interpret, PerfCollector

mgr = IWebsessionManager()
vbox = None

try:
   vbox = mgr.logon("test", "test")
except Exception, e:
   print "Cannot log in:", e
   traceback.print_exc()
   print "***********************************************"
   print "Have you started vboxwebsrv: 'vboxwebsrv -t 0'?"
   print "***********************************************"
   sys.exit(1)

ctx = {'mgr':mgr, 'vb':vbox, 'ifaces': g_reflectionInfo, 'remote': True,
       'type':'ws'}

interpret(ctx)

mgr.logoff(vbox)
