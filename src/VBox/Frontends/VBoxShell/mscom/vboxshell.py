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
        win32com.client.gencache.EnsureDispatch('VirtualBox.Session') 
except Exception,e:
    print "COM exception: ",e
    traceback.print_exc()
    sys.exit(1)

class ConstantFake:
  def __init__(self, parent, name):
        self.__dict__['_parent'] = parent
        self.__dict__['_name'] = name
        try:
             self.__dict__['_depth']=parent.__dict__['_depth']+1
        except:
             self.__dict__['_depth']=0
        self.__dict__['_klazz'] = self.__class__
        if self.__dict__['_depth'] > 4:
                raise AttributeError

  def __getattr__(self, attr):
    if attr.startswith("__"):
        raise AttributeError
        
    try:
        n = makeFullName(self) + "_" + attr
        v = win32com.client.constants.__getattr__(n)
        return v
    except AttributeError,e:
        return ConstantFake(self, attr)

def makeFullName(fake):
        name = fake._name
        parent = fake._parent
        while parent != None:
                name   = parent._name+'_'+name
                parent = parent._parent
        return name


class InterfacesWrapper:
  """COM constants name translator
  """
  def __init__(self):
    pass

  def __getattr__(self, a):
    try:
        return win32com.client.constants.__getattr__(a)
    except AttributeError,e:
        if a.startswith("__"):
                raise e
        return ConstantFake(None, a)

ctx = {'mgr':mgr, 'vb':vbox, 'ifaces':InterfacesWrapper(),
       'remote':False, 'type':'mscom' }

interpret(ctx)

del vbox
