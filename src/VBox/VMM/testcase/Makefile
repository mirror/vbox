#
# Makefile for the VMM testcases.
#

#
# Copyright (C) 2006 InnoTek Systemberatung GmbH
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License as published by the Free Software Foundation,
# in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
# distribution. VirtualBox OSE is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY of any kind.
#
# If you received this file as part of a commercial VirtualBox
# distribution, then only the terms of your commercial VirtualBox
# license agreement apply instead of the previous paragraph.
#

DEPTH = ../../../..
include	$(PATH_KBUILD)/header.kmk
include	$(PATH_KBUILD)/tools/MINGW32.kmk

#
# Target lists.
#
#LIBRARIES        = tstHelp
PROGRAMS         = tstVMStructSize tstVMStructGC
ifneq ($(BUILD_TARGET),darwin)
PROGRAMS        += tstAsmStructs tstAsmStructsGC
endif
PROGRAMS        += tstVMM
ifdef VBOX_WITH_TESTCASES
PROGRAMS        += tstCFGM tstSSM tstMMHyperHeap tstVMM-2 tstVMREQ tstMicro tstCompiler tstVMMR0CallHost-1
ifneq ($(BUILD_TARGET),l4)
PROGRAMS        += tstAnimate
endif
#PROGRAMS        += tstProg1 tstProg2 tstProg3
SYSMODS          = tstMicroGC
endif # VBOX_WITH_TESTCASES

#
# We setup two 'other' targets for executing the two structure & alignment
# validation testcases. Perhaps a bit hackish, but extremely useful.
#
ifeq ($(BUILD_TARGET),$(BUILD_PLATFORM))
 ifeq ($(filter-out x86.amd64 x86.x86, $(BUILD_TARGET_ARCH).$(BUILD_PLATFORM_ARCH)),)
  OTHERS          += \
	$(PATH_TARGET)/tstAsmStructs.run \
	$(PATH_TARGET)/tstVMStructSize.run
 endif
endif
OTHER_CLEAN     += \
	$(PATH_TARGET)/tstAsmStructs.run \
	$(PATH_TARGET)/tstVMStructSize.run \
	$(PATH_TARGET)/tstAsmStructs.h \
	$(PATH_TARGET)/tstAsmStructsAsm.o \
	$(PATH_TARGET)/tstAsmStructs.h.dump

#
# Globals.
#
TEMPLATE = VBOXR3TSTEXE

#
# Targets
#
tstHelp_TEMPLATE        = VBOXLNX32GUESTR3
tstHelp_SOURCES         = tstHelpA.asm tstHelp.c

tstProg1_TEMPLATE       = VBOXLNX32GUESTR3
tstProg1_SOURCES        = tstProg-1.c tstProg-1A.asm
tstProg1_LIBS           = $(PATH_LIB)/tstHelp$(VBOX_SUFF_LIB)
tstProg1_LDFLAGS.linux  = -static

tstProg2_TEMPLATE       = VBOXLNX32GUESTR3
tstProg2_SOURCES        = tstProg-2.c tstProg-2A.asm
tstProg2_LDFLAGS.linux  = -static

tstProg3_TEMPLATE       = VBOXLNX32GUESTR3
tstProg3_SOURCES        = tstProg-3.c
tstProg3_LDFLAGS.linux  = -static


tstVMStructGC_TEMPLATE  = VBOXGCEXE
tstVMStructGC_DEFS      = IN_VMM_GC IN_PDM_GC IN_CFGM_GC IN_IOM_GC IN_VM_GC IN_CPUM_GC IN_SELM_GC IN_PGM_GC IN_TRPM_GC IN_MM_GC IN_PDM_GC IN_DBGF_GC IN_PATM_GC IN_DIS_GC IN_STAM_GC IN_VGADEVICE_GC IN_CSAM_GC IN_SSM_GC IN_REM_GC IN_RRM_GC
tstVMStructGC_SOURCES   = tstVMStructGC.cpp
tstVMStructGC_INCS      = .. ../PATM

tstVMStructSize_DEFS    = IN_VMM_R3 IN_PDM_R3 IN_CFGM_R3 IN_IOM_R3 IN_VM_R3 IN_CPUM_R3 IN_SELM_R3 IN_PGM_R3 IN_TRPM_R3 IN_MM_R3 IN_PDM_R3 IN_DBGF_R3 IN_PATM_R3 IN_DIS_R3 IN_STAM_R3 IN_VGADEVICE_R3 IN_CSAM_R3 IN_SSM_R3 IN_REM_R3
tstVMStructSize_INCS    = .. ../PATM $(PATH_TARGET)
tstVMStructSize_SOURCES = tstVMStructSize.cpp
tstVMStructSize_LIBS.l4 = $(LIB_RUNTIME)
tstVMStructSize.cpp_DEPS= $(PATH_TARGET)/tstVMStructGC.h

tstAsmStructs_DEFS      = IN_VMM_R3 IN_PDM_R3 IN_CFGM_R3 IN_IOM_R3 IN_VM_R3 IN_CPUM_R3 IN_SELM_R3 IN_PGM_R3 IN_TRPM_R3 IN_MM_R3 IN_PDM_R3 IN_DBGF_R3 IN_PATM_R3 IN_DIS_R3 IN_STAM_R3 IN_VGADEVICE_R3 IN_CSAM_R3 IN_SSM_R3
tstAsmStructs_INCS      = .. $(PATH_TARGET)
tstAsmStructs_SOURCES   = tstAsmStructs.cpp
tstAsmStructs_LIBS.l4   = $(LIB_RUNTIME)

tstAsmStructsGC_TEMPLATE= VBOXGCEXE
tstAsmStructsGC_DEFS    = IN_VMM_GC IN_PDM_GC IN_CFGM_GC IN_IOM_GC IN_VM_GC IN_CPUM_GC IN_SELM_GC IN_PGM_GC IN_TRPM_GC IN_MM_GC IN_PDM_GC IN_DBGF_GC IN_PATM_GC IN_DIS_GC IN_STAM_GC IN_VGADEVICE_GC IN_CSAM_GC IN_SSM_GC IN_RRM_GC
tstAsmStructsGC_INCS    = .. $(PATH_TARGET)
tstAsmStructsGC_SOURCES = tstAsmStructs.cpp

tstAsmStructs.cpp_DEPS  = $(PATH_TARGET)/tstAsmStructsHC.h $(PATH_TARGET)/tstAsmStructsGC.h


tstMMHyperHeap_SOURCES  = tstMMHyperHeap.cpp
tstMMHyperHeap_LIBS     = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

tstSSM_SOURCES          = tstSSM.cpp
tstSSM_LIBS             = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

tstCFGM_SOURCES         = tstCFGM.cpp
tstCFGM_LIBS            = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

tstVMM_SOURCES          = tstVMM.cpp
tstVMM_TEMPLATE         = VBOXR3EXE
tstVMM_LIBS             = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

tstVMM-2_SOURCES        = tstVMM-2.cpp
tstVMM-2_TEMPLATE       = VBOXR3EXE
tstVMM-2_LIBS           = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

tstVMMR0CallHost-1_TEMPLATE = VBOXR3EXE
tstVMMR0CallHost-1_INCS  = ..
tstVMMR0CallHost-1_SOURCES  = \
	tstVMMR0CallHost-1.cpp \
	../VMMR0/VMMR0A.asm
tstVMMR0CallHost-1_LIBS     = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

tstVMREQ_SOURCES        = tstVMREQ.cpp
tstVMREQ_TEMPLATE       = VBOXR3EXE
tstVMREQ_LIBS           = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

tstAnimate_SOURCES      = tstAnimate.cpp
tstAnimate_TEMPLATE     = VBOXR3EXE
tstAnimate_LIBS         = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

tstCompiler_SOURCES     = tstCompiler.cpp
tstCompiler_LIBS        = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

tstMicro_SOURCES        = tstMicro.cpp
tstMicro_TEMPLATE       = VBOXR3EXE
tstMicro_LIBS           = $(LIB_VMM) $(LIB_REM) $(LIB_RUNTIME)

# tstMicroGC.gc
tstMicroGC_TEMPLATE     = VBOXGC
tstMicroGC_SOURCES      = tstMicroGC.cpp tstMicroGCA.asm
tstMicroGC_DEFS         =
ifeq ($(VBOX_LDR_FMT32),pe)
tstMicroGC_LDFLAGS      = -Entry:tstMicroGC
endif
tstMicroGC_SYSSUFF      = .gc
tstMicroGC_LIBS         = \
	$(PATH_LIB)/DisasmGC$(VBOX_SUFF_LIB) \
	$(PATH_LIB)/RuntimeGC$(VBOX_SUFF_LIB)
ifeq ($(filter-out pe lx,$(VBOX_LDR_FMT32)),)
tstMicroGC_LIBS        += \
	$(PATH_LIB)/VMMGCBuiltin$(VBOX_SUFF_LIB) \
	$(LIB_VMMGC)
endif
tstMicroGC_SOURCES.win = tstMicroGC.def


include	$(PATH_KBUILD)/footer.kmk


#
# Some handcrafted support for tstAsmStructs.
#

# 1. make an aout object containing the offsets.
$(PATH_TARGET)/tstAsmStructsAsm.o: tstAsmStructsAsm.asm \
		$(DEPTH)/include/iprt/asmdefs.mac \
		$(DEPTH)/include/VBox/cpum.mac \
		$(DEPTH)/include/VBox/vm.mac \
		../CPUMInternal.mac \
		../TRPMInternal.mac \
		../VMMInternal.mac \
		Makefile
	$(call MSG_L1,Compiling $<)
ifndef DONT_USE_YASM
	$(QUIET)$(TOOL_YASM_AS) \
		$(addprefix -D, \
			$(DEFS) \
			$(DEFS.$(BUILD_TYPE)) \
			$(DEFS.$(BUILD_TARGET)) \
			IN_RING3 \
			$(DEFS.$(BUILD_TARGET_ARCH)) \
			$(DEFS.$(BUILD_TARGET).$(BUILD_TARGET_ARCH)) \
		) \
		-f elf -g stabs $(foreach inc,$(INCS),-I$(inc)/) -o $@ $<
else
	$(QUIET)$(TOOL_NASM_AS) \
		$(addprefix -D, \
			$(DEFS) \
			$(DEFS.$(BUILD_TYPE)) \
			$(DEFS.$(BUILD_TARGET)) \
			IN_RING3 \
			$(DEFS.$(BUILD_TARGET_ARCH)) \
			$(DEFS.$(BUILD_TARGET).$(BUILD_TARGET_ARCH)) \
		) \
		-f elf $(foreach inc,$(INCS),-I$(inc)/) -o $@ $<
endif

# 2. use objdump and sed to transform this into the header we want.
$(PATH_TARGET)/tstAsmStructsHC.h: $(PATH_TARGET)/tstAsmStructsAsm.o
	$(call MSG_L1,Generating $@)
	$(QUIET)$(RM) -f $@ $@.dump $@.tmp
ifeq ($(BUILD_PLATFORM),win)
	$(QUIET)$(PATH_TOOL_MINGW32)/bin/objdump.exe -x $< > $@.dump
else
	$(QUIET)objdump -x $< > $@.dump
endif
	$(QUIET)$(SED) -e '/STAMPROFILEADV/d' \
	       -e '/ l[[:space:]]*\*ABS\*[[:space:]]*00000000[[:space:]]*[^_.]*\./!d'     -e 's/^[0]*\(.*\) l.*00000000 \([^.]*\)\.\(.*$$\)/    CHECK_OFF(\2, 0x0\1, \3);/' \
	       $@.dump >  $@.tmp
	$(QUIET)$(SED) -e '/VM_size$$/d' \
	       -e '/ l[[:space:]]*\*ABS\*[[:space:]]*00000000[[:space:]]*[^_.]*\_size/!d' -e 's/^[0]*\(.*\) l.*00000000 \([^.]*\)\_size/    CHECK_SIZE(\2, 0x0\1);/' \
	       $@.dump >> $@.tmp
	$(QUIET)$(MV) -f $@.tmp $@
	$(QUIET)$(RM) -f $@.dump

# 3. transform the HC header into a GC one by omitting some HC only structures.
$(PATH_TARGET)/tstAsmStructsGC.h: $(PATH_TARGET)/tstAsmStructsHC.h
	$(call MSG_L1,Generating $@)
	$(QUIET)$(SED) -e '/VMMSWITCHERDEF/d' \
		$^ > $@

# 4. run it.
$(PATH_TARGET)/tstAsmStructs.run: $(INSTARGET_tstAsmStructs) $(INSTARGET_tstAsmStructsGC)
	$(QUIET)$(RM) -f $@
	$(INSTARGET_tstAsmStructs)
	$(INSTARGET_tstAsmStructsGC)
	$(QUIET)$(APPEND) "$@" "done"

#
# Run rule for tstVMStructSize.
#

# 1. Dump selected structure in the VMMGC.gc debug info.
# 2. Generate a testcase from the dump
## future

# 1&2. Manually dump selected structures and members.
$(PATH_TARGET)/tstVMStructGC.h: $(INSTARGET_tstVMStructGC)
	$(call MSG_L1,Generating $<)
	$(QUIET)$(INSTARGET_tstVMStructGC) > $@

# 3. run it.
$(PATH_TARGET)/tstVMStructSize.run: $(INSTARGET_tstVMStructSize)
	$(QUIET)$(RM) -f $@
	$^
	$(QUIET)$(APPEND) "$@" "done"

# alias for the two struct tests.
run-struct-tests: $(PATH_TARGET)/tstAsmStructs.run $(PATH_TARGET)/tstVMStructSize.run

