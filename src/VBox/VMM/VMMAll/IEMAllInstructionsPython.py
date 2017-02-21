#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
IEM instruction extractor.

This script/module parses the IEMAllInstruction*.cpp.h files next to it and
collects information about the instructions.  It can then be used to generate
disassembler tables and tests.
"""

__copyright__ = \
"""
Copyright (C) 2017 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL) only, as it comes in the "COPYING.CDDL" file of the
VirtualBox OSE distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.
"""
__version__ = "$Revision$"

# Standard python imports.
import os
import re
import sys

# Only the main script needs to modify the path.
g_ksValidationKitDir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                    'ValidationKit');
sys.path.append(g_ksValidationKitDir);

from common import utils;

# Python 3 hacks:
if sys.version_info[0] >= 3:
    long = int;     # pylint: disable=redefined-builtin,invalid-name


# Annotation example:
#
# \@opmnemonic  add
# \@op1         reg:Eb
# \@op2         rm:Gb
# \@opmaps      onebyte
# \@oppfx       none
# \@opcode      0x00
# \@openc       ModR/M
# \@opfltest    none
# \@opflmodify  of,sz,zf,af,pf,cf
# \@opflundef   none
# \@opflset     none
# \@opflclear   none
# \@ophints     harmless
# \@opstats     add_Eb_Gb
# \@opgroup     op_gen_arith_bin
# \@optest                  in1=1 in2=1 -> out1=2 outfl=a?,p?
# \@optest      oppfx:o32   in1=0xfffffffe:dw in2=1:dw -> out1=0xffffffff:dw outfl=a?,p?


g_kdX86EFlagsConstants = {
    'X86_EFL_CF':          0x00000001, # RT_BIT_32(0)
    'X86_EFL_1':           0x00000002, # RT_BIT_32(1)
    'X86_EFL_PF':          0x00000004, # RT_BIT_32(2)
    'X86_EFL_AF':          0x00000010, # RT_BIT_32(4)
    'X86_EFL_ZF':          0x00000040, # RT_BIT_32(6)
    'X86_EFL_SF':          0x00000080, # RT_BIT_32(7)
    'X86_EFL_TF':          0x00000100, # RT_BIT_32(8)
    'X86_EFL_IF':          0x00000200, # RT_BIT_32(9)
    'X86_EFL_DF':          0x00000400, # RT_BIT_32(10)
    'X86_EFL_OF':          0x00000800, # RT_BIT_32(11)
    'X86_EFL_IOPL':        0x00003000, # (RT_BIT_32(12) | RT_BIT_32(13))
    'X86_EFL_NT':          0x00004000, # RT_BIT_32(14)
    'X86_EFL_RF':          0x00010000, # RT_BIT_32(16)
    'X86_EFL_VM':          0x00020000, # RT_BIT_32(17)
    'X86_EFL_AC':          0x00040000, # RT_BIT_32(18)
    'X86_EFL_VIF':         0x00080000, # RT_BIT_32(19)
    'X86_EFL_VIP':         0x00100000, # RT_BIT_32(20)
    'X86_EFL_ID':          0x00200000, # RT_BIT_32(21)
    'X86_EFL_LIVE_MASK':   0x003f7fd5, # UINT32_C(0x003f7fd5)
    'X86_EFL_RA1_MASK':    0x00000002, # RT_BIT_32(1)
};


def _isValidOpcodeByte(sOpcode):
    """
    Checks if sOpcode is a valid lower case opcode byte.
    Returns true/false.
    """
    if len(sOpcode) == 4:
        if sOpcode[:2] == '0x':
            if sOpcode[2] in '0123456789abcdef':
                if sOpcode[3] in '0123456789abcdef':
                    return True;
    return False;


class InstructionMap(object):
    """
    Instruction map.

    The opcode map provides the lead opcode bytes (empty for the one byte
    opcode map).  An instruction can be member of multiple opcode maps as long
    as it uses the same opcode value within the map (because of VEX).
    """

    kdEncodings = {
        'legacy':   [],
        'vex1':     [], ##< VEX or EVEX prefix with vvvvv = 1
        'vex2':     [], ##< VEX or EVEX prefix with vvvvv = 2
        'vex3':     [], ##< VEX or EVEX prefix with vvvvv = 3
        'xop8':     [], ##< XOP prefix with vvvvv = 8
        'xop9':     [], ##< XOP prefix with vvvvv = 9
        'xop10':    [], ##< XOP prefix with vvvvv = 10
    };
    kdSelectors = {
        'byte':     [], ##< next opcode byte selects the instruction (default).
        '/r':       [], ##< modrm.reg selects the instruction.
        'mod /r':   [], ##< modrm.reg and modrm.mod selects the instruction.
        '!11 /r':   [], ##< modrm.reg selects the instruction with modrm.mod != 0y11.
        '11 /r':    [], ##< modrm.reg select the instruction with modrm.mod == 0y11.
        '11':       [], ##< modrm.reg and modrm.rm select the instruction with modrm.mod == 0y11.
    };

    def __init__(self, sName, asLeadOpcodes = None, sSelector = 'byte', sEncoding = 'legacy'):
        assert sSelector in self.kdSelectors;
        assert sEncoding in self.kdEncodings;
        if asLeadOpcodes is None:
            asLeadOpcodes = [];
        else:
            for sOpcode in asLeadOpcodes:
                assert _isValidOpcodeByte(sOpcode);

        self.sName          = sName;
        self.asLeadOpcodes  = asLeadOpcodes;    ##< Lead opcode bytes formatted as hex strings like '0x0f'.
        self.sSelector      = sSelector;        ##< The member selector, see kdSelectors.
        self.sEncoding      = sEncoding;        ##< The encoding, see kdSelectors.
        self.aoInstructions = [];               # type: Instruction


class TestType(object):
    """
    Test value type.

    This base class deals with integer like values.  The fUnsigned constructor
    parameter indicates the default stance on zero vs sign extending.  It is
    possible to override fUnsigned=True by prefixing the value with '+' or '-'.
    """
    def __init__(self, sName, acbSizes = None, fUnsigned = True):
        self.sName = sName;
        self.acbSizes = [1, 2, 4, 8, 16, 32] if acbSizes is None else acbSizes;  # Normal sizes.
        self.fUnsigned = fUnsigned;

    class BadValue(Exception):
        """ Bad value exception. """
        def __init__(self, sMessage):
            Exception.__init__(self, sMessage);
            self.sMessage = sMessage;

    def get(self, sValue):
        """
        Get the shortest normal sized byte representation of oValue.

        Returns ((fSignExtend, bytearray), ) or ((fSignExtend, bytearray), (fSignExtend, bytearray), ).
        The latter form is for AND+OR pairs where the first entry is what to
        AND with the field and the second the one or OR with.

        Raises BadValue if invalid value.
        """
        if len(sValue) == 0:
            raise TestType.BadValue('empty value');

        # Deal with sign and detect hexadecimal or decimal.
        fSignExtend = not self.fUnsigned;
        if sValue[0] == '-' or sValue[0] == '+':
            fSignExtend = True;
            fHex = len(sValue) > 3 and sValue[1:3].lower() == '0x';
        else:
            fHex = len(sValue) > 2 and sValue[0:2].lower() == '0x';

        # try convert it to long integer.
        try:
            iValue = long(sValue, 16 if fHex else 10);
        except Exception as oXcpt:
            raise TestType.BadValue('failed to convert "%s" to integer (%s)' % (sValue, oXcpt));

        # Convert the hex string and pad it to a decent value.
        if iValue >= 0:
            sHex = hex(iValue);
        else:
            sHex = hex(iValue);
        assert sHex[:2] == '0x', sHex;
        if sys.version_info[0] >= 3:
            sHex = sHex[2:];
        else:
            assert sHex[-1] == 'L';
            sHex = sHex[2:-1];

        cDigits = len(sHex);
        if cDigits <= self.acbSizes[-1] * 2:
            for cb in self.acbSizes:
                if cDigits <= cb * 2:
                    cDigits = int((cDigits + cb - 1) / cb) * cb; # Seems like integer division returns a float in python.
                    break;
        else:
            cDigits = int((cDigits + self.acbSizes[-1] - 1) / self.acbSizes[-1]) * self.acbSizes[-1];
            assert isinstance(cDigits, int)

        if cDigits != len(sHex):
            cNeeded = cDigits - len(sHex);
            if iValue >= 0:
                sHex = ('0' * cNeeded) + sHex;
            else:
                sHex = ('f' * cNeeded) + sHex;

        # Invert and convert to bytearray and return it.
        abValue = bytearray([int(sHex[offHex - 2 : offHex], 16) for offHex in range(len(sHex), 0, -2)]);

        return ((fSignExtend, abValue),);

    def validate(self, sValue):
        """
        Returns True if value is okay, error message on failure.
        """
        try:
            self.get(sValue);
        except TestType.BadValue as oXcpt:
            return oXcpt.sMessage;
        return True;

    def isAndOrPair(self, sValue):
        """
        Checks if sValue is a pair.
        """
        return False;


class TestTypeEflags(TestType):
    """
    Special value parsing for EFLAGS/RFLAGS/FLAGS.
    """

    kdZeroValueFlags = { 'nv': 0, 'pl': 0, 'nz': 0, 'na': 0, 'pe': 0, 'nc': 0, 'di': 0, 'up': 0 };

    def __init__(self, sName):
        TestType.__init__(self, sName, acbSizes = [1, 2, 4, 8], fUnsigned = True);

    def get(self, sValue):
        print('get(%s)' % (sValue,));
        fClear = 0;
        fSet   = 0;
        for sFlag in sValue.split(','):
            sConstant = SimpleParser.kdEFlags.get(sFlag, None);
            if sConstant is None:
                print('get(%s) raise for %s/%s' % (sValue, sFlag,sConstant));
                raise self.BadValue('Unknown flag "%s" in "%s"' % (sFlag, sValue))
            if sConstant[0] == '!':
                fClear |= g_kdX86EFlagsConstants[sConstant[1:]];
            else:
                fSet   |= g_kdX86EFlagsConstants[sConstant];

        print('get -> TestType.get');
        aoSet = TestType.get(self, '0x%x' % (fSet,));
        print('get: aoSet=%s' % (aoSet,));
        if fClear != 0:
            print('get -> TestType.get(%#x)' % (~fClear));
            try:
                aoClear = TestType.get(self, '%#x' % (~fClear))
            except Exception as oXcpt:
                print( '%s' % (oXcpt,))
                raise;
            print('get: aoClear=%s' % (aoSet,));
            assert self.isAndOrPair(sValue) == True;
            return (aoClear[0], aoSet[0]);
        assert self.isAndOrPair(sValue) == False;
        return aoSet;

    def isAndOrPair(self, sValue):
        for sZeroFlag in self.kdZeroValueFlags.keys():
            if sValue.find(sZeroFlag) >= 0:
                print('isAndOrPair(%s) -> True' % (sValue,));
                return True;
        print('isAndOrPair(%s) -> False' % (sValue,));
        return False;



class TestInOut(object):
    """
    One input or output state modifier.

    This should be thought as values to modify BS3REGCTX and extended (needs
    to be structured) state.
    """
    ## Assigned operators.
    kasOperators = [
        '&~=',
        '&=',
        '|=',
        '='
    ];
    ## Types
    kdTypes = {
        'uint':  TestType('uint', fUnsigned = True),
        'int':   TestType('int'),
        'efl':   TestTypeEflags('efl'),
    };
    ## CPU context fields.
    kdFields = {
        # name:     ( default type, tbd, )
        # Operands.
        'op1':      ( 'uint', '', ), ## \@op1
        'op2':      ( 'uint', '', ), ## \@op2
        'op3':      ( 'uint', '', ), ## \@op3
        'op4':      ( 'uint', '', ), ## \@op4
        # Flags.
        'efl':      ( 'efl',  '', ),
        # 8-bit GPRs.
        'al':       ( 'uint', '', ),
        'cl':       ( 'uint', '', ),
        'dl':       ( 'uint', '', ),
        'bl':       ( 'uint', '', ),
        'ah':       ( 'uint', '', ),
        'ch':       ( 'uint', '', ),
        'dh':       ( 'uint', '', ),
        'bh':       ( 'uint', '', ),
        'r8l':      ( 'uint', '', ),
        'r9l':      ( 'uint', '', ),
        'r10l':     ( 'uint', '', ),
        'r11l':     ( 'uint', '', ),
        'r12l':     ( 'uint', '', ),
        'r13l':     ( 'uint', '', ),
        'r14l':     ( 'uint', '', ),
        'r15l':     ( 'uint', '', ),
        # 16-bit GPRs.
        'ax':       ( 'uint', '', ),
        'dx':       ( 'uint', '', ),
        'cx':       ( 'uint', '', ),
        'bx':       ( 'uint', '', ),
        'sp':       ( 'uint', '', ),
        'bp':       ( 'uint', '', ),
        'si':       ( 'uint', '', ),
        'di':       ( 'uint', '', ),
        'r8w':      ( 'uint', '', ),
        'r9w':      ( 'uint', '', ),
        'r10w':     ( 'uint', '', ),
        'r11w':     ( 'uint', '', ),
        'r12w':     ( 'uint', '', ),
        'r13w':     ( 'uint', '', ),
        'r14w':     ( 'uint', '', ),
        'r15w':     ( 'uint', '', ),
        # 32-bit GPRs.
        'eax':      ( 'uint', '', ),
        'edx':      ( 'uint', '', ),
        'ecx':      ( 'uint', '', ),
        'ebx':      ( 'uint', '', ),
        'esp':      ( 'uint', '', ),
        'ebp':      ( 'uint', '', ),
        'esi':      ( 'uint', '', ),
        'edi':      ( 'uint', '', ),
        'r8d':      ( 'uint', '', ),
        'r9d':      ( 'uint', '', ),
        'r10d':     ( 'uint', '', ),
        'r11d':     ( 'uint', '', ),
        'r12d':     ( 'uint', '', ),
        'r13d':     ( 'uint', '', ),
        'r14d':     ( 'uint', '', ),
        'r15d':     ( 'uint', '', ),
        # 64-bit GPRs.
        'rax':      ( 'uint', '', ),
        'rdx':      ( 'uint', '', ),
        'rcx':      ( 'uint', '', ),
        'rbx':      ( 'uint', '', ),
        'rsp':      ( 'uint', '', ),
        'rbp':      ( 'uint', '', ),
        'rsi':      ( 'uint', '', ),
        'rdi':      ( 'uint', '', ),
        'r8':       ( 'uint', '', ),
        'r9':       ( 'uint', '', ),
        'r10':      ( 'uint', '', ),
        'r11':      ( 'uint', '', ),
        'r12':      ( 'uint', '', ),
        'r13':      ( 'uint', '', ),
        'r14':      ( 'uint', '', ),
        'r15':      ( 'uint', '', ),
        # 16-bit, 32-bit or 64-bit registers according to operand size.
        'oz.rax':   ( 'uint', '', ),
        'oz.rdx':   ( 'uint', '', ),
        'oz.rcx':   ( 'uint', '', ),
        'oz.rbx':   ( 'uint', '', ),
        'oz.rsp':   ( 'uint', '', ),
        'oz.rbp':   ( 'uint', '', ),
        'oz.rsi':   ( 'uint', '', ),
        'oz.rdi':   ( 'uint', '', ),
        'oz.r8':    ( 'uint', '', ),
        'oz.r9':    ( 'uint', '', ),
        'oz.r10':   ( 'uint', '', ),
        'oz.r11':   ( 'uint', '', ),
        'oz.r12':   ( 'uint', '', ),
        'oz.r13':   ( 'uint', '', ),
        'oz.r14':   ( 'uint', '', ),
        'oz.r15':   ( 'uint', '', ),
    };

    def __init__(self, sField, sOp, sValue, sType):
        assert sField in self.kdFields;
        assert sOp in self.kasOperators;
        self.sField = sField;
        self.sOp    = sOp;
        self.sValue = sValue;
        self.sType  = sType;


class TestSelector(object):
    """
    One selector for an instruction test.
    """
    ## Selector compare operators.
    kasCompareOps = [ '==', '!=' ];
    ## Selector variables and their valid values.
    kdVariables = {
        # Operand size.
        'size': {
            'o16':  'size_o16',
            'o32':  'size_o32',
            'o64':  'size_o64',
        },
        # Execution ring.
        'ring': {
            '0':    'ring_0',
            '1':    'ring_1',
            '2':    'ring_2',
            '3':    'ring_3',
            '0..2': 'ring_0_thru_2',
            '1..3': 'ring_1_thru_3',
        },
        # Basic code mode.
        'codebits': {
            '64':   'code_64bit',
            '32':   'code_32bit',
            '16':   'code_16bit',
        },
        # cpu modes.
        'mode': {
            'real': 'mode_real',
            'prot': 'mode_prot',
            'long': 'mode_long',
            'v86':  'mode_v86',
            'smm':  'mode_smm',
            'vmx':  'mode_vmx',
            'svm':  'mode_svm',
        },
        # paging on/off
        'paging': {
            'on': 'paging_on',
            'off': 'paging_off',
        },
    };
    ## Selector shorthand predicates.
    ## These translates into variable expressions.
    kdPredicates = {
        'o16':          'size==o16',
        'o32':          'size==o32',
        'o64':          'size==o64',
        'ring0':        'ring==0',
        '!ring0':       'ring==1..3',
        'ring1':        'ring==1',
        'ring2':        'ring==2',
        'ring3':        'ring==3',
        'user':         'ring==3',
        'supervisor':   'ring==0..2',
        'real':         'mode==real',
        'prot':         'mode==prot',
        'long':         'mode==long',
        'v86':          'mode==v86',
        'smm':          'mode==smm',
        'vmx':          'mode==vmx',
        'svm':          'mode==svm',
        'paging':       'paging==on',
        '!paging':      'paging==off',
    };

    def __init__(self, sVariable, sOp, sValue):
        assert sVariable in self.kdVariables;
        assert sOp in self.kasCompareOps;
        assert sValue in self.kdVariables[sVariable];
        self.sVariable  = sVariable;
        self.sOp        = sOp;
        self.sValue     = sValue;


class InstructionTest(object):
    """
    Instruction test.
    """

    def __init__(self, oInstr): # type: (InstructionTest, Instruction)
        self.oInstr         = oInstr; # type: InstructionTest
        self.aoInputs       = [];
        self.aoOutputs      = [];
        self.aoSelectors    = []; # type: list(TestSelector)


class Operand(object):
    """
    Instruction operand.
    """

    ## \@op[1-4]
    kdLocations = {
        'reg':  [], ## modrm.reg
        'rm':   [], ## modrm.rm
    };

    ## \@op[1-4]
    kdTypes = {
        'Eb':   [],
        'Gb':   [],
    };

    def __init__(self, sWhere, sType):
        assert sWhere in self.kdLocations;
        assert sType  in self.kdTypes;
        self.sWhere = sWhere;           ##< kdLocations
        self.sType  = sType;            ##< kdTypes


class Instruction(object):
    """
    Instruction.
    """

    def __init__(self, sSrcFile, iLine):
        ## @name Core attributes.
        ## @{
        self.sMnemonic      = None;
        self.sBrief         = None;
        self.asDescSections = [];       # type: list(str)
        self.aoMaps         = [];       # type: list(InstructionMap)
        self.aoOperands     = [];       # type: list(Operand)
        self.sPrefix        = None;     ##< Single prefix: None, 0x66, 0xf3, 0xf2
        self.sOpcode        = None;
        self.sEncoding      = None;
        self.asFlTest       = None;
        self.asFlModify     = None;
        self.asFlUndefined  = None;
        self.asFlSet        = None;
        self.asFlClear      = None;
        self.dHints         = {};       ##< Dictionary of instruction hints, flags, whatnot. (Dictioarny for speed; dummy value).
        self.asCpuIds       = [];       ##< The CPUID feature bit names for this instruction. If multiple, assume AND.
        self.asReqFeatures  = [];       ##< Which features are required to be enabled to run this instruction.
        self.aoTests        = [];       # type: list(InstructionTest)
        self.oCpus          = None;     ##< Some CPU restriction expression...
        self.sGroup         = None;
        self.fUnused        = False;    ##< Unused instruction.
        self.fInvalid       = False;    ##< Invalid instruction (like UD2).
        self.sInvalidStyle  = None;     ##< Invalid behviour style
        ## @}

        ## @name Implementation attributes.
        ## @{
        self.sStats         = None;
        self.sFunction      = None;
        self.fStub          = False;
        self.fUdStub        = False;
        ## @}

        ## @name Decoding info
        ## @{
        self.sSrcFile       = sSrcFile;
        self.iLineCreated   = iLine;
        self.iLineCompleted = None;
        self.cOpTags        = 0;
        ## @}

        ## @name Intermediate input fields.
        ## @{
        self.sRawDisOpNo    = None;
        self.asRawDisParams = [];
        self.sRawIemOpFlags = None;
        self.sRawOldOpcodes = None;
        ## @}


## All the instructions.
g_aoAllInstructions = []; # type: Instruction

## Instruction maps.
g_dInstructionMaps = {
    'one':          InstructionMap('one'),
    'grp1_80':      InstructionMap('grp1_80',   asLeadOpcodes = ['0x80',]),
    'grp1_81':      InstructionMap('grp1_81',   asLeadOpcodes = ['0x81',], sSelector = '/r'),
    'grp1_82':      InstructionMap('grp1_82',   asLeadOpcodes = ['0x82',], sSelector = '/r'),
    'grp1_83':      InstructionMap('grp1_83',   asLeadOpcodes = ['0x83',], sSelector = '/r'),
    'grp1a':        InstructionMap('grp1a',     asLeadOpcodes = ['0x8f',], sSelector = '/r'),
    'grp2_c0':      InstructionMap('grp2_c0',   asLeadOpcodes = ['0xc0',], sSelector = '/r'),
    'grp2_c1':      InstructionMap('grp2_c1',   asLeadOpcodes = ['0xc1',], sSelector = '/r'),
    'grp2_d0':      InstructionMap('grp2_d0',   asLeadOpcodes = ['0xd0',], sSelector = '/r'),
    'grp2_d1':      InstructionMap('grp2_d1',   asLeadOpcodes = ['0xd1',], sSelector = '/r'),
    'grp2_d2':      InstructionMap('grp2_d2',   asLeadOpcodes = ['0xd2',], sSelector = '/r'),
    'grp2_d3':      InstructionMap('grp2_d3',   asLeadOpcodes = ['0xd3',], sSelector = '/r'),
    'grp3_f6':      InstructionMap('grp3_f6',   asLeadOpcodes = ['0xf6',], sSelector = '/r'),
    'grp3_f7':      InstructionMap('grp3_f7',   asLeadOpcodes = ['0xf7',], sSelector = '/r'),
    'grp4':         InstructionMap('grp4',      asLeadOpcodes = ['0xfe',], sSelector = '/r'),
    'grp5':         InstructionMap('grp5',      asLeadOpcodes = ['0xff',], sSelector = '/r'),
    'grp11_c6_m':   InstructionMap('grp11_c6_m',asLeadOpcodes = ['0xc6',], sSelector = '!11 /r'),
    'grp11_c6_r':   InstructionMap('grp11_c6_r',asLeadOpcodes = ['0xc6',], sSelector = '11'),    # xabort
    'grp11_c7_m':   InstructionMap('grp11_c7_m',asLeadOpcodes = ['0xc7',], sSelector = '!11 /r'),
    'grp11_c7_r':   InstructionMap('grp11_c7_r',asLeadOpcodes = ['0xc7',], sSelector = '11'),    # xbegin

    'two0f':        InstructionMap('two0f',     asLeadOpcodes = ['0x0f',]),
    'grp6':         InstructionMap('grp6',      asLeadOpcodes = ['0x0f', '0x00',], sSelector = '/r'),
    'grp7_m':       InstructionMap('grp7_m',    asLeadOpcodes = ['0x0f', '0x01',], sSelector = '!11 /r'),
    'grp7_r':       InstructionMap('grp7_r',    asLeadOpcodes = ['0x0f', '0x01',], sSelector = '11'),
    'grp8':         InstructionMap('grp8',      asLeadOpcodes = ['0x0f', '0xba',], sSelector = '/r'),
    'grp9':         InstructionMap('grp9',      asLeadOpcodes = ['0x0f', '0xc7',], sSelector = 'mod /r'),
    'grp10':        InstructionMap('grp10',     asLeadOpcodes = ['0x0f', '0xb9',], sSelector = '/r'), # UD1 /w modr/m
    'grp12':        InstructionMap('grp12',     asLeadOpcodes = ['0x0f', '0x71',], sSelector = 'mod /r'),
    'grp13':        InstructionMap('grp13',     asLeadOpcodes = ['0x0f', '0x72',], sSelector = 'mod /r'),
    'grp14':        InstructionMap('grp14',     asLeadOpcodes = ['0x0f', '0x73',], sSelector = 'mod /r'),
    'grp15':        InstructionMap('grp15',     asLeadOpcodes = ['0x0f', '0xae',], sSelector = 'mod /r'),
    'grp16':        InstructionMap('grp16',     asLeadOpcodes = ['0x0f', '0x18',], sSelector = 'mod /r'),
    'grpA17':       InstructionMap('grpA17',    asLeadOpcodes = ['0x0f', '0x78',], sSelector = '/r'), # AMD: EXTRQ weirdness
    'grpP':         InstructionMap('grpP',      asLeadOpcodes = ['0x0f', '0x0d',], sSelector = '/r'), # AMD: prefetch

    'three0f38':    InstructionMap('three0f38', asLeadOpcodes = ['0x0f', '0x38',]),
    'three0f38':    InstructionMap('three0f38', asLeadOpcodes = ['0x0f', '0x38',]),
    'three0f3a':    InstructionMap('three0f3a', asLeadOpcodes = ['0x0f', '0x3a',]),

    'vexmap1':      InstructionMap('vexmap1',   sEncoding = 'vex1'),
    'vexgrp12':     InstructionMap('vexgrp12',  sEncoding = 'vex1', asLeadOpcodes = ['0x71',], sSelector = 'mod /r'),
    'vexgrp13':     InstructionMap('vexgrp13',  sEncoding = 'vex1', asLeadOpcodes = ['0x72',], sSelector = 'mod /r'),
    'vexgrp14':     InstructionMap('vexgrp14',  sEncoding = 'vex1', asLeadOpcodes = ['0x73',], sSelector = 'mod /r'),
    'vexgrp15':     InstructionMap('vexgrp15',  sEncoding = 'vex1', asLeadOpcodes = ['0xae',], sSelector = 'mod /r'),
    'vexgrp17':     InstructionMap('vexgrp17',  sEncoding = 'vex1', asLeadOpcodes = ['0xf3',], sSelector = '/r'),

    'vexmap2':      InstructionMap('vexmap2',   sEncoding = 'vex2'),
    'vexmap3':      InstructionMap('vexmap3',   sEncoding = 'vex3'),

    'xopmap8':      InstructionMap('xopmap8',   sEncoding = 'xop8'),
    'xopmap9':      InstructionMap('xopmap9',   sEncoding = 'xop9'),
    'xopgrp1':      InstructionMap('xopgrp1',   sEncoding = 'xop9',  asLeadOpcodes = ['0x01'], sSelector = '/r'),
    'xopgrp2':      InstructionMap('xopgrp2',   sEncoding = 'xop9',  asLeadOpcodes = ['0x02'], sSelector = '/r'),
    'xopgrp3':      InstructionMap('xopgrp3',   sEncoding = 'xop9',  asLeadOpcodes = ['0x12'], sSelector = '/r'),
    'xopmap10':     InstructionMap('xopmap10',  sEncoding = 'xop10'),
    'xopgrp4':      InstructionMap('xopgrp4',   sEncoding = 'xop10', asLeadOpcodes = ['0x12'], sSelector = '/r'),
};



class ParserException(Exception):
    """ Parser exception """
    def __init__(self, sMessage):
        Exception.__init__(self, sMessage);


class SimpleParser(object):
    """
    Parser of IEMAllInstruction*.cpp.h instruction specifications.
    """

    ## @name Parser state.
    ## @{
    kiCode              = 0;
    kiCommentMulti      = 1;
    ## @}

    def __init__(self, sSrcFile, asLines, sDefaultMap):
        self.sSrcFile       = sSrcFile;
        self.asLines        = asLines;
        self.iLine          = 0;
        self.iState         = self.kiCode;
        self.sComment       = '';
        self.iCommentLine   = 0;
        self.asCurInstr     = [];

        assert sDefaultMap in g_dInstructionMaps;
        self.oDefaultMap    = g_dInstructionMaps[sDefaultMap];

        self.cTotalInstr    = 0;
        self.cTotalStubs    = 0;
        self.cTotalTagged   = 0;

        self.oReMacroName   = re.compile('^[A-Za-z_][A-Za-z0-9_]*$');
        self.oReMnemonic    = re.compile('^[A-Za-z_][A-Za-z0-9_]*$');
        self.oReStatsName   = re.compile('^[A-Za-z_][A-Za-z0-9_]*$');
        self.oReFunctionName= re.compile('^iemOp_[A-Za-z_][A-Za-z0-9_]*$');
        self.oReGroupName   = re.compile('^op_[a-z0-9]+(|_[a-z0-9]+|_[a-z0-9]+_[a-z0-9]+)$');
        self.fDebug         = True;

        self.dTagHandlers   = {
            '@opbrief':     self.parseTagOpBrief,
            '@opdesc':      self.parseTagOpDesc,
            '@opmnemonic':  self.parseTagOpMnemonic,
            '@op1':         self.parseTagOpOperandN,
            '@op2':         self.parseTagOpOperandN,
            '@op3':         self.parseTagOpOperandN,
            '@op4':         self.parseTagOpOperandN,
            '@oppfx':       self.parseTagOpPfx,
            '@opmaps':      self.parseTagOpMaps,
            '@opcode':      self.parseTagOpcode,
            '@openc':       self.parseTagOpEnc,
            '@opfltest':    self.parseTagOpEFlags,
            '@opflmodify':  self.parseTagOpEFlags,
            '@opflundef':   self.parseTagOpEFlags,
            '@opflset':     self.parseTagOpEFlags,
            '@opflclear':   self.parseTagOpEFlags,
            '@ophints':     self.parseTagOpHints,
            '@opcpuid':     self.parseTagOpCpuId,
            '@opgroup':     self.parseTagOpGroup,
            '@opunused':    self.parseTagOpUnusedInvalid,
            '@opinvalid':   self.parseTagOpUnusedInvalid,
            '@opinvlstyle': self.parseTagOpUnusedInvalid,
            '@optest':      self.parseTagOpTest,
            '@opstats':     self.parseTagOpStats,
            '@opfunction':  self.parseTagOpFunction,
            '@opdone':      self.parseTagOpDone,
        };

        self.asErrors = [];

    def raiseError(self, sMessage):
        """
        Raise error prefixed with the source and line number.
        """
        raise ParserException("%s:%d: error: %s" % (self.sSrcFile, self.iLine, sMessage,));

    def raiseCommentError(self, iLineInComment, sMessage):
        """
        Similar to raiseError, but the line number is iLineInComment + self.iCommentLine.
        """
        raise ParserException("%s:%d: error: %s" % (self.sSrcFile, self.iCommentLine + iLineInComment, sMessage,));

    def error(self, sMessage):
        """
        Adds an error.
        returns False;
        """
        self.asErrors.append(u'%s:%d: error: %s\n' % (self.sSrcFile, self.iLine, sMessage,));
        return False;

    def errorComment(self, iLineInComment, sMessage):
        """
        Adds a comment error.
        returns False;
        """
        self.asErrors.append(u'%s:%d: error: %s\n' % (self.sSrcFile, self.iCommentLine + iLineInComment, sMessage,));
        return False;

    def printErrors(self):
        """
        Print the errors to stderr.
        Returns number of errors.
        """
        if len(self.asErrors) > 0:
            sys.stderr.write(u''.join(self.asErrors));
        return len(self.asErrors);

    def debug(self, sMessage):
        """
        """
        if self.fDebug:
            print('debug: %s' % (sMessage,));


    def addInstruction(self, iLine = None):
        """
        Adds an instruction.
        """
        oInstr = Instruction(self.sSrcFile, self.iLine if iLine is None else iLine);
        g_aoAllInstructions.append(oInstr);
        self.asCurInstr.append(oInstr);
        return oInstr;

    def doneInstructionOne(self, oInstr, iLine):
        """
        Complete the parsing by processing, validating and expanding raw inputs.
        """
        assert oInstr.iLineCompleted is None;
        oInstr.iLineCompleted = iLine;

        #
        # Specified instructions.
        #
        if oInstr.cOpTags > 0:
            if oInstr.sStats is None:
                pass;

        #
        # Unspecified legacy stuff.  We generally only got a few things to go on here.
        #   /** Opcode 0x0f 0x00 /0. */
        #   FNIEMOPRM_DEF(iemOp_Grp6_sldt)
        #
        else:
            #if oInstr.sRawOldOpcodes:
            #
            #if oInstr.sMnemonic:
            pass;

        #
        # Apply default map and then add the instruction to all it's groups.
        #
        if len(oInstr.aoMaps) == 0:
            oInstr.aoMaps = [ self.oDefaultMap, ];
        for oMap in oInstr.aoMaps:
            oMap.aoInstructions.append(oInstr);

        self.debug('%d..%d: %s; %d @op tags' % (oInstr.iLineCreated, oInstr.iLineCompleted, oInstr.sFunction, oInstr.cOpTags));
        return True;

    def doneInstructions(self, iLineInComment = None):
        """
        Done with current instruction.
        """
        for oInstr in self.asCurInstr:
            self.doneInstructionOne(oInstr, self.iLine if iLineInComment is None else self.iCommentLine + iLineInComment);
            if oInstr.fStub:
                self.cTotalStubs += 1;

        self.cTotalInstr += len(self.asCurInstr);

        self.sComment     = '';
        self.asCurInstr   = [];
        return True;

    def setInstrunctionAttrib(self, sAttrib, oValue, fOverwrite = False):
        """
        Sets the sAttrib of all current instruction to oValue.  If fOverwrite
        is False, only None values and empty strings are replaced.
        """
        for oInstr in self.asCurInstr:
            if fOverwrite is not True:
                oOldValue = getattr(oInstr, sAttrib);
                if oOldValue is not None:
                    continue;
            setattr(oInstr, sAttrib, oValue);

    def setInstrunctionArrayAttrib(self, sAttrib, iEntry, oValue, fOverwrite = False):
        """
        Sets the iEntry of the array sAttrib of all current instruction to oValue.
        If fOverwrite is False, only None values and empty strings are replaced.
        """
        for oInstr in self.asCurInstr:
            aoArray = getattr(oInstr, sAttrib);
            while len(aoArray) <= iEntry:
                aoArray.append(None);
            if fOverwrite is True or aoArray[iEntry] is None:
                aoArray[iEntry] = oValue;

    def parseCommentOldOpcode(self, asLines):
        """ Deals with 'Opcode 0xff /4' like comments """
        asWords = asLines[0].split();
        if    len(asWords) >= 2  \
          and asWords[0] == 'Opcode'  \
          and (   asWords[1].startswith('0x')
               or asWords[1].startswith('0X')):
            asWords = asWords[:1];
            for iWord, sWord in enumerate(asWords):
                if sWord.startswith('0X'):
                    sWord = '0x' + sWord[:2];
                    asWords[iWord] = asWords;
            self.setInstrunctionAttrib('sRawOldOpcodes', ' '.join(asWords));

        return False;

    def ensureInstructionForOpTag(self, iTagLine):
        """ Ensure there is an instruction for the op-tag being parsed. """
        if len(self.asCurInstr) == 0:
            self.addInstruction(self.iCommentLine + iTagLine);
        for oInstr in self.asCurInstr:
            oInstr.cOpTags += 1;
            if oInstr.cOpTags == 1:
                self.cTotalTagged += 1;
        return self.asCurInstr[-1];

    @staticmethod
    def flattenSections(aasSections):
        """
        Flattens multiline sections into stripped single strings.
        Returns list of strings, on section per string.
        """
        asRet = [];
        for asLines in assSections:
            if len(asLines) > 0:
                asRet.append(' '.join([sLine.strip() for sLine in asLines]));
        return asRet;

    @staticmethod
    def flattenAllSections(aasSections, sLineSep = ' ', sSectionSep = '\n'):
        """
        Flattens sections into a simple stripped string with newlines as
        section breaks.  The final section does not sport a trailing newline.
        """
        # Typical: One section with a single line.
        if len(aasSections) == 1 and len(aasSections[0]) == 1:
            return aasSections[0][0].strip();

        sRet = '';
        for iSection, asLines in enumerate(aasSections):
            if len(asLines) > 0:
                if iSection > 0:
                    sRet += sSectionSep;
                sRet += sLineSep.join([sLine.strip() for sLine in asLines]);
        return sRet;



    ## @name Tag parsers
    ## @{

    def parseTagOpBrief(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:    \@opbrief
        Value:  Text description, multiple sections, appended.

        Brief description.  If not given, it's the first sentence from @opdesc.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten and validate the value.
        sBrief = self.flattenAllSections(aasSections);
        if len(sBrief) == 0:
            return self.errorComment(iTagLine, '%s: value required' % (sTag,));
        if sBrief[-1] != '.':
            sBrief = sBrief + '.';
        if len(sBrief) > 180:
            return self.errorComment(iTagLine, '%s: value too long (max 180 chars): %s' % (sTag, sBrief));
        offDot = sBrief.find('.');
        while offDot >= 0 and offDot < len(sBrief) - 1 and sBrief[offDot + 1] != ' ':
            offDot = sBrief.find('.', offDot + 1);
        if offDot >= 0 and offDot != len(sBrief) - 1:
            return self.errorComment(iTagLine, '%s: only one sentence: %s' % (sTag, sBrief));

        # Update the instruction.
        if oInstr.sBrief is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite brief "%s" with "%s"'
                                               % (sTag, oInstr.sBrief, sBrief,));
        _ = iEndLine;
        return True;

    def parseTagOpDesc(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:    \@opdesc
        Value:  Text description, multiple sections, appended.

        It is used to describe instructions.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);
        if len(self.aoInstructions) > 0 and len(aasSections) > 0:
            oInstr.asDescSections.extend(self.flattenSections(aasSections));
            return True;

        _ = sTag; _ = iEndLine;
        return True;

    def parseTagOpMnemonic(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:    @opmenmonic
        Value:  mnemonic

        The 'mnemonic' value must be a valid C identifier string.  Because of
        prefixes, groups and whatnot, there times when the mnemonic isn't that
        of an actual assembler mnemonic.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten and validate the value.
        sMnemonic = self.flattenAllSections(aasSections);
        if not self.oReMnemonic.match(sMnemonic):
            return self.errorComment(iTagLine, '%s: invalid menmonic name: "%s"' % (sTag, sMnemonic,));
        if oInstr.sMnemonic is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite menmonic "%s" with "%s"'
                                     % (sTag, oInstr.sMnemonic, sMnemonic,));
        oInstr.sMnemonic = sMnemonic

        _ = iEndLine;
        return True;

    def parseTagOpOperandN(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tags:  \@op1, \@op2, \@op3, \@op4
        Value: where:type

        The 'where' value indicates where the operand is found, like the 'reg'
        part of the ModR/M encoding. See Instruction.kdOperandLocations for
        a list.

        The 'type' value indicates the operand type.  These follow the types
        given in the opcode tables in the CPU reference manuals.
        See Instruction.kdOperandTypes for a list.

        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);
        idxOp = int(sTag[-1]) - 1;
        assert idxOp >= 0 and idxOp < 4;

        # flatten, split up, and validate the "where:type" value.
        sFlattened = self.flattenAllSections(aasSections);
        asSplit = sFlattened.split(':');
        if len(asSplit) != 2:
            return self.errorComment(iTagLine, 'expected %s value on format "<where>:<type>" not "%s"' % (sTag, sFlattened,));

        (sWhere, sType) = asSplit;
        if sWhere not in Operand.kdLocations:
            return self.errorComment(iTagLine, '%s: invalid where value "%s", valid: %s'
                                               % (sTag, sWhere, ', '.join(Operand.kdLocations.keys()),), iTagLine);

        if sType not in Operand.kdTypes:
            return self.errorComment(iTagLine, '%s: invalid where value "%s", valid: %s'
                                               % (sTag, sType, ', '.join(Operand.kdTypes.keys()),));

        # Insert the operand, refusing to overwrite an existing one.
        while idxOp >= len(oInstr.aoOperands):
            oInstr.aoOperands.append(None);
        if oInstr.aoOperands[idxOp] is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite "%s:%s" with "%s:%s"'
                                               % ( sTag, oInstr.aoOperands[idxOp].sWhere, oInstr.aoOperands[idxOp].sType,
                                                   sWhere, sType,));
        oInstr.aoOperands[idxOp] = Operand(sWhere, sType);

        _ = iEndLine;
        return True;

    def parseTagOpMaps(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:    \@opmaps
        Value:  map[,map2]

        Indicates which maps the instruction is in.  There is a default map
        associated with each input file.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten, split up and validate the value.
        sFlattened = self.flattenAllSections(aasSections, sLineSep = ',', sSectionSep = ',');
        asMaps = sFlattened.split(',');
        if len(asMaps) == 0:
            return self.errorComment(iTagLine, '%s: value required' % (sTag,));
        for sMap in asMaps:
            if sMap not in g_dInstructionMaps:
                return self.errorComment(iTagLine, '%s: invalid map value: %s  (valid values: %s)'
                                                   % (sTag, sMap, ', '.join(g_dInstructionMaps.keys()),));

        # Add the maps to the current list.  Throw errors on duplicates.
        for oMap in oInstr.aoMaps:
            if oMap.sName in asMaps:
                return self.errorComment(iTagLine, '%s: duplicate map assignment: %s' % (sTag, oMap.sName));

        for sMap in asMaps:
            oMap = g_dInstructionMaps[sMap];
            if oMap not in oInstr.aoMaps:
                oInstr.aoMaps.append(oMap);
            else:
                self.errorComment(iTagLine, '%s: duplicate map assignment (input): %s' % (sTag, sMap));

        _ = iEndLine;
        return True;

    ## \@oppfx values.
    kdPrefixes = {
        '0x66': [],
        '0xf3': [],
        '0xf2': [],
    };

    def parseTagOpPfx(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:        \@oppfx
        Value:      none|0x66|0xf3|0xf2

        Required prefix for the instruction.  (In a (E)VEX context this is the
        value of the 'pp' field rather than an actual prefix.)
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten and validate the value.
        sFlattened = self.flattenAllSections(aasSections);
        asPrefixes = sFlattened.split();
        if len(asPrefixes) > 1:
            return self.errorComment(iTagLine, '%s: max one prefix: %s' % (sTag, asPrefixes,));

        sPrefix = asPrefixes[0].lower();
        if sPrefix == 'none':
            sPrefix = None;
        else:
            if len(sPrefix) == 2:
                sPrefix = '0x' + sPrefix;
            if _isValidOpcodeByte(sPrefix):
                return self.errorComment(iTagLine, '%s: invalid prefix: %s' % (sTag, sPrefix,));

        if sPrefix is not None and sPrefix not in self.kdPrefixes:
            return self.errorComment(iTagLine, '%s: invalid prefix: %s (valid %s)' % (sTag, sPrefix, self.kdPrefixes,));

        # Set it.
        if oInstr.sPrefix is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite "%s" with "%s"' % ( sTag, oInstr.sPrefix, sPrefix,));
        oInstr.sPrefix = sPrefix;

        _ = iEndLine;
        return True;

    ## Special \@opcode tag values.
    kdSpecialOpcodes = {
        '/reg':         [],
        'mr/reg':       [],
        '11 /reg':      [],
        '!11 /reg':     [],
        '11 mr/reg':    [],
        '!11 mr/reg':   [],
    };

    def parseTagOpcode(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:        \@opcode
        Value:      0x?? | /reg | mr/reg | 11 /reg | !11 /reg | 11 mr/reg | !11 mr/reg

        The opcode byte or sub-byte for the instruction in the context of a map.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten and validate the value.
        sOpcode = self.flattenAllSections(aasSections);
        if sOpcode in self.kdSpecialOpcodes:
            pass;
        elif not _isValidOpcodeByte(sOpcode):
            return self.errorComment(iTagLine, '%s: invalid opcode: %s' % (sTag, sOpcode,));

        # Set it.
        if oInstr.sOpcode is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite "%s" with "%s"' % ( sTag, oInstr.sOpcode, sOpcode,));
        oInstr.sOpcode = sOpcode;

        _ = iEndLine;
        return True;

    ## Valid values for \@openc
    kdEncodings = {
        'ModR/M': [],
    };

    def parseTagOpEnc(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:        \@openc
        Value:      ModR/M|TBD

        The instruction operand encoding style.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten and validate the value.
        sEncoding = self.flattenAllSections(aasSections);
        if sEncoding in self.kdEncodings:
            pass;
        elif not _isValidOpcodeByte(sEncoding):
            return self.errorComment(iTagLine, '%s: invalid encoding: %s' % (sTag, sEncoding,));

        # Set it.
        if oInstr.sEncoding is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite "%s" with "%s"'
                                               % ( sTag, oInstr.sEncoding, sEncoding,));
        oInstr.sEncoding = sEncoding;

        _ = iEndLine;
        return True;

    ## EFlags values allowed in \@opfltest, \@opflmodify, \@opflundef, \@opflset, and \@opflclear.
    kdEFlags = {
        # Debugger flag notation:
        'ov':   'X86_EFL_OF',   ##< OVerflow.
        'nv':  '!X86_EFL_OF',   ##< No Overflow.

        'ng':   'X86_EFL_SF',   ##< NeGative (sign).
        'pl':  '!X86_EFL_SF',   ##< PLuss (sign).

        'zr':   'X86_EFL_ZF',   ##< ZeRo.
        'nz':  '!X86_EFL_ZF',   ##< No Zero.

        'af':   'X86_EFL_AF',   ##< Aux Flag.
        'na':  '!X86_EFL_AF',   ##< No Aux.

        'po':   'X86_EFL_PF',   ##< Parity Pdd.
        'pe':  '!X86_EFL_PF',   ##< Parity Even.

        'cf':   'X86_EFL_CF',   ##< Carry Flag.
        'nc':  '!X86_EFL_CF',   ##< No Carry.

        'ei':   'X86_EFL_IF',   ##< Enabled Interrupts.
        'di':  '!X86_EFL_IF',   ##< Disabled Interrupts.

        'dn':   'X86_EFL_DF',   ##< DowN (string op direction).
        'up':  '!X86_EFL_DF',   ##< UP (string op direction).

        'vip':  'X86_EFL_VIP',  ##< Virtual Interrupt Pending.
        'vif':  'X86_EFL_VIF',  ##< Virtual Interrupt Flag.
        'ac':   'X86_EFL_AC',   ##< Alignment Check.
        'vm':   'X86_EFL_VM',   ##< Virtual-8086 Mode.
        'rf':   'X86_EFL_RF',   ##< Resume Flag.
        'nt':   'X86_EFL_NT',   ##< Nested Task.
        'tf':   'X86_EFL_TF',   ##< Trap flag.

        # Reference manual notation:
        'of':   'X86_EFL_OF',
        'sf':   'X86_EFL_SF',
        'zf':   'X86_EFL_ZF',
        'cf':   'X86_EFL_CF',
        'pf':   'X86_EFL_PF',
        'if':   'X86_EFL_IF',
        'df':   'X86_EFL_DF',
        'iopl': 'X86_EFL_IOPL',
        'id':   'X86_EFL_ID',
    };

    ## EFlags tag to Instruction attribute name.
    kdOpFlagToAttr = {
        '@opfltest':    'asFlTest',
        '@opflmodify':  'asFlModify',
        '@opflundef':   'asFlUndefined',
        '@opflset':     'asFlSet',
        '@opflclear':   'asFlClear',
    };

    def parseTagOpEFlags(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tags:   \@opfltest, \@opflmodify, \@opflundef, \@opflset, \@opflclear
        Value:  <eflags specifier>

        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten, split up and validate the values.
        asFlags = self.flattenAllSections(aasSections, sLineSep = ',', sSectionSep = ',').split(',');
        if len(asFlags) == 1 and asFlags[0].lower() == 'none':
            asFlags = [];
        else:
            fRc = True;
            for iFlag, sFlag in enumerate(asFlags):
                if sFlag not in self.kdEFlags:
                    if sFlag.strip() in self.kdEFlags:
                        asFlags[iFlag] = sFlag.strip();
                    else:
                        fRc = self.errorComment(iTagLine, '%s: invalid EFLAGS value: %s' % (sTag, sFlag,));
            if not fRc:
                return False;

        # Set them.
        asOld = getattr(oInstr, self.kdOpFlagToAttr[sTag]);
        if asOld is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite "%s" with "%s"' % ( sTag, asOld, asFlags,));
        setattr(oInstr, self.kdOpFlagToAttr[sTag], asFlags);

        _ = iEndLine;
        return True;

    ## \@ophints values.
    kdHints = {
        'invalid':               'DISOPTYPE_INVALID',                ##<
        'harmless':              'DISOPTYPE_HARMLESS',               ##<
        'controlflow':           'DISOPTYPE_CONTROLFLOW',            ##<
        'potentially_dangerous': 'DISOPTYPE_POTENTIALLY_DANGEROUS',  ##<
        'dangerous':             'DISOPTYPE_DANGEROUS',              ##<
        'portio':                'DISOPTYPE_PORTIO',                 ##<
        'privileged':            'DISOPTYPE_PRIVILEGED',             ##<
        'privileged_notrap':     'DISOPTYPE_PRIVILEGED_NOTRAP',      ##<
        'uncond_controlflow':    'DISOPTYPE_UNCOND_CONTROLFLOW',     ##<
        'relative_controlflow':  'DISOPTYPE_RELATIVE_CONTROLFLOW',   ##<
        'cond_controlflow':      'DISOPTYPE_COND_CONTROLFLOW',       ##<
        'interrupt':             'DISOPTYPE_INTERRUPT',              ##<
        'illegal':               'DISOPTYPE_ILLEGAL',                ##<
        'rrm_dangerous':         'DISOPTYPE_RRM_DANGEROUS',          ##< Some additional dangerous ones when recompiling raw r0. */
        'rrm_dangerous_16':      'DISOPTYPE_RRM_DANGEROUS_16',       ##< Some additional dangerous ones when recompiling 16-bit raw r0. */
        'inhibit_irqs':          'DISOPTYPE_INHIBIT_IRQS',           ##< Will or can inhibit irqs (sti, pop ss, mov ss) */
        'portio_read':           'DISOPTYPE_PORTIO_READ',            ##<
        'portio_write':          'DISOPTYPE_PORTIO_WRITE',           ##<
        'invalid_64':            'DISOPTYPE_INVALID_64',             ##< Invalid in 64 bits mode */
        'only_64':               'DISOPTYPE_ONLY_64',                ##< Only valid in 64 bits mode */
        'default_64_op_size':    'DISOPTYPE_DEFAULT_64_OP_SIZE',     ##< Default 64 bits operand size */
        'forced_64_op_size':     'DISOPTYPE_FORCED_64_OP_SIZE',      ##< Forced 64 bits operand size; regardless of prefix bytes */
        'rexb_extends_opreg':    'DISOPTYPE_REXB_EXTENDS_OPREG',     ##< REX.B extends the register field in the opcode byte */
        'mod_fixed_11':          'DISOPTYPE_MOD_FIXED_11',           ##< modrm.mod is always 11b */
        'forced_32_op_size_x86': 'DISOPTYPE_FORCED_32_OP_SIZE_X86',  ##< Forced 32 bits operand size; regardless of prefix bytes (only in 16 & 32 bits mode!) */
        'sse':                   'DISOPTYPE_SSE',                    ##< SSE,SSE2,SSE3,AVX,++ instruction. Not implemented yet! */
        'mmx':                   'DISOPTYPE_MMX',                    ##< MMX,MMXExt,3DNow,++ instruction. Not implemented yet! */
        'fpu':                   'DISOPTYPE_FPU',                    ##< FPU instruction. Not implemented yet! */
    };

    def parseTagOpHints(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:        \@ophints
        Value:      Comma or space separated list of flags and hints.

        This covers the disassembler flags table and more.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten as a space separated list, split it up and validate the values.
        asHints = self.flattenAllSections(aasSections, sLineSep = ' ', sSectionSep = ' ').replace(',', ' ').split();
        if len(asHints) == 1 and asHints[0].lower() == 'none':
            asHints = [];
        else:
            fRc = True;
            for iHint, sHint in enumerate(asHints):
                if sHint not in self.kdHints:
                    if sHint.strip() in self.kdHints:
                        sHint[iHint] = sHint.strip();
                    else:
                        fRc = self.errorComment(iTagLine, '%s: invalid hint value: %s' % (sTag, sHint,));
            if not fRc:
                return False;

        # Append them.
        for sHint in asHints:
            if sHint not in oInstr.dHints:
                oInstr.dHints[sHint] = True; # (dummy value, using dictionary for speed)
            else:
                self.errorComment(iTagLine, '%s: duplicate hint: %s' % ( sTag, sHint,));

        _ = iEndLine;
        return True;

    ## \@opcpuid
    kdCpuIdFlags =  {
        'vme':      'X86_CPUID_FEATURE_EDX_VME',
        'tsc':      'X86_CPUID_FEATURE_EDX_TSC',
        'msr':      'X86_CPUID_FEATURE_EDX_MSR',
        'cx8':      'X86_CPUID_FEATURE_EDX_CX8',
        'sep':      'X86_CPUID_FEATURE_EDX_SEP',
        'cmov':     'X86_CPUID_FEATURE_EDX_CMOV',
        'clfsh':    'X86_CPUID_FEATURE_EDX_CLFSH',
        'mmx':      'X86_CPUID_FEATURE_EDX_MMX',
        'fxsr':     'X86_CPUID_FEATURE_EDX_FXSR',
        'sse':      'X86_CPUID_FEATURE_EDX_SSE',
        'sse2':     'X86_CPUID_FEATURE_EDX_SSE2',
        'sse3':     'X86_CPUID_FEATURE_ECX_SSE3',
        'pclmul':   'X86_CPUID_FEATURE_ECX_DTES64',
        'monitor':  'X86_CPUID_FEATURE_ECX_CPLDS',
        'vmx':      'X86_CPUID_FEATURE_ECX_VMX',
        'smx':      'X86_CPUID_FEATURE_ECX_TM2',
        'ssse3':    'X86_CPUID_FEATURE_ECX_SSSE3',
        'fma':      'X86_CPUID_FEATURE_ECX_FMA',
        'cx16':     'X86_CPUID_FEATURE_ECX_CX16',
        'pcid':     'X86_CPUID_FEATURE_ECX_PCID',
        'sse41':    'X86_CPUID_FEATURE_ECX_SSE4_1',
        'sse42':    'X86_CPUID_FEATURE_ECX_SSE4_2',
        'movbe':    'X86_CPUID_FEATURE_ECX_MOVBE',
        'popcnt':   'X86_CPUID_FEATURE_ECX_POPCNT',
        'aes':      'X86_CPUID_FEATURE_ECX_AES',
        'xsave':    'X86_CPUID_FEATURE_ECX_XSAVE',
        'avx':      'X86_CPUID_FEATURE_ECX_AVX',
        'f16c':     'X86_CPUID_FEATURE_ECX_F16C',
        'rdrand':   'X86_CPUID_FEATURE_ECX_RDRAND',

        'axmmx':    'X86_CPUID_AMD_FEATURE_EDX_AXMMX',
        '3dnowext': 'X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX',
        '3dnow':    'X86_CPUID_AMD_FEATURE_EDX_3DNOW',
        'svm':      'X86_CPUID_AMD_FEATURE_ECX_SVM',
        'cr8l':     'X86_CPUID_AMD_FEATURE_ECX_CR8L',
        'abm':      'X86_CPUID_AMD_FEATURE_ECX_ABM',
        'sse4a':    'X86_CPUID_AMD_FEATURE_ECX_SSE4A',
        '3dnowprf': 'X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF',
        'xop':      'X86_CPUID_AMD_FEATURE_ECX_XOP',
        'fma4':     'X86_CPUID_AMD_FEATURE_ECX_FMA4',
    };

    def parseTagOpCpuId(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:        \@opcpuid
        Value:      none | <CPUID flag specifier>

        CPUID feature bit which is required for the instruction to be present.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten as a space separated list, split it up and validate the values.
        asCpuIds = self.flattenAllSections(aasSections, sLineSep = ' ', sSectionSep = ' ').replace(',', ' ').split();
        if len(asCpuIds) == 1 and asCpuIds[0].lower() == 'none':
            asCpuIds = [];
        else:
            fRc = True;
            for iCpuId, sCpuId in enumerate(asCpuIds):
                if sCpuId not in self.kdCpuIds:
                    if sCpuId.strip() in self.kdCpuIds:
                        sCpuId[iCpuId] = sCpuId.strip();
                    else:
                        fRc = self.errorComment(iTagLine, '%s: invalid CPUID value: %s' % (sTag, sCpuId,));
            if not fRc:
                return False;

        # Append them.
        for sCpuId in asCpuIds:
            if sCpuId not in oInstr.asCpuIds:
                oInstr.asCpuIds.append(sCpuId);
            else:
                self.errorComment(iTagLine, '%s: duplicate CPUID: %s' % ( sTag, sCpuId,));

        _ = iEndLine;
        return True;

    def parseTagOpGroup(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:        \@opgroup
        Value:      op_grp1[_subgrp2[_subsubgrp3]]

        Instruction grouping.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten as a space separated list, split it up and validate the values.
        asGroups = self.flattenAllSections(aasSections).split();
        if len(asGroups) != 1:
            return self.errorComment(iTagLine, '%s: exactly one group, please: %s' % (sTag, asGroups,));
        sGroup = asGroups[0];
        if not self.oReGroupName.match(sGroup):
            return self.errorComment(iTagLine, '%s: invalid group name: %s (valid: %s)'
                                               % (sTag, sGroup, self.oReGroupName.pattern));

        # Set it.
        if oInstr.sGroup is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite "%s" with "%s"' % ( sTag, oInstr.sGroup, sGroup,));
        oInstr.sGroup = sGroup;

        _ = iEndLine;
        return True;

    ## \@opunused, \@opinvalid, \@opinvlstyle
    kdInvalidStyles = {
        'immediate':                [], ##< CPU stops decoding immediately after the opcode.
        'intel-modrm':              [], ##< Intel decodes ModR/M.
        'intel-modrm-imm8':         [], ##< Intel decodes ModR/M and an 8-byte immediate.
        'intel-opcode-modrm':       [], ##< Intel decodes another opcode byte followed by ModR/M. (Unused extension tables.)
        'intel-opcode-modrm-imm8':  [], ##< Intel decodes another opcode byte followed by ModR/M and an 8-byte immediate.
    };

    def parseTagOpUnusedInvalid(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:    \@opunused, \@opinvalid, \@opinvlstyle
        Value:  <invalid opcode behaviour style>

        The \@opunused indicates the specification is for a currently unused
        instruction encoding.

        The \@opinvalid indicates the specification is for an invalid currently
        instruction encoding (like UD2).

        The \@opinvlstyle just indicates how CPUs decode the instruction when
        not supported (\@opcpuid, \@opmincpu) or disabled.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten as a space separated list, split it up and validate the values.
        asStyles = self.flattenAllSections(aasSections).split();
        if len(asStyles) != 1:
            return self.errorComment(iTagLine, '%s: exactly one invalid behviour style, please: %s' % (sTag, asStyles,));
        sStyle = asStyles[0];
        if sStyle not in self.kdInvalidStyle:
            return self.errorComment(iTagLine, '%s: invalid invalid behviour style: %s (valid: %s)'
                                               % (sTag, sStyle, self.kdInvalidStyles.keys(),));
        # Set it.
        if oInstr.sInvlStyle is not None:
            return self.errorComment(iTagLine,
                                     '%s: attempting to overwrite "%s" with "%s" (only one @opunused, @opinvalid, @opinvlstyle)'
                                     % ( sTag, oInstr.sInvlStyle, sStyle,));
        oInstr.sInvlStyle = sStyle;
        if sTag == '@opunused':
            oInstr.fUnused = True;
        elif sTag == '@opinvalid':
            oInstr.fInvalid = True;

        _ = iEndLine;
        return True;

    def parseTagOpTest(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:        \@optest
        Value:      [<selectors>[ ]?] <inputs> -> <outputs>
        Example:    mode==64bit / in1=0xfffffffe:dw in2=1:dw -> out1=0xffffffff:dw outfl=a?,p?

        The main idea here is to generate basic instruction tests.

        The probably simplest way of handling the diverse input, would be to use
        it to produce size optimized byte code for a simple interpreter that
        modifies the register input and output states.

        An alternative to the interpreter would be creating multiple tables,
        but that becomes rather complicated wrt what goes where and then to use
        them in an efficient manner.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        #
        # Do it section by section.
        #
        for asSectionLines in aasSections:
            #
            # Sort the input into outputs, inputs and selector conditions.
            #
            sFlatSection = self.flattenAllSections([asSectionLines,]);
            if len(sFlatSection) == 0:
                self.errorComment(iTagLine, '%s: missing value' % ( sTag,));
                continue;
            oTest = InstructionTest(oInstr);

            asSelectors = [];
            asInputs    = [];
            asOutputs   = [];
            asCur   = asOutputs;
            fRc     = True;
            asWords = sFlatSection.split();
            for iWord in range(len(asWords) - 1, -1, -1):
                sWord = asWords[iWord];
                # Check for array switchers.
                if sWord == '->':
                    if asCur != asOutputs:
                        fRc = self.errorComment(iTagLine, '%s: "->" shall only occure once: %s' % (sTag, sFlatSection,));
                        break;
                    asCur = asInputs;
                elif sWord == '/':
                    if asCur != asInputs:
                        fRc = self.errorComment(iTagLine, '%s: "/" shall only occure once: %s' % (sTag, sFlatSection,));
                        break;
                    asCur = asSelectors;
                else:
                    asCur.insert(0, sWord);

            #
            # Validate and add selectors.
            #
            for sCond in asSelectors:
                sCondExp = TestSelector.kdPredicates.get(sCond, sCond);
                oSelector = None;
                for sOp in TestSelector.kasCompareOps:
                    off = sCondExp.find(sOp);
                    if off >= 0:
                        sVariable = sCondExp[:off];
                        sValue    = sCondExp[off + len(sOp):];
                        if sVariable in TestSelector.kdVariables:
                            if sValue in TestSelector.kdVariables[sVariable]:
                                oSelector = TestSelector(sVariable, sOp, sValue);
                            else:
                                self.errorComment(iTagLine, '%s: invalid condition value "%s" in "%s" (valid: %s)'
                                                             % ( sTag, sValue, sCond,
                                                                 TestSelector.kdVariables[sVariable].keys(),));
                        else:
                            self.errorComment(iTagLine, '%s: invalid condition variable "%s" in "%s" (valid: %s)'
                                                         % ( sTag, sVariable, sCond, TestSelector.kdVariables.keys(),));
                        break;
                if oSelector is not None:
                    for oExisting in oTest.aoSelectors:
                        if oExisting.sVariable == oSelector.sVariable:
                            self.errorComment(iTagLine, '%s: already have a selector for variable "%s" (existing: %s, new: %s)'
                                                         % ( sTag, oSelector.sVariable, oExisting, oSelector,));
                    oTest.aoSelectors.append(oSelector);
                else:
                    fRc = self.errorComment(iTagLine, '%s: failed to parse selector: %s' % ( sTag, sCond,));

            #
            # Validate outputs and inputs, adding them to the test as we go along.
            #
            for asItems, sDesc, aoDst in [ (asInputs, 'input', oTest.aoInputs), (asOutputs, 'output', oTest.aoOutputs)]:
                for sItem in asItems:
                    oItem = None;
                    for sOp in TestInOut.kasOperators:
                        off = sItem.find(sOp);
                        if off >= 0:
                            sField     = sItem[:off];
                            sValueType = sItem[off + len(sOp):];
                            if sField in TestInOut.kdFields:
                                asSplit = sValueType.split(':', 1);
                                sValue  = asSplit[0];
                                sType   = asSplit[1] if len(asSplit) > 1 else TestInOut.kdFields[sField][0];
                                if sType in TestInOut.kdTypes:
                                    oValid = TestInOut.kdTypes[sType].validate(sValue);
                                    if oValid is True:
                                        if not TestInOut.kdTypes[sType].isAndOrPair(sValue) or sOp == '!=':
                                            oItem = TestInOut(sField, sOp, sValue, sType);
                                        else:
                                            self.errorComment(iTagLine,
                                                              '%s: and-or value "%s" can only be used with the "="'
                                                                        % ( sTag, sDesc, sValue, sItem, sType, ));
                                    else:
                                        self.errorComment(iTagLine, '%s: invalid %s value "%s" in "%s" (type: %s)'
                                                                    % ( sTag, sDesc, sValue, sItem, sType, ));
                                else:
                                    self.errorComment(iTagLine, '%s: invalid %s type "%s" in "%s" (valid types: %s)'
                                                                 % ( sTag, sDesc, sType, sItem, TestInOut.kdTypes.keys(),));
                            else:
                                self.errorComment(iTagLine, '%s: invalid %s field "%s" in "%s" (valid fields: %s)'
                                                             % ( sTag, sDesc, sField, sItem, TestInOut.kdFields.keys(),));
                            break;
                    if oItem is not None:
                        for oExisting in aoDst:
                            if oExisting.sField == oItem.sField and oExisting.sOp == oItem.sOp:
                                self.errorComment(iTagLine,
                                                  '%s: already have a "%s" assignment for field "%s" (existing: %s, new: %s)'
                                                  % ( sTag, oItem.sOp, oItem.sField, oExisting, oItem,));
                        aoDst.append(oItem);
                    else:
                        fRc = self.errorComment(iTagLine, '%s: failed to parse assignment: %s' % ( sTag, sItem,));

            #
            # .
            #
            if fRc:
                oInstr.aoTests.append(oTest);
            else:
                self.errorComment(iTagLine, '%s: failed to parse test: %s' % (sTag, ' '.join(asWords),));
                self.errorComment(iTagLine, '%s: asSelectors=%s / asInputs=%s -> asOutputs=%s'
                                            % (sTag, asSelectors, asInputs, asOutputs,));

        return True;

    def parseTagOpFunction(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:        \@opfunction
        Value:      <VMM function name>

        This is for explicitly setting the IEM function name.  Normally we pick
        this up from the FNIEMOP_XXX macro invocation after the description, or
        generate it from the mnemonic and operands.

        It it thought it maybe necessary to set it when specifying instructions
        which implementation isn't following immediately or aren't implemented yet.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten and validate the value.
        sFunction = self.flattenAllSections(aasSections);
        if not self.oReFunctionName.match(sFunction):
            return self.errorComment(iTagLine, '%s: invalid VMM function name: "%s" (valid: %s)'
                                               % (sTag, Name, self.oReFunctionName.pattern));

        if oInstr.sFunction is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite VMM function name "%s" with "%s"'
                                     % (sTag, oInstr.sStats, sStats,));
        oInstr.sFunction = sFunction;

        _ = iEndLine;
        return True;

    def parseTagOpStats(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:        \@opstats
        Value:      <VMM statistics base name>

        This is for explicitly setting the statistics name.  Normally we pick
        this up from the IEMOP_MNEMONIC macro invocation, or generate it from
        the mnemonic and operands.

        It it thought it maybe necessary to set it when specifying instructions
        which implementation isn't following immediately or aren't implemented yet.
        """
        oInstr = self.ensureInstructionForOpTag(iTagLine);

        # Flatten and validate the value.
        sStats = self.flattenAllSections(aasSections);
        if not self.oReStatsName.match(sStats):
            return self.errorComment(iTagLine, '%s: invalid VMM statistics name: "%s" (valid: %s)'
                                               % (sTag, Name, self.oReStatsName.pattern));

        if oInstr.sStats is not None:
            return self.errorComment(iTagLine, '%s: attempting to overwrite VMM statistics base name "%s" with "%s"'
                                     % (sTag, oInstr.sStats, sStats,));
        oInstr.sStats = sStats;

        _ = iEndLine;
        return True;

    def parseTagOpDone(self, sTag, aasSections, iTagLine, iEndLine):
        """
        Tag:    \@opdone
        Value:  none

        Used to explictily flush the instructions that have been specified.
        """
        sFlattened = self.flattenAllSections(aasSections);
        if sFlattened != '':
            return self.errorComment(iTagLine, '%s: takes no value, found: "%s"' % (sTag, sFlattened,));
        _ = sTag; _ = iEndLine;
        return self.doneInstructions();

    ## @}


    def parseComment(self):
        """
        Parse the current comment (self.sComment).

        If it's a opcode specifiying comment, we reset the macro stuff.
        """
        #
        # Reject if comment doesn't seem to contain anything interesting.
        #
        if    self.sComment.find('Opcode') < 0 \
          and self.sComment.find('@') < 0:
            return False;

        #
        # Split the comment into lines, removing leading asterisks and spaces.
        # Also remove leading and trailing empty lines.
        #
        asLines = self.sComment.split('\n');
        for iLine, sLine in enumerate(asLines):
            asLines[iLine] = sLine.lstrip().lstrip('*').lstrip();

        while len(asLines) > 0 and len(asLines[0]) == 0:
            self.iCommentLine += 1;
            asLines.pop(0);

        while len(asLines) > 0 and len(asLines[-1]) == 0:
            asLines.pop(len(asLines) - 1);

        #
        # Check for old style: Opcode 0x0f 0x12
        #
        if asLines[0].startswith('Opcode '):
            self.parseCommentOldOpcode(asLines);

        #
        # Look for @op* tagged data.
        #
        cOpTags      = 0;
        sFlatDefault = None;
        sCurTag      = '@default';
        iCurTagLine  = 0;
        asCurSection = [];
        aasSections  = [ asCurSection, ];
        for iLine, sLine in enumerate(asLines):
            if not sLine.startswith('@'):
                if len(sLine) > 0:
                    asCurSection.append(sLine);
                elif len(asCurSection) != 0:
                    asCurSection = [];
                    aasSections.append(asCurSection);
            else:
                #
                # Process the previous tag.
                #
                if sCurTag in self.dTagHandlers:
                    self.dTagHandlers[sCurTag](sCurTag, aasSections, iCurTagLine, iLine);
                    cOpTags += 1;
                elif sCurTag.startswith('@op'):
                    self.errorComment(iCurTagLine, 'Unknown tag: %s' % (sCurTag));
                elif sCurTag == '@default':
                    sFlatDefault = self.flattenAllSections(aasSections);

                #
                # New tag.
                #
                asSplit = sLine.split(None, 1);
                sCurTag = asSplit[0].lower();
                if len(asSplit) > 1:
                    asCurSection = [asSplit[1],];
                else:
                    asCurSection = [];
                aasSections = [asCurSection, ];
                iCurTagLine = iLine;

        #
        # Process the final tag.
        #
        if sCurTag in self.dTagHandlers:
            self.dTagHandlers[sCurTag](sCurTag, aasSections, iCurTagLine, iLine);
            cOpTags += 1;
        elif sCurTag.startswith('@op'):
            self.errorComment(iCurTagLine, 'Unknown tag: %s' % (sCurTag));
        elif sCurTag == '@default':
            sFlatDefault = self.flattenAllSections(aasSections);

        #
        # Don't allow default text in blocks containing @op*.
        #
        if cOpTags > 0 and len(sFlatDefault) > 0:
            self.errorComment(0, 'Untagged comment text is not allowed with @op*: %s' % (sFlatDefault,));

        return True;

    def parseMacroInvocation(self, sInvocation):
        """
        Parses a macro invocation.

        Returns a tuple, first element is the offset following the macro
        invocation. The second element is a list of macro arguments, where the
        zero'th is the macro name.
        """
        # First the name.
        offOpen = sInvocation.find('(');
        if offOpen <= 0:
            raiseError("macro invocation open parenthesis not found");
        sName = sInvocation[:offOpen].strip();
        if not self.oReMacroName.match(sName):
            return self.error("invalid macro name '%s'" % (sName,));
        asRet = [sName, ];

        # Arguments.
        iLine    = self.iLine;
        cDepth   = 1;
        off      = offOpen + 1;
        offStart = off;
        while cDepth > 0:
            if off >= len(sInvocation):
                if iLine >= len(self.asLines):
                    return self.error('macro invocation beyond end of file');
                sInvocation += self.asLines[iLine];
                iLine += 1;
            ch = sInvocation[off];

            if ch == ',' or ch == ')':
                if cDepth == 1:
                    asRet.append(sInvocation[offStart:off].strip());
                    offStart = off + 1;
                if ch == ')':
                    cDepth -= 1;
            elif ch == '(':
                cDepth += 1;
            off += 1;

        return (off, asRet);

    def findAndParseMacroInvocation(self, sCode, sMacro):
        """
        Returns (len(sCode), None) if not found, parseMacroInvocation result if found.
        """
        offHit = sCode.find(sMacro);
        if offHit >= 0 and sCode[offHit + len(sMacro):].strip()[0] == '(':
            offAfter, asRet = self.parseMacroInvocation(sCode[offHit:])
            return (offHit + offAfter, asRet);
        return (len(sCode), None);

    def findAndParseFirstMacroInvocation(self, sCode, asMacro):
        """
        Returns (len(sCode), None) if not found, parseMacroInvocation result if found.
        """
        for sMacro in asMacro:
            offAfter, asRet = self.findAndParseMacroInvocation(sCode, sMacro);
            if asRet is not None:
                return (offAfter, asRet);
        return (len(sCode), None);

    def checkCodeForMacro(self, sCode):
        """
        Checks code for relevant macro invocation.
        """
        #
        # Scan macro invocations.
        #
        if sCode.find('(') > 0:
            # Look for instruction decoder function definitions. ASSUME single line.
            (_, asArgs) = self.findAndParseFirstMacroInvocation(sCode,
                                                                [ 'FNIEMOP_DEF',
                                                                  'FNIEMOP_STUB',
                                                                  'FNIEMOP_STUB_1',
                                                                  'FNIEMOP_UD_STUB',
                                                                  'FNIEMOP_UD_STUB_1' ]);
            if asArgs is not None:
                sFunction = asArgs[1];

                if len(self.asCurInstr) == 0:
                    self.addInstruction().sMnemonic = sFunction.split('_')[1];
                self.setInstrunctionAttrib('sFunction', sFunction);
                self.setInstrunctionAttrib('fStub', asArgs[0].find('STUB') > 0, fOverwrite = True);
                self.setInstrunctionAttrib('fUdStub', asArgs[0].find('UD_STUB') > 0, fOverwrite = True);
                if asArgs[0].find('STUB') > 0:
                    self.doneInstructions();
                return True;

            # IEMOP_MNEMONIC(a_Stats, a_szMnemonic) IEMOP_INC_STATS(a_Stats)
            (_, asArgs) = self.findAndParseMacroInvocation(sCode, 'IEMOP_MNEMONIC');
            if asArgs is not None:
                if len(self.asCurInstr) == 1:
                    self.setInstrunctionAttrib('sStats', asArgs[1]);
                    self.setInstrunctionAttrib('sMnemonic', asArgs[1].split('_')[0]);

            # IEMOP_HLP_DECODED_NL_1(a_uDisOpNo, a_fIemOpFlags, a_uDisParam0, a_fDisOpType)
            (_, asArgs) = self.findAndParseMacroInvocation(sCode, 'IEMOP_HLP_DECODED_NL_1');
            if asArgs is not None:
                if len(self.asCurInstr) == 1:
                    self.setInstrunctionAttrib('sRawDisOpNo', asArgs[1]);
                    self.setInstrunctionAttrib('sRawIemOpFlags', asArgs[2]);
                    self.setInstrunctionArrayAttrib('asRawDisParams', 0, asArgs[3]);

            # IEMOP_HLP_DECODED_NL_2(a_uDisOpNo, a_fIemOpFlags, a_uDisParam0, a_uDisParam1, a_fDisOpType)
            (_, asArgs) = self.findAndParseMacroInvocation(sCode, 'IEMOP_HLP_DECODED_NL_2');
            if asArgs is not None:
                if len(self.asCurInstr) == 1:
                    self.setInstrunctionAttrib('sRawDisOpNo', asArgs[1]);
                    self.setInstrunctionAttrib('sRawIemOpFlags', asArgs[2]);
                    self.setInstrunctionArrayAttrib('asRawDisParams', 0, asArgs[3]);
                    self.setInstrunctionArrayAttrib('asRawDisParams', 1, asArgs[4]);

        return False;


    def parse(self):
        """
        Parses the given file.
        Returns number or errors.
        Raises exception on fatal trouble.
        """
        self.debug('Parsing %s' % (self.sSrcFile,));

        while self.iLine < len(self.asLines):
            sLine = self.asLines[self.iLine];
            self.iLine  += 1;

            # We only look for comments, so only lines with a slash might possibly
            # influence the parser state.
            if sLine.find('/') >= 0:
                #self.debug('line %d: slash' % (self.iLine,));

                offLine = 0;
                while offLine < len(sLine):
                    if self.iState == self.kiCode:
                        offHit = sLine.find('/*', offLine); # only multiline comments for now.
                        if offHit >= 0:
                            self.sComment     = '';
                            self.iCommentLine = self.iLine;
                            self.iState       = self.kiCommentMulti;
                            offLine = offHit + 2;
                        else:
                            offLine = len(sLine);

                    elif self.iState == self.kiCommentMulti:
                        offHit = sLine.find('*/', offLine);
                        if offHit >= 0:
                            self.sComment += sLine[offLine:offHit];
                            self.iState    = self.kiCode;
                            offLine = offHit + 2;
                            self.parseComment();
                        else:
                            self.sComment += sLine[offLine:];
                            offLine = len(sLine);
                    else:
                        assert False;

            # No slash, but append the line if in multi-line comment.
            elif self.iState == self.kiCommentMulti:
                #self.debug('line %d: multi' % (self.iLine,));
                self.sComment += sLine;

            # No slash, but check code line for relevant macro.
            elif self.iState == self.kiCode and sLine.find('IEMOP_') >= 0:
                #self.debug('line %d: macro' % (self.iLine,));
                self.checkCodeForMacro(sLine);

            # If the line is a '}' in the first position, complete the instructions.
            elif self.iState == self.kiCode and sLine[0] == '}':
                #self.debug('line %d: }' % (self.iLine,));
                self.doneInstructions();

        self.doneInstructions();
        self.debug('%s instructions in %s' % (self.cTotalInstr, self.sSrcFile,));
        self.debug('%s instruction stubs' % (self.cTotalStubs,));
        return self.printErrors();


def __parseFileByName(sSrcFile, sDefaultMap):
    """
    Parses one source file for instruction specfications.
    """
    #
    # Read sSrcFile into a line array.
    #
    try:
        oFile = open(sSrcFile, "r");
    except Exception as oXcpt:
        raise Exception("failed to open %s for reading: %s" % (sSrcFile, oXcpt,));
    try:
        asLines = oFile.readlines();
    except Exception as oXcpt:
        raise Exception("failed to read %s: %s" % (sSrcFile, oXcpt,));
    finally:
        oFile.close();

    #
    # Do the parsing.
    #
    try:
        cErrors = SimpleParser(sSrcFile, asLines, sDefaultMap).parse();
    except ParserException as oXcpt:
        print(str(oXcpt));
        raise;
    except Exception as oXcpt:
        raise;

    return cErrors;


def __parseAll():
    """
    Parses all the IEMAllInstruction*.cpp.h files.

    Raises exception on failure.
    """
    sSrcDir = os.path.dirname(os.path.abspath(__file__));
    cErrors = 0;
    for sDefaultMap, sName in [
        ( 'one',    'IEMAllInstructionsOneByte.cpp.h'),
        #( 'two0f',  'IEMAllInstructionsTwoByte0f.cpp.h'),
    ]:
        cErrors += __parseFileByName(os.path.join(sSrcDir, sName), sDefaultMap);

    if cErrors != 0:
        raise Exception('%d parse errors' % (cErrors,));
    return True;



__parseAll();


