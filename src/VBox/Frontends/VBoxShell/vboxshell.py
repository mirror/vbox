#!/usr/bin/python
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
import shlex
import time

# Simple implementation of IConsoleCallback, one can use it as skeleton 
# for custom implementations
class GuestMonitor:
    def __init__(self, mach):
        self.mach = mach

    def onMousePointerShapeChange(self, visible, alpha, xHot, yHot, width, height, shape):
        print  "%s: onMousePointerShapeChange: visible=%d" %(self.mach.name, visible) 
    def onMouseCapabilityChange(self, supportsAbsolute, needsHostCursor):
        print  "%s: onMouseCapabilityChange: needsHostCursor=%d" %(self.mach.name, needsHostCursor)

    def onKeyboardLedsChange(self, numLock, capsLock, scrollLock):
        print  "%s: onKeyboardLedsChange capsLock=%d"  %(self.mach.name, capsLock)

    def onStateChange(self, state):
        print  "%s: onStateChange state=%d" %(self.mach.name, state)

    def onAdditionsStateChange(self):
        print  "%s: onAdditionsStateChange" %(self.mach.name)

    def onDVDDriveChange(self):
        print  "%s: onDVDDriveChange" %(self.mach.name)

    def onFloppyDriveChange(self):
        print  "%s: onFloppyDriveChange" %(self.mach.name)

    def onNetworkAdapterChange(self, adapter):
        print  "%s: onNetworkAdapterChange" %(self.mach.name)

    def onSerialPortChange(self, port):
        print  "%s: onSerialPortChange" %(self.mach.name)

    def onParallelPortChange(self, port):
        print  "%s: onParallelPortChange" %(self.mach.name)

    def onStorageControllerChange(self):
        print  "%s: onStorageControllerChange" %(self.mach.name)

    def onVRDPServerChange(self):
        print  "%s: onVRDPServerChange" %(self.mach.name)

    def onUSBControllerChange(self):
        print  "%s: onUSBControllerChange" %(self.mach.name)

    def onUSBDeviceStateChange(self, device, attached, error):
        print  "%s: onUSBDeviceStateChange" %(self.mach.name)

    def onSharedFolderChange(self, scope):
        print  "%s: onSharedFolderChange" %(self.mach.name)

    def onRuntimeError(self, fatal, id, message):
        print  "%s: onRuntimeError fatal=%d message=%s" %(self.mach.name, fatal, message)

    def onCanShowWindow(self):
        print  "%s: onCanShowWindow" %(self.mach.name)
        return True

    def onShowWindow(self, winId):
        print  "%s: onShowWindow: %d" %(self.mach.name, winId)

class VBoxMonitor:
    def __init__(self, vbox):
        self.vbox = vbox
        pass

    def onMachineStateChange(self, id, state):
        print "onMachineStateChange: %s %d" %(id, state)

    def onMachineDataChange(self,id):
        print "onMachineDataChange: %s" %(id)
    
    def onExtraDataCanChange(self, id, key, value):
        print "onExtraDataCanChange: %s %s=>%s" %(id, key, value)
        return True, ""

    def onExtraDataChange(self, id, key, value):
        print "onExtraDataChange: %s %s=>%s" %(id, key, value)

    def onMediaRegistred(self, id, type, registred):
        print "onMediaRegistred: %s" %(id)

    def onMachineRegistred(self, id, registred):
        print "onMachineRegistred: %s" %(id)

    def onSessionStateChange(self, id, state):
        print "onSessionStateChange: %s %d" %(id, state)

    def onSnapshotTaken(self, mach, id):
        print "onSnapshotTaken: %s %s" %(mach, id)

    def onSnapshotDiscarded(self, mach, id):
        print "onSnapshotDiscarded: %s %s" %(mach, id)

    def onSnapshotChange(self, mach, id):
        print "onSnapshotChange: %s %s" %(mach, id)

    def onGuestPropertyChange(self, id, name, newValue, flags):
        print "onGuestPropertyChange: %s: %s=%s" %(id, name, newValue)

g_hasreadline = 1
try:
    import readline
    import rlcompleter
except:
    g_hasreadline = 0


if g_hasreadline:
  class CompleterNG(rlcompleter.Completer):
    def __init__(self, dic, ctx):
        self.ctx = ctx
        return rlcompleter.Completer.__init__(self,dic)

    def complete(self, text, state):
        """
        taken from:
        http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/496812
        """
        if text == "":
            return ['\t',None][state]
        else:
            return rlcompleter.Completer.complete(self,text,state)

    def global_matches(self, text):
        """
        Compute matches when text is a simple name.
        Return a list of all names currently defined
        in self.namespace that match.
        """

        matches = []
        n = len(text)

        for list in [ self.namespace ]:
            for word in list:
                if word[:n] == text:
                    matches.append(word)


        try:
            for m in getMachines(self.ctx):
                # although it has autoconversion, we need to cast
                # explicitly for subscripts to work
                word = str(m.name)
                if word[:n] == text:
                    matches.append(word)
                word = str(m.id)
                if word[0] == '{':
                    word = word[1:-1]
                if word[:n] == text:
                    matches.append(word)
        except Exception,e:
            traceback.print_exc()
            print e

        return matches


def autoCompletion(commands, ctx):
  if  not g_hasreadline:
      return

  comps = {}
  for (k,v) in commands.items():
      comps[k] = None
  completer = CompleterNG(comps, ctx)
  readline.set_completer(completer.complete)
  readline.parse_and_bind("tab: complete")

g_verbose = True

def split_no_quotes(s):
    return shlex.split(s)

def createVm(ctx,name,kind,base):
    mgr = ctx['mgr']
    vb = ctx['vb']
    mach = vb.createMachine(name, kind, base,
                            "00000000-0000-0000-0000-000000000000")
    mach.saveSettings()
    print "created machine with UUID",mach.id
    vb.registerMachine(mach)

def removeVm(ctx,mach):
    mgr = ctx['mgr']
    vb = ctx['vb']
    id = mach.id
    print "removing machine ",mach.name,"with UUID",id
    session = ctx['global'].openMachineSession(id)
    mach=session.machine
    for d in mach.getHardDiskAttachments():
        mach.detachHardDisk(d.controller, d.port, d.device)
    ctx['global'].closeMachineSession(session)
    mach = vb.unregisterMachine(id)
    if mach:
         mach.deleteSettings()

def startVm(ctx,mach,type):
    mgr = ctx['mgr']
    vb = ctx['vb']
    perf = ctx['perf']
    session = mgr.getSessionObject(vb)
    uuid = mach.id
    progress = vb.openRemoteSession(session, uuid, type, "")
    progress.waitForCompletion(-1)
    completed = progress.completed
    rc = int(progress.resultCode)
    print "Completed:", completed, "rc:",hex(rc&0xffffffff)
    if rc == 0:
        # we ignore exceptions to allow starting VM even if
        # perf collector cannot be started
        if perf:
          try:
            perf.setup(['*'], [mach], 10, 15)
          except Exception,e:
            print e
            if g_verbose:
                traceback.print_exc()
            pass
         # if session not opened, close doesn't make sense
        session.close()
    else:
        # Not yet implemented error string query API for remote API
        if not ctx['remote']:
            print session.QueryErrorObject(rc)

def getMachines(ctx, invalidate = False):
    if ctx['vb'] is not None:
        if ctx['_machlist'] is None or invalidate:
            ctx['_machlist'] = ctx['global'].getArray(ctx['vb'], 'machines')
        return ctx['_machlist']
    else:
        return []

def asState(var):
    if var:
        return 'on'
    else:
        return 'off'

def guestStats(ctx,mach):
    if not ctx['perf']:
        return
    for metric in ctx['perf'].query(["*"], [mach]):
        print metric['name'], metric['values_as_string']

def guestExec(ctx, machine, console, cmds):
    exec cmds

def monitorGuest(ctx, machine, console, dur):
    cb = ctx['global'].createCallback('IConsoleCallback', GuestMonitor, machine)
    console.registerCallback(cb)
    if dur == -1:
        # not infinity, but close enough
        dur = 100000
    try:
        end = time.time() + dur
        while  time.time() < end:
            ctx['global'].waitForEvents(500)
    # We need to catch all exceptions here, otherwise callback will never be unregistered
    except:
        pass    
    console.unregisterCallback(cb)


def monitorVbox(ctx, dur):
    vbox = ctx['vb']
    cb = ctx['global'].createCallback('IVirtualBoxCallback', VBoxMonitor, vbox)
    vbox.registerCallback(cb)
    if dur == -1:
        # not infinity, but close enough
        dur = 100000
    try:
        end = time.time() + dur
        while  time.time() < end:
            ctx['global'].waitForEvents(500)
    # We need to catch all exceptions here, otherwise callback will never be unregistered
    except:
        if g_verbose:
                traceback.print_exc()
    vbox.unregisterCallback(cb)

def cmdExistingVm(ctx,mach,cmd,args):
    mgr=ctx['mgr']
    vb=ctx['vb']
    session = mgr.getSessionObject(vb)
    uuid = mach.id
    try:
        progress = vb.openExistingSession(session, uuid)
    except Exception,e:
        print "Session to '%s' not open: %s" %(mach.name,e)
        if g_verbose:
            traceback.print_exc()
        return
    if session.state != ctx['ifaces'].SessionState_Open:
        print "Session to '%s' in wrong state: %s" %(mach.name, session.state)
        return
    # unfortunately IGuest is suppressed, thus WebServices knows not about it
    # this is an example how to handle local only functionality
    if ctx['remote'] and cmd == 'stats2':
        print 'Trying to use local only functionality, ignored'
        return
    console=session.console    
    ops={'pause':           lambda: console.pause(),
         'resume':          lambda: console.resume(),
         'powerdown':       lambda: console.powerDown(),
         'powerbutton':     lambda: console.powerButton(),
         'stats':           lambda: guestStats(ctx, mach),
         'guest':           lambda: guestExec(ctx, mach, console, args),
         'monitorGuest':    lambda: monitorGuest(ctx, mach, console, args),
         'save':            lambda: console.saveState().waitForCompletion(-1)
         }
    try:
        ops[cmd]()
    except Exception, e:
        print 'failed: ',e
        if g_verbose:
            traceback.print_exc()

    session.close()

# can cache known machines, if needed
def machById(ctx,id):
    mach = None
    for m in getMachines(ctx):
        if m.name == id:
            mach = m
            break
        mid = str(m.id)
        if mid[0] == '{':
            mid = mid[1:-1]
        if mid == id:
            mach = m
            break
    return mach

def argsToMach(ctx,args):
    if len(args) < 2:
        print "usage: %s [vmname|uuid]" %(args[0])
        return None
    id = args[1]
    m = machById(ctx, id)
    if m == None:
        print "Machine '%s' is unknown, use list command to find available machines" %(id)
    return m

def helpCmd(ctx, args):
    if len(args) == 1:
        print "Help page:"
        names = commands.keys()
        names.sort()
        for i in names:
            print "   ",i,":", commands[i][0]
    else:
        c = commands.get(args[1], None)
        if c == None:
            print "Command '%s' not known" %(args[1])
        else:
            print "   ",args[1],":", c[0]
    return 0

def listCmd(ctx, args):
    for m in getMachines(ctx, True):
        print "Machine '%s' [%s], state=%s" %(m.name,m.id,m.sessionState)
    return 0

def getControllerType(type):
    if type == 0:
        return "Null"
    elif  type == 1:
        return "LsiLogic"
    elif type == 2:
        return "BusLogic"
    elif type == 3:
        return "IntelAhci"
    elif type == 4:
        return "PIIX3"
    elif type == 5:
        return "PIIX4"
    elif type == 6:
        return "ICH6"
    else:
        return "Unknown"

def infoCmd(ctx,args):
    if (len(args) < 2):
        print "usage: info [vmname|uuid]"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    os = ctx['vb'].getGuestOSType(mach.OSTypeId)
    print " One can use setvar <mach> <var> <value> to change variable, using name in []."
    print "  Name [name]: %s" %(mach.name)
    print "  ID [n/a]: %s" %(mach.id)
    print "  OS Type [n/a]: %s" %(os.description)
    print
    print "  CPUs [CPUCount]: %d" %(mach.CPUCount)
    print "  RAM [memorySize]: %dM" %(mach.memorySize)
    print "  VRAM [VRAMSize]: %dM" %(mach.VRAMSize)
    print "  Monitors [monitorCount]: %d" %(mach.monitorCount)
    print
    print "  Clipboard mode [clipboardMode]: %d" %(mach.clipboardMode)
    print "  Machine status [n/a]: %d" % (mach.sessionState)
    print
    bios = mach.BIOSSettings
    print "  ACPI [BIOSSettings.ACPIEnabled]: %s" %(asState(bios.ACPIEnabled))
    print "  APIC [BIOSSettings.IOAPICEnabled]: %s" %(asState(bios.IOAPICEnabled))
    print "  PAE [PAEEnabled]: %s" %(asState(int(mach.PAEEnabled)))
    print "  Hardware virtualization [HWVirtExEnabled]: " + asState(mach.HWVirtExEnabled)
    print "  VPID support [HWVirtExVPIDEnabled]: " + asState(mach.HWVirtExVPIDEnabled)
    print "  Hardware 3d acceleration[accelerate3DEnabled]: " + asState(mach.accelerate3DEnabled)
    print "  Nested paging [HWVirtExNestedPagingEnabled]: " + asState(mach.HWVirtExNestedPagingEnabled)
    print "  Last changed [n/a]: " + time.asctime(time.localtime(long(mach.lastStateChange)/1000))
    print "  VRDP server [VRDPServer.enabled]: %s" %(asState(mach.VRDPServer.enabled))

    controllers = ctx['global'].getArray(mach, 'storageControllers')
    if controllers:
        print
        print "  Controllers:"
    for controller in controllers:
        print "    %s %s bus: %d" % (controller.name, getControllerType(controller.controllerType), controller.bus)

    disks = ctx['global'].getArray(mach, 'hardDiskAttachments')
    if disks:
        print
        print "  Hard disk(s):"
    for disk in disks:
        print "    Controller: %s port: %d device: %d:" % (disk.controller, disk.port, disk.device)
        hd = disk.hardDisk
        print "    id: %s" %(hd.id)
        print "    location: %s" %(hd.location)
        print "    name: %s"  %(hd.name)
        print "    format: %s"  %(hd.format)
        print

    dvd = mach.DVDDrive
    if dvd.getHostDrive():
        hdvd = dvd.getHostDrive()
        print "  DVD:"
        print "    Host disk: %s" %(hdvd.name)
        print

    if dvd.getImage():
        vdvd = dvd.getImage()
        print "  DVD:"
        print "    Image at: %s" %(vdvd.location)
        print "    Size: %s" %(vdvd.size)
        print

    floppy = mach.floppyDrive
    if floppy.getHostDrive():
        hfloppy = floppy.getHostDrive()
        print "  Floppy:"
        print "    Host disk: %s" %(hfloppy.name)
        print

    if floppy.getImage():
        vfloppy = floppy.getImage()
        print "  Floppy:"
        print "    Image at: %s" %(vfloppy.location)
        print "    Size: %s" %(vfloppy.size)
        print

    return 0

def startCmd(ctx, args):
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    if len(args) > 2:
        type = args[2]
    else:
        type = "gui"
    startVm(ctx, mach, type)
    return 0

def createCmd(ctx, args):
    if (len(args) < 3 or len(args) > 4):
        print "usage: create name ostype <basefolder>"
        return 0
    name = args[1]
    oskind = args[2]
    if len(args) == 4:
        base = args[3]
    else:
        base = ''
    try:
         ctx['vb'].getGuestOSType(oskind)
    except Exception, e:
        print 'Unknown OS type:',oskind
        return 0
    createVm(ctx, name, oskind, base)
    return 0

def removeCmd(ctx, args):
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    removeVm(ctx, mach)
    return 0

def pauseCmd(ctx, args):
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'pause', '')
    return 0

def powerdownCmd(ctx, args):
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'powerdown', '')
    return 0

def powerbuttonCmd(ctx, args):
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'powerbutton', '')
    return 0

def resumeCmd(ctx, args):
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'resume', '')
    return 0

def saveCmd(ctx, args):
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'save', '')
    return 0

def statsCmd(ctx, args):
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'stats', '')
    return 0

def guestCmd(ctx, args):
    if (len(args) < 3):
        print "usage: guest name commands"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'guest', ' '.join(args[2:]))
    return 0

def setvarCmd(ctx, args):
    if (len(args) < 4):
        print "usage: setvar [vmname|uuid] expr value"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    session = ctx['global'].openMachineSession(mach.id)
    mach = session.machine
    expr = 'mach.'+args[2]+' = '+args[3]
    print "Executing",expr
    try:
        exec expr
    except Exception, e:
        print 'failed: ',e
        if g_verbose:
            traceback.print_exc()
    mach.saveSettings()
    session.close()
    return 0

def quitCmd(ctx, args):
    return 1

def aliasCmd(ctx, args):
    if (len(args) == 3):
        aliases[args[1]] = args[2]
        return 0
    
    for (k,v) in aliases.items():
        print "'%s' is an alias for '%s'" %(k,v)
    return 0

def verboseCmd(ctx, args):
    global g_verbose
    g_verbose = not g_verbose
    return 0

def hostCmd(ctx, args):
   host = ctx['vb'].host
   cnt = host.processorCount
   print "Processor count:",cnt
   for i in range(0,cnt):
      print "Processor #%d speed: %dMHz" %(i,host.getProcessorSpeed(i))

   if ctx['perf']:
     for metric in ctx['perf'].query(["*"], [host]):
       print metric['name'], metric['values_as_string']

   return 0

def monitorGuestCmd(ctx, args):
    if (len(args) < 2):
        print "usage: monitorGuest name (duration)"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    dur = 5
    if len(args) > 2:
        dur = float(args[2])
    cmdExistingVm(ctx, mach, 'monitorGuest', dur)
    return 0

def monitorVboxCmd(ctx, args):
    if (len(args) > 2):
        print "usage: monitorVbox (duration)"
        return 0
    dur = 5
    if len(args) > 1:
        dur = float(args[1])
    monitorVbox(ctx, dur)
    return 0

def getAdapterType(ctx, type):
    if (type == ctx['global'].constants.NetworkAdapterType_Am79C970A or
        type == ctx['global'].constants.NetworkAdapterType_Am79C973):
        return "pcnet"
    elif (type == ctx['global'].constants.NetworkAdapterType_I82540EM or
          type == ctx['global'].constants.NetworkAdapterType_I82545EM or
          type == ctx['global'].constants.NetworkAdapterType_I82543GC):
        return "e1000"
    elif (type == ctx['global'].constants.NetworkAdapterType_Null):
        return None
    else:
        raise Exception("Unknown adapter type: "+type)    
    

def portForwardCmd(ctx, args):
    if (len(args) != 5):
        print "usage: portForward <vm> <adapter> <hostPort> <guestPort>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    adapterNum = int(args[2])
    hostPort = int(args[3])
    guestPort = int(args[4])
    proto = "TCP"
    session = ctx['global'].openMachineSession(mach.id)
    mach = session.machine

    adapter = mach.getNetworkAdapter(adapterNum)
    adapterType = getAdapterType(ctx, adapter.adapterType)

    profile_name = proto+"_"+str(hostPort)+"_"+str(guestPort)
    config = "VBoxInternal/Devices/" + adapterType + "/"
    config = config + str(adapter.slot)  +"/LUN#0/Config/" + profile_name
  
    mach.setExtraData(config + "/Protocol", proto)
    mach.setExtraData(config + "/HostPort", str(hostPort))
    mach.setExtraData(config + "/GuestPort", str(guestPort))

    mach.saveSettings()
    session.close()
   
    return 0


def showLogCmd(ctx, args):
    if (len(args) < 2):
        print "usage: showLog <vm> <num>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0

    log = "VBox.log"
    if (len(args) > 2):
       log  += "."+args[2]
    fileName = os.path.join(mach.logFolder, log)

    try:
        lf = open(fileName, 'r')
    except IOError,e:
        print "cannot open: ",e
        return 0

    for line in lf:
        print line,
    lf.close()

    return 0

def evalCmd(ctx, args):
   expr = ' '.join(args[1:])
   try:
        exec expr
   except Exception, e:
        print 'failed: ',e
        if g_verbose:
            traceback.print_exc()
   return 0

def reloadExtCmd(ctx, args):
   # maybe will want more args smartness
   checkUserExtensions(ctx, commands, ctx['vb'].homeFolder)
   autoCompletion(commands, ctx)
   return 0


def runScriptCmd(ctx, args):
    if (len(args) != 2):
        print "usage: runScript <script>"
        return 0
    try:
        lf = open(args[1], 'r')
    except IOError,e:
        print "cannot open:",args[1], ":",e
        return 0

    try:
        for line in lf:
            done = runCommand(ctx, line)
            if done != 0: break
    except Exception,e:
        print "error:",e
        if g_verbose:
                traceback.print_exc()
    lf.close()
    return 0

def sleepCmd(ctx, args):
    if (len(args) != 2):
        print "usage: sleep <secs>"
        return 0

    time.sleep(float(args[1]))
    return 0


def connectCmd(ctx, args):
    if (len(args) > 4):
        print "usage: connect [url] [username] [passwd]"
        return 0

    if ctx['vb'] is not None:
        print "Already connected, disconnect first..."
        return 0

    if (len(args) > 1):
        url = args[1]
    else:
        url = None

    if (len(args) > 2):
        user = args[1]
    else:
        user = ""

    if (len(args) > 3):
        passwd = args[2]
    else:
        passwd = ""

    vbox = ctx['global'].platform.connect(url, user, passwd)
    ctx['vb'] = vbox
    print "Running VirtualBox version %s" %(vbox.version)
    ctx['perf'] = ctx['global'].getPerfCollector(ctx['vb'])
    return 0

def disconnectCmd(ctx, args):
    if (len(args) != 1):
        print "usage: disconnect"
        return 0

    if ctx['vb'] is None:
        print "Not connected yet."
        return 0

    try:
        ctx['global'].platform.disconnect()
    except:
        ctx['vb'] = None
        raise

    ctx['vb'] = None
    return 0

aliases = {'s':'start',
           'i':'info',
           'l':'list',
           'h':'help',
           'a':'alias',
           'q':'quit', 'exit':'quit',
           'v':'verbose'}

commands = {'help':['Prints help information', helpCmd, 0],
            'start':['Start virtual machine by name or uuid', startCmd, 0],
            'create':['Create virtual machine', createCmd, 0],
            'remove':['Remove virtual machine', removeCmd, 0],
            'pause':['Pause virtual machine', pauseCmd, 0],
            'resume':['Resume virtual machine', resumeCmd, 0],
            'save':['Save execution state of virtual machine', saveCmd, 0],
            'stats':['Stats for virtual machine', statsCmd, 0],
            'powerdown':['Power down virtual machine', powerdownCmd, 0],
            'powerbutton':['Effectively press power button', powerbuttonCmd, 0],
            'list':['Shows known virtual machines', listCmd, 0],
            'info':['Shows info on machine', infoCmd, 0],
            'alias':['Control aliases', aliasCmd, 0],
            'verbose':['Toggle verbosity', verboseCmd, 0],
            'setvar':['Set VMs variable: setvar Fedora BIOSSettings.ACPIEnabled True', setvarCmd, 0],
            'eval':['Evaluate arbitrary Python construction: eval \'for m in getMachines(ctx): print m.name,"has",m.memorySize,"M"\'', evalCmd, 0],
            'quit':['Exits', quitCmd, 0],
            'host':['Show host information', hostCmd, 0],
            'guest':['Execute command for guest: guest Win32 \'console.mouse.putMouseEvent(20, 20, 0, 0)\'', guestCmd, 0],
            'monitorGuest':['Monitor what happens with the guest for some time: monitorGuest Win32 10', monitorGuestCmd, 0],
            'monitorVbox':['Monitor what happens with Virtual Box for some time: monitorVbox 10', monitorVboxCmd, 0],
            'portForward':['Setup permanent port forwarding for a VM, takes adapter number host port and guest port: portForward Win32 0 8080 80', portForwardCmd, 0],
            'showLog':['Show log file of the VM, : showLog Win32', showLogCmd, 0],
            'reloadExt':['Reload custom extensions: reloadExt', reloadExtCmd, 0],
            'runScript':['Run VBox script: runScript script.vbox', runScriptCmd, 0],
            'sleep':['Sleep for specified number of seconds: sleep <secs>', sleepCmd, 0],
            }

def runCommandArgs(ctx, args):
    c = args[0]
    if aliases.get(c, None) != None:
        c = aliases[c]
    ci = commands.get(c,None)
    if ci == None:
        print "Unknown command: '%s', type 'help' for list of known commands" %(c)
        return 0
    return ci[1](ctx, args)


def runCommand(ctx, cmd):
    if len(cmd) == 0: return 0
    args = split_no_quotes(cmd)
    if len(args) == 0: return 0
    return runCommandArgs(ctx, args)

#
# To write your own custom commands to vboxshell, create
# file ~/.VirtualBox/shellext.py with content like
#
# def runTestCmd(ctx, args):
#    print "Testy test", ctx['vb']
#    return 0
#
# commands = {
#    'test': ['Test help', runTestCmd]
# }
# and issue reloadExt shell command.
# This file also will be read automatically on startup.
#
def checkUserExtensions(ctx, cmds, folder):
    name =  os.path.join(str(folder), "shellext.py")
    if not os.path.isfile(name):
        return
    d = {}
    try:
        execfile(name, d, d)
        for (k,v) in d['commands'].items():
            if g_verbose:
                print "customize: adding \"%s\" - %s" %(k, v[0])
            cmds[k] = [v[0], v[1], 1]
    except:
        print "Error loading user extensions:"
        traceback.print_exc()

def interpret(ctx):    
    if ctx['remote']:
        commands['connect'] = ["Connect to remote VBox instance", connectCmd, 0]
        commands['disconnect'] = ["Disconnect from remote VBox instance", disconnectCmd, 0]
    
    vbox = ctx['vb']

    if vbox is not None:
        print "Running VirtualBox version %s" %(vbox.version)
        ctx['perf'] = ctx['global'].getPerfCollector(ctx['vb'])
        home = vbox.homeFolder
    else:
        ctx['perf'] = None
        home = os.path.join(os.path.expanduser("~"), ".VirtualBox")

    print "h", home
    checkUserExtensions(ctx, commands, home)

    autoCompletion(commands, ctx)

    # to allow to print actual host information, we collect info for
    # last 150 secs maximum, (sample every 10 secs and keep up to 15 samples)
    if ctx['perf']:
      try:
        ctx['perf'].setup(['*'], [vbox.host], 10, 15)
      except:
        pass

    while True:
        try:
            cmd = raw_input("vbox> ")
            done = runCommand(ctx, cmd)
            if done != 0: break
        except KeyboardInterrupt:
            print '====== You can type quit or q to leave'
            break
        except EOFError:
            break;
        except Exception,e:
            print e
            if g_verbose:
                traceback.print_exc()

    try:
        # There is no need to disable metric collection. This is just an example.
        if ct['perf']:
           ctx['perf'].disable(['*'], [vbox.host])
    except:
        pass

def runCommandCb(ctx, cmd, args):
    args.insert(0, cmd)
    return runCommandArgs(ctx, args)

def main(argv):
    style = None
    autopath = False
    argv.pop(0)
    while len(argv) > 0:
        if argv[0] == "-w":
            style = "WEBSERVICE"
        if argv[0] == "-a":
            autopath = True
        argv.pop(0)

    if autopath:
        cwd = os.getcwd()
        vpp = os.environ.get("VBOX_PROGRAM_PATH")
        if vpp is None and (os.path.isfile(os.path.join(cwd, "VirtualBox")) or os.path.isfile(os.path.join(cwd, "VirtualBox.exe"))) :
            vpp = cwd
            print "Autodetected VBOX_PROGRAM_PATH as",vpp
            os.environ["VBOX_PROGRAM_PATH"] = cwd
            sys.path.append(os.path.join(vpp, "sdk", "installer"))

    from vboxapi import VirtualBoxManager
    g_virtualBoxManager = VirtualBoxManager(style, None)
    ctx = {'global':g_virtualBoxManager,
           'mgr':g_virtualBoxManager.mgr,
           'vb':g_virtualBoxManager.vbox, 
           'ifaces':g_virtualBoxManager.constants,
           'remote':g_virtualBoxManager.remote, 
           'type':g_virtualBoxManager.type,
           'run':runCommandCb,
           '_machlist':None
           }
    interpret(ctx)
    g_virtualBoxManager.deinit()
    del g_virtualBoxManager

if __name__ == '__main__':
    main(sys.argv)
