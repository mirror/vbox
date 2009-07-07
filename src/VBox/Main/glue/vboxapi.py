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
    # Will be set by the installer
    VboxBinDir = "%VBOX_INSTALL_PATH%"

if VboxSdkDir is None:
    VboxSdkDir = VboxBinDir+"/sdk"

os.environ["VBOX_PROGRAM_PATH"] = VboxBinDir
os.environ["VBOX_SDK_PATH"] = VboxSdkDir
sys.path.append(VboxBinDir)

from VirtualBox_constants import VirtualBoxReflectionInfo

class PerfCollector:
    """ This class provides a wrapper over IPerformanceCollector in order to
    get more 'pythonic' interface.

    To begin collection of metrics use setup() method.

    To get collected data use query() method.

    It is possible to disable metric collection without changing collection
    parameters with disable() method. The enable() method resumes metric
    collection.
    """

    def __init__(self, vb):
        """ Initializes the instance.

        Pass an instance of IVirtualBox as parameter.
        """
        self.collector = vb.performanceCollector

    def setup(self, names, objects, period, nsamples):
        """ Discards all previously collected values for the specified
        metrics, sets the period of collection and the number of retained
        samples, enables collection.
        """
        self.collector.setupMetrics(names, objects, period, nsamples)

    def enable(self, names, objects):
        """ Resumes metric collection for the specified metrics.
        """
        self.collector.enableMetrics(names, objects)

    def disable(self, names, objects):
        """ Suspends metric collection for the specified metrics.
        """
        self.collector.disableMetrics(names, objects)

    def query(self, names, objects):
        """ Retrieves collected metric values as well as some auxiliary
        information. Returns an array of dictionaries, one dictionary per
        metric. Each dictionary contains the following entries:
        'name': metric name
        'object': managed object this metric associated with
        'unit': unit of measurement
        'scale': divide 'values' by this number to get float numbers
        'values': collected data
        'values_as_string': pre-processed values ready for 'print' statement
        """
        (values, names_out, objects_out, units, scales, sequence_numbers,
            indices, lengths) = self.collector.queryMetricsData(names, objects)
        out = []
        for i in xrange(0, len(names_out)):
            scale = int(scales[i])
            if scale != 1:
                fmt = '%.2f%s'
            else:
                fmt = '%d %s'
            out.append({
                'name':str(names_out[i]),
                'object':str(objects_out[i]),
                'unit':str(units[i]),
                'scale':scale,
                'values':[int(values[j]) for j in xrange(int(indices[i]), int(indices[i])+int(lengths[i]))],
                'values_as_string':'['+', '.join([fmt % (int(values[j])/scale, units[i]) for j in xrange(int(indices[i]), int(indices[i])+int(lengths[i]))])+']'
            })
        return out

def ComifyName(name):
    return name[0].capitalize()+name[1:]    

_COMForward = { 'getattr' : None,
                'setattr' : None}
          
def CustomGetAttr(self, attr):
    # fastpath
    if self.__class__.__dict__.get(attr) != None:
        return self.__class__.__dict__.get(attr)

    # try case-insensitivity workaround for class attributes (COM methods)
    for k in self.__class__.__dict__.keys():
        if k.lower() == attr.lower():
            self.__class__.__dict__[attr] = self.__class__.__dict__[k]
            return getattr(self, k)
    try:
        return _COMForward['getattr'](self,ComifyName(attr))
    except AttributeError:
        return _COMForward['getattr'](self,attr)

def CustomSetAttr(self, attr, value):
    try:
        return _COMForward['setattr'](self, ComifyName(attr), value)
    except AttributeError:
        return _COMForward['setattr'](self, attr, value)

class PlatformMSCOM:
    # Class to fake access to constants in style of foo.bar.boo
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

    VBOX_TLB_GUID  = '{46137EEC-703B-4FE5-AFD4-7C9BBBBA0259}'
    VBOX_TLB_LCID  = 0
    VBOX_TLB_MAJOR = 1
    VBOX_TLB_MINOR = 0
    
    def __init__(self, params):
            from win32com import universal
            from win32com.client import gencache, DispatchBaseClass
            from win32com.client import constants, getevents
            import win32com
            import pythoncom
            import win32api
            self.constants = PlatformMSCOM.InterfacesWrapper()
            from win32con import DUPLICATE_SAME_ACCESS
            from win32api import GetCurrentThread,GetCurrentThreadId,DuplicateHandle,GetCurrentProcess
            pid = GetCurrentProcess()
            self.tid = GetCurrentThreadId()
            handle = DuplicateHandle(pid, GetCurrentThread(), pid, 0, 0, DUPLICATE_SAME_ACCESS)
            self.handles = []
            self.handles.append(handle)
            _COMForward['getattr'] = DispatchBaseClass.__dict__['__getattr__']
            DispatchBaseClass.__dict__['__getattr__'] = CustomGetAttr            
            _COMForward['setattr'] = DispatchBaseClass.__dict__['__setattr__']
            DispatchBaseClass.__dict__['__setattr__'] = CustomSetAttr            
            win32com.client.gencache.EnsureDispatch('VirtualBox.Session')
            win32com.client.gencache.EnsureDispatch('VirtualBox.VirtualBox')

    def getSessionObject(self, vbox):
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
        pythoncom.CoInitializeEx(0)

    def deinitPerThread(self):
        import pythoncom
        pythoncom.CoUninitialize()

    def createCallback(self, iface, impl, arg):
        d = {}
        d['BaseClass'] = impl
        d['arg'] = arg
        d['tlb_guid'] = PlatformMSCOM.VBOX_TLB_GUID
        str = ""
        str += "import win32com.server.util\n"
        #str += "import win32com.server.register\n"
        #str += "from win32com import universal\n"
        str += "import pythoncom\n"
        #str += "universal.RegisterInterfaces(tlb_guid, 0, 1, 0, ['"+iface+"'])\n"

        str += "class "+iface+"Impl(BaseClass):\n"
        str += "   _com_interfaces_ = ['"+iface+"']\n"
        str += "   _typelib_guid_ = tlb_guid\n"
        str += "   _typelib_version_ = 1, 0\n"
        #str += "   _reg_clsctx_ = pythoncom.CLSCTX_LOCAL_SERVER\n"
        #str += "   _reg_clsid_ = '{F21202A2-959A-4149-B1C3-68B9013F3335}'\n"
        #str += "   _reg_progid_ = 'VirtualBox."+iface+"Impl'\n"
        #str += "   _reg_desc_ = 'Generated callback implementation class'\n"
        #str += "   _reg_policy_spec_ = 'win32com.server.policy.EventHandlerPolicy'\n"

        # generate capitalized version of callbacks - that's how Python COM
        # looks them up on Windows
        for m in dir(impl):
           if m.startswith("on"):      
             str += "   "+m[0].capitalize()+m[1:]+"=BaseClass."+m+"\n"

        str += "   def __init__(self): BaseClass.__init__(self, arg)\n"
        #str += "win32com.server.register.UseCommandLine("+iface+"Impl)\n"

        str += "result = win32com.server.util.wrap("+iface+"Impl())\n"
        exec (str,d,d)
        return d['result']

    def waitForEvents(self, timeout):
        from win32api import GetCurrentThreadId
        from win32event import MsgWaitForMultipleObjects, \
                               QS_ALLINPUT, WAIT_TIMEOUT, WAIT_OBJECT_0
        from pythoncom import PumpWaitingMessages

        if (self.tid != GetCurrentThreadId()):
            raise Exception("wait for events from the same thread you inited!")

        rc = MsgWaitForMultipleObjects(self.handles, 0, timeout, QS_ALLINPUT)
        if rc >= WAIT_OBJECT_0 and rc < WAIT_OBJECT_0+len(self.handles):
            # is it possible?
            pass
        elif rc==WAIT_OBJECT_0 + len(self.handles):
            # Waiting messages
            PumpWaitingMessages()
        else:
            # Timeout
            pass

    def deinit(self):
        import pythoncom
        from win32file import CloseHandle

        for h in self.handles:
           if h is not None:
              CloseHandle(h)
        self.handles = None
        pythoncom.CoUninitialize()
        pass

    def getPerfCollector(self, vbox):
        return PerfCollector(vbox)


class PlatformXPCOM:
    def __init__(self, params):
        sys.path.append(VboxSdkDir+'/bindings/xpcom/python/')
        import xpcom.vboxxpcom
        import xpcom
        import xpcom.components

    def getSessionObject(self, vbox):
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

    def createCallback(self, iface, impl, arg):
        d = {}
        d['BaseClass'] = impl
        d['arg'] = arg
        str = ""
        str += "import xpcom.components\n"
        str += "class "+iface+"Impl(BaseClass):\n"
        str += "   _com_interfaces_ = xpcom.components.interfaces."+iface+"\n"
        str += "   def __init__(self): BaseClass.__init__(self, arg)\n"
        str += "result = "+iface+"Impl()\n"
        exec (str,d,d)
        return d['result']

    def waitForEvents(self, timeout):
        import xpcom
        xpcom._xpcom.WaitForEvents(timeout)

    def deinit(self):
        import xpcom
        xpcom._xpcom.DeinitCOM()

    def getPerfCollector(self, vbox):
        return PerfCollector(vbox)

class PlatformWEBSERVICE:
    def __init__(self, params):
        sys.path.append(VboxSdkDir+'/bindings/webservice/python/lib')
        import VirtualBox_services
        import VirtualBox_wrappers
        from VirtualBox_wrappers import IWebsessionManager2
        if params is not None:
            self.user = params.get("user", "")
            self.password = params.get("password", "")
            self.url = params.get("url", "")
        else:
            self.user = ""
            self.password = ""
            self.url = None
        self.wsmgr = IWebsessionManager2(self.url)

    def getSessionObject(self, vbox):
        return self.wsmgr.getSessionObject(vbox)

    def getVirtualBox(self):
        self.vbox = self.wsmgr.logon(self.user, self.password)

    def getConstants(self):
        return None

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

    def createCallback(self, iface, impl, arg):
        raise Exception("no callbacks for webservices")

    def waitForEvents(self, timeout):
        # Webservices cannot do that yet
        pass

    def deinit(self):
        try:
            if self.vbox is not None:
                self.wsmg.logoff(self.vbox)
                self.vbox = None
        except:
            pass

    def getPerfCollector(self, vbox):
        return PerfCollector(vbox)    

class SessionManager:
    def __init__(self, mgr):
        self.mgr = mgr

    def getSessionObject(self, vbox):
        return self.mgr.platform.getSessionObject(vbox)

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
            self.style = style            
        except Exception,e:
            print "init exception: ",e
            traceback.print_exc()
            raise e

    def getArray(self, obj, field):
        return self.platform.getArray(obj, field)

    def getVirtualBox(self):
        return  self.platform.getVirtualBox()

    def __del__(self):
        self.deinit()

    def deinit(self):
        if hasattr(self, "vbox"):
            del self.vbox
            self.vbox = None
        if hasattr(self, "platform"):
            self.platform.deinit()
            self.platform = None

    def initPerThread(self):
        self.platform.initPerThread()

    def openMachineSession(self, machineId):
         session = self.mgr.getSessionObject(self.vbox)
         self.vbox.openSession(session, machineId)
         return session

    def closeMachineSession(self, session):
        session.close()

    def deinitPerThread(self):
        self.platform.deinitPerThread()

    def createCallback(self, iface, impl, arg):
        return self.platform.createCallback(iface, impl, arg)

    def waitForEvents(self, timeout):
        return self.platform.waitForEvents(timeout)

    def getPerfCollector(self, vbox):
        return PerfCollector(vbox)       
