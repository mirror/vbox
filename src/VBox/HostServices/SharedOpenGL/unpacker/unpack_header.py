# Copyright (c) 2001, Stanford University
# All rights reserved.
#
# See the file LICENSE.txt for information on redistributing this software.

import sys;
import cPickle;
import types;
import string;
import re;

sys.path.append( "../opengl_stub" )

import stub_common;

parsed_file = open( "../glapi_parser/gl_header.parsed", "rb" )
gl_mapping = cPickle.load( parsed_file )

stub_common.CopyrightC()

print """#ifndef CR_UNPACKFUNCTIONS_H
#define CR_UNPACKFUNCTIONS_H
"""

keys = gl_mapping.keys()
keys.sort()

for func_name in keys:
	( return_type, arg_names, arg_types ) = gl_mapping[func_name]
	print 'void crUnpack%s();' %( func_name )
print 'void crUnpackExtend();'
print '\n#endif /* CR_UNPACKFUNCTIONS_H */'
