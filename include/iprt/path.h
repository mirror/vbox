/** @file
 * IPRT - Path Manipulation.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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

#ifndef ___iprt_path_h
#define ___iprt_path_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#ifdef IN_RING3
# include <iprt/fs.h>
#endif



RT_C_DECLS_BEGIN

/** @defgroup grp_rt_path   RTPath - Path Manipulation
 * @ingroup grp_rt
 * @{
 */

/**
 * Host max path (the reasonable value).
 * @remarks defined both by iprt/param.h and iprt/path.h.
 */
#if !defined(___iprt_param_h) || defined(DOXYGEN_RUNNING)
# define RTPATH_MAX         (4096 + 4)    /* (PATH_MAX + 1) on linux w/ some alignment */
#endif

/** @def RTPATH_SLASH
 * The preferred slash character.
 *
 * @remark IPRT will always accept unix slashes. So, normally you would
 *         never have to use this define.
 */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define RTPATH_SLASH       '\\'
#else
# define RTPATH_SLASH       '/'
#endif

/** @deprecated Use '/'! */
#define RTPATH_DELIMITER    RTPATH_SLASH


/** @def RTPATH_SLASH_STR
 * The preferred slash character as a string, handy for concatenations
 * with other strings.
 *
 * @remark IPRT will always accept unix slashes. So, normally you would
 *         never have to use this define.
 */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define RTPATH_SLASH_STR   "\\"
#else
# define RTPATH_SLASH_STR   "/"
#endif


/** @def RTPATH_IS_SLASH
 * Checks if a character is a slash.
 *
 * @returns true if it's a slash and false if not.
 * @returns @param      ch      Char to check.
 */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define RTPATH_IS_SLASH(ch)    ( (ch) == '\\' || (ch) == '/' )
#else
# define RTPATH_IS_SLASH(ch)    ( (ch) == '/' )
#endif


/** @def RTPATH_IS_VOLSEP
 * Checks if a character marks the end of the volume specification.
 *
 * @remark  This is sufficient for the drive letter concept on PC.
 *          However it might be insufficient on other platforms
 *          and even on PC a UNC volume spec won't be detected this way.
 *          Use the RTPath@<too be created@>() instead.
 *
 * @returns true if it is and false if it isn't.
 * @returns @param      ch      Char to check.
 */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define RTPATH_IS_VOLSEP(ch)   ( (ch) == ':' )
#else
# define RTPATH_IS_VOLSEP(ch)   (false)
#endif


/** @def RTPATH_IS_SEP
 * Checks if a character is path component separator
 *
 * @returns true if it is and false if it isn't.
 * @returns @param      ch      Char to check.
 * @
 */
#define RTPATH_IS_SEP(ch)       ( RTPATH_IS_SLASH(ch) || RTPATH_IS_VOLSEP(ch) )


/** @name Generic RTPath flags
 * @{ */
/** Last component: Work on the link. */
#define RTPATH_F_ON_LINK          RT_BIT_32(0)
/** Last component: Follow if link. */
#define RTPATH_F_FOLLOW_LINK      RT_BIT_32(1)
/** Don't allow symbolic links as part of the path.
 * @remarks this flag is currently not implemented and will be ignored. */
#define RTPATH_F_NO_SYMLINKS      RT_BIT_32(2)
/** @} */


/** Validates a flags parameter containing RTPATH_F_*.
 * @remarks The parameters will be referenced multiple times. */
#define RTPATH_F_IS_VALID(fFlags, fIgnore) \
    (    ((fFlags) & ~(uint32_t)((fIgnore)|RTPATH_F_NO_SYMLINKS)) == RTPATH_F_ON_LINK \
      || ((fFlags) & ~(uint32_t)((fIgnore)|RTPATH_F_NO_SYMLINKS)) == RTPATH_F_FOLLOW_LINK )


/**
 * Checks if the path exists.
 *
 * Symbolic links will all be attempted resolved and broken links means false.
 *
 * @returns true if it exists and false if it doesn't.
 * @param   pszPath     The path to check.
 */
RTDECL(bool) RTPathExists(const char *pszPath);

/**
 * Checks if the path exists.
 *
 * @returns true if it exists and false if it doesn't.
 * @param   pszPath     The path to check.
 * @param   fFlags      RTPATH_F_ON_LINK or RTPATH_F_FOLLOW_LINK.
 */
RTDECL(bool) RTPathExistsEx(const char *pszPath, uint32_t fFlags);

/**
 * Sets the current working directory of the process.
 *
 * @returns IPRT status code.
 * @param   pszPath         The path to the new working directory.
 */
RTDECL(int)  RTPathSetCurrent(const char *pszPath);

/**
 * Gets the current working directory of the process.
 *
 * @returns IPRT status code.
 * @param   pszPath         Where to store the path.
 * @param   cchPath         The size of the buffer pszPath points to.
 */
RTDECL(int)  RTPathGetCurrent(char *pszPath, size_t cchPath);

/**
 * Get the real path (no symlinks, no . or .. components), must exist.
 *
 * @returns iprt status code.
 * @param   pszPath         The path to resolve.
 * @param   pszRealPath     Where to store the real path.
 * @param   cchRealPath     Size of the buffer.
 */
RTDECL(int) RTPathReal(const char *pszPath, char *pszRealPath, size_t cchRealPath);

/**
 * Same as RTPathReal only the result is RTStrDup()'ed.
 *
 * @returns Pointer to real path. Use RTStrFree() to free this string.
 * @returns NULL if RTPathReal() or RTStrDup() fails.
 * @param   pszPath         The path to resolve.
 */
RTDECL(char *) RTPathRealDup(const char *pszPath);

/**
 * Get the absolute path (starts from root, no . or .. components), doesn't have
 * to exist. Note that this method is designed to never perform actual file
 * system access, therefore symlinks are not resolved.
 *
 * @returns iprt status code.
 * @param   pszPath         The path to resolve.
 * @param   pszAbsPath      Where to store the absolute path.
 * @param   cchAbsPath      Size of the buffer.
 */
RTDECL(int) RTPathAbs(const char *pszPath, char *pszAbsPath, size_t cchAbsPath);

/**
 * Same as RTPathAbs only the result is RTStrDup()'ed.
 *
 * @returns Pointer to the absolute path. Use RTStrFree() to free this string.
 * @returns NULL if RTPathAbs() or RTStrDup() fails.
 * @param   pszPath         The path to resolve.
 */
RTDECL(char *) RTPathAbsDup(const char *pszPath);

/**
 * Get the absolute path (no symlinks, no . or .. components), assuming the
 * given base path as the current directory. The resulting path doesn't have
 * to exist.
 *
 * @returns iprt status code.
 * @param   pszBase         The base path to act like a current directory.
 *                          When NULL, the actual cwd is used (i.e. the call
 *                          is equivalent to RTPathAbs(pszPath, ...).
 * @param   pszPath         The path to resolve.
 * @param   pszAbsPath      Where to store the absolute path.
 * @param   cchAbsPath      Size of the buffer.
 */
RTDECL(int) RTPathAbsEx(const char *pszBase, const char *pszPath, char *pszAbsPath, size_t cchAbsPath);

/**
 * Same as RTPathAbsEx only the result is RTStrDup()'ed.
 *
 * @returns Pointer to the absolute path. Use RTStrFree() to free this string.
 * @returns NULL if RTPathAbsEx() or RTStrDup() fails.
 * @param   pszBase         The base path to act like a current directory.
 *                          When NULL, the actual cwd is used (i.e. the call
 *                          is equivalent to RTPathAbs(pszPath, ...).
 * @param   pszPath         The path to resolve.
 */
RTDECL(char *) RTPathAbsExDup(const char *pszBase, const char *pszPath);

/**
 * Strips the filename from a path. Truncates the given string in-place by overwriting the
 * last path separator character with a null byte in a platform-neutral way.
 *
 * @param   pszPath     Path from which filename should be extracted, will be truncated.
 *                      If the string contains no path separator, it will be changed to a "." string.
 */
RTDECL(void) RTPathStripFilename(char *pszPath);

/**
 * Strips the extension from a path.
 *
 * @param   pszPath     Path which extension should be stripped.
 */
RTDECL(void) RTPathStripExt(char *pszPath);

/**
 * Strips the trailing slashes of a path name.
 *
 * Won't strip root slashes.
 *
 * @returns The new length of pszPath.
 * @param   pszPath     Path to strip.
 */
RTDECL(size_t) RTPathStripTrailingSlash(char *pszPath);

/**
 * Changes all the slashes in the specified path to DOS style.
 *
 * Unless @a fForce is set, nothing will be done when on a UNIX flavored system
 * since paths wont work with DOS style slashes there.
 *
 * @returns @a pszPath.
 * @param   pszPath             The path to modify.
 * @param   fForce              Whether to force the conversion on non-DOS OSes.
 */
RTDECL(char *) RTPathChangeToDosSlashes(char *pszPath, bool fForce);

/**
 * Changes all the slashes in the specified path to unix style.
 *
 * Unless @a fForce is set, nothing will be done when on a UNIX flavored system
 * since paths wont work with DOS style slashes there.
 *
 * @returns @a pszPath.
 * @param   pszPath             The path to modify.
 * @param   fForce              Whether to force the conversion on non-DOS OSes.
 */
RTDECL(char *) RTPathChangeToUnixSlashes(char *pszPath, bool fForce);

/**
 * Parses a path.
 *
 * It figures the length of the directory component, the offset of
 * the file name and the location of the suffix dot.
 *
 * @returns The path length.
 *
 * @param   pszPath     Path to find filename in.
 * @param   pcchDir     Where to put the length of the directory component. If
 *                      no directory, this will be 0. Optional.
 * @param   poffName    Where to store the filename offset.
 *                      If empty string or if it's ending with a slash this
 *                      will be set to -1. Optional.
 * @param   poffSuff    Where to store the suffix offset (the last dot).
 *                      If empty string or if it's ending with a slash this
 *                      will be set to -1. Optional.
 */
RTDECL(size_t) RTPathParse(const char *pszPath, size_t *pcchDir, ssize_t *poffName, ssize_t *poffSuff);

/**
 * Finds the filename in a path.
 *
 * @returns Pointer to filename within pszPath.
 * @returns NULL if no filename (i.e. empty string or ends with a slash).
 * @param   pszPath     Path to find filename in.
 */
RTDECL(char *) RTPathFilename(const char *pszPath);

/**
 * Finds the extension part of in a path.
 *
 * @returns Pointer to extension within pszPath.
 * @returns NULL if no extension.
 * @param   pszPath     Path to find extension in.
 */
RTDECL(char *) RTPathExt(const char *pszPath);

/**
 * Checks if a path has an extension.
 *
 * @returns true if extension present.
 * @returns false if no extension.
 * @param   pszPath     Path to check.
 */
RTDECL(bool) RTPathHasExt(const char *pszPath);
/** Misspelled, don't use.  */
#define RTPathHaveExt   RTPathHasExt

/**
 * Checks if a path includes more than a filename.
 *
 * @returns true if path present.
 * @returns false if no path.
 * @param   pszPath     Path to check.
 */
RTDECL(bool) RTPathHasPath(const char *pszPath);
/** Misspelled, don't use.  */
#define RTPathHavePath  RTPathHasPath

/**
 * Checks if the path starts with a root specifier or not.
 *
 * @returns @c true if it starts with root, @c false if not.
 *
 * @param   pszPath     Path to check.
 */
RTDECL(bool) RTPathStartsWithRoot(const char *pszPath);

/**
 * Counts the components in the specified path.
 *
 * An empty string has zero components.  A lone root slash is considered have
 * one.  The paths "/init" and "/bin/" are considered having two components.  An
 * UNC share specifier like "\\myserver\share" will be considered as one single
 * component.
 *
 * @returns The number of path components.
 * @param   pszPath     The path to parse.
 */
RTDECL(size_t) RTPathCountComponents(const char *pszPath);

/**
 * Copies the specified number of path components from @a pszSrc and into @a
 * pszDst.
 *
 * @returns VINF_SUCCESS or VERR_BUFFER_OVERFLOW.  In the latter case the buffer
 *          is not touched.
 *
 * @param   pszDst      The destination buffer.
 * @param   cbDst       The size of the destination buffer.
 * @param   pszSrc      The source path.
 * @param   cComponents The number of components to copy from @a pszSrc.
 */
RTDECL(int) RTPathCopyComponents(char *pszDst, size_t cbDst, const char *pszSrc, size_t cComponents);

/**
 * Compares two paths.
 *
 * The comparison takes platform-dependent details into account,
 * such as:
 * <ul>
 * <li>On DOS-like platforms, both separator chars (|\| and |/|) are considered
 *     to be equal.
 * <li>On platforms with case-insensitive file systems, mismatching characters
 *     are uppercased and compared again.
 * </ul>
 *
 * @returns @< 0 if the first path less than the second path.
 * @returns 0 if the first path identical to the second path.
 * @returns @> 0 if the first path greater than the second path.
 *
 * @param   pszPath1    Path to compare (must be an absolute path).
 * @param   pszPath2    Path to compare (must be an absolute path).
 *
 * @remarks File system details are currently ignored. This means that you won't
 *          get case-insensitive compares on unix systems when a path goes into a
 *          case-insensitive filesystem like FAT, HPFS, HFS, NTFS, JFS, or
 *          similar. For NT, OS/2 and similar you'll won't get case-sensitive
 *          compares on a case-sensitive file system.
 */
RTDECL(int) RTPathCompare(const char *pszPath1, const char *pszPath2);

/**
 * Checks if a path starts with the given parent path.
 *
 * This means that either the path and the parent path matches completely, or
 * that the path is to some file or directory residing in the tree given by the
 * parent directory.
 *
 * The path comparison takes platform-dependent details into account,
 * see RTPathCompare() for details.
 *
 * @returns |true| when \a pszPath starts with \a pszParentPath (or when they
 *          are identical), or |false| otherwise.
 *
 * @param   pszPath         Path to check, must be an absolute path.
 * @param   pszParentPath   Parent path, must be an absolute path.
 *                          No trailing directory slash!
 *
 * @remarks This API doesn't currently handle root directory compares in a
 *          manner consistent with the other APIs. RTPathStartsWith(pszSomePath,
 *          "/") will not work if pszSomePath isn't "/".
 */
RTDECL(bool) RTPathStartsWith(const char *pszPath, const char *pszParentPath);

/**
 * Appends one partial path to another.
 *
 * The main purpose of this function is to deal correctly with the slashes when
 * concatenating the two partial paths.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_BUFFER_OVERFLOW if the result is too big to fit within
 *          cbPathDst bytes. No changes has been made.
 * @retval  VERR_INVALID_PARAMETER if the string pointed to by pszPath is longer
 *          than cbPathDst-1 bytes (failed to find terminator). Asserted.
 *
 * @param   pszPath         The path to append pszAppend to. This serves as both
 *                          input and output. This can be empty, in which case
 *                          pszAppend is just copied over.
 * @param   cbPathDst       The size of the buffer pszPath points to, terminator
 *                          included. This should NOT be strlen(pszPath).
 * @param   pszAppend       The partial path to append to pszPath. This can be
 *                          NULL, in which case nothing is done.
 *
 * @remarks See the RTPathAppendEx remarks.
 */
RTDECL(int) RTPathAppend(char *pszPath, size_t cbPathDst, const char *pszAppend);

/**
 * Appends one partial path to another.
 *
 * The main purpose of this function is to deal correctly with the slashes when
 * concatenating the two partial paths.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_BUFFER_OVERFLOW if the result is too big to fit within
 *          cbPathDst bytes. No changes has been made.
 * @retval  VERR_INVALID_PARAMETER if the string pointed to by pszPath is longer
 *          than cbPathDst-1 bytes (failed to find terminator). Asserted.
 *
 * @param   pszPath         The path to append pszAppend to. This serves as both
 *                          input and output. This can be empty, in which case
 *                          pszAppend is just copied over.
 * @param   cbPathDst       The size of the buffer pszPath points to, terminator
 *                          included. This should NOT be strlen(pszPath).
 * @param   pszAppend       The partial path to append to pszPath. This can be
 *                          NULL, in which case nothing is done.
 * @param   cchAppendMax    The maximum number or characters to take from @a
 *                          pszAppend.  RTSTR_MAX is fine.
 *
 * @remarks On OS/2, Window and similar systems, concatenating a drive letter
 *          specifier with a slash prefixed path will result in an absolute
 *          path. Meaning, RTPathAppend(strcpy(szBuf, "C:"), sizeof(szBuf),
 *          "/bar") will result in "C:/bar". (This follows directly from the
 *          behavior when pszPath is empty.)
 *
 *          On the other hand, when joining a drive letter specifier with a
 *          partial path that does not start with a slash, the result is not an
 *          absolute path. Meaning, RTPathAppend(strcpy(szBuf, "C:"),
 *          sizeof(szBuf), "bar") will result in "C:bar".
 */
RTDECL(int) RTPathAppendEx(char *pszPath, size_t cbPathDst, const char *pszAppend, size_t cchAppendMax);

/**
 * Like RTPathAppend, but with the base path as a separate argument instead of
 * in the path buffer.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_BUFFER_OVERFLOW if the result is too big to fit within
 *          cbPathDst bytes.
 * @retval  VERR_INVALID_PARAMETER if the string pointed to by pszPath is longer
 *          than cbPathDst-1 bytes (failed to find terminator). Asserted.
 *
 * @param   pszPathDst      Where to store the resulting path.
 * @param   cbPathDst       The size of the buffer pszPathDst points to,
 *                          terminator included.
 * @param   pszPathSrc      The base path to copy into @a pszPathDst before
 *                          appending @a pszAppend.
 * @param   pszAppend       The partial path to append to pszPathSrc. This can
 *                          be NULL, in which case nothing is done.
 *
 */
RTDECL(int) RTPathJoin(char *pszPathDst, size_t cbPathDst, const char *pszPathSrc,
                       const char *pszAppend);

/**
 * Same as RTPathJoin, except that the output buffer is allocated.
 *
 * @returns Buffer containing the joined up path, call RTStrFree to free.  NULL
 *          on allocation failure.
 * @param   pszPathSrc      The base path to copy into @a pszPathDst before
 *                          appending @a pszAppend.
 * @param   pszAppend       The partial path to append to pszPathSrc. This can
 *                          be NULL, in which case nothing is done.
 *
 */
RTDECL(char *) RTPathJoinA(const char *pszPathSrc, const char *pszAppend);

/**
 * Extended version of RTPathJoin, both inputs can be specified as substrings.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_BUFFER_OVERFLOW if the result is too big to fit within
 *          cbPathDst bytes.
 * @retval  VERR_INVALID_PARAMETER if the string pointed to by pszPath is longer
 *          than cbPathDst-1 bytes (failed to find terminator). Asserted.
 *
 * @param   pszPathDst      Where to store the resulting path.
 * @param   cbPathDst       The size of the buffer pszPathDst points to,
 *                          terminator included.
 * @param   pszPathSrc      The base path to copy into @a pszPathDst before
 *                          appending @a pszAppend.
 * @param   cchPathSrcMax   The maximum number of bytes to copy from @a
 *                          pszPathSrc.  RTSTR_MAX is find.
 * @param   pszAppend       The partial path to append to pszPathSrc. This can
 *                          be NULL, in which case nothing is done.
 * @param   cchAppendMax    The maximum number of bytes to copy from @a
 *                          pszAppend.  RTSTR_MAX is find.
 *
 */
RTDECL(int) RTPathJoinEx(char *pszPathDst, size_t cbPathDst,
                         const char *pszPathSrc, size_t cchPathSrcMax,
                         const char *pszAppend, size_t cchAppendMax);

/**
 * Callback for RTPathTraverseList that's called for each element.
 *
 * @returns IPRT style status code. Return VERR_TRY_AGAIN to continue, any other
 *          value will abort the traversing and be returned to the caller.
 *
 * @param   pchPath         Pointer to the start of the current path. This is
 *                          not null terminated.
 * @param   cchPath         The length of the path.
 * @param   pvUser1         The first user parameter.
 * @param   pvUser2         The second user parameter.
 */
typedef DECLCALLBACK(int) FNRTPATHTRAVERSER(char const *pchPath, size_t cchPath, void *pvUser1, void *pvUser2);
/** Pointer to a FNRTPATHTRAVERSER. */
typedef FNRTPATHTRAVERSER *PFNRTPATHTRAVERSER;

/**
 * Traverses a string that can contain multiple paths separated by a special
 * character.
 *
 * @returns IPRT style status code from the callback or VERR_END_OF_STRING if
 *          the callback returned VERR_TRY_AGAIN for all paths in the string.
 *
 * @param   pszPathList     The string to traverse.
 * @param   chSep           The separator character.  Using the null terminator
 *                          is fine, but the result will simply be that there
 *                          will only be one callback for the entire string
 *                          (save any leading white space).
 * @param   pfnCallback     The callback.
 * @param   pvUser1         First user argument for the callback.
 * @param   pvUser2         Second user argument for the callback.
 */
RTDECL(int) RTPathTraverseList(const char *pszPathList, char chSep, PFNRTPATHTRAVERSER pfnCallback, void *pvUser1, void *pvUser2);


/**
 * Calculate a relative path between the two given paths.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_BUFFER_OVERFLOW if the result is too big to fit within
 *          cbPathDst bytes.
 * @retval  VERR_NOT_SUPPORTED if both paths start with different volume specifiers.
 * @param   pszPathDst      Where to store the resulting path.
 * @param   cbPathDst       The size of the buffer pszPathDst points to,
 *                          terminator included.
 * @param   pszPathFrom     The path to start from creating the relative path.
 * @param   pszPathTo       The path to reach with the created relative path.
 */
RTDECL(int) RTPathCalcRelative(char *pszPathDst, size_t cbPathDst,
                               const char *pszPathFrom,
                               const char *pszPathTo);

#ifdef IN_RING3

/**
 * Gets the path to the directory containing the executable.
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathExecDir(char *pszPath, size_t cchPath);

/**
 * Gets the user home directory.
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathUserHome(char *pszPath, size_t cchPath);

/**
 * Gets the user documents directory.
 *
 * The returned path isn't guarantied to exist.
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathUserDocuments(char *pszPath, size_t cchPath);

/**
 * Gets the directory of shared libraries.
 *
 * This is not the same as RTPathAppPrivateArch() as Linux depends all shared
 * libraries in a common global directory where ld.so can find them.
 *
 * Linux:    /usr/lib
 * Solaris:  /opt/@<application@>/@<arch>@ or something
 * Windows:  @<program files directory@>/@<application@>
 * Old path: same as RTPathExecDir()
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathSharedLibs(char *pszPath, size_t cchPath);

/**
 * Gets the directory for architecture-independent application data, for
 * example NLS files, module sources, ...
 *
 * Linux:    /usr/shared/@<application@>
 * Solaris:  /opt/@<application@>
 * Windows:  @<program files directory@>/@<application@>
 * Old path: same as RTPathExecDir()
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathAppPrivateNoArch(char *pszPath, size_t cchPath);

/**
 * Gets the directory for architecture-dependent application data, for
 * example modules which can be loaded at runtime.
 *
 * Linux:    /usr/lib/@<application@>
 * Solaris:  /opt/@<application@>/@<arch>@ or something
 * Windows:  @<program files directory@>/@<application@>
 * Old path: same as RTPathExecDir()
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathAppPrivateArch(char *pszPath, size_t cchPath);

/**
 * Gets the toplevel directory for architecture-dependent application data.
 *
 * This differs from RTPathAppPrivateArch on Solaris only where it will work
 * around the /opt/@<application@>/amd64 and /opt/@<application@>/i386 multi
 * architecture installation style.
 *
 * Linux:    /usr/lib/@<application@>
 * Solaris:  /opt/@<application@>
 * Windows:  @<program files directory@>/@<application@>
 * Old path: same as RTPathExecDir()
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathAppPrivateArchTop(char *pszPath, size_t cchPath);

/**
 * Gets the directory for documentation.
 *
 * Linux:    /usr/share/doc/@<application@>
 * Solaris:  /opt/@<application@>
 * Windows:  @<program files directory@>/@<application@>
 * Old path: same as RTPathExecDir()
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathAppDocs(char *pszPath, size_t cchPath);

/**
 * Gets the temporary directory path.
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathTemp(char *pszPath, size_t cchPath);

/**
 * Query information about a file system object.
 *
 * This API will resolve NOT symbolic links in the last component (just like
 * unix lstat()).
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS if the object exists, information returned.
 * @retval  VERR_PATH_NOT_FOUND if any but the last component in the specified
 *          path was not found or was not a directory.
 * @retval  VERR_FILE_NOT_FOUND if the object does not exist (but path to the
 *          parent directory exists).
 *
 * @param   pszPath     Path to the file system object.
 * @param   pObjInfo    Object information structure to be filled on successful
 *                      return.
 * @param   enmAdditionalAttribs
 *                      Which set of additional attributes to request.
 *                      Use RTFSOBJATTRADD_NOTHING if this doesn't matter.
 */
RTR3DECL(int) RTPathQueryInfo(const char *pszPath, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs);

/**
 * Query information about a file system object.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS if the object exists, information returned.
 * @retval  VERR_PATH_NOT_FOUND if any but the last component in the specified
 *          path was not found or was not a directory.
 * @retval  VERR_FILE_NOT_FOUND if the object does not exist (but path to the
 *          parent directory exists).
 *
 * @param   pszPath     Path to the file system object.
 * @param   pObjInfo    Object information structure to be filled on successful return.
 * @param   enmAdditionalAttribs
 *                      Which set of additional attributes to request.
 *                      Use RTFSOBJATTRADD_NOTHING if this doesn't matter.
 * @param   fFlags      RTPATH_F_ON_LINK or RTPATH_F_FOLLOW_LINK.
 */
RTR3DECL(int) RTPathQueryInfoEx(const char *pszPath, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs, uint32_t fFlags);

/**
 * Changes the mode flags of a file system object.
 *
 * The API requires at least one of the mode flag sets (Unix/Dos) to
 * be set. The type is ignored.
 *
 * This API will resolve symbolic links in the last component since
 * mode isn't important for symbolic links.
 *
 * @returns iprt status code.
 * @param   pszPath     Path to the file system object.
 * @param   fMode       The new file mode, see @ref grp_rt_fs for details.
 */
RTR3DECL(int) RTPathSetMode(const char *pszPath, RTFMODE fMode);

/**
 * Gets the mode flags of a file system object.
 *
 * @returns iprt status code.
 * @param   pszPath     Path to the file system object.
 * @param   pfMode      Where to store the file mode, see @ref grp_rt_fs for details.
 *
 * @remark  This is wrapper around RTPathQueryInfoEx(RTPATH_F_FOLLOW_LINK) and
 *          exists to complement RTPathSetMode().
 */
RTR3DECL(int) RTPathGetMode(const char *pszPath, PRTFMODE pfMode);

/**
 * Changes one or more of the timestamps associated of file system object.
 *
 * This API will not resolve symbolic links in the last component (just
 * like unix lutimes()).
 *
 * @returns iprt status code.
 * @param   pszPath             Path to the file system object.
 * @param   pAccessTime         Pointer to the new access time.
 * @param   pModificationTime   Pointer to the new modification time.
 * @param   pChangeTime         Pointer to the new change time. NULL if not to be changed.
 * @param   pBirthTime          Pointer to the new time of birth. NULL if not to be changed.
 *
 * @remark  The file system might not implement all these time attributes,
 *          the API will ignore the ones which aren't supported.
 *
 * @remark  The file system might not implement the time resolution
 *          employed by this interface, the time will be chopped to fit.
 *
 * @remark  The file system may update the change time even if it's
 *          not specified.
 *
 * @remark  POSIX can only set Access & Modification and will always set both.
 */
RTR3DECL(int) RTPathSetTimes(const char *pszPath, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                             PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime);

/**
 * Changes one or more of the timestamps associated of file system object.
 *
 * @returns iprt status code.
 * @param   pszPath             Path to the file system object.
 * @param   pAccessTime         Pointer to the new access time.
 * @param   pModificationTime   Pointer to the new modification time.
 * @param   pChangeTime         Pointer to the new change time. NULL if not to be changed.
 * @param   pBirthTime          Pointer to the new time of birth. NULL if not to be changed.
 * @param   fFlags              RTPATH_F_ON_LINK or RTPATH_F_FOLLOW_LINK.
 *
 * @remark  The file system might not implement all these time attributes,
 *          the API will ignore the ones which aren't supported.
 *
 * @remark  The file system might not implement the time resolution
 *          employed by this interface, the time will be chopped to fit.
 *
 * @remark  The file system may update the change time even if it's
 *          not specified.
 *
 * @remark  POSIX can only set Access & Modification and will always set both.
 */
RTR3DECL(int) RTPathSetTimesEx(const char *pszPath, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                               PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime, uint32_t fFlags);

/**
 * Gets one or more of the timestamps associated of file system object.
 *
 * @returns iprt status code.
 * @param   pszPath             Path to the file system object.
 * @param   pAccessTime         Where to store the access time. NULL is ok.
 * @param   pModificationTime   Where to store the modification time. NULL is ok.
 * @param   pChangeTime         Where to store the change time. NULL is ok.
 * @param   pBirthTime          Where to store the creation time. NULL is ok.
 *
 * @remark  This is wrapper around RTPathQueryInfo() and exists to complement
 *          RTPathSetTimes().  If the last component is a symbolic link, it will
 *          not be resolved.
 */
RTR3DECL(int) RTPathGetTimes(const char *pszPath, PRTTIMESPEC pAccessTime, PRTTIMESPEC pModificationTime,
                             PRTTIMESPEC pChangeTime, PRTTIMESPEC pBirthTime);

/**
 * Changes the owner and/or group of a file system object.
 *
 * This API will not resolve symbolic links in the last component (just
 * like unix lchown()).
 *
 * @returns iprt status code.
 * @param   pszPath     Path to the file system object.
 * @param   uid         The new file owner user id.  Pass NIL_RTUID to leave
 *                      this unchanged.
 * @param   gid         The new group id.  Pass NIL_RTGUID to leave this
 *                      unchanged.
 */
RTR3DECL(int) RTPathSetOwner(const char *pszPath, uint32_t uid, uint32_t gid);

/**
 * Changes the owner and/or group of a file system object.
 *
 * @returns iprt status code.
 * @param   pszPath     Path to the file system object.
 * @param   uid         The new file owner user id.  Pass NIL_RTUID to leave
 *                      this unchanged.
 * @param   gid         The new group id.  Pass NIL_RTGID to leave this
 *                      unchanged.
 * @param   fFlags      RTPATH_F_ON_LINK or RTPATH_F_FOLLOW_LINK.
 */
RTR3DECL(int) RTPathSetOwnerEx(const char *pszPath, uint32_t uid, uint32_t gid, uint32_t fFlags);

/**
 * Gets the owner and/or group of a file system object.
 *
 * @returns iprt status code.
 * @param   pszPath     Path to the file system object.
 * @param   pUid        Where to store the owner user id. NULL is ok.
 * @param   pGid        Where to store the group id. NULL is ok.
 *
 * @remark  This is wrapper around RTPathQueryInfo() and exists to complement
 *          RTPathGetOwner().  If the last component is a symbolic link, it will
 *          not be resolved.
 */
RTR3DECL(int) RTPathGetOwner(const char *pszPath, uint32_t *pUid, uint32_t *pGid);


/** @name RTPathRename, RTDirRename & RTFileRename flags.
 * @{ */
/** Do not replace anything. */
#define RTPATHRENAME_FLAGS_NO_REPLACE   UINT32_C(0)
/** This will replace attempt any target which isn't a directory. */
#define RTPATHRENAME_FLAGS_REPLACE      RT_BIT(0)
/** Don't allow symbolic links as part of the path.
 * @remarks this flag is currently not implemented and will be ignored. */
#define RTPATHRENAME_FLAGS_NO_SYMLINKS  RT_BIT(1)
/** @} */

/**
 * Renames a path within a filesystem.
 *
 * This will rename symbolic links.  If RTPATHRENAME_FLAGS_REPLACE is used and
 * pszDst is a symbolic link, it will be replaced and not its target.
 *
 * @returns IPRT status code.
 * @param   pszSrc      The source path.
 * @param   pszDst      The destination path.
 * @param   fRename     Rename flags, RTPATHRENAME_FLAGS_*.
 */
RTR3DECL(int) RTPathRename(const char *pszSrc,  const char *pszDst, unsigned fRename);

/** @name RTPathUnlink flags.
 * @{ */
/** Don't allow symbolic links as part of the path.
 * @remarks this flag is currently not implemented and will be ignored. */
#define RTPATHUNLINK_FLAGS_NO_SYMLINKS  RT_BIT(0)
/** @} */

/**
 * Removes the last component of the path.
 *
 * @returns IPRT status code.
 * @param   pszPath     The path.
 * @param   fUnlink     Unlink flags, RTPATHUNLINK_FLAGS_*.
 */
RTR3DECL(int) RTPathUnlink(const char *pszPath, uint32_t fUnlink);

/**
 * A /bin/rm tool.
 *
 * @returns Program exit code.
 *
 * @param   cArgs               The number of arguments.
 * @param   papszArgs           The argument vector.  (Note that this may be
 *                              reordered, so the memory must be writable.)
 */
RTDECL(RTEXITCODE) RTPathRmCmd(unsigned cArgs, char **papszArgs);

#endif /* IN_RING3 */

/** @} */

RT_C_DECLS_END

#endif

