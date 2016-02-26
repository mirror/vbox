#!/usr/bin/python

"""
Copyright (C) 2009-2016 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
"""

import os,sys
from distutils.version import StrictVersion

versions = ["2.6", "2.7", "3.1", "3.2", "3.3", "3.4", "3.5"]
prefixes = ["/usr", "/usr/local", "/opt", "/opt/local"]
known = {}

def checkPair(p, v,dllpre,dllsuff, bitness_magic):
    file =  os.path.join(p, "include", "python"+v, "Python.h")
    if not os.path.isfile(file):
        return None

    lib = os.path.join(p, "lib/i386-linux-gnu", dllpre+"python"+v+dllsuff)
    if not os.path.isfile(lib):
        lib = os.path.join(p, "lib", dllpre+"python"+v+dllsuff)

    if bitness_magic == 1:
        lib64 = os.path.join(p, "lib", "64", dllpre+"python"+v+dllsuff)
    elif bitness_magic == 2:
        lib64 = os.path.join(p, "lib/x86_64-linux-gnu", dllpre+"python"+v+dllsuff)
        if not os.path.isfile(lib64):
            lib64 = os.path.join(p, "lib64", dllpre+"python"+v+dllsuff)
            if not os.path.isfile(lib64):
                lib64 = lib
                if not os.path.isfile(lib64):
                    return None

    else:
        lib64 = None
    return [os.path.join(p, "include", "python"+v), lib, lib64]

def print_vars(vers, known, sep, bitness_magic):
    print("VBOX_PYTHON%s_INC=%s%s" %(vers, known[0], sep))
    if bitness_magic > 0:
        print("VBOX_PYTHON%s_LIB=%s%s" %(vers, known[2], sep))
        print("VBOX_PYTHON%s_LIB_X86=%s%s" %(vers, known[1], sep))
    else:
        print("VBOX_PYTHON%s_LIB=%s%s" %(vers, known[1], sep))


def main(argv):
    global prefixes
    global versions

    dllpre = "lib"
    dllsuff = ".so"
    bitness_magic = 0

    if len(argv) > 1:
        target = argv[1]
    else:
        target = sys.platform

    if len(argv) > 2:
        arch = argv[2]
    else:
        arch = "unknown"

    if len(argv) > 3:
        multi = int(argv[3])
    else:
        multi = 1

    if multi == 0:
        prefixes = ["/usr"]
        versions = [str(sys.version_info[0])+'.'+str(sys.version_info[1])]

    if target == 'darwin':
        ## @todo Pick up the locations from VBOX_PATH_MACOSX_SDK_10_*.
        prefixes = ['/Developer/SDKs/MacOSX10.4u.sdk/usr',
                    '/Developer/SDKs/MacOSX10.5.sdk/usr',
                    '/Developer/SDKs/MacOSX10.6.sdk/usr',
                    '/Developer/SDKs/MacOSX10.7.sdk/usr']
        dllsuff = '.dylib'

    if target == 'solaris' and arch == 'amd64':
        bitness_magic = 1

    if target == 'linux' and arch == 'amd64':
        bitness_magic = 2

    for v in versions:
        if StrictVersion(v) < StrictVersion('2.6'):
            continue
        for p in prefixes:
            c = checkPair(p, v, dllpre, dllsuff, bitness_magic)
            if c is not None:
                known[v] = c
                break
    keys = list(known.keys())
    # we want default to be the lowest versioned Python
    keys.sort()
    d = None
    # We need separator other than newline, to sneak through $(shell)
    sep = "|"
    for k in keys:
        if d is None:
            d = k
        vers = k.replace('.', '')
        print_vars(vers, known[k], sep, bitness_magic)
    if d is not None:
        print_vars("DEF", known[d], sep, bitness_magic)

if __name__ == '__main__':
    main(sys.argv)
