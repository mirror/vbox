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
    # TODO: This needs to be changed if it is supposed to work with OSE as well.
    saved_sys_path = sys.path;
    sys.path = [ '/Applications/VirtualBox.app/Contents/MacOS', ] + saved_sys_path
    cglue = __import__('VBoxPython')
    sys.path = saved_sys_path    
else:
    cglue = __import__('VBoxPython')  
sys.modules['xpcom._xpcom'] = cglue
xpcom._xpcom = cglue
