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
 * URI parsing and creation based on RFC 3986.
 * See http://datatracker.ietf.org/doc/rfc3986/ for the full specification.
 * @note Currently it isn't the full specification implemented.
 * @note Currently only some generic URI support and a minimum File(file:) URI
 * support is implemented. Other specific scheme support, like html:, ldap:,
 * data:, ..., is missing.
 * @see grp_rt_uri_file
 * @ingroup grp_rt
 * @{
 */

/**
 * Creates a generic URI. The returned pointer must be freed
 * using RTStrFree().
 *
 * @returns the new URI on success, NULL otherwise.
 * @param   pszScheme           The URI scheme.
 * @param   pszAuthority        The authority part of the URI (optional).
 * @param   pszPath             The path part of the URI (optional).
 * @param   pszQuery            The query part of the URI (optional).
 * @param   pszFragment         The fragment part of the URI (optional).
 */
RTR3DECL(char *) RTUriCreate(const char *pszScheme, const char *pszAuthority, const char *pszPath, const char *pszQuery, const char *pszFragment);

/**
 * Check an string for a specific URI scheme.
 *
 * @returns true if the scheme match, false if not.
 * @param   pszUri              The URI to check.
 * @param   pszScheme           The scheme to compare with.
 */
RTR3DECL(bool)   RTUriHasScheme(const char *pszUri, const char *pszScheme);

/**
 * Extract the scheme out of an URI.
 *
 * @returns the scheme if the URI is valid, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 */
RTR3DECL(char *) RTUriScheme(const char *pszUri);

/**
 * Extract the authority out of an URI.
 *
 * @returns the authority if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 */
RTR3DECL(char *) RTUriAuthority(const char *pszUri);

/**
 * Extract the path out of an URI.
 *
 * @returns the path if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 */
RTR3DECL(char *) RTUriPath(const char *pszUri);

/**
 * Extract the query out of an URI.
 *
 * @returns the query if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 */
RTR3DECL(char *) RTUriQuery(const char *pszUri);

/**
 * Extract the fragment out of an URI.
 *
 * @returns the fragment if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 */
RTR3DECL(char *) RTUriFragment(const char *pszUri);

/** @defgroup grp_rt_uri_file   RTUriFile - Uri file parsing and creation
 * Adds file: scheme support to the generic RTUri interface. This is partly
 * documented in http://datatracker.ietf.org/doc/rfc1738/.
 * @{
 */

/** Auto detect in which format a path is returned. */
#define URI_FILE_FORMAT_AUTO  UINT32_C(0)
/** Return a path in UNIX format style. */
#define URI_FILE_FORMAT_UNIX  UINT32_C(1)
/** Return a path in Windows format style. */
#define URI_FILE_FORMAT_WIN   UINT32_C(2)

/**
 * Creates a file URI. The returned pointer must be freed
 * using RTStrFree().
 *
 * @see RTUriCreate
 *
 * @returns the new URI on success, NULL otherwise.
 * @param   pszPath             The path of the URI.
 */
RTR3DECL(char *) RTUriFileCreate(const char *pszPath);

/**
 * Returns the file path encoded in the URI.
 *
 * @returns the path if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 * @param   uFormat             In which format should the path returned.
 */
RTR3DECL(char *) RTUriFilePath(const char *pszUri, uint32_t uFormat);

/**
 * Returns the file path encoded in the URI, given a max string length.
 *
 * @returns the path if the URI contains one, NULL otherwise.
 * @param   pszUri              The URI to extract from.
 * @param   uFormat             In which format should the path returned.
 * @param   cbMax               The max string length to inspect.
 */
RTR3DECL(char *) RTUriFileNPath(const char *pszUri, uint32_t uFormat, size_t cchMax);

/** @} */

/** @} */

RT_C_DECLS_END

#endif

