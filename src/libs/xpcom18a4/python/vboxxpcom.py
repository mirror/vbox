"""
Copyright (C) 2008-2012 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
"""

import xpcom
import sys
import platform

# this code overcomes somewhat unlucky feature of Python, where it searches
# for binaries in the same place as platfom independent modules, while
# rest of Python bindings expect _xpcom to be inside xpcom module

candidates = ['VBoxPython' + str(sys.version_info[0]) + '_' + str(sys.version_info[1]), 
              'VBoxPython' + str(sys.version_info[0]), 
              'VBoxPython']
if platform.system() == 'Darwin':
    # On Darwin (aka Mac OS X) we know exactly where things are in a normal 
    # VirtualBox installation. Also, there are two versions of python there
    # (2.3.x and 2.5.x) depending on whether the os is striped or spotty, so
    # we have to choose the right module to load.
    # 
    # XXX: This needs to be adjusted for OSE builds. A more general solution would 
    #      be to to sed the file during install and inject the VBOX_PATH_APP_PRIVATE_ARCH
    #      and VBOX_PATH_SHARED_LIBS when these are set.
    sys.path.append('/Applications/VirtualBox.app/Contents/MacOS')

cglue = None
for m in candidates:
   try:
      cglue =  __import__(m)
      break
   except:
      pass

if platform.system() == 'Darwin':
    sys.path.remove('/Applications/VirtualBox.app/Contents/MacOS')

if cglue == None:
    raise Exception, "Cannot find VBoxPython module"

sys.modules['xpcom._xpcom'] = cglue
xpcom._xpcom = cglue

