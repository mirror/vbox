#
# Copyright (C) 2008 Sun Microsystems, Inc.
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

import xpcom
import sys
import platform

# this code overcomes somewhat unlucky feature of Python, where it searches
# for binaries in the same place as platfom independent modules, while
# rest of Python bindings expect _xpcom to be inside xpcom module
# XXX: maybe implement per Python version module search
#cglue = __import__('VBoxPython')
if platform.system() == 'Darwin':
    # On Darwin (aka Mac OS X) we know exactly where things are in a normal 
    # VirtualBox installation. Also, there are two versions of python there
    # (2.3.x and 2.5.x) depending on whether the os is striped or spotty, so
    # we have to choose the right module to load.
    # 
    # XXX: This needs to be adjusted for OSE builds. A more general solution would 
    #      be to to sed the file during install and inject the VBOX_PATH_APP_PRIVATE_ARCH
    #      and VBOX_PATH_SHARED_LIBS when these are set.
    saved_sys_path = sys.path;
    sys.path = [ '/Applications/VirtualBox.app/Contents/MacOS', ] + saved_sys_path
    mod = 'VBoxPython' + str(sys.version_info[0]) + '_' + str(sys.version_info[1])
    cglue = __import__(mod)
    sys.path = saved_sys_path    
else:
    cglue = __import__('VBoxPython')  
sys.modules['xpcom._xpcom'] = cglue
xpcom._xpcom = cglue

