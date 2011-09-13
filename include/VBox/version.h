/** @file
 * VBox Version Management.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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

#ifndef ___VBox_version_h
#define ___VBox_version_h

/* Product info. */
#include <product-generated.h>

#ifndef RC_INVOKED
# include <version-generated.h>

/** Combined version number. */
# define VBOX_VERSION                    (VBOX_VERSION_MAJOR << 16 | VBOX_VERSION_MINOR)
/** Get minor version from combined version. */
# define VBOX_GET_VERSION_MINOR(uVer)    ((uVer) & 0xffff)
/** Get major version from combined version. */
# define VBOX_GET_VERSION_MAJOR(uVer)    ((uVer) >> 16)

/**
 * Make a full version number.
 *
 * The returned number can be used in normal integer comparsions and will yield
 * the expected results.
 *
 * @param   uMajor      The major version number.
 * @param   uMinor      The minor version number.
 * @param   uBuild      The build number.
 * @returns Full version number.
 */
# define VBOX_FULL_VERSION_MAKE(uMajor, uMinor, uBuild) \
    (  (uint32_t)((uMajor) &   0xff) << 24 \
     | (uint32_t)((uMinor) &   0xff) << 16 \
     | (uint32_t)((uMinor) & 0xffff)       \
    )

/** Combined version number. */
# define VBOX_FULL_VERSION              \
    VBOX_FULL_VERSION_MAKE(VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD)
/** Get the major version number from a VBOX_FULL_VERSION style number. */
# define VBOX_FULL_VERSION_GET_MAJOR(uFullVer)  ( ((uFullVer) >> 24) &   0xffU )
/** Get the minor version number from a VBOX_FULL_VERSION style number. */
# define VBOX_FULL_VERSION_GET_MINOR(uFullVer)  ( ((uFullVer) >> 16) &   0xffU )
/** Get the build version number from a VBOX_FULL_VERSION style number. */
# define VBOX_FULL_VERSION_GET_BUILD(uFullVer)  ( ((uFullVer)      ) & 0xffffU )

/**
 * Make a short version number for use in 16 bit version fields.
 *
 * The returned number can be used in normal integer comparsions and will yield
 * the expected results.
 *
 * @param   uMajor      The major version number.
 * @param   uMinor      The minor version number.
 * @returns Short version number.
 */
# define VBOX_SHORT_VERSION_MAKE(uMajor, uMinor) \
    (  (uint16_t)((uMajor) &   0xff) << 8 \
     | (uint16_t)((uMinor) &   0xff)      \
    )

/** Combined short version number. */
# define VBOX_SHORT_VERSION               \
    VBOX_SHORT_VERSION_MAKE(VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR)
/** Get the major version number from a VBOX_SHORT_VERSION style number. */
# define VBOX_SHORT_VERSION_GET_MAJOR(uShortVer)  ( ((uShortVer) >> 8) &   0xffU )
/** Get the minor version number from a VBOX_SHORT_VERSION style number. */
# define VBOX_SHORT_VERSION_GET_MINOR(uShortVer)  ( (uShortVer) &   0xffU )

#endif /* !RC_INVOKED */

/** @name Prefined strings for Windows resource files
 *
 * @remarks The VBOX_VERSION_*_NR define are integer numbers while
 *          VBOX_VERSION_* are strings when using the resource compile.
 *          Kind of confusing...
 *
 * @{ */
#define VBOX_RC_COMPANY_NAME            VBOX_VENDOR
#define VBOX_RC_LEGAL_COPYRIGHT         "Copyright (C) 2009-" VBOX_C_YEAR " Oracle Corporation\0"
#define VBOX_RC_PRODUCT_VERSION         VBOX_VERSION_MAJOR_NR , VBOX_VERSION_MINOR_NR , 0 , 0
#define VBOX_RC_FILE_VERSION            VBOX_VERSION_MAJOR_NR , VBOX_VERSION_MINOR_NR , 0 , 0
/** @} */

/** @todo Clean up the resource compiler mess where we cannot include
 *        version-generated.h and requires two files. */

#endif

