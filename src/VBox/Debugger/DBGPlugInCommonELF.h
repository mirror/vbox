/* $Id$ */
/** @file
 * DBGPlugInCommonELF - Common code for dealing with ELF images, Header.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef ___Debugger_DBGPlugInCommonELF_h
#define ___Debugger_DBGPlugInCommonELF_h

#include <VBox/types.h>
#include "../Runtime/include/internal/ldrELF32.h"
#include "../Runtime/include/internal/ldrELF64.h"

/** @name DBGDiggerCommonParseElf32Mod and DBGDiggerCommonParseElf64Mod flags
 * @{ */
/** Wheter to adjust the symbol values or not. */
#define DBG_DIGGER_ELF_ADJUST_SYM_VALUE     RT_BIT_32(0)
/** Indicates that we're missing section headers and that
 * all section indexes are to be considered invalid. (Solaris hack.)
 * This flag is incompatible with DBG_DIGGER_ELF_ADJUST_SYM_VALUE. */
#define DBG_DIGGER_ELF_FUNNY_SHDRS          RT_BIT_32(1)
/** Valid bit mask. */
#define DBG_DIGGER_ELF_MASK                 UINT32_C(0x00000003)
/* @} */

int DBGDiggerCommonParseElf32Mod(PVM pVM, const char *pszModName, const char *pszFilename, uint32_t fFlags,
                                 Elf32_Ehdr const *pEhdr, Elf32_Shdr const *paShdrs,
                                 Elf32_Sym const *paSyms, size_t cMaxSyms,
                                 char const *pbStrings, size_t cbMaxStrings,
                                 RTGCPTR MinAddr, RTGCPTR MaxAddr, uint64_t uModTag);

int DBGDiggerCommonParseElf64Mod(PVM pVM, const char *pszModName, const char *pszFilename, uint32_t fFlags,
                                 Elf64_Ehdr const *pEhdr, Elf64_Shdr const *paShdrs,
                                 Elf64_Sym const *paSyms, size_t cMaxSyms,
                                 char const *pbStrings, size_t cbMaxStrings,
                                 RTGCPTR MinAddr, RTGCPTR MaxAddr, uint64_t uModTag);

#endif

