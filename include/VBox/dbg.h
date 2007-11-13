/** @file
 * Debugger Interfaces.
 *
 * This header covers all external interfaces of the Debugger module.
 * However, it does not cover the DBGF interface since that part of the
 * VMM. Use dbgf.h for that.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_dbg_h
#define ___VBox_dbg_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/dbgf.h>

#include <iprt/stdarg.h>

__BEGIN_DECLS

/** @def VBOX_WITH_DEBUGGER
 * The build is with debugger module. Test if this is defined before
 * registering external debugger commands.
 */
#ifndef VBOX_WITH_DEBUGGER
# ifdef DEBUG
#  define VBOX_WITH_DEBUGGER
# endif
#endif


/**
 * DBGC variable category.
 *
 * Used to describe an argument to a command or function and a functions
 * return value.
 */
typedef enum DBGCVARCAT
{
    /** Any type is fine. */
    DBGCVAR_CAT_ANY = 0,
    /** Any kind of pointer. */
    DBGCVAR_CAT_POINTER,
    /** Any kind of pointer with no range option. */
    DBGCVAR_CAT_POINTER_NO_RANGE,
    /** GC pointer. */
    DBGCVAR_CAT_GC_POINTER,
    /** GC pointer with no range option. */
    DBGCVAR_CAT_GC_POINTER_NO_RANGE,
    /** Numeric argument. */
    DBGCVAR_CAT_NUMBER,
    /** Numeric argument with no range option. */
    DBGCVAR_CAT_NUMBER_NO_RANGE,
    /** String. */
    DBGCVAR_CAT_STRING,
    /** Symbol. */
    DBGCVAR_CAT_SYMBOL,
    /** Option. */
    DBGCVAR_CAT_OPTION,
    /** Option + string. */
    DBGCVAR_CAT_OPTION_STRING,
    /** Option + number. */
    DBGCVAR_CAT_OPTION_NUMBER
} DBGCVARCAT;


/**
 * DBGC variable type.
 */
typedef enum DBGCVARTYPE
{
    /** unknown... */
    DBGCVAR_TYPE_UNKNOWN = 0,
    /** Flat GC pointer. */
    DBGCVAR_TYPE_GC_FLAT,
    /** Segmented GC pointer. */
    DBGCVAR_TYPE_GC_FAR,
    /** Physical GC pointer. */
    DBGCVAR_TYPE_GC_PHYS,
    /** Flat HC pointer. */
    DBGCVAR_TYPE_HC_FLAT,
    /** Segmented HC pointer. */
    DBGCVAR_TYPE_HC_FAR,
    /** Physical HC pointer. */
    DBGCVAR_TYPE_HC_PHYS,
    /** String. */
    DBGCVAR_TYPE_STRING,
    /** Number. */
    DBGCVAR_TYPE_NUMBER,
    /** Symbol. */
    DBGCVAR_TYPE_SYMBOL,
    /** Special type used when querying symbols. */
    DBGCVAR_TYPE_ANY
} DBGCVARTYPE;

/** @todo Rename to DBGCVAR_IS_xyz. */

/** Checks if the specified variable type is of a pointer persuasion. */
#define DBGCVAR_ISPOINTER(enmType)      ((enmType) >= DBGCVAR_TYPE_GC_FLAT && enmType <= DBGCVAR_TYPE_HC_PHYS)
/** Checks if the specified variable type is of a pointer persuasion. */
#define DBGCVAR_IS_FAR_PTR(enmType)     ((enmType) == DBGCVAR_TYPE_GC_FAR || (enmType) == DBGCVAR_TYPE_HC_FAR)
/** Checks if the specified variable type is of a pointer persuasion and of the guest context sort. */
#define DBGCVAR_ISGCPOINTER(enmType)    ((enmType) >= DBGCVAR_TYPE_GC_FLAT && (enmType) <= DBGCVAR_TYPE_GC_PHYS)
/** Checks if the specified variable type is of a pointer persuasion and of the host context sort. */
#define DBGCVAR_ISHCPOINTER(enmType)    ((enmType) >= DBGCVAR_TYPE_HC_FLAT && (enmType) <= DBGCVAR_TYPE_HC_PHYS)


/**
 * DBGC variable range type.
 */
typedef enum DBGCVARRANGETYPE
{
    /** No range appliable or no range specified. */
    DBGCVAR_RANGE_NONE = 0,
    /** Number of elements. */
    DBGCVAR_RANGE_ELEMENTS,
    /** Number of bytes. */
    DBGCVAR_RANGE_BYTES
} DBGCVARRANGETYPE;


/**
 * Variable descriptor.
 */
typedef struct DBGCVARDESC
{
    /** The minimal number of times this argument may occur.
     * Use 0 here to inidicate that the argument is optional. */
    unsigned    cTimesMin;
    /** Maximum number of occurences.
     * Use ~0 here to indicate infinite. */
    unsigned    cTimesMax;
    /** Argument category. */
    DBGCVARCAT  enmCategory;
    /** Flags, DBGCVD_FLAGS_* */
    unsigned    fFlags;
    /** Argument name. */
    const char *pszName;
    /** Argument name. */
    const char *pszDescription;
} DBGCVARDESC;
/** Pointer to an argument descriptor. */
typedef DBGCVARDESC *PDBGCVARDESC;
/** Pointer to a const argument descriptor. */
typedef const DBGCVARDESC *PCDBGCVARDESC;

/** Variable descriptor flags.
 * @{ */
/** Indicates that the variable depends on the previous being present. */
#define DBGCVD_FLAGS_DEP_PREV       RT_BIT(1)
/** @} */


/**
 * DBGC variable.
 */
typedef struct DBGCVAR
{
    /** Pointer to the argument descriptor. */
    PCDBGCVARDESC   pDesc;
    /** Pointer to the next argument. */
    struct DBGCVAR *pNext;

    /** Argument type. */
    DBGCVARTYPE     enmType;
    /** Type specific. */
    union
    {
        /** Flat GC Address.        (DBGCVAR_TYPE_GC_FLAT) */
        RTGCPTR         GCFlat;
        /** Far (16:32) GC Address. (DBGCVAR_TYPE_GC_FAR) */
        RTFAR32         GCFar;
        /** Physical GC Address.    (DBGCVAR_TYPE_GC_PHYS) */
        RTGCPHYS        GCPhys;
        /** Flat HC Address.        (DBGCVAR_TYPE_HC_FLAT) */
        void           *pvHCFlat;
        /** Far (16:32) HC Address. (DBGCVAR_TYPE_HC_FAR) */
        RTFAR32         HCFar;
        /** Physical GC Address.    (DBGCVAR_TYPE_HC_PHYS) */
        RTHCPHYS        HCPhys;
        /** String.                 (DBGCVAR_TYPE_STRING)
         * The basic idea is the the this is a pointer to the expression we're
         * parsing, so no messing with freeing. */
        const char     *pszString;
        /** Number.                 (DBGCVAR_TYPE_NUMBER) */
        uint64_t        u64Number;
    } u;

    /** Range type. */
    DBGCVARRANGETYPE    enmRangeType;
    /** Range. The use of the content depends on the enmRangeType. */
    uint64_t            u64Range;
} DBGCVAR;
/** Pointer to a command argument. */
typedef DBGCVAR *PDBGCVAR;
/** Pointer to a const command argument. */
typedef const DBGCVAR *PCDBGCVAR;


/**
 * Macro for initializing a DBGC variable with defaults.
 * The result is an unknown variable type without any range.
 */
#define DBGCVAR_INIT(pVar) \
        do { \
            (pVar)->pDesc = NULL;\
            (pVar)->pNext = NULL; \
            (pVar)->enmType = DBGCVAR_TYPE_UNKNOWN; \
            (pVar)->u.u64Number = 0; \
            (pVar)->enmRangeType = DBGCVAR_RANGE_NONE; \
            (pVar)->u64Range = 0; \
        } while (0)

/**
 * Macro for initializing a DBGC variable with a HC physical address.
 */
#define DBGCVAR_INIT_HC_PHYS(pVar, Phys) \
        do { \
            DBGCVAR_INIT(pVar); \
            (pVar)->enmType = DBGCVAR_TYPE_HC_PHYS; \
            (pVar)->u.HCPhys = (Phys); \
        } while (0)

/**
 * Macro for initializing a DBGC variable with a HC flat address.
 */
#define DBGCVAR_INIT_HC_FLAT(pVar, Flat) \
        do { \
            DBGCVAR_INIT(pVar); \
            (pVar)->enmType = DBGCVAR_TYPE_HC_FLAT; \
            (pVar)->u.pvHCFlat = (Flat); \
        } while (0)

/**
 * Macro for initializing a DBGC variable with a GC physical address.
 */
#define DBGCVAR_INIT_GC_PHYS(pVar, Phys) \
        do { \
            DBGCVAR_INIT(pVar); \
            (pVar)->enmType = DBGCVAR_TYPE_GC_PHYS; \
            (pVar)->u.GCPhys = (Phys); \
        } while (0)

/**
 * Macro for initializing a DBGC variable with a GC flat address.
 */
#define DBGCVAR_INIT_GC_FLAT(pVar, Flat) \
        do { \
            DBGCVAR_INIT(pVar); \
            (pVar)->enmType = DBGCVAR_TYPE_GC_FLAT; \
            (pVar)->u.GCFlat = (Flat); \
        } while (0)

/**
 * Macro for initializing a DBGC variable with a GC far address.
 */
#define DBGCVAR_INIT_GC_FAR(pVar, _sel, _off) \
        do { \
            DBGCVAR_INIT(pVar); \
            (pVar)->enmType = DBGCVAR_TYPE_GC_FAR; \
            (pVar)->u.GCFar.sel = (_sel); \
            (pVar)->u.GCFar.off = (_off); \
        } while (0)

/**
 * Macro for initializing a DBGC variable with a number
 */
#define DBGCVAR_INIT_NUMBER(pVar, Value) \
        do { \
            DBGCVAR_INIT(pVar); \
            (pVar)->enmType = DBGCVAR_TYPE_NUMBER; \
            (pVar)->u.u64 = (Value); \
        } while (0)


/** Pointer to helper functions for commands. */
typedef struct DBGCCMDHLP *PDBGCCMDHLP;

/**
 * Command helper for writing text to the debug console.
 *
 * @returns VBox status.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pvBuf       What to write.
 * @param   cbBuf       Number of bytes to write.
 * @param   pcbWritten  Where to store the number of bytes actually written.
 *                      If NULL the entire buffer must be successfully written.
 */
typedef DECLCALLBACK(int) FNDBGCHLPWRITE(PDBGCCMDHLP pCmdHlp, const void *pvBuf, size_t cbBuf, size_t *pcbWritten);
/** Pointer to a FNDBGCHLPWRITE() function. */
typedef FNDBGCHLPWRITE *PFNDBGCHLPWRITE;

/**
 * Command helper for writing formatted text to the debug console.
 *
 * @returns VBox status.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pcb         Where to store the number of bytes written.
 * @param   pszFormat   The format string.
 *                      This is using the log formatter, so it's format extensions can be used.
 * @param   ...         Arguments specified in the format string.
 */
typedef DECLCALLBACK(int) FNDBGCHLPPRINTF(PDBGCCMDHLP pCmdHlp, size_t *pcbWritten, const char *pszFormat, ...);
/** Pointer to a FNDBGCHLPPRINTF() function. */
typedef FNDBGCHLPPRINTF *PFNDBGCHLPPRINTF;

/**
 * Command helper for writing formatted text to the debug console.
 *
 * @returns VBox status.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pcb         Where to store the number of bytes written.
 * @param   pszFormat   The format string.
 *                      This is using the log formatter, so it's format extensions can be used.
 * @param   args        Arguments specified in the format string.
 */
typedef DECLCALLBACK(int) FNDBGCHLPPRINTFV(PDBGCCMDHLP pCmdHlp, size_t *pcbWritten, const char *pszFormat, va_list args);
/** Pointer to a FNDBGCHLPPRINTFV() function. */
typedef FNDBGCHLPPRINTFV *PFNDBGCHLPPRINTFV;

/**
 * Command helper for formatting and error message for a VBox status code.
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   rc          The VBox status code.
 * @param   pcb         Where to store the number of bytes written.
 * @param   pszFormat   Format string for additional messages. Can be NULL.
 * @param   ...         Format arguments, optional.
 */
typedef DECLCALLBACK(int) FNDBGCHLPVBOXERROR(PDBGCCMDHLP pCmdHlp, int rc, const char *pszFormat, ...);
/** Pointer to a FNDBGCHLPVBOXERROR() function. */
typedef FNDBGCHLPVBOXERROR *PFNDBGCHLPVBOXERROR;

/**
 * Command helper for formatting and error message for a VBox status code.
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   rc          The VBox status code.
 * @param   pcb         Where to store the number of bytes written.
 * @param   pszFormat   Format string for additional messages. Can be NULL.
 * @param   args        Format arguments, optional.
 */
typedef DECLCALLBACK(int) FNDBGCHLPVBOXERRORV(PDBGCCMDHLP pCmdHlp, int rc, const char *pszFormat, va_list args);
/** Pointer to a FNDBGCHLPVBOXERRORV() function. */
typedef FNDBGCHLPVBOXERRORV *PFNDBGCHLPVBOXERRORV;

/**
 * Command helper for reading memory specified by a DBGC variable.
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pVM         VM handle if GC or physical HC address.
 * @param   pvBuffer    Where to store the read data.
 * @param   cbRead      Number of bytes to read.
 * @param   pVarPointer DBGC variable specifying where to start reading.
 * @param   pcbRead     Where to store the number of bytes actually read.
 *                      This optional, but it's useful when read GC virtual memory where a
 *                      page in the requested range might not be present.
 *                      If not specified not-present failure or end of a HC physical page
 *                      will cause failure.
 */
typedef DECLCALLBACK(int) FNDBGCHLPMEMREAD(PDBGCCMDHLP pCmdHlp, PVM pVM, void *pvBuffer, size_t cbRead, PCDBGCVAR pVarPointer, size_t *pcbRead);
/** Pointer to a FNDBGCHLPMEMREAD() function. */
typedef FNDBGCHLPMEMREAD *PFNDBGCHLPMEMREAD;

/**
 * Command helper for writing memory specified by a DBGC variable.
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pVM         VM handle if GC or physical HC address.
 * @param   pvBuffer    What to write.
 * @param   cbWrite     Number of bytes to write.
 * @param   pVarPointer DBGC variable specifying where to start reading.
 * @param   pcbWritten  Where to store the number of bytes written.
 *                      This is optional. If NULL be aware that some of the buffer
 *                      might have been written to the specified address.
 */
typedef DECLCALLBACK(int) FNDBGCHLPMEMWRITE(PDBGCCMDHLP pCmdHlp, PVM pVM, const void *pvBuffer, size_t cbWrite, PCDBGCVAR pVarPointer, size_t *pcbWritten);
/** Pointer to a FNDBGCHLPMEMWRITE() function. */
typedef FNDBGCHLPMEMWRITE *PFNDBGCHLPMEMWRITE;


/**
 * Evaluates an expression.
 * (Hopefully the parser and functions are fully reentrant.)
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pResult     Where to store the result.
 * @param   pszExpr     The expression. Format string with the format DBGC extensions.
 * @param   ...         Format arguments.
 */
typedef DECLCALLBACK(int) FNDBGCHLPEVAL(PDBGCCMDHLP pCmdHlp, PDBGCVAR pResult, const char *pszExpr, ...);
/** Pointer to a FNDBGCHLPEVAL() function. */
typedef FNDBGCHLPEVAL *PFNDBGCHLPEVAL;


/**
 * Executes command an expression.
 * (Hopefully the parser and functions are fully reentrant.)
 *
 * @returns VBox status code appropriate to return from a command.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pszExpr     The expression. Format string with the format DBGC extensions.
 * @param   ...         Format arguments.
 */
typedef DECLCALLBACK(int) FNDBGCHLPEXEC(PDBGCCMDHLP pCmdHlp, const char *pszExpr, ...);
/** Pointer to a FNDBGCHLPEVAL() function. */
typedef FNDBGCHLPEXEC *PFNDBGCHLPEXEC;


/**
 * Helper functions for commands.
 */
typedef struct DBGCCMDHLP
{
    /** Pointer to a FNDBCHLPWRITE() function. */
    PFNDBGCHLPWRITE         pfnWrite;
    /** Pointer to a FNDBGCHLPPRINTF() function. */
    PFNDBGCHLPPRINTF        pfnPrintf;
    /** Pointer to a FNDBGCHLPPRINTFV() function. */
    PFNDBGCHLPPRINTFV       pfnPrintfV;
    /** Pointer to a FNDBGCHLPVBOXERROR() function. */
    PFNDBGCHLPVBOXERROR     pfnVBoxError;
    /** Pointer to a FNDBGCHLPVBOXERRORV() function. */
    PFNDBGCHLPVBOXERRORV    pfnVBoxErrorV;
    /** Pointer to a FNDBGCHLPMEMREAD() function. */
    PFNDBGCHLPMEMREAD       pfnMemRead;
    /** Pointer to a FNDBGCHLPMEMWRITE() function. */
    PFNDBGCHLPMEMWRITE      pfnMemWrite;
    /** Pointer to a FNDBGCHLPEVAL() function. */
    PFNDBGCHLPEVAL          pfnEval;
    /** Pointer to a FNDBGCHLPEXEC() function. */
    PFNDBGCHLPEXEC          pfnExec;

    /**
     * Converts a DBGC variable to a DBGF address structure.
     *
     * @returns VBox status code.
     * @param   pCmdHlp     Pointer to the command callback structure.
     * @param   pVar        The variable to convert.
     * @param   pAddress    The target address.
     */
    DECLCALLBACKMEMBER(int, pfnVarToDbgfAddr)(PDBGCCMDHLP pCmdHlp, PCDBGCVAR pVar, PDBGFADDRESS pAddress);

    /**
     * Converts a DBGC variable to a boolean.
     *
     * @returns VBox status code.
     * @param   pCmdHlp     Pointer to the command callback structure.
     * @param   pVar        The variable to convert.
     * @param   pf          Where to store the boolean.
     */
    DECLCALLBACKMEMBER(int, pfnVarToBool)(PDBGCCMDHLP pCmdHlp, PCDBGCVAR pVar, bool *pf);

} DBGCCMDHLP;


#ifdef IN_RING3
/**
 * Command helper for writing formatted text to the debug console.
 *
 * @returns VBox status.
 * @param   pCmdHlp     Pointer to the command callback structure.
 * @param   pszFormat   The format string.
 *                      This is using the log formatter, so it's format extensions can be used.
 * @param   ...         Arguments specified in the format string.
 */
DECLINLINE(int) DBGCCmdHlpPrintf(PDBGCCMDHLP pCmdHlp, const char *pszFormat, ...)
{
    va_list va;
    int rc;
    va_start(va, pszFormat);
    rc = pCmdHlp->pfnPrintfV(pCmdHlp, NULL, pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * @copydoc FNDBGCHLPVBOXERROR
 */
DECLINLINE(int) DBGCCmdHlpVBoxError(PDBGCCMDHLP pCmdHlp, int rc, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    rc = pCmdHlp->pfnVBoxErrorV(pCmdHlp, rc, pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * @copydoc FNDBGCHLPMEMREAD
 */
DECLINLINE(int) DBGCCmdHlpMemRead(PDBGCCMDHLP pCmdHlp, PVM pVM, void *pvBuffer, size_t cbRead, PCDBGCVAR pVarPointer, size_t *pcbRead)
{
    return pCmdHlp->pfnMemRead(pCmdHlp, pVM, pvBuffer, cbRead, pVarPointer, pcbRead);
}
#endif /* IN_RING3 */


/** Pointer to command descriptor. */
typedef struct DBGCCMD *PDBGCCMD;
/** Pointer to const command descriptor. */
typedef const struct DBGCCMD *PCDBGCCMD;

/**
 * Command handler.
 *
 * The console will call the handler for a command once it's finished
 * parsing the user input. The command handler function is responsible
 * for executing the command itself.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 * @param   pResult     Where to store the result. NULL if no result descriptor was specified.
 */
typedef DECLCALLBACK(int) FNDBGCCMD(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR pArgs, unsigned cArgs, PDBGCVAR pResult);
/** Pointer to a FNDBGCCMD() function. */
typedef FNDBGCCMD *PFNDBGCCMD;

/**
 * DBGC command descriptor.
 *
 * If a pResultDesc is specified the command can be called and used
 * as a function too. If it's a pure function, set fFlags to
 * DBGCCMD_FLAGS_FUNCTION.
 */
typedef struct DBGCCMD
{
    /** Command string. */
    const char     *pszCmd;
    /** Minimum number of arguments. */
    unsigned        cArgsMin;
    /** Max number of arguments. */
    unsigned        cArgsMax;
    /** Argument descriptors (array). */
    PCDBGCVARDESC   paArgDescs;
    /** Number of argument descriptors. */
    unsigned        cArgDescs;
    /** Result descriptor. */
    PCDBGCVARDESC   pResultDesc;
    /** flags. (reserved for now) */
    unsigned        fFlags;
    /** Handler function. */
    PFNDBGCCMD      pfnHandler;
    /** Command syntax. */
    const char     *pszSyntax;
    /** Command description. */
    const char     *pszDescription;
} DBGCCMD;

/** DBGCCMD Flags.
 * @{
 */
/** The description is of a pure function which cannot be invoked
 * as a command from the commandline. */
#define DBGCCMD_FLAGS_FUNCTION  1
/** @} */



/** Pointer to a DBGC backend. */
typedef struct DBGCBACK *PDBGCBACK;

/**
 * Checks if there is input.
 *
 * @returns true if there is input ready.
 * @returns false if there not input ready.
 * @param   pBack       Pointer to the backend structure supplied by
 *                      the backend. The backend can use this to find
 *                      it's instance data.
 * @param   cMillies    Number of milliseconds to wait on input data.
 */
typedef DECLCALLBACK(bool) FNDBGCBACKINPUT(PDBGCBACK pBack, uint32_t cMillies);
/** Pointer to a FNDBGCBACKINPUT() callback. */
typedef FNDBGCBACKINPUT *PFNDBGCBACKINPUT;

/**
 * Read input.
 *
 * @returns VBox status code.
 * @param   pBack       Pointer to the backend structure supplied by
 *                      the backend. The backend can use this to find
 *                      it's instance data.
 * @param   pvBuf       Where to put the bytes we read.
 * @param   cbBuf       Maximum nymber of bytes to read.
 * @param   pcbRead     Where to store the number of bytes actually read.
 *                      If NULL the entire buffer must be filled for a
 *                      successful return.
 */
typedef DECLCALLBACK(int) FNDBGCBACKREAD(PDBGCBACK pBack, void *pvBuf, size_t cbBuf, size_t *pcbRead);
/** Pointer to a FNDBGCBACKREAD() callback. */
typedef FNDBGCBACKREAD *PFNDBGCBACKREAD;

/**
 * Write (output).
 *
 * @returns VBox status code.
 * @param   pBack       Pointer to the backend structure supplied by
 *                      the backend. The backend can use this to find
 *                      it's instance data.
 * @param   pvBuf       What to write.
 * @param   cbBuf       Number of bytes to write.
 * @param   pcbWritten  Where to store the number of bytes actually written.
 *                      If NULL the entire buffer must be successfully written.
 */
typedef DECLCALLBACK(int) FNDBGCBACKWRITE(PDBGCBACK pBack, const void *pvBuf, size_t cbBuf, size_t *pcbWritten);
/** Pointer to a FNDBGCBACKWRITE() callback. */
typedef FNDBGCBACKWRITE *PFNDBGCBACKWRITE;


/**
 * The communication backend provides the console with a number of callbacks
 * which can be used
 */
typedef struct DBGCBACK
{
    /** Check for input. */
    PFNDBGCBACKINPUT    pfnInput;
    /** Read input. */
    PFNDBGCBACKREAD     pfnRead;
    /** Write output. */
    PFNDBGCBACKWRITE    pfnWrite;
} DBGCBACK;


/**
 * Make a console instance.
 *
 * This will not return until either an 'exit' command is issued or a error code
 * indicating connection loss is encountered.
 *
 * @returns VINF_SUCCESS if console termination caused by the 'exit' command.
 * @returns The VBox status code causing the console termination.
 *
 * @param   pVM         VM Handle.
 * @param   pBack       Pointer to the backend structure. This must contain
 *                      a full set of function pointers to service the console.
 * @param   fFlags      Reserved, must be zero.
 * @remark  A forced termination of the console is easiest done by forcing the
 *          callbacks to return fatal failures.
 */
DBGDECL(int)    DBGCCreate(PVM pVM, PDBGCBACK pBack, unsigned fFlags);


/**
 * Register one or more external commands.
 *
 * @returns VBox status.
 * @param   paCommands      Pointer to an array of command descriptors.
 *                          The commands must be unique. It's not possible
 *                          to register the same commands more than once.
 * @param   cCommands       Number of commands.
 */
DBGDECL(int)    DBGCRegisterCommands(PCDBGCCMD paCommands, unsigned cCommands);


/**
 * Deregister one or more external commands previously registered by
 * DBGCRegisterCommands().
 *
 * @returns VBox status.
 * @param   paCommands      Pointer to an array of command descriptors
 *                          as given to DBGCRegisterCommands().
 * @param   cCommands       Number of commands.
 */
DBGDECL(int)    DBGCDeregisterCommands(PCDBGCCMD paCommands, unsigned cCommands);


/**
 * Spawns a new thread with a TCP based debugging console service.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   ppvData     Where to store the pointer to instance data.
 */
DBGDECL(int)    DBGCTcpCreate(PVM pVM, void **ppvUser);

/**
 * Terminates any running TCP base debugger consolse service.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvData      Instance data set by DBGCTcpCreate().
 */
DBGDECL(int)    DBGCTcpTerminate(PVM pVM, void *pvData);


__END_DECLS

#endif
