/** @file
 * VirtualBox - manpage/refentry C extracts for built-in help.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_refentry_h
#define ___VBox_refentry_h

#include <iprt/types.h>

/** @defgroup grp_refentry Help generated from refentry/manpage.
 *
 * The refentry/manpage docbook source in doc/manual/en_US/man_* is processed by
 * doc/manual/docbook-refentry-to-C-help.xsl and turned a set of the structures
 * defined here.
 *
 * @{
 */

/** The non-breaking space character.
 * @remarks We could've used U+00A0, but it is easier both to encode and to
 *          search and replace a single ASCII character. */
#define REFENTRY_NBSP               '\b'

/** @name REFENTRYSTR_SCOPE_XXX - Common string scoping and flags.
 * @{ */
/** Same scope as previous string table entry, flags are reset and can be
 *  ORed in. */
#define REFENTRYSTR_SCOPE_SAME      UINT64_C(0)
/** Global scope. */
#define REFENTRYSTR_SCOPE_GLOBAL    UINT64_C(0x00ffffffffffffff)
/** Scope mask. */
#define REFENTRYSTR_SCOPE_MASK      UINT64_C(0x00ffffffffffffff)
/** Flags mask. */
#define REFENTRYSTR_FLAGS_MASK      UINT64_C(0xff00000000000000)
/** Command synopsis, special hanging indent rules applies. */
#define REFENTRYSTR_FLAGS_SYNOPSIS  RT_BIT_64(63)
/** @} */

/** String table entry for a re.   */
typedef struct REFENTRYSTR
{
    /** The scope of the string.  There are two predefined scopes,
     *  REFENTRYSTR_SCOPE_SAME and REFENTRYSTR_SCOPE_GLOBAL.  The rest are
     *  reference entry specific. */
    uint64_t        fScope;
    /** The string.  Non-breaking space is represented by the char
     * REFENTRY_NBSP defines, just in case the string needs wrapping.  There is
     * no trailing newline, that's implicit. */
    const char     *psz;
} REFENTRYSTR;
/** Pointer to a read-only string table entry. */
typedef const REFENTRYSTR *PCREFENTRYSTR;

typedef struct REFENTRYSTRTAB
{
    /** Number of strings. */
    uint16_t        cStrings;
    /** Reserved for future use. */
    uint16_t        fReserved;
    /** Pointer to the string table. */
    PCREFENTRYSTR   paStrings;
} REFENTRYSTRTAB;
/** Pointer to a read-only string table. */
typedef REFENTRYSTRTAB const *PCREFENTRYSTRTAB;

/**
 * Help extracted from a docbook refentry document.
 */
typedef struct REFENTRY
{
    /** Internal reference entry identifier.  */
    int64_t        idInternal;
    /** Usage synopsis. */
    REFENTRYSTRTAB Synopsis;
    /** Full help. */
    REFENTRYSTRTAB Help;
    /** Brief command description. */
    const char    *pszBrief;
} REFENTRY;
/** Pointer to a read-only refentry help extract structure. */
typedef REFENTRY const *PCREFENTRY;

/** @} */

#endif

