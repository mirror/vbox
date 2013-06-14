#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id$

"""
Instruction Test Generator.
"""

from __future__ import print_function;

__copyright__ = \
"""
Copyright (C) 2012-2013 Oracle Corporation

Oracle Corporation confidential
All rights reserved
"""
__version__ = "$Revision$";


# Standard python imports.
import io;
import os;
from optparse import OptionParser
import random;
import sys;


## @name Exit codes
## @{
RTEXITCODE_SUCCESS = 0;
RTEXITCODE_SYNTAX  = 2;
## @}

## @name Various C macros we're used to.
## @{
UINT8_MAX  = 0xff
UINT16_MAX = 0xffff
UINT32_MAX = 0xffffffff
UINT64_MAX = 0xffffffffffffffff
def RT_BIT_32(iBit): # pylint: disable=C0103
    """ 32-bit one bit mask. """
    return 1 << iBit;
def RT_BIT_64(iBit): # pylint: disable=C0103
    """ 64-bit one bit mask. """
    return 1 << iBit;
## @}


## @name ModR/M
## @{
X86_MODRM_RM_MASK   = 0x07;
X86_MODRM_REG_MASK  = 0x38;
X86_MODRM_REG_SMASK = 0x07;
X86_MODRM_REG_SHIFT = 3;
X86_MODRM_MOD_MASK  = 0xc0;
X86_MODRM_MOD_SMASK = 0x03;
X86_MODRM_MOD_SHIFT = 6;
## @}

## @name SIB
## @{
X86_SIB_BASE_MASK   = 0x07;
X86_SIB_INDEX_MASK  = 0x38;
X86_SIB_INDEX_SMASK = 0x07;
X86_SIB_INDEX_SHIFT = 3;
X86_SIB_SCALE_MASK  = 0xc0;
X86_SIB_SCALE_SMASK = 0x03;
X86_SIB_SCALE_SHIFT = 6;
## @}

## @name PRefixes
## @
X86_OP_PRF_CS       = 0x2e;
X86_OP_PRF_SS       = 0x36;
X86_OP_PRF_DS       = 0x3e;
X86_OP_PRF_ES       = 0x26;
X86_OP_PRF_FS       = 0x64;
X86_OP_PRF_GS       = 0x65;
X86_OP_PRF_SIZE_OP  = 0x66;
X86_OP_PRF_SIZE_ADDR = 0x67;
X86_OP_PRF_LOCK     = 0xf0;
X86_OP_PRF_REPZ     = 0xf2;
X86_OP_PRF_REPNZ    = 0xf3;
X86_OP_REX_B        = 0x41;
X86_OP_REX_X        = 0x42;
X86_OP_REX_R        = 0x44;
X86_OP_REX_W        = 0x48;
## @}


## @name Register names.
## @{
g_asGRegs64NoSp = ('rax', 'rcx', 'rdx', 'rbx', None,  'rbp', 'rsi', 'rdi', 'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15');
g_asGRegs64     = ('rax', 'rcx', 'rdx', 'rbx', 'rsp', 'rbp', 'rsi', 'rdi', 'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15');
g_asGRegs32NoSp = ('eax', 'ecx', 'edx', 'ebx', None,  'ebp', 'esi', 'edi',
                   'r8d', 'r9d', 'r10d', 'r11d', 'r12d', 'r13d', 'r14d', 'r15d');
g_asGRegs32     = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi',
                   'r8d', 'r9d', 'r10d', 'r11d', 'r12d', 'r13d', 'r14d', 'r15d');
g_asGRegs16NoSp = ('ax',  'cx',  'dx',  'bx',  None,  'bp',  'si',  'di',
                   'r8w', 'r9w', 'r10w', 'r11w', 'r12w', 'r13w', 'r14w', 'r15w');
g_asGRegs16     = ('ax',  'cx',  'dx',  'bx',  'sp',  'bp',  'si',  'di',
                   'r8w', 'r9w', 'r10w', 'r11w', 'r12w', 'r13w', 'r14w', 'r15w');
g_asGRegs8      = ('al',  'cl',  'dl',  'bl',  'ah',  'ah',  'dh',  'bh');
g_asGRegs8_64   = ('al',  'cl',  'dl',  'bl',  'spl', 'bpl', 'sil',  'dil',        # pylint: disable=C0103
                   'r8b', 'r9b', 'r10b', 'r11b', 'r12b', 'r13b', 'r14b', 'r15b');
## @}


## @name Random
## @{
g_oMyRand = random.SystemRandom()
def randU16():
    """ Unsigned 16-bit random number. """
    return g_oMyRand.getrandbits(16);

def randU32():
    """ Unsigned 32-bit random number. """
    return g_oMyRand.getrandbits(32);

def randU64():
    """ Unsigned 64-bit random number. """
    return g_oMyRand.getrandbits(64);

def randUxx(cBits):
    """ Unsigned 8-, 16-, 32-, or 64-bit random number. """
    return g_oMyRand.getrandbits(cBits);

def randUxxList(cBits, cElements):
    """ List of nsigned 8-, 16-, 32-, or 64-bit random numbers. """
    return [randUxx(cBits) for _ in range(cElements)];
## @}



## @name Instruction Emitter Helpers
## @{

def calcRexPrefixForTwoModRmRegs(iReg, iRm, bOtherRexPrefixes = 0):
    """
    Calculates a rex prefix if neccessary given the two registers
    and optional rex size prefixes.
    Returns an empty array if not necessary.
    """
    bRex = bOtherRexPrefixes;
    if iReg >= 8:
        bRex = bRex | X86_OP_REX_R;
    if iRm >= 8:
        bRex = bRex | X86_OP_REX_B;
    if bRex == 0:
        return [];
    return [bRex,];

def calcModRmForTwoRegs(iReg, iRm):
    """
    Calculate the RM byte for two registers.
    Returns an array with one byte in it.
    """
    bRm = (0x3   << X86_MODRM_MOD_SHIFT) \
        | ((iReg << X86_MODRM_REG_SHIFT) & X86_MODRM_REG_MASK) \
        | (iRm   &  X86_MODRM_RM_MASK);
    return [bRm,];

## @}


class TargetEnv(object):
    """
    Target Runtime Environment.
    """

    ## @name CPU Modes
    ## @{
    ksCpuMode_Real      = 'real';
    ksCpuMode_Protect   = 'prot';
    ksCpuMode_Paged     = 'paged';
    ksCpuMode_Long      = 'long';
    ksCpuMode_V86       = 'v86';
    ## @}

    ## @name Instruction set.
    ## @{
    ksInstrSet_16       = '16';
    ksInstrSet_32       = '32';
    ksInstrSet_64       = '64';
    ## @}

    def __init__(self, sName,
                 sInstrSet = ksInstrSet_32,
                 sCpuMode = ksCpuMode_Paged,
                 iRing = 3,
                 ):
        self.sName          = sName;
        self.sInstrSet      = sInstrSet;
        self.sCpuMode       = sCpuMode;
        self.iRing          = iRing;
        self.asGRegs        = g_asGRegs64     if self.is64Bit() else g_asGRegs32;
        self.asGRegsNoSp    = g_asGRegs64NoSp if self.is64Bit() else g_asGRegs32NoSp;

    def isUsingIprt(self):
        """ Whether it's an IPRT environment or not. """
        return self.sName.startswith('iprt');

    def is64Bit(self):
        """ Whether it's a 64-bit environment or not. """
        return self.sInstrSet == self.ksInstrSet_64;

    def getDefOpBits(self):
        """ Get the default operand size as a bit count. """
        if self.sInstrSet == self.ksInstrSet_16:
            return 16;
        return 32;

    def getDefOpBytes(self):
        """ Get the default operand size as a byte count. """
        return self.getDefOpBits() / 8;

    def getMaxOpBits(self):
        """ Get the max operand size as a bit count. """
        if self.sInstrSet == self.ksInstrSet_64:
            return 64;
        return 32;

    def getMaxOpBytes(self):
        """ Get the max operand size as a byte count. """
        return self.getMaxOpBits() / 8;

    def getGRegCount(self):
        """ Get the number of general registers. """
        if self.sInstrSet == self.ksInstrSet_64:
            return 16;
        return 8;



## Target environments.
g_dTargetEnvs = {
    'iprt-r3-32':   TargetEnv('iprt-r3-32', TargetEnv.ksInstrSet_32, TargetEnv.ksCpuMode_Protect, 3),
    'iprt-r3-64':   TargetEnv('iprt-r3-64', TargetEnv.ksInstrSet_64, TargetEnv.ksCpuMode_Long,    3),
};


class InstrTestBase(object): # pylint: disable=R0903
    """
    Base class for testing one instruction.
    """

    def __init__(self, sName, sInstr = None):
        self.sName = sName;
        self.sInstr = sInstr if sInstr else sName.split()[0];

    def isApplicable(self, oGen):
        """
        Tests if the instruction test is applicable to the selected environment.
        """
        _ = oGen;
        return True;

    def generateTest(self, oGen, sTestFnName):
        """
        Emits the test assembly code.
        """
        oGen.write(';; @todo not implemented. This is for the linter: %s, %s\n' % (oGen, sTestFnName));
        return True;


class InstrTest_MemOrGreg_2_Greg(InstrTestBase):
    """
    Instruction reading memory or general register and writing the result to a
    general register.
    """

    def __init__(self, sName, fnCalcResult, sInstr = None, acbOpVars = None):
        InstrTestBase.__init__(self, sName, sInstr);
        self.fnCalcResult = fnCalcResult;
        self.acbOpVars = [ 1, 2, 4, 8 ] if not acbOpVars else list(acbOpVars);

    def generateInputs(self, cbEffOp, cbMaxOp):
        """ Generate a list of inputs. """
        # Fixed ranges.
        auRet = [0, 1, ];
        if cbEffOp == 1:
            auRet += [ UINT8_MAX  / 2, UINT8_MAX  / 2 + 1, UINT8_MAX  ];
        elif cbEffOp == 2:
            auRet += [ UINT16_MAX / 2, UINT16_MAX / 2 + 1, UINT16_MAX ];
        elif cbEffOp == 4:
            auRet += [ UINT32_MAX / 2, UINT32_MAX / 2 + 1, UINT32_MAX ];
        elif cbEffOp == 8:
            auRet += [ UINT64_MAX / 2, UINT64_MAX / 2 + 1, UINT64_MAX ];
        else:
            assert False;

        # Append some random values as well.
        auRet += randUxxList(cbEffOp * 8, 4);
        auRet += randUxxList(cbMaxOp * 8, 4);

        return auRet;

    def writeInstrGregGreg(self, cbEffOp, iOp1, iOp2, oGen):
        """ Writes the instruction with two general registers as operands. """
        if cbEffOp == 8:
            iOp2 = iOp2 % len(g_asGRegs32);
            oGen.write('        %s %s, %s\n' % (self.sInstr, g_asGRegs64[iOp1], g_asGRegs32[iOp2]));
        elif cbEffOp == 4:
            oGen.write('        %s %s, %s\n' % (self.sInstr, g_asGRegs32[iOp1], g_asGRegs16[iOp2]));
        elif cbEffOp == 2:
            oGen.write('        %s %s, %s\n' % (self.sInstr, g_asGRegs16[iOp1], g_asGRegs8[iOp2]));
        else:
            assert False;
        return True;

    def generateOneStdTest(self, oGen, cbEffOp, cbMaxOp, iOp1, iOp2, uInput, uResult):
        """ Generate one standard test. """
        oGen.write('        call VBINSTST_NAME(Common_LoadKnownValues)\n');
        oGen.write('        mov     %s, 0x%x\n' % (oGen.oTarget.asGRegs[iOp2], uInput,));
        oGen.write('        push    %s\n' % (oGen.oTarget.asGRegs[iOp2],));
        self.writeInstrGregGreg(cbEffOp, iOp1, iOp2, oGen);
        oGen.pushConst(uResult, cbMaxOp);
        oGen.write('        call VBINSTST_NAME(%s)\n' % (oGen.needGRegChecker(iOp1, iOp2),));
        return True;

    def generateStandardTests(self, oGen):
        """ Generate standard tests. """

        cbDefOp  = oGen.oTarget.getDefOpBytes();
        cbMaxOp  = oGen.oTarget.getMaxOpBytes();
        auInputs = self.generateInputs(cbDefOp, cbMaxOp);


        for cbEffOp in self.acbOpVars:
            if cbEffOp > cbMaxOp:
                continue;
            for iOp1 in range(oGen.oTarget.getGRegCount()):
                if oGen.oTarget.asGRegsNoSp[iOp1] is None:
                    continue;
                for iOp2 in range(oGen.oTarget.getGRegCount()):
                    if oGen.oTarget.asGRegsNoSp[iOp2] is None:
                        continue;
                    for uInput in auInputs:
                        uResult = self.fnCalcResult(cbEffOp, uInput, oGen.auRegValues[iOp1] if iOp1 != iOp2 else uInput, oGen);
                        oGen.newSubTest();
                        self.generateOneStdTest(oGen, cbEffOp, cbMaxOp, iOp1, iOp2, uInput, uResult);
        return True;

    def generateTest(self, oGen, sTestFnName):
        oGen.write('VBINSTST_BEGINPROC %s\n' % (sTestFnName,));
        #oGen.write('        int3\n');

        self.generateStandardTests(oGen);

        #oGen.write('        int3\n');
        oGen.write('        ret\n');
        oGen.write('VBINSTST_ENDPROC   %s\n' % (sTestFnName,));
        return True;


class InstrTest_MovSxD_Gv_Ev(InstrTest_MemOrGreg_2_Greg):
    """
    Tests MOVSXD Gv,Ev.
    """
    def __init__(self):
        InstrTest_MemOrGreg_2_Greg.__init__(self, 'movsxd Gv,Ev', self.calc_movsxd, acbOpVars = [ 8, 4, 2, ]);

    def writeInstrGregGreg(self, cbEffOp, iOp1, iOp2, oGen):
        if cbEffOp == 8:
            oGen.write('        %s %s, %s\n' % (self.sInstr, g_asGRegs64[iOp1], g_asGRegs32[iOp2]));
        elif cbEffOp == 4 or cbEffOp == 2:
            abInstr = [];
            if cbEffOp != oGen.oTarget.getDefOpBytes():
                abInstr.append(X86_OP_PRF_SIZE_OP);
            abInstr += calcRexPrefixForTwoModRmRegs(iOp1, iOp2);
            abInstr.append(0x63);
            abInstr += calcModRmForTwoRegs(iOp1, iOp2);
            oGen.writeInstrBytes(abInstr);
        else:
            assert False;
            assert False;
        return True;

    def isApplicable(self, oGen):
        return oGen.oTarget.is64Bit();

    @staticmethod
    def calc_movsxd(cbEffOp, uInput, uCur, oGen):
        """
        Calculates the result of a movxsd instruction.
        Returns the result value (cbMaxOp sized).
        """
        _ = oGen;
        if cbEffOp == 8 and (uInput & RT_BIT_32(31)):
            return (UINT32_MAX << 32) | (uInput & UINT32_MAX);
        if cbEffOp == 2:
            return (uCur & 0xffffffffffff0000) | (uInput & 0xffff);
        return uInput & UINT32_MAX;


## Instruction Tests.
g_aoInstructionTests = [
    InstrTest_MovSxD_Gv_Ev(),
];


class InstructionTestGen(object):
    """
    Instruction Test Generator.
    """


    def __init__(self, oOptions):
        self.oOptions = oOptions;
        self.oTarget  = g_dTargetEnvs[oOptions.sTargetEnv];

        # Calculate the number of output files.
        self.cFiles  = 1;
        if len(g_aoInstructionTests) > self.oOptions.cInstrPerFile:
            self.cFiles = len(g_aoInstructionTests) / self.oOptions.cInstrPerFile;
            if self.cFiles * self.oOptions.cInstrPerFile < len(g_aoInstructionTests):
                self.cFiles += 1;

        # Fix the known register values.
        self.au64Regs       = randUxxList(64, 16);
        self.au32Regs       = [(self.au64Regs[i] & UINT32_MAX) for i in range(8)];
        self.au16Regs       = [(self.au64Regs[i] & UINT16_MAX) for i in range(8)];
        self.auRegValues    = self.au64Regs if self.oTarget.is64Bit() else self.au32Regs;

        # Declare state variables used while generating.
        self.oFile          = sys.stderr;
        self.iFile          = -1;
        self.sFile          = '';
        self.dCheckFns      = dict();
        self.d64BitConsts   = dict();


    #
    # Methods used by instruction tests.
    #

    def write(self, sText):
        """ Writes to the current output file. """
        return self.oFile.write(unicode(sText));

    def writeln(self, sText):
        """ Writes a line to the current output file. """
        self.write(sText);
        return self.write('\n');

    def writeInstrBytes(self, abInstr):
        """
        Emits an instruction given as a sequence of bytes values.
        """
        self.write('        db %#04x' % (abInstr[0],));
        for i in range(1, len(abInstr)):
            self.write(', %#04x' % (abInstr[i],));
        return self.write('\n');

    def newSubTest(self):
        """
        Indicates that a new subtest has started.
        """
        self.write('        mov     dword [VBINSTST_NAME(g_uVBInsTstSubTestIndicator) xWrtRIP], __LINE__\n');
        return True;

    def needGRegChecker(self, iReg1, iReg2):
        """
        Records the need for a given register checker function, returning its label.
        """
        assert iReg1 < 32; assert iReg2 < 32;
        iRegs = iReg1 + iReg2 * 32;
        if iRegs in self.dCheckFns:
            self.dCheckFns[iRegs] += 1;
        else:
            self.dCheckFns[iRegs]  = 1;
        return 'Common_Check_%s_%s' % (self.oTarget.asGRegs[iReg1], self.oTarget.asGRegs[iReg2]);

    def need64BitConstant(self, uVal):
        """
        Records the need for a 64-bit constant, returning its label.
        These constants are pooled to attempt reduce the size of the whole thing.
        """
        assert uVal >= 0 and uVal <= UINT64_MAX;
        if uVal in self.d64BitConsts:
            self.d64BitConsts[uVal] += 1;
        else:
            self.d64BitConsts[uVal]  = 1;
        return 'g_u64Const_0x%016x' % (uVal, );

    def pushConst(self, uResult, cbMaxOp):
        """
        Emits a push constant value, taking care of high values on 64-bit hosts.
        """
        if cbMaxOp == 8 and uResult >= 0x80000000:
            self.write('        push    qword [%s wrt rip]\n' % (self.need64BitConstant(uResult),));
        else:
            self.write('        push    dword 0x%x\n' % (uResult,));
        return True;


    #
    # Internal machinery.
    #

    def _calcTestFunctionName(self, oInstrTest, iInstrTest):
        """
        Calc a test function name for the given instruction test.
        """
        sName = 'TestInstr%03u_%s' % (iInstrTest, oInstrTest.sName);
        return sName.replace(',', '_').replace(' ', '_').replace('%', '_');

    def _generateFileHeader(self, ):
        """
        Writes the file header.
        Raises exception on trouble.
        """
        self.write('; $Id$\n'
                   ';; @file %s\n'
                   '; Autogenerate by %s %s. DO NOT EDIT\n'
                   ';\n'
                   '\n'
                   ';\n'
                   '; Headers\n'
                   ';\n'
                   '%%include "env-%s.mac"\n'
                   % ( os.path.basename(self.sFile),
                       os.path.basename(__file__), __version__[11:-1],
                       self.oTarget.sName,
                   ) );
        # Target environment specific init stuff.

        #
        # Global variables.
        #
        self.write('\n\n'
                   ';\n'
                   '; Globals\n'
                   ';\n');
        self.write('VBINSTST_BEGINDATA\n'
                   'VBINSTST_GLOBALNAME_EX g_uVBInsTstSubTestIndicator, data hidden\n'
                   '        dd      0\n'
                   'VBINSTST_BEGINCODE\n'
                   );
        self.write('%ifdef RT_ARCH_AMD64\n');
        for i in range(len(g_asGRegs64)):
            self.write('g_u64KnownValue_%s: dq 0x%x\n' % (g_asGRegs64[i], self.au64Regs[i]));
        self.write('%endif\n\n')

        #
        # Common functions.
        #

        # Loading common values.
        self.write('\n\n'
                   'VBINSTST_BEGINPROC Common_LoadKnownValues\n'
                   '%ifdef RT_ARCH_AMD64\n');
        for i in range(len(g_asGRegs64NoSp)):
            if g_asGRegs64NoSp[i]:
                self.write('        mov     %s, 0x%x\n' % (g_asGRegs64NoSp[i], self.au64Regs[i],));
        self.write('%else\n');
        for i in range(8):
            if g_asGRegs32NoSp[i]:
                self.write('        mov     %s, 0x%x\n' % (g_asGRegs32NoSp[i], self.au32Regs[i],));
        self.write('%endif\n'
                   '        ret\n'
                   'VBINSTST_ENDPROC   Common_LoadKnownValues\n'
                   '\n');

        self.write('VBINSTST_BEGINPROC Common_CheckKnownValues\n'
                   '%ifdef RT_ARCH_AMD64\n');
        for i in range(len(g_asGRegs64NoSp)):
            if g_asGRegs64NoSp[i]:
                self.write('        cmp     %s, [g_u64KnownValue_%s wrt rip]\n'
                           '        je      .ok_%u\n'
                           '        push    %u         ; register number\n'
                           '        push    %s         ; actual\n'
                           '        push    qword [g_u64KnownValue_%s wrt rip] ; expected\n'
                           '        call    VBINSTST_NAME(Common_BadValue)\n'
                           '.ok_%u:\n'
                           % ( g_asGRegs64NoSp[i], g_asGRegs64NoSp[i], i, i, g_asGRegs64NoSp[i], g_asGRegs64NoSp[i], i,));
        self.write('%else\n');
        for i in range(8):
            if g_asGRegs32NoSp[i]:
                self.write('        cmp     %s, 0x%x\n'
                           '        je      .ok_%u\n'
                           '        push    %u         ; register number\n'
                           '        push    %s         ; actual\n'
                           '        push    dword 0x%x ; expected\n'
                           '        call    VBINSTST_NAME(Common_BadValue)\n'
                           '.ok_%u:\n'
                           % ( g_asGRegs32NoSp[i], self.au32Regs[i], i, i, g_asGRegs32NoSp[i], self.au32Regs[i], i,));
        self.write('%endif\n'
                   '        ret\n'
                   'VBINSTST_ENDPROC   Common_CheckKnownValues\n'
                   '\n');

        return True;

    def _generateFileFooter(self):
        """
        Generates file footer.
        """

        # Register checking functions.
        for iRegs in self.dCheckFns:
            iReg1 = iRegs & 0x1f;
            sReg1 = self.oTarget.asGRegs[iReg1];
            iReg2 = (iRegs >> 5) & 0x1f;
            sReg2 = self.oTarget.asGRegs[iReg2];
            sPushSize = 'dword';

            self.write('\n\n'
                       '; Checks two register values, expected values pushed on the stack.\n'
                        '; To save space, the callee cleans up the stack.'
                       '; Ref count: %u\n'
                       'VBINSTST_BEGINPROC Common_Check_%s_%s\n'
                       '        MY_PUSH_FLAGS\n'
                       % ( self.dCheckFns[iRegs], sReg1, sReg2, ) );

            self.write('        cmp     %s, [xSP + MY_PUSH_FLAGS_SIZE + xCB]\n'
                       '        je      .equal1\n'
                       '        push    %s %u      ; register number\n'
                       '        push    %s         ; actual\n'
                       '        mov     %s, [xSP + sCB*2 + MY_PUSH_FLAGS_SIZE + xCB]\n'
                       '        push    %s         ; expected\n'
                       '        call    VBINSTST_NAME(Common_BadValue)\n'
                       '        pop     %s\n'
                       '        pop     %s\n'
                       '        pop     %s\n'
                       '.equal1:\n'
                       % ( sReg1, sPushSize, iReg1, sReg1, sReg1, sReg1, sReg1, sReg1, sReg1, ) );
            if iReg1 != iReg2: # If input and result regs are the same, only check the result.
                self.write('        cmp     %s, [xSP + sCB + MY_PUSH_FLAGS_SIZE + xCB]\n'
                           '        je      .equal2\n'
                           '        push    %s %u      ; register number\n'
                           '        push    %s         ; actual\n'
                           '        mov     %s, [xSP + sCB*3 + MY_PUSH_FLAGS_SIZE + xCB]\n'
                           '        push    %s         ; expected\n'
                           '        call    VBINSTST_NAME(Common_BadValue)\n'
                           '        pop     %s\n'
                           '        pop     %s\n'
                           '        pop     %s\n'
                           '.equal2:\n'
                           % ( sReg2, sPushSize, iReg2, sReg2, sReg2, sReg2, sReg2, sReg2, sReg2, ) );

            if self.oTarget.is64Bit():
                self.write('        mov     %s, [g_u64KnownValue_%s wrt rip]\n' % (sReg1, sReg1,));
                if iReg1 != iReg2:
                    self.write('        mov     %s, [g_u64KnownValue_%s wrt rip]\n' % (sReg2, sReg2,));
            else:
                self.write('        mov     %s, 0x%x\n' % (sReg1, self.au32Regs[iReg1],));
                if iReg1 != iReg2:
                    self.write('        mov     %s, 0x%x\n' % (sReg2, self.au32Regs[iReg2],));
            self.write('        MY_POP_FLAGS\n'
                       '        call    VBINSTST_NAME(Common_CheckKnownValues)\n'
                       '        ret     sCB*2\n'
                       'VBINSTST_ENDPROC   Common_Check_%s_%s\n'
                       % (sReg1, sReg2,));


        # 64-bit constants.
        if len(self.d64BitConsts) > 0:
            self.write('\n\n'
                       ';\n'
                       '; 64-bit constants\n'
                       ';\n');
            for uVal in self.d64BitConsts:
                self.write('g_u64Const_0x%016x: dq 0x%016x ; Ref count: %d\n' % (uVal, uVal, self.d64BitConsts[uVal], ) );

        return True;

    def _generateTests(self):
        """
        Generate the test cases.
        """
        for self.iFile in range(self.cFiles):
            if self.cFiles == 1:
                self.sFile = '%s.asm' % (self.oOptions.sOutputBase,)
            else:
                self.sFile = '%s-%u.asm' % (self.oOptions.sOutputBase, self.iFile)
            self.oFile = sys.stdout;
            if self.oOptions.sOutputBase != '-':
                self.oFile = io.open(self.sFile, 'w', encoding = 'utf-8');

            self._generateFileHeader();

            # Calc the range.
            iInstrTestStart = self.iFile * self.oOptions.cInstrPerFile;
            iInstrTestEnd   = iInstrTestStart + self.oOptions.cInstrPerFile;
            if iInstrTestEnd > len(g_aoInstructionTests):
                iInstrTestEnd = len(g_aoInstructionTests);

            # Generate the instruction tests.
            for iInstrTest in range(iInstrTestStart, iInstrTestEnd):
                oInstrTest = g_aoInstructionTests[iInstrTest];
                self.write('\n'
                           '\n'
                           ';\n'
                           '; %s\n'
                           ';\n'
                           % (oInstrTest.sName,));
                oInstrTest.generateTest(self, self._calcTestFunctionName(oInstrTest, iInstrTest));

            # Generate the main function.
            self.write('\n\n'
                       'VBINSTST_BEGINPROC TestInstrMain\n'
                       '        MY_PUSH_ALL\n'
                       '        sub xSP, 40h\n'
                       '\n');

            for iInstrTest in range(iInstrTestStart, iInstrTestEnd):
                oInstrTest = g_aoInstructionTests[iInstrTest];
                if oInstrTest.isApplicable(self):
                    self.write('%%ifdef ASM_CALL64_GCC\n'
                               '        lea  rdi, [.szInstr%03u wrt rip]\n'
                               '%%elifdef ASM_CALL64_MSC\n'
                               '        lea  rcx, [.szInstr%03u wrt rip]\n'
                               '%%else\n'
                               '        mov  xAX, .szInstr%03u\n'
                               '        mov  [xSP], xAX\n'
                               '%%endif\n'
                               '        VBINSTST_CALL_FN_SUB_TEST\n'
                               '        call VBINSTST_NAME(%s)\n'
                               % ( iInstrTest, iInstrTest, iInstrTest, self._calcTestFunctionName(oInstrTest, iInstrTest)));

            self.write('\n'
                       '        add  xSP, 40h\n'
                       '        MY_POP_ALL\n'
                       '        ret\n\n');
            for iInstrTest in range(iInstrTestStart, iInstrTestEnd):
                self.write('.szInstr%03u: db \'%s\', 0\n' % (iInstrTest, g_aoInstructionTests[iInstrTest].sName,));
            self.write('VBINSTST_ENDPROC TestInstrMain\n\n');

            self._generateFileFooter();
            if self.oOptions.sOutputBase != '-':
                self.oFile.close();
            self.oFile = None;
        self.sFile = '';

        return RTEXITCODE_SUCCESS;

    def _runMakefileMode(self):
        """
        Generate a list of output files on standard output.
        """
        if self.cFiles == 1:
            print('%s.asm' % (self.oOptions.sOutputBase,));
        else:
            print(' '.join('%s-%s.asm' % (self.oOptions.sOutputBase, i) for i in range(self.cFiles)));
        return RTEXITCODE_SUCCESS;

    def run(self):
        """
        Generates the tests or whatever is required.
        """
        if self.oOptions.fMakefileMode:
            return self._runMakefileMode();
        return self._generateTests();

    @staticmethod
    def main():
        """
        Main function a la C/C++. Returns exit code.
        """

        #
        # Parse the command line.
        #
        oParser = OptionParser(version = __version__[11:-1].strip());
        oParser.add_option('--makefile-mode', dest = 'fMakefileMode', action = 'store_true', default = False,
                           help = 'Special mode for use to output a list of output files for the benefit of '
                                  'the make program (kmk).');
        oParser.add_option('--split', dest = 'cInstrPerFile', metavar = '<instr-per-file>', type = 'int', default = 9999999,
                           help = 'Number of instruction to test per output file.');
        oParser.add_option('--output-base', dest = 'sOutputBase', metavar = '<file>', default = None,
                           help = 'The output file base name, no suffix please. Required.');
        oParser.add_option('--target', dest = 'sTargetEnv', metavar = '<target>',
                           default = 'iprt-r3-32',
                           choices = g_dTargetEnvs.keys(),
                           help = 'The target environment. Choices: %s'
                                % (', '.join(sorted(g_dTargetEnvs.keys())),));

        (oOptions, asArgs) = oParser.parse_args();
        if len(asArgs) > 0:
            oParser.print_help();
            return RTEXITCODE_SYNTAX
        if oOptions.sOutputBase is None:
            print('syntax error: Missing required option --output-base.', file = sys.stderr);
            return RTEXITCODE_SYNTAX

        #
        # Instantiate the program class and run it.
        #
        oProgram = InstructionTestGen(oOptions);
        return oProgram.run();


if __name__ == '__main__':
    sys.exit(InstructionTestGen.main());

