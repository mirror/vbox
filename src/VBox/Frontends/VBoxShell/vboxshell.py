#!/usr/bin/python
#
# Copyright (C) 2009-2011 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#
#################################################################################
# This program is a simple interactive shell for VirtualBox. You can query      #
# information and issue commands from a simple command line.                    #
#                                                                               #
# It also provides you with examples on how to use VirtualBox's Python API.     #
# This shell is even somewhat documented, supports TAB-completion and           #
# history if you have Python readline installed.                                #
#                                                                               #
# Finally, shell allows arbitrary custom extensions, just create                #
# .VirtualBox/shexts/ and drop your extensions there.                           #
#                                                Enjoy.                         #
################################################################################

import os,sys
import traceback
import shlex
import time
import re
import platform
from optparse import OptionParser

g_batchmode = False
g_scripfile = None
g_cmd = None
g_hasreadline = True
try:
    if g_hasreadline:
        import readline
        import rlcompleter
except:
    g_hasreadline = False


g_prompt = "vbox> "

g_hascolors = True
term_colors = {
    'red':'\033[31m',
    'blue':'\033[94m',
    'green':'\033[92m',
    'yellow':'\033[93m',
    'magenta':'\033[35m',
    'cyan':'\033[36m'
    }
def colored(string,color):
    if not g_hascolors:
        return string
    global term_colors
    col = term_colors.get(color,None)
    if col:
        return col+str(string)+'\033[0m'
    else:
        return string

if g_hasreadline:
  import string
  class CompleterNG(rlcompleter.Completer):
    def __init__(self, dic, ctx):
        self.ctx = ctx
        return rlcompleter.Completer.__init__(self,dic)

    def complete(self, text, state):
        """
        taken from:
        http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/496812
        """
        if False and text == "":
            return ['\t',None][state]
        else:
            return rlcompleter.Completer.complete(self,text,state)

    def canBePath(self, phrase,word):
        return word.startswith('/')

    def canBeCommand(self, phrase, word):
        spaceIdx = phrase.find(" ")
        begIdx = readline.get_begidx()
        firstWord = (spaceIdx == -1 or begIdx < spaceIdx)
        if firstWord:
            return True
        if phrase.startswith('help'):
            return True
        return False

    def canBeMachine(self,phrase,word):
        return not self.canBePath(phrase,word) and not self.canBeCommand(phrase, word)

    def global_matches(self, text):
        """
        Compute matches when text is a simple name.
        Return a list of all names currently defined
        in self.namespace that match.
        """

        matches = []
        phrase = readline.get_line_buffer()

        try:
            if self.canBePath(phrase,text):
                (dir,rest) = os.path.split(text)
                n = len(rest)
                for word in os.listdir(dir):
                    if n == 0 or word[:n] == rest:
                        matches.append(os.path.join(dir,word))

            if self.canBeCommand(phrase,text):
                n = len(text)
                for list in [ self.namespace ]:
                    for word in list:
                        if word[:n] == text:
                            matches.append(word)

            if self.canBeMachine(phrase,text):
                n = len(text)
                for m in getMachines(self.ctx, False, True):
                    # although it has autoconversion, we need to cast
                    # explicitly for subscripts to work
                    word = re.sub("(?<!\\\\) ", "\\ ", str(m.name))
                    if word[:n] == text:
                        matches.append(word)
                    word = str(m.id)
                    if word[:n] == text:
                        matches.append(word)

        except Exception,e:
            printErr(e)
            if g_verbose:
                traceback.print_exc()

        return matches

def autoCompletion(commands, ctx):
  if  not g_hasreadline:
      return

  comps = {}
  for (k,v) in commands.items():
      comps[k] = None
  completer = CompleterNG(comps, ctx)
  readline.set_completer(completer.complete)
  delims = readline.get_completer_delims()
  readline.set_completer_delims(re.sub("[\\./-]", "", delims)) # remove some of the delimiters
  readline.parse_and_bind("set editing-mode emacs")
  # OSX need it
  if platform.system() == 'Darwin':
      # see http://www.certif.com/spec_help/readline.html
      readline.parse_and_bind ("bind ^I rl_complete")
      readline.parse_and_bind ("bind ^W ed-delete-prev-word")
      # Doesn't work well
      # readline.parse_and_bind ("bind ^R em-inc-search-prev")
  readline.parse_and_bind("tab: complete")


g_verbose = False

def split_no_quotes(s):
    return shlex.split(s)

def progressBar(ctx,p,wait=1000):
    try:
        while not p.completed:
            print "%s %%\r" %(colored(str(p.percent),'red')),
            sys.stdout.flush()
            p.waitForCompletion(wait)
            ctx['global'].waitForEvents(0)
        if int(p.resultCode) != 0:
            reportError(ctx, p)
        return 1
    except KeyboardInterrupt:
        print "Interrupted."
        ctx['interrupt'] = True
        if p.cancelable:
            print "Canceling task..."
            p.cancel()
        return 0

def printErr(ctx,e):
     print colored(str(e), 'red')

def reportError(ctx,progress):
    ei = progress.errorInfo
    if ei:
        print colored("Error in module '%s': %s" %(ei.component, ei.text), 'red')

def colCat(ctx,str):
    return colored(str, 'magenta')

def colVm(ctx,vm):
    return colored(vm, 'blue')

def colPath(ctx,p):
    return colored(p, 'green')

def colSize(ctx,m):
    return colored(m, 'red')

def colPci(ctx,vm):
    return colored(vm, 'green')

def colDev(ctx,vm):
    return colored(vm, 'cyan')

def colSizeM(ctx,m):
    return colored(str(m)+'M', 'red')

def createVm(ctx,name,kind):
    mgr = ctx['mgr']
    vb = ctx['vb']
    mach = vb.createMachine("", name, kind, "", False)
    mach.saveSettings()
    print "created machine with UUID",mach.id
    vb.registerMachine(mach)
    # update cache
    getMachines(ctx, True)

def removeVm(ctx,mach):
    mgr = ctx['mgr']
    vb = ctx['vb']
    id = mach.id
    print "removing machine ",mach.name,"with UUID",id
    cmdClosedVm(ctx, mach, detachVmDevice, ["ALL"])
    mach = mach.unregister(ctx['global'].constants.CleanupMode_Full)
    if mach:
         mach.deleteSettings()
    # update cache
    getMachines(ctx, True)

def startVm(ctx,mach,type):
    mgr = ctx['mgr']
    vb = ctx['vb']
    perf = ctx['perf']
    session = mgr.getSessionObject(vb)
    progress = mach.launchVMProcess(session, type, "")
    if progressBar(ctx, progress, 100) and int(progress.resultCode) == 0:
        # we ignore exceptions to allow starting VM even if
        # perf collector cannot be started
        if perf:
          try:
            perf.setup(['*'], [mach], 10, 15)
          except Exception,e:
            printErr(ctx, e)
            if g_verbose:
                traceback.print_exc()
        session.unlockMachine()

class CachedMach:
        def __init__(self, mach):
            self.name = mach.name
            self.id = mach.id

def cacheMachines(ctx,list):
    result = []
    for m in list:
        try:
            elem = CachedMach(m)
            result.append(elem)
        except:
            pass
    return result

def getMachines(ctx, invalidate = False, simple=False):
    if ctx['vb'] is not None:
        if ctx['_machlist'] is None or invalidate:
            ctx['_machlist'] = ctx['global'].getArray(ctx['vb'], 'machines')
            ctx['_machlistsimple'] = cacheMachines(ctx,ctx['_machlist'])
        if simple:
            return ctx['_machlistsimple']
        else:
            return ctx['_machlist']
    else:
        return []

def asState(var):
    if var:
        return colored('on', 'green')
    else:
        return colored('off', 'green')

def asFlag(var):
    if var:
        return 'yes'
    else:
        return 'no'

def perfStats(ctx,mach):
    if not ctx['perf']:
        return
    for metric in ctx['perf'].query(["*"], [mach]):
        print metric['name'], metric['values_as_string']

def guestExec(ctx, machine, console, cmds):
    exec cmds

def printMouseEvent(ctx, mev):
    print "Mouse : absolute=%d x=%d y=%d z=%d buttons=%x" %(mev.absolute, mev.x, mev.y, mev.z, mev.buttons)

def printKbdEvent(ctx, kev):
    print "Kbd: ", ctx['global'].getArray(kev, 'scancodes')

def monitorSource(ctx, es, active, dur):
    def handleEventImpl(ev):
         type = ev.type
         print "got event: %s %s" %(str(type), asEnumElem(ctx, 'VBoxEventType', type))
         if type == ctx['global'].constants.VBoxEventType_OnMachineStateChanged:
             scev = ctx['global'].queryInterface(ev, 'IMachineStateChangedEvent')
             if scev:
                 print "machine state event: mach=%s state=%s" %(scev.machineId, scev.state)
         elif  type == ctx['global'].constants.VBoxEventType_OnGuestPropertyChanged:
             gpcev = ctx['global'].queryInterface(ev, 'IGuestPropertyChangedEvent')
             if gpcev:
                 print "guest property change: name=%s value=%s" %(gpcev.name, gpcev.value)
         elif  type == ctx['global'].constants.VBoxEventType_OnMousePointerShapeChanged:
             psev = ctx['global'].queryInterface(ev, 'IMousePointerShapeChangedEvent')
             if psev:
                 shape = ctx['global'].getArray(psev, 'shape')
                 if shape is None:
                     print "pointer shape event - empty shape"
                 else:
                     print "pointer shape event: w=%d h=%d shape len=%d" %(psev.width, psev.height, len(shape))
         elif type == ctx['global'].constants.VBoxEventType_OnGuestMouse:
             mev = ctx['global'].queryInterface(ev, 'IGuestMouseEvent')
             if mev:
                 printMouseEvent(ctx, mev)
         elif type == ctx['global'].constants.VBoxEventType_OnGuestKeyboard:
             kev = ctx['global'].queryInterface(ev, 'IGuestKeyboardEvent')
             if kev:
                 printKbdEvent(ctx, kev)

    class EventListener:
     def __init__(self, arg):
         pass

     def handleEvent(self, ev):
         try:
            # a bit convoluted QI to make it work with MS COM
            handleEventImpl(ctx['global'].queryInterface(ev, 'IEvent'))
         except:
            traceback.print_exc()
	    pass

    if active:
        listener = ctx['global'].createListener(EventListener)
    else:
        listener = es.createListener()
    registered = False
    if dur == -1:
        # not infinity, but close enough
        dur = 100000
    try:
        es.registerListener(listener, [ctx['global'].constants.VBoxEventType_Any], active)
        registered = True
        end = time.time() + dur
        while  time.time() < end:
            if active:
                ctx['global'].waitForEvents(500)
            else:
                ev = es.getEvent(listener, 500)
                if ev:
                    handleEventImpl(ev)
                    # otherwise waitable events will leak (active listeners ACK automatically)
                    es.eventProcessed(listener, ev)
    # We need to catch all exceptions here, otherwise listener will never be unregistered
    except:
        traceback.print_exc()
        pass
    if listener and registered:
        es.unregisterListener(listener)


tsLast = 0
def recordDemo(ctx, console, file, dur):
    demo = open(file, 'w')
    header="VM="+console.machine.name+"\n"
    demo.write(header)

    global tsLast
    tsLast = time.time()

    def stamp():
        global tsLast
        tsCur = time.time()
        rv = int((tsCur-tsLast)*1000)
        tsLast = tsCur
        return rv

    def handleEventImpl(ev):
         type = ev.type
         #print "got event: %s %s" %(str(type), asEnumElem(ctx, 'VBoxEventType', type))
         if type == ctx['global'].constants.VBoxEventType_OnGuestMouse:
             mev = ctx['global'].queryInterface(ev, 'IGuestMouseEvent')
             if mev:
                 l = "%d: m %d %d %d %d %d %d\n" %(stamp(), mev.absolute, mev.x, mev.y, mev.z, mev.w, mev.buttons)
                 demo.write(l)
         elif type == ctx['global'].constants.VBoxEventType_OnGuestKeyboard:
             kev = ctx['global'].queryInterface(ev, 'IGuestKeyboardEvent')
             if kev:
                 l = "%d: k %s\n" %(stamp(), str(ctx['global'].getArray(kev, 'scancodes')))
                 demo.write(l)

    listener = console.eventSource.createListener()
    registered = False
    # we create an aggregated event source to listen for multiple event sources (keyboard and mouse in our case)
    agg = console.eventSource.createAggregator([console.keyboard.eventSource, console.mouse.eventSource])
    demo = open(file, 'w')
    header="VM="+console.machine.name+"\n"
    demo.write(header)
    if dur == -1:
        # not infinity, but close enough
        dur = 100000
    try:
        agg.registerListener(listener, [ctx['global'].constants.VBoxEventType_Any], False)
        registered = True
        end = time.time() + dur
        while  time.time() < end:
            ev = agg.getEvent(listener, 1000)
            if ev:
                handleEventImpl(ev)
                # keyboard/mouse events aren't waitable, so no need for eventProcessed
    # We need to catch all exceptions here, otherwise listener will never be unregistered
    except:
        traceback.print_exc()
        pass
    demo.close()
    if listener and registered:
        agg.unregisterListener(listener)


def playbackDemo(ctx, console, file, dur):
    demo = open(file, 'r')

    if dur == -1:
        # not infinity, but close enough
        dur = 100000

    header = demo.readline()
    print "Header is", header
    basere = re.compile(r'(?P<s>\d+): (?P<t>[km]) (?P<p>.*)')
    mre = re.compile(r'(?P<a>\d+) (?P<x>-*\d+) (?P<y>-*\d+) (?P<z>-*\d+) (?P<w>-*\d+) (?P<b>-*\d+)')
    kre = re.compile(r'\d+')

    kbd = console.keyboard
    mouse = console.mouse

    try:
        end = time.time() + dur
        for line in demo:
            if time.time() > end:
                break
            m = basere.search(line)
            if m is None:
                continue

            dict = m.groupdict()
            stamp = dict['s']
            params = dict['p']
            type = dict['t']

            time.sleep(float(stamp)/1000)

            if type == 'k':
                codes=kre.findall(params)
                #print "KBD:",codes
                kbd.putScancodes(codes)
            elif type == 'm':
                mm = mre.search(params)
                if mm is not None:
                    mdict = mm.groupdict()
                    if mdict['a'] == '1':
                        # absolute
                        #print "MA: ",mdict['x'],mdict['y'],mdict['z'],mdict['b']
                        mouse.putMouseEventAbsolute(int(mdict['x']), int(mdict['y']), int(mdict['z']), int(mdict['w']), int(mdict['b']))
                    else:
                        #print "MR: ",mdict['x'],mdict['y'],mdict['b']
                        mouse.putMouseEvent(int(mdict['x']), int(mdict['y']), int(mdict['z']), int(mdict['w']), int(mdict['b']))

    # We need to catch all exceptions here, to close file
    except KeyboardInterrupt:
        ctx['interrupt'] = True
    except:
        traceback.print_exc()
        pass
    demo.close()


def takeScreenshotOld(ctx,console,args):
    from PIL import Image
    display = console.display
    if len(args) > 0:
        f = args[0]
    else:
        f = "/tmp/screenshot.png"
    if len(args) > 3:
        screen = int(args[3])
    else:
        screen = 0
    (fbw, fbh, fbbpp) = display.getScreenResolution(screen)
    if len(args) > 1:
        w = int(args[1])
    else:
        w = fbw
    if len(args) > 2:
        h = int(args[2])
    else:
        h = fbh

    print "Saving screenshot (%d x %d) screen %d in %s..." %(w,h,screen,f)
    data = display.takeScreenShotToArray(screen, w,h)
    size = (w,h)
    mode = "RGBA"
    im = Image.frombuffer(mode, size, str(data), "raw", mode, 0, 1)
    im.save(f, "PNG")

def takeScreenshot(ctx,console,args):
    display = console.display
    if len(args) > 0:
        f = args[0]
    else:
        f = "/tmp/screenshot.png"
    if len(args) > 3:
        screen = int(args[3])
    else:
        screen = 0
    (fbw, fbh, fbbpp) = display.getScreenResolution(screen)
    if len(args) > 1:
        w = int(args[1])
    else:
        w = fbw
    if len(args) > 2:
        h = int(args[2])
    else:
        h = fbh

    print "Saving screenshot (%d x %d) screen %d in %s..." %(w,h,screen,f)
    data = display.takeScreenShotPNGToArray(screen, w,h)
    size = (w,h)
    file = open(f, 'wb')
    file.write(data)
    file.close()

def teleport(ctx,session,console,args):
    if args[0].find(":") == -1:
        print "Use host:port format for teleport target"
        return
    (host,port) = args[0].split(":")
    if len(args) > 1:
        passwd = args[1]
    else:
        passwd = ""

    if len(args) > 2:
        maxDowntime  = int(args[2])
    else:
        maxDowntime = 250

    port = int(port)
    print "Teleporting to %s:%d..." %(host,port)
    progress = console.teleport(host, port, passwd, maxDowntime)
    if progressBar(ctx, progress, 100) and int(progress.resultCode) == 0:
        print "Success!"
    else:
        reportError(ctx,progress)


def guestStats(ctx,console,args):
    guest = console.guest
    # we need to set up guest statistics
    if len(args) > 0 :
        update = args[0]
    else:
        update = 1
    if guest.statisticsUpdateInterval != update:
        guest.statisticsUpdateInterval = update
        try:
            time.sleep(float(update)+0.1)
        except:
            # to allow sleep interruption
            pass
    all_stats = ctx['const'].all_values('GuestStatisticType')
    cpu = 0
    for s in all_stats.keys():
        try:
            val = guest.getStatistic( cpu, all_stats[s])
            print "%s: %d" %(s, val)
        except:
            # likely not implemented
            pass

def plugCpu(ctx,machine,session,args):
    cpu = int(args[0])
    print "Adding CPU %d..." %(cpu)
    machine.hotPlugCPU(cpu)

def unplugCpu(ctx,machine,session,args):
    cpu = int(args[0])
    print "Removing CPU %d..." %(cpu)
    machine.hotUnplugCPU(cpu)

def mountIso(ctx,machine,session,args):
    machine.mountMedium(args[0], args[1], args[2], args[3], args[4])
    machine.saveSettings()

def cond(c,v1,v2):
    if c:
        return v1
    else:
        return v2

def printHostUsbDev(ctx,ud):
    print "  %s: %s (vendorId=%d productId=%d serial=%s) %s" %(ud.id, colored(ud.product,'blue'), ud.vendorId, ud.productId, ud.serialNumber,asEnumElem(ctx, 'USBDeviceState', ud.state))

def printUsbDev(ctx,ud):
    print "  %s: %s (vendorId=%d productId=%d serial=%s)" %(ud.id,  colored(ud.product,'blue'), ud.vendorId, ud.productId, ud.serialNumber)

def printSf(ctx,sf):
    print "    name=%s host=%s %s %s" %(sf.name, colPath(ctx,sf.hostPath), cond(sf.accessible, "accessible", "not accessible"), cond(sf.writable, "writable", "read-only"))

def ginfo(ctx,console, args):
    guest = console.guest
    if guest.additionsActive:
        vers = int(str(guest.additionsVersion))
        print "Additions active, version %d.%d"  %(vers >> 16, vers & 0xffff)
        print "Support seamless: %s"          %(asFlag(guest.supportsSeamless))
        print "Support graphics: %s"          %(asFlag(guest.supportsGraphics))
        print "Baloon size: %d"               %(guest.memoryBalloonSize)
        print "Statistic update interval: %d" %(guest.statisticsUpdateInterval)
    else:
        print "No additions"
    usbs = ctx['global'].getArray(console, 'USBDevices')
    print "Attached USB:"
    for ud in usbs:
         printUsbDev(ctx,ud)
    rusbs = ctx['global'].getArray(console, 'remoteUSBDevices')
    print "Remote USB:"
    for ud in rusbs:
        printHostUsbDev(ctx,ud)
    print "Transient shared folders:"
    sfs =  rusbs = ctx['global'].getArray(console, 'sharedFolders')
    for sf in sfs:
        printSf(ctx,sf)

def cmdExistingVm(ctx,mach,cmd,args):
    session = None
    try:
        vb = ctx['vb']
        session = ctx['mgr'].getSessionObject(vb)
        mach.lockMachine(session, ctx['global'].constants.LockType_Shared)
    except Exception,e:
        printErr(ctx, "Session to '%s' not open: %s" %(mach.name,str(e)))
        if g_verbose:
            traceback.print_exc()
        return
    if session.state != ctx['const'].SessionState_Locked:
        print "Session to '%s' in wrong state: %s" %(mach.name, session.state)
        session.unlockMachine()
        return
    # this could be an example how to handle local only (i.e. unavailable
    # in Webservices) functionality
    if ctx['remote'] and cmd == 'some_local_only_command':
        print 'Trying to use local only functionality, ignored'
        session.unlockMachine()
        return
    console=session.console
    ops={'pause':           lambda: console.pause(),
         'resume':          lambda: console.resume(),
         'powerdown':       lambda: console.powerDown(),
         'powerbutton':     lambda: console.powerButton(),
         'stats':           lambda: perfStats(ctx, mach),
         'guest':           lambda: guestExec(ctx, mach, console, args),
         'ginfo':           lambda: ginfo(ctx, console, args),
         'guestlambda':     lambda: args[0](ctx, mach, console, args[1:]),
         'save':            lambda: progressBar(ctx,console.saveState()),
         'screenshot':      lambda: takeScreenshot(ctx,console,args),
         'teleport':        lambda: teleport(ctx,session,console,args),
         'gueststats':      lambda: guestStats(ctx, console, args),
         'plugcpu':         lambda: plugCpu(ctx, session.machine, session, args),
         'unplugcpu':       lambda: unplugCpu(ctx, session.machine, session, args),
         'mountiso':        lambda: mountIso(ctx, session.machine, session, args),
         }
    try:
        ops[cmd]()
    except KeyboardInterrupt:
        ctx['interrupt'] = True
    except Exception, e:
        printErr(ctx,e)
        if g_verbose:
            traceback.print_exc()

    session.unlockMachine()


def cmdClosedVm(ctx,mach,cmd,args=[],save=True):
    session = ctx['global'].openMachineSession(mach, True)
    mach = session.machine
    try:
        cmd(ctx, mach, args)
    except Exception, e:
        save = False
        printErr(ctx,e)
        if g_verbose:
            traceback.print_exc()
    if save:
        try:
            mach.saveSettings()
        except Exception, e:
            printErr(ctx,e)
            if g_verbose:
                traceback.print_exc()
    ctx['global'].closeMachineSession(session)


def cmdAnyVm(ctx,mach,cmd, args=[],save=False):
    session = ctx['global'].openMachineSession(mach)
    mach = session.machine
    try:
         cmd(ctx, mach, session.console, args)
    except Exception, e:
        save = False;
        printErr(ctx,e)
        if g_verbose:
            traceback.print_exc()
    if save:
         mach.saveSettings()
    ctx['global'].closeMachineSession(session)

def machById(ctx,id):
    try:
        mach = ctx['vb'].getMachine(id)
    except:
        mach = ctx['vb'].findMachine(id)
    return mach

class XPathNode:
    def __init__(self, parent, obj, type):
        self.parent = parent
        self.obj = obj
        self.type = type
    def lookup(self, subpath):
        children = self.enum()
        matches = []
        for e in children:
            if e.matches(subpath):
                matches.append(e)
        return matches
    def enum(self):
        return []
    def matches(self,subexp):
        if subexp == self.type:
            return True
        if not subexp.startswith(self.type):
            return False
        m = re.search(r"@(?P<a>\w+)=(?P<v>[^\'\[\]]+)", subexp)
        matches = False
        try:
            if m is not None:
                dict = m.groupdict()
                attr = dict['a']
                val  = dict['v']
                matches = (str(getattr(self.obj, attr)) == val)
        except:
            pass
        return matches
    def apply(self, cmd):
        exec(cmd, {'obj':self.obj,'node':self,'ctx':self.getCtx()}, {})
    def getCtx(self):
        if hasattr(self,'ctx'):
            return self.ctx
        return self.parent.getCtx()

class XPathNodeHolder(XPathNode):
    def __init__(self, parent, obj, attr, heldClass, xpathname):
        XPathNode.__init__(self, parent, obj, 'hld '+xpathname)
        self.attr = attr
        self.heldClass = heldClass
        self.xpathname = xpathname
    def enum(self):
        children = []
        for n in self.getCtx()['global'].getArray(self.obj, self.attr):
            node = self.heldClass(self, n)
            children.append(node)
        return children
    def matches(self,subexp):
        return subexp == self.xpathname

class XPathNodeValue(XPathNode):
    def __init__(self, parent, obj, xpathname):
        XPathNode.__init__(self, parent, obj, 'val '+xpathname)
        self.xpathname = xpathname
    def matches(self,subexp):
        return subexp == self.xpathname

class XPathNodeHolderVM(XPathNodeHolder):
    def __init__(self, parent, vbox):
        XPathNodeHolder.__init__(self, parent, vbox, 'machines', XPathNodeVM, 'vms')

class XPathNodeVM(XPathNode):
    def __init__(self, parent, obj):
        XPathNode.__init__(self, parent, obj, 'vm')
    #def matches(self,subexp):
    #    return subexp=='vm'
    def enum(self):
        return [XPathNodeHolderNIC(self, self.obj),
                XPathNodeValue(self, self.obj.BIOSSettings,  'bios'),
                XPathNodeValue(self, self.obj.USBController, 'usb')]

class XPathNodeHolderNIC(XPathNodeHolder):
    def __init__(self, parent, mach):
        XPathNodeHolder.__init__(self, parent, mach, 'nics', XPathNodeVM, 'nics')
        self.maxNic = self.getCtx()['vb'].systemProperties.getMaxNetworkAdapters(self.obj.chipsetType)
    def enum(self):
        children = []
        for i in range(0, self.maxNic):
            node = XPathNodeNIC(self, self.obj.getNetworkAdapter(i))
            children.append(node)
        return children

class XPathNodeNIC(XPathNode):
    def __init__(self, parent, obj):
        XPathNode.__init__(self, parent, obj, 'nic')
    def matches(self,subexp):
        return subexp=='nic'

class XPathNodeRoot(XPathNode):
    def __init__(self, ctx):
        XPathNode.__init__(self, None, None, 'root')
        self.ctx = ctx
    def enum(self):
        return [XPathNodeHolderVM(self, self.ctx['vb'])]
    def matches(self,subexp):
        return True

def eval_xpath(ctx,scope):
    pathnames = scope.split("/")[2:]
    nodes = [XPathNodeRoot(ctx)]
    for p in pathnames:
        seen = []
        while len(nodes) > 0:
            n = nodes.pop()
            seen.append(n)
        for s in seen:
            matches = s.lookup(p)
            for m in matches:
                nodes.append(m)
        if len(nodes) == 0:
            break
    return nodes

def argsToMach(ctx,args):
    if len(args) < 2:
        print "usage: %s [vmname|uuid]" %(args[0])
        return None
    id = args[1]
    m = machById(ctx, id)
    if m == None:
        print "Machine '%s' is unknown, use list command to find available machines" %(id)
    return m

def helpSingleCmd(cmd,h,sp):
    if sp != 0:
        spec = " [ext from "+sp+"]"
    else:
        spec = ""
    print "    %s: %s%s" %(colored(cmd,'blue'),h,spec)

def helpCmd(ctx, args):
    if len(args) == 1:
        print "Help page:"
        names = commands.keys()
        names.sort()
        for i in names:
            helpSingleCmd(i, commands[i][0], commands[i][2])
    else:
        cmd = args[1]
        c = commands.get(cmd)
        if c == None:
            print "Command '%s' not known" %(cmd)
        else:
            helpSingleCmd(cmd, c[0], c[2])
    return 0

def asEnumElem(ctx,enum,elem):
    all = ctx['const'].all_values(enum)
    for e in all.keys():
        if str(elem) == str(all[e]):
            return colored(e, 'green')
    return colored("<unknown>", 'green')

def enumFromString(ctx,enum,str):
    all = ctx['const'].all_values(enum)
    return all.get(str, None)

def listCmd(ctx, args):
    for m in getMachines(ctx, True):
        try:
            if m.teleporterEnabled:
                tele = "[T] "
            else:
                tele = "    "
                print "%sMachine '%s' [%s], machineState=%s, sessionState=%s" %(tele,colVm(ctx,m.name),m.id,asEnumElem(ctx, "MachineState", m.state), asEnumElem(ctx,"SessionState", m.sessionState))
        except Exception, e:
            printErr(ctx,e)
            if g_verbose:
                traceback.print_exc()
    return 0

def infoCmd(ctx,args):
    if (len(args) < 2):
        print "usage: info [vmname|uuid]"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    os = ctx['vb'].getGuestOSType(mach.OSTypeId)
    print " One can use setvar <mach> <var> <value> to change variable, using name in []."
    print "  Name [name]: %s" %(colVm(ctx,mach.name))
    print "  Description [description]: %s" %(mach.description)
    print "  ID [n/a]: %s" %(mach.id)
    print "  OS Type [via OSTypeId]: %s" %(os.description)
    print "  Firmware [firmwareType]: %s (%s)" %(asEnumElem(ctx,"FirmwareType", mach.firmwareType),mach.firmwareType)
    print
    print "  CPUs [CPUCount]: %d" %(mach.CPUCount)
    print "  RAM [memorySize]: %dM" %(mach.memorySize)
    print "  VRAM [VRAMSize]: %dM" %(mach.VRAMSize)
    print "  Monitors [monitorCount]: %d" %(mach.monitorCount)
    print "  Chipset [chipsetType]: %s (%s)" %(asEnumElem(ctx,"ChipsetType", mach.chipsetType), mach.chipsetType)
    print
    print "  Clipboard mode [clipboardMode]: %s (%s)" %(asEnumElem(ctx,"ClipboardMode", mach.clipboardMode), mach.clipboardMode)
    print "  Machine status [n/a]: %s (%s)" % (asEnumElem(ctx,"SessionState", mach.sessionState), mach.sessionState)
    print
    if mach.teleporterEnabled:
        print "  Teleport target on port %d (%s)" %(mach.teleporterPort, mach.teleporterPassword)
        print
    bios = mach.BIOSSettings
    print "  ACPI [BIOSSettings.ACPIEnabled]: %s" %(asState(bios.ACPIEnabled))
    print "  APIC [BIOSSettings.IOAPICEnabled]: %s" %(asState(bios.IOAPICEnabled))
    hwVirtEnabled = mach.getHWVirtExProperty(ctx['global'].constants.HWVirtExPropertyType_Enabled)
    print "  Hardware virtualization [guest win machine.setHWVirtExProperty(ctx[\\'const\\'].HWVirtExPropertyType_Enabled,value)]: " + asState(hwVirtEnabled)
    hwVirtVPID = mach.getHWVirtExProperty(ctx['const'].HWVirtExPropertyType_VPID)
    print "  VPID support [guest win machine.setHWVirtExProperty(ctx[\\'const\\'].HWVirtExPropertyType_VPID,value)]: " + asState(hwVirtVPID)
    hwVirtNestedPaging = mach.getHWVirtExProperty(ctx['const'].HWVirtExPropertyType_NestedPaging)
    print "  Nested paging [guest win machine.setHWVirtExProperty(ctx[\\'const\\'].HWVirtExPropertyType_NestedPaging,value)]: " + asState(hwVirtNestedPaging)

    print "  Hardware 3d acceleration [accelerate3DEnabled]: " + asState(mach.accelerate3DEnabled)
    print "  Hardware 2d video acceleration [accelerate2DVideoEnabled]: " + asState(mach.accelerate2DVideoEnabled)

    print "  Use universal time [RTCUseUTC]: %s" %(asState(mach.RTCUseUTC))
    print "  HPET [hpetEnabled]: %s" %(asState(mach.hpetEnabled))
    if mach.audioAdapter.enabled:
        print "  Audio [via audioAdapter]: chip %s; host driver %s" %(asEnumElem(ctx,"AudioControllerType", mach.audioAdapter.audioController), asEnumElem(ctx,"AudioDriverType",  mach.audioAdapter.audioDriver))
    if mach.USBController.enabled:
        print "  USB [via USBController]: high speed %s" %(asState(mach.USBController.enabledEhci))
    print "  CPU hotplugging [CPUHotPlugEnabled]: %s" %(asState(mach.CPUHotPlugEnabled))

    print "  Keyboard [keyboardHidType]: %s (%s)" %(asEnumElem(ctx,"KeyboardHidType", mach.keyboardHidType), mach.keyboardHidType)
    print "  Pointing device [pointingHidType]: %s (%s)" %(asEnumElem(ctx,"PointingHidType", mach.pointingHidType), mach.pointingHidType)
    print "  Last changed [n/a]: " + time.asctime(time.localtime(long(mach.lastStateChange)/1000))
    # OSE has no VRDE
    try:
        print "  VRDE server [VRDEServer.enabled]: %s" %(asState(mach.VRDEServer.enabled))
    except:
        pass
    print
    print colCat(ctx,"  I/O subsystem info:")
    print "   Cache enabled [ioCacheEnabled]: %s" %(asState(mach.ioCacheEnabled))
    print "   Cache size [ioCacheSize]: %dM" %(mach.ioCacheSize)

    controllers = ctx['global'].getArray(mach, 'storageControllers')
    if controllers:
        print
        print colCat(ctx,"  Controllers:")
    for controller in controllers:
        print "    '%s': bus %s type %s" % (controller.name, asEnumElem(ctx,"StorageBus", controller.bus), asEnumElem(ctx,"StorageControllerType", controller.controllerType))

    attaches = ctx['global'].getArray(mach, 'mediumAttachments')
    if attaches:
        print
        print colCat(ctx,"  Media:")
    for a in attaches:
        print "   Controller: '%s' port/device: %d:%d type: %s (%s):" % (a.controller, a.port, a.device, asEnumElem(ctx,"DeviceType", a.type), a.type)
        m = a.medium
        if a.type == ctx['global'].constants.DeviceType_HardDisk:
            print "   HDD:"
            print "    Id: %s" %(m.id)
            print "    Location: %s" %(colPath(ctx,m.location))
            print "    Name: %s"  %(m.name)
            print "    Format: %s"  %(m.format)

        if a.type == ctx['global'].constants.DeviceType_DVD:
            print "   DVD:"
            if m:
                print "    Id: %s" %(m.id)
                print "    Name: %s" %(m.name)
                if m.hostDrive:
                    print "    Host DVD %s" %(colPath(ctx,m.location))
                    if a.passthrough:
                         print "    [passthrough mode]"
                else:
                    print "    Virtual image at %s" %(colPath(ctx,m.location))
                    print "    Size: %s" %(m.size)

        if a.type == ctx['global'].constants.DeviceType_Floppy:
            print "   Floppy:"
            if m:
                print "    Id: %s" %(m.id)
                print "    Name: %s" %(m.name)
                if m.hostDrive:
                    print "    Host floppy %s" %(colPath(ctx,m.location))
                else:
                    print "    Virtual image at %s" %(colPath(ctx,m.location))
                    print "    Size: %s" %(m.size)

    print
    print colCat(ctx,"  Shared folders:")
    for sf in ctx['global'].getArray(mach, 'sharedFolders'):
        printSf(ctx,sf)

    return 0

def startCmd(ctx, args):
    if len(args) < 2:
        print "usage: start name <frontend>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    if len(args) > 2:
        type = args[2]
    else:
        type = "gui"
    startVm(ctx, mach, type)
    return 0

def createVmCmd(ctx, args):
    if (len(args) != 3):
        print "usage: createvm name ostype"
        return 0
    name = args[1]
    oskind = args[2]
    try:
         ctx['vb'].getGuestOSType(oskind)
    except Exception, e:
        print 'Unknown OS type:',oskind
        return 0
    createVm(ctx, name, oskind)
    return 0

def ginfoCmd(ctx,args):
    if (len(args) < 2):
        print "usage: ginfo [vmname|uuid]"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'ginfo', '')
    return 0

def execInGuest(ctx,console,args,env,user,passwd,tmo,inputPipe=None,outputPipe=None):
    if len(args) < 1:
        print "exec in guest needs at least program name"
        return
    guest = console.guest
    # shall contain program name as argv[0]
    gargs = args
    print "executing %s with args %s as %s" %(args[0], gargs, user)
    flags = 0
    if inputPipe is not None:
        flags = 1 # set WaitForProcessStartOnly
    print args[0]
    (progress, pid) = guest.executeProcess(args[0], flags, gargs, env, user, passwd, tmo)
    print "executed with pid %d" %(pid)
    if pid != 0:
        try:
            while True:
                if inputPipe is not None:
                    indata = inputPipe(ctx)
                    if indata is not None:
                        write = len(indata)
                        off = 0
                        while write > 0:
                            w = guest.setProcessInput(pid, 0, 10*1000, indata[off:])
                            off = off + w
                            write = write - w
                    else:
                        # EOF
                        try:
                            guest.setProcessInput(pid, 1, 10*1000, " ")
                        except:
                            pass
                data = guest.getProcessOutput(pid, 0, 10000, 4096)
                if data and len(data) > 0:
                    sys.stdout.write(data)
                    continue
                progress.waitForCompletion(100)
                ctx['global'].waitForEvents(0)
                data = guest.getProcessOutput(pid, 0, 0, 4096)
                if data and len(data) > 0:
                    if outputPipe is not None:
                        outputPipe(ctx,data)
                    else:
                        sys.stdout.write(data)
                    continue
                if progress.completed:
                    break

        except KeyboardInterrupt:
            print "Interrupted."
            ctx['interrupt'] = True
            if progress.cancelable:
                progress.cancel()
        (reason, code, flags) = guest.getProcessStatus(pid)
        print "Exit code: %d" %(code)
        return 0
    else:
        reportError(ctx, progress)

def copyToGuest(ctx,console,args,user,passwd):
    src = args[0]
    dst = args[1]
    flags = 0
    print "Copying host %s to guest %s" %(src,dst)
    progress = console.guest.copyToGuest(src, dst, user, passwd, flags)
    progressBar(ctx, progress)

def nh_raw_input(prompt=""):
    stream = sys.stdout
    prompt = str(prompt)
    if prompt:
        stream.write(prompt)
    line = sys.stdin.readline()
    if not line:
        raise EOFError
    if line[-1] == '\n':
        line = line[:-1]
    return line


def getCred(ctx):
    import getpass
    user = getpass.getuser()
    user_inp = nh_raw_input("User (%s): " %(user))
    if len (user_inp) > 0:
        user = user_inp
    passwd = getpass.getpass()

    return (user,passwd)

def gexecCmd(ctx,args):
    if (len(args) < 2):
        print "usage: gexec [vmname|uuid] command args"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    gargs = args[2:]
    env = [] # ["DISPLAY=:0"]
    (user,passwd) = getCred(ctx)
    gargs.insert(0, lambda ctx,mach,console,args: execInGuest(ctx,console,args,env,user,passwd,10000))
    cmdExistingVm(ctx, mach, 'guestlambda', gargs)
    return 0

def gcopyCmd(ctx,args):
    if (len(args) < 2):
        print "usage: gcopy [vmname|uuid] host_path guest_path"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    gargs = args[2:]
    (user,passwd) = getCred(ctx)
    gargs.insert(0, lambda ctx,mach,console,args: copyToGuest(ctx,console,args,user,passwd))
    cmdExistingVm(ctx, mach, 'guestlambda', gargs)
    return 0

def readCmdPipe(ctx,hcmd):
    try:
        return ctx['process'].communicate()[0]
    except:
        return None

def gpipeCmd(ctx,args):
    if (len(args) < 4):
        print "usage: gpipe [vmname|uuid] hostProgram guestProgram, such as gpipe linux  '/bin/uname -a' '/bin/sh -c \"/usr/bin/tee; /bin/uname -a\"'"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    hcmd = args[2]
    gcmd = args[3]
    (user,passwd) = getCred(ctx)
    import subprocess
    ctx['process'] = subprocess.Popen(split_no_quotes(hcmd), stdout=subprocess.PIPE)
    gargs = split_no_quotes(gcmd)
    env = []
    gargs.insert(0, lambda ctx,mach,console,args: execInGuest(ctx,console,args,env,user,passwd, 10000,lambda ctx:readCmdPipe(ctx, hcmd)))
    cmdExistingVm(ctx, mach, 'guestlambda', gargs)
    try:
        ctx['process'].terminate()
    except:
        pass
    ctx['process'] = None
    return 0


def removeVmCmd(ctx, args):
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
    if mach.state != ctx['const'].MachineState_Running:
        cmdClosedVm(ctx, mach, lambda ctx, mach, a: guestExec (ctx, mach, None, ' '.join(args[2:])))
    else:
        cmdExistingVm(ctx, mach, 'guest', ' '.join(args[2:]))
    return 0

def screenshotCmd(ctx, args):
    if (len(args) < 2):
        print "usage: screenshot vm <file> <width> <height> <monitor>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'screenshot', args[2:])
    return 0

def teleportCmd(ctx, args):
    if (len(args) < 3):
        print "usage: teleport name host:port <password>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'teleport', args[2:])
    return 0

def portalsettings(ctx,mach,args):
    enabled = args[0]
    mach.teleporterEnabled = enabled
    if enabled:
        port = args[1]
        passwd = args[2]
        mach.teleporterPort = port
        mach.teleporterPassword = passwd

def openportalCmd(ctx, args):
    if (len(args) < 3):
        print "usage: openportal name port <password>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    port = int(args[2])
    if (len(args) > 3):
        passwd = args[3]
    else:
        passwd = ""
    if not mach.teleporterEnabled or mach.teleporterPort != port or passwd:
        cmdClosedVm(ctx, mach, portalsettings, [True, port, passwd])
    startVm(ctx, mach, "gui")
    return 0

def closeportalCmd(ctx, args):
    if (len(args) < 2):
        print "usage: closeportal name"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    if mach.teleporterEnabled:
        cmdClosedVm(ctx, mach, portalsettings, [False])
    return 0

def gueststatsCmd(ctx, args):
    if (len(args) < 2):
        print "usage: gueststats name <check interval>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'gueststats', args[2:])
    return 0

def plugcpu(ctx,mach,args):
    plug = args[0]
    cpu = args[1]
    if plug:
        print "Adding CPU %d..." %(cpu)
        mach.hotPlugCPU(cpu)
    else:
        print "Removing CPU %d..." %(cpu)
        mach.hotUnplugCPU(cpu)

def plugcpuCmd(ctx, args):
    if (len(args) < 2):
        print "usage: plugcpu name cpuid"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    if str(mach.sessionState) != str(ctx['const'].SessionState_Locked):
        if mach.CPUHotPlugEnabled:
            cmdClosedVm(ctx, mach, plugcpu, [True, int(args[2])])
    else:
        cmdExistingVm(ctx, mach, 'plugcpu', args[2])
    return 0

def unplugcpuCmd(ctx, args):
    if (len(args) < 2):
        print "usage: unplugcpu name cpuid"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    if str(mach.sessionState) != str(ctx['const'].SessionState_Locked):
        if mach.CPUHotPlugEnabled:
            cmdClosedVm(ctx, mach, plugcpu, [False, int(args[2])])
    else:
        cmdExistingVm(ctx, mach, 'unplugcpu', args[2])
    return 0

def setvar(ctx,mach,args):
    expr = 'mach.'+args[0]+' = '+args[1]
    print "Executing",expr
    exec expr

def setvarCmd(ctx, args):
    if (len(args) < 4):
        print "usage: setvar [vmname|uuid] expr value"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdClosedVm(ctx, mach, setvar, args[2:])
    return 0

def setvmextra(ctx,mach,args):
    key = args[0]
    value = args[1]
    print "%s: setting %s to %s" %(mach.name, key, value)
    mach.setExtraData(key, value)

def setExtraDataCmd(ctx, args):
    if (len(args) < 3):
        print "usage: setextra [vmname|uuid|global] key <value>"
        return 0
    key = args[2]
    if len(args) == 4:
        value = args[3]
    else:
        value = None
    if args[1] == 'global':
        ctx['vb'].setExtraData(key, value)
        return 0

    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdClosedVm(ctx, mach, setvmextra, [key, value])
    return 0

def printExtraKey(obj, key, value):
    print "%s: '%s' = '%s'" %(obj, key, value)

def getExtraDataCmd(ctx, args):
    if (len(args) < 2):
        print "usage: getextra [vmname|uuid|global] <key>"
        return 0
    if len(args) == 3:
        key = args[2]
    else:
        key = None

    if args[1] == 'global':
        obj = ctx['vb']
    else:
        obj = argsToMach(ctx,args)
        if obj == None:
            return 0

    if key == None:
        keys = obj.getExtraDataKeys()
    else:
        keys = [ key ]
    for k in keys:
        printExtraKey(args[1], k, obj.getExtraData(k))

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
    if (len(args) > 1):
        g_verbose  = (args[1]=='on')
    else:
        g_verbose = not g_verbose
    return 0

def colorsCmd(ctx, args):
    global g_hascolors
    if (len(args) > 1):
        g_hascolors = (args[1]=='on')
    else:
        g_hascolors = not g_hascolors
    return 0

def hostCmd(ctx, args):
   vb = ctx['vb']
   print "VirtualBox version %s" %(colored(vb.version, 'blue'))
   props = vb.systemProperties
   print "Machines: %s" %(colPath(ctx,props.defaultMachineFolder))

   #print "Global shared folders:"
   #for ud in ctx['global'].getArray(vb, 'sharedFolders'):
   #    printSf(ctx,sf)
   host = vb.host
   cnt = host.processorCount
   print colCat(ctx,"Processors:")
   print "  available/online: %d/%d " %(cnt,host.processorOnlineCount)
   for i in range(0,cnt):
       print "  processor #%d speed: %dMHz %s" %(i,host.getProcessorSpeed(i), host.getProcessorDescription(i))

   print colCat(ctx, "RAM:")
   print "  %dM (free %dM)" %(host.memorySize, host.memoryAvailable)
   print colCat(ctx,"OS:");
   print "  %s (%s)" %(host.operatingSystem, host.OSVersion)
   if host.Acceleration3DAvailable:
       print colCat(ctx,"3D acceleration available")
   else:
       print colCat(ctx,"3D acceleration NOT available")

   print colCat(ctx,"Network interfaces:")
   for ni in ctx['global'].getArray(host, 'networkInterfaces'):
       print "  %s (%s)" %(ni.name, ni.IPAddress)

   print colCat(ctx,"DVD drives:")
   for dd in ctx['global'].getArray(host, 'DVDDrives'):
       print "  %s - %s" %(dd.name, dd.description)

   print colCat(ctx,"Floppy drives:")
   for dd in ctx['global'].getArray(host, 'floppyDrives'):
       print "  %s - %s" %(dd.name, dd.description)

   print colCat(ctx,"USB devices:")
   for ud in ctx['global'].getArray(host, 'USBDevices'):
       printHostUsbDev(ctx,ud)

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
    active = False
    cmdExistingVm(ctx, mach, 'guestlambda', [lambda ctx,mach,console,args:  monitorSource(ctx, console.eventSource, active, dur)])
    return 0

def monitorGuestKbdCmd(ctx, args):
    if (len(args) < 2):
        print "usage: monitorGuestKbd name (duration)"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    dur = 5
    if len(args) > 2:
        dur = float(args[2])
    active = False
    cmdExistingVm(ctx, mach, 'guestlambda', [lambda ctx,mach,console,args:  monitorSource(ctx, console.keyboard.eventSource, active, dur)])
    return 0

def monitorGuestMouseCmd(ctx, args):
    if (len(args) < 2):
        print "usage: monitorGuestMouse name (duration)"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    dur = 5
    if len(args) > 2:
        dur = float(args[2])
    active = False
    cmdExistingVm(ctx, mach, 'guestlambda', [lambda ctx,mach,console,args:  monitorSource(ctx, console.mouse.eventSource, active, dur)])
    return 0

def monitorVBoxCmd(ctx, args):
    if (len(args) > 2):
        print "usage: monitorVBox (duration)"
        return 0
    dur = 5
    if len(args) > 1:
        dur = float(args[1])
    vbox = ctx['vb']
    active = False
    monitorSource(ctx, vbox.eventSource, active, dur)
    return 0

def getAdapterType(ctx, type):
    if (type == ctx['global'].constants.NetworkAdapterType_Am79C970A or
        type == ctx['global'].constants.NetworkAdapterType_Am79C973):
        return "pcnet"
    elif (type == ctx['global'].constants.NetworkAdapterType_I82540EM or
          type == ctx['global'].constants.NetworkAdapterType_I82545EM or
          type == ctx['global'].constants.NetworkAdapterType_I82543GC):
        return "e1000"
    elif (type == ctx['global'].constants.NetworkAdapterType_Virtio):
        return "virtio"
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
    session = ctx['global'].openMachineSession(mach)
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
    session.unlockMachine()

    return 0


def showLogCmd(ctx, args):
    if (len(args) < 2):
        print "usage: showLog vm <num>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0

    log = 0
    if (len(args) > 2):
       log  = args[2]

    uOffset = 0
    while True:
        data = mach.readLog(log, uOffset, 4096)
        if (len(data) == 0):
            break
        # print adds either NL or space to chunks not ending with a NL
        sys.stdout.write(str(data))
        uOffset += len(data)

    return 0

def findLogCmd(ctx, args):
    if (len(args) < 3):
        print "usage: findLog vm pattern <num>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0

    log = 0
    if (len(args) > 3):
       log  = args[3]

    pattern = args[2]
    uOffset = 0
    while True:
        # to reduce line splits on buffer boundary
        data = mach.readLog(log, uOffset, 512*1024)
        if (len(data) == 0):
            break
        d = str(data).split("\n")
        for s in d:
            m = re.findall(pattern, s)
            if len(m) > 0:
                for mt in m:
                    s = s.replace(mt, colored(mt,'red'))
                print s
        uOffset += len(data)

    return 0


def findAssertCmd(ctx, args):
    if (len(args) < 2):
        print "usage: findAssert vm <num>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0

    log = 0
    if (len(args) > 2):
       log  = args[2]

    uOffset = 0
    ere = re.compile(r'(Expression:|\!\!\!\!\!\!)')
    active = False
    context = 0
    while True:
        # to reduce line splits on buffer boundary
        data = mach.readLog(log, uOffset, 512*1024)
        if (len(data) == 0):
            break
        d = str(data).split("\n")
        for s in d:
            if active:
                print s
                if context == 0:
                    active = False
                else:
                    context = context - 1
                continue
            m = ere.findall(s)
            if len(m) > 0:
                active = True
                context = 50
                print s
        uOffset += len(data)

    return 0

def evalCmd(ctx, args):
   expr = ' '.join(args[1:])
   try:
        exec expr
   except Exception, e:
        printErr(ctx,e)
        if g_verbose:
            traceback.print_exc()
   return 0

def reloadExtCmd(ctx, args):
   # maybe will want more args smartness
   checkUserExtensions(ctx, commands, getHomeFolder(ctx))
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
        lines = lf.readlines()
        ctx['scriptLine'] = 0
        ctx['interrupt'] = False
        while ctx['scriptLine'] < len(lines):
            line = lines[ctx['scriptLine']]
            ctx['scriptLine'] = ctx['scriptLine'] + 1
            done = runCommand(ctx, line)
            if done != 0 or ctx['interrupt']:
                break

    except Exception,e:
        printErr(ctx,e)
        if g_verbose:
            traceback.print_exc()
    lf.close()
    return 0

def sleepCmd(ctx, args):
    if (len(args) != 2):
        print "usage: sleep <secs>"
        return 0

    try:
        time.sleep(float(args[1]))
    except:
        # to allow sleep interrupt
        pass
    return 0


def shellCmd(ctx, args):
    if (len(args) < 2):
        print "usage: shell <commands>"
        return 0
    cmd = ' '.join(args[1:])

    try:
        os.system(cmd)
    except KeyboardInterrupt:
        # to allow shell command interruption
        pass
    return 0


def connectCmd(ctx, args):
    if (len(args) > 4):
        print "usage: connect url <username> <passwd>"
        return 0

    if ctx['vb'] is not None:
        print "Already connected, disconnect first..."
        return 0

    if (len(args) > 1):
        url = args[1]
    else:
        url = None

    if (len(args) > 2):
        user = args[2]
    else:
        user = ""

    if (len(args) > 3):
        passwd = args[3]
    else:
        passwd = ""

    ctx['wsinfo'] = [url, user, passwd]
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

def reconnectCmd(ctx, args):
    if ctx['wsinfo'] is None:
        print "Never connected..."
        return 0

    try:
        ctx['global'].platform.disconnect()
    except:
        pass

    [url,user,passwd] = ctx['wsinfo']
    ctx['vb'] = ctx['global'].platform.connect(url, user, passwd)
    print "Running VirtualBox version %s" %(ctx['vb'].version)
    return 0

def exportVMCmd(ctx, args):
    import sys

    if len(args) < 3:
        print "usage: exportVm <machine> <path> <format> <license>"
        return 0
    mach = argsToMach(ctx,args)
    if mach is None:
        return 0
    path = args[2]
    if (len(args) > 3):
        format = args[3]
    else:
        format = "ovf-1.0"
    if (len(args) > 4):
        license = args[4]
    else:
        license = "GPL"

    app = ctx['vb'].createAppliance()
    desc = mach.export(app)
    desc.addDescription(ctx['global'].constants.VirtualSystemDescriptionType_License, license, "")
    p = app.write(format, path)
    if (progressBar(ctx, p) and int(p.resultCode) == 0):
        print "Exported to %s in format %s" %(path, format)
    else:
        reportError(ctx,p)
    return 0

# PC XT scancodes
scancodes = {
    'a':  0x1e,
    'b':  0x30,
    'c':  0x2e,
    'd':  0x20,
    'e':  0x12,
    'f':  0x21,
    'g':  0x22,
    'h':  0x23,
    'i':  0x17,
    'j':  0x24,
    'k':  0x25,
    'l':  0x26,
    'm':  0x32,
    'n':  0x31,
    'o':  0x18,
    'p':  0x19,
    'q':  0x10,
    'r':  0x13,
    's':  0x1f,
    't':  0x14,
    'u':  0x16,
    'v':  0x2f,
    'w':  0x11,
    'x':  0x2d,
    'y':  0x15,
    'z':  0x2c,
    '0':  0x0b,
    '1':  0x02,
    '2':  0x03,
    '3':  0x04,
    '4':  0x05,
    '5':  0x06,
    '6':  0x07,
    '7':  0x08,
    '8':  0x09,
    '9':  0x0a,
    ' ':  0x39,
    '-':  0xc,
    '=':  0xd,
    '[':  0x1a,
    ']':  0x1b,
    ';':  0x27,
    '\'': 0x28,
    ',':  0x33,
    '.':  0x34,
    '/':  0x35,
    '\t': 0xf,
    '\n': 0x1c,
    '`':  0x29
};

extScancodes = {
    'ESC' :    [0x01],
    'BKSP':    [0xe],
    'SPACE':   [0x39],
    'TAB':     [0x0f],
    'CAPS':    [0x3a],
    'ENTER':   [0x1c],
    'LSHIFT':  [0x2a],
    'RSHIFT':  [0x36],
    'INS':     [0xe0, 0x52],
    'DEL':     [0xe0, 0x53],
    'END':     [0xe0, 0x4f],
    'HOME':    [0xe0, 0x47],
    'PGUP':    [0xe0, 0x49],
    'PGDOWN':  [0xe0, 0x51],
    'LGUI':    [0xe0, 0x5b], # GUI, aka Win, aka Apple key
    'RGUI':    [0xe0, 0x5c],
    'LCTR':    [0x1d],
    'RCTR':    [0xe0, 0x1d],
    'LALT':    [0x38],
    'RALT':    [0xe0, 0x38],
    'APPS':    [0xe0, 0x5d],
    'F1':      [0x3b],
    'F2':      [0x3c],
    'F3':      [0x3d],
    'F4':      [0x3e],
    'F5':      [0x3f],
    'F6':      [0x40],
    'F7':      [0x41],
    'F8':      [0x42],
    'F9':      [0x43],
    'F10':     [0x44 ],
    'F11':     [0x57],
    'F12':     [0x58],
    'UP':      [0xe0, 0x48],
    'LEFT':    [0xe0, 0x4b],
    'DOWN':    [0xe0, 0x50],
    'RIGHT':   [0xe0, 0x4d],
};

def keyDown(ch):
    code = scancodes.get(ch, 0x0)
    if code != 0:
        return [code]
    extCode = extScancodes.get(ch, [])
    if len(extCode) == 0:
        print "bad ext",ch
    return extCode

def keyUp(ch):
    codes = keyDown(ch)[:] # make a copy
    if len(codes) > 0:
        codes[len(codes)-1] += 0x80
    return codes

def typeInGuest(console, text, delay):
    import time
    pressed = []
    group = False
    modGroupEnd = True
    i = 0
    kbd = console.keyboard
    while i < len(text):
        ch = text[i]
        i = i+1
        if ch == '{':
            # start group, all keys to be pressed at the same time
            group = True
            continue
        if ch == '}':
            # end group, release all keys
            for c in pressed:
                 kbd.putScancodes(keyUp(c))
            pressed = []
            group = False
            continue
        if ch == 'W':
            # just wait a bit
            time.sleep(0.3)
            continue
        if  ch == '^' or  ch == '|' or ch == '$' or ch == '_':
            if ch == '^':
                ch = 'LCTR'
            if ch == '|':
                ch = 'LSHIFT'
            if ch == '_':
                ch = 'LALT'
            if ch == '$':
                ch = 'LGUI'
            if not group:
                modGroupEnd = False
        else:
            if ch == '\\':
                if i < len(text):
                    ch = text[i]
                    i = i+1
                    if ch == 'n':
                        ch = '\n'
            elif ch == '&':
                combo = ""
                while i  < len(text):
                    ch = text[i]
                    i = i+1
                    if ch == ';':
                        break
                    combo += ch
                ch = combo
            modGroupEnd = True
        kbd.putScancodes(keyDown(ch))
        pressed.insert(0, ch)
        if not group and modGroupEnd:
            for c in pressed:
                kbd.putScancodes(keyUp(c))
            pressed = []
            modGroupEnd = True
        time.sleep(delay)

def typeGuestCmd(ctx, args):
    import sys

    if len(args) < 3:
        print "usage: typeGuest <machine> <text> <charDelay>"
        return 0
    mach =  argsToMach(ctx,args)
    if mach is None:
        return 0

    text = args[2]

    if len(args) > 3:
        delay = float(args[3])
    else:
        delay = 0.1

    gargs = [lambda ctx,mach,console,args: typeInGuest(console, text, delay)]
    cmdExistingVm(ctx, mach, 'guestlambda', gargs)

    return 0

def optId(verbose,id):
   if verbose:
      return ": "+id
   else:
      return ""

def asSize(val,inBytes):
   if inBytes:
      return int(val)/(1024*1024)
   else:
      return int(val)

def listMediaCmd(ctx,args):
   if len(args) > 1:
      verbose = int(args[1])
   else:
      verbose = False
   hdds = ctx['global'].getArray(ctx['vb'], 'hardDisks')
   print colCat(ctx,"Hard disks:")
   for hdd in hdds:
       if hdd.state != ctx['global'].constants.MediumState_Created:
           hdd.refreshState()
       print "   %s (%s)%s %s [logical %s]" %(colPath(ctx,hdd.location), hdd.format, optId(verbose,hdd.id),colSizeM(ctx,asSize(hdd.size, True)), colSizeM(ctx,asSize(hdd.logicalSize, True)))

   dvds = ctx['global'].getArray(ctx['vb'], 'DVDImages')
   print colCat(ctx,"CD/DVD disks:")
   for dvd in dvds:
       if dvd.state != ctx['global'].constants.MediumState_Created:
           dvd.refreshState()
       print "   %s (%s)%s %s" %(colPath(ctx,dvd.location), dvd.format,optId(verbose,dvd.id),colSizeM(ctx,asSize(dvd.size, True)))

   floppys = ctx['global'].getArray(ctx['vb'], 'floppyImages')
   print colCat(ctx,"Floppy disks:")
   for floppy in floppys:
       if floppy.state != ctx['global'].constants.MediumState_Created:
           floppy.refreshState()
       print "   %s (%s)%s %s" %(colPath(ctx,floppy.location), floppy.format,optId(verbose,floppy.id), colSizeM(ctx,asSize(floppy.size, True)))

   return 0

def listUsbCmd(ctx,args):
   if (len(args) > 1):
      print "usage: listUsb"
      return 0

   host = ctx['vb'].host
   for ud in ctx['global'].getArray(host, 'USBDevices'):
       printHostUsbDev(ctx,ud)

   return 0

def findDevOfType(ctx,mach,type):
    atts = ctx['global'].getArray(mach, 'mediumAttachments')
    for a in atts:
        if a.type == type:
            return [a.controller, a.port, a.device]
    return [None, 0, 0]

def createHddCmd(ctx,args):
   if (len(args) < 3):
      print "usage: createHdd sizeM location type"
      return 0

   size = int(args[1])
   loc = args[2]
   if len(args) > 3:
      format = args[3]
   else:
      format = "vdi"

   hdd = ctx['vb'].createHardDisk(format, loc)
   progress = hdd.createBaseStorage(size, ctx['global'].constants.MediumVariant_Standard)
   if progressBar(ctx,progress) and hdd.id:
       print "created HDD at %s as %s" %(colPath(ctx,hdd.location), hdd.id)
   else:
      print "cannot create disk (file %s exist?)" %(loc)
      reportError(ctx,progress)
      return 0

   return 0

def registerHddCmd(ctx,args):
   if (len(args) < 2):
      print "usage: registerHdd location"
      return 0

   vb = ctx['vb']
   loc = args[1]
   setImageId = False
   imageId = ""
   setParentId = False
   parentId = ""
   hdd = vb.openMedium(loc, ctx['global'].constants.DeviceType_HardDisk, ctx['global'].constants.AccessMode_ReadWrite)
   print "registered HDD as %s" %(hdd.id)
   return 0

def controldevice(ctx,mach,args):
    [ctr,port,slot,type,id] = args
    mach.attachDevice(ctr, port, slot,type,id)

def attachHddCmd(ctx,args):
   if (len(args) < 3):
      print "usage: attachHdd vm hdd controller port:slot"
      return 0

   mach = argsToMach(ctx,args)
   if mach is None:
        return 0
   vb = ctx['vb']
   loc = args[2]
   try:
      hdd = vb.findMedium(loc, ctx['global'].constants.DeviceType_HardDisk)
   except:
      print "no HDD with path %s registered" %(loc)
      return 0
   if len(args) > 3:
       ctr = args[3]
       (port,slot) = args[4].split(":")
   else:
       [ctr, port, slot] = findDevOfType(ctx, mach, ctx['global'].constants.DeviceType_HardDisk)

   cmdClosedVm(ctx, mach, lambda ctx,mach,args: mach.attachDevice(ctr, port, slot, ctx['global'].constants.DeviceType_HardDisk,hdd.id))
   return 0

def detachVmDevice(ctx,mach,args):
    atts = ctx['global'].getArray(mach, 'mediumAttachments')
    hid = args[0]
    for a in atts:
        if a.medium:
            if hid == "ALL" or a.medium.id == hid:
                mach.detachDevice(a.controller, a.port, a.device)

def detachMedium(ctx,mid,medium):
    cmdClosedVm(ctx, machById(ctx, mid), detachVmDevice, [medium])

def detachHddCmd(ctx,args):
   if (len(args) < 3):
      print "usage: detachHdd vm hdd"
      return 0

   mach = argsToMach(ctx,args)
   if mach is None:
        return 0
   vb = ctx['vb']
   loc = args[2]
   try:
      hdd = vb.findMedium(loc, ctx['global'].constants.DeviceType_HardDisk)
   except:
      print "no HDD with path %s registered" %(loc)
      return 0

   detachMedium(ctx, mach.id, hdd)
   return 0

def unregisterHddCmd(ctx,args):
   if (len(args) < 2):
      print "usage: unregisterHdd path <vmunreg>"
      return 0

   vb = ctx['vb']
   loc = args[1]
   if (len(args) > 2):
      vmunreg = int(args[2])
   else:
      vmunreg = 0
   try:
      hdd = vb.findMedium(loc, ctx['global'].constants.DeviceType_HardDisk)
   except:
      print "no HDD with path %s registered" %(loc)
      return 0

   if vmunreg != 0:
      machs = ctx['global'].getArray(hdd, 'machineIds')
      try:
         for m in machs:
            print "Trying to detach from %s" %(m)
            detachMedium(ctx, m, hdd)
      except Exception, e:
         print 'failed: ',e
         return 0
   hdd.close()
   return 0

def removeHddCmd(ctx,args):
   if (len(args) != 2):
      print "usage: removeHdd path"
      return 0

   vb = ctx['vb']
   loc = args[1]
   try:
      hdd = vb.findMedium(loc, ctx['global'].constants.DeviceType_HardDisk)
   except:
      print "no HDD with path %s registered" %(loc)
      return 0

   progress = hdd.deleteStorage()
   progressBar(ctx,progress)

   return 0

def registerIsoCmd(ctx,args):
   if (len(args) < 2):
      print "usage: registerIso location"
      return 0
   vb = ctx['vb']
   loc = args[1]
   iso = vb.openMedium(loc, ctx['global'].constants.DeviceType_DVD, ctx['global'].constants.AccessMode_ReadOnly)
   print "registered ISO as %s" %(iso.id)
   return 0

def unregisterIsoCmd(ctx,args):
   if (len(args) != 2):
      print "usage: unregisterIso path"
      return 0

   vb = ctx['vb']
   loc = args[1]
   try:
      dvd = vb.findMedium(loc, ctx['global'].constants.DeviceType_DVD)
   except:
      print "no DVD with path %s registered" %(loc)
      return 0

   progress = dvd.close()
   print "Unregistered ISO at %s" %(colPath(ctx,loc))

   return 0

def removeIsoCmd(ctx,args):
   if (len(args) != 2):
      print "usage: removeIso path"
      return 0

   vb = ctx['vb']
   loc = args[1]
   try:
      dvd = vb.findMedium(loc, ctx['global'].constants.DeviceType_DVD)
   except:
      print "no DVD with path %s registered" %(loc)
      return 0

   progress = dvd.deleteStorage()
   if progressBar(ctx,progress):
       print "Removed ISO at %s" %(colPath(ctx,dvd.location))
   else:
       reportError(ctx,progress)
   return 0

def attachIsoCmd(ctx,args):
   if (len(args) < 3):
      print "usage: attachIso vm iso controller port:slot"
      return 0

   mach = argsToMach(ctx,args)
   if mach is None:
        return 0
   vb = ctx['vb']
   loc = args[2]
   try:
      dvd = vb.findMedium(loc, ctx['global'].constants.DeviceType_DVD)
   except:
      print "no DVD with path %s registered" %(loc)
      return 0
   if len(args) > 3:
       ctr = args[3]
       (port,slot) = args[4].split(":")
   else:
       [ctr, port, slot] = findDevOfType(ctx, mach, ctx['global'].constants.DeviceType_DVD)
   cmdClosedVm(ctx, mach, lambda ctx,mach,args: mach.attachDevice(ctr, port, slot, ctx['global'].constants.DeviceType_DVD, dvd))
   return 0

def detachIsoCmd(ctx,args):
   if (len(args) < 3):
      print "usage: detachIso vm iso"
      return 0

   mach =  argsToMach(ctx,args)
   if mach is None:
        return 0
   vb = ctx['vb']
   loc = args[2]
   try:
      dvd = vb.findMedium(loc, ctx['global'].constants.DeviceType_DVD)
   except:
      print "no DVD with path %s registered" %(loc)
      return 0

   detachMedium(ctx, mach.id, dvd)
   return 0

def mountIsoCmd(ctx,args):
   if (len(args) < 3):
      print "usage: mountIso vm iso controller port:slot"
      return 0

   mach = argsToMach(ctx,args)
   if mach is None:
        return 0
   vb = ctx['vb']
   loc = args[2]
   try:
      dvd = vb.findMedium(loc, ctx['global'].constants.DeviceType_DVD)
   except:
      print "no DVD with path %s registered" %(loc)
      return 0

   if len(args) > 3:
       ctr = args[3]
       (port,slot) = args[4].split(":")
   else:
       # autodetect controller and location, just find first controller with media == DVD
       [ctr, port, slot] = findDevOfType(ctx, mach, ctx['global'].constants.DeviceType_DVD)

   cmdExistingVm(ctx, mach, 'mountiso', [ctr, port, slot, dvd, True])

   return 0

def unmountIsoCmd(ctx,args):
   if (len(args) < 2):
      print "usage: unmountIso vm controller port:slot"
      return 0

   mach = argsToMach(ctx,args)
   if mach is None:
        return 0
   vb = ctx['vb']

   if len(args) > 3:
       ctr = args[2]
       (port,slot) = args[3].split(":")
   else:
       # autodetect controller and location, just find first controller with media == DVD
       [ctr, port, slot] = findDevOfType(ctx, mach, ctx['global'].constants.DeviceType_DVD)

   cmdExistingVm(ctx, mach, 'mountiso', [ctr, port, slot, None, True])

   return 0

def attachCtr(ctx,mach,args):
    [name, bus, type] = args
    ctr = mach.addStorageController(name, bus)
    if type != None:
        ctr.controllerType = type

def attachCtrCmd(ctx,args):
   if (len(args) < 4):
      print "usage: attachCtr vm cname bus <type>"
      return 0

   if len(args) > 4:
       type = enumFromString(ctx,'StorageControllerType', args[4])
       if type == None:
           print "Controller type %s unknown" %(args[4])
           return 0
   else:
       type = None

   mach = argsToMach(ctx,args)
   if mach is None:
        return 0
   bus = enumFromString(ctx,'StorageBus', args[3])
   if bus is None:
       print "Bus type %s unknown" %(args[3])
       return 0
   name = args[2]
   cmdClosedVm(ctx, mach, attachCtr, [name, bus, type])
   return 0

def detachCtrCmd(ctx,args):
   if (len(args) < 3):
      print "usage: detachCtr vm name"
      return 0

   mach = argsToMach(ctx,args)
   if mach is None:
        return 0
   ctr = args[2]
   cmdClosedVm(ctx, mach, lambda ctx,mach,args: mach.removeStorageController(ctr))
   return 0

def usbctr(ctx,mach,console,args):
    if (args[0]):
        console.attachUSBDevice(args[1])
    else:
        console.detachUSBDevice(args[1])

def attachUsbCmd(ctx,args):
   if (len(args) < 3):
      print "usage: attachUsb vm deviceuid"
      return 0

   mach = argsToMach(ctx,args)
   if mach is None:
        return 0
   dev = args[2]
   cmdExistingVm(ctx, mach, 'guestlambda', [usbctr,True,dev])
   return 0

def detachUsbCmd(ctx,args):
   if (len(args) < 3):
      print "usage: detachUsb vm deviceuid"
      return 0

   mach = argsToMach(ctx,args)
   if mach is None:
        return 0
   dev = args[2]
   cmdExistingVm(ctx, mach, 'guestlambda', [usbctr,False,dev])
   return 0


def guiCmd(ctx,args):
   if (len(args) > 1):
      print "usage: gui"
      return 0

   binDir = ctx['global'].getBinDir()

   vbox = os.path.join(binDir, 'VirtualBox')
   try:
        os.system(vbox)
   except KeyboardInterrupt:
        # to allow interruption
        pass
   return 0

def shareFolderCmd(ctx,args):
    if (len(args) < 4):
        print "usage: shareFolder vm path name <writable> <persistent>"
        return 0

    mach = argsToMach(ctx,args)
    if mach is None:
        return 0
    path = args[2]
    name = args[3]
    writable = False
    persistent = False
    if len(args) > 4:
        for a in args[4:]:
            if a == 'writable':
                writable = True
            if a == 'persistent':
                persistent = True
    if persistent:
        cmdClosedVm(ctx, mach, lambda ctx,mach,args: mach.createSharedFolder(name, path, writable), [])
    else:
        cmdExistingVm(ctx, mach, 'guestlambda', [lambda ctx,mach,console,args: console.createSharedFolder(name, path, writable)])
    return 0

def unshareFolderCmd(ctx,args):
    if (len(args) < 3):
        print "usage: unshareFolder vm name"
        return 0

    mach = argsToMach(ctx,args)
    if mach is None:
        return 0
    name = args[2]
    found = False
    for sf in ctx['global'].getArray(mach, 'sharedFolders'):
        if sf.name == name:
            cmdClosedVm(ctx, mach, lambda ctx,mach,args: mach.removeSharedFolder(name), [])
            found = True
            break
    if not found:
        cmdExistingVm(ctx, mach, 'guestlambda', [lambda ctx,mach,console,args: console.removeSharedFolder(name)])
    return 0


def snapshotCmd(ctx,args):
    if (len(args) < 2 or args[1] == 'help'):
        print "Take snapshot:    snapshot vm take name <description>"
        print "Restore snapshot: snapshot vm restore name"
        print "Merge snapshot:   snapshot vm merge name"
        return 0

    mach = argsToMach(ctx,args)
    if mach is None:
        return 0
    cmd = args[2]
    if cmd == 'take':
        if (len(args) < 4):
            print "usage: snapshot vm take name <description>"
            return 0
        name = args[3]
        if (len(args) > 4):
            desc = args[4]
        else:
            desc = ""
        cmdAnyVm(ctx, mach, lambda ctx,mach,console,args: progressBar(ctx, console.takeSnapshot(name,desc)))
        return 0

    if cmd == 'restore':
        if (len(args) < 4):
            print "usage: snapshot vm restore name"
            return 0
        name = args[3]
        snap = mach.findSnapshot(name)
        cmdAnyVm(ctx, mach, lambda ctx,mach,console,args: progressBar(ctx, console.restoreSnapshot(snap)))
        return 0

    if cmd == 'restorecurrent':
        if (len(args) < 4):
            print "usage: snapshot vm restorecurrent"
            return 0
        snap = mach.currentSnapshot()
        cmdAnyVm(ctx, mach, lambda ctx,mach,console,args: progressBar(ctx, console.restoreSnapshot(snap)))
        return 0

    if cmd == 'delete':
        if (len(args) < 4):
            print "usage: snapshot vm delete name"
            return 0
        name = args[3]
        snap = mach.findSnapshot(name)
        cmdAnyVm(ctx, mach, lambda ctx,mach,console,args: progressBar(ctx, console.deleteSnapshot(snap.id)))
        return 0

    print "Command '%s' is unknown" %(cmd)
    return 0

def natAlias(ctx, mach, nicnum, nat, args=[]):
    """This command shows/alters NAT's alias settings.
    usage: nat <vm> <nicnum> alias [default|[log] [proxyonly] [sameports]]
    default - set settings to default values
    log - switch on alias logging
    proxyonly - switch proxyonly mode on
    sameports - enforces NAT using the same ports
    """
    alias = {
        'log': 0x1,
        'proxyonly': 0x2,
        'sameports': 0x4
    }
    if len(args) == 1:
        first = 0
        msg = ''
        for aliasmode, aliaskey in alias.iteritems():
            if first == 0:
                first = 1
            else:
                msg += ', '
            if int(nat.aliasMode) & aliaskey:
                msg += '%d: %s' % (aliasmode, 'on')
            else:
                msg += '%d: %s' % (aliasmode, 'off')
        msg += ')'
        return (0, [msg])
    else:
        nat.aliasMode = 0
        if 'default' not in args:
            for a in range(1, len(args)):
                if not alias.has_key(args[a]):
                    print 'Invalid alias mode: ' + args[a]
                    print natAlias.__doc__
                    return (1, None)
                nat.aliasMode = int(nat.aliasMode) | alias[args[a]];
    return (0, None)

def natSettings(ctx, mach, nicnum, nat, args):
    """This command shows/alters NAT settings.
    usage: nat <vm> <nicnum> settings [<mtu> [[<socsndbuf> <sockrcvbuf> [<tcpsndwnd> <tcprcvwnd>]]]]
    mtu - set mtu <= 16000
    socksndbuf/sockrcvbuf - sets amount of kb for socket sending/receiving buffer
    tcpsndwnd/tcprcvwnd - sets size of initial tcp sending/receiving window
    """
    if len(args) == 1:
        (mtu, socksndbuf, sockrcvbuf, tcpsndwnd, tcprcvwnd) = nat.getNetworkSettings();
        if mtu == 0: mtu = 1500
        if socksndbuf == 0: socksndbuf = 64
        if sockrcvbuf == 0: sockrcvbuf = 64
        if tcpsndwnd == 0: tcpsndwnd = 64
        if tcprcvwnd == 0: tcprcvwnd = 64
        msg = 'mtu:%s socket(snd:%s, rcv:%s) tcpwnd(snd:%s, rcv:%s)' % (mtu, socksndbuf, sockrcvbuf, tcpsndwnd, tcprcvwnd);
        return (0, [msg])
    else:
        if args[1] < 16000:
            print 'invalid mtu value (%s not in range [65 - 16000])' % (args[1])
            return (1, None)
        for i in range(2, len(args)):
            if not args[i].isdigit() or int(args[i]) < 8 or int(args[i]) > 1024:
                print 'invalid %s parameter (%i not in range [8-1024])' % (i, args[i])
                return (1, None)
        a = [args[1]]
        if len(args) < 6:
            for i in range(2, len(args)): a.append(args[i])
            for i in range(len(args), 6): a.append(0)
        else:
            for i in range(2, len(args)): a.append(args[i])
        #print a
        nat.setNetworkSettings(int(a[0]), int(a[1]), int(a[2]), int(a[3]), int(a[4]))
    return (0, None)

def natDns(ctx, mach, nicnum, nat, args):
    """This command shows/alters DNS's NAT settings
    usage: nat <vm> <nicnum> dns [passdomain] [proxy] [usehostresolver]
    passdomain - enforces builtin DHCP server to pass domain
    proxy - switch on builtin NAT DNS proxying mechanism
    usehostresolver - proxies all DNS requests to Host Resolver interface
    """
    yesno = {0: 'off', 1: 'on'}
    if len(args) == 1:
        msg = 'passdomain:%s, proxy:%s, usehostresolver:%s' % (yesno[int(nat.dnsPassDomain)], yesno[int(nat.dnsProxy)], yesno[int(nat.dnsUseHostResolver)])
        return (0, [msg])
    else:
        nat.dnsPassDomain = 'passdomain' in args
        nat.dnsProxy =  'proxy' in args
        nat.dnsUseHostResolver =  'usehostresolver' in args
    return (0, None)

def natTftp(ctx, mach, nicnum, nat, args):
    """This command shows/alters TFTP settings
    usage nat <vm> <nicnum> tftp [prefix <prefix>| bootfile <bootfile>| server <server>]
    prefix - alters prefix TFTP settings
    bootfile - alters bootfile TFTP settings
    server - sets booting server
    """
    if len(args) == 1:
        server = nat.tftpNextServer
        if server is None:
            server = nat.network
            if server is None:
                server = '10.0.%d/24' % (int(nicnum) + 2)
            (server,mask) = server.split('/')
            while server.count('.') != 3:
                server += '.0'
            (a,b,c,d) = server.split('.')
            server = '%d.%d.%d.4' % (a,b,c)
        prefix = nat.tftpPrefix
        if prefix is None:
            prefix = '%s/TFTP/' % (ctx['vb'].homeFolder)
        bootfile = nat.tftpBootFile
        if bootfile is None:
            bootfile = '%s.pxe' % (mach.name)
        msg = 'server:%s, prefix:%s, bootfile:%s' % (server, prefix, bootfile)
        return (0, [msg])
    else:

        cmd = args[1]
        if len(args) != 3:
            print 'invalid args:', args
            print natTftp.__doc__
            return (1, None)
        if cmd == 'prefix': nat.tftpPrefix = args[2]
        elif cmd == 'bootfile': nat.tftpBootFile = args[2]
        elif cmd == 'server': nat.tftpNextServer = args[2]
        else:
            print "invalid cmd:", cmd
            return (1, None)
    return (0, None)

def natPortForwarding(ctx, mach, nicnum, nat, args):
    """This command shows/manages port-forwarding settings
    usage:
        nat <vm> <nicnum> <pf> [ simple tcp|udp <hostport> <guestport>]
            |[no_name tcp|udp <hostip> <hostport> <guestip> <guestport>]
            |[ex tcp|udp <pf-name> <hostip> <hostport> <guestip> <guestport>]
            |[delete <pf-name>]
    """
    if len(args) == 1:
        # note: keys/values are swapped in defining part of the function
        proto = {0: 'udp', 1: 'tcp'}
        msg = []
        pfs = ctx['global'].getArray(nat, 'redirects')
        for pf in pfs:
            (pfnme, pfp, pfhip, pfhp, pfgip, pfgp) = str(pf).split(',')
            msg.append('%s: %s %s:%s => %s:%s' % (pfnme, proto[int(pfp)], pfhip, pfhp, pfgip, pfgp))
        return (0, msg) # msg is array
    else:
        proto = {'udp': 0, 'tcp': 1}
        pfcmd = {
            'simple': {
                'validate': lambda: args[1] in pfcmd.keys() and args[2] in proto.keys() and len(args) == 5,
                'func':lambda: nat.addRedirect('', proto[args[2]], '', int(args[3]), '', int(args[4]))
            },
            'no_name': {
                'validate': lambda: args[1] in pfcmd.keys() and args[2] in proto.keys() and len(args) == 7,
                'func': lambda: nat.addRedirect('', proto[args[2]], args[3], int(args[4]), args[5], int(args[6]))
            },
            'ex': {
                'validate': lambda: args[1] in pfcmd.keys() and args[2] in proto.keys() and len(args) == 8,
                'func': lambda: nat.addRedirect(args[3], proto[args[2]], args[4], int(args[5]), args[6], int(args[7]))
            },
            'delete': {
                'validate': lambda: len(args) == 3,
                'func': lambda: nat.removeRedirect(args[2])
            }
        }

        if not pfcmd[args[1]]['validate']():
            print 'invalid port-forwarding or args of sub command ', args[1]
            print natPortForwarding.__doc__
            return (1, None)

        a = pfcmd[args[1]]['func']()
    return (0, None)

def natNetwork(ctx, mach, nicnum, nat, args):
    """This command shows/alters NAT network settings
    usage: nat <vm> <nicnum> network [<network>]
    """
    if len(args) == 1:
        if nat.network is not None and len(str(nat.network)) != 0:
            msg = '\'%s\'' % (nat.network)
        else:
            msg = '10.0.%d.0/24' % (int(nicnum) + 2)
        return (0, [msg])
    else:
        (addr, mask) = args[1].split('/')
        if addr.count('.') > 3 or int(mask) < 0 or int(mask) > 32:
            print 'Invalid arguments'
            return (1, None)
        nat.network = args[1]
    return (0, None)

def natCmd(ctx, args):
    """This command is entry point to NAT settins management
    usage: nat <vm> <nicnum> <cmd> <cmd-args>
    cmd - [alias|settings|tftp|dns|pf|network]
    for more information about commands:
    nat help <cmd>
    """

    natcommands = {
        'alias' : natAlias,
        'settings' : natSettings,
        'tftp': natTftp,
        'dns': natDns,
        'pf': natPortForwarding,
        'network': natNetwork
    }

    if len(args) < 2 or args[1] == 'help':
        if len(args) > 2:
            print natcommands[args[2]].__doc__
        else:
            print natCmd.__doc__
        return 0
    if len(args) == 1 or len(args) < 4 or args[3] not in natcommands:
        print natCmd.__doc__
        return 0
    mach = ctx['argsToMach'](args)
    if mach == None:
        print "please specify vm"
        return 0
    if len(args) < 3 or not args[2].isdigit() or int(args[2]) not in range(0, ctx['vb'].systemProperties.getMaxNetworkAdapters(mach.chipsetType)):
        print 'please specify adapter num %d isn\'t in range [0-%d]' % (args[2], ctx['vb'].systemProperties.getMaxNetworkAdapters(mach.chipsetType))
        return 0
    nicnum = int(args[2])
    cmdargs = []
    for i in range(3, len(args)):
        cmdargs.append(args[i])

    # @todo vvl if nicnum is missed but command is entered
    # use NAT func for every adapter on machine.
    func = args[3]
    rosession = 1
    session = None
    if len(cmdargs) > 1:
        rosession = 0
        session = ctx['global'].openMachineSession(mach, False);
        mach = session.machine;

    adapter = mach.getNetworkAdapter(nicnum)
    natEngine = adapter.natDriver
    (rc, report) = natcommands[func](ctx, mach, nicnum, natEngine, cmdargs)
    if rosession == 0:
        if rc == 0:
            mach.saveSettings()
        session.unlockMachine()
    elif report is not None:
        for r in report:
            msg ='%s nic%d %s: %s' % (mach.name, nicnum, func, r)
            print msg
    return 0

def nicSwitchOnOff(adapter, attr, args):
    if len(args) == 1:
        yesno = {0: 'off', 1: 'on'}
        r = yesno[int(adapter.__getattr__(attr))]
        return (0, r)
    else:
        yesno = {'off' : 0, 'on' : 1}
        if args[1] not in yesno:
            print '%s isn\'t acceptable, please choose %s' % (args[1], yesno.keys())
            return (1, None)
        adapter.__setattr__(attr, yesno[args[1]])
    return (0, None)

def nicTraceSubCmd(ctx, vm, nicnum, adapter, args):
    '''
    usage: nic <vm> <nicnum> trace [on|off [file]]
    '''
    (rc, r) = nicSwitchOnOff(adapter, 'traceEnabled', args)
    if len(args) == 1 and rc == 0:
        r = '%s file:%s' % (r, adapter.traceFile)
        return (0, r)
    elif len(args) == 3 and rc == 0:
        adapter.traceFile = args[2]
    return (0, None)

def nicLineSpeedSubCmd(ctx, vm, nicnum, adapter, args):
    if len(args) == 1:
        r = '%d kbps'%(adapter.lineSpeed)
        return (0, r)
    else:
        if not args[1].isdigit():
            print '%s isn\'t a number' % (args[1])
            print (1, None)
        adapter.lineSpeed = int(args[1])
    return (0, None)

def nicCableSubCmd(ctx, vm, nicnum, adapter, args):
    '''
    usage: nic <vm> <nicnum> cable [on|off]
    '''
    return nicSwitchOnOff(adapter, 'cableConnected', args)

def nicEnableSubCmd(ctx, vm, nicnum, adapter, args):
    '''
    usage: nic <vm> <nicnum> enable [on|off]
    '''
    return nicSwitchOnOff(adapter, 'enabled', args)

def nicTypeSubCmd(ctx, vm, nicnum, adapter, args):
    '''
    usage: nic <vm> <nicnum> type [Am79c970A|Am79c970A|I82540EM|I82545EM|I82543GC|Virtio]
    '''
    if len(args) == 1:
        nictypes = ctx['const'].all_values('NetworkAdapterType')
        for n in nictypes.keys():
            if str(adapter.adapterType) == str(nictypes[n]):
                return (0, str(n))
        return (1, None)
    else:
        nictypes = ctx['const'].all_values('NetworkAdapterType')
        if args[1] not in nictypes.keys():
            print '%s not in acceptable values (%s)' % (args[1], nictypes.keys())
            return (1, None)
        adapter.adapterType = nictypes[args[1]]
    return (0, None)

def nicAttachmentSubCmd(ctx, vm, nicnum, adapter, args):
    '''
    usage: nic <vm> <nicnum> attachment [Null|NAT|Bridged <interface>|Internal <name>|HostOnly <interface>
    '''
    if len(args) == 1:
        nicAttachmentType = {
            ctx['global'].constants.NetworkAttachmentType_Null: ('Null', ''),
            ctx['global'].constants.NetworkAttachmentType_NAT: ('NAT', ''),
            ctx['global'].constants.NetworkAttachmentType_Bridged: ('Bridged', adapter.bridgedInterface),
            ctx['global'].constants.NetworkAttachmentType_Internal: ('Internal', adapter.internalNetwork),
            ctx['global'].constants.NetworkAttachmentType_HostOnly: ('HostOnly', adapter.hostOnlyInterface),
            # @todo show details of the generic network attachment type
            ctx['global'].constants.NetworkAttachmentType_Generic: ('Generic', ''),
        }
        import types
        if type(adapter.attachmentType) != types.IntType:
            t = str(adapter.attachmentType)
        else:
            t = adapter.attachmentType
        (r, p) = nicAttachmentType[t]
        return (0, 'attachment:%s, name:%s' % (r, p))
    else:
        nicAttachmentType = {
            'Null': {
                'v': lambda: len(args) == 2,
                'p': lambda: 'do nothing',
                'f': lambda: ctx['global'].constants.NetworkAttachmentType_Null},
            'NAT': {
                'v': lambda: len(args) == 2,
                'p': lambda: 'do nothing',
                'f': lambda: ctx['global'].constants.NetworkAttachmentType_NAT},
            'Bridged': {
                'v': lambda: len(args) == 3,
                'p': lambda: adapter.__setattr__('bridgedInterface', args[2]),
                'f': lambda: ctx['global'].constants.NetworkAttachmentType_Bridged},
            'Internal': {
                'v': lambda: len(args) == 3,
                'p': lambda: adapter.__setattr__('internalNetwork', args[2]),
                'f': lambda: ctx['global'].constants.NetworkAttachmentType_Internal},
            'HostOnly': {
                'v': lambda: len(args) == 2,
                'p': lambda: adapter.__setattr__('hostOnlyInterface', args[2]),
                'f': lambda: ctx['global'].constants.NetworkAttachmentType_HostOnly},
            # @todo implement setting the properties of a generic attachment
            'Generic': {
                'v': lambda: len(args) == 3,
                'p': lambda: 'do nothing',
                'f': lambda: ctx['global'].constants.NetworkAttachmentType_Generic}
        }
        if args[1] not in nicAttachmentType.keys():
            print '%s not in acceptable values (%s)' % (args[1], nicAttachmentType.keys())
            return (1, None)
        if not nicAttachmentType[args[1]]['v']():
            print nicAttachmentType.__doc__
            return (1, None)
        nicAttachmentType[args[1]]['p']()
        adapter.attachmentType = nicAttachmentType[args[1]]['f']()
    return (0, None)

def nicCmd(ctx, args):
    '''
    This command to manage network adapters
    usage: nic <vm> <nicnum> <cmd> <cmd-args>
    where cmd : attachment, trace, linespeed, cable, enable, type
    '''
    # 'command name':{'runtime': is_callable_at_runtime, 'op': function_name}
    niccomand = {
        'attachment': nicAttachmentSubCmd,
        'trace':  nicTraceSubCmd,
        'linespeed': nicLineSpeedSubCmd,
        'cable': nicCableSubCmd,
        'enable': nicEnableSubCmd,
        'type': nicTypeSubCmd
    }
    if  len(args) < 2 \
        or args[1] == 'help' \
        or (len(args) > 2 and args[3] not in niccomand):
        if len(args) == 3 \
           and args[2] in niccomand:
            print niccomand[args[2]].__doc__
        else:
            print nicCmd.__doc__
        return 0

    vm = ctx['argsToMach'](args)
    if vm is None:
        print 'please specify vm'
        return 0

    if    len(args) < 3 \
       or int(args[2]) not in range(0, ctx['vb'].systemProperties.getMaxNetworkAdapters(vm.chipsetType)):
            print 'please specify adapter num %d isn\'t in range [0-%d]'%(args[2], ctx['vb'].systemProperties.getMaxNetworkAdapters(vm.chipsetType))
            return 0
    nicnum = int(args[2])
    cmdargs = args[3:]
    func = args[3]
    session = None
    session = ctx['global'].openMachineSession(vm)
    vm = session.machine
    adapter = vm.getNetworkAdapter(nicnum)
    (rc, report) = niccomand[func](ctx, vm, nicnum, adapter, cmdargs)
    if rc == 0:
            vm.saveSettings()
    if report is not None:
        print '%s nic %d %s: %s' % (vm.name, nicnum, args[3], report)
    session.unlockMachine()
    return 0


def promptCmd(ctx, args):
    if    len(args) < 2:
        print "Current prompt: '%s'" %(ctx['prompt'])
        return 0

    ctx['prompt'] = args[1]
    return 0

def foreachCmd(ctx, args):
    if len(args) < 3:
        print "usage: foreach scope command, where scope is XPath-like expression //vms/vm[@CPUCount='2']"
        return 0

    scope = args[1]
    cmd = args[2]
    elems = eval_xpath(ctx,scope)
    try:
        for e in elems:
            e.apply(cmd)
    except:
        print "Error executing"
        traceback.print_exc()
    return 0

def foreachvmCmd(ctx, args):
    if len(args) < 2:
        print "foreachvm command <args>"
        return 0
    cmdargs = args[1:]
    cmdargs.insert(1, '')
    for m in getMachines(ctx):
        cmdargs[1] = m.id
        runCommandArgs(ctx, cmdargs)
    return 0

def recordDemoCmd(ctx, args):
    if (len(args) < 3):
        print "usage: recordDemo vm filename (duration)"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    filename = args[2]
    dur = 10000
    if len(args) > 3:
        dur = float(args[3])
    cmdExistingVm(ctx, mach, 'guestlambda', [lambda ctx,mach,console,args:  recordDemo(ctx, console, filename, dur)])
    return 0

def playbackDemoCmd(ctx, args):
    if (len(args) < 3):
        print "usage: playbackDemo vm filename (duration)"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    filename = args[2]
    dur = 10000
    if len(args) > 3:
        dur = float(args[3])
    cmdExistingVm(ctx, mach, 'guestlambda', [lambda ctx,mach,console,args:  playbackDemo(ctx, console, filename, dur)])
    return 0


def pciAddr(ctx,addr):
    str = "%02x:%02x.%d" %(addr >> 8, (addr & 0xff) >> 3, addr & 7)
    return colPci(ctx, str)

def lspci(ctx, console):
    assigned = ctx['global'].getArray(console.machine, 'pciDeviceAssignments')
    for a in assigned:
        if a.isPhysicalDevice:
            print "%s: assigned host device %s guest %s" %(colDev(ctx, a.name), pciAddr(ctx, a.hostAddress), pciAddr(ctx, a.guestAddress))

    atts = ctx['global'].getArray(console, 'attachedPciDevices')
    for a in atts:
        if a.isPhysicalDevice:
            print "%s: physical, guest %s, host %s" %(colDev(ctx, a.name), pciAddr(ctx, a.guestAddress), pciAddr(ctx, a.hostAddress))
        else:
            print "%s: virtual, guest %s" %(colDev(ctx, a.name), pciAddr(ctx, a.guestAddress))
    return

def parsePci(str):
    pcire = re.compile(r'(?P<b>[0-9a-fA-F]+):(?P<d>[0-9a-fA-F]+)\.(?P<f>\d)')
    m = pcire.search(str)
    if m is None:
        return -1
    dict = m.groupdict()
    return ((int(dict['b'], 16)) << 8) | ((int(dict['d'], 16)) << 3) | int(dict['f'])

def lspciCmd(ctx, args):
    if (len(args) < 2):
        print "usage: lspci vm"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    cmdExistingVm(ctx, mach, 'guestlambda', [lambda ctx,mach,console,args:  lspci(ctx, console)])
    return 0

def attachpciCmd(ctx, args):
    if (len(args) < 3):
        print "usage: attachpci vm hostpci <guestpci>"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    hostaddr = parsePci(args[2])
    if hostaddr == -1:
        print "invalid host PCI %s, accepted format 01:02.3 for bus 1, device 2, function 3" %(args[2])
        return 0

    if (len(args) > 3):
        guestaddr = parsePci(args[3])
        if guestaddr == -1:
            print "invalid guest PCI %s, accepted format 01:02.3 for bus 1, device 2, function 3" %(args[3])
            return 0
    else:
        guestaddr = hostaddr
    cmdClosedVm(ctx, mach, lambda ctx,mach,a: mach.attachHostPciDevice(hostaddr, guestaddr, True))
    return 0

def detachpciCmd(ctx, args):
    if (len(args) < 3):
        print "usage: detachpci vm hostpci"
        return 0
    mach = argsToMach(ctx,args)
    if mach == None:
        return 0
    hostaddr = parsePci(args[2])
    if hostaddr == -1:
        print "invalid host PCI %s, accepted format 01:02.3 for bus 1, device 2, function 3" %(args[2])
        return 0

    cmdClosedVm(ctx, mach, lambda ctx,mach,a: mach.detachHostPciDevice(hostaddr))
    return 0

def gotoCmd(ctx, args):
    if (len(args) < 2):
        print "usage: goto line"
        return 0

    line = int(args[1])

    ctx['scriptLine'] = line

    return 0

aliases = {'s':'start',
           'i':'info',
           'l':'list',
           'h':'help',
           'a':'alias',
           'q':'quit', 'exit':'quit',
           'tg': 'typeGuest',
           'v':'verbose'}

commands = {'help':['Prints help information', helpCmd, 0],
            'start':['Start virtual machine by name or uuid: start Linux headless', startCmd, 0],
            'createVm':['Create virtual machine: createVm macvm MacOS', createVmCmd, 0],
            'removeVm':['Remove virtual machine', removeVmCmd, 0],
            'pause':['Pause virtual machine', pauseCmd, 0],
            'resume':['Resume virtual machine', resumeCmd, 0],
            'save':['Save execution state of virtual machine', saveCmd, 0],
            'stats':['Stats for virtual machine', statsCmd, 0],
            'powerdown':['Power down virtual machine', powerdownCmd, 0],
            'powerbutton':['Effectively press power button', powerbuttonCmd, 0],
            'list':['Shows known virtual machines', listCmd, 0],
            'info':['Shows info on machine', infoCmd, 0],
            'ginfo':['Shows info on guest', ginfoCmd, 0],
            'gexec':['Executes program in the guest', gexecCmd, 0],
            'gcopy':['Copy file to the guest', gcopyCmd, 0],
            'gpipe':['Pipe between host and guest', gpipeCmd, 0],
            'alias':['Control aliases', aliasCmd, 0],
            'verbose':['Toggle verbosity', verboseCmd, 0],
            'setvar':['Set VMs variable: setvar Fedora BIOSSettings.ACPIEnabled True', setvarCmd, 0],
            'eval':['Evaluate arbitrary Python construction: eval \'for m in getMachines(ctx): print m.name,"has",m.memorySize,"M"\'', evalCmd, 0],
            'quit':['Exits', quitCmd, 0],
            'host':['Show host information', hostCmd, 0],
            'guest':['Execute command for guest: guest Win32 \'console.mouse.putMouseEvent(20, 20, 0, 0, 0)\'', guestCmd, 0],
            'monitorGuest':['Monitor what happens with the guest for some time: monitorGuest Win32 10', monitorGuestCmd, 0],
            'monitorGuestKbd':['Monitor guest keyboardfor some time: monitorGuestKbd Win32 10', monitorGuestKbdCmd, 0],
            'monitorGuestMouse':['Monitor guest keyboardfor some time: monitorGuestMouse Win32 10', monitorGuestMouseCmd, 0],
            'monitorVBox':['Monitor what happens with Virtual Box for some time: monitorVBox 10', monitorVBoxCmd, 0],
            'portForward':['Setup permanent port forwarding for a VM, takes adapter number host port and guest port: portForward Win32 0 8080 80', portForwardCmd, 0],
            'showLog':['Show log file of the VM, : showLog Win32', showLogCmd, 0],
            'findLog':['Show entries matching pattern in log file of the VM, : findLog Win32 PDM|CPUM', findLogCmd, 0],
            'findAssert':['Find assert in log file of the VM, : findAssert Win32', findAssertCmd, 0],
            'reloadExt':['Reload custom extensions: reloadExt', reloadExtCmd, 0],
            'runScript':['Run VBox script: runScript script.vbox', runScriptCmd, 0],
            'sleep':['Sleep for specified number of seconds: sleep 3.14159', sleepCmd, 0],
            'shell':['Execute external shell command: shell "ls /etc/rc*"', shellCmd, 0],
            'exportVm':['Export VM in OVF format: exportVm Win /tmp/win.ovf', exportVMCmd, 0],
            'screenshot':['Take VM screenshot to a file: screenshot Win /tmp/win.png 1024 768 0', screenshotCmd, 0],
            'teleport':['Teleport VM to another box (see openportal): teleport Win anotherhost:8000 <passwd> <maxDowntime>', teleportCmd, 0],
            'typeGuest':['Type arbitrary text in guest: typeGuest Linux "^lls\\n&UP;&BKSP;ess /etc/hosts\\nq^c" 0.7', typeGuestCmd, 0],
            'openportal':['Open portal for teleportation of VM from another box (see teleport): openportal Win 8000 <passwd>', openportalCmd, 0],
            'closeportal':['Close teleportation portal (see openportal,teleport): closeportal Win', closeportalCmd, 0],
            'getextra':['Get extra data, empty key lists all: getextra <vm|global> <key>', getExtraDataCmd, 0],
            'setextra':['Set extra data, empty value removes key: setextra <vm|global> <key> <value>', setExtraDataCmd, 0],
            'gueststats':['Print available guest stats (only Windows guests with additions so far): gueststats Win32', gueststatsCmd, 0],
            'plugcpu':['Add a CPU to a running VM: plugcpu Win 1', plugcpuCmd, 0],
            'unplugcpu':['Remove a CPU from a running VM (additions required, Windows cannot unplug): unplugcpu Linux 1', unplugcpuCmd, 0],
            'createHdd': ['Create virtual HDD:  createHdd 1000 /disk.vdi ', createHddCmd, 0],
            'removeHdd': ['Permanently remove virtual HDD: removeHdd /disk.vdi', removeHddCmd, 0],
            'registerHdd': ['Register HDD image with VirtualBox instance: registerHdd /disk.vdi', registerHddCmd, 0],
            'unregisterHdd': ['Unregister HDD image with VirtualBox instance: unregisterHdd /disk.vdi', unregisterHddCmd, 0],
            'attachHdd': ['Attach HDD to the VM: attachHdd win /disk.vdi "IDE Controller" 0:1', attachHddCmd, 0],
            'detachHdd': ['Detach HDD from the VM: detachHdd win /disk.vdi', detachHddCmd, 0],
            'registerIso': ['Register CD/DVD image with VirtualBox instance: registerIso /os.iso', registerIsoCmd, 0],
            'unregisterIso': ['Unregister CD/DVD image with VirtualBox instance: unregisterIso /os.iso', unregisterIsoCmd, 0],
            'removeIso': ['Permanently remove CD/DVD image: removeIso /os.iso', removeIsoCmd, 0],
            'attachIso': ['Attach CD/DVD to the VM: attachIso win /os.iso "IDE Controller" 0:1', attachIsoCmd, 0],
            'detachIso': ['Detach CD/DVD from the VM: detachIso win /os.iso', detachIsoCmd, 0],
            'mountIso': ['Mount CD/DVD to the running VM: mountIso win /os.iso "IDE Controller" 0:1', mountIsoCmd, 0],
            'unmountIso': ['Unmount CD/DVD from running VM: unmountIso win "IDE Controller" 0:1', unmountIsoCmd, 0],
            'attachCtr': ['Attach storage controller to the VM: attachCtr win Ctr0 IDE ICH6', attachCtrCmd, 0],
            'detachCtr': ['Detach HDD from the VM: detachCtr win Ctr0', detachCtrCmd, 0],
            'attachUsb': ['Attach USB device to the VM (use listUsb to show available devices): attachUsb win uuid', attachUsbCmd, 0],
            'detachUsb': ['Detach USB device from the VM: detachUsb win uuid', detachUsbCmd, 0],
            'listMedia': ['List media known to this VBox instance', listMediaCmd, 0],
            'listUsb': ['List known USB devices', listUsbCmd, 0],
            'shareFolder': ['Make host\'s folder visible to guest: shareFolder win /share share writable', shareFolderCmd, 0],
            'unshareFolder': ['Remove folder sharing', unshareFolderCmd, 0],
            'gui': ['Start GUI frontend', guiCmd, 0],
            'colors':['Toggle colors', colorsCmd, 0],
            'snapshot':['VM snapshot manipulation, snapshot help for more info', snapshotCmd, 0],
            'nat':['NAT (network address translation engine) manipulation, nat help for more info', natCmd, 0],
            'nic' : ['Network adapter management', nicCmd, 0],
            'prompt' : ['Control shell prompt', promptCmd, 0],
            'foreachvm' : ['Perform command for each VM', foreachvmCmd, 0],
            'foreach' : ['Generic "for each" construction, using XPath-like notation: foreach //vms/vm[@OSTypeId=\'MacOS\'] "print obj.name"', foreachCmd, 0],
            'recordDemo':['Record demo: recordDemo Win32 file.dmo 10', recordDemoCmd, 0],
            'playbackDemo':['Playback demo: playbackDemo Win32 file.dmo 10', playbackDemoCmd, 0],
            'lspci': ['List PCI devices attached to the VM: lspci Win32', lspciCmd, 0],
            'attachpci': ['Attach host PCI device to the VM: attachpci Win32 01:00.0', attachpciCmd, 0],
            'detachpci': ['Detach host PCI device from the VM: detachpci Win32 01:00.0', detachpciCmd, 0],
            'goto': ['Go to line in script (script-only)', gotoCmd, 0]
            }

def runCommandArgs(ctx, args):
    c = args[0]
    if aliases.get(c, None) != None:
        c = aliases[c]
    ci = commands.get(c,None)
    if ci == None:
        print "Unknown command: '%s', type 'help' for list of known commands" %(c)
        return 0
    if ctx['remote'] and ctx['vb'] is None:
        if c not in ['connect', 'reconnect', 'help', 'quit']:
            print "First connect to remote server with %s command." %(colored('connect', 'blue'))
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
# This file also will be read automatically on startup or 'reloadExt'.
#
# Also one can put shell extensions into ~/.VirtualBox/shexts and
# they will also be picked up, so this way one can exchange
# shell extensions easily.
def addExtsFromFile(ctx, cmds, file):
    if not os.path.isfile(file):
        return
    d = {}
    try:
        execfile(file, d, d)
        for (k,v) in d['commands'].items():
            if g_verbose:
                print "customize: adding \"%s\" - %s" %(k, v[0])
            cmds[k] = [v[0], v[1], file]
    except:
        print "Error loading user extensions from %s" %(file)
        traceback.print_exc()


def checkUserExtensions(ctx, cmds, folder):
    folder = str(folder)
    name = os.path.join(folder, "shellext.py")
    addExtsFromFile(ctx, cmds, name)
    # also check 'exts' directory for all files
    shextdir = os.path.join(folder, "shexts")
    if not os.path.isdir(shextdir):
        return
    exts = os.listdir(shextdir)
    for e in exts:
        # not editor temporary files, please.
        if e.endswith('.py'):
            addExtsFromFile(ctx, cmds, os.path.join(shextdir,e))

def getHomeFolder(ctx):
    if ctx['remote'] or ctx['vb'] is None:
        if 'VBOX_USER_HOME' in os.environ:
            return os.path.join(os.environ['VBOX_USER_HOME'])
        return os.path.join(os.path.expanduser("~"), ".VirtualBox")
    else:
        return ctx['vb'].homeFolder

def interpret(ctx):
    if ctx['remote']:
        commands['connect'] = ["Connect to remote VBox instance: connect http://server:18083 user password", connectCmd, 0]
        commands['disconnect'] = ["Disconnect from remote VBox instance", disconnectCmd, 0]
        commands['reconnect'] = ["Reconnect to remote VBox instance", reconnectCmd, 0]
        ctx['wsinfo'] = ["http://localhost:18083", "", ""]

    vbox = ctx['vb']
    if vbox is not None:
        print "Running VirtualBox version %s" %(vbox.version)
        ctx['perf'] = None # ctx['global'].getPerfCollector(vbox)
    else:
        ctx['perf'] = None

    home = getHomeFolder(ctx)
    checkUserExtensions(ctx, commands, home)
    if platform.system() in ['Windows', 'Microsoft']:
        global g_hascolors
        g_hascolors = False
    hist_file=os.path.join(home, ".vboxshellhistory")
    autoCompletion(commands, ctx)

    if g_hasreadline and os.path.exists(hist_file):
        readline.read_history_file(hist_file)

    # to allow to print actual host information, we collect info for
    # last 150 secs maximum, (sample every 10 secs and keep up to 15 samples)
    if ctx['perf']:
      try:
        ctx['perf'].setup(['*'], [vbox.host], 10, 15)
      except:
        pass
    cmds = []

    if g_cmd is not None:
        cmds = g_cmd.split(';')
    it = cmds.__iter__()

    while True:
        try:
            if g_batchmode:
                cmd = 'runScript %s'%(g_scripfile)
            elif g_cmd is not None:
                cmd = it.next()
            else:
                cmd = raw_input(ctx['prompt'])
            done = runCommand(ctx, cmd)
            if done != 0: break
            if g_batchmode:
                break
        except KeyboardInterrupt:
            print '====== You can type quit or q to leave'
        except StopIteration:
            break
        except EOFError:
            break
        except Exception,e:
            printErr(ctx,e)
            if g_verbose:
                traceback.print_exc()
        ctx['global'].waitForEvents(0)
    try:
        # There is no need to disable metric collection. This is just an example.
        if ct['perf']:
           ctx['perf'].disable(['*'], [vbox.host])
    except:
        pass
    if g_hasreadline:
        readline.write_history_file(hist_file)

def runCommandCb(ctx, cmd, args):
    args.insert(0, cmd)
    return runCommandArgs(ctx, args)

def runGuestCommandCb(ctx, id, guestLambda, args):
    mach =  machById(ctx,id)
    if mach == None:
        return 0
    args.insert(0, guestLambda)
    cmdExistingVm(ctx, mach, 'guestlambda', args)
    return 0

def main(argv):
    style = None
    params = None
    autopath = False
    script_file = None
    parse = OptionParser()
    parse.add_option("-v", "--verbose", dest="verbose", action="store_true", default=False, help = "switch on verbose")
    parse.add_option("-a", "--autopath", dest="autopath", action="store_true", default=False, help = "switch on autopath")
    parse.add_option("-w", "--webservice", dest="style", action="store_const", const="WEBSERVICE", help = "connect to webservice")
    parse.add_option("-b", "--batch", dest="batch_file", help = "script file to execute")
    parse.add_option("-c", dest="command_line", help = "command sequence to execute")
    parse.add_option("-o", dest="opt_line", help = "option line")
    global g_verbose, g_scripfile, g_batchmode, g_hascolors, g_hasreadline, g_cmd
    (options, args) = parse.parse_args()
    g_verbose = options.verbose
    style = options.style
    if options.batch_file is not None:
        g_batchmode = True
        g_hascolors = False
        g_hasreadline = False
        g_scripfile = options.batch_file
    if options.command_line is not None:
        g_hascolors = False
        g_hasreadline = False
        g_cmd = options.command_line
    if options.opt_line is not None:
        params = {}
        strparams = options.opt_line
        l = strparams.split(',')
        for e in l:
            (k,v) = e.split('=')
            params[k] = v
    else:
        params = None

    if options.autopath:
        cwd = os.getcwd()
        vpp = os.environ.get("VBOX_PROGRAM_PATH")
        if vpp is None and (os.path.isfile(os.path.join(cwd, "VirtualBox")) or os.path.isfile(os.path.join(cwd, "VirtualBox.exe"))) :
            vpp = cwd
            print "Autodetected VBOX_PROGRAM_PATH as",vpp
            os.environ["VBOX_PROGRAM_PATH"] = vpp
            sys.path.append(os.path.join(vpp, "sdk", "installer"))
        vsp = os.environ.get("VBOX_SDK_PATH")
        if vsp is None and os.path.isfile(os.path.join(cwd, "sdk", "bindings", "VirtualBox.xidl")) :
            vsp = os.path.join(cwd, "sdk")
        if vsp is None and os.path.isfile(os.path.join(vpp, "sdk", "bindings", "VirtualBox.xidl")) :
            vsp = os.path.join(vpp, "sdk")
        if vsp is not None :
            print "Autodetected VBOX_SDK_PATH as",vsp
            os.environ["VBOX_SDK_PATH"] = vsp

    from vboxapi import VirtualBoxManager
    g_virtualBoxManager = VirtualBoxManager(style, params)
    ctx = {'global':g_virtualBoxManager,
           'mgr':g_virtualBoxManager.mgr,
           'vb':g_virtualBoxManager.vbox,
           'const':g_virtualBoxManager.constants,
           'remote':g_virtualBoxManager.remote,
           'type':g_virtualBoxManager.type,
           'run': lambda cmd,args: runCommandCb(ctx, cmd, args),
           'guestlambda': lambda id,guestLambda,args: runGuestCommandCb(ctx, id, guestLambda, args),
           'machById': lambda id: machById(ctx,id),
           'argsToMach': lambda args: argsToMach(ctx,args),
           'progressBar': lambda p: progressBar(ctx,p),
           'typeInGuest': typeInGuest,
           '_machlist': None,
           'prompt': g_prompt,
           'scriptLine': 0,
           'interrupt': False
           }
    interpret(ctx)
    g_virtualBoxManager.deinit()
    del g_virtualBoxManager

if __name__ == '__main__':
    main(sys.argv)
