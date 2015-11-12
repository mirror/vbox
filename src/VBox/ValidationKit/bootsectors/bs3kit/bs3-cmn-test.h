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
extern bool                 g_fbBs3VMMDevTesting;

/** The number of tests that have failed. */
extern uint16_t             g_uscBs3TestErrors;

/** The start error count of the current subtest. */
extern uint16_t             g_uscBs3SubTestAtErrors;

/**  Whether we've reported the sub-test result or not. */
extern bool                 g_fbBs3SubTestReported;

/** The number of sub tests. */
extern uint16_t             g_uscBs3SubTests;

/** The number of sub tests that failed. */
extern uint16_t             g_uscBs3SubTestsFailed;

/** VMMDEV_TESTING_UNIT_XXX -> string */
extern char const           g_aszBs3TestUnitNames[][16];

/** The test name. */
extern const char BS3_FAR  *g_pszBs3Test_c16;
extern const char          *g_pszBs3Test_c32;
extern const char          *g_pszBs3Test_c64;
/** The subtest name. */
extern const char BS3_FAR  *g_pszBs3SubTest_c16;
extern const char          *g_pszBs3SubTest_c32;
extern const char          *g_pszBs3SubTest_c64;


/**
 * Sends a command to VMMDev followed by a single string.
 *
 * If the VMMDev is not present or is not being used, this function will
 * do nothing.
 *
 * @param   uCmd       The command.
 * @param   pszString  The string.
 */
BS3_DECL(void) bs3TestSendStrCmd_c16(uint32_t uCmd, const char BS3_FAR *pszString);
BS3_DECL(void) bs3TestSendStrCmd_c32(uint32_t uCmd, const char BS3_FAR *pszString); /**< @copydoc bs3TestSendStrCmd_c16 */
BS3_DECL(void) bs3TestSendStrCmd_c64(uint32_t uCmd, const char BS3_FAR *pszString); /**< @copydoc bs3TestSendStrCmd_c16 */
#define bs3TestSendStrCmd BS3_CMN_NM(bs3TestSendStrCmd) /**< Selects #bs3TestSendStrCmd_c16, #bs3TestSendStrCmd_c32 or #bs3TestSendStrCmd_c64. */


/**
 * Checks if the VMMDev is configured for testing.
 *
 * @returns true / false.
 */
BS3_DECL(bool) bs3TestIsVmmDevTestingPresent_c16(void);
BS3_DECL(bool) bs3TestIsVmmDevTestingPresent_c32(void); /**< @copydoc bs3TestIsVmmDevTestingPresent_c16 */
BS3_DECL(bool) bs3TestIsVmmDevTestingPresent_c64(void); /**< @copydoc bs3TestIsVmmDevTestingPresent_c16 */
#define bs3TestIsVmmDevTestingPresent BS3_CMN_NM(bs3TestIsVmmDevTestingPresent) /**< Selects #bs3TestIsVmmDevTestingPresent_c16, #bs3TestIsVmmDevTestingPresent_c32 or #bs3TestIsVmmDevTestingPresent_c64. */


#endif

