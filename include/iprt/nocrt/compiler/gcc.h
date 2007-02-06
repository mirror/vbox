/* A quick hack for freebsd where there are no separate location 
   for compiler specific headers like on linux, mingw, os2, ++. 
   This file will be cleaned up later...  */
   
#ifndef __nocrt_compiler_gcc_h__
#define __nocrt_compiler_gcc_h__


/* stddef.h */
#ifdef __PTRDIFF_TYPE__
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#elif ARCH_BITS == 32
typedef int32_t ptrdiff_t;
#elif ARCH_BITS == 64
typedef int64_t ptrdiff_t;
#else
# error "ARCH_BITS is undefined or incorrect."
#endif
#define _PTRDIFF_T_DECLARED

#ifdef __SIZE_TYPE__
typedef __SIZE_TYPE__ size_t;  
#elif ARCH_BITS == 32
typedef uint32_t size_t;
#elif ARCH_BITS == 64
typedef uint64_t size_t;
#else
# error "ARCH_BITS is undefined or incorrect."
#endif 
#define _SIZE_T_DECLARED

#ifdef __SSIZE_TYPE__
typedef __SSIZE_TYPE__ ssize_t;  
#elif ARCH_BITS == 32
typedef int32_t ssize_t;
#elif ARCH_BITS == 64
typedef int64_t ssize_t;
#else
# define ARCH_BITS 123123
# error "ARCH_BITS is undefined or incorrect."
#endif 
#define _SSIZE_T_DECLARED

#ifdef __WCHAR_TYPE__
typedef __WCHAR_TYPE__ wchar_t;
#elif defined(__OS2__) || defined(__WIN__) 
typedef uint16_t wchar_t;
#else
typedef int wchar_t;
#endif
#define _WCHAR_T_DECLARED

#ifdef __WINT_TYPE__
typedef __WINT_TYPE__ wint_t;
#else
typedef unsigned int wint_t;
#endif
#define _WINT_T_DECLARED

#ifndef NULL
# ifdef __cplusplus
#  define NULL  0
# else
#  define NULL  ((void *)0)
# endif
#endif 


#ifndef offsetof
# if defined(__cplusplus) && defined(__offsetof__) 
#  define offsetof(type, memb) 
    (__offsetof__ (reinterpret_cast<size_t>(&reinterpret_cast<const volatile char &>(static_cast<type *>(0)->memb))) )
# else
#  define offsetof(type, memb) ((size_t)&((type *)0)->memb)
# endif                                        
#endif                                        

#endif
