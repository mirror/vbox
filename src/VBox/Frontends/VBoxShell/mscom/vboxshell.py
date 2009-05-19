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

from shellcommon import interpret

class LocalManager:
	def getSessionObject(self, vb):
		return win32com.client.Dispatch("{3C02F46D-C9D2-4f11-A384-53F0CF917214}")

class ConstantFake:
        def __init__(self, parent, name):
                self.__dict__['_parent'] = parent
		self.__dict__['_name'] = name
		self.__dict__['_consts'] = {}
        	try:
             		self.__dict__['_depth']=parent.__dict__['_depth']+1
        	except:
             		self.__dict__['_depth']=0
        	if self.__dict__['_depth'] > 4:
			raise AttributeError

	def __getattr__(self, attr):
    		if attr.startswith("__"):
         		raise AttributeError

    		consts = self.__dict__['_consts']
    		fake = consts.get(attr, None)
    		if fake != None:
         		return fake  
    		try:
			name = makeFullName(self, attr)
			return win32com.client.constants.__getattr__(name)
		except AttributeError,e:
			fake = ConstantFake(self, attr)
			consts[attr] = fake
			return fake  	

def makeFullName(fake, attr):
	name = fake._name
	parent = fake._parent
	while parent != None:
		if parent._name is not None:
			name = parent._name+'_'+name
		parent = parent._parent

	if name:
                name += "_" + attr
        else:
                name = attr
	return name


class InterfacesWrapper:
	def __init__(self):
    		self.__dict__['_rootFake'] = ConstantFake(None, None)

  	def __getattr__(self, a):
    		if a.startswith("__"):
        		raise AttributeError
    		try:
       			return win32com.client.constants.__getattr__(a)
    		except AttributeError,e:
                	return self.__dict__['_rootFake'].__getattr__(a)


vbox = None
mgr = LocalManager()
try:
        win32com.client.gencache.EnsureDispatch('VirtualBox.Session') 
        vbox = win32com.client.Dispatch("{B1A7A4F2-47B9-4A1E-82B2-07CCD5323C3F}")
except Exception,e:
        print "COM exception: ",e
        traceback.print_exc()
        sys.exit(1)


ctx = {'mgr':mgr, 'vb':vbox, 'ifaces':InterfacesWrapper(),
       'remote':False, 'type':'mscom' }

interpret(ctx)

del vbox
