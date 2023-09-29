#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=invalid-name

"""
Annotates and generates threaded functions from IEMAllInst*.cpp.h.
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
import copy;
import datetime;
import os;
import re;
import sys;
import argparse;

import IEMAllInstPython as iai;
import IEMAllN8vePython as ian;


# Python 3 hacks:
if sys.version_info[0] >= 3:
    long = int;     # pylint: disable=redefined-builtin,invalid-name

## Number of generic parameters for the thread functions.
g_kcThreadedParams = 3;

g_kdTypeInfo = {
    # type name:    (cBits, fSigned, C-type       )
    'int8_t':       (    8,    True, 'int8_t',    ),
    'int16_t':      (   16,    True, 'int16_t',   ),
    'int32_t':      (   32,    True, 'int32_t',   ),
    'int64_t':      (   64,    True, 'int64_t',   ),
    'uint4_t':      (    4,   False, 'uint8_t',   ),
    'uint8_t':      (    8,   False, 'uint8_t',   ),
    'uint16_t':     (   16,   False, 'uint16_t',  ),
    'uint32_t':     (   32,   False, 'uint32_t',  ),
    'uint64_t':     (   64,   False, 'uint64_t',  ),
    'uintptr_t':    (   64,   False, 'uintptr_t', ), # ASSUMES 64-bit host pointer size.
    'bool':         (    1,   False, 'bool',      ),
    'IEMMODE':      (    2,   False, 'IEMMODE',   ),
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

    def __init__(self, sOrgRef, sType, oStmt, iParam = None, offParam = 0, sStdRef = None):
        ## The name / reference in the original code.
        self.sOrgRef    = sOrgRef;
        ## Normalized name to deal with spaces in macro invocations and such.
        self.sStdRef    = sStdRef if sStdRef else ''.join(sOrgRef.split());
        ## Indicates that sOrgRef may not match the parameter.
        self.fCustomRef = sStdRef is not None;
        ## The type (typically derived).
        self.sType      = sType;
        ## The statement making the reference.
        self.oStmt      = oStmt;
        ## The parameter containing the references. None if implicit.
        self.iParam     = iParam;
        ## The offset in the parameter of the reference.
        self.offParam   = offParam;

        ## The variable name in the threaded function.
        self.sNewName     = 'x';
        ## The this is packed into.
        self.iNewParam    = 99;
        ## The bit offset in iNewParam.
        self.offNewParam  = 1024


class ThreadedFunctionVariation(object):
    """ Threaded function variation. """

    ## @name Variations.
    ## These variations will match translation block selection/distinctions as well.
    ## @{
    ksVariation_Default     = '';               ##< No variations - only used by IEM_MC_DEFER_TO_CIMPL_X_RET.
    ksVariation_16          = '_16';            ##< 16-bit mode code (386+).
    ksVariation_16_Addr32   = '_16_Addr32';     ##< 16-bit mode code (386+), address size prefixed to 32-bit addressing.
    ksVariation_16_Pre386   = '_16_Pre386';     ##< 16-bit mode code, pre-386 CPU target.
    ksVariation_32          = '_32';            ##< 32-bit mode code (386+).
    ksVariation_32_Flat     = '_32_Flat';       ##< 32-bit mode code (386+) with CS, DS, E,S and SS flat and 4GB wide.
    ksVariation_32_Addr16   = '_32_Addr16';     ##< 32-bit mode code (386+), address size prefixed to 16-bit addressing.
    ksVariation_64          = '_64';            ##< 64-bit mode code.
    ksVariation_64_FsGs     = '_64_FsGs';       ##< 64-bit mode code, with memory accesses via FS or GS.
    ksVariation_64_Addr32   = '_64_Addr32';     ##< 64-bit mode code, address size prefixed to 32-bit addressing.
    kasVariations           = (
        ksVariation_Default,
        ksVariation_16,
        ksVariation_16_Addr32,
        ksVariation_16_Pre386,
        ksVariation_32,
        ksVariation_32_Flat,
        ksVariation_32_Addr16,
        ksVariation_64,
        ksVariation_64_FsGs,
        ksVariation_64_Addr32,
    );
    kasVariationsWithoutAddress = (
        ksVariation_16,
        ksVariation_16_Pre386,
        ksVariation_32,
        ksVariation_64,
    );
    kasVariationsWithoutAddressNot286 = (
        ksVariation_16,
        ksVariation_32,
        ksVariation_64,
    );
    kasVariationsWithoutAddressNot286Not64 = (
        ksVariation_16,
        ksVariation_32,
    );
    kasVariationsWithoutAddressNot64 = (
        ksVariation_16,
        ksVariation_16_Pre386,
        ksVariation_32,
    );
    kasVariationsWithoutAddressOnly64 = (
        ksVariation_64,
    );
    kasVariationsWithAddress = (
        ksVariation_16,
        ksVariation_16_Addr32,
        ksVariation_16_Pre386,
        ksVariation_32,
        ksVariation_32_Flat,
        ksVariation_32_Addr16,
        ksVariation_64,
        ksVariation_64_FsGs,
        ksVariation_64_Addr32,
    );
    kasVariationsWithAddressNot286 = (
        ksVariation_16,
        ksVariation_16_Addr32,
        ksVariation_32,
        ksVariation_32_Flat,
        ksVariation_32_Addr16,
        ksVariation_64,
        ksVariation_64_FsGs,
        ksVariation_64_Addr32,
    );
    kasVariationsWithAddressNot286Not64 = (
        ksVariation_16,
        ksVariation_16_Addr32,
        ksVariation_32,
        ksVariation_32_Flat,
        ksVariation_32_Addr16,
    );
    kasVariationsWithAddressNot64 = (
        ksVariation_16,
        ksVariation_16_Addr32,
        ksVariation_16_Pre386,
        ksVariation_32,
        ksVariation_32_Flat,
        ksVariation_32_Addr16,
    );
    kasVariationsWithAddressOnly64 = (
        ksVariation_64,
        ksVariation_64_FsGs,
        ksVariation_64_Addr32,
    );
    kasVariationsEmitOrder = (
        ksVariation_Default,
        ksVariation_64,
        ksVariation_64_FsGs,
        ksVariation_32_Flat,
        ksVariation_32,
        ksVariation_16,
        ksVariation_16_Addr32,
        ksVariation_16_Pre386,
        ksVariation_32_Addr16,
        ksVariation_64_Addr32,
    );
    kdVariationNames = {
        ksVariation_Default:    'defer-to-cimpl',
        ksVariation_16:         '16-bit',
        ksVariation_16_Addr32:  '16-bit w/ address prefix (Addr32)',
        ksVariation_16_Pre386:  '16-bit on pre-386 CPU',
        ksVariation_32:         '32-bit',
        ksVariation_32_Flat:    '32-bit flat and wide open CS, SS, DS and ES',
        ksVariation_32_Addr16:  '32-bit w/ address prefix (Addr16)',
        ksVariation_64:         '64-bit',
        ksVariation_64_FsGs:    '64-bit with memory accessed via FS or GS',
        ksVariation_64_Addr32:  '64-bit w/ address prefix (Addr32)',

    };
    ## @}

    ## IEM_CIMPL_F_XXX flags that we know.
    ## The value indicates whether it terminates the TB or not. The goal is to
    ## improve the recompiler so all but END_TB will be False.
    ##
    ## @note iemThreadedRecompilerMcDeferToCImpl0 duplicates info found here.
    kdCImplFlags = {
        'IEM_CIMPL_F_MODE':                 False,
        'IEM_CIMPL_F_BRANCH_DIRECT':        False,
        'IEM_CIMPL_F_BRANCH_INDIRECT':      False,
        'IEM_CIMPL_F_BRANCH_RELATIVE':      False,
        'IEM_CIMPL_F_BRANCH_FAR':           True,
        'IEM_CIMPL_F_BRANCH_CONDITIONAL':   False,
        'IEM_CIMPL_F_RFLAGS':               False,
        'IEM_CIMPL_F_CHECK_IRQ_AFTER':      False,
        'IEM_CIMPL_F_CHECK_IRQ_BEFORE':     False,
        'IEM_CIMPL_F_STATUS_FLAGS':         False,
        'IEM_CIMPL_F_VMEXIT':               False,
        'IEM_CIMPL_F_FPU':                  False,
        'IEM_CIMPL_F_REP':                  False,
        'IEM_CIMPL_F_IO':                   False,
        'IEM_CIMPL_F_END_TB':               True,
        'IEM_CIMPL_F_XCPT':                 True,
    };

    def __init__(self, oThreadedFunction, sVariation = ksVariation_Default):
        self.oParent        = oThreadedFunction # type: ThreadedFunction
        ##< ksVariation_Xxxx.
        self.sVariation     = sVariation

        ## Threaded function parameter references.
        self.aoParamRefs    = []            # type: list(ThreadedParamRef)
        ## Unique parameter references.
        self.dParamRefs     = {}            # type: dict(str,list(ThreadedParamRef))
        ## Minimum number of parameters to the threaded function.
        self.cMinParams     = 0;

        ## List/tree of statements for the threaded function.
        self.aoStmtsForThreadedFunction = [] # type: list(McStmt)

        ## Dictionary with any IEM_CIMPL_F_XXX flags associated to the code block.
        self.dsCImplFlags   = { }           # type: dict(str, bool)

        ## Function enum number, for verification. Set by generateThreadedFunctionsHeader.
        self.iEnumValue     = -1;

        ## Native recompilation details for this variation.
        self.oNativeRecomp  = None;

    def getIndexName(self):
        sName = self.oParent.oMcBlock.sFunction;
        if sName.startswith('iemOp_'):
            sName = sName[len('iemOp_'):];
        if self.oParent.oMcBlock.iInFunction == 0:
            return 'kIemThreadedFunc_%s%s' % ( sName, self.sVariation, );
        return 'kIemThreadedFunc_%s_%s%s' % ( sName, self.oParent.oMcBlock.iInFunction, self.sVariation, );

    def getThreadedFunctionName(self):
        sName = self.oParent.oMcBlock.sFunction;
        if sName.startswith('iemOp_'):
            sName = sName[len('iemOp_'):];
        if self.oParent.oMcBlock.iInFunction == 0:
            return 'iemThreadedFunc_%s%s' % ( sName, self.sVariation, );
        return 'iemThreadedFunc_%s_%s%s' % ( sName, self.oParent.oMcBlock.iInFunction, self.sVariation, );

    def getNativeFunctionName(self):
        return 'iemNativeRecompFunc_' + self.getThreadedFunctionName()[len('iemThreadedFunc_'):];

    #
    # Analysis and code morphing.
    #

    def raiseProblem(self, sMessage):
        """ Raises a problem. """
        self.oParent.raiseProblem(sMessage);

    def warning(self, sMessage):
        """ Emits a warning. """
        self.oParent.warning(sMessage);

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
                return 'int16_t';
            if sRef.startswith('i32'):
                return 'int32_t';
            if sRef.startswith('i64'):
                return 'int64_t';
            if sRef in ('iReg', 'iFixedReg', 'iGReg', 'iSegReg', 'iSrcReg', 'iDstReg', 'iCrReg'):
                return 'uint8_t';
        elif ch0 == 'p':
            if sRef.find('-') < 0:
                return 'uintptr_t';
            if sRef.startswith('pVCpu->iem.s.'):
                sField = sRef[len('pVCpu->iem.s.') : ];
                if sField in g_kdIemFieldToType:
                    if g_kdIemFieldToType[sField][0]:
                        return g_kdIemFieldToType[sField][0];
        elif ch0 == 'G' and sRef.startswith('GCPtr'):
            return 'uint64_t';
        elif ch0 == 'e':
            if sRef == 'enmEffOpSize':
                return 'IEMMODE';
        elif ch0 == 'o':
            if sRef.startswith('off32'):
                return 'uint32_t';
        elif sRef == 'cbFrame':  # enter
            return 'uint16_t';
        elif sRef == 'cShift': ## @todo risky
            return 'uint8_t';

        self.raiseProblem('Unknown reference: %s' % (sRef,));
        return None; # Shut up pylint 2.16.2.

    def analyzeCallToType(self, sFnRef):
        """
        Determins the type of an indirect function call.
        """
        assert sFnRef[0] == 'p';

        #
        # Simple?
        #
        if sFnRef.find('-') < 0:
            oDecoderFunction = self.oParent.oMcBlock.oFunction;

            # Try the argument list of the function defintion macro invocation first.
            iArg = 2;
            while iArg < len(oDecoderFunction.asDefArgs):
                if sFnRef == oDecoderFunction.asDefArgs[iArg]:
                    return oDecoderFunction.asDefArgs[iArg - 1];
                iArg += 1;

            # Then check out line that includes the word and looks like a variable declaration.
            oRe = re.compile(' +(P[A-Z0-9_]+|const +IEMOP[A-Z0-9_]+ *[*]) +(const |) *' + sFnRef + ' *(;|=)');
            for sLine in oDecoderFunction.asLines:
                oMatch = oRe.match(sLine);
                if oMatch:
                    if not oMatch.group(1).startswith('const'):
                        return oMatch.group(1);
                    return 'PC' + oMatch.group(1)[len('const ') : -1].strip();

        #
        # Deal with the pImpl->pfnXxx:
        #
        elif sFnRef.startswith('pImpl->pfn'):
            sMember   = sFnRef[len('pImpl->') : ];
            sBaseType = self.analyzeCallToType('pImpl');
            offBits   = sMember.rfind('U') + 1;
            if sBaseType == 'PCIEMOPBINSIZES':          return 'PFNIEMAIMPLBINU'        + sMember[offBits:];
            if sBaseType == 'PCIEMOPUNARYSIZES':        return 'PFNIEMAIMPLUNARYU'      + sMember[offBits:];
            if sBaseType == 'PCIEMOPSHIFTSIZES':        return 'PFNIEMAIMPLSHIFTU'      + sMember[offBits:];
            if sBaseType == 'PCIEMOPSHIFTDBLSIZES':     return 'PFNIEMAIMPLSHIFTDBLU'   + sMember[offBits:];
            if sBaseType == 'PCIEMOPMULDIVSIZES':       return 'PFNIEMAIMPLMULDIVU'     + sMember[offBits:];
            if sBaseType == 'PCIEMOPMEDIAF3':           return 'PFNIEMAIMPLMEDIAF3U'    + sMember[offBits:];
            if sBaseType == 'PCIEMOPMEDIAOPTF3':        return 'PFNIEMAIMPLMEDIAOPTF3U' + sMember[offBits:];
            if sBaseType == 'PCIEMOPMEDIAOPTF2':        return 'PFNIEMAIMPLMEDIAOPTF2U' + sMember[offBits:];
            if sBaseType == 'PCIEMOPMEDIAOPTF3IMM8':    return 'PFNIEMAIMPLMEDIAOPTF3U' + sMember[offBits:] + 'IMM8';
            if sBaseType == 'PCIEMOPBLENDOP':           return 'PFNIEMAIMPLAVXBLENDU'   + sMember[offBits:];

            self.raiseProblem('Unknown call reference: %s::%s (%s)' % (sBaseType, sMember, sFnRef,));

        self.raiseProblem('Unknown call reference: %s' % (sFnRef,));
        return None; # Shut up pylint 2.16.2.

    def analyze8BitGRegStmt(self, oStmt):
        """
        Gets the 8-bit general purpose register access details of the given statement.
        ASSUMES the statement is one accessing an 8-bit GREG.
        """
        idxReg = 0;
        if (   oStmt.sName.find('_FETCH_') > 0
            or oStmt.sName.find('_REF_') > 0
            or oStmt.sName.find('_TO_LOCAL') > 0):
            idxReg = 1;

        sRegRef = oStmt.asParams[idxReg];
        if sRegRef.startswith('IEM_GET_MODRM_RM') or sRegRef.startswith('IEM_GET_MODRM_REG'):
            asBits = [sBit.strip() for sBit in sRegRef.replace('(', ',').replace(')', '').split(',')];
            if len(asBits) != 3 or asBits[1] != 'pVCpu' or (asBits[0] != 'IEM_GET_MODRM_RM' and asBits[0] != 'IEM_GET_MODRM_REG'):
                self.raiseProblem('Unexpected reference: %s (asBits=%s)' % (sRegRef, asBits));
            sOrgExpr = asBits[0] + '_EX8(pVCpu, ' + asBits[2] + ')';
        else:
            sOrgExpr = '((%s) < 4 || (pVCpu->iem.s.fPrefixes & IEM_OP_PRF_REX) ? (%s) : (%s) + 12)' % (sRegRef, sRegRef, sRegRef);

        if sRegRef.find('IEM_GET_MODRM_RM') >= 0:    sStdRef = 'bRmRm8Ex';
        elif sRegRef.find('IEM_GET_MODRM_REG') >= 0: sStdRef = 'bRmReg8Ex';
        elif sRegRef == 'X86_GREG_xAX':              sStdRef = 'bGregXAx8Ex';
        elif sRegRef == 'X86_GREG_xCX':              sStdRef = 'bGregXCx8Ex';
        elif sRegRef == 'X86_GREG_xSP':              sStdRef = 'bGregXSp8Ex';
        elif sRegRef == 'iFixedReg':                 sStdRef = 'bFixedReg8Ex';
        else:
            self.warning('analyze8BitGRegStmt: sRegRef=%s -> bOther8Ex; %s %s; sOrgExpr=%s'
                         % (sRegRef, oStmt.sName, oStmt.asParams, sOrgExpr,));
            sStdRef = 'bOther8Ex';

        #print('analyze8BitGRegStmt: %s %s; sRegRef=%s\n -> idxReg=%s sOrgExpr=%s sStdRef=%s'
        #      % (oStmt.sName, oStmt.asParams, sRegRef, idxReg, sOrgExpr, sStdRef));
        return (idxReg, sOrgExpr, sStdRef);


    ## Maps memory related MCs to info for FLAT conversion.
    ## This is used in 64-bit and flat 32-bit variants to skip the unnecessary
    ## segmentation checking for every memory access.  Only applied to access
    ## via ES, DS and SS.  FS, GS and CS gets the full segmentation threatment,
    ## the latter (CS) is just to keep things simple (we could safely fetch via
    ## it, but only in 64-bit mode could we safely write via it, IIRC).
    kdMemMcToFlatInfo = {
        'IEM_MC_FETCH_MEM_U8':                    (  1, 'IEM_MC_FETCH_MEM_FLAT_U8' ),
        'IEM_MC_FETCH_MEM16_U8':                  (  1, 'IEM_MC_FETCH_MEM16_FLAT_U8' ),
        'IEM_MC_FETCH_MEM32_U8':                  (  1, 'IEM_MC_FETCH_MEM32_FLAT_U8' ),
        'IEM_MC_FETCH_MEM_U16':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_U16' ),
        'IEM_MC_FETCH_MEM_U16_DISP':              (  1, 'IEM_MC_FETCH_MEM_FLAT_U16_DISP' ),
        'IEM_MC_FETCH_MEM_I16':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_I16' ),
        'IEM_MC_FETCH_MEM_U32':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_U32' ),
        'IEM_MC_FETCH_MEM_U32_DISP':              (  1, 'IEM_MC_FETCH_MEM_FLAT_U32_DISP' ),
        'IEM_MC_FETCH_MEM_I32':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_I32' ),
        'IEM_MC_FETCH_MEM_U64':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_U64' ),
        'IEM_MC_FETCH_MEM_U64_DISP':              (  1, 'IEM_MC_FETCH_MEM_FLAT_U64_DISP' ),
        'IEM_MC_FETCH_MEM_U64_ALIGN_U128':        (  1, 'IEM_MC_FETCH_MEM_FLAT_U64_ALIGN_U128' ),
        'IEM_MC_FETCH_MEM_I64':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_I64' ),
        'IEM_MC_FETCH_MEM_R32':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_R32' ),
        'IEM_MC_FETCH_MEM_R64':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_R64' ),
        'IEM_MC_FETCH_MEM_R80':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_R80' ),
        'IEM_MC_FETCH_MEM_D80':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_D80' ),
        'IEM_MC_FETCH_MEM_U128':                  (  1, 'IEM_MC_FETCH_MEM_FLAT_U128' ),
        'IEM_MC_FETCH_MEM_U128_NO_AC':            (  1, 'IEM_MC_FETCH_MEM_FLAT_U128_NO_AC' ),
        'IEM_MC_FETCH_MEM_U128_ALIGN_SSE':        (  1, 'IEM_MC_FETCH_MEM_FLAT_U128_ALIGN_SSE' ),
        'IEM_MC_FETCH_MEM_XMM':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_XMM' ),
        'IEM_MC_FETCH_MEM_XMM_NO_AC':             (  1, 'IEM_MC_FETCH_MEM_FLAT_XMM_NO_AC' ),
        'IEM_MC_FETCH_MEM_XMM_ALIGN_SSE':         (  1, 'IEM_MC_FETCH_MEM_FLAT_XMM_ALIGN_SSE' ),
        'IEM_MC_FETCH_MEM_XMM_U32':               (  2, 'IEM_MC_FETCH_MEM_FLAT_XMM_U32' ),
        'IEM_MC_FETCH_MEM_XMM_U64':               (  2, 'IEM_MC_FETCH_MEM_FLAT_XMM_U64' ),
        'IEM_MC_FETCH_MEM_U256':                  (  1, 'IEM_MC_FETCH_MEM_FLAT_U256' ),
        'IEM_MC_FETCH_MEM_U256_NO_AC':            (  1, 'IEM_MC_FETCH_MEM_FLAT_U256_NO_AC' ),
        'IEM_MC_FETCH_MEM_U256_ALIGN_AVX':        (  1, 'IEM_MC_FETCH_MEM_FLAT_U256_ALIGN_AVX' ),
        'IEM_MC_FETCH_MEM_YMM':                   (  1, 'IEM_MC_FETCH_MEM_FLAT_YMM' ),
        'IEM_MC_FETCH_MEM_YMM_NO_AC':             (  1, 'IEM_MC_FETCH_MEM_FLAT_YMM_NO_AC' ),
        'IEM_MC_FETCH_MEM_YMM_ALIGN_AVX':         (  1, 'IEM_MC_FETCH_MEM_FLAT_YMM_ALIGN_AVX' ),
        'IEM_MC_FETCH_MEM_U8_ZX_U16':             (  1, 'IEM_MC_FETCH_MEM_FLAT_U8_ZX_U16' ),
        'IEM_MC_FETCH_MEM_U8_ZX_U32':             (  1, 'IEM_MC_FETCH_MEM_FLAT_U8_ZX_U32' ),
        'IEM_MC_FETCH_MEM_U8_ZX_U64':             (  1, 'IEM_MC_FETCH_MEM_FLAT_U8_ZX_U64' ),
        'IEM_MC_FETCH_MEM_U16_ZX_U32':            (  1, 'IEM_MC_FETCH_MEM_FLAT_U16_ZX_U32' ),
        'IEM_MC_FETCH_MEM_U16_ZX_U64':            (  1, 'IEM_MC_FETCH_MEM_FLAT_U16_ZX_U64' ),
        'IEM_MC_FETCH_MEM_U32_ZX_U64':            (  1, 'IEM_MC_FETCH_MEM_FLAT_U32_ZX_U64' ),
        'IEM_MC_FETCH_MEM_U8_SX_U16':             (  1, 'IEM_MC_FETCH_MEM_FLAT_U8_SX_U16' ),
        'IEM_MC_FETCH_MEM_U8_SX_U32':             (  1, 'IEM_MC_FETCH_MEM_FLAT_U8_SX_U32' ),
        'IEM_MC_FETCH_MEM_U8_SX_U64':             (  1, 'IEM_MC_FETCH_MEM_FLAT_U8_SX_U64' ),
        'IEM_MC_FETCH_MEM_U16_SX_U32':            (  1, 'IEM_MC_FETCH_MEM_FLAT_U16_SX_U32' ),
        'IEM_MC_FETCH_MEM_U16_SX_U64':            (  1, 'IEM_MC_FETCH_MEM_FLAT_U16_SX_U64' ),
        'IEM_MC_FETCH_MEM_U32_SX_U64':            (  1, 'IEM_MC_FETCH_MEM_FLAT_U32_SX_U64' ),
        'IEM_MC_STORE_MEM_U8':                    (  0, 'IEM_MC_STORE_MEM_FLAT_U8' ),
        'IEM_MC_STORE_MEM_U16':                   (  0, 'IEM_MC_STORE_MEM_FLAT_U16' ),
        'IEM_MC_STORE_MEM_U32':                   (  0, 'IEM_MC_STORE_MEM_FLAT_U32' ),
        'IEM_MC_STORE_MEM_U64':                   (  0, 'IEM_MC_STORE_MEM_FLAT_U64' ),
        'IEM_MC_STORE_MEM_U8_CONST':              (  0, 'IEM_MC_STORE_MEM_FLAT_U8_CONST' ),
        'IEM_MC_STORE_MEM_U16_CONST':             (  0, 'IEM_MC_STORE_MEM_FLAT_U16_CONST' ),
        'IEM_MC_STORE_MEM_U32_CONST':             (  0, 'IEM_MC_STORE_MEM_FLAT_U32_CONST' ),
        'IEM_MC_STORE_MEM_U64_CONST':             (  0, 'IEM_MC_STORE_MEM_FLAT_U64_CONST' ),
        'IEM_MC_STORE_MEM_U128':                  (  0, 'IEM_MC_STORE_MEM_FLAT_U128' ),
        'IEM_MC_STORE_MEM_U128_ALIGN_SSE':        (  0, 'IEM_MC_STORE_MEM_FLAT_U128_ALIGN_SSE' ),
        'IEM_MC_STORE_MEM_U256':                  (  0, 'IEM_MC_STORE_MEM_FLAT_U256' ),
        'IEM_MC_STORE_MEM_U256_ALIGN_AVX':        (  0, 'IEM_MC_STORE_MEM_FLAT_U256_ALIGN_AVX' ),
        'IEM_MC_MEM_MAP':                         (  2, 'IEM_MC_MEM_FLAT_MAP' ),
        'IEM_MC_MEM_MAP_U8_RW':                   (  2, 'IEM_MC_MEM_FLAT_MAP_U8_RW' ),
        'IEM_MC_MEM_MAP_U8_RO':                   (  2, 'IEM_MC_MEM_FLAT_MAP_U8_RO' ),
        'IEM_MC_MEM_MAP_U8_WO':                   (  2, 'IEM_MC_MEM_FLAT_MAP_U8_WO' ),
        'IEM_MC_MEM_MAP_U16_RW':                  (  2, 'IEM_MC_MEM_FLAT_MAP_U16_RW' ),
        'IEM_MC_MEM_MAP_U16_RO':                  (  2, 'IEM_MC_MEM_FLAT_MAP_U16_RO' ),
        'IEM_MC_MEM_MAP_U16_WO':                  (  2, 'IEM_MC_MEM_FLAT_MAP_U16_WO' ),
        'IEM_MC_MEM_MAP_U32_RW':                  (  2, 'IEM_MC_MEM_FLAT_MAP_U32_RW' ),
        'IEM_MC_MEM_MAP_U32_RO':                  (  2, 'IEM_MC_MEM_FLAT_MAP_U32_RO' ),
        'IEM_MC_MEM_MAP_U32_WO':                  (  2, 'IEM_MC_MEM_FLAT_MAP_U32_WO' ),
        'IEM_MC_MEM_MAP_U64_RW':                  (  2, 'IEM_MC_MEM_FLAT_MAP_U64_RW' ),
        'IEM_MC_MEM_MAP_U64_RO':                  (  2, 'IEM_MC_MEM_FLAT_MAP_U64_RO' ),
        'IEM_MC_MEM_MAP_U64_WO':                  (  2, 'IEM_MC_MEM_FLAT_MAP_U64_WO' ),
        'IEM_MC_MEM_MAP_EX':                      (  3, 'IEM_MC_MEM_FLAT_MAP_EX' ),
    };

    kdMemMcToFlatInfoStack = {
        'IEM_MC_PUSH_U16':                        (  'IEM_MC_FLAT32_PUSH_U16',      'IEM_MC_FLAT64_PUSH_U16', ),
        'IEM_MC_PUSH_U32':                        (  'IEM_MC_FLAT32_PUSH_U32',      'IEM_MC_PUSH_U32', ),
        'IEM_MC_PUSH_U64':                        (  'IEM_MC_PUSH_U64',             'IEM_MC_FLAT64_PUSH_U64', ),
        'IEM_MC_PUSH_U32_SREG':                   (  'IEM_MC_FLAT32_PUSH_U32_SREG', 'IEM_MC_PUSH_U32_SREG' ),
        'IEM_MC_POP_U16':                         (  'IEM_MC_FLAT32_POP_U16',       'IEM_MC_FLAT64_POP_U16', ),
        'IEM_MC_POP_U32':                         (  'IEM_MC_FLAT32_POP_U32',       'IEM_MC_POP_U32', ),
        'IEM_MC_POP_U64':                         (  'IEM_MC_POP_U64',              'IEM_MC_FLAT64_POP_U64', ),
    };

    def analyzeMorphStmtForThreaded(self, aoStmts, iParamRef = 0):
        """
        Transforms (copy) the statements into those for the threaded function.

        Returns list/tree of statements (aoStmts is not modified) and the new
        iParamRef value.
        """
        #
        # We'll be traversing aoParamRefs in parallel to the statements, so we
        # must match the traversal in analyzeFindThreadedParamRefs exactly.
        #
        #print('McBlock at %s:%s' % (os.path.split(self.oMcBlock.sSrcFile)[1], self.oMcBlock.iBeginLine,));
        aoThreadedStmts = [];
        for oStmt in aoStmts:
            # Skip C++ statements that is purely related to decoding.
            if not oStmt.isCppStmt() or not oStmt.fDecode:
                # Copy the statement. Make a deep copy to make sure we've got our own
                # copies of all instance variables, even if a bit overkill at the moment.
                oNewStmt = copy.deepcopy(oStmt);
                aoThreadedStmts.append(oNewStmt);
                #print('oNewStmt %s %s' % (oNewStmt.sName, len(oNewStmt.asParams),));

                # If the statement has parameter references, process the relevant parameters.
                # We grab the references relevant to this statement and apply them in reserve order.
                if iParamRef < len(self.aoParamRefs) and self.aoParamRefs[iParamRef].oStmt == oStmt:
                    iParamRefFirst = iParamRef;
                    while True:
                        iParamRef += 1;
                        if iParamRef >= len(self.aoParamRefs) or self.aoParamRefs[iParamRef].oStmt != oStmt:
                            break;

                    #print('iParamRefFirst=%s iParamRef=%s' % (iParamRefFirst, iParamRef));
                    for iCurRef in range(iParamRef - 1, iParamRefFirst - 1, -1):
                        oCurRef = self.aoParamRefs[iCurRef];
                        if oCurRef.iParam is not None:
                            assert oCurRef.oStmt == oStmt;
                            #print('iCurRef=%s iParam=%s sOrgRef=%s' % (iCurRef, oCurRef.iParam, oCurRef.sOrgRef));
                            sSrcParam = oNewStmt.asParams[oCurRef.iParam];
                            assert (   sSrcParam[oCurRef.offParam : oCurRef.offParam + len(oCurRef.sOrgRef)] == oCurRef.sOrgRef
                                    or oCurRef.fCustomRef), \
                                   'offParam=%s sOrgRef=%s iParam=%s oStmt.sName=%s sSrcParam=%s<eos>' \
                                   % (oCurRef.offParam, oCurRef.sOrgRef, oCurRef.iParam, oStmt.sName, sSrcParam);
                            oNewStmt.asParams[oCurRef.iParam] = sSrcParam[0 : oCurRef.offParam] \
                                                              + oCurRef.sNewName \
                                                              + sSrcParam[oCurRef.offParam + len(oCurRef.sOrgRef) : ];

                # Morph IEM_MC_CALC_RM_EFF_ADDR into IEM_MC_CALC_RM_EFF_ADDR_THREADED ...
                if oNewStmt.sName == 'IEM_MC_CALC_RM_EFF_ADDR':
                    assert self.sVariation != self.ksVariation_Default;
                    oNewStmt.sName = 'IEM_MC_CALC_RM_EFF_ADDR_THREADED' + self.sVariation.upper();
                    assert len(oNewStmt.asParams) == 3;

                    if self.sVariation in (self.ksVariation_16, self.ksVariation_16_Pre386, self.ksVariation_32_Addr16):
                        oNewStmt.asParams = [
                            oNewStmt.asParams[0], oNewStmt.asParams[1], self.dParamRefs['u16Disp'][0].sNewName,
                        ];
                    else:
                        sSibAndMore = self.dParamRefs['bSib'][0].sNewName; # Merge bSib and 2nd part of cbImmAndRspOffset.
                        if oStmt.asParams[2] not in ('0', '1', '2', '4'):
                            sSibAndMore = '(%s) | ((%s) & 0x0f00)' % (self.dParamRefs['bSib'][0].sNewName, oStmt.asParams[2]);

                        if self.sVariation in (self.ksVariation_32, self.ksVariation_32_Flat, self.ksVariation_16_Addr32):
                            oNewStmt.asParams = [
                                oNewStmt.asParams[0], oNewStmt.asParams[1], sSibAndMore, self.dParamRefs['u32Disp'][0].sNewName,
                            ];
                        else:
                            oNewStmt.asParams = [
                                oNewStmt.asParams[0], self.dParamRefs['bRmEx'][0].sNewName, sSibAndMore,
                                self.dParamRefs['u32Disp'][0].sNewName, self.dParamRefs['cbInstr'][0].sNewName,
                            ];
                # ... and IEM_MC_ADVANCE_RIP_AND_FINISH into *_THREADED and maybe *_LM64/_NOT64 ...
                elif oNewStmt.sName in ('IEM_MC_ADVANCE_RIP_AND_FINISH', 'IEM_MC_REL_JMP_S8_AND_FINISH',
                                        'IEM_MC_REL_JMP_S16_AND_FINISH', 'IEM_MC_REL_JMP_S32_AND_FINISH'):
                    oNewStmt.asParams.append(self.dParamRefs['cbInstr'][0].sNewName);
                    if (    oNewStmt.sName in ('IEM_MC_REL_JMP_S8_AND_FINISH', )
                        and self.sVariation != self.ksVariation_16_Pre386):
                        oNewStmt.asParams.append(self.dParamRefs['pVCpu->iem.s.enmEffOpSize'][0].sNewName);
                    oNewStmt.sName += '_THREADED';
                    if self.sVariation in (self.ksVariation_64, self.ksVariation_64_FsGs, self.ksVariation_64_Addr32):
                        oNewStmt.sName += '_PC64';
                    elif self.sVariation == self.ksVariation_16_Pre386:
                        oNewStmt.sName += '_PC16';
                    elif self.sVariation != self.ksVariation_Default:
                        oNewStmt.sName += '_PC32';

                # ... and IEM_MC_*_GREG_U8 into *_THREADED w/ reworked index taking REX into account
                elif oNewStmt.sName.startswith('IEM_MC_') and oNewStmt.sName.find('_GREG_U8') > 0:
                    (idxReg, _, sStdRef) = self.analyze8BitGRegStmt(oStmt); # Don't use oNewStmt as it has been modified!
                    oNewStmt.asParams[idxReg] = self.dParamRefs[sStdRef][0].sNewName;
                    oNewStmt.sName += '_THREADED';

                # ... and IEM_MC_CALL_CIMPL_[0-5] and IEM_MC_DEFER_TO_CIMPL_[0-5]_RET into *_THREADED ...
                elif oNewStmt.sName.startswith('IEM_MC_CALL_CIMPL_') or oNewStmt.sName.startswith('IEM_MC_DEFER_TO_CIMPL_'):
                    oNewStmt.sName += '_THREADED';
                    oNewStmt.asParams.insert(0, self.dParamRefs['cbInstr'][0].sNewName);

                # ... and in FLAT modes we must morph memory access into FLAT accesses ...
                elif (    self.sVariation in (self.ksVariation_64, self.ksVariation_32_Flat,)
                      and (   oNewStmt.sName.startswith('IEM_MC_FETCH_MEM')
                           or (oNewStmt.sName.startswith('IEM_MC_STORE_MEM_') and oNewStmt.sName.find('_BY_REF') < 0)
                           or oNewStmt.sName.startswith('IEM_MC_MEM_MAP') )):
                    idxEffSeg = self.kdMemMcToFlatInfo[oNewStmt.sName][0];
                    if idxEffSeg != -1:
                        if (    oNewStmt.asParams[idxEffSeg].find('iEffSeg') < 0
                            and oNewStmt.asParams[idxEffSeg] not in ('X86_SREG_ES', ) ):
                            self.raiseProblem('Expected iEffSeg as param #%d to %s: %s'
                                              % (idxEffSeg + 1, oNewStmt.sName, oNewStmt.asParams[idxEffSeg],));
                        oNewStmt.asParams.pop(idxEffSeg);
                    oNewStmt.sName = self.kdMemMcToFlatInfo[oNewStmt.sName][1];

                # ... PUSH and POP also needs flat variants, but these differ a little.
                elif (    self.sVariation in (self.ksVariation_64, self.ksVariation_32_Flat,)
                      and (   (oNewStmt.sName.startswith('IEM_MC_PUSH') and oNewStmt.sName.find('_FPU') < 0)
                           or oNewStmt.sName.startswith('IEM_MC_POP'))):
                    oNewStmt.sName = self.kdMemMcToFlatInfoStack[oNewStmt.sName][int(self.sVariation == self.ksVariation_64)];


                # Process branches of conditionals recursively.
                if isinstance(oStmt, iai.McStmtCond):
                    (oNewStmt.aoIfBranch, iParamRef) = self.analyzeMorphStmtForThreaded(oStmt.aoIfBranch,   iParamRef);
                    if oStmt.aoElseBranch:
                        (oNewStmt.aoElseBranch, iParamRef) = self.analyzeMorphStmtForThreaded(oStmt.aoElseBranch, iParamRef);

        return (aoThreadedStmts, iParamRef);


    def analyzeCodeOperation(self, aoStmts, fSeenConditional = False):
        """
        Analyzes the code looking clues as to additional side-effects.

        Currently this is simply looking for any IEM_IMPL_C_F_XXX flags and
        collecting these in self.dsCImplFlags.
        """
        for oStmt in aoStmts:
            # Pick up hints from CIMPL calls and deferals.
            if oStmt.sName.startswith('IEM_MC_CALL_CIMPL_') or oStmt.sName.startswith('IEM_MC_DEFER_TO_CIMPL_'):
                sFlagsSansComments = iai.McBlock.stripComments(oStmt.asParams[0]);
                for sFlag in sFlagsSansComments.split('|'):
                    sFlag = sFlag.strip();
                    if sFlag != '0':
                        if sFlag in self.kdCImplFlags:
                            self.dsCImplFlags[sFlag] = True;
                        elif sFlag == 'IEM_CIMPL_F_CHECK_IRQ_BEFORE_AND_AFTER':
                            self.dsCImplFlags['IEM_CIMPL_F_CHECK_IRQ_BEFORE'] = True;
                            self.dsCImplFlags['IEM_CIMPL_F_CHECK_IRQ_AFTER']  = True;
                        else:
                            self.raiseProblem('Unknown CIMPL flag value: %s' % (sFlag,));

            # Set IEM_IMPL_C_F_BRANCH if we see any branching MCs.
            elif oStmt.sName.startswith('IEM_MC_SET_RIP'):
                assert not fSeenConditional;
                self.dsCImplFlags['IEM_CIMPL_F_BRANCH_INDIRECT'] = True;
            elif oStmt.sName.startswith('IEM_MC_REL_JMP'):
                self.dsCImplFlags['IEM_CIMPL_F_BRANCH_RELATIVE'] = True;
                if fSeenConditional:
                    self.dsCImplFlags['IEM_CIMPL_F_BRANCH_CONDITIONAL'] = True;

            # Process branches of conditionals recursively.
            if isinstance(oStmt, iai.McStmtCond):
                self.analyzeCodeOperation(oStmt.aoIfBranch, True);
                if oStmt.aoElseBranch:
                    self.analyzeCodeOperation(oStmt.aoElseBranch, True);

        return True;


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
            if aoRefs[0].sType[0] != 'P':
                cBits = g_kdTypeInfo[aoRefs[0].sType][0];
                assert(cBits <= 64);
            else:
                cBits = 64;

            if cBits not in dBySize:
                dBySize[cBits] = [sStdRef,]
            else:
                dBySize[cBits].append(sStdRef);

        # Pack the parameters as best as we can, starting with the largest ones
        # and ASSUMING a 64-bit parameter size.
        self.cMinParams = 0;
        offNewParam     = 0;
        for cBits in sorted(dBySize.keys(), reverse = True):
            for sStdRef in dBySize[cBits]:
                if offNewParam == 0 or offNewParam + cBits > 64:
                    self.cMinParams += 1;
                    offNewParam      = cBits;
                else:
                    offNewParam     += cBits;
                assert(offNewParam <= 64);

                for oRef in self.dParamRefs[sStdRef]:
                    oRef.iNewParam   = self.cMinParams - 1;
                    oRef.offNewParam = offNewParam - cBits;

        # Currently there are a few that requires 4 parameters, list these so we can figure out why:
        if self.cMinParams >= 4:
            print('debug: cMinParams=%s cRawParams=%s - %s:%d'
                  % (self.cMinParams, len(self.dParamRefs), self.oParent.oMcBlock.sSrcFile, self.oParent.oMcBlock.iBeginLine,));

        return True;

    ksHexDigits = '0123456789abcdefABCDEF';

    def analyzeFindThreadedParamRefs(self, aoStmts): # pylint: disable=too-many-statements
        """
        Scans the statements for things that have to passed on to the threaded
        function (populates self.aoParamRefs).
        """
        for oStmt in aoStmts:
            # Some statements we can skip alltogether.
            if isinstance(oStmt, iai.McCppPreProc):
                continue;
            if oStmt.isCppStmt() and oStmt.fDecode:
                continue;
            if oStmt.sName in ('IEM_MC_BEGIN',):
                continue;

            if isinstance(oStmt, iai.McStmtVar):
                if oStmt.sConstValue is None:
                    continue;
                aiSkipParams = { 0: True, 1: True, 3: True };
            else:
                aiSkipParams = {};

            # Several statements have implicit parameters and some have different parameters.
            if oStmt.sName in ('IEM_MC_ADVANCE_RIP_AND_FINISH', 'IEM_MC_REL_JMP_S8_AND_FINISH', 'IEM_MC_REL_JMP_S16_AND_FINISH',
                               'IEM_MC_REL_JMP_S32_AND_FINISH', 'IEM_MC_CALL_CIMPL_0', 'IEM_MC_CALL_CIMPL_1',
                               'IEM_MC_CALL_CIMPL_2', 'IEM_MC_CALL_CIMPL_3', 'IEM_MC_CALL_CIMPL_4', 'IEM_MC_CALL_CIMPL_5',
                               'IEM_MC_DEFER_TO_CIMPL_0_RET', 'IEM_MC_DEFER_TO_CIMPL_1_RET', 'IEM_MC_DEFER_TO_CIMPL_2_RET',
                               'IEM_MC_DEFER_TO_CIMPL_3_RET', 'IEM_MC_DEFER_TO_CIMPL_4_RET', 'IEM_MC_DEFER_TO_CIMPL_5_RET', ):
                self.aoParamRefs.append(ThreadedParamRef('IEM_GET_INSTR_LEN(pVCpu)', 'uint4_t', oStmt, sStdRef = 'cbInstr'));

            if (    oStmt.sName in ('IEM_MC_REL_JMP_S8_AND_FINISH',)
                and self.sVariation != self.ksVariation_16_Pre386):
                self.aoParamRefs.append(ThreadedParamRef('pVCpu->iem.s.enmEffOpSize', 'IEMMODE', oStmt));

            if oStmt.sName == 'IEM_MC_CALC_RM_EFF_ADDR':
                # This is being pretty presumptive about bRm always being the RM byte...
                assert len(oStmt.asParams) == 3;
                assert oStmt.asParams[1] == 'bRm';

                if self.sVariation in (self.ksVariation_16, self.ksVariation_16_Pre386, self.ksVariation_32_Addr16):
                    self.aoParamRefs.append(ThreadedParamRef('bRm',     'uint8_t',  oStmt));
                    self.aoParamRefs.append(ThreadedParamRef('(uint16_t)uEffAddrInfo' ,
                                                             'uint16_t', oStmt, sStdRef = 'u16Disp'));
                elif self.sVariation in (self.ksVariation_32, self.ksVariation_32_Flat, self.ksVariation_16_Addr32):
                    self.aoParamRefs.append(ThreadedParamRef('bRm',     'uint8_t',  oStmt));
                    self.aoParamRefs.append(ThreadedParamRef('(uint8_t)(uEffAddrInfo >> 32)',
                                                             'uint8_t',  oStmt, sStdRef = 'bSib'));
                    self.aoParamRefs.append(ThreadedParamRef('(uint32_t)uEffAddrInfo',
                                                             'uint32_t', oStmt, sStdRef = 'u32Disp'));
                else:
                    assert self.sVariation in (self.ksVariation_64, self.ksVariation_64_FsGs, self.ksVariation_64_Addr32);
                    self.aoParamRefs.append(ThreadedParamRef('IEM_GET_MODRM_EX(pVCpu, bRm)',
                                                             'uint8_t',  oStmt, sStdRef = 'bRmEx'));
                    self.aoParamRefs.append(ThreadedParamRef('(uint8_t)(uEffAddrInfo >> 32)',
                                                             'uint8_t',  oStmt, sStdRef = 'bSib'));
                    self.aoParamRefs.append(ThreadedParamRef('(uint32_t)uEffAddrInfo',
                                                             'uint32_t', oStmt, sStdRef = 'u32Disp'));
                    self.aoParamRefs.append(ThreadedParamRef('IEM_GET_INSTR_LEN(pVCpu)',
                                                             'uint4_t',  oStmt, sStdRef = 'cbInstr'));
                    aiSkipParams[1] = True; # Skip the bRm parameter as it is being replaced by bRmEx.

            # 8-bit register accesses needs to have their index argument reworked to take REX into account.
            if oStmt.sName.startswith('IEM_MC_') and oStmt.sName.find('_GREG_U8') > 0:
                (idxReg, sOrgRef, sStdRef) = self.analyze8BitGRegStmt(oStmt);
                self.aoParamRefs.append(ThreadedParamRef(sOrgRef, 'uint16_t', oStmt, idxReg, sStdRef = sStdRef));
                aiSkipParams[idxReg] = True; # Skip the parameter below.

            # If in flat mode variation, ignore the effective segment parameter to memory MCs.
            if (    self.sVariation in (self.ksVariation_64, self.ksVariation_32_Flat,)
                and oStmt.sName in self.kdMemMcToFlatInfo
                and self.kdMemMcToFlatInfo[oStmt.sName][0] != -1):
                aiSkipParams[self.kdMemMcToFlatInfo[oStmt.sName][0]] = True;

            # Inspect the target of calls to see if we need to pass down a
            # function pointer or function table pointer for it to work.
            if isinstance(oStmt, iai.McStmtCall):
                if oStmt.sFn[0] == 'p':
                    self.aoParamRefs.append(ThreadedParamRef(oStmt.sFn, self.analyzeCallToType(oStmt.sFn), oStmt, oStmt.idxFn));
                elif (    oStmt.sFn[0] != 'i'
                      and not oStmt.sFn.startswith('IEMTARGETCPU_EFL_BEHAVIOR_SELECT')
                      and not oStmt.sFn.startswith('IEM_SELECT_HOST_OR_FALLBACK') ):
                    self.raiseProblem('Bogus function name in %s: %s' % (oStmt.sName, oStmt.sFn,));
                aiSkipParams[oStmt.idxFn] = True;

                # Skip the hint parameter (first) for IEM_MC_CALL_CIMPL_X.
                if oStmt.sName.startswith('IEM_MC_CALL_CIMPL_'):
                    assert oStmt.idxFn == 1;
                    aiSkipParams[0] = True;


            # Check all the parameters for bogus references.
            for iParam, sParam in enumerate(oStmt.asParams):
                if iParam not in aiSkipParams  and  sParam not in self.oParent.dVariables:
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
                                offParam = offCloseParam + 1;
                                self.aoParamRefs.append(ThreadedParamRef(sParam[offStart : offParam], 'uint8_t',
                                                                         oStmt, iParam, offStart));

                            # We can skip known variables.
                            elif sRef in self.oParent.dVariables:
                                pass;

                            # Skip certain macro invocations.
                            elif sRef in ('IEM_GET_HOST_CPU_FEATURES',
                                          'IEM_GET_GUEST_CPU_FEATURES',
                                          'IEM_IS_GUEST_CPU_AMD',
                                          'IEM_IS_16BIT_CODE',
                                          'IEM_IS_32BIT_CODE',
                                          'IEM_IS_64BIT_CODE',
                                          ):
                                offParam = iai.McBlock.skipSpacesAt(sParam, offParam, len(sParam));
                                if sParam[offParam] != '(':
                                    self.raiseProblem('Expected "(" following %s in "%s"' % (sRef, oStmt.renderCode(),));
                                (asMacroParams, offCloseParam) = iai.McBlock.extractParams(sParam, offParam);
                                if asMacroParams is None:
                                    self.raiseProblem('Unable to find ")" for %s in "%s"' % (sRef, oStmt.renderCode(),));
                                offParam = offCloseParam + 1;

                                # Skip any dereference following it, unless it's a predicate like IEM_IS_GUEST_CPU_AMD.
                                if sRef not in ('IEM_IS_GUEST_CPU_AMD',
                                                'IEM_IS_16BIT_CODE',
                                                'IEM_IS_32BIT_CODE',
                                                'IEM_IS_64BIT_CODE',
                                                ):
                                    offParam = iai.McBlock.skipSpacesAt(sParam, offParam, len(sParam));
                                    if offParam + 2 <= len(sParam) and sParam[offParam : offParam + 2] == '->':
                                        offParam = iai.McBlock.skipSpacesAt(sParam, offParam + 2, len(sParam));
                                        while offParam < len(sParam) and (sParam[offParam].isalnum() or sParam[offParam] in '_.'):
                                            offParam += 1;

                            # Skip constants, globals, types (casts), sizeof and macros.
                            elif (   sRef.startswith('IEM_OP_PRF_')
                                  or sRef.startswith('IEM_ACCESS_')
                                  or sRef.startswith('IEMINT_')
                                  or sRef.startswith('X86_GREG_')
                                  or sRef.startswith('X86_SREG_')
                                  or sRef.startswith('X86_EFL_')
                                  or sRef.startswith('X86_FSW_')
                                  or sRef.startswith('X86_FCW_')
                                  or sRef.startswith('X86_XCPT_')
                                  or sRef.startswith('IEMMODE_')
                                  or sRef.startswith('IEM_F_')
                                  or sRef.startswith('IEM_CIMPL_F_')
                                  or sRef.startswith('g_')
                                  or sRef.startswith('iemAImpl_')
                                  or sRef in ( 'int8_t',    'int16_t',    'int32_t',
                                               'INT8_C',    'INT16_C',    'INT32_C',    'INT64_C',
                                               'UINT8_C',   'UINT16_C',   'UINT32_C',   'UINT64_C',
                                               'UINT8_MAX', 'UINT16_MAX', 'UINT32_MAX', 'UINT64_MAX',
                                               'INT8_MAX',  'INT16_MAX',  'INT32_MAX',  'INT64_MAX',
                                               'INT8_MIN',  'INT16_MIN',  'INT32_MIN',  'INT64_MIN',
                                               'sizeof',    'NOREF',      'RT_NOREF',   'IEMMODE_64BIT',
                                               'RT_BIT_32', 'true',       'false',      'NIL_RTGCPTR',) ):
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
                        # Comment?
                        elif (    ch == '/'
                              and offParam + 4 <= len(sParam)
                              and sParam[offParam + 1] == '*'):
                            offParam += 2;
                            offNext = sParam.find('*/', offParam);
                            if offNext < offParam:
                                self.raiseProblem('Unable to find "*/" in "%s" ("%s")' % (sRef, oStmt.renderCode(),));
                            offParam = offNext + 2;
                        # Whatever else.
                        else:
                            offParam += 1;

            # Traverse the branches of conditionals.
            if isinstance(oStmt, iai.McStmtCond):
                self.analyzeFindThreadedParamRefs(oStmt.aoIfBranch);
                self.analyzeFindThreadedParamRefs(oStmt.aoElseBranch);
        return True;

    def analyzeVariation(self, aoStmts):
        """
        2nd part of the analysis, done on each variation.

        The variations may differ in parameter requirements and will end up with
        slightly different MC sequences. Thus this is done on each individually.

        Returns dummy True - raises exception on trouble.
        """
        # Now scan the code for variables and field references that needs to
        # be passed to the threaded function because they are related to the
        # instruction decoding.
        self.analyzeFindThreadedParamRefs(aoStmts);
        self.analyzeConsolidateThreadedParamRefs();

        # Scan the code for IEM_CIMPL_F_ and other clues.
        self.analyzeCodeOperation(aoStmts);

        # Morph the statement stream for the block into what we'll be using in the threaded function.
        (self.aoStmtsForThreadedFunction, iParamRef) = self.analyzeMorphStmtForThreaded(aoStmts);
        if iParamRef != len(self.aoParamRefs):
            raise Exception('iParamRef=%s, expected %s!' % (iParamRef, len(self.aoParamRefs),));

        return True;

    def emitThreadedCallStmts(self, cchIndent, sCallVarNm = None):
        """
        Produces generic C++ statments that emits a call to the thread function
        variation and any subsequent checks that may be necessary after that.

        The sCallVarNm is for emitting
        """
        aoStmts = [
            iai.McCppCall('IEM_MC2_BEGIN_EMIT_CALLS', ['1' if 'IEM_CIMPL_F_CHECK_IRQ_BEFORE' in self.dsCImplFlags else '0'],
                          cchIndent = cchIndent), # Scope and a hook for various stuff.
        ];

        # The call to the threaded function.
        asCallArgs = [ self.getIndexName() if not sCallVarNm else sCallVarNm, ];
        for iParam in range(self.cMinParams):
            asFrags = [];
            for aoRefs in self.dParamRefs.values():
                oRef = aoRefs[0];
                if oRef.iNewParam == iParam:
                    sCast = '(uint64_t)'
                    if oRef.sType in ('int8_t', 'int16_t', 'int32_t'): # Make sure these doesn't get sign-extended.
                        sCast = '(uint64_t)(u' + oRef.sType + ')';
                    if oRef.offNewParam == 0:
                        asFrags.append(sCast + '(' + oRef.sOrgRef + ')');
                    else:
                        asFrags.append('(%s(%s) << %s)' % (sCast, oRef.sOrgRef, oRef.offNewParam));
            assert asFrags;
            asCallArgs.append(' | '.join(asFrags));

        aoStmts.append(iai.McCppCall('IEM_MC2_EMIT_CALL_%s' % (len(asCallArgs) - 1,), asCallArgs, cchIndent = cchIndent));

        # For CIMPL stuff, we need to consult the associated IEM_CIMPL_F_XXX
        # mask and maybe emit additional checks.
        if (   'IEM_CIMPL_F_MODE'   in self.dsCImplFlags
            or 'IEM_CIMPL_F_XCPT'   in self.dsCImplFlags
            or 'IEM_CIMPL_F_VMEXIT' in self.dsCImplFlags):
            aoStmts.append(iai.McCppCall('IEM_MC2_EMIT_CALL_1', ( 'kIemThreadedFunc_BltIn_CheckMode', 'pVCpu->iem.s.fExec', ),
                                         cchIndent = cchIndent));

        sCImplFlags = ' | '.join(self.dsCImplFlags.keys());
        if not sCImplFlags:
            sCImplFlags = '0'
        aoStmts.append(iai.McCppCall('IEM_MC2_END_EMIT_CALLS', ( sCImplFlags, ), cchIndent = cchIndent)); # For closing the scope.

        # Emit fEndTb = true or fTbBranched = true if any of the CIMPL flags
        # indicates we should do so.
        # Note! iemThreadedRecompilerMcDeferToCImpl0 duplicates work done here.
        asEndTbFlags      = [];
        asTbBranchedFlags = [];
        for sFlag in self.dsCImplFlags:
            if self.kdCImplFlags[sFlag] is True:
                asEndTbFlags.append(sFlag);
            elif sFlag.startswith('IEM_CIMPL_F_BRANCH_'):
                asTbBranchedFlags.append(sFlag);
        if asTbBranchedFlags:
            aoStmts.append(iai.McCppGeneric('iemThreadedSetBranched(pVCpu, %s);'
                                            % ((' | '.join(asTbBranchedFlags)).replace('IEM_CIMPL_F_BRANCH', 'IEMBRANCHED_F'),),
                                            cchIndent = cchIndent)); # Inline fn saves ~2 seconds for gcc 13/dbg (1m13s vs 1m15s).
        if asEndTbFlags:
            aoStmts.append(iai.McCppGeneric('pVCpu->iem.s.fEndTb = true; /* %s */' % (','.join(asEndTbFlags),),
                                            cchIndent = cchIndent));

        if 'IEM_CIMPL_F_CHECK_IRQ_AFTER' in self.dsCImplFlags:
            aoStmts.append(iai.McCppGeneric('pVCpu->iem.s.cInstrTillIrqCheck = 0;', cchIndent = cchIndent));

        return aoStmts;


class ThreadedFunction(object):
    """
    A threaded function.
    """

    def __init__(self, oMcBlock):
        self.oMcBlock       = oMcBlock      # type: IEMAllInstPython.McBlock
        ## Variations for this block. There is at least one.
        self.aoVariations   = []            # type: list(ThreadedFunctionVariation)
        ## Variation dictionary containing the same as aoVariations.
        self.dVariations    = {}            # type: dict(str,ThreadedFunctionVariation)
        ## Dictionary of local variables (IEM_MC_LOCAL[_CONST]) and call arguments (IEM_MC_ARG*).
        self.dVariables     = {}            # type: dict(str,McStmtVar)

    @staticmethod
    def dummyInstance():
        """ Gets a dummy instance. """
        return ThreadedFunction(iai.McBlock('null', 999999999, 999999999,
                                            iai.DecoderFunction('null', 999999999, 'nil', ('','')), 999999999));

    def raiseProblem(self, sMessage):
        """ Raises a problem. """
        raise Exception('%s:%s: error: %s' % (self.oMcBlock.sSrcFile, self.oMcBlock.iBeginLine, sMessage, ));

    def warning(self, sMessage):
        """ Emits a warning. """
        print('%s:%s: warning: %s' % (self.oMcBlock.sSrcFile, self.oMcBlock.iBeginLine, sMessage, ));

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

        Returns dummy True - raises exception on trouble.
        """

        # Check the block for errors before we proceed (will decode it).
        asErrors = self.oMcBlock.check();
        if asErrors:
            raise Exception('\n'.join(['%s:%s: error: %s' % (self.oMcBlock.sSrcFile, self.oMcBlock.iBeginLine, sError, )
                                       for sError in asErrors]));

        # Decode the block into a list/tree of McStmt objects.
        aoStmts = self.oMcBlock.decode();

        # Scan the statements for local variables and call arguments (self.dVariables).
        self.analyzeFindVariablesAndCallArgs(aoStmts);

        # Create variations as needed.
        if iai.McStmt.findStmtByNames(aoStmts,
                                      { 'IEM_MC_DEFER_TO_CIMPL_0_RET': True,
                                        'IEM_MC_DEFER_TO_CIMPL_1_RET': True,
                                        'IEM_MC_DEFER_TO_CIMPL_2_RET': True,
                                        'IEM_MC_DEFER_TO_CIMPL_3_RET': True, }):
            asVariations = [ThreadedFunctionVariation.ksVariation_Default,];

        elif iai.McStmt.findStmtByNames(aoStmts, {'IEM_MC_CALC_RM_EFF_ADDR' : True,}):
            if 'IEM_MC_F_64BIT' in self.oMcBlock.dMcFlags:
                asVariations = ThreadedFunctionVariation.kasVariationsWithAddressOnly64;
            elif 'IEM_MC_F_NOT_64BIT' in self.oMcBlock.dMcFlags and 'IEM_MC_F_NOT_286_OR_OLDER' in self.oMcBlock.dMcFlags:
                asVariations = ThreadedFunctionVariation.kasVariationsWithAddressNot286Not64;
            elif 'IEM_MC_F_NOT_286_OR_OLDER' in self.oMcBlock.dMcFlags:
                asVariations = ThreadedFunctionVariation.kasVariationsWithAddressNot286;
            elif 'IEM_MC_F_NOT_64BIT' in self.oMcBlock.dMcFlags:
                asVariations = ThreadedFunctionVariation.kasVariationsWithAddressNot64;
            elif 'IEM_MC_F_ONLY_8086' in self.oMcBlock.dMcFlags:
                asVariations = [ThreadedFunctionVariation.ksVariation_16_Pre386,];
            else:
                asVariations = ThreadedFunctionVariation.kasVariationsWithAddress;
        else:
            if 'IEM_MC_F_64BIT' in self.oMcBlock.dMcFlags:
                asVariations = ThreadedFunctionVariation.kasVariationsWithoutAddressOnly64;
            elif 'IEM_MC_F_NOT_64BIT' in self.oMcBlock.dMcFlags and 'IEM_MC_F_NOT_286_OR_OLDER' in self.oMcBlock.dMcFlags:
                asVariations = ThreadedFunctionVariation.kasVariationsWithoutAddressNot286Not64;
            elif 'IEM_MC_F_NOT_286_OR_OLDER' in self.oMcBlock.dMcFlags:
                asVariations = ThreadedFunctionVariation.kasVariationsWithoutAddressNot286;
            elif 'IEM_MC_F_NOT_64BIT' in self.oMcBlock.dMcFlags:
                asVariations = ThreadedFunctionVariation.kasVariationsWithoutAddressNot64;
            elif 'IEM_MC_F_ONLY_8086' in self.oMcBlock.dMcFlags:
                asVariations = [ThreadedFunctionVariation.ksVariation_16_Pre386,];
            else:
                asVariations = ThreadedFunctionVariation.kasVariationsWithoutAddress;

        self.aoVariations = [ThreadedFunctionVariation(self, sVar) for sVar in asVariations];

        # Dictionary variant of the list.
        self.dVariations = { oVar.sVariation: oVar for oVar in self.aoVariations };

        # Continue the analysis on each variation.
        for oVariation in self.aoVariations:
            oVariation.analyzeVariation(aoStmts);

        return True;

    def emitThreadedCallStmts(self):
        """
        Worker for morphInputCode that returns a list of statements that emits
        the call to the threaded functions for the block.
        """
        # Special case for only default variation:
        if len(self.aoVariations) == 1  and  self.aoVariations[0].sVariation == ThreadedFunctionVariation.ksVariation_Default:
            return self.aoVariations[0].emitThreadedCallStmts(0);

        #
        # Case statement sub-class.
        #
        dByVari = self.dVariations;
        #fDbg = self.oMcBlock.sFunction == 'iemOpCommonPushSReg';
        class Case:
            def __init__(self, sCond, sVarNm = None):
                self.sCond  = sCond;
                self.sVarNm = sVarNm;
                self.oVar   = dByVari[sVarNm] if sVarNm else None;
                self.aoBody = self.oVar.emitThreadedCallStmts(8) if sVarNm else None;

            def toCode(self):
                aoStmts = [ iai.McCppGeneric('case %s:' % (self.sCond), cchIndent = 4), ];
                if self.aoBody:
                    aoStmts.extend(self.aoBody);
                    aoStmts.append(iai.McCppGeneric('break;', cchIndent = 8));
                return aoStmts;

            def toFunctionAssignment(self):
                aoStmts = [ iai.McCppGeneric('case %s:' % (self.sCond), cchIndent = 4), ];
                if self.aoBody:
                    aoStmts.extend([
                        iai.McCppGeneric('enmFunction = %s;' % (self.oVar.getIndexName(),), cchIndent = 8),
                        iai.McCppGeneric('break;', cchIndent = 8),
                    ]);
                return aoStmts;

            def isSame(self, oThat):
                if not self.aoBody:                 # fall thru always matches.
                    return True;
                if len(self.aoBody) != len(oThat.aoBody):
                    #if fDbg: print('dbg: body len diff: %s vs %s' % (len(self.aoBody), len(oThat.aoBody),));
                    return False;
                for iStmt, oStmt in enumerate(self.aoBody):
                    oThatStmt = oThat.aoBody[iStmt] # type: iai.McStmt
                    assert isinstance(oStmt, iai.McCppGeneric);
                    assert not isinstance(oStmt, iai.McStmtCond);
                    if isinstance(oStmt, iai.McStmtCond):
                        return False;
                    if oStmt.sName != oThatStmt.sName:
                        #if fDbg: print('dbg: stmt #%s name: %s vs %s' % (iStmt, oStmt.sName, oThatStmt.sName,));
                        return False;
                    if len(oStmt.asParams) != len(oThatStmt.asParams):
                        #if fDbg: print('dbg: stmt #%s param count: %s vs %s'
                        #               % (iStmt, len(oStmt.asParams), len(oThatStmt.asParams),));
                        return False;
                    for iParam, sParam in enumerate(oStmt.asParams):
                        if (    sParam != oThatStmt.asParams[iParam]
                            and (   iParam != 1
                                 or not isinstance(oStmt, iai.McCppCall)
                                 or not oStmt.asParams[0].startswith('IEM_MC2_EMIT_CALL_')
                                 or sParam != self.oVar.getIndexName()
                                 or oThatStmt.asParams[iParam] != oThat.oVar.getIndexName() )):
                            #if fDbg: print('dbg: stmt #%s, param #%s: %s vs %s'
                            #               % (iStmt, iParam, sParam, oThatStmt.asParams[iParam],));
                            return False;
                return True;

        #
        # Determine what we're switch on.
        # This ASSUMES that (IEM_F_MODE_X86_FLAT_OR_PRE_386_MASK | IEM_F_MODE_CPUMODE_MASK) == 7!
        #
        fSimple = True;
        sSwitchValue  = '(pVCpu->iem.s.fExec & (IEM_F_MODE_CPUMODE_MASK | IEM_F_MODE_X86_FLAT_OR_PRE_386_MASK))';
        if (   ThrdFnVar.ksVariation_64_Addr32 in dByVari
            or ThrdFnVar.ksVariation_64_FsGs   in dByVari
            or ThrdFnVar.ksVariation_32_Addr16 in dByVari
            or ThrdFnVar.ksVariation_32_Flat   in dByVari
            or ThrdFnVar.ksVariation_16_Addr32 in dByVari):
            sSwitchValue += ' | (pVCpu->iem.s.enmEffAddrMode == (pVCpu->iem.s.fExec & IEM_F_MODE_CPUMODE_MASK) ? 0 : 8)';
            # Accesses via FS and GS and CS goes thru non-FLAT functions. (CS
            # is not writable in 32-bit mode (at least), thus the penalty mode
            # for any accesses via it (simpler this way).)
            sSwitchValue += ' | (pVCpu->iem.s.iEffSeg < X86_SREG_FS && pVCpu->iem.s.iEffSeg != X86_SREG_CS ? 0 : 16)';
            fSimple       = False;                                              # threaded functions.

        #
        # Generate the case statements.
        #
        # pylintx: disable=x
        aoCases = [];
        if ThreadedFunctionVariation.ksVariation_64_Addr32 in dByVari:
            assert not fSimple;
            aoCases.extend([
                Case('IEMMODE_64BIT',       ThrdFnVar.ksVariation_64),
                Case('IEMMODE_64BIT | 16',  ThrdFnVar.ksVariation_64_FsGs),
                Case('IEMMODE_64BIT | 8 | 16', None), # fall thru
                Case('IEMMODE_64BIT | 8',   ThrdFnVar.ksVariation_64_Addr32),
            ]);
        elif ThrdFnVar.ksVariation_64 in dByVari:
            assert fSimple;
            aoCases.append(Case('IEMMODE_64BIT', ThrdFnVar.ksVariation_64));

        if ThrdFnVar.ksVariation_32_Addr16 in dByVari:
            assert not fSimple;
            aoCases.extend([
                Case('IEMMODE_32BIT | IEM_F_MODE_X86_FLAT_OR_PRE_386_MASK',         ThrdFnVar.ksVariation_32_Flat),
                Case('IEMMODE_32BIT | IEM_F_MODE_X86_FLAT_OR_PRE_386_MASK | 16',    None), # fall thru
                Case('IEMMODE_32BIT | 16',                                          None), # fall thru
                Case('IEMMODE_32BIT',                                               ThrdFnVar.ksVariation_32),
                Case('IEMMODE_32BIT | IEM_F_MODE_X86_FLAT_OR_PRE_386_MASK | 8',     None), # fall thru
                Case('IEMMODE_32BIT | IEM_F_MODE_X86_FLAT_OR_PRE_386_MASK | 8 | 16',None), # fall thru
                Case('IEMMODE_32BIT                                       | 8 | 16',None), # fall thru
                Case('IEMMODE_32BIT                                       | 8',     ThrdFnVar.ksVariation_32_Addr16),
            ]);
        elif ThrdFnVar.ksVariation_32 in dByVari:
            assert fSimple;
            aoCases.extend([
                Case('IEMMODE_32BIT | IEM_F_MODE_X86_FLAT_OR_PRE_386_MASK', None), # fall thru
                Case('IEMMODE_32BIT',                                       ThrdFnVar.ksVariation_32),
            ]);

        if ThrdFnVar.ksVariation_16_Addr32 in dByVari:
            assert not fSimple;
            aoCases.extend([
                Case('IEMMODE_16BIT | 16',      None), # fall thru
                Case('IEMMODE_16BIT',           ThrdFnVar.ksVariation_16),
                Case('IEMMODE_16BIT | 8 | 16',  None), # fall thru
                Case('IEMMODE_16BIT | 8',       ThrdFnVar.ksVariation_16_Addr32),
            ]);
        elif ThrdFnVar.ksVariation_16 in dByVari:
            assert fSimple;
            aoCases.append(Case('IEMMODE_16BIT', ThrdFnVar.ksVariation_16));

        if ThrdFnVar.ksVariation_16_Pre386 in dByVari:
            aoCases.append(Case('IEMMODE_16BIT | IEM_F_MODE_X86_FLAT_OR_PRE_386_MASK', ThrdFnVar.ksVariation_16_Pre386));

        #
        # If the case bodies are all the same, except for the function called,
        # we can reduce the code size and hopefully compile time.
        #
        iFirstCaseWithBody = 0;
        while not aoCases[iFirstCaseWithBody].aoBody:
            iFirstCaseWithBody += 1
        fAllSameCases = True
        for iCase in range(iFirstCaseWithBody + 1, len(aoCases)):
            fAllSameCases = fAllSameCases and aoCases[iCase].isSame(aoCases[iFirstCaseWithBody]);
        #if fDbg: print('fAllSameCases=%s %s' % (fAllSameCases, self.oMcBlock.sFunction,));
        if fAllSameCases:
            aoStmts = [
                iai.McCppGeneric('IEMTHREADEDFUNCS enmFunction;'),
                iai.McCppGeneric('switch (%s)' % (sSwitchValue,)),
                iai.McCppGeneric('{'),
            ];
            for oCase in aoCases:
                aoStmts.extend(oCase.toFunctionAssignment());
            aoStmts.extend([
                iai.McCppGeneric('IEM_NOT_REACHED_DEFAULT_CASE_RET();', cchIndent = 4),
                iai.McCppGeneric('}'),
            ]);
            aoStmts.extend(dByVari[aoCases[iFirstCaseWithBody].sVarNm].emitThreadedCallStmts(0, 'enmFunction'));

        else:
            #
            # Generate the generic switch statement.
            #
            aoStmts = [
                iai.McCppGeneric('switch (%s)' % (sSwitchValue,)),
                iai.McCppGeneric('{'),
            ];
            for oCase in aoCases:
                aoStmts.extend(oCase.toCode());
            aoStmts.extend([
                iai.McCppGeneric('IEM_NOT_REACHED_DEFAULT_CASE_RET();', cchIndent = 4),
                iai.McCppGeneric('}'),
            ]);

        return aoStmts;

    def morphInputCode(self, aoStmts, fCallEmitted = False, cDepth = 0):
        """
        Adjusts (& copies) the statements for the input/decoder so it will emit
        calls to the right threaded functions for each block.

        Returns list/tree of statements (aoStmts is not modified) and updated
        fCallEmitted status.
        """
        #print('McBlock at %s:%s' % (os.path.split(self.oMcBlock.sSrcFile)[1], self.oMcBlock.iBeginLine,));
        aoDecoderStmts = [];

        for oStmt in aoStmts:
            # Copy the statement. Make a deep copy to make sure we've got our own
            # copies of all instance variables, even if a bit overkill at the moment.
            oNewStmt = copy.deepcopy(oStmt);
            aoDecoderStmts.append(oNewStmt);
            #print('oNewStmt %s %s' % (oNewStmt.sName, len(oNewStmt.asParams),));

            # If we haven't emitted the threaded function call yet, look for
            # statements which it would naturally follow or preceed.
            if not fCallEmitted:
                if not oStmt.isCppStmt():
                    if (   oStmt.sName.startswith('IEM_MC_MAYBE_RAISE_') \
                        or (oStmt.sName.endswith('_AND_FINISH') and oStmt.sName.startswith('IEM_MC_'))
                        or oStmt.sName.startswith('IEM_MC_CALL_CIMPL_')
                        or oStmt.sName.startswith('IEM_MC_DEFER_TO_CIMPL_')
                        or oStmt.sName in ('IEM_MC_RAISE_DIVIDE_ERROR',)):
                        aoDecoderStmts.pop();
                        aoDecoderStmts.extend(self.emitThreadedCallStmts());
                        aoDecoderStmts.append(oNewStmt);
                        fCallEmitted = True;
                elif (    oStmt.fDecode
                      and (   oStmt.asParams[0].find('IEMOP_HLP_DONE_') >= 0
                           or oStmt.asParams[0].find('IEMOP_HLP_DECODED_') >= 0)):
                    aoDecoderStmts.extend(self.emitThreadedCallStmts());
                    fCallEmitted = True;

            # Process branches of conditionals recursively.
            if isinstance(oStmt, iai.McStmtCond):
                (oNewStmt.aoIfBranch, fCallEmitted1) = self.morphInputCode(oStmt.aoIfBranch, fCallEmitted, cDepth + 1);
                if oStmt.aoElseBranch:
                    (oNewStmt.aoElseBranch, fCallEmitted2) = self.morphInputCode(oStmt.aoElseBranch, fCallEmitted, cDepth + 1);
                else:
                    fCallEmitted2 = False;
                fCallEmitted = fCallEmitted or (fCallEmitted1 and fCallEmitted2);

        if not fCallEmitted and cDepth == 0:
            self.raiseProblem('Unable to insert call to threaded function.');

        return (aoDecoderStmts, fCallEmitted);


    def generateInputCode(self):
        """
        Modifies the input code.
        """
        cchIndent = (self.oMcBlock.cchIndent + 3) // 4 * 4;

        if len(self.oMcBlock.aoStmts) == 1:
            # IEM_MC_DEFER_TO_CIMPL_X_RET - need to wrap in {} to make it safe to insert into random code.
            sCode = iai.McStmt.renderCodeForList(self.morphInputCode(self.oMcBlock.aoStmts)[0],
                                                 cchIndent = cchIndent).replace('\n', ' /* gen */\n', 1);
            sCode = ' ' * (min(cchIndent, 2) - 2) + '{\n' \
                  + sCode \
                  + ' ' * (min(cchIndent, 2) - 2) + '}\n';
            return sCode;

        # IEM_MC_BEGIN/END block
        assert len(self.oMcBlock.asLines) > 2, "asLines=%s" % (self.oMcBlock.asLines,);
        return iai.McStmt.renderCodeForList(self.morphInputCode(self.oMcBlock.aoStmts)[0],
                                            cchIndent = cchIndent).replace('\n', ' /* gen */\n', 1);

# Short alias for ThreadedFunctionVariation.
ThrdFnVar = ThreadedFunctionVariation;


class IEMThreadedGenerator(object):
    """
    The threaded code generator & annotator.
    """

    def __init__(self):
        self.aoThreadedFuncs = []       # type: list(ThreadedFunction)
        self.oOptions        = None     # type: argparse.Namespace
        self.aoParsers       = []       # type: list(IEMAllInstPython.SimpleParser)
        self.aidxFirstFunctions = []    # type: list(int) ##< Runs parallel to aoParser giving the index of the first function.

    #
    # Processing.
    #

    def processInputFiles(self, sNativeRecompilerArch = None):
        """
        Process the input files.
        """

        # Parse the files.
        self.aoParsers = iai.parseFiles(self.oOptions.asInFiles);

        # Create threaded functions for the MC blocks.
        self.aoThreadedFuncs = [ThreadedFunction(oMcBlock) for oMcBlock in iai.g_aoMcBlocks];

        # Analyze the threaded functions.
        dRawParamCounts = {};
        dMinParamCounts = {};
        for oThreadedFunction in self.aoThreadedFuncs:
            oThreadedFunction.analyze();
            for oVariation in oThreadedFunction.aoVariations:
                dRawParamCounts[len(oVariation.dParamRefs)] = dRawParamCounts.get(len(oVariation.dParamRefs), 0) + 1;
                dMinParamCounts[oVariation.cMinParams]      = dMinParamCounts.get(oVariation.cMinParams,      0) + 1;
        print('debug: param count distribution, raw and optimized:', file = sys.stderr);
        for cCount in sorted({cBits: True for cBits in list(dRawParamCounts.keys()) + list(dMinParamCounts.keys())}.keys()):
            print('debug:     %s params: %4s raw, %4s min'
                  % (cCount, dRawParamCounts.get(cCount, 0), dMinParamCounts.get(cCount, 0)),
                  file = sys.stderr);

        # Populate aidxFirstFunctions.  This is ASSUMING that
        # g_aoMcBlocks/self.aoThreadedFuncs are in self.aoParsers order.
        iThreadedFunction       = 0;
        oThreadedFunction       = self.getThreadedFunctionByIndex(0);
        self.aidxFirstFunctions = [];
        for oParser in self.aoParsers:
            self.aidxFirstFunctions.append(iThreadedFunction);

            while oThreadedFunction.oMcBlock.sSrcFile == oParser.sSrcFile:
                iThreadedFunction += 1;
                oThreadedFunction  = self.getThreadedFunctionByIndex(iThreadedFunction);

        # Analyze the threaded functions and their variations for native recompilation.
        if sNativeRecompilerArch:
            cTotal  = 0;
            cNative = 0;
            for oThreadedFunction in self.aoThreadedFuncs:
                for oVariation in oThreadedFunction.aoVariations:
                    cTotal += 1;
                    oVariation.oNativeRecomp = ian.analyzeVariantForNativeRecomp(oVariation, sNativeRecompilerArch);
                    if oVariation.oNativeRecomp and oVariation.oNativeRecomp.isRecompilable():
                        cNative += 1;
            print('debug: %.1f%% / %u out of %u threaded function variations are recompilable'
                  % (cNative * 100.0 / cTotal, cNative, cTotal));

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

    ## List of built-in threaded functions with user argument counts and
    ## whether it has a native recompiler implementation.
    katBltIns = (
        ( 'DeferToCImpl0',                                      2, True  ),
        ( 'CheckIrq',                                           0, False ),
        ( 'CheckMode',                                          1, False ),
        ( 'CheckHwInstrBps',                                    0, False ),
        ( 'CheckCsLim',                                         1, False ),

        ( 'CheckCsLimAndOpcodes',                               3, False ),
        ( 'CheckOpcodes',                                       3, False ),
        ( 'CheckOpcodesConsiderCsLim',                          3, False ),

        ( 'CheckCsLimAndPcAndOpcodes',                          3, False ),
        ( 'CheckPcAndOpcodes',                                  3, False ),
        ( 'CheckPcAndOpcodesConsiderCsLim',                     3, False ),

        ( 'CheckCsLimAndOpcodesAcrossPageLoadingTlb',           3, False ),
        ( 'CheckOpcodesAcrossPageLoadingTlb',                   3, False ),
        ( 'CheckOpcodesAcrossPageLoadingTlbConsiderCsLim',      2, False ),

        ( 'CheckCsLimAndOpcodesLoadingTlb',                     3, False ),
        ( 'CheckOpcodesLoadingTlb',                             3, False ),
        ( 'CheckOpcodesLoadingTlbConsiderCsLim',                3, False ),

        ( 'CheckCsLimAndOpcodesOnNextPageLoadingTlb',           2, False ),
        ( 'CheckOpcodesOnNextPageLoadingTlb',                   2, False ),
        ( 'CheckOpcodesOnNextPageLoadingTlbConsiderCsLim',      2, False ),

        ( 'CheckCsLimAndOpcodesOnNewPageLoadingTlb',            2, False ),
        ( 'CheckOpcodesOnNewPageLoadingTlb',                    2, False ),
        ( 'CheckOpcodesOnNewPageLoadingTlbConsiderCsLim',       2, False ),
    );

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
            '',
            '    /*',
            '     * Predefined',
            '     */',
        ];
        asLines += ['    kIemThreadedFunc_BltIn_%s,' % (sFuncNm,) for sFuncNm, _, _ in self.katBltIns];

        iThreadedFunction = 1 + len(self.katBltIns);
        for sVariation in ThreadedFunctionVariation.kasVariationsEmitOrder:
            asLines += [
                    '',
                    '    /*',
                    '     * Variation: ' + ThreadedFunctionVariation.kdVariationNames[sVariation] + '',
                    '     */',
            ];
            for oThreadedFunction in self.aoThreadedFuncs:
                oVariation = oThreadedFunction.dVariations.get(sVariation, None);
                if oVariation:
                    iThreadedFunction += 1;
                    oVariation.iEnumValue = iThreadedFunction;
                    asLines.append('    ' + oVariation.getIndexName() + ',');
        asLines += [
            '    kIemThreadedFunc_End',
            '} IEMTHREADEDFUNCS;',
            '',
        ];

        # Prototype the function table.
        asLines += [
            'extern const PFNIEMTHREADEDFUNC g_apfnIemThreadedFunctions[kIemThreadedFunc_End];',
            '#if defined(IN_RING3) || defined(LOG_ENABLED)',
            'extern const char * const       g_apszIemThreadedFunctions[kIemThreadedFunc_End];',
            '#endif',
            'extern uint8_t const            g_acIemThreadedFunctionUsedArgs[kIemThreadedFunc_End];',
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

    def generateFunctionParameterUnpacking(self, oVariation, oOut, asParams):
        """
        Outputs code for unpacking parameters.
        This is shared by the threaded and native code generators.
        """
        aasVars = [];
        for aoRefs in oVariation.dParamRefs.values():
            oRef  = aoRefs[0];
            if oRef.sType[0] != 'P':
                cBits = g_kdTypeInfo[oRef.sType][0];
                sType = g_kdTypeInfo[oRef.sType][2];
            else:
                cBits = 64;
                sType = oRef.sType;

            sTypeDecl = sType + ' const';

            if cBits == 64:
                assert oRef.offNewParam == 0;
                if sType == 'uint64_t':
                    sUnpack = '%s;' % (asParams[oRef.iNewParam],);
                else:
                    sUnpack = '(%s)%s;' % (sType, asParams[oRef.iNewParam],);
            elif oRef.offNewParam == 0:
                sUnpack = '(%s)(%s & %s);' % (sType, asParams[oRef.iNewParam], self.ksBitsToIntMask[cBits]);
            else:
                sUnpack = '(%s)((%s >> %s) & %s);' \
                        % (sType, asParams[oRef.iNewParam], oRef.offNewParam, self.ksBitsToIntMask[cBits]);

            sComment = '/* %s - %s ref%s */' % (oRef.sOrgRef, len(aoRefs), 's' if len(aoRefs) != 1 else '',);

            aasVars.append([ '%s:%02u' % (oRef.iNewParam, oRef.offNewParam),
                             sTypeDecl, oRef.sNewName, sUnpack, sComment ]);
        acchVars = [0, 0, 0, 0, 0];
        for asVar in aasVars:
            for iCol, sStr in enumerate(asVar):
                acchVars[iCol] = max(acchVars[iCol], len(sStr));
        sFmt = '    %%-%ss %%-%ss = %%-%ss %%s\n' % (acchVars[1], acchVars[2], acchVars[3]);
        for asVar in sorted(aasVars):
            oOut.write(sFmt % (asVar[1], asVar[2], asVar[3], asVar[4],));
        return True;

    kasThreadedParamNames = ('uParam0', 'uParam1', 'uParam2');
    def generateThreadedFunctionsSource(self, oOut):
        """
        Generates the threaded functions source file.
        Returns success indicator.
        """

        asLines = self.generateLicenseHeader();
        oOut.write('\n'.join(asLines));

        #
        # Emit the function definitions.
        #
        for sVariation in ThreadedFunctionVariation.kasVariationsEmitOrder:
            sVarName = ThreadedFunctionVariation.kdVariationNames[sVariation];
            oOut.write(  '\n'
                       + '\n'
                       + '\n'
                       + '\n'
                       + '/*' + '*' * 128 + '\n'
                       + '*   Variation: ' + sVarName + ' ' * (129 - len(sVarName) - 15) + '*\n'
                       + '*' * 128 + '*/\n');

            for oThreadedFunction in self.aoThreadedFuncs:
                oVariation = oThreadedFunction.dVariations.get(sVariation, None);
                if oVariation:
                    oMcBlock = oThreadedFunction.oMcBlock;

                    # Function header
                    oOut.write(  '\n'
                               + '\n'
                               + '/**\n'
                               + ' * #%u: %s at line %s offset %s in %s%s\n'
                                  % (oVariation.iEnumValue, oMcBlock.sFunction, oMcBlock.iBeginLine, oMcBlock.offBeginLine,
                                     os.path.split(oMcBlock.sSrcFile)[1],
                                     ' (macro expansion)' if oMcBlock.iBeginLine == oMcBlock.iEndLine else '')
                               + ' */\n'
                               + 'static IEM_DECL_IEMTHREADEDFUNC_DEF(' + oVariation.getThreadedFunctionName() + ')\n'
                               + '{\n');

                    # Unpack parameters.
                    self.generateFunctionParameterUnpacking(oVariation, oOut, self.kasThreadedParamNames);

                    # RT_NOREF for unused parameters.
                    if oVariation.cMinParams < g_kcThreadedParams:
                        oOut.write('    RT_NOREF(' + ', '.join(self.kasThreadedParamNames[oVariation.cMinParams:]) + ');\n');

                    # Now for the actual statements.
                    oOut.write(iai.McStmt.renderCodeForList(oVariation.aoStmtsForThreadedFunction, cchIndent = 4));

                    oOut.write('}\n');


        #
        # Generate the output tables in parallel.
        #
        asFuncTable = [
            '/**',
            ' * Function pointer table.',
            ' */',
            'PFNIEMTHREADEDFUNC const g_apfnIemThreadedFunctions[kIemThreadedFunc_End] =',
            '{',
            '    /*Invalid*/ NULL,',
        ];
        asNameTable = [
            '/**',
            ' * Function name table.',
            ' */',
            'const char * const g_apszIemThreadedFunctions[kIemThreadedFunc_End] =',
            '{',
            '    "Invalid",',
        ];
        asArgCntTab = [
            '/**',
            ' * Argument count table.',
            ' */',
            'uint8_t const g_acIemThreadedFunctionUsedArgs[kIemThreadedFunc_End] =',
            '{',
            '    0, /*Invalid*/',
        ];
        aasTables = (asFuncTable, asNameTable, asArgCntTab,);

        for asTable in aasTables:
            asTable.extend((
                '',
                '    /*',
                '     * Predefined.',
                '     */',
            ));
        for sFuncNm, cArgs, _ in self.katBltIns:
            asFuncTable.append('    iemThreadedFunc_BltIn_%s,' % (sFuncNm,));
            asNameTable.append('    "BltIn_%s",' % (sFuncNm,));
            asArgCntTab.append('    %d, /*BltIn_%s*/' % (cArgs, sFuncNm,));

        iThreadedFunction = 1 + len(self.katBltIns);
        for sVariation in ThreadedFunctionVariation.kasVariationsEmitOrder:
            for asTable in aasTables:
                asTable.extend((
                    '',
                    '    /*',
                    '     * Variation: ' + ThreadedFunctionVariation.kdVariationNames[sVariation],
                    '     */',
                ));
            for oThreadedFunction in self.aoThreadedFuncs:
                oVariation = oThreadedFunction.dVariations.get(sVariation, None);
                if oVariation:
                    iThreadedFunction += 1;
                    assert oVariation.iEnumValue == iThreadedFunction;
                    sName = oVariation.getThreadedFunctionName();
                    asFuncTable.append('    /*%4u*/ %s,' % (iThreadedFunction, sName,));
                    asNameTable.append('    /*%4u*/ "%s",' % (iThreadedFunction, sName,));
                    asArgCntTab.append('    /*%4u*/ %d, /*%s*/' % (iThreadedFunction, oVariation.cMinParams, sName,));

        for asTable in aasTables:
            asTable.append('};');

        #
        # Output the tables.
        #
        oOut.write(  '\n'
                   + '\n');
        oOut.write('\n'.join(asFuncTable));
        oOut.write(  '\n'
                   + '\n'
                   + '\n'
                   + '#if defined(IN_RING3) || defined(LOG_ENABLED)\n');
        oOut.write('\n'.join(asNameTable));
        oOut.write(  '\n'
                   + '#endif /* IN_RING3 || LOG_ENABLED */\n'
                   + '\n'
                   + '\n');
        oOut.write('\n'.join(asArgCntTab));
        oOut.write('\n');

        return True;

    def generateNativeFunctionsHeader(self, oOut):
        """
        Generates the native recompiler functions header file.
        Returns success indicator.
        """
        if not self.oOptions.sNativeRecompilerArch:
            return True;

        asLines = self.generateLicenseHeader();

        # Prototype the function table.
        asLines += [
            'extern const PFNIEMNATIVERECOMPFUNC g_apfnIemNativeRecompileFunctions[kIemThreadedFunc_End];',
            '',
        ];

        oOut.write('\n'.join(asLines));
        return True;

    def generateNativeFunctionsSource(self, oOut):
        """
        Generates the native recompiler functions source file.
        Returns success indicator.
        """
        if not self.oOptions.sNativeRecompilerArch:
            return True;

        #
        # The file header.
        #
        oOut.write('\n'.join(self.generateLicenseHeader()));

        #
        # Emit the functions.
        #
        for sVariation in ThreadedFunctionVariation.kasVariationsEmitOrder:
            sVarName = ThreadedFunctionVariation.kdVariationNames[sVariation];
            oOut.write(  '\n'
                       + '\n'
                       + '\n'
                       + '\n'
                       + '/*' + '*' * 128 + '\n'
                       + '*   Variation: ' + sVarName + ' ' * (129 - len(sVarName) - 15) + '*\n'
                       + '*' * 128 + '*/\n');

            for oThreadedFunction in self.aoThreadedFuncs:
                oVariation = oThreadedFunction.dVariations.get(sVariation, None) # type: ThreadedFunctionVariation
                if oVariation and oVariation.oNativeRecomp and oVariation.oNativeRecomp.isRecompilable():
                    oMcBlock = oThreadedFunction.oMcBlock;

                    # Function header
                    oOut.write(  '\n'
                               + '\n'
                               + '/**\n'
                               + ' * #%u: %s at line %s offset %s in %s%s\n'
                                  % (oVariation.iEnumValue, oMcBlock.sFunction, oMcBlock.iBeginLine, oMcBlock.offBeginLine,
                                     os.path.split(oMcBlock.sSrcFile)[1],
                                     ' (macro expansion)' if oMcBlock.iBeginLine == oMcBlock.iEndLine else '')
                               + ' */\n'
                               + 'static IEM_DECL_IEMNATIVERECOMPFUNC_DEF(' + oVariation.getNativeFunctionName() + ')\n'
                               + '{\n');

                    # Unpack parameters.
                    self.generateFunctionParameterUnpacking(oVariation, oOut,
                                                            ('pCallEntry->auParams[0]',
                                                             'pCallEntry->auParams[1]',
                                                             'pCallEntry->auParams[2]',));

                    # Now for the actual statements.
                    oOut.write(oVariation.oNativeRecomp.renderCode(cchIndent = 4));

                    oOut.write('}\n');

        #
        # Output the function table.
        #
        oOut.write(   '\n'
                    + '\n'
                    + '/*\n'
                    + ' * Function table running parallel to g_apfnIemThreadedFunctions and friends.\n'
                    + ' */\n'
                    + 'const PFNIEMNATIVERECOMPFUNC g_apfnIemNativeRecompileFunctions[kIemThreadedFunc_End] =\n'
                    + '{\n'
                    + '    /*Invalid*/ NULL,'
                    + '\n'
                    + '    /*\n'
                    + '     * Predefined.\n'
                    + '     */\n'
                    );
        for sFuncNm, _, fHaveRecompFunc in self.katBltIns:
            if fHaveRecompFunc:
                oOut.write('    iemNativeRecompFunc_BltIn_%s,\n' % (sFuncNm,))
            else:
                oOut.write('    NULL, /*BltIn_%s*/\n' % (sFuncNm,))

        iThreadedFunction = 1 + len(self.katBltIns);
        for sVariation in ThreadedFunctionVariation.kasVariationsEmitOrder:
            oOut.write(  '    /*\n'
                       + '     * Variation: ' + ThreadedFunctionVariation.kdVariationNames[sVariation] + '\n'
                       + '     */\n');
            for oThreadedFunction in self.aoThreadedFuncs:
                oVariation = oThreadedFunction.dVariations.get(sVariation, None);
                if oVariation:
                    iThreadedFunction += 1;
                    assert oVariation.iEnumValue == iThreadedFunction;
                    sName = oVariation.getNativeFunctionName();
                    if oVariation.oNativeRecomp and oVariation.oNativeRecomp.isRecompilable():
                        oOut.write('    /*%4u*/ %s,\n' % (iThreadedFunction, sName,));
                    else:
                        oOut.write('    /*%4u*/ NULL /*%s*/,\n' % (iThreadedFunction, sName,));

        oOut.write(  '};\n'
                   + '\n');
        return True;


    def getThreadedFunctionByIndex(self, idx):
        """
        Returns a ThreadedFunction object for the given index.  If the index is
        out of bounds, a dummy is returned.
        """
        if idx < len(self.aoThreadedFuncs):
            return self.aoThreadedFuncs[idx];
        return ThreadedFunction.dummyInstance();

    def generateModifiedInput(self, oOut, idxFile):
        """
        Generates the combined modified input source/header file.
        Returns success indicator.
        """
        #
        # File header and assert assumptions.
        #
        oOut.write('\n'.join(self.generateLicenseHeader()));
        oOut.write('AssertCompile((IEM_F_MODE_X86_FLAT_OR_PRE_386_MASK | IEM_F_MODE_CPUMODE_MASK) == 7);\n');

        #
        # Iterate all parsers (input files) and output the ones related to the
        # file set given by idxFile.
        #
        for idxParser, oParser in enumerate(self.aoParsers): # type: int, IEMAllInstPython.SimpleParser
            # Is this included in the file set?
            sSrcBaseFile = os.path.basename(oParser.sSrcFile).lower();
            fInclude     = -1;
            for aoInfo in iai.g_aaoAllInstrFilesAndDefaultMapAndSet:
                if sSrcBaseFile == aoInfo[0].lower():
                    fInclude = aoInfo[2] in (-1, idxFile);
                    break;
            if fInclude is not True:
                assert fInclude is False;
                continue;

            # Output it.
            oOut.write("\n\n/* ****** BEGIN %s ******* */\n" % (oParser.sSrcFile,));

            iThreadedFunction = self.aidxFirstFunctions[idxParser];
            oThreadedFunction = self.getThreadedFunctionByIndex(iThreadedFunction);
            iLine             = 0;
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
                    assert sLine.count('IEM_MC_') - sLine.count('IEM_MC_F_') == 1, 'sLine="%s"' % (sLine,);
                    oOut.write(sLine[:oThreadedFunction.oMcBlock.offBeginLine]);
                    sModified = oThreadedFunction.generateInputCode().strip();
                    oOut.write(sModified);

                    iLine = oThreadedFunction.oMcBlock.iEndLine;
                    sLine = oParser.asLines[iLine - 1];
                    assert sLine.count('IEM_MC_') - sLine.count('IEM_MC_F_') == 1 or len(oThreadedFunction.oMcBlock.aoStmts) == 1;
                    oOut.write(sLine[oThreadedFunction.oMcBlock.offAfterEnd : ]);

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
                        assert (   sModified.startswith('IEM_MC_BEGIN')
                                or (sModified.find('IEM_MC_DEFER_TO_CIMPL_') > 0 and sModified.strip().startswith('{\n'))
                                or sModified.startswith('pVCpu->iem.s.fEndTb = true')
                                ), 'sModified="%s"' % (sModified,);
                        oOut.write(sModified);

                        offLine = oThreadedFunction.oMcBlock.offAfterEnd;

                        # Advance
                        iThreadedFunction += 1;
                        oThreadedFunction  = self.getThreadedFunctionByIndex(iThreadedFunction);

                    # Last line segment.
                    if offLine < len(sLine):
                        oOut.write(sLine[offLine : ]);

            oOut.write("/* ****** END %s ******* */\n" % (oParser.sSrcFile,));

        return True;

    def generateModifiedInput1(self, oOut):
        """
        Generates the combined modified input source/header file, part 1.
        Returns success indicator.
        """
        return self.generateModifiedInput(oOut, 1);

    def generateModifiedInput2(self, oOut):
        """
        Generates the combined modified input source/header file, part 2.
        Returns success indicator.
        """
        return self.generateModifiedInput(oOut, 2);

    def generateModifiedInput3(self, oOut):
        """
        Generates the combined modified input source/header file, part 3.
        Returns success indicator.
        """
        return self.generateModifiedInput(oOut, 3);

    def generateModifiedInput4(self, oOut):
        """
        Generates the combined modified input source/header file, part 4.
        Returns success indicator.
        """
        return self.generateModifiedInput(oOut, 4);


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
        oParser.add_argument('asInFiles',
                             metavar = 'input.cpp.h',
                             nargs   = '*',
                             default = [os.path.join(sScriptDir, aoInfo[0])
                                        for aoInfo in iai.g_aaoAllInstrFilesAndDefaultMapAndSet],
                             help    = "Selection of VMMAll/IEMAllInst*.cpp.h files to use as input.");
        oParser.add_argument('--out-thrd-funcs-hdr',
                             metavar = 'file-thrd-funcs.h',
                             dest    = 'sOutFileThrdFuncsHdr',
                             action  = 'store',
                             default = '-',
                             help    = 'The output header file for the threaded functions.');
        oParser.add_argument('--out-thrd-funcs-cpp',
                             metavar = 'file-thrd-funcs.cpp',
                             dest    = 'sOutFileThrdFuncsCpp',
                             action  = 'store',
                             default = '-',
                             help    = 'The output C++ file for the threaded functions.');
        oParser.add_argument('--out-n8ve-funcs-hdr',
                             metavar = 'file-n8tv-funcs.h',
                             dest    = 'sOutFileN8veFuncsHdr',
                             action  = 'store',
                             default = '-',
                             help    = 'The output header file for the native recompiler functions.');
        oParser.add_argument('--out-n8ve-funcs-cpp',
                             metavar = 'file-n8tv-funcs.cpp',
                             dest    = 'sOutFileN8veFuncsCpp',
                             action  = 'store',
                             default = '-',
                             help    = 'The output C++ file for the native recompiler functions.');
        oParser.add_argument('--native-arch',
                             metavar = 'arch',
                             dest    = 'sNativeRecompilerArch',
                             action  = 'store',
                             default = None,
                             help    = 'The host architecture for the native recompiler. No default as it enables/disables '
                                     + 'generating the files related to native recompilation.');
        oParser.add_argument('--out-mod-input1',
                             metavar = 'file-instr.cpp.h',
                             dest    = 'sOutFileModInput1',
                             action  = 'store',
                             default = '-',
                             help    = 'The output C++/header file for modified input instruction files part 1.');
        oParser.add_argument('--out-mod-input2',
                             metavar = 'file-instr.cpp.h',
                             dest    = 'sOutFileModInput2',
                             action  = 'store',
                             default = '-',
                             help    = 'The output C++/header file for modified input instruction files part 2.');
        oParser.add_argument('--out-mod-input3',
                             metavar = 'file-instr.cpp.h',
                             dest    = 'sOutFileModInput3',
                             action  = 'store',
                             default = '-',
                             help    = 'The output C++/header file for modified input instruction files part 3.');
        oParser.add_argument('--out-mod-input4',
                             metavar = 'file-instr.cpp.h',
                             dest    = 'sOutFileModInput4',
                             action  = 'store',
                             default = '-',
                             help    = 'The output C++/header file for modified input instruction files part 4.');
        oParser.add_argument('--help', '-h', '-?',
                             action  = 'help',
                             help    = 'Display help and exit.');
        oParser.add_argument('--version', '-V',
                             action  = 'version',
                             version = 'r%s (IEMAllThreadedPython.py), r%s (IEMAllInstPython.py)'
                                     % (__version__.split()[1], iai.__version__.split()[1],),
                             help    = 'Displays the version/revision of the script and exit.');
        self.oOptions = oParser.parse_args(asArgs[1:]);
        print("oOptions=%s" % (self.oOptions,));

        #
        # Process the instructions specified in the IEM sources.
        #
        if self.processInputFiles(self.oOptions.sNativeRecompilerArch):
            #
            # Generate the output files.
            #
            aaoOutputFiles = (
                 ( self.oOptions.sOutFileThrdFuncsHdr,  self.generateThreadedFunctionsHeader ),
                 ( self.oOptions.sOutFileThrdFuncsCpp,  self.generateThreadedFunctionsSource ),
                 ( self.oOptions.sOutFileN8veFuncsHdr,  self.generateNativeFunctionsHeader ),
                 ( self.oOptions.sOutFileN8veFuncsCpp,  self.generateNativeFunctionsSource ),
                 ( self.oOptions.sOutFileModInput1,     self.generateModifiedInput1 ),
                 ( self.oOptions.sOutFileModInput2,     self.generateModifiedInput2 ),
                 ( self.oOptions.sOutFileModInput3,     self.generateModifiedInput3 ),
                 ( self.oOptions.sOutFileModInput4,     self.generateModifiedInput4 ),
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

