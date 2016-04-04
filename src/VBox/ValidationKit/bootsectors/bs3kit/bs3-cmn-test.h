/* $Id$ */
/** @file
 * BS3Kit - Bs3Test internal header.
 */

/*
 * Copyright (C) 2007-2015 Oracle Corporation
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

#ifndef ___bs3_cmn_test_h
#define ___bs3_cmn_test_h

#include "bs3kit.h"
#include <VBox/VMMDevTesting.h>


/** Indicates whether the VMMDev is operational. */
#ifndef DOXYGEN_RUNNING
# define g_fbBs3VMMDevTesting BS3_DATA_NM(g_fbBs3VMMDevTesting)
#endif
extern bool                 g_fbBs3VMMDevTesting;

/** The number of tests that have failed. */
#ifndef DOXYGEN_RUNNING
# define g_cusBs3TestErrors BS3_DATA_NM(g_cusBs3TestErrors)
#endif
extern uint16_t             g_cusBs3TestErrors;

/** The start error count of the current subtest. */
#ifndef DOXYGEN_RUNNING
# define g_cusBs3SubTestAtErrors BS3_DATA_NM(g_cusBs3SubTestAtErrors)
#endif
extern uint16_t             g_cusBs3SubTestAtErrors;

/**  Whether we've reported the sub-test result or not. */
#ifndef DOXYGEN_RUNNING
# define g_fbBs3SubTestReported BS3_DATA_NM(g_fbBs3SubTestReported)
#endif
extern bool                 g_fbBs3SubTestReported;
/** Whether the sub-test has been skipped or not. */
#ifndef DOXYGEN_RUNNING
# define g_fbBs3SubTestSkipped BS3_DATA_NM(g_fbBs3SubTestSkipped)
#endif
extern bool                 g_fbBs3SubTestSkipped;

/** The number of sub tests. */
#ifndef DOXYGEN_RUNNING
# define g_cusBs3SubTests   BS3_DATA_NM(g_cusBs3SubTests)
#endif
extern uint16_t             g_cusBs3SubTests;

/** The number of sub tests that failed. */
#ifndef DOXYGEN_RUNNING
# define g_cusBs3SubTestsFailed BS3_DATA_NM(g_cusBs3SubTestsFailed)
#endif
extern uint16_t             g_cusBs3SubTestsFailed;

/** VMMDEV_TESTING_UNIT_XXX -> string */
#ifndef DOXYGEN_RUNNING
# define g_aszBs3TestUnitNames BS3_DATA_NM(g_aszBs3TestUnitNames)
#endif
extern char const           g_aszBs3TestUnitNames[][16];

/** The test name. */
extern const char BS3_FAR  *g_pszBs3Test_c16;
extern const char          *g_pszBs3Test_c32;
extern const char          *g_pszBs3Test_c64;

/** The subtest name. */
#ifndef DOXYGEN_RUNNING
# define g_szBs3SubTest     BS3_DATA_NM(g_szBs3SubTest)
#endif
extern char                 g_szBs3SubTest[64];


/**
 * Sends a command to VMMDev followed by a single string.
 *
 * If the VMMDev is not present or is not being used, this function will
 * do nothing.
 *
 * @param   uCmd        The command.
 * @param   pszString   The string.
 */
BS3_DECL(void) bs3TestSendCmdWithStr_c16(uint32_t uCmd, const char BS3_FAR *pszString);
BS3_DECL(void) bs3TestSendCmdWithStr_c32(uint32_t uCmd, const char BS3_FAR *pszString); /**< @copydoc bs3TestSendCmdWithStr_c16 */
BS3_DECL(void) bs3TestSendCmdWithStr_c64(uint32_t uCmd, const char BS3_FAR *pszString); /**< @copydoc bs3TestSendCmdWithStr_c16 */
#define bs3TestSendCmdWithStr BS3_CMN_NM(bs3TestSendCmdWithStr) /**< Selects #bs3TestSendCmdWithStr_c16, #bs3TestSendCmdWithStr_c32 or #bs3TestSendCmdWithStr_c64. */

/**
 * Sends a command to VMMDev followed by a 32-bit unsigned integer value.
 *
 * If the VMMDev is not present or is not being used, this function will
 * do nothing.
 *
 * @param   uCmd        The command.
 * @param   uValue      The value.
 */
BS3_DECL(void) bs3TestSendCmdWithU32_c16(uint32_t uCmd, uint32_t uValue);
BS3_DECL(void) bs3TestSendCmdWithU32_c32(uint32_t uCmd, uint32_t uValue); /**< @copydoc bs3TestSendCmdWithU32_c16 */
BS3_DECL(void) bs3TestSendCmdWithU32_c64(uint32_t uCmd, uint32_t uValue); /**< @copydoc bs3TestSendCmdWithU32_c16 */
#define bs3TestSendCmdWithU32 BS3_CMN_NM(bs3TestSendCmdWithU32) /**< Selects #bs3TestSendCmdWithU32_c16, #bs3TestSendCmdWithU32_c32 or #bs3TestSendCmdWithU32_c64. */

/**
 * Checks if the VMMDev is configured for testing.
 *
 * @returns true / false.
 */
BS3_DECL(bool) bs3TestIsVmmDevTestingPresent_c16(void);
BS3_DECL(bool) bs3TestIsVmmDevTestingPresent_c32(void); /**< @copydoc bs3TestIsVmmDevTestingPresent_c16 */
BS3_DECL(bool) bs3TestIsVmmDevTestingPresent_c64(void); /**< @copydoc bs3TestIsVmmDevTestingPresent_c16 */
#define bs3TestIsVmmDevTestingPresent BS3_CMN_NM(bs3TestIsVmmDevTestingPresent) /**< Selects #bs3TestIsVmmDevTestingPresent_c16, #bs3TestIsVmmDevTestingPresent_c32 or #bs3TestIsVmmDevTestingPresent_c64. */

/**
 * Similar to rtTestSubCleanup.
 */
BS3_DECL(void) bs3TestSubCleanup_c16(void);
BS3_DECL(void) bs3TestSubCleanup_c32(void); /**< @copydoc bs3TestSubCleanup_c16 */
BS3_DECL(void) bs3TestSubCleanup_c64(void); /**< @copydoc bs3TestSubCleanup_c16 */
#define bs3TestSubCleanup BS3_CMN_NM(bs3TestSubCleanup) /**< Selects #bs3TestSubCleanup_c16, #bs3TestSubCleanup_c32 or #bs3TestSubCleanup_c64. */

/**
 * @impl_callback_method{FNBS3STRFORMATOUTPUT,
 *      Used by Bs3TestFailedV and Bs3TestSkippedV.
 *
 *      The @a pvUser parameter must point to a boolean that was initialized to false. }
 */
BS3_DECL_CALLBACK(size_t) bs3TestFailedStrOutput_c16(char ch, void BS3_FAR *pvUser);
BS3_DECL_CALLBACK(size_t) bs3TestFailedStrOutput_c32(char ch, void BS3_FAR *pvUser);
BS3_DECL_CALLBACK(size_t) bs3TestFailedStrOutput_c64(char ch, void BS3_FAR *pvUser);
#define bs3TestFailedStrOutput BS3_CMN_NM(bs3TestFailedStrOutput) /**< Selects #bs3TestFailedStrOutput_c16, #bs3TestFailedStrOutput_c32 or #bs3TestFailedStrOutput_c64. */

#endif

