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
################################################################################

import os,sys
import traceback

from vboxapi import VirtualBoxManager
from shellcommon import interpret

def main(argv):    
    style = None
    if len(argv) > 1:
        if argv[1] == "-w":
            style = "WEBSERVICE"
        
    g_virtualBoxManager = VirtualBoxManager(style, None)
    ctx = {'global':g_virtualBoxManager,
           'mgr':g_virtualBoxManager.mgr,
           'vb':g_virtualBoxManager.vbox, 
           'ifaces':g_virtualBoxManager.constants,
           'remote':g_virtualBoxManager.remote, 
           'type':g_virtualBoxManager.type
           }
    interpret(ctx)
    del g_virtualBoxManager

if __name__ == '__main__':
    main(sys.argv)
