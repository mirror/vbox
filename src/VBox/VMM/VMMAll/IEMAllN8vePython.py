#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=invalid-name

"""
Native recompiler side-kick for IEMAllThrdPython.py.

Analyzes the each threaded function variant to see if we can we're able to
recompile it, then provides modifies MC block code for doing so.
"""

from __future__ import print_function;

__copyright__ = \
"""
Copyright (C) 2023 Oracle and/or its affiliates.

This file is part of VirtualBox base platform packages, as
available from https://www.virtualbox.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, in version 3 of the
License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses>.

SPDX-License-Identifier: GPL-3.0-only
"""
__version__ = "$Revision$"

# Standard python imports:
#import sys;

# Out python imports:
import IEMAllInstPython as iai;


## Supplememnts g_dMcStmtParsers.
g_dMcStmtThreaded = {
    'IEM_MC_DEFER_TO_CIMPL_0_RET_THREADED':                      (None, True,  True,  ),
    'IEM_MC_DEFER_TO_CIMPL_1_RET_THREADED':                      (None, True,  True,  ),
    'IEM_MC_DEFER_TO_CIMPL_2_RET_THREADED':                      (None, True,  True,  ),
    'IEM_MC_DEFER_TO_CIMPL_3_RET_THREADED':                      (None, True,  True,  ),

    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16':               (None, True,  False, ),
    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32':               (None, True,  False, ), # True ),
    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64':               (None, True,  False, ),

    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16_WITH_FLAGS':    (None, True,  False, ),
    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32_WITH_FLAGS':    (None, True,  False,  ),
    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64_WITH_FLAGS':    (None, True,  False, ),

    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_16_ADDR32':                (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_16_PRE386':                (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_16':                       (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_32_ADDR16':                (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_32_FLAT':                  (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_32':                       (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_64_ADDR32':                (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_64_FSGS':                  (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_64':                       (None, False, False, ),

    'IEM_MC_CALL_CIMPL_1_THREADED':                              (None, True,  False, ),
    'IEM_MC_CALL_CIMPL_2_THREADED':                              (None, True,  False, ),
    'IEM_MC_CALL_CIMPL_3_THREADED':                              (None, True,  False, ),
    'IEM_MC_CALL_CIMPL_4_THREADED':                              (None, True,  False, ),
    'IEM_MC_CALL_CIMPL_5_THREADED':                              (None, True,  False, ),

    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC16':                (None, True,  False, ),
    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC32':                (None, True,  False, ),
    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC64':                (None, True,  False, ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC16':               (None, True,  False, ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC32':               (None, True,  False, ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC64':               (None, True,  False, ),
    'IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC32':               (None, True,  False, ),
    'IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC64':               (None, True,  False, ),

    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC16_WITH_FLAGS':     (None, True,  False, ),
    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC32_WITH_FLAGS':     (None, True,  False, ),
    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC64_WITH_FLAGS':     (None, True,  False, ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC16_WITH_FLAGS':    (None, True,  False, ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC32_WITH_FLAGS':    (None, True,  False, ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC64_WITH_FLAGS':    (None, True,  False, ),
    'IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC32_WITH_FLAGS':    (None, True,  False, ),
    'IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC64_WITH_FLAGS':    (None, True,  False, ),

    'IEM_MC_STORE_GREG_U8_THREADED':                             (None, True,  False, ),
    'IEM_MC_STORE_GREG_U8_CONST_THREADED':                       (None, True,  False, ),
    'IEM_MC_FETCH_GREG_U8_THREADED':                             (None, False, False, ),
    'IEM_MC_FETCH_GREG_U8_SX_U16_THREADED':                      (None, False, False, ),
    'IEM_MC_FETCH_GREG_U8_SX_U32_THREADED':                      (None, False, False, ),
    'IEM_MC_FETCH_GREG_U8_SX_U64_THREADED':                      (None, False, False, ),
    'IEM_MC_FETCH_GREG_U8_ZX_U16_THREADED':                      (None, False, False, ),
    'IEM_MC_FETCH_GREG_U8_ZX_U32_THREADED':                      (None, False, False, ),
    'IEM_MC_FETCH_GREG_U8_ZX_U64_THREADED':                      (None, False, False, ),
    'IEM_MC_REF_GREG_U8_THREADED':                               (None, True,  False, ),

    # Flat Mem:
    'IEM_MC_FETCH_MEM16_FLAT_U8':                                (None, True,  False, ),
    'IEM_MC_FETCH_MEM32_FLAT_U8':                                (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_D80':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_I16':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_I32':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_I64':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_R32':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_R64':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_R80':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U128_ALIGN_SSE':                      (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U128_NO_AC':                          (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U128':                                (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U16_DISP':                            (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U16_SX_U32':                          (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U16_SX_U64':                          (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U16':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U16_ZX_U32':                          (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U16_ZX_U64':                          (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U256_ALIGN_AVX':                      (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U256_NO_AC':                          (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U256':                                (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U32_DISP':                            (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U32_SX_U64':                          (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U32':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U32_ZX_U64':                          (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U64':                                 (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U8_SX_U16':                           (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U8_SX_U32':                           (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U8_SX_U64':                           (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U8':                                  (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U8_ZX_U16':                           (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U8_ZX_U32':                           (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U8_ZX_U64':                           (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_XMM_ALIGN_SSE':                       (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_XMM_U32':                             (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_XMM_U64':                             (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP_EX':                                    (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP':                                       (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP_U16_RO':                                (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP_U16_RW':                                (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP_U32_RO':                                (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP_U32_RW':                                (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP_U64_RO':                                (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP_U64_RW':                                (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP_U8_RO':                                 (None, True,  False, ),
    'IEM_MC_MEM_FLAT_MAP_U8_RW':                                 (None, True,  False, ),
    'IEM_MC_STORE_MEM_FLAT_U128_ALIGN_SSE':                      (None, True,  False, ),
    'IEM_MC_STORE_MEM_FLAT_U128':                                (None, True,  False, ),
    'IEM_MC_STORE_MEM_FLAT_U16':                                 (None, True,  False, ),
    'IEM_MC_STORE_MEM_FLAT_U256_ALIGN_AVX':                      (None, True,  False, ),
    'IEM_MC_STORE_MEM_FLAT_U256':                                (None, True,  False, ),
    'IEM_MC_STORE_MEM_FLAT_U32':                                 (None, True,  False, ),
    'IEM_MC_STORE_MEM_FLAT_U64':                                 (None, True,  False, ),
    'IEM_MC_STORE_MEM_FLAT_U8_CONST':                            (None, True,  False, ),
    'IEM_MC_STORE_MEM_FLAT_U8':                                  (None, True,  False, ),

    # Flat Stack:
    'IEM_MC_FLAT64_PUSH_U16':                                    (None, True,  False, ),
    'IEM_MC_FLAT64_PUSH_U64':                                    (None, True,  False, ),
    'IEM_MC_FLAT64_PUSH_U16':                                    (None, True,  False, ),
    'IEM_MC_FLAT64_PUSH_U64':                                    (None, True,  False, ),
    'IEM_MC_FLAT64_POP_U16':                                     (None, True,  False, ),
    'IEM_MC_FLAT64_POP_U64':                                     (None, True,  False, ),
    'IEM_MC_FLAT64_POP_U16':                                     (None, True,  False, ),
    'IEM_MC_FLAT64_POP_U64':                                     (None, True,  False, ),
    'IEM_MC_FLAT64_PUSH_U16':                                    (None, True,  False, ),
    'IEM_MC_FLAT64_PUSH_U64':                                    (None, True,  False, ),
    'IEM_MC_FLAT64_PUSH_U16':                                    (None, True,  False, ),
    'IEM_MC_FLAT64_PUSH_U64':                                    (None, True,  False, ),
    'IEM_MC_FLAT32_PUSH_U16':                                    (None, True,  False, ),
    'IEM_MC_FLAT64_PUSH_U16':                                    (None, True,  False, ),
    'IEM_MC_FLAT32_PUSH_U32':                                    (None, True,  False, ),
    'IEM_MC_FLAT64_PUSH_U64':                                    (None, True,  False, ),
};

class NativeRecompFunctionVariation(object):
    """
    Class that deals with transforming a threaded function variation into a
    native recompiler function.

    This base class doesn't do any transforming and just renders the same
    code as for the threaded function.
    """

    def __init__(self, oVariation, sHostArch):
        self.oVariation = oVariation # type: ThreadedFunctionVariation
        self.sHostArch  = sHostArch;

    def isRecompilable(self):
        """
        Predicate that returns whether the variant can be recompiled natively
        (for the selected host architecture).
        """
        return True;

    def renderCode(self, cchIndent):
        """
        Returns the native recompiler function body for this threaded variant.
        """
        aoStmts = self.oVariation.aoStmtsForThreadedFunction # type: list(McStmt)
        return iai.McStmt.renderCodeForList(aoStmts, cchIndent);

    @staticmethod
    def checkStatements(aoStmts, sHostArch):
        """
        Checks that all the given statements are supported by the native recompiler.
        """
        _ = sHostArch;
        for oStmt in aoStmts:   # type: McStmt
            if not oStmt.isCppStmt():
                aInfo = iai.g_dMcStmtParsers.get(oStmt.sName);
                if not aInfo:
                    aInfo = g_dMcStmtThreaded.get(oStmt.sName);
                    if not aInfo:
                        raise Exception('Unknown statement: %s' % (oStmt.sName, ));
                if aInfo[2] is False:
                    return False;
                if aInfo[2] is not True:
                    if isinstance(aInfo[2], str):
                        if aInfo[2] != sHostArch:
                            return False;
                    elif sHostArch not in aInfo[2]:
                        return False;
            #elif not self.fDecode:

            if isinstance(oStmt, iai.McStmtCond):
                if not NativeRecompFunctionVariation.checkStatements(oStmt.aoIfBranch, sHostArch):
                    return False;
                if not NativeRecompFunctionVariation.checkStatements(oStmt.aoElseBranch, sHostArch):
                    return False;
        return True;



def analyzeVariantForNativeRecomp(oVariation,
                                  sHostArch): # type: (ThreadedFunctionVariation, str) -> NativeRecompFunctionVariation
    """
    This function analyzes the threaded function variant and returns an
    NativeRecompFunctionVariation instance for it, unless it's not
    possible to recompile at present.

    Returns NativeRecompFunctionVariation or None.
    """

    #
    # Analyze the statements.
    #
    aoStmts = oVariation.aoStmtsForThreadedFunction # type: list(McStmt)
    if NativeRecompFunctionVariation.checkStatements(aoStmts, sHostArch):
        return NativeRecompFunctionVariation(oVariation, sHostArch);

    # The simplest case are the IEM_MC_DEFER_TO_CIMPL_*_RET_THREADED ones, just pass them thru:
    #if (    len(aoStmts) == 1
    #    and aoStmts[0].sName.startswith('IEM_MC_DEFER_TO_CIMPL_')
    #    and aoStmts[0].sName.endswith('_RET_THREADED')):
    #    return NativeRecompFunctionVariation(oVariation, sHostArch);


    # g_dMcStmtParsers


    return None;
