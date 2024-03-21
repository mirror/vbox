copyright = '''
/*
 * Copyright 2009 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * VMWARE AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
'''

import argparse
import itertools
import typing as T

GENERATE, UINT8, UINT16, UINT32 = 'generate', 'uint8', 'uint16', 'uint32'
FIRST, LAST = 'first', 'last'
PRDISABLE, PRENABLE = 'prdisable', 'prenable'

INTYPES = (GENERATE, UINT8, UINT16, UINT32)
OUTTYPES = (UINT16, UINT32)
PVS=(FIRST, LAST)
PRS=(PRDISABLE, PRENABLE)
PRIMS=('points',
       'lines',
       'linestrip',
       'lineloop',
       'tris',
       'trifan',
       'tristrip',
       'quads',
       'quadstrip',
       'polygon',
       'linesadj',
       'linestripadj',
       'trisadj',
       'tristripadj')

OUT_TRIS, OUT_QUADS = 'tris', 'quads'

LONGPRIMS=('MESA_PRIM_POINTS',
           'MESA_PRIM_LINES',
           'MESA_PRIM_LINE_STRIP',
           'MESA_PRIM_LINE_LOOP',
           'MESA_PRIM_TRIANGLES',
           'MESA_PRIM_TRIANGLE_FAN',
           'MESA_PRIM_TRIANGLE_STRIP',
           'MESA_PRIM_QUADS',
           'MESA_PRIM_QUAD_STRIP',
           'MESA_PRIM_POLYGON',
           'MESA_PRIM_LINES_ADJACENCY',
           'MESA_PRIM_LINE_STRIP_ADJACENCY',
           'MESA_PRIM_TRIANGLES_ADJACENCY',
           'MESA_PRIM_TRIANGLE_STRIP_ADJACENCY')

longprim = dict(zip(PRIMS, LONGPRIMS))
intype_idx = dict(uint8='IN_UINT8', uint16='IN_UINT16', uint32='IN_UINT32')
outtype_idx = dict(uint16='OUT_UINT16', uint32='OUT_UINT32')
pv_idx = dict(first='PV_FIRST', last='PV_LAST')
pr_idx = dict(prdisable='PR_DISABLE', prenable='PR_ENABLE')

def prolog(f: 'T.TextIO') -> None:
    f.write('/* File automatically generated by u_indices_gen.py */\n')
    f.write(copyright)
    f.write(r'''

/**
 * @file
 * Functions to translate and generate index lists
 */

#include "indices/u_indices_priv.h"
#include "util/u_debug.h"
#include "util/u_memory.h"

#include "c99_compat.h"

static u_translate_func translate[IN_COUNT][OUT_COUNT][PV_COUNT][PV_COUNT][PR_COUNT][PRIM_COUNT];
static u_generate_func  generate[OUT_COUNT][PV_COUNT][PV_COUNT][PRIM_COUNT];

static u_translate_func translate_quads[IN_COUNT][OUT_COUNT][PV_COUNT][PV_COUNT][PR_COUNT][PRIM_COUNT];
static u_generate_func  generate_quads[OUT_COUNT][PV_COUNT][PV_COUNT][PRIM_COUNT];


''')

def vert( intype, outtype, v0 ):
    if intype == GENERATE:
        return '(' + outtype + '_t)(' + v0 + ')'
    else:
        return '(' + outtype + '_t)in[' + v0 + ']'

def shape(f: 'T.TextIO', intype, outtype, ptr, *vertices):
    for i, v in enumerate(vertices):
        f.write(f'      ({ptr})[{i}] = {vert(intype, outtype, v)};\n')

def do_point(f: 'T.TextIO', intype, outtype, ptr, v0 ):
    shape(f, intype, outtype, ptr, v0 )

def do_line(f: 'T.TextIO', intype, outtype, ptr, v0, v1, inpv, outpv ):
    if inpv == outpv:
        shape(f, intype, outtype, ptr, v0, v1 )
    else:
        shape(f, intype, outtype, ptr, v1, v0 )

def do_tri(f: 'T.TextIO', intype, outtype, ptr, v0, v1, v2, inpv, outpv ):
    if inpv == outpv:
        shape(f, intype, outtype, ptr, v0, v1, v2 )
    elif inpv == FIRST:
        shape(f, intype, outtype, ptr, v1, v2, v0 )
    else:
        shape(f, intype, outtype, ptr, v2, v0, v1 )

def do_quad(f: 'T.TextIO', intype, outtype, ptr, v0, v1, v2, v3, inpv, outpv, out_prim ):
    if out_prim == OUT_TRIS:
        if inpv == LAST:
            do_tri(f, intype, outtype, ptr+'+0',  v0, v1, v3, inpv, outpv );
            do_tri(f, intype, outtype, ptr+'+3',  v1, v2, v3, inpv, outpv );
        else:
            do_tri(f, intype, outtype, ptr+'+0',  v0, v1, v2, inpv, outpv );
            do_tri(f, intype, outtype, ptr+'+3',  v0, v2, v3, inpv, outpv );
    else:
        if inpv == outpv:
            shape(f, intype, outtype, ptr, v0, v1, v2, v3)
        elif inpv == FIRST:
            shape(f, intype, outtype, ptr, v1, v2, v3, v0)
        else:
            shape(f, intype, outtype, ptr, v3, v0, v1, v2)

def do_lineadj(f: 'T.TextIO', intype, outtype, ptr, v0, v1, v2, v3, inpv, outpv ):
    if inpv == outpv:
        shape(f, intype, outtype, ptr, v0, v1, v2, v3 )
    else:
        shape(f, intype, outtype, ptr, v3, v2, v1, v0 )

def do_triadj(f: 'T.TextIO', intype, outtype, ptr, v0, v1, v2, v3, v4, v5, inpv, outpv ):
    if inpv == outpv:
        shape(f, intype, outtype, ptr, v0, v1, v2, v3, v4, v5 )
    else:
        shape(f, intype, outtype, ptr, v4, v5, v0, v1, v2, v3 )

def name(intype, outtype, inpv, outpv, pr, prim, out_prim):
    if intype == GENERATE:
        return 'generate_' + prim + '_' + outtype + '_' + inpv + '2' + outpv + '_' + str(out_prim)
    else:
        return 'translate_' + prim + '_' + intype + '2' + outtype + '_' + inpv + '2' + outpv + '_' + pr + '_' + str(out_prim)

def preamble(f: 'T.TextIO', intype, outtype, inpv, outpv, pr, prim, out_prim):
    f.write('static void ' + name( intype, outtype, inpv, outpv, pr, prim, out_prim ) + '(\n')
    if intype != GENERATE:
        f.write('    const void * restrict _in,\n')
    f.write('    unsigned start,\n')
    if intype != GENERATE:
        f.write('    unsigned in_nr,\n')
    f.write('    unsigned out_nr,\n')
    if intype != GENERATE:
        f.write('    unsigned restart_index,\n')
    f.write('    void * restrict _out )\n')
    f.write('{\n')
    if intype != GENERATE:
        f.write('  const ' + intype + '_t* restrict in = (const ' + intype + '_t* restrict)_in;\n')
    f.write('  ' + outtype + '_t * restrict out = (' + outtype + '_t* restrict)_out;\n')
    f.write('  unsigned i, j;\n')
    f.write('  (void)j;\n')

def postamble(f: 'T.TextIO'):
    f.write('}\n')

def prim_restart(f: 'T.TextIO', in_verts, out_verts, out_prims, close_func = None):
    f.write('restart:\n')
    f.write('      if (i + ' + str(in_verts) + ' > in_nr) {\n')
    for i, j in itertools.product(range(out_prims), range(out_verts)):
        f.write('         (out+j+' + str(out_verts * i) + ')[' + str(j) + '] = restart_index;\n')
    f.write('         continue;\n')
    f.write('      }\n')
    for i in range(in_verts):
        f.write('      if (in[i + ' + str(i) + '] == restart_index) {\n')
        f.write('         i += ' + str(i + 1) + ';\n')

        if close_func is not None:
            close_func(i)

        f.write('         goto restart;\n')
        f.write('      }\n')

def points(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='points')
    f.write('  for (i = start, j = 0; j < out_nr; j++, i++) {\n')
    do_point(f, intype, outtype, 'out+j',  'i' );
    f.write('   }\n')
    postamble(f)

def lines(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='lines')
    f.write('  for (i = start, j = 0; j < out_nr; j+=2, i+=2) {\n')
    do_line(f,  intype, outtype, 'out+j',  'i', 'i+1', inpv, outpv );
    f.write('   }\n')
    postamble(f)

def linestrip(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='linestrip')
    f.write('  for (i = start, j = 0; j < out_nr; j+=2, i++) {\n')
    do_line(f, intype, outtype, 'out+j',  'i', 'i+1', inpv, outpv );
    f.write('   }\n')
    postamble(f)

def lineloop(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='lineloop')
    f.write('  unsigned end = start;\n')
    f.write('  for (i = start, j = 0; j < out_nr - 2; j+=2, i++) {\n')
    if pr == PRENABLE:
        def close_func(index):
            do_line(f, intype, outtype, 'out+j',  'end', 'start', inpv, outpv )
            f.write('         start = i;\n')
            f.write('         end = start;\n')
            f.write('         j += 2;\n')

        prim_restart(f, 2, 2, 1, close_func)

    do_line(f, intype, outtype, 'out+j',  'i', 'i+1', inpv, outpv );
    f.write('      end = i+1;\n')
    f.write('   }\n')
    do_line(f, intype, outtype, 'out+j',  'end', 'start', inpv, outpv );
    postamble(f)

def tris(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='tris')
    f.write('  for (i = start, j = 0; j < out_nr; j+=3, i+=3) {\n')
    do_tri(f, intype, outtype, 'out+j',  'i', 'i+1', 'i+2', inpv, outpv );
    f.write('   }\n')
    postamble(f)


def tristrip(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='tristrip')
    f.write('  for (i = start, j = 0; j < out_nr; j+=3, i++) {\n')
    if inpv == FIRST:
        do_tri(f, intype, outtype, 'out+j',  'i', 'i+1+(i&1)', 'i+2-(i&1)', inpv, outpv );
    else:
        do_tri(f, intype, outtype, 'out+j',  'i+(i&1)', 'i+1-(i&1)', 'i+2', inpv, outpv );
    f.write('   }\n')
    postamble(f)


def trifan(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='trifan')
    f.write('  for (i = start, j = 0; j < out_nr; j+=3, i++) {\n')

    if pr == PRENABLE:
        def close_func(index):
            f.write('         start = i;\n')
        prim_restart(f, 3, 3, 1, close_func)

    if inpv == FIRST:
        do_tri(f, intype, outtype, 'out+j',  'i+1', 'i+2', 'start', inpv, outpv );
    else:
        do_tri(f, intype, outtype, 'out+j',  'start', 'i+1', 'i+2', inpv, outpv );

    f.write('   }')
    postamble(f)



def polygon(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='polygon')
    f.write('  for (i = start, j = 0; j < out_nr; j+=3, i++) {\n')
    if pr == PRENABLE:
        def close_func(index):
            f.write('         start = i;\n')
        prim_restart(f, 3, 3, 1, close_func)

    if inpv == FIRST:
        do_tri(f, intype, outtype, 'out+j',  'start', 'i+1', 'i+2', inpv, outpv );
    else:
        do_tri(f, intype, outtype, 'out+j',  'i+1', 'i+2', 'start', inpv, outpv );
    f.write('   }')
    postamble(f)


def quads(f: 'T.TextIO', intype, outtype, inpv, outpv, pr, out_prim):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=out_prim, prim='quads')
    if out_prim == OUT_TRIS:
        f.write('  for (i = start, j = 0; j < out_nr; j+=6, i+=4) {\n')
    else:
        f.write('  for (i = start, j = 0; j < out_nr; j+=4, i+=4) {\n')
    if pr == PRENABLE and out_prim == OUT_TRIS:
        prim_restart(f, 4, 3, 2)
    elif pr == PRENABLE:
        prim_restart(f, 4, 4, 1)

    do_quad(f, intype, outtype, 'out+j', 'i+0', 'i+1', 'i+2', 'i+3', inpv, outpv, out_prim );
    f.write('   }\n')
    postamble(f)


def quadstrip(f: 'T.TextIO', intype, outtype, inpv, outpv, pr, out_prim):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=out_prim, prim='quadstrip')
    if out_prim == OUT_TRIS:
        f.write('  for (i = start, j = 0; j < out_nr; j+=6, i+=2) {\n')
    else:
        f.write('  for (i = start, j = 0; j < out_nr; j+=4, i+=2) {\n')
    if pr == PRENABLE and out_prim == OUT_TRIS:
        prim_restart(f, 4, 3, 2)
    elif pr == PRENABLE:
        prim_restart(f, 4, 4, 1)

    if inpv == LAST:
        do_quad(f, intype, outtype, 'out+j', 'i+2', 'i+0', 'i+1', 'i+3', inpv, outpv, out_prim );
    else:
        do_quad(f, intype, outtype, 'out+j', 'i+0', 'i+1', 'i+3', 'i+2', inpv, outpv, out_prim );
    f.write('   }\n')
    postamble(f)


def linesadj(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='linesadj')
    f.write('  for (i = start, j = 0; j < out_nr; j+=4, i+=4) {\n')
    do_lineadj(f, intype, outtype, 'out+j',  'i+0', 'i+1', 'i+2', 'i+3', inpv, outpv )
    f.write('  }\n')
    postamble(f)


def linestripadj(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='linestripadj')
    f.write('  for (i = start, j = 0; j < out_nr; j+=4, i++) {\n')
    do_lineadj(f, intype, outtype, 'out+j',  'i+0', 'i+1', 'i+2', 'i+3', inpv, outpv )
    f.write('  }\n')
    postamble(f)


def trisadj(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='trisadj')
    f.write('  for (i = start, j = 0; j < out_nr; j+=6, i+=6) {\n')
    do_triadj(f, intype, outtype, 'out+j',  'i+0', 'i+1', 'i+2', 'i+3',
              'i+4', 'i+5', inpv, outpv )
    f.write('  }\n')
    postamble(f)


def tristripadj(f: 'T.TextIO', intype, outtype, inpv, outpv, pr):
    preamble(f, intype, outtype, inpv, outpv, pr, out_prim=OUT_TRIS, prim='tristripadj')
    f.write('  for (i = start, j = 0; j < out_nr; i+=2, j+=6) {\n')
    f.write('    if (i % 4 == 0) {\n')
    f.write('      /* even triangle */\n')
    do_triadj(f, intype, outtype, 'out+j',
              'i+0', 'i+1', 'i+2', 'i+3', 'i+4', 'i+5', inpv, outpv )
    f.write('    } else {\n')
    f.write('      /* odd triangle */\n')
    do_triadj(f, intype, outtype, 'out+j',
              'i+2', 'i-2', 'i+0', 'i+3', 'i+4', 'i+6', inpv, outpv )
    f.write('    }\n')
    f.write('  }\n')
    postamble(f)


def emit_funcs(f: 'T.TextIO') -> None:
    for intype, outtype, inpv, outpv, pr in itertools.product(
            INTYPES, OUTTYPES, [FIRST, LAST], [FIRST, LAST], [PRDISABLE, PRENABLE]):
        if pr == PRENABLE and intype == GENERATE:
            continue
        points(f, intype, outtype, inpv, outpv, pr)
        lines(f, intype, outtype, inpv, outpv, pr)
        linestrip(f, intype, outtype, inpv, outpv, pr)
        lineloop(f, intype, outtype, inpv, outpv, pr)
        tris(f, intype, outtype, inpv, outpv, pr)
        tristrip(f, intype, outtype, inpv, outpv, pr)
        trifan(f, intype, outtype, inpv, outpv, pr)
        quads(f, intype, outtype, inpv, outpv, pr, OUT_TRIS)
        quadstrip(f, intype, outtype, inpv, outpv, pr, OUT_TRIS)
        polygon(f, intype, outtype, inpv, outpv, pr)
        linesadj(f, intype, outtype, inpv, outpv, pr)
        linestripadj(f, intype, outtype, inpv, outpv, pr)
        trisadj(f, intype, outtype, inpv, outpv, pr)
        tristripadj(f, intype, outtype, inpv, outpv, pr)

    for intype, outtype, inpv, outpv, pr in itertools.product(
            INTYPES, OUTTYPES, [FIRST, LAST], [FIRST, LAST], [PRDISABLE, PRENABLE]):
        if pr == PRENABLE and intype == GENERATE:
            continue
        quads(f, intype, outtype, inpv, outpv, pr, OUT_QUADS)
        quadstrip(f, intype, outtype, inpv, outpv, pr, OUT_QUADS)

def init(f: 'T.TextIO', intype, outtype, inpv, outpv, pr, prim, out_prim=OUT_TRIS):
    generate_name = 'generate'
    translate_name = 'translate'
    if out_prim == OUT_QUADS:
        generate_name = 'generate_quads'
        translate_name = 'translate_quads'

    if intype == GENERATE:
        f.write(f'{generate_name}[' +
                outtype_idx[outtype] +
                '][' + pv_idx[inpv] +
                '][' + pv_idx[outpv] +
                '][' + longprim[prim] +
                '] = ' + name( intype, outtype, inpv, outpv, pr, prim, out_prim ) + ';\n')
    else:
        f.write(f'{translate_name}[' +
                intype_idx[intype] +
                '][' + outtype_idx[outtype] +
                '][' + pv_idx[inpv] +
                '][' + pv_idx[outpv] +
                '][' + pr_idx[pr] +
                '][' + longprim[prim] +
                '] = ' + name( intype, outtype, inpv, outpv, pr, prim, out_prim ) + ';\n')


def emit_all_inits(f: 'T.TextIO'):
    for intype, outtype, inpv, outpv, pr, prim in itertools.product(
            INTYPES, OUTTYPES, PVS, PVS, PRS, PRIMS):
        init(f,intype, outtype, inpv, outpv, pr, prim)

    for intype, outtype, inpv, outpv, pr, prim in itertools.product(
            INTYPES, OUTTYPES, PVS, PVS, PRS, ['quads', 'quadstrip']):
        init(f,intype, outtype, inpv, outpv, pr, prim, OUT_QUADS)

def emit_init(f: 'T.TextIO'):
    f.write('void u_index_init( void )\n')
    f.write('{\n')
    f.write('  static int firsttime = 1;\n')
    f.write('  if (!firsttime) return;\n')
    f.write('  firsttime = 0;\n')
    emit_all_inits(f)
    f.write('}\n')




def epilog(f: 'T.TextIO') -> None:
    f.write('#include "indices/u_indices.c"\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('output')
    args = parser.parse_args()

    with open(args.output, 'w') as f:
        prolog(f)
        emit_funcs(f)
        emit_init(f)
        epilog(f)


if __name__ == '__main__':
    main()
