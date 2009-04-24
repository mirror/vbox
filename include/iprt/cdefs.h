/** @file
 * IPRT - Common C and C++ definitions.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___iprt_cdefs_h
#define ___iprt_cdefs_h


/** @defgroup grp_rt_cdefs  IPRT Common Definitions and Macros
 * @{
 */

/*
 * Include sys/cdefs.h if present, if not define the stuff we need.
 */
#ifdef HAVE_SYS_CDEFS_H
# if defined(RT_ARCH_LINUX) && defined(__KERNEL__)
#  error "oops"
# endif
# include <sys/cdefs.h>
#else

 /** @def __BEGIN_DECLS
  * Used to start a block of function declarations which are shared
  * between C and C++ program.
  */

 /** @def __END_DECLS
  * Used to end a block of function declarations which are shared
  * between C and C++ program.
  */

 #if defined(__cplusplus)
 # define __BEGIN_DECLS extern "C" {
 # define __END_DECLS   }
 #else
 # define __BEGIN_DECLS
 # define __END_DECLS
 #endif

#endif


/*
 * Shut up DOXYGEN warnings and guide it properly thru the code.
 */
#ifdef DOXYGEN_RUNNING
#define __AMD64__
#define __X86__
#define RT_ARCH_AMD64
#define RT_ARCH_X86
#define IN_RING0
#define IN_RING3
#define IN_RC
#define IN_RC
#define IN_RT_GC
#define IN_RT_R0
#define IN_RT_R3
#define IN_RT_STATIC
#define RT_STRICT
#define Breakpoint
#define RT_NO_DEPRECATED_MACROS
#endif /* DOXYGEN_RUNNING */

/** @def RT_ARCH_X86
 * Indicates that we're compiling for the X86 architecture.
 */

/** @def RT_ARCH_AMD64
 * Indicates that we're compiling for the AMD64 architecture.
 */
#if !defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)
# if defined(__amd64__) || defined(__x86_64__) || defined(_M_X64) || defined(__AMD64__)
#  define RT_ARCH_AMD64
# elif defined(__i386__) || defined(_M_IX86) || defined(__X86__)
#  define RT_ARCH_X86
# else /* PORTME: append test for new archs. */
#  error "Check what predefined macros your compiler uses to indicate architecture."
# endif
#elif defined(RT_ARCH_X86) && defined(RT_ARCH_AMD64) /* PORTME: append new archs. */
# error "Both RT_ARCH_X86 and RT_ARCH_AMD64 cannot be defined at the same time!"
#endif


/** @def __X86__
 * Indicates that we're compiling for the X86 architecture.
 * @deprecated
 */

/** @def __AMD64__
 * Indicates that we're compiling for the AMD64 architecture.
 * @deprecated
 */
#if !defined(__X86__) && !defined(__AMD64__)
# if defined(RT_ARCH_AMD64)
#  define __AMD64__
# elif defined(RT_ARCH_X86)
#  define __X86__
# else
#  error "Check what predefined macros your compiler uses to indicate architecture."
# endif
#elif defined(__X86__) && defined(__AMD64__)
# error "Both __X86__ and __AMD64__ cannot be defined at the same time!"
#elif defined(__X86__) && !defined(RT_ARCH_X86)
# error "Both __X86__ without RT_ARCH_X86!"
#elif defined(__AMD64__) && !defined(RT_ARCH_AMD64)
# error "Both __AMD64__ without RT_ARCH_AMD64!"
#endif

/** @def IN_RING0
 * Used to indicate that we're compiling code which is running
 * in Ring-0 Host Context.
 */

/** @def IN_RING3
 * Used to indicate that we're compiling code which is running
 * in Ring-3 Host Context.
 */

/** @def IN_RC
 * Used to indicate that we're compiling code which is running
 * in the Raw-mode Context (implies R0).
 */
#if !defined(IN_RING3) && !defined(IN_RING0) && !defined(IN_RC) && !defined(IN_RC)
# error "You must define which context the compiled code should run in; IN_RING3, IN_RING0 or IN_RC"
#endif
#if (defined(IN_RING3) && (defined(IN_RING0) || defined(IN_RC)) ) \
 || (defined(IN_RING0) && (defined(IN_RING3) || defined(IN_RC)) ) \
 || (defined(IN_RC)    && (defined(IN_RING3) || defined(IN_RING0)) )
# error "Only one of the IN_RING3, IN_RING0, IN_RC defines should be defined."
#endif


/** @def ARCH_BITS
 * Defines the bit count of the current context.
 */
#if !defined(ARCH_BITS) || defined(DOXYGEN_RUNNING)
# if defined(RT_ARCH_AMD64)
#  define ARCH_BITS 64
# else
#  define ARCH_BITS 32
# endif
#endif

/** @def HC_ARCH_BITS
 * Defines the host architecture bit count.
 */
#if !defined(HC_ARCH_BITS) || defined(DOXYGEN_RUNNING)
# ifndef IN_RC
#  define HC_ARCH_BITS ARCH_BITS
# else
#  define HC_ARCH_BITS 32
# endif
#endif

/** @def GC_ARCH_BITS
 * Defines the guest architecture bit count.
 */
#if !defined(GC_ARCH_BITS) && !defined(DOXYGEN_RUNNING)
# ifdef VBOX_WITH_64_BITS_GUESTS
#  define GC_ARCH_BITS  64
# else
#  define GC_ARCH_BITS  32
# endif
#endif

/** @def R3_ARCH_BITS
 * Defines the host ring-3 architecture bit count.
 */
#if !defined(R3_ARCH_BITS) || defined(DOXYGEN_RUNNING)
# ifdef IN_RING3
#  define R3_ARCH_BITS ARCH_BITS
# else
#  define R3_ARCH_BITS HC_ARCH_BITS
# endif
#endif

/** @def R0_ARCH_BITS
 * Defines the host ring-0 architecture bit count.
 */
#if !defined(R0_ARCH_BITS) || defined(DOXYGEN_RUNNING)
# ifdef IN_RING0
#  define R0_ARCH_BITS ARCH_BITS
# else
#  define R0_ARCH_BITS HC_ARCH_BITS
# endif
#endif

/** @def GC_ARCH_BITS
 * Defines the guest architecture bit count.
 */
#if !defined(GC_ARCH_BITS) || defined(DOXYGEN_RUNNING)
# ifdef IN_RC
#  define GC_ARCH_BITS ARCH_BITS
# else
#  define GC_ARCH_BITS 32
# endif
#endif


/** @def CTXTYPE
 * Declare a type differently in GC, R3 and R0.
 *
 * @param   GCType  The GC type.
 * @param   R3Type  The R3 type.
 * @param   R0Type  The R0 type.
 * @remark  For pointers used only in one context use RCPTRTYPE(), R3R0PTRTYPE(), R3PTRTYPE() or R0PTRTYPE().
 */
#ifdef IN_RC
# define CTXTYPE(GCType, R3Type, R0Type)  GCType
#elif defined(IN_RING3)
# define CTXTYPE(GCType, R3Type, R0Type)  R3Type
#else
# define CTXTYPE(GCType, R3Type, R0Type)  R0Type
#endif

/** @def RCPTRTYPE
 * Declare a pointer which is used in the raw mode context but appears in structure(s) used by
 * both HC and RC. The main purpose is to make sure structures have the same
 * size when built for different architectures.
 *
 * @param   RCType  The RC type.
 */
#define RCPTRTYPE(RCType)       CTXTYPE(RCType, RTRCPTR, RTRCPTR)

/** @def R3R0PTRTYPE
 * Declare a pointer which is used in HC, is explicitly valid in ring 3 and 0,
 * but appears in structure(s) used by both HC and GC. The main purpose is to
 * make sure structures have the same size when built for different architectures.
 *
 * @param   R3R0Type  The R3R0 type.
 * @remarks This used to be called HCPTRTYPE.
 */
#define R3R0PTRTYPE(R3R0Type)   CTXTYPE(RTHCPTR, R3R0Type, R3R0Type)

/** @def R3PTRTYPE
 * Declare a pointer which is used in R3 but appears in structure(s) used by
 * both HC and GC. The main purpose is to make sure structures have the same
 * size when built for different architectures.
 *
 * @param   R3Type  The R3 type.
 */
#define R3PTRTYPE(R3Type)       CTXTYPE(RTHCUINTPTR, R3Type, RTHCUINTPTR)

/** @def R0PTRTYPE
 * Declare a pointer which is used in R0 but appears in structure(s) used by
 * both HC and GC. The main purpose is to make sure structures have the same
 * size when built for different architectures.
 *
 * @param   R0Type  The R0 type.
 */
#define R0PTRTYPE(R0Type)       CTXTYPE(RTHCUINTPTR, RTHCUINTPTR, R0Type)

/** @def CTXSUFF
 * Adds the suffix of the current context to the passed in
 * identifier name. The suffix is HC or GC.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   var     Identifier name.
 * @deprecated Use CTX_SUFF. Do NOT use this for new code.
 */
/** @def OTHERCTXSUFF
 * Adds the suffix of the other context to the passed in
 * identifier name. The suffix is HC or GC.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   var     Identifier name.
 * @deprecated Use CTX_SUFF. Do NOT use this for new code.
 */
#ifdef IN_RC
# define CTXSUFF(var)       var##GC
# define OTHERCTXSUFF(var)  var##HC
#else
# define CTXSUFF(var)       var##HC
# define OTHERCTXSUFF(var)  var##GC
#endif

/** @def CTXALLSUFF
 * Adds the suffix of the current context to the passed in
 * identifier name. The suffix is R3, R0 or GC.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   var     Identifier name.
 * @deprecated Use CTX_SUFF. Do NOT use this for new code.
 */
#ifdef IN_RC
# define CTXALLSUFF(var)    var##GC
#elif defined(IN_RING0)
# define CTXALLSUFF(var)    var##R0
#else
# define CTXALLSUFF(var)    var##R3
#endif

/** @def CTX_SUFF
 * Adds the suffix of the current context to the passed in
 * identifier name. The suffix is R3, R0 or RC.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   var     Identifier name.
 *
 * @remark  This will replace CTXALLSUFF and CTXSUFF before long.
 */
#ifdef IN_RC
# define CTX_SUFF(var)      var##RC
#elif defined(IN_RING0)
# define CTX_SUFF(var)      var##R0
#else
# define CTX_SUFF(var)      var##R3
#endif

/** @def CTX_SUFF_Z
 * Adds the suffix of the current context to the passed in
 * identifier name, combining RC and R0 into RZ.
 * The suffix thus is R3 or RZ.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   var     Identifier name.
 *
 * @remark  This will replace CTXALLSUFF and CTXSUFF before long.
 */
#ifdef IN_RING3
# define CTX_SUFF_Z(var)    var##R3
#else
# define CTX_SUFF_Z(var)    var##RZ
#endif


/** @def CTXMID
 * Adds the current context as a middle name of an identifier name
 * The middle name is HC or GC.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   first   First name.
 * @param   last    Surname.
 */
/** @def OTHERCTXMID
 * Adds the other context as a middle name of an identifier name
 * The middle name is HC or GC.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   first   First name.
 * @param   last    Surname.
 * @deprecated use CTX_MID or CTX_MID_Z
 */
#ifdef IN_RC
# define CTXMID(first, last)        first##GC##last
# define OTHERCTXMID(first, last)   first##HC##last
#else
# define CTXMID(first, last)        first##HC##last
# define OTHERCTXMID(first, last)   first##GC##last
#endif

/** @def CTXALLMID
 * Adds the current context as a middle name of an identifier name.
 * The middle name is R3, R0 or GC.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   first   First name.
 * @param   last    Surname.
 * @deprecated use CTX_MID or CTX_MID_Z
 */
#ifdef IN_RC
# define CTXALLMID(first, last)     first##GC##last
#elif defined(IN_RING0)
# define CTXALLMID(first, last)     first##R0##last
#else
# define CTXALLMID(first, last)     first##R3##last
#endif

/** @def CTX_MID
 * Adds the current context as a middle name of an identifier name.
 * The middle name is R3, R0 or RC.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   first   First name.
 * @param   last    Surname.
 */
#ifdef IN_RC
# define CTX_MID(first, last)       first##RC##last
#elif defined(IN_RING0)
# define CTX_MID(first, last)       first##R0##last
#else
# define CTX_MID(first, last)       first##R3##last
#endif

/** @def CTX_MID_Z
 * Adds the current context as a middle name of an identifier name, combining RC
 * and R0 into RZ.
 * The middle name thus is either R3 or RZ.
 *
 * This is macro should only be used in shared code to avoid a forest of ifdefs.
 * @param   first   First name.
 * @param   last    Surname.
 */
#ifdef IN_RING3
# define CTX_MID_Z(first, last)     first##R3##last
#else
# define CTX_MID_Z(first, last)     first##RZ##last
#endif


/** @def R3STRING
 * A macro which in GC and R0 will return a dummy string while in R3 it will return
 * the parameter.
 *
 * This is typically used to wrap description strings in structures shared
 * between R3, R0 and/or GC. The intention is to avoid the \#ifdef IN_RING3 mess.
 *
 * @param   pR3String   The R3 string. Only referenced in R3.
 * @see R0STRING and GCSTRING
 */
#ifdef IN_RING3
# define R3STRING(pR3String)    (pR3String)
#else
# define R3STRING(pR3String)    ("<R3_STRING>")
#endif

/** @def R0STRING
 * A macro which in GC and R3 will return a dummy string while in R0 it will return
 * the parameter.
 *
 * This is typically used to wrap description strings in structures shared
 * between R3, R0 and/or GC. The intention is to avoid the \#ifdef IN_RING0 mess.
 *
 * @param   pR0String   The R0 string. Only referenced in R0.
 * @see R3STRING and GCSTRING
 */
#ifdef IN_RING0
# define R0STRING(pR0String)    (pR0String)
#else
# define R0STRING(pR0String)    ("<R0_STRING>")
#endif

/** @def RCSTRING
 * A macro which in R3 and R0 will return a dummy string while in RC it will return
 * the parameter.
 *
 * This is typically used to wrap description strings in structures shared
 * between R3, R0 and/or RC. The intention is to avoid the \#ifdef IN_RC mess.
 *
 * @param   pR0String   The RC string. Only referenced in RC.
 * @see R3STRING, R0STRING
 */
#ifdef IN_RC
# define RCSTRING(pRCString)    (pRCString)
#else
# define RCSTRING(pRCString)    ("<RC_STRING>")
#endif


/** @def RT_NOTHING
 * A macro that expands to nothing.
 * This is primarily intended as a dummy argument for macros to avoid the
 * undefined behavior passing empty arguments to an macro (ISO C90 and C++98,
 * gcc v4.4 warns about it).
 */
#define RT_NOTHING


/** @def RTCALL
 * The standard calling convention for the Runtime interfaces.
 */
#ifdef _MSC_VER
# define RTCALL     __cdecl
#elif defined(__GNUC__) && defined(IN_RING0) && !(defined(RT_OS_OS2) || defined(RT_ARCH_AMD64)) /* the latter is kernel/gcc */
# define RTCALL     __attribute__((cdecl,regparm(0)))
#else
# define RTCALL
#endif

/** @def RT_NO_THROW
 * How to express that a function doesn't throw C++ exceptions
 * and the compiler can thus save itself the bother of trying
 * to catch any of them. Put this between the closing parenthesis
 * and the semicolon in function prototypes (and implementation if C++).
 */
#if defined(__cplusplus) \
 && (   (defined(_MSC_VER) && defined(_CPPUNWIND)) \
     || (defined(__GNUC__) && defined(__EXCEPTIONS)))
# define RT_NO_THROW    throw()
#else
# define RT_NO_THROW
#endif

/** @def DECLEXPORT
 * How to declare an exported function.
 * @param   type    The return type of the function declaration.
 */
#if defined(_MSC_VER) || defined(RT_OS_OS2)
# define DECLEXPORT(type)       __declspec(dllexport) type
#elif defined(RT_USE_VISIBILITY_DEFAULT)
# define DECLEXPORT(type)      __attribute__((visibility("default"))) type
#else
# define DECLEXPORT(type)      type
#endif

/** @def DECLIMPORT
 * How to declare an imported function.
 * @param   type    The return type of the function declaration.
 */
#if defined(_MSC_VER) || (defined(RT_OS_OS2) && !defined(__IBMC__) && !defined(__IBMCPP__))
# define DECLIMPORT(type)       __declspec(dllimport) type
#else
# define DECLIMPORT(type)       type
#endif

/** @def DECLHIDDEN
 * How to declare a non-exported function or variable.
 * @param   type    The return type of the function or the data type of the variable.
 */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS) || !defined(RT_USE_VISIBILITY_HIDDEN)
# define DECLHIDDEN(type)       type
#else
# define DECLHIDDEN(type)       __attribute__((visibility("hidden"))) type
#endif

/** @def DECLASM
 * How to declare an internal assembly function.
 * @param   type    The return type of the function declaration.
 */
#ifdef __cplusplus
# ifdef _MSC_VER
#  define DECLASM(type)          extern "C" type __cdecl
# else
#  define DECLASM(type)          extern "C" type
# endif
#else
# ifdef _MSC_VER
#  define DECLASM(type)          type __cdecl
# else
#  define DECLASM(type)          type
# endif
#endif

/** @def DECLASMTYPE
 * How to declare an internal assembly function type.
 * @param   type    The return type of the function.
 */
#ifdef _MSC_VER
# define DECLASMTYPE(type)      type __cdecl
#else
# define DECLASMTYPE(type)      type
#endif

/** @def DECLNORETURN
 * How to declare a function which does not return.
 * @note: This macro can be combined with other macros, for example
 * @code
 *   EMR3DECL(DECLNORETURN(void)) foo(void);
 * @endcode
 */
#ifdef _MSC_VER
# define DECLNORETURN(type)     __declspec(noreturn) type
#elif defined(__GNUC__)
# define DECLNORETURN(type)     __attribute__((noreturn)) type
#else
# define DECLNORETURN(type)     type
#endif

/** @def DECLCALLBACK
 * How to declare an call back function type.
 * @param   type    The return type of the function declaration.
 */
#define DECLCALLBACK(type)      type RTCALL

/** @def DECLCALLBACKPTR
 * How to declare an call back function pointer.
 * @param   type    The return type of the function declaration.
 * @param   name    The name of the variable member.
 */
#define DECLCALLBACKPTR(type, name)  type (RTCALL * name)

/** @def DECLCALLBACKMEMBER
 * How to declare an call back function pointer member.
 * @param   type    The return type of the function declaration.
 * @param   name    The name of the struct/union/class member.
 */
#define DECLCALLBACKMEMBER(type, name)  type (RTCALL * name)

/** @def DECLR3CALLBACKMEMBER
 * How to declare an call back function pointer member - R3 Ptr.
 * @param   type    The return type of the function declaration.
 * @param   name    The name of the struct/union/class member.
 * @param   args    The argument list enclosed in parentheses.
 */
#ifdef IN_RING3
# define DECLR3CALLBACKMEMBER(type, name, args)  type (RTCALL * name) args
#else
# define DECLR3CALLBACKMEMBER(type, name, args)  RTR3PTR name
#endif

/** @def DECLRCCALLBACKMEMBER
 * How to declare an call back function pointer member - RC Ptr.
 * @param   type    The return type of the function declaration.
 * @param   name    The name of the struct/union/class member.
 * @param   args    The argument list enclosed in parentheses.
 */
#ifdef IN_RC
# define DECLRCCALLBACKMEMBER(type, name, args)  type (RTCALL * name) args
#else
# define DECLRCCALLBACKMEMBER(type, name, args)  RTRCPTR name
#endif

/** @def DECLR0CALLBACKMEMBER
 * How to declare an call back function pointer member - R0 Ptr.
 * @param   type    The return type of the function declaration.
 * @param   name    The name of the struct/union/class member.
 * @param   args    The argument list enclosed in parentheses.
 */
#ifdef IN_RING0
# define DECLR0CALLBACKMEMBER(type, name, args)  type (RTCALL * name) args
#else
# define DECLR0CALLBACKMEMBER(type, name, args)  RTR0PTR name
#endif

/** @def DECLINLINE
 * How to declare a function as inline.
 * @param   type    The return type of the function declaration.
 * @remarks Don't use this macro on C++ methods.
 */
#ifdef __GNUC__
# define DECLINLINE(type) static __inline__ type
#elif defined(__cplusplus)
# define DECLINLINE(type) inline type
#elif defined(_MSC_VER)
# define DECLINLINE(type) _inline type
#elif defined(__IBMC__)
# define DECLINLINE(type) _Inline type
#else
# define DECLINLINE(type) inline type
#endif


/** @def IN_RT_STATIC
 * Used to indicate whether we're linking against a static IPRT
 * or not. The IPRT symbols will be declared as hidden (if
 * supported). Note that this define has no effect without setting
 * IN_RT_R0, IN_RT_R3 or IN_RT_GC indicators are set first.
 */

/** @def IN_RT_R0
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-0 Runtime Library.
 */
/** @def RTR0DECL(type)
 * Runtime Library HC Ring-0 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_RT_R0
# ifdef IN_RT_STATIC
#  define RTR0DECL(type)    DECLHIDDEN(type) RTCALL
# else
#  define RTR0DECL(type)    DECLEXPORT(type) RTCALL
# endif
#else
# define RTR0DECL(type)     DECLIMPORT(type) RTCALL
#endif

/** @def IN_RT_R3
 * Used to indicate whether we're inside the same link module as
 * the HC Ring-3 Runtime Library.
 */
/** @def RTR3DECL(type)
 * Runtime Library HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_RT_R3
# ifdef IN_RT_STATIC
#  define RTR3DECL(type)    DECLHIDDEN(type) RTCALL
# else
#  define RTR3DECL(type)    DECLEXPORT(type) RTCALL
# endif
#else
# define RTR3DECL(type)     DECLIMPORT(type) RTCALL
#endif

/** @def IN_RT_GC
 * Used to indicate whether we're inside the same link module as
 * the GC Runtime Library.
 */
/** @def RTGCDECL(type)
 * Runtime Library HC Ring-3 export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_RT_GC
# ifdef IN_RT_STATIC
#  define RTGCDECL(type)    DECLHIDDEN(type) RTCALL
# else
#  define RTGCDECL(type)    DECLEXPORT(type) RTCALL
# endif
#else
# define RTGCDECL(type)     DECLIMPORT(type) RTCALL
#endif

/** @def RTDECL(type)
 * Runtime Library export or import declaration.
 * Functions declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_RT_R3) || defined(IN_RT_GC) || defined(IN_RT_R0)
# ifdef IN_RT_STATIC
#  define RTDECL(type)      DECLHIDDEN(type) RTCALL
# else
#  define RTDECL(type)      DECLEXPORT(type) RTCALL
# endif
#else
# define RTDECL(type)       DECLIMPORT(type) RTCALL
#endif

/** @def RTDATADECL(type)
 * Runtime Library export or import declaration.
 * Data declared using this macro exists in all contexts.
 * @param   type    The return type of the function declaration.
 */
#if defined(IN_RT_R3) || defined(IN_RT_GC) || defined(IN_RT_R0)
# ifdef IN_RT_STATIC
#  define RTDATADECL(type)  DECLHIDDEN(type)
# else
#  define RTDATADECL(type)  DECLEXPORT(type)
# endif
#else
# define RTDATADECL(type)   DECLIMPORT(type)
#endif


/** @def RT_NOCRT
 * Symbol name wrapper for the No-CRT bits.
 *
 * In order to coexist in the same process as other CRTs, we need to
 * decorate the symbols such that they don't conflict the ones in the
 * other CRTs. The result of such conflicts / duplicate symbols can
 * confuse the dynamic loader on Unix like systems.
 *
 * Define RT_WITHOUT_NOCRT_WRAPPERS to drop the wrapping.
 * Define RT_WITHOUT_NOCRT_WRAPPER_ALIASES to drop the aliases to the
 * wrapped names.
 */
/** @def RT_NOCRT_STR
 * Same as RT_NOCRT only it'll return a double quoted string of the result.
 */
#ifndef RT_WITHOUT_NOCRT_WRAPPERS
# define RT_NOCRT(name) nocrt_ ## name
# define RT_NOCRT_STR(name) "nocrt_" # name
#else
# define RT_NOCRT(name) name
# define RT_NOCRT_STR(name) #name
#endif



/** @def RT_LIKELY
 * Give the compiler a hint that an expression is very likely to hold true.
 *
 * Some compilers support explicit branch prediction so that the CPU backend
 * can hint the processor and also so that code blocks can be reordered such
 * that the predicted path sees a more linear flow, thus improving cache
 * behaviour, etc.
 *
 * IPRT provides the macros RT_LIKELY() and RT_UNLIKELY() as a way to utilize
 * this compiler feature when present.
 *
 * A few notes about the usage:
 *
 *      - Generally, use RT_UNLIKELY() with error condition checks (unless you
 *        have some _strong_ reason to do otherwise, in which case document it),
 *        and/or RT_LIKELY() with success condition checks, assuming you want
 *        to optimize for the success path.
 *
 *      - Other than that, if you don't know the likelihood of a test succeeding
 *        from empirical or other 'hard' evidence, don't make predictions unless
 *        you happen to be a Dirk Gently.
 *
 *      - These macros are meant to be used in places that get executed a lot. It
 *        is wasteful to make predictions in code that is executed rarely (e.g.
 *        at subsystem initialization time) as the basic block reordering that this
 *        affects can often generate larger code.
 *
 *      - Note that RT_SUCCESS() and RT_FAILURE() already makes use of RT_LIKELY()
 *        and RT_UNLIKELY(). Should you wish for prediction free status checks,
 *        use the RT_SUCCESS_NP() and RT_FAILURE_NP() macros instead.
 *
 *
 * @returns the boolean result of the expression.
 * @param   expr        The expression that's very likely to be true.
 * @see RT_UNLIKELY
 */
/** @def RT_UNLIKELY
 * Give the compiler a hint that an expression is highly unlikely to hold true.
 *
 * See the usage instructions give in the RT_LIKELY() docs.
 *
 * @returns the boolean result of the expression.
 * @param   expr        The expression that's very unlikely to be true.
 * @see RT_LIKELY
 */
#if defined(__GNUC__)
# if __GNUC__ >= 3
#  define RT_LIKELY(expr)       __builtin_expect(!!(expr), 1)
#  define RT_UNLIKELY(expr)     __builtin_expect(!!(expr), 0)
# else
#  define RT_LIKELY(expr)       (expr)
#  define RT_UNLIKELY(expr)     (expr)
# endif
#else
# define RT_LIKELY(expr)        (expr)
# define RT_UNLIKELY(expr)      (expr)
#endif


/** @def RT_BIT
 * Make a bitmask for one integer sized bit.
 * @param   bit     Bit number.
 */
#define RT_BIT(bit)                     (1U << (bit))

/** @def RT_BIT_32
 * Make a 32-bit bitmask for one bit.
 * @param   bit     Bit number.
 */
#define RT_BIT_32(bit)                  (UINT32_C(1) << (bit))

/** @def RT_BIT_64
 * Make a 64-bit bitmask for one bit.
 * @param   bit     Bit number.
 */
#define RT_BIT_64(bit)                  (UINT64_C(1) << (bit))

/** @def RT_ALIGN
 * Align macro.
 * @param   u           Value to align.
 * @param   uAlignment  The alignment. Power of two!
 *
 * @remark  Be extremely careful when using this macro with type which sizeof != sizeof int.
 *          When possible use any of the other RT_ALIGN_* macros. And when that's not
 *          possible, make 101% sure that uAlignment is specified with a right sized type.
 *
 *          Specifying an unsigned 32-bit alignment constant with a 64-bit value will give
 *          you a 32-bit return value!
 *
 *          In short: Don't use this macro. Use RT_ALIGN_T() instead.
 */
#define RT_ALIGN(u, uAlignment)         ( ((u) + ((uAlignment) - 1)) & ~((uAlignment) - 1) )

/** @def RT_ALIGN_T
 * Align macro.
 * @param   u           Value to align.
 * @param   uAlignment  The alignment. Power of two!
 * @param   type        Integer type to use while aligning.
 * @remark  This macro is the preferred alignment macro, it doesn't have any of the pitfalls RT_ALIGN has.
 */
#define RT_ALIGN_T(u, uAlignment, type) ( ((type)(u) + ((uAlignment) - 1)) & ~(type)((uAlignment) - 1) )

/** @def RT_ALIGN_32
 * Align macro for a 32-bit value.
 * @param   u32         Value to align.
 * @param   uAlignment  The alignment. Power of two!
 */
#define RT_ALIGN_32(u32, uAlignment)            RT_ALIGN_T(u32, uAlignment, uint32_t)

/** @def RT_ALIGN_64
 * Align macro for a 64-bit value.
 * @param   u64         Value to align.
 * @param   uAlignment  The alignment. Power of two!
 */
#define RT_ALIGN_64(u64, uAlignment)            RT_ALIGN_T(u64, uAlignment, uint64_t)

/** @def RT_ALIGN_Z
 * Align macro for size_t.
 * @param   cb          Value to align.
 * @param   uAlignment  The alignment. Power of two!
 */
#define RT_ALIGN_Z(cb, uAlignment)              RT_ALIGN_T(cb, uAlignment, size_t)

/** @def RT_ALIGN_P
 * Align macro for pointers.
 * @param   pv          Value to align.
 * @param   uAlignment  The alignment. Power of two!
 */
#define RT_ALIGN_P(pv, uAlignment)              RT_ALIGN_PT(pv, uAlignment, void *)

/** @def RT_ALIGN_PT
 * Align macro for pointers with type cast.
 * @param   u           Value to align.
 * @param   uAlignment  The alignment. Power of two!
 * @param   CastType    The type to cast the result to.
 */
#define RT_ALIGN_PT(u, uAlignment, CastType)    ((CastType)RT_ALIGN_T(u, uAlignment, uintptr_t))

/** @def RT_ALIGN_R3PT
 * Align macro for ring-3 pointers with type cast.
 * @param   u           Value to align.
 * @param   uAlignment  The alignment. Power of two!
 * @param   CastType    The type to cast the result to.
 */
#define RT_ALIGN_R3PT(u, uAlignment, CastType)  ((CastType)RT_ALIGN_T(u, uAlignment, RTR3UINTPTR))

/** @def RT_ALIGN_R0PT
 * Align macro for ring-0 pointers with type cast.
 * @param   u           Value to align.
 * @param   uAlignment  The alignment. Power of two!
 * @param   CastType    The type to cast the result to.
 */
#define RT_ALIGN_R0PT(u, uAlignment, CastType)  ((CastType)RT_ALIGN_T(u, uAlignment, RTR0UINTPTR))

/** @def RT_ALIGN_GCPT
 * Align macro for GC pointers with type cast.
 * @param   u           Value to align.
 * @param   uAlignment  The alignment. Power of two!
 * @param   CastType        The type to cast the result to.
 */
#define RT_ALIGN_GCPT(u, uAlignment, CastType)  ((CastType)RT_ALIGN_T(u, uAlignment, RTGCUINTPTR))


/** @def RT_OFFSETOF
 * Our own special offsetof() variant, returns a signed result.
 *
 * This differs from the usual offsetof() in that it's not relying on builtin
 * compiler stuff and thus can use variables in arrays the structure may
 * contain. This is useful to determine the sizes of structures ending
 * with a variable length field.
 *
 * @returns offset into the structure of the specified member. signed.
 * @param   type    Structure type.
 * @param   member  Member.
 */
#define RT_OFFSETOF(type, member)   ( (int)(uintptr_t)&( ((type *)(void *)0)->member) )

/** @def RT_UOFFSETOF
 * Our own special offsetof() variant, returns an unsigned result.
 *
 * This differs from the usual offsetof() in that it's not relying on builtin
 * compiler stuff and thus can use variables in arrays the structure may
 * contain. This is useful to determine the sizes of structures ending
 * with a variable length field.
 *
 * @returns offset into the structure of the specified member. unsigned.
 * @param   type    Structure type.
 * @param   member  Member.
 */
#define RT_UOFFSETOF(type, member)   ( (uintptr_t)&( ((type *)(void *)0)->member) )

/** @def RT_OFFSETOF_ADD
 * RT_OFFSETOF with an addend.
 *
 * @returns offset into the structure of the specified member. signed.
 * @param   type    Structure type.
 * @param   member  Member.
 * @param   addend  The addend to add to the offset.
 */
#define RT_OFFSETOF_ADD(type, member, addend)   ( (int)RT_UOFFSETOF_ADD(type, member, addend) )

/** @def RT_UOFFSETOF_ADD
 * RT_UOFFSETOF with an addend.
 *
 * @returns offset into the structure of the specified member. signed.
 * @param   type    Structure type.
 * @param   member  Member.
 * @param   addend  The addend to add to the offset.
 */
#define RT_UOFFSETOF_ADD(type, member, addend)   ( (uintptr_t)&( ((type *)(void *)(uintptr_t)(addend))->member) )

/** @def RT_SIZEOFMEMB
 * Get the size of a structure member.
 *
 * @returns size of the structure member.
 * @param   type    Structure type.
 * @param   member  Member.
 */
#define RT_SIZEOFMEMB(type, member) ( sizeof(((type *)(void *)0)->member) )

/** @def RT_ELEMENTS
 * Calculates the number of elements in an array.
 * @returns Element count.
 * @param   aArray      Array in question.
 */
#define RT_ELEMENTS(aArray)         ( sizeof(aArray) / sizeof((aArray)[0]) )

#ifdef RT_OS_OS2
/* Undefine RT_MAX since there is an unfortunate clash with the max
   resource type define in os2.h. */
# undef RT_MAX
#endif

/** @def RT_MAX
 * Finds the maximum value.
 * @returns The higher of the two.
 * @param   Value1      Value 1
 * @param   Value2      Value 2
 */
#define RT_MAX(Value1, Value2)  ((Value1) >= (Value2) ? (Value1) : (Value2))

/** @def RT_MIN
 * Finds the minimum value.
 * @returns The lower of the two.
 * @param   Value1      Value 1
 * @param   Value2      Value 2
 */
#define RT_MIN(Value1, Value2)  ((Value1) <= (Value2) ? (Value1) : (Value2))

/** @def RT_ABS
 * Get the absolute (non-negative) value.
 * @returns The absolute value of Value.
 * @param   Value       The value.
 */
#define RT_ABS(Value)           ((Value) >= 0 ? (Value) : -(Value))

/** @def RT_LODWORD
 * Gets the low dword (=uint32_t) of something. */
#define RT_LODWORD(a)           ( (uint32_t)(a) )

/** @def RT_HIDWORD
 * Gets the high dword (=uint32_t) of a 64-bit of something. */
#define RT_HIDWORD(a)           ( (uint32_t)((a) >> 32) )

/** @def RT_LOWORD
 * Gets the low word (=uint16_t) of something. */
#define RT_LOWORD(a)            ((a) & 0xffff)

/** @def RT_HIWORD
 * Gets the high word (=uint16_t) of a 32-bit something. */
#define RT_HIWORD(a)            ((a) >> 16)

/** @def RT_LOBYTE
 * Gets the low byte of something. */
#define RT_LOBYTE(a)            ((a) & 0xff)

/** @def RT_HIBYTE
 * Gets the low byte of a 16-bit something. */
#define RT_HIBYTE(a)            ((a) >> 8)

/** @def RT_BYTE1
 * Gets first byte of something. */
#define RT_BYTE1(a)             ((a) & 0xff)

/** @def RT_BYTE2
 * Gets second byte of something. */
#define RT_BYTE2(a)             (((a) >> 8) & 0xff)

/** @def RT_BYTE3
 * Gets second byte of something. */
#define RT_BYTE3(a)             (((a) >> 16) & 0xff)

/** @def RT_BYTE4
 * Gets fourth byte of something. */
#define RT_BYTE4(a)             (((a) >> 24) & 0xff)


/** @def RT_MAKE_U64
 * Constructs a uint64_t value from two uint32_t values.
 */
#define RT_MAKE_U64(Lo, Hi) ( (uint64_t)((uint32_t)(Hi)) << 32 | (uint32_t)(Lo) )

/** @def RT_MAKE_U64_FROM_U16
 * Constructs a uint64_t value from four uint16_t values.
 */
#define RT_MAKE_U64_FROM_U16(w0, w1, w2, w3) \
                (   (uint64_t)((uint16_t)(w3)) << 48 \
                  | (uint64_t)((uint16_t)(w2)) << 32 \
                  | (uint32_t)((uint16_t)(w1)) << 16 \
                  |            (uint16_t)(w0) )

/** @def RT_MAKE_U64_FROM_U8
 * Constructs a uint64_t value from eight uint8_t values.
 */
#define RT_MAKE_U64_FROM_U8(b0, b1, b2, b3, b4, b5, b6, b7) \
                (   (uint64_t)((uint8_t)(b7)) << 56 \
                  | (uint64_t)((uint8_t)(b6)) << 48 \
                  | (uint64_t)((uint8_t)(b5)) << 40 \
                  | (uint64_t)((uint8_t)(b4)) << 32 \
                  | (uint32_t)((uint8_t)(b3)) << 24 \
                  | (uint32_t)((uint8_t)(b2)) << 16 \
                  | (uint16_t)((uint8_t)(b1)) << 8 \
                  |            (uint8_t)(b0) )

/** @def RT_MAKE_U32
 * Constructs a uint32_t value from two uint16_t values.
 */
#define RT_MAKE_U32(Lo, Hi) ( (uint32_t)((uint16_t)(Hi)) << 16 | (uint16_t)(Lo) )

/** @def RT_MAKE_U32_FROM_U8
 * Constructs a uint32_t value from four uint8_t values.
 */
#define RT_MAKE_U32_FROM_U8(b0, b1, b2, b3) \
                (   (uint32_t)((uint8_t)(b3)) << 24 \
                  | (uint32_t)((uint8_t)(b2)) << 16 \
                  | (uint16_t)((uint8_t)(b1)) << 8 \
                  |            (uint8_t)(b0) )

/** @def RT_MAKE_U16
 * Constructs a uint32_t value from two uint16_t values.
 */
#define RT_MAKE_U16(Lo, Hi) ( (uint16_t)((uint8_t)(Hi)) << 8 | (uint8_t)(Lo) )


/** @def RT_BSWAP_U64
 * Reverses the byte order of an uint64_t value. */
#if 0
# define RT_BSWAP_U64(u64)  RT_BSWAP_U64_C(u64)
#elif defined(__GNUC__)
/** @todo use __builtin_constant_p? */
# define RT_BSWAP_U64(u64)  ASMByteSwapU64(u64)
#else
# define RT_BSWAP_U64(u64)  ASMByteSwapU64(u64)
#endif

/** @def RT_BSWAP_U32
 * Reverses the byte order of an uint32_t value. */
#if 0
# define RT_BSWAP_U32(u32)  RT_BSWAP_U32_C(u32)
#elif defined(__GNUC__)
/** @todo use __builtin_constant_p? */
# define RT_BSWAP_U32(u32)  ASMByteSwapU32(u32)
#else
# define RT_BSWAP_U32(u32)  ASMByteSwapU32(u32)
#endif

/** @def RT_BSWAP_U16
 * Reverses the byte order of an uint16_t value. */
#if 0
# define RT_BSWAP_U16(u16)  RT_BSWAP_U16_C(u16)
#elif defined(__GNUC__)
/** @todo use __builtin_constant_p? */
# define RT_BSWAP_U16(u16)  ASMByteSwapU16(u16)
#else
# define RT_BSWAP_U16(u16)  ASMByteSwapU16(u16)
#endif


/** @def RT_BSWAP_U64_C
 * Reverses the byte order of an uint64_t constant. */
#define RT_BSWAP_U64_C(u64) RT_MAKE_U64(RT_BSWAP_U32_C((u64) >> 32), RT_BSWAP_U32_C((u64) & 0xffffffff))

/** @def RT_BSWAP_U32_C
 * Reverses the byte order of an uint32_t constant. */
#define RT_BSWAP_U32_C(u32) (RT_BYTE4(u32) | (RT_BYTE3(u32) << 8) | (RT_BYTE2(u32) << 16) | (RT_BYTE1(u32) << 24))

/** @def RT_BSWAP_U16_C
 * Reverses the byte order of an uint16_t constant. */
#define RT_BSWAP_U16_C(u16) (RT_HIBYTE(u16) | (RT_LOBYTE(u16) << 8))


/** @def RT_H2LE_U64
 * Converts an uint64_t value from host to little endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2LE_U64(u64)   RT_BSWAP_U64(u64)
#else
# define RT_H2LE_U64(u64)   (u64)
#endif

/** @def RT_H2LE_U64_C
 * Converts an uint64_t constant from host to little endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2LE_U64_C(u64) RT_BSWAP_U64_C(u64)
#else
# define RT_H2LE_U64_C(u64) (u64)
#endif

/** @def RT_H2LE_U32
 * Converts an uint32_t value from host to little endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2LE_U32(u32)   RT_BSWAP_U32(u32)
#else
# define RT_H2LE_U32(u32)   (u32)
#endif

/** @def RT_H2LE_U32_C
 * Converts an uint32_t constant from host to little endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2LE_U32_C(u32) RT_BSWAP_U32_C(u32)
#else
# define RT_H2LE_U32_C(u32) (u32)
#endif

/** @def RT_H2LE_U16
 * Converts an uint16_t value from host to little endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2LE_U16(u16)   RT_BSWAP_U16(u16)
#else
# define RT_H2LE_U16(u16)   (u16)
#endif

/** @def RT_H2LE_U16_C
 * Converts an uint16_t constant from host to little endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2LE_U16_C(u16) RT_BSWAP_U16_C(u16)
#else
# define RT_H2LE_U16_C(u16) (u16)
#endif


/** @def RT_LE2H_U64
 * Converts an uint64_t value from little endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_LE2H_U64(u64)   RT_BSWAP_U64(u64)
#else
# define RT_LE2H_U64(u64)   (u64)
#endif

/** @def RT_LE2H_U64_C
 * Converts an uint64_t constant from little endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_LE2H_U64_C(u64) RT_BSWAP_U64_C(u64)
#else
# define RT_LE2H_U64_C(u64) (u64)
#endif

/** @def RT_LE2H_U32
 * Converts an uint32_t value from little endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_LE2H_U32(u32)   RT_BSWAP_U32(u32)
#else
# define RT_LE2H_U32(u32)   (u32)
#endif

/** @def RT_LE2H_U32_C
 * Converts an uint32_t constant from little endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_LE2H_U32_C(u32) RT_BSWAP_U32_C(u32)
#else
# define RT_LE2H_U32_C(u32) (u32)
#endif

/** @def RT_LE2H_U16
 * Converts an uint16_t value from little endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_LE2H_U16(u16)   RT_BSWAP_U16(u16)
#else
# define RT_LE2H_U16(u16)   (u16)
#endif

/** @def RT_LE2H_U16_C
 * Converts an uint16_t constant from little endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_LE2H_U16_C(u16) RT_BSWAP_U16_C(u16)
#else
# define RT_LE2H_U16_C(u16) (u16)
#endif


/** @def RT_H2BE_U64
 * Converts an uint64_t value from host to big endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2BE_U64(u64)   (u64)
#else
# define RT_H2BE_U64(u64)   RT_BSWAP_U64(u64)
#endif

/** @def RT_H2BE_U64_C
 * Converts an uint64_t constant from host to big endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2BE_U64_C(u64) (u64)
#else
# define RT_H2BE_U64_C(u64) RT_BSWAP_U64_C(u64)
#endif

/** @def RT_H2BE_U32
 * Converts an uint32_t value from host to big endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2BE_U32(u32)   (u32)
#else
# define RT_H2BE_U32(u32)   RT_BSWAP_U32(u32)
#endif

/** @def RT_H2BE_U32_C
 * Converts an uint32_t constant from host to big endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2BE_U32_C(u32) (u32)
#else
# define RT_H2BE_U32_C(u32) RT_BSWAP_U32_C(u32)
#endif

/** @def RT_H2BE_U16
 * Converts an uint16_t value from host to big endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2BE_U16(u16)   (u16)
#else
# define RT_H2BE_U16(u16)   RT_BSWAP_U16(u16)
#endif

/** @def RT_H2BE_U16_C
 * Converts an uint16_t constant from host to big endian byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_H2BE_U16_C(u16) (u16)
#else
# define RT_H2BE_U16_C(u16) RT_BSWAP_U16_C(u16)
#endif

/** @def RT_BE2H_U64
 * Converts an uint64_t value from big endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_BE2H_U64(u64)   (u64)
#else
# define RT_BE2H_U64(u64)   RT_BSWAP_U64(u64)
#endif

/** @def RT_BE2H_U64
 * Converts an uint64_t constant from big endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_BE2H_U64_C(u64) (u64)
#else
# define RT_BE2H_U64_C(u64) RT_BSWAP_U64_C(u64)
#endif

/** @def RT_BE2H_U32
 * Converts an uint32_t value from big endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_BE2H_U32(u32)   (u32)
#else
# define RT_BE2H_U32(u32)   RT_BSWAP_U32(u32)
#endif

/** @def RT_BE2H_U32_C
 * Converts an uint32_t value from big endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_BE2H_U32_C(u32) (u32)
#else
# define RT_BE2H_U32_C(u32) RT_BSWAP_U32_C(u32)
#endif

/** @def RT_BE2H_U16
 * Converts an uint16_t value from big endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_BE2H_U16(u16)   (u16)
#else
# define RT_BE2H_U16(u16)   RT_BSWAP_U16(u16)
#endif

/** @def RT_BE2H_U16_C
 * Converts an uint16_t constant from big endian to host byte order. */
#ifdef RT_BIG_ENDIAN
# define RT_BE2H_U16_C(u16) (u16)
#else
# define RT_BE2H_U16_C(u16) RT_BSWAP_U16_C(u16)
#endif


/** @def RT_H2N_U64
 * Converts an uint64_t value from host to network byte order. */
#define RT_H2N_U64(u64)     RT_H2BE_U64(u64)

/** @def RT_H2N_U64_C
 * Converts an uint64_t constant from host to network byte order. */
#define RT_H2N_U64_C(u64)   RT_H2BE_U64_C(u64)

/** @def RT_H2N_U32
 * Converts an uint32_t value from host to network byte order. */
#define RT_H2N_U32(u32)     RT_H2BE_U32(u32)

/** @def RT_H2N_U32_C
 * Converts an uint32_t constant from host to network byte order. */
#define RT_H2N_U32_C(u32)   RT_H2BE_U32_C(u32)

/** @def RT_H2N_U16
 * Converts an uint16_t value from host to network byte order. */
#define RT_H2N_U16(u16)     RT_H2BE_U16(u16)

/** @def RT_H2N_U16_C
 * Converts an uint16_t constant from host to network byte order. */
#define RT_H2N_U16_C(u16)   RT_H2BE_U16_C(u16)

/** @def RT_N2H_U64
 * Converts an uint64_t value from network to host byte order. */
#define RT_N2H_U64(u64)     RT_BE2H_U64(u64)

/** @def RT_N2H_U64_C
 * Converts an uint64_t constant from network to host byte order. */
#define RT_N2H_U64_C(u64)   RT_BE2H_U64_C(u64)

/** @def RT_N2H_U32
 * Converts an uint32_t value from network to host byte order. */
#define RT_N2H_U32(u32)     RT_BE2H_U32(u32)

/** @def RT_N2H_U32_C
 * Converts an uint32_t constant from network to host byte order. */
#define RT_N2H_U32_C(u32)   RT_BE2H_U32_C(u32)

/** @def RT_N2H_U16
 * Converts an uint16_t value from network to host byte order. */
#define RT_N2H_U16(u16)     RT_BE2H_U16(u16)

/** @def RT_N2H_U16_C
 * Converts an uint16_t value from network to host byte order. */
#define RT_N2H_U16_C(u16)   RT_BE2H_U16_C(u16)


/*
 * The BSD sys/param.h + machine/param.h file is a major source of
 * namespace pollution. Kill off some of the worse ones unless we're
 * compiling kernel code.
 */
#if defined(RT_OS_DARWIN) \
  && !defined(KERNEL) \
  && !defined(RT_NO_BSD_PARAM_H_UNDEFING) \
  && ( defined(_SYS_PARAM_H_) || defined(_I386_PARAM_H_) )
/* sys/param.h: */
# undef PSWP
# undef PVM
# undef PINOD
# undef PRIBO
# undef PVFS
# undef PZERO
# undef PSOCK
# undef PWAIT
# undef PLOCK
# undef PPAUSE
# undef PUSER
# undef PRIMASK
# undef MINBUCKET
# undef MAXALLOCSAVE
# undef FSHIFT
# undef FSCALE

/* i386/machine.h: */
# undef ALIGN
# undef ALIGNBYTES
# undef DELAY
# undef STATUS_WORD
# undef USERMODE
# undef BASEPRI
# undef MSIZE
# undef CLSIZE
# undef CLSIZELOG2
#endif


/** @def NULL
 * NULL pointer.
 */
#ifndef NULL
# ifdef __cplusplus
#  define NULL 0
# else
#  define NULL ((void*)0)
# endif
#endif

/** @def NIL_OFFSET
 * NIL offset.
 * Whenever we use offsets instead of pointers to save space and relocation effort
 * NIL_OFFSET shall be used as the equivalent to NULL.
 */
#define NIL_OFFSET   (~0U)

/** @def NOREF
 * Keeps the compiler from bitching about an unused parameter.
 */
#define NOREF(var)               (void)(var)

/** @def Breakpoint
 * Emit a debug breakpoint instruction.
 *
 * Use this for instrumenting a debugging session only!
 * No committed code shall use Breakpoint().
 */
#ifdef __GNUC__
# define Breakpoint()           __asm__ __volatile__("int $3\n\t")
#endif
#ifdef _MSC_VER
# define Breakpoint()           __asm int 3
#endif
#if defined(__IBMC__) || defined(__IBMCPP__)
# define Breakpoint()           __interrupt(3)
#endif
#ifndef Breakpoint
# error "This compiler is not supported!"
#endif


/** Size Constants
 * (Of course, these are binary computer terms, not SI.)
 * @{
 */
/** 1 K (Kilo)                     (1 024). */
#define _1K                     0x00000400
/** 4 K (Kilo)                     (4 096). */
#define _4K                     0x00001000
/** 32 K (Kilo)                   (32 678). */
#define _32K                    0x00008000
/** 64 K (Kilo)                   (65 536). */
#define _64K                    0x00010000
/** 128 K (Kilo)                 (131 072). */
#define _128K                   0x00020000
/** 256 K (Kilo)                 (262 144). */
#define _256K                   0x00040000
/** 512 K (Kilo)                 (524 288). */
#define _512K                   0x00080000
/** 1 M (Mega)                 (1 048 576). */
#define _1M                     0x00100000
/** 2 M (Mega)                 (2 097 152). */
#define _2M                     0x00200000
/** 4 M (Mega)                 (4 194 304). */
#define _4M                     0x00400000
/** 1 G (Giga)             (1 073 741 824). */
#define _1G                     0x40000000
/** 2 G (Giga)             (2 147 483 648). (32-bit) */
#define _2G32                   0x80000000U
/** 2 G (Giga)             (2 147 483 648). (64-bit) */
#define _2G             0x0000000080000000LL
/** 4 G (Giga)             (4 294 967 296). */
#define _4G             0x0000000100000000LL
/** 1 T (Tera)         (1 099 511 627 776). */
#define _1T             0x0000010000000000LL
/** 1 P (Peta)     (1 125 899 906 842 624). */
#define _1P             0x0004000000000000LL
/** 1 E (Exa)  (1 152 921 504 606 846 976). */
#define _1E             0x1000000000000000LL
/** 2 E (Exa)  (2 305 843 009 213 693 952). */
#define _2E             0x2000000000000000ULL
/** @} */

/** @def VALID_PTR
 * Pointer validation macro.
 * @param   ptr
 */
#if defined(RT_ARCH_AMD64)
# ifdef IN_RING3
#  if defined(RT_OS_DARWIN) /* first 4GB is reserved for legacy kernel. */
#   define VALID_PTR(ptr)   (   (uintptr_t)(ptr) >= _4G \
                             && !((uintptr_t)(ptr) & 0xffff800000000000ULL) )
#  elif defined(RT_OS_SOLARIS) /* The kernel only used the top 2TB, but keep it simple. */
#   define VALID_PTR(ptr)   (   (uintptr_t)(ptr) + 0x1000U >= 0x2000U \
                             && (   ((uintptr_t)(ptr) & 0xffff800000000000ULL) == 0xffff800000000000ULL \
                                 || ((uintptr_t)(ptr) & 0xffff800000000000ULL) == 0) )
#  else
#   define VALID_PTR(ptr)   (   (uintptr_t)(ptr) + 0x1000U >= 0x2000U \
                             && !((uintptr_t)(ptr) & 0xffff800000000000ULL) )
#  endif
# else /* !IN_RING3 */
#  define VALID_PTR(ptr)    (   (uintptr_t)(ptr) + 0x1000U >= 0x2000U \
                             && (   ((uintptr_t)(ptr) & 0xffff800000000000ULL) == 0xffff800000000000ULL \
                                 || ((uintptr_t)(ptr) & 0xffff800000000000ULL) == 0) )
# endif /* !IN_RING3 */
#elif defined(RT_ARCH_X86)
# define VALID_PTR(ptr)     ( (uintptr_t)(ptr) + 0x1000U >= 0x2000U )
#else
# error "Architecture identifier missing / not implemented."
#endif


/** @def VALID_PHYS32
 * 32 bits physical address validation macro.
 * @param   Phys          The RTGCPHYS address.
 */
#define VALID_PHYS32(Phys)  ( (uint64_t)(Phys) < (uint64_t)_4G )

/** @def N_
 * The \#define N_ is used to mark a string for translation. This is usable in
 * any part of the code, as it is only used by the tools that create message
 * catalogs. This macro is a no-op as far as the compiler and code generation
 * is concerned.
 *
 * If you want to both mark a string for translation and translate it, use _().
 */
#define N_(s) (s)

/** @def _
 * The \#define _ is used to mark a string for translation and to translate it
 * in one step.
 *
 * If you want to only mark a string for translation, use N_().
 */
#define _(s) gettext(s)


/** @def __PRETTY_FUNCTION__
 *  With GNU C we'd like to use the builtin __PRETTY_FUNCTION__, so define that
 *  for the other compilers.
 */
#if !defined(__GNUC__) && !defined(__PRETTY_FUNCTION__)
# define __PRETTY_FUNCTION__    __FUNCTION__
#endif


/** @def RT_STRICT
 * The \#define RT_STRICT controls whether or not assertions and other runtime
 * checks should be compiled in or not.
 *
 * If you want assertions which are not subject to compile time options use
 * the AssertRelease*() flavors.
 */
#if !defined(RT_STRICT) && defined(DEBUG)
# define RT_STRICT
#endif

/** Source position. */
#define RT_SRC_POS         __FILE__, __LINE__, __PRETTY_FUNCTION__

/** Source position declaration. */
#define RT_SRC_POS_DECL    const char *pszFile, unsigned iLine, const char *pszFunction

/** Source position arguments. */
#define RT_SRC_POS_ARGS    pszFile, iLine, pszFunction

/** @} */


/** @defgroup grp_rt_cdefs_cpp  Special Macros for C++
 * @ingroup grp_rt_cdefs
 * @{
 */

#ifdef __cplusplus

/** @def DECLEXPORT_CLASS
 * How to declare an exported class. Place this macro after the 'class'
 * keyword in the declaration of every class you want to export.
 *
 * @note It is necessary to use this macro even for inner classes declared
 * inside the already exported classes. This is a GCC specific requirement,
 * but it seems not to harm other compilers.
 */
#if defined(_MSC_VER) || defined(RT_OS_OS2)
# define DECLEXPORT_CLASS       __declspec(dllexport)
#elif defined(RT_USE_VISIBILITY_DEFAULT)
# define DECLEXPORT_CLASS       __attribute__((visibility("default")))
#else
# define DECLEXPORT_CLASS
#endif

/** @def DECLIMPORT_CLASS
 * How to declare an imported class Place this macro after the 'class'
 * keyword in the declaration of every class you want to export.
 *
 * @note It is necessary to use this macro even for inner classes declared
 * inside the already exported classes. This is a GCC specific requirement,
 * but it seems not to harm other compilers.
 */
#if defined(_MSC_VER) || (defined(RT_OS_OS2) && !defined(__IBMC__) && !defined(__IBMCPP__))
# define DECLIMPORT_CLASS       __declspec(dllimport)
#elif defined(RT_USE_VISIBILITY_DEFAULT)
# define DECLIMPORT_CLASS       __attribute__((visibility("default")))
#else
# define DECLIMPORT_CLASS
#endif

/** @def WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP
 * Macro to work around error C2593 of the not-so-smart MSVC 7.x ambiguity
 * resolver. The following snippet clearly demonstrates the code causing this
 * error:
 * @code
 *      class A
 *      {
 *      public:
 *          operator bool() const { return false; }
 *          operator int*() const { return NULL; }
 *      };
 *      int main()
 *      {
 *          A a;
 *          if (!a);
 *          if (a && 0);
 *          return 0;
 *      }
 * @endcode
 * The code itself seems pretty valid to me and GCC thinks the same.
 *
 * This macro fixes the compiler error by explicitly overloading implicit
 * global operators !, && and || that take the given class instance as one of
 * their arguments.
 *
 * The best is to use this macro right after the class declaration.
 *
 * @note The macro expands to nothing for compilers other than MSVC.
 *
 * @param Cls Class to apply the workaround to
 */
#if defined(_MSC_VER)
# define WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP(Cls) \
    inline bool operator! (const Cls &that) { return !bool (that); } \
    inline bool operator&& (const Cls &that, bool b) { return bool (that) && b; } \
    inline bool operator|| (const Cls &that, bool b) { return bool (that) || b; } \
    inline bool operator&& (bool b, const Cls &that) { return b && bool (that); } \
    inline bool operator|| (bool b, const Cls &that) { return b || bool (that); }
#else
# define WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP(Cls)
#endif

/** @def WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP_TPL
 * Version of WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP for template classes.
 *
 * @param Tpl       Name of the template class to apply the workaround to
 * @param ArgsDecl  arguments of the template, as declared in |<>| after the
 *                  |template| keyword, including |<>|
 * @param Args      arguments of the template, as specified in |<>| after the
 *                  template class name when using the, including |<>|
 *
 * Example:
 * @code
 *      // template class declaration
 *      template <class C>
 *      class Foo { ... };
 *      // applied workaround
 *      WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP_TPL (Foo, <class C>, <C>)
 * @endcode
 */
#if defined(_MSC_VER)
# define WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP_TPL(Tpl, ArgsDecl, Args) \
    template ArgsDecl \
    inline bool operator! (const Tpl Args &that) { return !bool (that); } \
    template ArgsDecl \
    inline bool operator&& (const Tpl Args  &that, bool b) { return bool (that) && b; } \
    template ArgsDecl \
    inline bool operator|| (const Tpl Args  &that, bool b) { return bool (that) || b; } \
    template ArgsDecl \
    inline bool operator&& (bool b, const Tpl Args  &that) { return b && bool (that); } \
    template ArgsDecl \
    inline bool operator|| (bool b, const Tpl Args  &that) { return b || bool (that); }
#else
# define WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP_TPL(Tpl, ArgsDecl, Args)
#endif


/** @def DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP
 * Declares the copy constructor and the assignment operation as inlined no-ops
 * (non-existent functions) for the given class. Use this macro inside the
 * private section if you want to effectively disable these operations for your
 * class.
 *
 * @param      Cls     class name to declare for
 */

#define DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(Cls) \
    inline Cls (const Cls &); \
    inline Cls &operator= (const Cls &);


/** @def DECLARE_CLS_NEW_DELETE_NOOP
 * Declares the new and delete operations as no-ops (non-existent functions)
 * for the given class. Use this macro inside the private section if you want
 * to effectively limit creating class instances on the stack only.
 *
 * @note The destructor of the given class must not be virtual, otherwise a
 * compile time error will occur. Note that this is not a drawback: having
 * the virtual destructor for a stack-based class is absolutely useless
 * (the real class of the stack-based instance is always known to the compiler
 * at compile time, so it will always call the correct destructor).
 *
 * @param      Cls     class name to declare for
 */
#define DECLARE_CLS_NEW_DELETE_NOOP(Cls) \
    inline static void *operator new (size_t); \
    inline static void operator delete (void *);

#endif /* defined(__cplusplus) */

/** @} */

#endif

