/** @file
 * IPRT - Uniform Resource Identifier handling.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
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

#ifndef ___iprt_uri_h
#define ___iprt_uri_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_uri    RTUri - Uri parsing and creation
 *
 * URI parsing and creation based on RFC-3986.
 *
 * @remarks The whole specification isn't implemented and we only provide scheme
 *          specific special APIs for "file://".
 *
 * @ingroup grp_rt
 * @{
 */


/**
 * Parsed URI.
 *
 * @remarks This structure is subject to change.
 */
typedef struct RTURIPARSED
{
    /** Magic value (for internal use only). */
    uint32_t    u32Magic;
    /** RTURIPARSED_F_XXX. */
    uint32_t    fFlags;

    /** The length of the scheme. */
    size_t      cchScheme;

    /** The offset into the string of the authority. */
    size_t      offAuthority;
    /** The authority length.
     * @remarks The authority component can be zero length, so to check whether
     *          it's there or not consult RTURIPARSED_F_HAVE_AUTHORITY. */
    size_t      cchAuthority;

    /** The offset into the string of the path. */
    size_t      offPath;
    /** The length of the path. */
    size_t      cchPath;

    /** The offset into the string of the query. */
    size_t      offQuery;
    /** The length of the query. */
    size_t      cchQuery;

    /** The offset into the string of the fragment. */
    size_t      offFragment;
    /** The length of the fragment. */
    size_t      cchFragment;

    /** @name Authority subdivisions
     * @{ */
    /** If there is a userinfo part, this is the start of it. Otherwise it's the
     * same as offAuthorityHost. */
    size_t      offAuthorityUsername;
    /** The length of the username (zero if not present). */
    size_t      cchAuthorityUsername;
    /** If there is a userinfo part containing a password, this is the start of it.
     * Otherwise it's the same as offAuthorityHost. */
    size_t      offAuthorityPassword;
    /** The length of the password (zero if not present). */
    size_t      cchAuthorityPassword;
    /** The offset of the host part of the authority. */
    size_t      offAuthorityHost;
    /** The length of the host part of the authority. */
    size_t      cchAuthorityHost;
    /** The authority port number, UINT32_MAX if not present. */
    uint32_t    uAuthorityPort;
    /** @} */
} RTURIPARSED;
/** Pointer to a parsed URI. */
typedef RTURIPARSED *PRTURIPARSED;
/** Pointer to a const parsed URI. */
typedef RTURIPARSED const *PCRTURIPARSED;

/** @name  RTURIPARSED_F_XXX - RTURIPARSED::fFlags
 * @{  */
/** Set if the URI contains escaped characters. */
#define RTURIPARSED_F_CONTAINS_ESCAPED_CHARS        UINT32_C(0x00000001)
/** Set if the URI have an authority component.  Necessary since the authority
 * component can have a zero length. */
#define RTURIPARSED_F_HAVE_AUTHORITY                UINT32_C(0x00000002)
/** @} */

/**
 * Parses a URI.
 *
 * @returns IPRT status code.
 * @param   pszUri              The URI to parse.
 * @param   pParsed             Where to return the details.  This can be handed
 *                              to the RTUriParsed* APIs for retriving
 *                              information.
 */
RTDECL(int) RTUriParse(const char *pszUri, PRTURIPARSED pParsed);

/**
 * Extract the scheme out of a parsed URI.
 *
 * @returns the scheme if the URI is valid, NULL otherwise.
 * @param   pszUri              The URI passed to RTUriParse when producing the
 *                              info in @a pParsed.
 * @param   pParsed             Pointer to the RTUriParse output.
 */
RTDECL(char *) RTUriParsedScheme(const char *pszUri, PCRTURIPARSED pParsed);

/**
 * Extract the authority out of a parsed URI.
 *
 * @returns the authority if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI passed to RTUriParse when producing the
 *                              info in @a pParsed.
 * @param   pParsed             Pointer to the RTUriParse output.
 * @remarks The authority can have a zero length.
 */
RTDECL(char *) RTUriParsedAuthority(const char *pszUri, PCRTURIPARSED pParsed);

/**
 * Extract the username out of the authority component in a parsed URI.
 *
 * @returns The username if the URI contains one, otherwise NULL.
 * @param   pszUri              The URI passed to RTUriParse when producing the
 *                              info in @a pParsed.
 * @param   pParsed             Pointer to the RTUriParse output.
 *
 * @todo    This may currently be returning NULL when it maybe would be more
 *          appropriate to return an empty string...
 */
RTDECL(char *) RTUriParsedAuthorityUsername(const char *pszUri, PCRTURIPARSED pParsed);

/**
 * Extract the password out of the authority component in a parsed URI.
 *
 * @returns The password if the URI contains one, otherwise NULL.
 * @param   pszUri              The URI passed to RTUriParse when producing the
 *                              info in @a pParsed.
 * @param   pParsed             Pointer to the RTUriParse output.
 *
 * @todo    This may currently be returning NULL when it maybe would be more
 *          appropriate to return an empty string...
 */
RTDECL(char *) RTUriParsedAuthorityPassword(const char *pszUri, PCRTURIPARSED pParsed);

/**
 * Extract the host out of the authority component in a parsed URI.
 *
 * @returns The host if the URI contains one, otherwise NULL.
 * @param   pszUri              The URI passed to RTUriParse when producing the
 *                              info in @a pParsed.
 * @param   pParsed             Pointer to the RTUriParse output.
 *
 * @todo    This may currently be returning NULL when it maybe would be more
 *          appropriate to return an empty string...
 */
RTDECL(char *) RTUriParsedAuthorityHost(const char *pszUri, PCRTURIPARSED pParsed);

/**
 * Extract the port number out of the authority component in a parsed URI.
 *
 * @returns The port number if the URI contains one, otherwise UINT32_MAX.
 * @param   pszUri              The URI passed to RTUriParse when producing the
 *                              info in @a pParsed.
 * @param   pParsed             Pointer to the RTUriParse output.
 */
RTDECL(uint32_t) RTUriParsedAuthorityPort(const char *pszUri, PCRTURIPARSED pParsed);

/**
 * Extract the path out of a parsed URI.
 *
 * @returns the path if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI passed to RTUriParse when producing the
 *                              info in @a pParsed.
 * @param   pParsed             Pointer to the RTUriParse output.
 */
RTDECL(char *) RTUriParsedPath(const char *pszUri, PCRTURIPARSED pParsed);

/**
 * Extract the query out of a parsed URI.
 *
 * @returns the query if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI passed to RTUriParse when producing the
 *                              info in @a pParsed.
 * @param   pParsed             Pointer to the RTUriParse output.
 */
RTDECL(char *) RTUriParsedQuery(const char *pszUri, PCRTURIPARSED pParsed);

/**
 * Extract the fragment out of a parsed URI.
 *
 * @returns the fragment if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI passed to RTUriParse when producing the
 *                              info in @a pParsed.
 * @param   pParsed             Pointer to the RTUriParse output.
 */
RTDECL(char *) RTUriParsedFragment(const char *pszUri, PCRTURIPARSED pParsed);



/**
 * Creates a generic URI.
 *
 * The returned pointer must be freed using RTStrFree().
 *
 * @returns the new URI on success, NULL otherwise.
 * @param   pszScheme           The URI scheme.
 * @param   pszAuthority        The authority part of the URI (optional).
 * @param   pszPath             The path part of the URI (optional).
 * @param   pszQuery            The query part of the URI (optional).
 * @param   pszFragment         The fragment part of the URI (optional).
 */
RTDECL(char *) RTUriCreate(const char *pszScheme, const char *pszAuthority, const char *pszPath, const char *pszQuery,
                           const char *pszFragment);

/**
 * Check whether the given scheme matches that of the URI.
 *
 * This does not validate the URI, it just compares the scheme, no more, no
 * less.  Thus it's much faster than using RTUriParsedScheme.
 *
 * @returns true if the scheme match, false if not.
 * @param   pszUri              The URI to check.
 * @param   pszScheme           The scheme to compare with.
 */
RTDECL(bool)   RTUriIsSchemeMatch(const char *pszUri, const char *pszScheme);

/**
 * Extract the path out of an URI.
 *
 * @returns the path if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 * @deprecated
 */
RTDECL(char *) RTUriPath(const char *pszUri);


/** @defgroup grp_rt_uri_file   RTUriFile - Uri file parsing and creation
 *
 * Implements basic "file:" scheme support to the generic RTUri interface.  This
 * is partly documented in RFC-1738.
 *
 * @{
 */

/** Return the host format. */
#define URI_FILE_FORMAT_AUTO  UINT32_C(0)
/** Return a path in UNIX format style. */
#define URI_FILE_FORMAT_UNIX  UINT32_C(1)
/** Return a path in Windows format style. */
#define URI_FILE_FORMAT_WIN   UINT32_C(2)

/**
 * Creates a file URI.
 *
 * The returned pointer must be freed using RTStrFree().
 *
 * @see RTUriCreate
 *
 * @returns The new URI on success, NULL otherwise.  Free With RTStrFree.
 * @param   pszPath             The path of the URI.
 */
RTDECL(char *) RTUriFileCreate(const char *pszPath);

/**
 * Returns the file path encoded in the URI.
 *
 * @returns the path if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 * @param   uFormat             In which format should the path returned.
 */
RTDECL(char *) RTUriFilePath(const char *pszUri, uint32_t uFormat);

/**
 * Returns the file path encoded in the URI, given a max string length.
 *
 * @returns the path if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 * @param   uFormat             In which format should the path returned.
 * @param   cbMax               The max string length to inspect.
 */
RTDECL(char *) RTUriFileNPath(const char *pszUri, uint32_t uFormat, size_t cchMax);

/** @} */

/** @} */

RT_C_DECLS_END

#endif

