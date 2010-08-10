/* $Id$ */
/** @file
 * VBox Debugger GUI - Disassembly View.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef ___Debugger_VBoxDbgDisas_h
#define ___Debugger_VBoxDbgDisas_h

/**
 * Feature list:
 *  - Combobox (or some entry field with history similar to the command) for
 *    entering the address. Should support registers and other symbols, and
 *    possibly also grok various operators like the debugger command line.
 *    => Needs to make the DBGC evaluator available somehow.
 *  - Refresh manual/interval (for EIP or other non-fixed address expression).
 *  - Scrollable - down is not an issue, up is a bit more difficult.
 *  - Hide/Unhide PATM patches (jumps/int3/whatever) button in the guest disas.
 *  - Drop down for selecting mixed original guest disas and PATM/REM disas
 *    (Guest Only, Guest+PATM, Guest+REM).
 *
 */
class VBoxDbgDisas
{

};

#endif

