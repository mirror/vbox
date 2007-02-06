/* todo: header */

#ifndef __iprt_nocrt_compiler_compiler_h__
#define __iprt_nocrt_compiler_compiler_h__

#ifdef __GNUC__
# include <iprt/nocrt/compiler/gcc.h>
#elif defined(_MSC_VER)
# include <iprt/nocrt/compiler/msc.h>
#else
# error "Unsupported compiler."
#endif

#endif
