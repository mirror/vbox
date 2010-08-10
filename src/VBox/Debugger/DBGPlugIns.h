/* $Id$ */
/** @file
 * DBGPlugIns - Debugger Plug-Ins.
 *
 * This is just a temporary static wrapper for what may eventually
 * become some fancy dynamic stuff.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef ___Debugger_DBGPlugIns_h
#define ___Debugger_DBGPlugIns_h

#include <VBox/dbgf.h>

RT_C_DECLS_BEGIN

extern const DBGFOSREG g_DBGDiggerFreeBSD;
extern const DBGFOSREG g_DBGDiggerLinux;
extern const DBGFOSREG g_DBGDiggerOS2;
extern const DBGFOSREG g_DBGDiggerSolaris;
extern const DBGFOSREG g_DBGDiggerWinNt;

RT_C_DECLS_END

#endif

