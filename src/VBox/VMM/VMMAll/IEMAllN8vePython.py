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
import copy;

# Out python imports:
import IEMAllInstPython as iai;


## Supplememnts g_dMcStmtParsers.
g_dMcStmtThreaded = {
    'IEM_MC_DEFER_TO_CIMPL_0_RET_THREADED':                      (None, True,  True,  ),
    'IEM_MC_DEFER_TO_CIMPL_1_RET_THREADED':                      (None, True,  True,  ),
    'IEM_MC_DEFER_TO_CIMPL_2_RET_THREADED':                      (None, True,  True,  ),
    'IEM_MC_DEFER_TO_CIMPL_3_RET_THREADED':                      (None, True,  True,  ),

    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16':               (None, True,  True,  ),
    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32':               (None, True,  True,  ),
    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64':               (None, True,  True,  ),

    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16_WITH_FLAGS':    (None, True,  True,  ),
    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32_WITH_FLAGS':    (None, True,  True,  ),
    'IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64_WITH_FLAGS':    (None, True,  True,  ),

    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_16_ADDR32':                (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_16_PRE386':                (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_16':                       (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_32_ADDR16':                (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_32_FLAT':                  (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_32':                       (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_64_ADDR32':                (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_64_FSGS':                  (None, False, False, ),
    'IEM_MC_CALC_RM_EFF_ADDR_THREADED_64':                       (None, False, False, ),

    'IEM_MC_CALL_CIMPL_1_THREADED':                              (None, True,  True,  ),
    'IEM_MC_CALL_CIMPL_2_THREADED':                              (None, True,  True,  ),
    'IEM_MC_CALL_CIMPL_3_THREADED':                              (None, True,  False, ),
    'IEM_MC_CALL_CIMPL_4_THREADED':                              (None, True,  False, ),
    'IEM_MC_CALL_CIMPL_5_THREADED':                              (None, True,  False, ),

    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC16':                (None, True,  True,  ),
    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC32':                (None, True,  True,  ),
    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC64':                (None, True,  True,  ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC16':               (None, True,  True,  ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC32':               (None, True,  True,  ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC64':               (None, True,  True,  ),
    'IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC32':               (None, True,  True,  ),
    'IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC64':               (None, True,  True,  ),

    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC16_WITH_FLAGS':     (None, True,  True,  ),
    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC32_WITH_FLAGS':     (None, True,  True,  ),
    'IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC64_WITH_FLAGS':     (None, True,  True,  ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC16_WITH_FLAGS':    (None, True,  True,  ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC32_WITH_FLAGS':    (None, True,  True,  ),
    'IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC64_WITH_FLAGS':    (None, True,  True,  ),
    'IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC32_WITH_FLAGS':    (None, True,  True,  ),
    'IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC64_WITH_FLAGS':    (None, True,  True,  ),

    'IEM_MC_STORE_GREG_U8_THREADED':                             (None, True,  False, ),
    'IEM_MC_STORE_GREG_U8_CONST_THREADED':                       (None, True,  True,  ),
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
    'IEM_MC_FETCH_MEM_FLAT_U128_AND_XREG_U128':                  (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_XMM_ALIGN_SSE_AND_XREG_XMM':          (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_XMM_U32_AND_XREG_XMM':                (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_XMM_U64_AND_XREG_XMM':                (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U128_AND_XREG_U128_AND_RAX_RDX_U64':  (None, True,  False, ),
    'IEM_MC_FETCH_MEM_FLAT_U128_AND_XREG_U128_AND_EAX_EDX_U32_SX_U64': (None, True,  False, ),
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
    'IEM_MC_FLAT64_POP_U16':                                     (None, True,  False, ),
    'IEM_MC_FLAT64_POP_U64':                                     (None, True,  False, ),
    'IEM_MC_FLAT32_PUSH_U16':                                    (None, True,  False, ),
    'IEM_MC_FLAT32_PUSH_U32':                                    (None, True,  False, ),
    'IEM_MC_FLAT32_POP_U16':                                     (None, True,  False, ),
    'IEM_MC_FLAT32_POP_U32':                                     (None, True,  False, ),
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

    def raiseProblem(self, sMessage):
        """ Raises a problem. """
        raise Exception('%s:%s: error: %s'
                        % (self.oVariation.oParent.oMcBlock.sSrcFile, self.oVariation.oParent.oMcBlock.iBeginLine, sMessage,));

    def __analyzeVariableLiveness(self, aoStmts, dVars, iDepth = 0):
        """
        Performs liveness analysis of the given statement list, inserting new
        statements to signal to the native recompiler that a variable is no
        longer used and can be freed.

        Returns list of freed variables.
        """

        class VarInfo(object):
            """ Variable info """
            def __init__(self, oStmt):
                self.oStmt         = oStmt;
                self.fIsArg        = isinstance(oStmt, iai.McStmtArg);
                self.oReferences   = None       # type: VarInfo
                self.oReferencedBy = None       # type: VarInfo

            def isArg(self):
                return self.fIsArg;

            def makeReference(self, oLocal, oParent):
                if not self.isArg():
                    oParent.raiseProblem('Attempt to make a reference out of an local variable: %s = &%s'
                                         % (self.oStmt.sVarName, oLocal.oStmt.sVarName,));
                if self.oReferences:
                    oParent.raiseProblem('Can only make a variable a reference once: %s = &%s; now = &%s'
                                         % (self.oStmt.sVarName, self.oReferences.oStmt.sVarName, oLocal.oStmt.sVarName,));
                if oLocal.isArg():
                    oParent.raiseProblem('Attempt to make a reference to an argument: %s = &%s'
                                         % (self.oStmt.sVarName, oLocal.oStmt.sVarName,));
                if oLocal.oReferencedBy:
                    oParent.raiseProblem('%s is already referenced by %s, so cannot make %s reference it as well'
                                         % (oLocal.oStmt.sVarName, oLocal.oReferencedBy.oStmt.sVarName, self.oStmt.sVarName,));
                oInfo.oReferences = oLocal;
                oInfo.oReferences.oReferencedBy = self;
                return True;

        #
        # Gather variable declarations and add them to dVars.
        # Also keep a local list of them for scoping when iDepth > 0.
        #
        asVarsInScope = [];
        for oStmt in aoStmts:
            if isinstance(oStmt, iai.McStmtVar):
                if oStmt.sVarName in dVars:
                    raise Exception('Duplicate variable: %s' % (oStmt.sVarName, ));

                oInfo = VarInfo(oStmt);
                if oInfo.isArg() and oStmt.sRefType == 'local':
                    oInfo.makeReference(dVars[oStmt.sRef], self);

                dVars[oStmt.sVarName] = oInfo;
                asVarsInScope.append(oStmt.sVarName);

            elif oStmt.sName == 'IEM_MC_REF_LOCAL':
                dVars[oStmt.asParam[0]].makeReference(dVars[oStmt.asParam[1]], self);

        #
        # Now work the statements backwards and look for the last reference to
        # each of the variables in dVars.  We remove the variables from the
        # collections as we go along.
        #
        def freeVariable(aoStmts, iStmt, oVarInfo, dFreedVars, dVars, fIncludeReferences = True):
            sVarName = oVarInfo.oStmt.sVarName;
            if not oVarInfo.isArg():
                aoStmts.insert(iStmt + 1, iai.McStmt('IEM_MC_FREE_LOCAL', [sVarName,]));
                assert not oVarInfo.oReferences;
            else:
                aoStmts.insert(iStmt + 1, iai.McStmt('IEM_MC_FREE_ARG', [sVarName,]));
                if fIncludeReferences and oVarInfo.sReference:
                    sRefVarName = oVarInfo.oReferences.oStmt.sVarName;
                    if sRefVarName in dVars:
                        dFreedVars[sRefVarName] = dVars[sRefVarName];
                        del dVars[sRefVarName];
                        aoStmts.insert(iStmt + 1, iai.McStmt('IEM_MC_FREE_LOCAL', [sRefVarName,]));
            dFreedVars[sVarName] = oVarInfo;
            if dVars is not None:
                del dVars[sVarName];

        dFreedVars = {};
        for iStmt in range(len(aoStmts) - 1, -1, -1):
            oStmt = aoStmts[iStmt];
            if isinstance(oStmt, iai.McStmtCond):
                #
                # Conditionals requires a bit more work...
                #

                # Start by replacing the conditional statement by a shallow copy.
                oStmt = copy.copy(oStmt);
                oStmt.aoIfBranch   = list(oStmt.aoIfBranch);
                oStmt.aoElseBranch = list(oStmt.aoElseBranch);
                aoStmts[iStmt] = oStmt;

                # Check the two branches for final references. Both branches must
                # start processing with the same dVars set, fortunately as shallow
                # copy suffices.
                dFreedInIfBranch   = self.__analyzeVariableLiveness(oStmt.aoIfBranch, dict(dVars), iDepth + 1);
                dFreedInElseBranch = self.__analyzeVariableLiveness(oStmt.aoElseBranch, dVars, iDepth + 1);

                # Add free statements to the start of the IF-branch for variables use
                # for the last time in the else branch.
                for sVarName, oVarInfo in dFreedInElseBranch.items():
                    if sVarName not in dFreedInIfBranch:
                        freeVariable(oStmt.aoIfBranch, -1, oVarInfo, dFreedVars, None, False);
                    else:
                        dFreedVars[sVarName] = oVarInfo;

                # And vice versa.
                for sVarName, oVarInfo in dFreedInIfBranch.items():
                    if sVarName not in dFreedInElseBranch:
                        freeVariable(oStmt.aoElseBranch, -1, oVarInfo, dFreedVars, dVars, False);

                #
                # Now check if any remaining variables are used for the last time
                # in the conditional statement ifself, in which case we need to insert
                # free statements to both branches.
                #
                if not oStmt.isCppStmt():
                    aoFreeStmts = [];
                    for sParam in oStmt.asParams:
                        if sParam in dVars:
                            freeVariable(aoFreeStmts, -1, dVars[sParam], dFreedVars, dVars);
                    for oFreeStmt in aoFreeStmts:
                        oStmt.aoIfBranch.insert(0, oFreeStmt);
                        oStmt.aoElseBranch.insert(0, oFreeStmt);

            elif not oStmt.isCppStmt():
                if isinstance(oStmt, iai.McStmtCall):
                    #
                    # Call statements will make use of all argument variables and
                    # will implicitly free them.  So, iterate the variable and
                    # move them from dVars and onto dFreedVars.
                    #
                    # We explictly free any referenced variable that is still in
                    # dVar at this point (since only arguments can hold variable
                    # references).
                    #
                    for sParam in oStmt.asParams[oStmt.idxParams:]:
                        oVarInfo = dVars.get(sParam);
                        if oVarInfo:
                            if not oVarInfo.isArg():
                                self.raiseProblem('Argument %s in %s is not an argument!' % (sParam, oStmt.sName,));
                            if oVarInfo.oReferences:
                                sRefVarName = oVarInfo.oReferences.oStmt.sVarName;
                                if sRefVarName in dVars:
                                    dFreedVars[sRefVarName] = dVars[sRefVarName];
                                    del dVars[sRefVarName];
                                    aoStmts.insert(iStmt + 1, iai.McStmt('IEM_MC_FREE_LOCAL', [sRefVarName,]));
                            dFreedVars[sParam] = oVarInfo;
                            del dVars[sParam];
                        elif sParam in dFreedVars:
                            self.raiseProblem('Argument %s in %s was used after the call!' % (sParam, oStmt.sName,));
                        else:
                            self.raiseProblem('Argument %s in %s is not known to us' % (sParam, oStmt.sName,));

                    # Check for stray argument variables.
                    for oVarInfo in dVars.values():
                        if oVarInfo.isArg():
                            self.raiseProblem('Unused argument variable: %s' % (oVarInfo.oStmt.sVarName,));
                else:
                    #
                    # Scan all the parameters of generic statements.
                    #
                    for sParam in oStmt.asParams:
                        if sParam in dVars:
                            freeVariable(aoStmts, iStmt, dVars[sParam], dFreedVars, dVars);

        #
        # Free anything left from asVarsInScope that's now going out of scope.
        #
        if iDepth > 0:
            for sVarName in asVarsInScope:
                if sVarName in dVars:
                    freeVariable(aoStmts, len(aoStmts) - 1, dVars[sVarName], dFreedVars, dVars);
        return dFreedVars;

    def __morphStatements(self, aoStmts):
        """
        Morphs the given statement list into something more suitable for
        native recompilation.

        The following is currently done here:
            - Amend IEM_MC_BEGIN with all the IEM_CIMPL_F_XXX and IEM_MC_F_XXX
              flags found and derived, including IEM_MC_F_WITHOUT_FLAGS which
              we determine here.
            - Insert IEM_MC_FREE_LOCAL when after the last statment a local
              variable is last used.

        Returns a new list of statements.
        """

        #
        # We can skip IEM_MC_DEFER_TO_CIMPL_x_RET stuff.
        #
        if self.oVariation.oParent.oMcBlock.fDeferToCImpl:
            return aoStmts;

        #
        # We make a shallow copy of the list, and only make deep copies of the
        # statements we modify.
        #
        aoStmts = list(aoStmts) # type: list(iai.McStmt)

        #
        # First, amend the IEM_MC_BEGIN statment, adding all the flags found
        # to it so the native recompiler can correctly process ARG and CALL
        # statements (among other things).
        #
        # Also add IEM_MC_F_WITHOUT_FLAGS if this isn't a variation with eflags
        # checking and clearing while there are such variations for this
        # function (this sounds a bit backwards, but has to be done this way
        # for the use we make of the flags in CIMPL calls).
        #
        for iStmt, oStmt in enumerate(aoStmts):
            if oStmt.sName == 'IEM_MC_BEGIN':
                fWithoutFlags = (    self.oVariation.isWithFlagsCheckingAndClearingVariation()
                                 and self.oVariation.oParent.hasWithFlagsCheckingAndClearingVariation());
                if fWithoutFlags or self.oVariation.oParent.dsCImplFlags:
                    oNewStmt = copy.deepcopy(oStmt);
                    if fWithoutFlags:
                        oNewStmt.asParams[2] = ' | '.join(sorted(  list(self.oVariation.oParent.oMcBlock.dsMcFlags.keys())
                                                                 + ['IEM_MC_F_WITHOUT_FLAGS',] ));
                    if self.oVariation.oParent.dsCImplFlags:
                        oNewStmt.asParams[3] = ' | '.join(sorted(self.oVariation.oParent.dsCImplFlags.keys()));
                    aoStmts[iStmt] = oNewStmt;
                break;

        #
        # Do a simple liveness analysis of the variable and insert
        # IEM_MC_FREE_LOCAL statements after the last statements using each
        # variable.  We do this recursively to best handle conditionals and
        # scoping related to those.
        #
        self.__analyzeVariableLiveness(aoStmts, {});

        return aoStmts;


    def renderCode(self, cchIndent):
        """
        Returns the native recompiler function body for this threaded variant.
        """
        return iai.McStmt.renderCodeForList(self.__morphStatements(self.oVariation.aoStmtsForThreadedFunction), cchIndent);

    @staticmethod
    def checkStatements(aoStmts, sHostArch):
        """
        Checks that all the given statements are supported by the native recompiler.
        Returns dictionary with the unsupported statments.
        """
        dRet = {};
        _ = sHostArch;
        for oStmt in aoStmts:   # type: McStmt
            if not oStmt.isCppStmt():
                aInfo = iai.g_dMcStmtParsers.get(oStmt.sName);
                if not aInfo:
                    aInfo = g_dMcStmtThreaded.get(oStmt.sName);
                    if not aInfo:
                        raise Exception('Unknown statement: %s' % (oStmt.sName, ));
                if aInfo[2] is False:
                    dRet[oStmt.sName] = 1;
                elif aInfo[2] is not True:
                    if isinstance(aInfo[2], str):
                        if aInfo[2] != sHostArch:
                            dRet[oStmt.sName] = 1;
                    elif sHostArch not in aInfo[2]:
                        dRet[oStmt.sName] = 1;
            #elif not self.fDecode:

            if isinstance(oStmt, iai.McStmtCond):
                dRet.update(NativeRecompFunctionVariation.checkStatements(oStmt.aoIfBranch, sHostArch));
                dRet.update(NativeRecompFunctionVariation.checkStatements(oStmt.aoElseBranch, sHostArch));

        return dRet;


## Statistics: Number of MC blocks (value) depending on each unsupported statement (key).
g_dUnsupportedMcStmtStats = {}

## Statistics: List of variations (value) that is only missing this one statement (key).
g_dUnsupportedMcStmtLastOneStats = {}

## Statistics: List of variations (value) with vars/args that is only missing this one statement (key).
g_dUnsupportedMcStmtLastOneVarStats = {}


def analyzeVariantForNativeRecomp(oVariation,
                                  sHostArch): # type: (ThreadedFunctionVariation, str) -> NativeRecompFunctionVariation
    """
    This function analyzes the threaded function variant and returns an
    NativeRecompFunctionVariation instance for it, unless it's not
    possible to recompile at present.

    Returns NativeRecompFunctionVariation or the number of unsupported MCs.
    """

    #
    # Analyze the statements.
    #
    aoStmts = oVariation.aoStmtsForThreadedFunction # type: list(McStmt)
    dUnsupportedStmts = NativeRecompFunctionVariation.checkStatements(aoStmts, sHostArch);
    if not dUnsupportedStmts:
        return NativeRecompFunctionVariation(oVariation, sHostArch);

    #
    # Update the statistics.
    #
    for sStmt in dUnsupportedStmts:
        g_dUnsupportedMcStmtStats[sStmt] = 1 + g_dUnsupportedMcStmtStats.get(sStmt, 0);

    if len(dUnsupportedStmts) == 1:
        for sStmt in dUnsupportedStmts:
            if sStmt in g_dUnsupportedMcStmtLastOneStats:
                g_dUnsupportedMcStmtLastOneStats[sStmt].append(oVariation);
            else:
                g_dUnsupportedMcStmtLastOneStats[sStmt] = [oVariation,];

    if (    len(dUnsupportedStmts) == 1 #in (1,2)
        and iai.McStmt.findStmtByNames(aoStmts,
                                       { 'IEM_MC_LOCAL': 1, 'IEM_MC_LOCAL_CONST': 1, 'IEM_MC_ARG': 1, 'IEM_MC_ARG_CONST': 1,
                                         'IEM_MC_ARG_LOCAL_REF': 1, 'IEM_MC_ARG_LOCAL_EFLAGS': 1, })):
        for sStmt in dUnsupportedStmts:
            if sStmt in g_dUnsupportedMcStmtLastOneVarStats:
                g_dUnsupportedMcStmtLastOneVarStats[sStmt].append(oVariation);
            else:
                g_dUnsupportedMcStmtLastOneVarStats[sStmt] = [oVariation,];

    return None;
