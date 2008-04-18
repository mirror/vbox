/** $Id$ */
/** @file
 * DBGC - Debugger Console, Internal Header File.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___DBGCInternal_h
#define ___DBGCInternal_h


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/dbg.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/* to err.h! */
#define VERR_DBGC_QUIT                          (-11999)
#define VERR_PARSE_FIRST                        (-11000)
#define VERR_PARSE_TOO_FEW_ARGUMENTS            (VERR_PARSE_FIRST - 0)
#define VERR_PARSE_TOO_MANY_ARGUMENTS           (VERR_PARSE_FIRST - 1)
#define VERR_PARSE_ARGUMENT_OVERFLOW            (VERR_PARSE_FIRST - 2)
#define VERR_PARSE_ARGUMENT_TYPE_MISMATCH       (VERR_PARSE_FIRST - 3)
#define VERR_PARSE_NO_RANGE_ALLOWED             (VERR_PARSE_FIRST - 4)
#define VERR_PARSE_UNBALANCED_QUOTE             (VERR_PARSE_FIRST - 5)
#define VERR_PARSE_UNBALANCED_PARENTHESIS       (VERR_PARSE_FIRST - 6)
#define VERR_PARSE_EMPTY_ARGUMENT               (VERR_PARSE_FIRST - 7)
#define VERR_PARSE_UNEXPECTED_OPERATOR          (VERR_PARSE_FIRST - 8)
#define VERR_PARSE_INVALID_NUMBER               (VERR_PARSE_FIRST - 9)
#define VERR_PARSE_NUMBER_TOO_BIG               (VERR_PARSE_FIRST - 10)
#define VERR_PARSE_INVALID_OPERATION            (VERR_PARSE_FIRST - 11)
#define VERR_PARSE_FUNCTION_NOT_FOUND           (VERR_PARSE_FIRST - 12)
#define VERR_PARSE_NOT_A_FUNCTION               (VERR_PARSE_FIRST - 13)
#define VERR_PARSE_NO_MEMORY                    (VERR_PARSE_FIRST - 14)
#define VERR_PARSE_INCORRECT_ARG_TYPE           (VERR_PARSE_FIRST - 15)
#define VERR_PARSE_VARIABLE_NOT_FOUND           (VERR_PARSE_FIRST - 16)
#define VERR_PARSE_CONVERSION_FAILED            (VERR_PARSE_FIRST - 17)
#define VERR_PARSE_NOT_IMPLEMENTED              (VERR_PARSE_FIRST - 18)
#define VERR_PARSE_BAD_RESULT_TYPE              (VERR_PARSE_FIRST - 19)
#define VERR_PARSE_WRITEONLY_SYMBOL             (VERR_PARSE_FIRST - 20)
#define VERR_PARSE_NO_ARGUMENT_MATCH            (VERR_PARSE_FIRST - 21)
#define VINF_PARSE_COMMAND_NOT_FOUND            (VERR_PARSE_FIRST - 22)
#define VINF_PARSE_INVALD_COMMAND_NAME          (VERR_PARSE_FIRST - 23)
#define VERR_PARSE_LAST                         (VERR_PARSE_FIRST - 30)

#define VWRN_DBGC_CMD_PENDING                   12000
#define VWRN_DBGC_ALREADY_REGISTERED            12001
#define VERR_DBGC_COMMANDS_NOT_REGISTERED       (-12002)
#define VERR_DBGC_BP_NOT_FOUND                  (-12003)
#define VERR_DBGC_BP_EXISTS                     (-12004)
#define VINF_DBGC_BP_NO_COMMAND                 12005
#define VERR_DBGC_COMMAND_FAILED                (-12006)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Debugger console per breakpoint data.
 */
typedef struct DBGCBP
{
    /** Pointer to the next breakpoint in the list. */
    struct DBGCBP  *pNext;
    /** The breakpoint identifier. */
    RTUINT          iBp;
    /** The size of the command. */
    size_t          cchCmd;
    /** The command to execute when the breakpoint is hit. */
    char            szCmd[1];
} DBGCBP;
/** Pointer to a breakpoint. */
typedef DBGCBP *PDBGCBP;


/**
 * Named variable.
 *
 * Always allocated from heap in one signle block.
 */
typedef struct DBGCNAMEDVAR
{
    /** The variable. */
    DBGCVAR     Var;
    /** It's name. */
    char        szName[1];
} DBGCNAMEDVAR;
/** Pointer to named variable. */
typedef DBGCNAMEDVAR *PDBGCNAMEDVAR;


/**
 * Debugger console status
 */
typedef enum DBGCSTATUS
{
    /** Normal status, .*/
    DBGC_HALTED

} DBGCSTATUS;


/**
 * Debugger console instance data.
 */
typedef struct DBGC
{
    /** Command helpers. */
    DBGCCMDHLP          CmdHlp;
    /** Pointer to backend callback structure. */
    PDBGCBACK           pBack;

    /** Pointer to the current VM. */
    PVM                 pVM;
    /** The current debugger emulation. */
    const char         *pszEmulation;
    /** Pointer to the command and functions for the current debugger emulation. */
    PCDBGCCMD           paEmulationCmds;
    /** The number of commands paEmulationCmds points to. */
    unsigned            cEmulationCmds;
    /** Log indicator. (If set we're writing the log to the console.) */
    bool                fLog;

    /** Indicates whether we're in guest (true) or hypervisor (false) register context. */
    bool                fRegCtxGuest;
    /** Indicates whether the register are terse or sparse. */
    bool                fRegTerse;

    /** Current dissassembler position. */
    DBGCVAR             DisasmPos;
    /** Current source position. (flat GC) */
    DBGCVAR             SourcePos;
    /** Current memory dump position. */
    DBGCVAR             DumpPos;
    /** Size of the previous dump element. */
    unsigned            cbDumpElement;

    /** Number of variables in papVars. */
    unsigned            cVars;
    /** Array of global variables.
     * Global variables can be referenced using the $ operator and set
     * and unset using command with those names. */
    PDBGCNAMEDVAR      *papVars;

    /** The list of breakpoints. (singly linked) */
    PDBGCBP             pFirstBp;

    /** Save search pattern. */
    uint8_t             abSearch[256];
    /** The length of the search pattern. */
    uint32_t            cbSearch;
    /** The search unit */
    uint32_t            cbSearchUnit;
    /** The max hits. */
    uint64_t            cMaxSearchHits;
    /** The address to resume searching from. */
    DBGFADDRESS         SearchAddr;
    /** What's left of the original search range. */
    RTGCUINTPTR         cbSearchRange;

    /** @name Parsing and Execution
     * @{ */

    /** Input buffer. */
    char                achInput[2048];
    /** To ease debugging. */
    unsigned            uInputZero;
    /** Write index in the input buffer. */
    unsigned            iWrite;
    /** Read index in the input buffer. */
    unsigned            iRead;
    /** The number of lines in the buffer. */
    unsigned            cInputLines;
    /** Indicates that we have a buffer overflow condition.
     * This means that input is ignored up to the next newline. */
    bool                fInputOverflow;
    /** Indicates whether or we're ready for input. */
    bool                fReady;

    /** Scratch buffer position. */
    char               *pszScratch;
    /** Scratch buffer. */
    char                achScratch[16384];
    /** Argument array position. */
    unsigned            iArg;
    /** Array of argument variables. */
    DBGCVAR             aArgs[100];

    /** rc from the last dbgcHlpPrintfV(). */
    int                 rcOutput;
    /** rc from the last command. */
    int                 rcCmd;
    /** @} */
} DBGC;
/** Pointer to debugger console instance data. */
typedef DBGC *PDBGC;

/** Converts a Command Helper pointer to a pointer to DBGC instance data. */
#define DBGC_CMDHLP2DBGC(pCmdHlp)   ( (PDBGC)((uintptr_t)(pCmdHlp) - RT_OFFSETOF(DBGC, CmdHlp)) )


/**
 * Chunk of external commands.
 */
typedef struct DBGCEXTCMDS
{
    /** Number of commands descriptors. */
    unsigned            cCmds;
    /** Pointer to array of command descriptors. */
    PCDBGCCMD           paCmds;
    /** Pointer to the next chunk. */
    struct DBGCEXTCMDS *pNext;
} DBGCEXTCMDS;
/** Pointer to chunk of external commands. */
typedef DBGCEXTCMDS *PDBGCEXTCMDS;



/**
 * Unary operator handler function.
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg        The argument.
 * @param   pResult     Where to store the result.
 */
typedef DECLCALLBACK(int) FNDBGCOPUNARY(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
/** Pointer to a unary operator handler function. */
typedef FNDBGCOPUNARY *PFNDBGCOPUNARY;


/**
 * Binary operator handler function.
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pDbgc       Debugger console instance data.
 * @param   pArg1       The first argument.
 * @param   pArg2       The 2nd argument.
 * @param   pResult     Where to store the result.
 */
typedef DECLCALLBACK(int) FNDBGCOPBINARY(PDBGC pDbgc, PCDBGCVAR pArg1, PCDBGCVAR pArg2, PDBGCVAR pResult);
/** Pointer to a binary operator handler function. */
typedef FNDBGCOPBINARY *PFNDBGCOPBINARY;


/**
 * Operator descriptor.
 */
typedef struct DBGCOP
{
    /** Operator mnemonic. */
    char            szName[4];
    /** Length of name. */
    const unsigned  cchName;
    /** Whether or not this is a binary operator.
     * Unary operators are evaluated right-to-left while binary are left-to-right. */
    bool            fBinary;
    /** Precedence level. */
    unsigned        iPrecedence;
    /** Unary operator handler. */
    PFNDBGCOPUNARY  pfnHandlerUnary;
    /** Binary operator handler. */
    PFNDBGCOPBINARY pfnHandlerBinary;
    /** Operator description. */
    const char     *pszDescription;
} DBGCOP;
/** Pointer to an operator descriptor. */
typedef DBGCOP *PDBGCOP;
/** Pointer to a const operator descriptor. */
typedef const DBGCOP *PCDBGCOP;



/** Pointer to symbol descriptor. */
typedef struct DBGCSYM *PDBGCSYM;
/** Pointer to const symbol descriptor. */
typedef const struct DBGCSYM *PCDBGCSYM;

/**
 * Get builtin symbol.
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pSymDesc    Pointer to the symbol descriptor.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   enmType     The result type.
 * @param   pResult     Where to store the result.
 */
typedef DECLCALLBACK(int) FNDBGCSYMGET(PCDBGCSYM pSymDesc, PDBGCCMDHLP pCmdHlp, DBGCVARTYPE enmType, PDBGCVAR pResult);
/** Pointer to get function for a builtin symbol. */
typedef FNDBGCSYMGET *PFNDBGCSYMGET;

/**
 * Set builtin symbol.
 *
 * @returns 0 on success.
 * @returns VBox evaluation / parsing error code on failure.
 *          The caller does the bitching.
 * @param   pSymDesc    Pointer to the symbol descriptor.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pValue      The value to assign the symbol.
 */
typedef DECLCALLBACK(int) FNDBGCSYMSET(PCDBGCSYM pSymDesc, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pValue);
/** Pointer to set function for a builtin symbol. */
typedef FNDBGCSYMSET *PFNDBGCSYMSET;


/**
 * Symbol description (for builtin symbols).
 */
typedef struct DBGCSYM
{
    /** Symbol name. */
    const char     *pszName;
    /** Get function. */
    PFNDBGCSYMGET   pfnGet;
    /** Set function. (NULL if readonly) */
    PFNDBGCSYMSET   pfnSet;
    /** User data. */
    unsigned        uUser;
} DBGCSYM;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
int     dbgcBpAdd(PDBGC pDbgc, RTUINT iBp, const char *pszCmd);
int     dbgcBpUpdate(PDBGC pDbgc, RTUINT iBp, const char *pszCmd);
int     dbgcBpDelete(PDBGC pDbgc, RTUINT iBp);
PDBGCBP dbgcBpGet(PDBGC pDbgc, RTUINT iBp);
int     dbgcBpExec(PDBGC pDbgc, RTUINT iBp);

void    dbgcVarInit(PDBGCVAR pVar);
void    dbgcVarSetGCFlat(PDBGCVAR pVar, RTGCPTR GCFlat);
void    dbgcVarSetGCFlatByteRange(PDBGCVAR pVar, RTGCPTR GCFlat, uint64_t cb);
void    dbgcVarSetU64(PDBGCVAR pVar, uint64_t u64);
void    dbgcVarSetVar(PDBGCVAR pVar, PCDBGCVAR pVar2);
void    dbgcVarSetDbgfAddr(PDBGCVAR pVar, PCDBGFADDRESS pAddress);
void    dbgcVarSetNoRange(PDBGCVAR pVar);
void    dbgcVarSetByteRange(PDBGCVAR pVar, uint64_t cb);
int     dbgcVarToDbgfAddr(PDBGC pDbgc, PCDBGCVAR pVar, PDBGFADDRESS pAddress);

int     dbgcEvalSub(PDBGC pDbgc, char *pszExpr, size_t cchExpr, PDBGCVAR pResult);
int     dbgcProcessCommand(PDBGC pDbgc, char *pszCmd, size_t cchCmd, bool fNoExecute);

int     dbgcSymbolGet(PDBGC pDbgc, const char *pszSymbol, DBGCVARTYPE enmType, PDBGCVAR pResult);
PCDBGCSYM   dbgcLookupRegisterSymbol(PDBGC pDbgc, const char *pszSymbol);
PCDBGCOP    dbgcOperatorLookup(PDBGC pDbgc, const char *pszExpr, bool fPreferBinary, char chPrev);
PCDBGCCMD   dbgcRoutineLookup(PDBGC pDbgc, const char *pachName, size_t cchName, bool fExternal);

DECLCALLBACK(int) dbgcOpAddrFlat(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
DECLCALLBACK(int) dbgcOpAddrHost(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
DECLCALLBACK(int) dbgcOpAddrPhys(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);
DECLCALLBACK(int) dbgcOpAddrHostPhys(PDBGC pDbgc, PCDBGCVAR pArg, PDBGCVAR pResult);

void    dbgcInitCmdHlp(PDBGC pDbgc);

/* For tstDBGCParser: */
int     dbgcCreate(PDBGC *ppDbgc, PDBGCBACK pBack, unsigned fFlags);
int     dbgcRun(PDBGC pDbgc);
int     dbgcProcessInput(PDBGC pDbgc, bool fNoExecute);
void    dbgcDestroy(PDBGC pDbgc);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
extern const DBGCCMD    g_aCmds[];
extern const unsigned   g_cCmds;
extern const DBGCCMD    g_aCmdsCodeView[];
extern const unsigned   g_cCmdsCodeView;
extern const DBGCOP     g_aOps[];
extern const unsigned   g_cOps;


#endif
