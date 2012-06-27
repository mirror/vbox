/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef CR_ERROR_H
#define CR_ERROR_H

#include <iprt/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __GNUC__
#define NORETURN_PRINTF
#define PRINTF
#elif defined IN_GUEST
#define NORETURN_PRINTF __attribute__ ((__noreturn__,format(printf,1,2)))
#define PRINTF __attribute__ ((format(printf,1,2)))
#else
#define NORETURN_PRINTF
#define PRINTF
#endif

DECLEXPORT(void) crEnableWarnings(int onOff);

DECLEXPORT(void) crDebug(const char *format, ... ) PRINTF;
DECLEXPORT(void) crWarning(const char *format, ... ) PRINTF;
DECLEXPORT(void) crInfo(const char *format, ... ) PRINTF;

DECLEXPORT(void) crError(const char *format, ... ) NORETURN_PRINTF;

/* Throw more info while opengl is not stable */
#if defined(DEBUG) || 1
# ifdef DEBUG_misha
#  include <iprt/assert.h>
#  define CRASSERT Assert
//extern int g_VBoxFbgFBreakDdi;
#  define CR_DDI_PROLOGUE() do { /*if (g_VBoxFbgFBreakDdi) {Assert(0);}*/ } while (0)
# else
#  define CRASSERT( PRED ) ((PRED)?(void)0:crWarning( "Assertion failed: %s, file %s, line %d", #PRED, __FILE__, __LINE__))
#  define CR_DDI_PROLOGUE() do {} while (0)
# endif
# define THREADASSERT( PRED ) ((PRED)?(void)0:crError( "Are you trying to run a threaded app ?\nBuild with 'make threadsafe'\nAssertion failed: %s, file %s, line %d", #PRED, __FILE__, __LINE__))
#else
# define CRASSERT( PRED ) ((void)0)
# define THREADASSERT( PRED ) ((void)0)
# define CR_DDI_PROLOGUE() do {} while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* CR_ERROR_H */
