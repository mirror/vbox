/** @file
 * IPRT - Runtime Loader Generation.
 */

/*
 * Copyright (C) 2008-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#include <iprt/types.h>
#ifdef RT_RUNTIME_LOADER_GENERATE_BODY_STUBS
# include <iprt/ldr.h>
# include <iprt/log.h>
# include <iprt/once.h>
#endif

/** @defgroup grp_rt_runtime_loader     Runtime Loader Generation
 * @ingroup grp_rt
 *
 * How to use this loader generator
 *
 * This loader generator can be used to generate stub code for loading a shared
 * library and its functions at runtime, or for generating a header file with
 * the declaration of the loader function and optionally declarations for the
 * functions loaded.  It should be included in a header file or a C source
 * file, after defining certain macros which it makes use of.
 *
 * To generate the C source code for function proxy stubs and the library
 * loader function, you should define the following macros in your source file
 * before including this header:
 *
 *  RT_RUNTIME_LOADER_LIB_NAME       - the file name of the library to load
 *  RT_RUNTIME_LOADER_FUNCTION       - the name of the loader function
 *  RT_RUNTIME_LOADER_INSERT_SYMBOLS - a macro containing the names of the
 *                                     functions to be loaded, defined in the
 *                                     following pattern:
 * @code
 * #define RT_RUNTIME_LOADER_INSERT_SYMBOLS \
 *  RT_PROXY_STUB(func_name, ret_type, (long_param_list), (short_param_list)) \
 *  RT_PROXY_STUB(func_name2, ret_type2, (long_param_list2), (short_param_list2)) \
 *  RT_PROXY_VARIADIC_STUB(func_name3, ret_type3, (long_param_list3, ...)) \
 *  ...
 * #define func_name3(...) g_pfn_func_name3(__VA_ARGS__)
 * @endcode
 *
 * where long_param_list is a parameter list for declaring the function of the
 * form (type1 arg1, type2 arg2, ...) and short_param_list for calling it, of
 * the form (arg1, arg2, ...).
 *
 * To generate the header file, you should define RT_RUNTIME_LOADER_FUNCTION
 * and if you wish to generate declarations for the functions you should
 * additionally define RT_RUNTIME_LOADER_INSERT_SYMBOLS as above and
 * RT_RUNTIME_LOADER_GENERATE_DECLS (without a value) before including this
 * file.
 *
 * @note For functions with a variable number of parameters, this approch is
 *       clumsy as it requires an additional \#define for each function that
 *       makes use of the g_pfn_XXX function pointer. See func_name3 in the
 *       snipped above.  Instead, use the VBoxDef2LazyLoad approach.
 *
 * @deprecated This is deprecated. Use VBoxDef2LazyLoad instead where possible.
 *             See VBOX_DEF_2_LAZY_LOAD in /Config.kmk,
 *             src/bldprog/VBoxDef2LazyLoad.cpp and examples in
 *             src/VBox/Devices/Makefile.kmk and other places.
 *
 * @{
 */
/** @todo this is far too complicated.  A script for generating the files would
 * probably be preferable.
 *
 * bird> An alternative is to generate assembly jump wrappers, this only
 * requires the symbol names and prefix.  I've done this ages ago when we forked
 * the EMX/GCC toolchain on OS/2...  It's a wee bit more annoying in x86 PIC/PIE
 * mode, but nothing that cannot be dealt with.
 *
 * Update: This was done years ago. See src/bldprogs/VBoxDef2LazyLoad.cpp and
 *         VBOX_DEF_2_LAZY_LOAD in /Config.kmk.
 */
/** @todo r=bird: The use of RTR3DECL here is an unresolved issue. */
/** @todo r=bird: The lack of RT_C_DECLS_BEGIN/END is an unresolved issue.  Here
 *        we'll get into trouble if we use the same symbol names as the
 *        original! */
/** @todo r=bird: The prefix usage here is very confused: RT_RUNTIME_LOADER_XXX,
 *        RT_PROXY_STUB, etc. */

#ifdef RT_RUNTIME_LOADER_GENERATE_BODY_STUBS

/* The following are the symbols which we need from the library. */
# define RT_PROXY_STUB(function, rettype, signature, shortsig) \
    rettype (*g_pfn_ ## function) signature; \
    RTR3DECL(rettype) function signature \
    { return g_pfn_ ## function shortsig; }

/* The following are the symbols which correspond to variadic functions
 * provided by the library. */
# define RT_PROXY_VARIADIC_STUB(function, rettype, signature) \
    rettype (*g_pfn_ ## function) signature;

RT_RUNTIME_LOADER_INSERT_SYMBOLS

# undef RT_PROXY_STUB
# undef RT_PROXY_VARIADIC_STUB

/* Function pointer type for easy casting below. */
typedef void (*PFNRTLDRSHAREDGENERIC)(void);

/* Now comes a table of functions to be loaded from the library. */
typedef struct
{
    const char            *pszName;
    PFNRTLDRSHAREDGENERIC *ppfn;
} RTLDRSHAREDFUNC;

# define RT_PROXY_STUB(function, rettype, signature, shortsig ) { #function , (PFNRTLDRSHAREDGENERIC *)&g_pfn_ ## function } ,
# define RT_PROXY_VARIADIC_STUB(function, rettype, signature)   { #function , (PFNRTLDRSHAREDGENERIC *)&g_pfn_ ## function } ,
static RTLDRSHAREDFUNC const g_aSharedFuncs[] =
{
    RT_RUNTIME_LOADER_INSERT_SYMBOLS
};
# undef RT_PROXY_VARIADIC_STUB
# undef RT_PROXY_STUB

/**
 * The function which does the actual work for RT_RUNTIME_LOADER_FUNCTION,
 * serialised for thread safety.
 */
static DECLCALLBACK(int) rtldrLoadOnce(void *)
{
    LogFlowFunc(("\n"));
    RTLDRMOD hLdrMod;
    int rcRet = RTLdrLoadEx(RT_RUNTIME_LOADER_LIB_NAME, &hLdrMod, RTLDRLOAD_FLAGS_LOCAL | RTLDRLOAD_FLAGS_NO_UNLOAD, NULL);
    if (RT_SUCCESS(rcRet))
    {
        for (unsigned i = 0; i < RT_ELEMENTS(g_aSharedFuncs); ++i)
        {
            int rc2 = RTLdrGetSymbol(hLdrMod, g_aSharedFuncs[i].pszName, (void **)g_aSharedFuncs[i].ppfn);
            if (RT_FAILURE(rc2))
            {
                LogFunc(("RTLdrGetSymbol(%s, %s) failed: %Rrc\n", RT_RUNTIME_LOADER_LIB_NAME, g_aSharedFuncs[i].pszName, rc2));
                rcRet = rc2;
            }
        }
        LogFlowFunc(("rcRet = %Rrc\n", rcRet));
    }
    else
        LogFunc(("RTLdrLoadEx(%s) failed: %Rrc\n",  RT_RUNTIME_LOADER_LIB_NAME, rcRet));
    return rcRet;
}

/**
 * Load the shared library RT_RUNTIME_LOADER_LIB_NAME and resolve the symbols
 * pointed to by RT_RUNTIME_LOADER_INSERT_SYMBOLS.
 *
 * May safely be called from multiple threads and will not return until the
 * library is loaded or has failed to load.
 *
 * @returns IPRT status code.
 */
RTR3DECL(int) RT_RUNTIME_LOADER_FUNCTION(void)
{
    static RTONCE s_Once = RTONCE_INITIALIZER;
    LogFlowFunc(("\n"));
    int rc = RTOnce(&s_Once, rtldrLoadOnce, NULL);
    LogFlowFunc(("rc = %Rrc\n", rc));
    return rc;
}

#elif defined(RT_RUNTIME_LOADER_GENERATE_HEADER)
# ifdef RT_RUNTIME_LOADER_GENERATE_DECLS
/* Declarations of the functions that we need from
 * RT_RUNTIME_LOADER_LIB_NAME */
#  define RT_PROXY_STUB(function, rettype, signature, shortsig) \
    RTR3DECL(rettype)  function  signature ;
/* Variadict functions needs custom mappings via \#defines as we cannot forward
   the arguments in an inline function, so only make the function pointer available here. */
# define RT_PROXY_VARIADIC_STUB(function, rettype, signature) \
    rettype (*g_pfn_ ## function) signature; \

RT_RUNTIME_LOADER_INSERT_SYMBOLS


#  undef RT_PROXY_STUB
#  undef RT_PROXY_VARIADIC_STUB
# endif /* RT_RUNTIME_LOADER_GENERATE_DECLS */

/**
 * Try to dynamically load the library.  This function should be called before
 * attempting to use any of the library functions.  It is safe to call this
 * function multiple times.
 *
 * @returns iprt status code
 */
RTR3DECL(int) RT_RUNTIME_LOADER_FUNCTION(void);

#else
# error "One of RT_RUNTIME_LOADER_GENERATE_HEADER or RT_RUNTIME_LOADER_GENERATE_BODY_STUBS must be defined when including this file"
#endif

/** @} */

