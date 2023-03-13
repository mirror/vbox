#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=invalid-name

"""
Annotates and generates threaded functions from IEMAllInstructions*.cpp.h.
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

# Standard python imports.
import datetime;
import os;
import sys;
import argparse;

import IEMAllInstructionsPython as iai;


# Python 3 hacks:
if sys.version_info[0] >= 3:
    long = int;     # pylint: disable=redefined-builtin,invalid-name

## Number of generic parameters for the thread functions.
g_kcThreadedParams = 3;

g_kdTypeInfo = {
    # type name:    (cBits, fSigned,)
    'int8_t':       (    8,    True, ),
    'int16_t':      (   16,    True, ),
    'int32_t':      (   32,    True, ),
    'int64_t':      (   64,    True, ),
    'uint8_t':      (    8,   False, ),
    'uint16_t':     (   16,   False, ),
    'uint32_t':     (   32,   False, ),
    'uint64_t':     (   64,   False, ),
    'uintptr_t':    (   64,   False, ), # ASSUMES 64-bit host pointer size.
    'bool':         (    1,   False, ),
    'IEMMODE':      (    2,   False, ),
};

g_kdIemFieldToType = {
    # Illegal ones:
    'offInstrNextByte':     ( None, ),
    'cbInstrBuf':           ( None, ),
    'pbInstrBuf':           ( None, ),
    'uInstrBufPc':          ( None, ),
    'cbInstrBufTotal':      ( None, ),
    'offCurInstrStart':     ( None, ),
    'cbOpcode':             ( None, ),
    'offOpcode':            ( None, ),
    'offModRm':             ( None, ),
    # Okay ones.
    'fPrefixes':            ( 'uint32_t', ),
    'uRexReg':              ( 'uint8_t', ),
    'uRexB':                ( 'uint8_t', ),
    'uRexIndex':            ( 'uint8_t', ),
    'iEffSeg':              ( 'uint8_t', ),
    'enmEffOpSize':         ( 'IEMMODE', ),
    'enmDefAddrMode':       ( 'IEMMODE', ),
    'enmEffAddrMode':       ( 'IEMMODE', ),
    'enmDefOpSize':         ( 'IEMMODE', ),
    'idxPrefix':            ( 'uint8_t', ),
    'uVex3rdReg':           ( 'uint8_t', ),
    'uVexLength':           ( 'uint8_t', ),
    'fEvexStuff':           ( 'uint8_t', ),
    'uFpuOpcode':           ( 'uint16_t', ),
};

class ThreadedParamRef(object):
    """
    A parameter reference for a threaded function.
    """

    def __init__(self, sOrgRef, sType, oStmt, iParam, offParam = 0):
        self.sNewName = 'x';                        ##< The variable name in the threaded function.
        self.sOrgRef  = sOrgRef;                    ##< The name / reference in the original code.
        self.sStdRef  = ''.join(sOrgRef.split());   ##< Normalized name to deal with spaces in macro invocations and such.
        self.sType    = sType;                      ##< The type (typically derived).
        self.oStmt    = oStmt;                      ##< The statement making the reference.
        self.iParam   = iParam;                     ##< The parameter containing the references.
        self.offParam = offParam;                   ##< The offset in the parameter of the reference.


class ThreadedFunction(object):
    """
    A threaded function.
    """

    def __init__(self, oMcBlock):
        self.oMcBlock               = oMcBlock  # type: IEMAllInstructionsPython.McBlock
        ## Dictionary of local variables (IEM_MC_LOCAL[_CONST]) and call arguments (IEM_MC_ARG*).
        self.dVariables  = {}           # type: dict(str,McStmtVar)
        ##
        self.aoParamRefs = []           # type: list(ThreadedParamRef)
        self.dParamRefs  = {}           # type: dict(str,list(ThreadedParamRef))
        self.cMinParams  = 0;           ##< Minimum number of parameters to the threaded function.

    @staticmethod
    def dummyInstance():
        """ Gets a dummy instance. """
        return ThreadedFunction(iai.McBlock('null', 999999999, 999999999, 'nil', 999999999));

    def raiseProblem(self, sMessage):
        """ Raises a problem. """
        raise Exception('%s:%s: error: %s' % (self.oMcBlock.sSrcFile, self.oMcBlock.iBeginLine, sMessage, ));

    def getIndexName(self):
        sName = self.oMcBlock.sFunction;
        if sName.startswith('iemOp_'):
            sName = sName[len('iemOp_'):];
        if self.oMcBlock.iInFunction == 0:
            return 'kIemThreadedFunc_%s' % ( sName, );
        return 'kIemThreadedFunc_%s_%s' % ( sName, self.oMcBlock.iInFunction, );

    def getFunctionName(self):
        sName = self.oMcBlock.sFunction;
        if sName.startswith('iemOp_'):
            sName = sName[len('iemOp_'):];
        if self.oMcBlock.iInFunction == 0:
            return 'iemThreadedFunc_%s' % ( sName, );
        return 'iemThreadedFunc_%s_%s' % ( sName, self.oMcBlock.iInFunction, );

    def analyzeReferenceToType(self, sRef):
        """
        Translates a variable or structure reference to a type.
        Returns type name.
        Raises exception if unable to figure it out.
        """
        ch0 = sRef[0];
        if ch0 == 'u':
            if sRef.startswith('u32'):
                return 'uint32_t';
            if sRef.startswith('u8') or sRef == 'uReg':
                return 'uint8_t';
            if sRef.startswith('u64'):
                return 'uint64_t';
            if sRef.startswith('u16'):
                return 'uint16_t';
        elif ch0 == 'b':
            return 'uint8_t';
        elif ch0 == 'f':
            return 'bool';
        elif ch0 == 'i':
            if sRef.startswith('i8'):
                return 'int8_t';
            if sRef.startswith('i16'):
                return 'int32_t';
            if sRef.startswith('i32'):
                return 'int32_t';
            if sRef.startswith('i64'):
                return 'int64_t';
            if sRef in ('iReg', 'iSegReg', 'iSrcReg', 'iDstReg'):
                return 'uint8_t';
        elif ch0 == 'p':
            if sRef.find('-') < 0:
                return 'uintptr_t';
            if sRef.startswith('pVCpu->iem.s.'):
                sField = sRef[len('pVCpu->iem.s.') : ];
                if sField in g_kdIemFieldToType:
                    if g_kdIemFieldToType[sField][0]:
                        return g_kdIemFieldToType[sField][0];
                    self.raiseProblem('Reference out-of-bounds decoder field: %s' % (sRef,));
        elif ch0 == 'G' and sRef.startswith('GCPtr'):
            return 'uint64_t';
        elif sRef == 'cShift': ## @todo risky
            return 'uint8_t';
        self.raiseProblem('Unknown reference: %s' % (sRef,));
        return None; # Shut up pylint 2.16.2.

    def analyzeConsolidateThreadedParamRefs(self):
        """
        Consolidate threaded function parameter references into a dictionary
        with lists of the references to each variable/field.
        """
        # Gather unique parameters.
        self.dParamRefs = {};
        for oRef in self.aoParamRefs:
            if oRef.sStdRef not in self.dParamRefs:
                self.dParamRefs[oRef.sStdRef] = [oRef,];
            else:
                self.dParamRefs[oRef.sStdRef].append(oRef);

        # Generate names for them for use in the threaded function.
        dParamNames = {};
        for sName, aoRefs in self.dParamRefs.items():
            # Morph the reference expression into a name.
            if sName.startswith('IEM_GET_MODRM_REG'):           sName = 'bModRmRegP';
            elif sName.startswith('IEM_GET_MODRM_RM'):          sName = 'bModRmRmP';
            elif sName.startswith('IEM_GET_MODRM_REG_8'):       sName = 'bModRmReg8P';
            elif sName.startswith('IEM_GET_MODRM_RM_8'):        sName = 'bModRmRm8P';
            elif sName.startswith('IEM_GET_EFFECTIVE_VVVV'):    sName = 'bEffVvvvP';
            elif sName.find('.') >= 0 or sName.find('->') >= 0:
                sName = sName[max(sName.rfind('.'), sName.rfind('>')) + 1 : ] + 'P';
            else:
                sName += 'P';

            # Ensure it's unique.
            if sName in dParamNames:
                for i in range(10):
                    if sName + str(i) not in dParamNames:
                        sName += str(i);
                        break;
            dParamNames[sName] = True;

            # Update all the references.
            for oRef in aoRefs:
                oRef.sNewName = sName;

        # Organize them by size too for the purpose of optimize them.
        dBySize = {}        # type: dict(str,str)
        for sStdRef, aoRefs in self.dParamRefs.items():
            cBits = g_kdTypeInfo[aoRefs[0].sType][0];
            assert(cBits <= 64);
            if cBits not in dBySize:
                dBySize[cBits] = [sStdRef,]
            else:
                dBySize[cBits].append(sStdRef);

        # Pack the parameters as best as we can, starting with the largest ones
        # and ASSUMING a 64-bit parameter size.
        self.cMinParams = 0;
        offParam        = 0;
        for cBits in sorted(dBySize.keys(), reverse = True):
            for sStdRef in dBySize[cBits]:
                if offParam < 64:
                    offParam        += cBits;
                else:
                    self.cMinParams += 1;
                    offParam         = cBits;
                assert(offParam <= 64);

                for oRef in self.dParamRefs[sStdRef]:
                    oRef.iParam   = self.cMinParams;
                    oRef.offParam = offParam - cBits;

        if offParam > 0:
            self.cMinParams += 1;

        # Currently there are a few that requires 4 parameters, list these so we can figure out why:
        if self.cMinParams >= 3:
            print('debug: cMinParams=%s cRawParams=%s - %s:%d'
                  % (self.cMinParams, len(self.dParamRefs), self.oMcBlock.sSrcFile, self.oMcBlock.iBeginLine,));

        return True;

    ksHexDigits = '0123456789abcdefABCDEF';

    def analyzeFindThreadedParamRefs(self, aoStmts):
        """
        Scans the statements for things that have to passed on to the threaded
        function (populates self.aoParamRefs).
        """
        for oStmt in aoStmts:
            # Some statements we can skip alltogether.
            if isinstance(oStmt, (iai.McStmtVar, iai.McCppPreProc)):
                continue;
            if not oStmt.asParams:
                continue;
            if oStmt.isCppStmt() and oStmt.fDecode:
                continue;

            # Inspect the target of calls to see if we need to pass down a
            # function pointer or function table pointer for it to work.
            aiSkipParams = {};
            if isinstance(oStmt, iai.McStmtCall):
                if oStmt.sFn[0] == 'p':
                    self.aoParamRefs.append(ThreadedParamRef(oStmt.sFn, 'uintptr_t', oStmt, oStmt.idxFn));
                elif (    oStmt.sFn[0] != 'i'
                      and not oStmt.sFn.startswith('IEMTARGETCPU_EFL_BEHAVIOR_SELECT')
                      and not oStmt.sFn.startswith('IEM_SELECT_HOST_OR_FALLBACK') ):
                    self.raiseProblem('Bogus function name in %s: %s' % (oStmt.sName, oStmt.sFn,));
                aiSkipParams[oStmt.idxFn] = True;

            # Check all the parameters for bogus references.
            for iParam, sParam in enumerate(oStmt.asParams):
                if iParam not in aiSkipParams  and  sParam not in self.dVariables:
                    # The parameter may contain a C expression, so we have to try
                    # extract the relevant bits, i.e. variables and fields while
                    # ignoring operators and parentheses.
                    offParam = 0;
                    while offParam < len(sParam):
                        # Is it the start of an C identifier? If so, find the end, but don't stop on field separators (->, .).
                        ch = sParam[offParam];
                        if ch.isalpha() or ch == '_':
                            offStart = offParam;
                            offParam += 1;
                            while offParam < len(sParam):
                                ch = sParam[offParam];
                                if not ch.isalnum() and ch != '_' and ch != '.':
                                    if ch != '-' or sParam[offParam + 1] != '>':
                                        # Special hack for the 'CTX_SUFF(pVM)' bit in pVCpu->CTX_SUFF(pVM)->xxxx:
                                        if (    ch == '('
                                            and sParam[offStart : offParam + len('(pVM)->')] == 'pVCpu->CTX_SUFF(pVM)->'):
                                            offParam += len('(pVM)->') - 1;
                                        else:
                                            break;
                                    offParam += 1;
                                offParam += 1;
                            sRef = sParam[offStart : offParam];

                            # For register references, we pass the full register indexes instead as macros
                            # like IEM_GET_MODRM_REG implicitly references pVCpu->iem.s.uRexReg and the
                            # threaded function will be more efficient if we just pass the register index
                            # as a 4-bit param.
                            if (   sRef.startswith('IEM_GET_MODRM')
                                or sRef.startswith('IEM_GET_EFFECTIVE_VVVV') ):
                                offParam = iai.McBlock.skipSpacesAt(sParam, offParam, len(sParam));
                                if sParam[offParam] != '(':
                                    self.raiseProblem('Expected "(" following %s in "%s"' % (sRef, oStmt.renderCode(),));
                                (asMacroParams, offCloseParam) = iai.McBlock.extractParams(sParam, offParam);
                                if asMacroParams is None:
                                    self.raiseProblem('Unable to find ")" for %s in "%s"' % (sRef, oStmt.renderCode(),));
                                self.aoParamRefs.append(ThreadedParamRef(sRef, 'uint8_t', oStmt, iParam, offStart));
                                offParam = offCloseParam + 1;

                            # We can skip known variables.
                            elif sRef in self.dVariables:
                                pass;

                            # Skip certain macro invocations.
                            elif sRef in ('IEM_GET_HOST_CPU_FEATURES',
                                          'IEM_GET_GUEST_CPU_FEATURES',
                                          'IEM_IS_GUEST_CPU_AMD'):
                                offParam = iai.McBlock.skipSpacesAt(sParam, offParam, len(sParam));
                                if sParam[offParam] != '(':
                                    self.raiseProblem('Expected "(" following %s in "%s"' % (sRef, oStmt.renderCode(),));
                                (asMacroParams, offCloseParam) = iai.McBlock.extractParams(sParam, offParam);
                                if asMacroParams is None:
                                    self.raiseProblem('Unable to find ")" for %s in "%s"' % (sRef, oStmt.renderCode(),));
                                offParam = offCloseParam + 1;
                                while offParam < len(sParam) and (sParam[offParam].isalnum() or sParam[offParam] in '_.'):
                                    offParam += 1;

                            # Skip constants, globals, types (casts), sizeof and macros.
                            elif (   sRef.startswith('IEM_OP_PRF_')
                                  or sRef.startswith('IEM_ACCESS_')
                                  or sRef.startswith('X86_GREG_')
                                  or sRef.startswith('X86_SREG_')
                                  or sRef.startswith('X86_EFL_')
                                  or sRef.startswith('X86_FSW_')
                                  or sRef.startswith('X86_FCW_')
                                  or sRef.startswith('g_')
                                  or sRef in ( 'int8_t',    'int16_t',    'int32_t',
                                               'INT8_C',    'INT16_C',    'INT32_C',    'INT64_C',
                                               'UINT8_C',   'UINT16_C',   'UINT32_C',   'UINT64_C',
                                               'UINT8_MAX', 'UINT16_MAX', 'UINT32_MAX', 'UINT64_MAX',
                                               'sizeof',    'NOREF',      'RT_NOREF',   'IEMMODE_64BIT' ) ):
                                pass;

                            # Skip certain macro invocations.
                            # Any variable (non-field) and decoder fields in IEMCPU will need to be parameterized.
                            elif (   (    '.' not in sRef
                                      and '-' not in sRef
                                      and sRef not in ('pVCpu', ) )
                                  or iai.McBlock.koReIemDecoderVars.search(sRef) is not None):
                                self.aoParamRefs.append(ThreadedParamRef(sRef, self.analyzeReferenceToType(sRef),
                                                                         oStmt, iParam, offStart));
                        # Number.
                        elif ch.isdigit():
                            if (    ch == '0'
                                and offParam + 2 <= len(sParam)
                                and sParam[offParam + 1] in 'xX'
                                and sParam[offParam + 2] in self.ksHexDigits ):
                                offParam += 2;
                                while offParam < len(sParam) and sParam[offParam] in self.ksHexDigits:
                                    offParam += 1;
                            else:
                                while offParam < len(sParam) and sParam[offParam].isdigit():
                                    offParam += 1;
                        # Whatever else.
                        else:
                            offParam += 1;

            # Traverse the branches of conditionals.
            if isinstance(oStmt, iai.McStmtCond):
                self.analyzeFindThreadedParamRefs(oStmt.aoIfBranch);
                self.analyzeFindThreadedParamRefs(oStmt.aoElseBranch);
        return True;

    def analyzeFindVariablesAndCallArgs(self, aoStmts):
        """ Scans the statements for MC variables and call arguments. """
        for oStmt in aoStmts:
            if isinstance(oStmt, iai.McStmtVar):
                if oStmt.sVarName in self.dVariables:
                    raise Exception('Variable %s is defined more than once!' % (oStmt.sVarName,));
                self.dVariables[oStmt.sVarName] = oStmt.sVarName;

            # There shouldn't be any variables or arguments declared inside if/
            # else blocks, but scan them too to be on the safe side.
            if isinstance(oStmt, iai.McStmtCond):
                cBefore = len(self.dVariables);
                self.analyzeFindVariablesAndCallArgs(oStmt.aoIfBranch);
                self.analyzeFindVariablesAndCallArgs(oStmt.aoElseBranch);
                if len(self.dVariables) != cBefore:
                    raise Exception('Variables/arguments defined in conditional branches!');
        return True;

    def analyze(self):
        """
        Analyzes the code, identifying the number of parameters it requires and such.
        May raise exceptions if we cannot grok the code.
        """

        # Decode the block into a list/tree of McStmt objects.
        aoStmts = self.oMcBlock.decode();

        # Scan the statements for local variables and call arguments (self.dVariables).
        self.analyzeFindVariablesAndCallArgs(aoStmts);

        # Now scan the code for variables and field references that needs to
        # be passed to the threaded function because they are related to the
        # instruction decoding.
        self.analyzeFindThreadedParamRefs(aoStmts);
        self.analyzeConsolidateThreadedParamRefs();

        return True;

    def generateInputCode(self):
        """
        Modifies the input code.
        """
        assert len(self.oMcBlock.asLines) > 2, "asLines=%s" % (self.oMcBlock.asLines,);
        cchIndent = (self.oMcBlock.cchIndent + 3) // 4 * 4;
        return iai.McStmt.renderCodeForList(self.oMcBlock.aoStmts, cchIndent = cchIndent).replace('\n', ' /* gen */\n', 1);


class IEMThreadedGenerator(object):
    """
    The threaded code generator & annotator.
    """

    def __init__(self):
        self.aoThreadedFuncs = []       # type: list(ThreadedFunction)
        self.oOptions        = None     # type: argparse.Namespace
        self.aoParsers       = []       # type: list(IEMAllInstructionsPython.SimpleParser)

    #
    # Processing.
    #

    def processInputFiles(self):
        """
        Process the input files.
        """

        # Parse the files.
        self.aoParsers = iai.parseFiles(self.oOptions.asInFiles);

        # Wrap MC blocks into threaded functions and analyze these.
        self.aoThreadedFuncs = [ThreadedFunction(oMcBlock) for oMcBlock in iai.g_aoMcBlocks];
        dRawParamCounts = {};
        dMinParamCounts = {};
        for oThreadedFunction in self.aoThreadedFuncs:
            oThreadedFunction.analyze();
            dRawParamCounts[len(oThreadedFunction.dParamRefs)] = dRawParamCounts.get(len(oThreadedFunction.dParamRefs), 0) + 1;
            dMinParamCounts[oThreadedFunction.cMinParams]      = dMinParamCounts.get(oThreadedFunction.cMinParams,      0) + 1;
        print('debug: param count distribution, raw and optimized:', file = sys.stderr);
        for cCount in sorted({cBits: True for cBits in list(dRawParamCounts.keys()) + list(dMinParamCounts.keys())}.keys()):
            print('debug:     %s params: %4s raw, %4s min'
                  % (cCount, dRawParamCounts.get(cCount, 0), dMinParamCounts.get(cCount, 0)),
                  file = sys.stderr);

        return True;

    #
    # Output
    #

    def generateLicenseHeader(self):
        """
        Returns the lines for a license header.
        """
        return [
            '/*',
            ' * Autogenerated by $Id$ ',
            ' * Do not edit!',
            ' */',
            '',
            '/*',
            ' * Copyright (C) 2023-' + str(datetime.date.today().year) + ' Oracle and/or its affiliates.',
            ' *',
            ' * This file is part of VirtualBox base platform packages, as',
            ' * available from https://www.virtualbox.org.',
            ' *',
            ' * This program is free software; you can redistribute it and/or',
            ' * modify it under the terms of the GNU General Public License',
            ' * as published by the Free Software Foundation, in version 3 of the',
            ' * License.',
            ' *',
            ' * This program is distributed in the hope that it will be useful, but',
            ' * WITHOUT ANY WARRANTY; without even the implied warranty of',
            ' * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU',
            ' * General Public License for more details.',
            ' *',
            ' * You should have received a copy of the GNU General Public License',
            ' * along with this program; if not, see <https://www.gnu.org/licenses>.',
            ' *',
            ' * The contents of this file may alternatively be used under the terms',
            ' * of the Common Development and Distribution License Version 1.0',
            ' * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included',
            ' * in the VirtualBox distribution, in which case the provisions of the',
            ' * CDDL are applicable instead of those of the GPL.',
            ' *',
            ' * You may elect to license modified versions of this file under the',
            ' * terms and conditions of either the GPL or the CDDL or both.',
            ' *',
            ' * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0',
            ' */',
            '',
            '',
            '',
        ];


    def generateThreadedFunctionsHeader(self, oOut):
        """
        Generates the threaded functions header file.
        Returns success indicator.
        """

        asLines = self.generateLicenseHeader();

        # Generate the threaded function table indexes.
        asLines += [
            'typedef enum IEMTHREADEDFUNCS',
            '{',
            '    kIemThreadedFunc_Invalid = 0,',
        ];
        for oFunc in self.aoThreadedFuncs:
            asLines.append('    ' + oFunc.getIndexName() + ',');
        asLines += [
            '    kIemThreadedFunc_End',
            '} IEMTHREADEDFUNCS;',
            '',
        ];

        # Prototype the function table.
        sFnType = 'typedef IEM_DECL_IMPL_TYPE(VBOXSTRICTRC, FNIEMTHREADEDFUNC, (PVMCPU pVCpu';
        for iParam in range(g_kcThreadedParams):
            sFnType += ', uint64_t uParam' + str(iParam);
        sFnType += '));'

        asLines += [
            sFnType,
            'typedef FNIEMTHREADEDFUNC *PFNIEMTHREADEDFUNC;',
            '',
            'extern const PFNIEMTHREADEDFUNC g_apfnIemThreadedFunctions[kIemThreadedFunc_End];',
        ];

        oOut.write('\n'.join(asLines));
        return True;

    ksBitsToIntMask = {
        1:  "UINT64_C(0x1)",
        2:  "UINT64_C(0x3)",
        4:  "UINT64_C(0xf)",
        8:  "UINT64_C(0xff)",
        16: "UINT64_C(0xffff)",
        32: "UINT64_C(0xffffffff)",
    };
    def generateThreadedFunctionsSource(self, oOut):
        """
        Generates the threaded functions source file.
        Returns success indicator.
        """

        asLines = self.generateLicenseHeader();
        oOut.write('\n'.join(asLines));

        # Prepare the fixed bits.
        sParamList = '(PVMCPU pVCpu';
        for iParam in range(g_kcThreadedParams):
            sParamList += ', uint64_t uParam' + str(iParam);
        sParamList += '))\n';

        #
        # Emit the function definitions.
        #
        for oThreadedFunction in self.aoThreadedFuncs:
            oMcBlock = oThreadedFunction.oMcBlock;
            # Function header
            oOut.write(  '\n'
                       + '\n'
                       + '/**\n'
                       + ' * %s at line %s offset %s in %s%s\n'
                          % (oMcBlock.sFunction, oMcBlock.iBeginLine, oMcBlock.offBeginLine, os.path.split(oMcBlock.sSrcFile)[1],
                             ' (macro expansion)' if oMcBlock.iBeginLine == oMcBlock.iEndLine else '')
                       + ' */\n'
                       + 'static IEM_DECL_IMPL_DEF(VBOXSTRICTRC, ' + oThreadedFunction.getFunctionName() + ',\n'
                       + '                         ' + sParamList
                       + '{\n');

            aasVars = [];
            for aoRefs in oThreadedFunction.dParamRefs.values():
                oRef  = aoRefs[0];
                cBits = g_kdTypeInfo[oRef.sType][0];

                sTypeDecl = oRef.sType + ' const';

                if cBits == 64:
                    assert oRef.offParam == 0;
                    if oRef.sType == 'uint64_t':
                        sUnpack = 'uParam%s;' % (oRef.iParam,);
                    else:
                        sUnpack = '(%s)uParam%s;' % (oRef.sType, oRef.iParam,);
                elif oRef.offParam == 0:
                    sUnpack = '(%s)(uParam%s & %s);' % (oRef.sType, oRef.iParam, self.ksBitsToIntMask[cBits]);
                else:
                    sUnpack = '(%s)((uParam%s >> %s) & %s);' \
                            % (oRef.sType, oRef.iParam, oRef.offParam, self.ksBitsToIntMask[cBits]);

                sComment = '/* %s - %s ref%s */' % (oRef.sOrgRef, len(aoRefs), 's' if len(aoRefs) != 1 else '',);

                aasVars.append([ '%s:%02u' % (oRef.iParam, oRef.offParam), sTypeDecl, oRef.sNewName, sUnpack, sComment ]);
            acchVars = [0, 0, 0, 0, 0];
            for asVar in aasVars:
                for iCol, sStr in enumerate(asVar):
                    acchVars[iCol] = max(acchVars[iCol], len(sStr));
            sFmt = '    %%-%ss %%-%ss = %%-%ss %%s\n' % (acchVars[1], acchVars[2], acchVars[3]);
            for asVar in sorted(aasVars):
                oOut.write(sFmt % (asVar[1], asVar[2], asVar[3], asVar[4],));

            # RT_NOREF for unused parameters.
            if oThreadedFunction.cMinParams < g_kcThreadedParams:
                oOut.write('    RT_NOREF('
                           + ', '.join(['uParam%u' % (i,) for i in range(oThreadedFunction.cMinParams, g_kcThreadedParams)])
                           + ');\n');


            oOut.write('}\n');

        #
        # Emit the function table.
        #
        oOut.write(  '\n'
                   + '\n'
                   + '/**\n'
                   + ' * Function table.\n'
                   + ' */\n'
                   + 'const PFNIEMTHREADEDFUNC g_apfnIemThreadedFunctions[kIemThreadedFunc_End] =\n'
                   + '{\n'
                   + '    /*Invalid*/ NULL, \n');
        for iThreadedFunction, oThreadedFunction in enumerate(self.aoThreadedFuncs):
            oOut.write('    /*%4u*/ %s,\n' % (iThreadedFunction + 1, oThreadedFunction.getFunctionName(),));
        oOut.write('};\n');

        return True;

    def getThreadedFunctionByIndex(self, idx):
        """
        Returns a ThreadedFunction object for the given index.  If the index is
        out of bounds, a dummy is returned.
        """
        if idx < len(self.aoThreadedFuncs):
            return self.aoThreadedFuncs[idx];
        return ThreadedFunction.dummyInstance();

    def findEndOfMcEndStmt(self, sLine, offEndStmt):
        """
        Helper that returns the line offset following the 'IEM_MC_END();'.
        """
        assert sLine[offEndStmt:].startswith('IEM_MC_END');
        off = sLine.find(';', offEndStmt + len('IEM_MC_END'));
        assert off > 0, 'sLine="%s"' % (sLine, );
        return off + 1 if off > 0 else 99999998;

    def generateModifiedInput(self, oOut):
        """
        Generates the combined modified input source/header file.
        Returns success indicator.
        """
        #
        # File header.
        #
        oOut.write('\n'.join(self.generateLicenseHeader()));

        #
        # ASSUMING that g_aoMcBlocks/self.aoThreadedFuncs are in self.aoParsers
        # order, we iterate aoThreadedFuncs in parallel to the lines from the
        # parsers and apply modifications where required.
        #
        iThreadedFunction = 0;
        oThreadedFunction = self.getThreadedFunctionByIndex(0);
        for oParser in self.aoParsers: # IEMAllInstructionsPython.SimpleParser
            oOut.write("\n\n/* ****** BEGIN %s ******* */\n" % (oParser.sSrcFile,));

            iLine = 0;
            while iLine < len(oParser.asLines):
                sLine = oParser.asLines[iLine];
                iLine += 1;                 # iBeginLine and iEndLine are 1-based.

                # Can we pass it thru?
                if (   iLine not in [oThreadedFunction.oMcBlock.iBeginLine, oThreadedFunction.oMcBlock.iEndLine]
                    or oThreadedFunction.oMcBlock.sSrcFile != oParser.sSrcFile):
                    oOut.write(sLine);
                #
                # Single MC block.  Just extract it and insert the replacement.
                #
                elif oThreadedFunction.oMcBlock.iBeginLine != oThreadedFunction.oMcBlock.iEndLine:
                    assert sLine.count('IEM_MC_') == 1;
                    oOut.write(sLine[:oThreadedFunction.oMcBlock.offBeginLine]);
                    sModified = oThreadedFunction.generateInputCode().strip();
                    oOut.write(sModified);

                    iLine = oThreadedFunction.oMcBlock.iEndLine;
                    sLine = oParser.asLines[iLine - 1];
                    assert sLine.count('IEM_MC_') == 1;
                    oOut.write(sLine[self.findEndOfMcEndStmt(sLine, oThreadedFunction.oMcBlock.offEndLine) : ]);

                    # Advance
                    iThreadedFunction += 1;
                    oThreadedFunction  = self.getThreadedFunctionByIndex(iThreadedFunction);
                #
                # Macro expansion line that have sublines and may contain multiple MC blocks.
                #
                else:
                    offLine = 0;
                    while iLine == oThreadedFunction.oMcBlock.iBeginLine:
                        oOut.write(sLine[offLine : oThreadedFunction.oMcBlock.offBeginLine]);

                        sModified = oThreadedFunction.generateInputCode().strip();
                        assert sModified.startswith('IEM_MC_BEGIN'), 'sModified="%s"' % (sModified,);
                        oOut.write(sModified);

                        offLine = self.findEndOfMcEndStmt(sLine, oThreadedFunction.oMcBlock.offEndLine);

                        # Advance
                        iThreadedFunction += 1;
                        oThreadedFunction  = self.getThreadedFunctionByIndex(iThreadedFunction);

                    # Last line segment.
                    if offLine < len(sLine):
                        oOut.write(sLine[offLine : ]);

            oOut.write("/* ****** END %s ******* */\n" % (oParser.sSrcFile,));

        return True;

    #
    # Main
    #

    def main(self, asArgs):
        """
        C-like main function.
        Returns exit code.
        """

        #
        # Parse arguments
        #
        sScriptDir = os.path.dirname(__file__);
        oParser = argparse.ArgumentParser(add_help = False);
        oParser.add_argument('asInFiles',       metavar = 'input.cpp.h',        nargs = '*',
                             default = [os.path.join(sScriptDir, asFM[0]) for asFM in iai.g_aasAllInstrFilesAndDefaultMap],
                             help = "Selection of VMMAll/IEMAllInstructions*.cpp.h files to use as input.");
        oParser.add_argument('--out-funcs-hdr', metavar = 'file-funcs.h',       dest = 'sOutFileFuncsHdr', action = 'store',
                             default = '-', help = 'The output header file for the functions.');
        oParser.add_argument('--out-funcs-cpp', metavar = 'file-funcs.cpp',     dest = 'sOutFileFuncsCpp', action = 'store',
                             default = '-', help = 'The output C++ file for the functions.');
        oParser.add_argument('--out-mod-input', metavar = 'file-instr.cpp.h',   dest = 'sOutFileModInput', action = 'store',
                             default = '-', help = 'The output C++/header file for the modified input instruction files.');
        oParser.add_argument('--help', '-h', '-?', action = 'help', help = 'Display help and exit.');
        oParser.add_argument('--version', '-V', action = 'version',
                             version = 'r%s (IEMAllThreadedPython.py), r%s (IEMAllInstructionsPython.py)'
                                     % (__version__.split()[1], iai.__version__.split()[1],),
                             help = 'Displays the version/revision of the script and exit.');
        self.oOptions = oParser.parse_args(asArgs[1:]);
        print("oOptions=%s" % (self.oOptions,));

        #
        # Process the instructions specified in the IEM sources.
        #
        if self.processInputFiles():
            #
            # Generate the output files.
            #
            aaoOutputFiles = (
                 ( self.oOptions.sOutFileFuncsHdr, self.generateThreadedFunctionsHeader ),
                 ( self.oOptions.sOutFileFuncsCpp, self.generateThreadedFunctionsSource ),
                 ( self.oOptions.sOutFileModInput, self.generateModifiedInput ),
            );
            fRc = True;
            for sOutFile, fnGenMethod in aaoOutputFiles:
                if sOutFile == '-':
                    fRc = fnGenMethod(sys.stdout) and fRc;
                else:
                    try:
                        oOut = open(sOutFile, 'w');                 # pylint: disable=consider-using-with,unspecified-encoding
                    except Exception as oXcpt:
                        print('error! Failed open "%s" for writing: %s' % (sOutFile, oXcpt,), file = sys.stderr);
                        return 1;
                    fRc = fnGenMethod(oOut) and fRc;
                    oOut.close();
            if fRc:
                return 0;

        return 1;


if __name__ == '__main__':
    sys.exit(IEMThreadedGenerator().main(sys.argv));

