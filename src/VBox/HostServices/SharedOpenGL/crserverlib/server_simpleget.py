# Copyright (c) 2001, Stanford University
# All rights reserved.
#
# See the file LICENSE.txt for information on redistributing this software.

import sys

import apiutil


apiutil.CopyrightC()

print """#include "cr_spu.h"
#include "chromium.h"
#include "cr_error.h"
#include "cr_mem.h"
#include "cr_net.h"
#include "server_dispatch.h"
#include "server.h"
"""

from get_sizes import *;


funcs = [ 'GetIntegerv', 'GetFloatv', 'GetDoublev', 'GetBooleanv' ]
types = [ 'GLint', 'GLfloat', 'GLdouble', 'GLboolean' ]

for index in range(len(funcs)):
    func_name = funcs[index]
    params = apiutil.Parameters(func_name)
    print 'void SERVER_DISPATCH_APIENTRY crServerDispatch%s( %s )' % ( func_name, apiutil.MakeDeclarationString(params))
    print '{'
    print '\t%s *get_values;' % types[index]
    print '\tint tablesize;'
    print """
    #ifdef CR_ARB_texture_compression
    if (GL_COMPRESSED_TEXTURE_FORMATS_ARB == pname)
    {
        GLint numtexfmts = 0;
        cr_server.head_spu->dispatch_table.GetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB, &numtexfmts);
        tablesize = numtexfmts * sizeof(%s);
    }
    else
    #endif
    {
        tablesize = __numValues( pname ) * sizeof(%s);
    }
    """ % (types[index], types[index])
    print '\t(void) params;'
    print '\tget_values = (%s *) crAlloc( tablesize );' % types[index]
    print '\tif (tablesize>0)'
    print '\tcr_server.head_spu->dispatch_table.%s( pname, get_values );' % func_name
    print """
    if (GL_TEXTURE_BINDING_1D==pname
        || GL_TEXTURE_BINDING_2D==pname
        || GL_TEXTURE_BINDING_3D==pname
        || GL_TEXTURE_BINDING_RECTANGLE_ARB==pname
        || GL_TEXTURE_BINDING_CUBE_MAP_ARB==pname)
    {
        GLuint texid;
        CRASSERT(tablesize/sizeof(%s)==1);
        texid = (GLuint) *get_values;
        *get_values = (%s) crServerTranslateTextureID(texid);
    }
    """ % (types[index], types[index])
    print '\tcrServerReturnValue( get_values, tablesize );'
    print '\tcrFree(get_values);'
    print '}\n'
