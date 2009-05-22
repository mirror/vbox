#
# Copyright (C) 2009 Sun Microsystems, Inc.
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
# Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
# Clara, CA 95054 USA or visit http://www.sun.com if you need
# additional information or have any questions.
#
import sys,os
import traceback

VboxBinDir = os.environ.get("VBOX_PROGRAM_PATH", None)
VboxSdkDir = os.environ.get("VBOX_SDK_PATH", None)

if VboxBinDir is None:
    # @todo: To be set by installer
    VboxBinDir = "/home/nike/work/ws/out/linux.amd64/debug/bin/"

if VboxSdkDir is None:
    VboxSdkDir = VboxBinDir+"/sdk"

os.environ["VBOX_PROGRAM_PATH"] = VboxBinDir
os.environ["VBOX_SDK_PATH"] = VboxSdkDir
sys.path.append(VboxBinDir)
sys.path.append(VboxSdkDir+"/bindings/glue/python")

from VirtualBox_constants import VirtualBoxReflectionInfo

class PlatformMSCOM:
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
            import win32com
            from win32com.client import constants
                
            if attr.startswith("__"):
                raise AttributeError

            consts = self.__dict__['_consts']
            
            fake = consts.get(attr, None)
            if fake != None:
               return fake  
            try:
               name = self.__dict__['_name']
               parent = self.__dict__['_parent']
               while parent != None:                  
                  if parent._name is not None:
                    name = parent._name+'_'+name
                  parent = parent._parent
                
               if name is not None:
                  name += "_" + attr
               else:
                  name = attr
               print "ask",name
               return win32com.client.constants.__getattr__(name)
            except AttributeError,e:
               fake = PlatformMSCOM.ConstantFake(self, attr)
               consts[attr] = fake
               return fake 


    class InterfacesWrapper:
            def __init__(self):
                self.__dict__['_rootFake'] = PlatformMSCOM.ConstantFake(None, None)

            def __getattr__(self, a):
                import win32com
                from win32com.client import constants
                if a.startswith("__"):
                    raise AttributeError
                try:
                    return win32com.client.constants.__getattr__(a)
                except AttributeError,e:
                    return self.__dict__['_rootFake'].__getattr__(a)

    def __init__(self, params):
            sys.path.append(VboxSdkDir+'/bindings/mscom/python/')
            from win32com import universal
            from win32com.client import gencache, DispatchWithEvents, Dispatch
            from win32com.client import constants, getevents
            import win32com.server.register
            import win32com
            import pythoncom
            import win32api
            self.constants = PlatformMSCOM.InterfacesWrapper()
            #win32com.client.gencache.EnsureDispatch('VirtualBox.Session')
            #win32com.client.gencache.EnsureDispatch('VirtualBox.VirtualBox')

    def getSessionObject(self):
        import win32com
        from win32com.client import Dispatch
	return win32com.client.Dispatch("VirtualBox.Session")

    def getVirtualBox(self):
	import win32com
        from win32com.client import Dispatch
        return win32com.client.Dispatch("VirtualBox.VirtualBox")

    def getConstants(self):
        return self.constants
    
    def getType(self):
        return 'MSCOM'

    def getRemote(self):
        return False

    def getArray(self, obj, field):
        return obj.__getattr__(field)

    def initPerThread(self):
        import pythoncom
        # or with pythoncom.COINIT_APARTMENTTHREADED?
        pythoncom.CoInitializeEx()

    def deinitPerThread(self):
        import pythoncom
        pythoncom.CoUninitialize()


class PlatformXPCOM:
    def __init__(self, params):
        sys.path.append(VboxSdkDir+'/bindings/xpcom/python/')
        import xpcom.vboxxpcom
        import xpcom
        import xpcom.components

    def getSessionObject(self):
        import xpcom.components
        return xpcom.components.classes["@virtualbox.org/Session;1"].createInstance()

    def getVirtualBox(self):
        import xpcom.components
        return xpcom.components.classes["@virtualbox.org/VirtualBox;1"].createInstance()

    def getConstants(self):
        import xpcom.components
        return xpcom.components.interfaces
    
    def getType(self):
        return 'XPCOM'
    
    def getRemote(self):
        return False

    def getArray(self, obj, field):
        return obj.__getattr__('get'+field.capitalize())()

    def initPerThread(self):
        pass

    def deinitPerThread(self):
        pass

class PlatformWEBSERVICE:
    def __init__(self, params):
        sys.path.append(VboxSdkDir+'/bindings/webservice/python/lib')
        import VirtualBox_services
        import VirtualBox_wrappers
        from VirtualBox_wrappers import IWebsessionManager
        from VirtualBox_wrappers import g_port
        from VirtualBox_wrappers import g_reflectionInfo
        self.wsmgr = IWebsessionManager()
        self.port = g_port
        self.constants = g_reflectionInfo
        self.user = ""
        self.password = ""

    def getSessionObject(self):
        return self.wsmgr.getSessionObject()

    def getVirtualBox(self):
        return self.wsmgr.logon(self.user, self.password)

    def getConstants(self):
        from VirtualBox_wrappers import g_reflectionInfo
        return g_reflectionInfo
    
    def getType(self):
        return 'WEBSERVICE'
    
    def getRemote(self):
        return True

    def getArray(self, obj, field):
        return obj.__getattr__(field)
    
    def initPerThread(self):
        pass

    def deinitPerThread(self):
        pass

class SessionManager:
    def __init__(self, mgr):
        self.mgr = mgr

    def getSessionObject(self, vbox):
        return self.mgr.platform.getSessionObject()

class VirtualBoxManager:
    def __init__(self, style, platparams):
        if style is None:
            if sys.platform == 'win32':
                style = "MSCOM"
            else:
                style = "XPCOM"
        try:
            exec "self.platform = Platform"+style+"(platparams)"
            self.vbox = self.platform.getVirtualBox()
            self.mgr = SessionManager(self)
            self.constants = VirtualBoxReflectionInfo()
            self.type = self.platform.getType()
            self.remote = self.platform.getRemote()
        except Exception,e:
            print "init exception: ",e
            traceback.print_exc()
            raise e

    def getArray(self, obj, field):
        return self.platform.getArray(obj, field)       

    def getVirtualBox(self):
        return  self.platform.getVirtualBox()

    def __del__(self):
        if hasattr(self, "vbox"):
            del self.vbox

    def initPerThread(self):
        self.platform.initPerThread()

    def deinitPerThread(self):
        self.platform.deinitPerThread()
