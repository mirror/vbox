#!/usr/bin/env python

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

    class InterfacesWrapper:
            def __init__(self):
                self.__dict__['_rootFake'] = PlatformMSCOM.ConstantFake(None, None)

            def __getattr__(self, a):
                if a.startswith("__"):
                    raise AttributeError
                try:
                    return win32com.client.constants.__getattr__(a)
                except AttributeError,e:
                    return self.__dict__['_rootFake'].__getattr__(a)

    def makeFullName(fake, attr):
            name = fake._name
            parent = fake._parent
            while parent != None:
                if parent._name is not None:
                    name = parent._name+'_'+name
                parent = parent._parent
                
            if name is not None:
                name += "_" + attr
            else:
                name = attr
            return name

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
            win32com.client.gencache.EnsureDispatch('VirtualBox.Session')

    def getSessionObject(self):
        import win32com
        from win32com.client import Dispatch
        return win32com.client.Dispatch("{3C02F46D-C9D2-4f11-A384-53F0CF917214}")

    def getVirtualBox(self):
	import win32com
        from win32com.client import Dispatch
        return win32com.client.Dispatch("{3C02F46D-C9D2-4f11-A384-53F0CF917214}")

    def getConstants(self):
        return self.constants
    
    def getType(self):
        return 'MSCOM'

    def getRemote(self):
        return False

    def getArray(self, obj, field):
        return obj.__getattr__(field)


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
            self.constants = self.platform.getConstants()
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
