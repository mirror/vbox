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

import os,sys
from distutils.core import setup

def patchWith(file,install):
    newFile=file+".new"
    try: 
        os.remove(newFile)
    except:
        pass
    oldF = open(file, 'r')
    newF = open(newFile, 'w')
    for line in oldF:
        line=line.replace("%VBOX_INSTALL_PATH%",install)
        newF.write(line)
    newF.close()
    oldF.close()
     try: 
        os.remove(file)
    except:
        pass
    os.rename(newFile, file)

# See http://docs.python.org/distutils/index.html
def main(argv):
    vboxDest=os.environ.get("VBOX_INSTALL_PATH", None)
    if vboxDest is None:
        raise Exception("No VBOX_INSTALL_PATH defined, exiting")
    vboxVersion=os.environ.get("VBOX_VERSION", None)
    if vboxVersion is None:
        # Should we use VBox version for binding module versioning?
        vboxVersion = "1.0"
        #raise Exception("No VBOX_VERSION defined, exiting")
    patchWith(os.path.join(os.path.dirname(sys.argv[0]), 'vboxapi', '__init__.py'), vboxDest)
    setup(name='vboxapi',
      version=vboxVersion,
      description='Python interface to VirtualBox',
      author='Sun Microsystems',
      author_email='vbox-dev@virtualbox.org',
      url='http://www.virtualbox.org',
      packages=['vboxapi']
      )


if __name__ == '__main__':
    main(sys.argv)
