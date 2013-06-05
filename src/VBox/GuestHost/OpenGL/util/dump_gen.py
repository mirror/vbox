import sys

import apiutil

import sys, re, string


line_re = re.compile(r'^(\S+)\s+(GL_\S+)\s+(.*)\s*$')
extensions_line_re = re.compile(r'^(\S+)\s+(GL_\S+)\s(\S+)\s+(.*)\s*$')

params = {}
extended_params = {}

input = open( sys.argv[2]+"/../state_tracker/state_isenabled.txt", 'r' )
for line in input.readlines():
    match = line_re.match( line )
    if match:
        type = match.group(1)
        pname = match.group(2)
        fields = string.split( match.group(3) )
        params[pname] = ( type, fields )

input = open( sys.argv[2]+"/../state_tracker/state_extensions_isenabled.txt", 'r' )
for line in input.readlines():
    match = extensions_line_re.match( line )
    if match:
        type = match.group(1)
        pname = match.group(2)
        ifdef = match.group(3)
        fields = string.split( match.group(4) )
        extended_params[pname] = ( type, ifdef, fields )


apiutil.CopyrightC()

print """#include "cr_blitter.h"
#include "cr_spu.h"
#include "chromium.h"
#include "cr_error.h"
#include "cr_net.h"
#include "cr_rand.h"
#include "cr_mem.h"
#include "cr_string.h"
#include <cr_dump.h>
#include "cr_pixeldata.h"

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/mem.h>

#include <stdio.h>

#ifdef VBOX_WITH_CRDUMPER
"""

from get_sizes import *;

print """
static void crRecDumpPrintVal(CR_DUMPER *pDumper, struct nv_struct *pDesc, float *pfData)
{
    char aBuf[4096];
    crDmpFormatArrayf(aBuf, sizeof (aBuf), pfData, pDesc->num_values);
    crDmpStrF(pDumper, "%s = %s;", pDesc->pszName, aBuf);
}


void crRecDumpGlGetState(CR_RECORDER *pRec, CRContext *ctx)
{
    float afData[CR_MAX_GET_VALUES];
    struct nv_struct *pDesc;
    
    for (pDesc = num_values_array; pDesc->num_values != 0 ; pDesc++)
    {
        memset(afData, 0, sizeof(afData));
        pRec->pDispatch->GetFloatv(pDesc->pname, afData);
        crRecDumpPrintVal(pRec->pDumper, pDesc, afData);
    }
}

void crRecDumpGlEnableState(CR_RECORDER *pRec, CRContext *ctx)
{
    GLboolean fEnabled;
"""
keys = params.keys()
keys.sort();

for pname in keys:
    print "\tfEnabled = pRec->pDispatch->IsEnabled(%s);" % pname
    print "\tcrDmpStrF(pRec->pDumper, \"%s = %%d;\", fEnabled);" % pname

keys = extended_params.keys();
keys.sort()

for pname in keys:
    (srctype,ifdef,fields) = extended_params[pname]
    ext = ifdef[3:]  # the extension name with the "GL_" prefix removed
    ext = ifdef
    print '#ifdef CR_%s' % ext
    print "\tfEnabled = pRec->pDispatch->IsEnabled(%s);" % pname
    print "\tcrDmpStrF(pRec->pDumper, \"%s = %%d;\", fEnabled);" % pname
    print '#endif /* CR_%s */' % ext

print """
}
#endif
"""