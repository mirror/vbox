/** @file
 * IPRT - Tar archive I/O.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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

#ifndef ___iprt_tar_h
#define ___iprt_tar_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/time.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_tar    RTTar - Tar archive I/O
 * @ingroup grp_rt
 * @{
 */

/** A tar handle */
typedef R3PTRTYPE(struct RTTARINTERNAL *)        RTTAR;
/** Pointer to a RTTAR interface handle. */
typedef RTTAR                                   *PRTTAR;
/** Nil RTTAR interface handle. */
#define NIL_RTTAR                                ((RTTAR)0)

/** A tar file handle */
typedef R3PTRTYPE(struct RTTARFILEINTERNAL *)    RTTARFILE;
/** Pointer to a RTTARFILE interface handle. */
typedef RTTARFILE                               *PRTTARFILE;
/** Nil RTTARFILE interface handle. */
#define NIL_RTTARFILE                            ((RTTARFILE)0)

/** Maximum length of a tar filename, excluding the terminating '\0'. More
 * does not fit into a tar record. */
#define RTTAR_NAME_MAX                           99

/**
 * Opens a Tar archive.
 *
 * Use the mask to specify the access type. In create mode the target file
 * have not to exists.
 *
 * @returns IPRT status code.
 *
 * @param   phTar          Where to store the RTTAR handle.
 * @param   pszTarname     The file name of the tar archive to open.
 * @param   fMode          Open flags, i.e a combination of the RTFILE_O_* defines.
 *                         The ACCESS, ACTION and DENY flags are mandatory!
 * @param   fStream        Open the file in stream mode. Within this mode no
 *                         seeking is allowed. Use this together with
 *                         RTTarFileCurrent, RTTarFileOpenCurrent,
 *                         RTTarFileSeekNextFile and the read method to
 *                         sequential read a tar file. Currently ignored with
 *                         RTFILE_O_WRITE.
 */
RTR3DECL(int) RTTarOpen(PRTTAR phTar, const char *pszTarname, uint32_t fMode, bool fStream);

#if 0
/**
 * Opens a Tar archive by handle.
 *
 * Use the mask to specify the access type. In create mode the target file
 * have not to exists.
 *
 * @returns IPRT status code.
 *
 * @param   phTar          Where to store the RTTAR handle.
 * @param   hFile          The file handle of the tar file.  This is expected
 *                         to be a regular file at the moment.
 * @param   fStream        Open the file in stream mode. Within this mode no
 *                         seeking is allowed.  Use this together with
 *                         RTTarFileCurrent, RTTarFileOpenCurrent,
 *                         RTTarFileSeekNextFile and the read method to
 *                         sequential read a tar file.  Currently ignored with
 *                         RTFILE_O_WRITE.
 */
RTR3DECL(int) RTTarOpenByHandle(PRTTAR phTar, RTFILE hFile, uint32_t fMode, bool fStream);
#endif

/**
 * Close the Tar archive.
 *
 * @returns IPRT status code.
 *
 * @param   hTar           Handle to the RTTAR interface.
 */
RTR3DECL(int) RTTarClose(RTTAR hTar);

/**
 * Open a file in the Tar archive.
 *
 * @returns IPRT status code.
 *
 * @param   hTar           The handle of the tar archive.
 * @param   phFile         Where to store the handle to the opened file.
 * @param   pszFilename    Path to the file which is to be opened. (UTF-8)
 * @param   fOpen          Open flags, i.e a combination of the RTFILE_O_* defines.
 *                         The ACCESS, ACTION flags are mandatory! DENY flags
 *                         are currently not supported.
 *
 * @remarks Write mode means append mode only. It is not possible to make
 *          changes to existing files.
 *
 * @remarks Currently it is not possible to open more than one file in write
 *          mode. Although open more than one file in read only mode (even when
 *          one file is opened in write mode) is always possible.
 */
RTR3DECL(int) RTTarFileOpen(RTTAR hTar, PRTTARFILE phFile, const char *pszFilename, uint32_t fOpen);

/**
 * Close the file opened by RTTarFileOpen.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          The file handle to close.
 */
RTR3DECL(int) RTTarFileClose(RTTARFILE hFile);

/**
 * Changes the read & write position in a file.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          Handle to the file.
 * @param   offSeek        Offset to seek.
 * @param   uMethod        Seek method, i.e. one of the RTFILE_SEEK_* defines.
 * @param   poffActual     Where to store the new file position.
 *                         NULL is allowed.
 */
RTR3DECL(int) RTTarFileSeek(RTTARFILE hFile, uint64_t offSeek, unsigned uMethod, uint64_t *poffActual);

/**
 * Gets the current file position.
 *
 * @returns File offset.
 * @returns UINT64_MAX on failure.
 *
 * @param   hFile          Handle to the file.
 */
RTR3DECL(uint64_t) RTTarFileTell(RTTARFILE hFile);

/**
 * Read bytes from a file.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          Handle to the file.
 * @param   pvBuf          Where to put the bytes we read.
 * @param   cbToRead       How much to read.
 * @param   *pcbRead       How much we actually read .
 *                         If NULL an error will be returned for a partial read.
 */
RTR3DECL(int) RTTarFileRead(RTTARFILE hFile, void *pvBuf, size_t cbToRead, size_t *pcbRead);

/**
 * Read bytes from a file at a given offset.
 * This function may modify the file position.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          Handle to the file.
 * @param   off            Where to read.
 * @param   pvBuf          Where to put the bytes we read.
 * @param   cbToRead       How much to read.
 * @param   *pcbRead       How much we actually read .
 *                         If NULL an error will be returned for a partial read.
 */
RTR3DECL(int) RTTarFileReadAt(RTTARFILE hFile, uint64_t off, void *pvBuf, size_t cbToRead, size_t *pcbRead);

/**
 * Write bytes to a file.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          Handle to the file.
 * @param   pvBuf          What to write.
 * @param   cbToWrite      How much to write.
 * @param   *pcbWritten    How much we actually wrote.
 *                         If NULL an error will be returned for a partial write.
 */
RTR3DECL(int) RTTarFileWrite(RTTARFILE hFile, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten);

/**
 * Write bytes to a file at a given offset.
 * This function may modify the file position.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          Handle to the file.
 * @param   off            Where to write.
 * @param   pvBuf          What to write.
 * @param   cbToWrite      How much to write.
 * @param   *pcbWritten    How much we actually wrote.
 *                         If NULL an error will be returned for a partial write.
 */
RTR3DECL(int) RTTarFileWriteAt(RTTARFILE hFile, uint64_t off, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten);

/**
 * Query the size of the file.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          Handle to the file.
 * @param   pcbSize        Where to store the filesize.
 */
RTR3DECL(int) RTTarFileGetSize(RTTARFILE hFile, uint64_t *pcbSize);

/**
 * Set the size of the file.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          Handle to the file.
 * @param   cbSize         The new file size.
 */
RTR3DECL(int) RTTarFileSetSize(RTTARFILE hFile, uint64_t cbSize);

/**
 * Gets the mode flags of an open file.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          Handle to the file.
 * @param   pfMode         Where to store the file mode, see @ref grp_rt_fs for details.
 */
RTR3DECL(int) RTTarFileGetMode(RTTARFILE hFile, uint32_t *pfMode);

/**
 * Changes the mode flags of an open file.
 *
 * @returns IPRT status code.
 *
 * @param   hFile          Handle to the file.
 * @param   fMode          The new file mode, see @ref grp_rt_fs for details.
 */
RTR3DECL(int) RTTarFileSetMode(RTTARFILE hFile, uint32_t fMode);

/**
 * Gets the modification timestamp of the file.
 *
 * @returns IPRT status code.
 *
 * @param   pFile           Handle to the file.
 * @param   pTime           Where to store the time.
 */
RTR3DECL(int) RTTarFileGetTime(RTTARFILE hFile, PRTTIMESPEC pTime);

/**
 * Sets the modification timestamp of the file.
 *
 * @returns IPRT status code.
 *
 * @param   pFile           Handle to the file.
 * @param   pTime           The time to store.
 */
RTR3DECL(int) RTTarFileSetTime(RTTARFILE hFile, PRTTIMESPEC pTime);

/**
 * Gets the owner and/or group of an open file.
 *
 * @returns IPRT status code.
 *
 * @param   hFile           Handle to the file.
 * @param   pUid            Where to store the owner user id. NULL is ok.
 * @param   pGid            Where to store the group id. NULL is ok.
 */
RTR3DECL(int) RTTarFileGetOwner(RTTARFILE hFile, uint32_t *pUid, uint32_t *pGid);

/**
 * Changes the owner and/or group of an open file.
 *
 * @returns IPRT status code.
 *
 * @param   hFile           Handle to the file.
 * @param   uid             The new file owner user id. Use -1 (or ~0) to leave this unchanged.
 * @param   gid             The new group id. Use -1 (or ~0) to leave this unchanged.
 */
RTR3DECL(int) RTTarFileSetOwner(RTTARFILE hFile, uint32_t uid, uint32_t gid);

/******************************************************************************
 *   Convenience Functions                                                    *
 ******************************************************************************/

/**
 * Check if the specified file exists in the Tar archive.
 *
 * (The matching is case sensitive.)
 *
 * @note    Currently only regular files are supported.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS when the file exists in the Tar archive.
 * @retval  VERR_FILE_NOT_FOUND when the file not exists in the Tar archive.
 *
 * @param   pszTarFile      Tar file to check.
 * @param   pszFile         Filename to check for.
 *
 * @todo    This is predicate function which SHALL return bool!
 */
RTR3DECL(int) RTTarFileExists(const char *pszTarFile, const char *pszFile);

/**
 * Create a file list from a Tar archive.
 *
 * @note    Currently only regular files are supported.
 *
 * @returns IPRT status code.
 *
 * @param   pszTarFile      Tar file to list files from.
 * @param   ppapszFiles     On success an array with array with the filenames is
 *                          returned. The names must be freed with RTStrFree and
 *                          the array with RTMemFree.
 * @param   pcFiles         On success the number of entries in ppapszFiles.
 */
RTR3DECL(int) RTTarList(const char *pszTarFile, char ***ppapszFiles, size_t *pcFiles);

/**
 * Extract a file from a Tar archive into a memory buffer.
 *
 * The caller is responsible for the deletion of the returned memory buffer.
 *
 * (The matching is case sensitive.)
 *
 * @note    Currently only regular files are supported. Also some of the header
 *          fields are not used (uid, gid, uname, gname, mtime).
 *
 * @returns IPRT status code.
 *
 * @param   pszTarFile           Tar file to extract files from.
 * @param   ppBuf                The buffer which will held the extracted data.
 * @param   pcbSize              The size (in bytes) of ppBuf after successful
 *                               extraction.
 * @param   pszFile              The file to extract.
 * @param   pfnProgressCallback  Progress callback function. Optional.
 * @param   pvUser               User defined data for the progress
 *                               callback. Optional.
 */
RTR3DECL(int) RTTarExtractFileToBuf(const char *pszTarFile, void **ppvBuf, size_t *pcbSize, const char *pszFile,
                                    PFNRTPROGRESS pfnProgressCallback, void *pvUser);

/**
 * Extract a set of files from a Tar archive.
 *
 * Also note that this function is atomic. If an error occurs all previously
 * extracted files will be deleted.
 *
 * (The matching is case sensitive.)
 *
 * @note    Currently only regular files are supported. Also some of the header
 *          fields are not used (uid, gid, uname, gname, mtime).
 *
 * @returns IPRT status code.
 *
 * @param   pszTarFile           Tar file to extract files from.
 * @param   pszOutputDir         Where to store the extracted files. Must exist.
 * @param   papszFiles           Which files should be extracted.
 * @param   cFiles               The number of files in papszFiles.
 * @param   pfnProgressCallback  Progress callback function. Optional.
 * @param   pvUser               User defined data for the progress
 *                               callback. Optional.
 */
RTR3DECL(int) RTTarExtractFiles(const char *pszTarFile, const char *pszOutputDir, const char * const *papszFiles, size_t cFiles, PFNRTPROGRESS pfnProgressCallback, void *pvUser);

/**
 * Extract all files of the archive.
 *
 * @note    Currently only regular files are supported. Also some of the header
 *          fields are not used (uid, gid, uname, gname, mtime).
 *
 * @returns IPRT status code.
 *
 * @param   pszTarFile           Tar file to extract the files from.
 * @param   pszOutputDir         Where to store the extracted files. Must exist.
 * @param   pfnProgressCallback  Progress callback function. Optional.
 * @param   pvUser               User defined data for the progress
 *                               callback. Optional.
 */
RTR3DECL(int) RTTarExtractAll(const char *pszTarFile, const char *pszOutputDir, PFNRTPROGRESS pfnProgressCallback, void *pvUser);

/**
 * Create a Tar archive out of the given files.
 *
 * @note Currently only regular files are supported.
 *
 * @returns IPRT status code.
 *
 * @param   pszTarFile           Where to create the Tar archive.
 * @param   papszFiles           Which files should be included.
 * @param   cFiles               The number of files in papszFiles.
 * @param   pfnProgressCallback  Progress callback function. Optional.
 * @param   pvUser               User defined data for the progress
 *                               callback. Optional.
 */
RTR3DECL(int) RTTarCreate(const char *pszTarFile, const char * const *papszFiles, size_t cFiles, PFNRTPROGRESS pfnProgressCallback, void *pvUser);

/******************************************************************************
 *   Streaming Functions                                                      *
 ******************************************************************************/

/**
 * Return the filename where RTTar currently stays at.
 *
 * @returns IPRT status code.
 *
 * @param   hTar           Handle to the RTTAR interface.
 * @param   ppszFilename   On success the filename.
 */
RTR3DECL(int) RTTarCurrentFile(RTTAR hTar, char **ppszFilename);

/**
 * Jumps to the next file from the current RTTar position.
 *
 * @returns IPRT status code.
 *
 * @param   hTar           Handle to the RTTAR interface.
 */
RTR3DECL(int) RTTarSeekNextFile(RTTAR hTar);

/**
 * Opens the file where RTTar currently stays at.
 *
 * @returns IPRT status code.
 *
 * @param   hTar           Handle to the RTTAR interface.
 * @param   phFile         Where to store the handle to the opened file.
 * @param   ppszFilename   On success the filename.
 * @param   fOpen          Open flags, i.e a combination of the RTFILE_O_* defines.
 *                         The ACCESS, ACTION flags are mandatory! Currently
 *                         only RTFILE_O_OPEN | RTFILE_O_READ is supported.
 */
RTR3DECL(int) RTTarFileOpenCurrentFile(RTTAR hTar, PRTTARFILE phFile, char **ppszFilename, uint32_t fOpen);


/** @} */

RT_C_DECLS_END

#endif /* ___iprt_tar_h */

