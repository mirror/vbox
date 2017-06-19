/** @file
 * IPRT - ISO Image Maker.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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

#ifndef ___iprt_fsisomaker_h
#define ___iprt_fsisomaker_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/time.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_fsisomaker    RTFsIsoMaker - ISO Image Maker
 * @ingroup grp_rt_fs
 * @{
 */


/** @name RTFSISOMAKER_NAMESPACE_XXX - Namespace selector.
 * @{
 */
#define RTFSISOMAKER_NAMESPACE_ISO_9660     RT_BIT_32(0)            /**< The primary ISO-9660 namespace. */
#define RTFSISOMAKER_NAMESPACE_JOLIET       RT_BIT_32(1)            /**< The joliet namespace. */
#define RTFSISOMAKER_NAMESPACE_UDF          RT_BIT_32(2)            /**< The UDF namespace. */
#define RTFSISOMAKER_NAMESPACE_HFS          RT_BIT_32(3)            /**< The HFS namespace */
#define RTFSISOMAKER_NAMESPACE_ALL          UINT32_C(0x0000000f)    /**< All namespaces. */
#define RTFSISOMAKER_NAMESPACE_VALID_MASK   UINT32_C(0x0000000f)    /**< Valid namespace bits. */
/** @} */

/** Root directory configuration index. */
#define RTFSISOMAKER_CFG_IDX_ROOT           UINT32_C(0)


/**
 * Creates an ISO maker instance.
 *
 * @returns IPRT status code.
 * @param   phIsoMaker          Where to return the handle to the new ISO maker.
 */
RTDECL(int) RTFsIsoMakerCreate(PRTFSISOMAKER phIsoMaker);

/**
 * Retains a references to an ISO maker instance.
 *
 * @returns New reference count on success, UINT32_MAX if invalid handle.
 * @param   hIsoMaker           The ISO maker handle.
 */
RTDECL(uint32_t) RTFsIsoMakerRetain(RTFSISOMAKER hIsoMaker);

/**
 * Releases a references to an ISO maker instance.
 *
 * @returns New reference count on success, UINT32_MAX if invalid handle.
 * @param   hIsoMaker           The ISO maker handle.  NIL is ignored.
 */
RTDECL(uint32_t) RTFsIsoMakerRelease(RTFSISOMAKER hIsoMaker);

/**
 * Sets the ISO-9660 level.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uIsoLevel           The level, 1-3.
 */
RTDECL(int) RTFsIsoMakerSetIso9660Level(RTFSISOMAKER hIsoMaker, uint8_t uIsoLevel);

/**
 * Sets the joliet level.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uJolietLevel        The joliet UCS-2 level 1-3, or 0 to disable
 *                              joliet.
 */
RTDECL(int) RTFsIsoMakerSetJolietUcs2Level(RTFSISOMAKER hIsoMaker, uint8_t uJolietLevel);

/**
 * Sets the rock ridge support level (on the primary ISO-9660 namespace).
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uLevel              0 if disabled, 1 to just enable, 2 to enable and
 *                              write the ER tag.
 */
RTDECL(int) RTFsIsoMakerSetRockRidgeLevel(RTFSISOMAKER hIsoMaker, uint8_t uLevel);

/**
 * Sets the rock ridge support level on the joliet namespace (experimental).
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   uLevel              0 if disabled, 1 to just enable, 2 to enable and
 *                              write the ER tag.
 */
RTDECL(int) RTFsIsoMakerSetJolietRockRidgeLevel(RTFSISOMAKER hIsoMaker, uint8_t uLevel);

/**
 * Sets the content of the system area, i.e. the first 32KB of the image.
 *
 * This can be used to put generic boot related stuff.
 *
 * @note    Other settings may overwrite parts of the content (yet to be
 *          determined which).
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pvContent           The content to put in the system area.
 * @param   cbContent           The size of the content.
 * @param   off                 The offset into the system area.
 */
RTDECL(int) RTFsIsoMakerSetSysAreaContent(RTFSISOMAKER hIsoMaker, void const *pvContent, size_t cbContent, uint32_t off);

/**
 * Resolves a path into a object ID.
 *
 * This will be doing the looking up using the specified object names rather
 * than the version adjusted and mangled according to the namespace setup.
 *
 * @returns The object ID corresponding to @a pszPath, or UINT32_MAX if not
 *          found or invalid parameters.
 * @param   hIsoMaker           The ISO maker instance.
 * @param   fNamespaces         The namespace to resolve @a pszPath in.  It's
 *                              possible to specify multiple namespaces here, of
 *                              course, but that's inefficient.
 * @param   pszPath             The path to the object.
 */
RTDECL(uint32_t) RTFsIsoMakerGetObjIdxForPath(RTFSISOMAKER hIsoMaker, uint32_t fNamespaces, const char *pszPath);

/**
 * Queries the configuration index of the boot catalog file object.
 *
 * The boot catalog file is created as necessary, thus this have to be a query
 * rather than a getter since object creation may fail.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pidxObj             Where to return the configuration index.
 */
RTDECL(int) RTFsIsoMakerQueryObjIdxForBootCatalog(RTFSISOMAKER hIsoMaker, uint32_t *pidxObj);

/**
 * Removes the specified object from the image.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker instance.
 * @param   idxObj              The index of the object to remove.
 */
RTDECL(int) RTFsIsoMakerObjRemove(RTFSISOMAKER hIsoMaker, uint32_t idxObj);

/**
 * Sets the path (name) of an object in the selected namespaces.
 *
 * The name will be transformed as necessary.
 *
 * The initial implementation does not allow this function to be called more
 * than once on an object.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index of to name.
 * @param   fNamespaces         The namespaces to apply the path to
 *                              (RTFSISOMAKER_NAMESPACE_XXX).
 * @param   pszPath             The path.
 */
RTDECL(int) RTFsIsoMakerObjSetPath(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint32_t fNamespaces, const char *pszPath);

/**
 * Sets the name of an object in the selected namespaces, placing it under the
 * given directory.
 *
 * The name will be transformed as necessary.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index of to name.
 * @param   idxParentObj        The parent directory object.
 * @param   fNamespaces         The namespaces to apply the path to
 *                              (RTFSISOMAKER_NAMESPACE_XXX).
 * @param   pszName             The name.
 */
RTDECL(int) RTFsIsoMakerObjSetNameAndParent(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint32_t idxParentObj,
                                            uint32_t fNamespaces, const char *pszName);

/**
 * Enables or disable syslinux boot info table patching of a file.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index.
 * @param   fEnable             Whether to enable or disable patching.
 */
RTDECL(int) RTFsIsoMakerObjEnableBootInfoTablePatching(RTFSISOMAKER hIsoMaker, uint32_t idxObj, bool fEnable);

/**
 * Gets the data size of an object.
 *
 * Currently only supported on file objects.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxObj              The configuration index.
 * @param   pcbData             Where to return the size.
 */
RTDECL(int) RTFsIsoMakerObjQueryDataSize(RTFSISOMAKER hIsoMaker, uint32_t idxObj, uint64_t *pcbData);

/**
 * Adds an unnamed directory to the image.
 *
 * The directory must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddDir, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedDir(RTFSISOMAKER hIsoMaker, uint32_t *pidxObj);

/**
 * Adds a directory to the image in all namespaces and default attributes.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pszDir              The path (UTF-8) to the directory in the ISO.
 *
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.  Optional.
 * @sa      RTFsIsoMakerAddUnnamedDir, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddDir(RTFSISOMAKER hIsoMaker, const char *pszDir, uint32_t *pidxObj);

/**
 * Adds an unnamed file to the image that's backed by a host file.
 *
 * The file must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pszSrcFile          The source file path.  VFS chain spec allowed.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddFile, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedFileWithSrcPath(RTFSISOMAKER hIsoMaker, const char *pszSrcFile, uint32_t *pidxObj);

/**
 * Adds an unnamed file to the image that's backed by a VFS file.
 *
 * The file must explictly be entered into the desired namespaces.
 *
 * @returns IPRT status code
 * @param   hIsoMaker           The ISO maker handle.
 * @param   hVfsFileSrc         The source file handle.
 * @param   pidxObj             Where to return the configuration index of the
 *                              directory.
 * @sa      RTFsIsoMakerAddUnnamedFileWithSrcPath, RTFsIsoMakerObjSetPath
 */
RTDECL(int) RTFsIsoMakerAddUnnamedFileWithVfsFile(RTFSISOMAKER hIsoMaker, RTVFSFILE hVfsFileSrc, uint32_t *pidxObj);

/**
 * Adds a file that's backed by a host file to the image in all namespaces and
 * with attributes taken from the source file.
 *
 * @returns IPRT status code
 * @param   hIsoMaker       The ISO maker handle.
 * @param   pszFile         The path to the file in the image.
 * @param   pszSrcFile      The source file path.  VFS chain spec allowed.
 * @param   pidxObj         Where to return the configuration index of the file.
 *                          Optional
 * @sa      RTFsIsoMakerAddFileWithVfsFile,
 *          RTFsIsoMakerAddUnnamedFileWithSrcPath
 */
RTDECL(int) RTFsIsoMakerAddFileWithSrcPath(RTFSISOMAKER hIsoMaker, const char *pszFile, const char *pszSrcFile, uint32_t *pidxObj);

/**
 * Adds a file that's backed by a VFS file to the image in all namespaces and
 * with attributes taken from the source file.
 *
 * @returns IPRT status code
 * @param   hIsoMaker       The ISO maker handle.
 * @param   pszFile         The path to the file in the image.
 * @param   hVfsFileSrc     The source file handle.
 * @param   pidxObj         Where to return the configuration index of the file.
 *                          Optional.
 * @sa      RTFsIsoMakerAddUnnamedFileWithVfsFile,
 *          RTFsIsoMakerAddFileWithSrcPath
 */
RTDECL(int) RTFsIsoMakerAddFileWithVfsFile(RTFSISOMAKER hIsoMaker, const char *pszFile, RTVFSFILE hVfsFileSrc, uint32_t *pidxObj);


/**
 * Set the validation entry of the boot catalog (this is the first entry).
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idPlatform          The platform ID
 *                              (ISO9660_ELTORITO_PLATFORM_ID_XXX).
 * @param   pszString           CD/DVD-ROM identifier.  Optional.
 */
RTDECL(int) RTFsIsoMakerBootCatSetValidationEntry(RTFSISOMAKER hIsoMaker, uint8_t idPlatform, const char *pszString);

/**
 * Set the validation entry of the boot catalog (this is the first entry).
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxBootCat          The boot catalog entry.  Zero and two are
 *                              invalid.  Must be less than 63.
 * @param   idxImageObj         The configuration index of the boot image.
 * @param   bBootMediaType      The media type and flag (not for entry 1)
 *                              (ISO9660_ELTORITO_BOOT_MEDIA_TYPE_XXX,
 *                              ISO9660_ELTORITO_BOOT_MEDIA_F_XXX).
 * @param   bSystemType         The partitiona table system ID.
 * @param   fBootable           Whether it's a bootable entry or if we just want
 *                              the BIOS to setup the emulation without booting
 *                              it.
 * @param   uLoadSeg            The load address divided by 0x10 (i.e. the real
 *                              mode segment number).
 * @param   cSectorsToLoad      Number of emulated sectors to load.
 */
RTDECL(int) RTFsIsoMakerBootCatSetSectionEntry(RTFSISOMAKER hIsoMaker, uint32_t idxBootCat, uint32_t idxImageObj,
                                               uint8_t bBootMediaType, uint8_t bSystemType, bool fBootable,
                                               uint16_t uLoadSeg, uint16_t cSectorsToLoad);

/**
 * Set the validation entry of the boot catalog (this is the first entry).
 *
 * @returns IPRT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   idxBootCat          The boot catalog entry.
 * @param   cEntries            Number of entries in the section.
 * @param   idPlatform          The platform ID
 *                              (ISO9660_ELTORITO_PLATFORM_ID_XXX).
 * @param   pszString           Section identifier or something.  Optional.
 */
RTDECL(int) RTFsIsoMakerBootCatSetSectionHeaderEntry(RTFSISOMAKER hIsoMaker, uint32_t idxBootCat, uint32_t cEntries,
                                                     uint8_t idPlatform, const char *pszString);

/**
 * Imports an existing ISO.
 *
 * Just like other source files, the existing image must remain present and
 * unmodified till the ISO maker is done with it.
 *
 * @returns IRPT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pszIso              Path to the existing image to import / clone.
 * @param   fFlags              Reserved for the future, MBZ.
 * @param   pErrInfo            Where to return additional error information.
 *                              Optional.
 */
RTDECL(int) RTFsIsoMakerImport(RTFSISOMAKER hIsoMaker, const char *pszIso, uint32_t fFlags, PRTERRINFO pErrInfo);

/** @name RTFSISOMK_IMPORT_F_XXX - Flags for RTFsIsoMakerImport.
 * @{ */
#define RTFSISOMK_IMPORT_F_NO_PRIMARY_ISO   RT_BIT_32(0)  /**< Skip the primary ISO-9660 namespace (rock ridge included). */
#define RTFSISOMK_IMPORT_F_NO_JOLIET        RT_BIT_32(1)  /**< Skip the joliet namespace. */
#define RTFSISOMK_IMPORT_F_NO_ROCK_RIDGE    RT_BIT_32(2)  /**< Skip rock ridge (both primary and joliet). */
#define RTFSISOMK_IMPORT_F_NO_UDF           RT_BIT_32(3)  /**< Skip the UDF namespace. */
#define RTFSISOMK_IMPORT_F_NO_HFS           RT_BIT_32(4)  /**< Skip the HFS namespace. */
#define RTFSISOMK_IMPORT_F_NO_BOOT          RT_BIT_32(5)  /**< Skip importing El Torito boot stuff. */
#define RTFSISOMK_IMPORT_F_NO_SYS_AREA      RT_BIT_32(6)  /**< Skip importing the system area (first 32KB). */
#define RTFSISOMK_IMPORT_F_VALID_MASK       UINT32_C(0x0000007f)
/** @} */


/**
 * Finalizes the image.
 *
 * @returns IPRT status code.
 * @param   hIsoMaker       The ISO maker handle.
 */
RTDECL(int) RTFsIsoMakerFinalize(RTFSISOMAKER hIsoMaker);

/**
 * Creates a VFS file for a finalized ISO maker instanced.
 *
 * The file can be used to access the image.  Both sequential and random access
 * are supported, so that this could in theory be hooked up to a CD/DVD-ROM
 * drive emulation and used as a virtual ISO image.
 *
 * @returns IRPT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   phVfsFile           Where to return the handle.
 */
RTDECL(int) RTFsIsoMakerCreateVfsOutputFile(RTFSISOMAKER hIsoMaker, PRTVFSFILE phVfsFile);



/**
 * ISO maker command (creates image file on disk).
 *
 * @returns IPRT status code
 * @param   cArgs               Number of arguments.
 * @param   papszArgs           Pointer to argument array.
 */
RTDECL(RTEXITCODE) RTFsIsoMakerCmd(unsigned cArgs, char **papszArgs);

/**
 * Extended ISO maker command.
 *
 * This can be used as a ISO maker command that produces a image file, or
 * alternatively for setting up a virtual ISO in memory.
 *
 * @returns IPRT status code
 * @param   cArgs               Number of arguments.
 * @param   papszArgs           Pointer to argument array.
 * @param   phVfsFile           Where to return the virtual ISO.  Pass NULL to
 *                              for normal operation (creates file on disk).
 * @param   pErrInfo            Where to return extended error information in
 *                              the virtual ISO mode.
 */
RTDECL(int) RTFsIsoMakerCmdEx(unsigned cArgs, char **papszArgs, PRTVFSFILE phVfsFile, PRTERRINFO pErrInfo);


/** @} */

RT_C_DECLS_END

#endif

