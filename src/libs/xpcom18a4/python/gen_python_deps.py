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


import os,sys

versions = ["2.3", "2.4", "2.5", "2.6", "2.7", "2.8"]
prefixes = ["/usr", "/usr/local", "/opt"]
known = {}

def checkPair(p,v):
    file =  os.path.join(p, "include", "python"+v, "Python.h")
    # or just stat()?
    try:
        lf = open(file, 'r')
    except IOError,e:
        return None
    lf.close()
    return [os.path.join(p, "include", "python"+v), 
            os.path.join(p, "lib", "libpython"+v+".so")]

def main(argv):
    for v in versions:
        for p in prefixes:
            c = checkPair(p, v)
            if c is not None:
                known[v] = c
                break
    keys = known.keys()
    # we want default to be the lowest versioned Python
    keys.sort()
    d = None
    for k in keys:
        if d is None:
            d = k
        vers = k.replace('.', '')
        print "VBOX_PYTHON%s_INC=%s" %(vers, known[k][0])
        print "VBOX_PYTHON%s_LIB=%s" %(vers, known[k][1])
    if d is not None:
        print "VBOX_PYTHONDEF_INC=%s" %(known[d][0])
        print "VBOX_PYTHONDEF_LIB=%s" %(known[d][1])

if __name__ == '__main__':
    main(sys.argv)
