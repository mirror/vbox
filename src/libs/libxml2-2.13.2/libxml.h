/*
 * libxml.h: internal header only used during the compilation of libxml
 *
 * See COPYRIGHT for the status of this software
 *
 * Author: breese@users.sourceforge.net
 */

#ifndef __XML_LIBXML_H__
#define __XML_LIBXML_H__

/*
 * These macros must be defined before including system headers.
 * Do not add any #include directives above this block.
 */
#ifndef NO_LARGEFILE_SOURCE
  #ifndef _LARGEFILE_SOURCE
    #define _LARGEFILE_SOURCE
  #endif
  #ifndef _FILE_OFFSET_BITS
    #define _FILE_OFFSET_BITS 64
  #endif
#endif

#if defined(VBOX)
#include <vboxconfig.h>
#include <libxml/xmlversion.h>
#endif


#if defined(macintosh)
#include "config-mac.h"
#elif defined(_WIN32)
#include <win32config.h>
#include <libxml/xmlversion.h>
#elif defined(_WIN32_WCE)
/*
 * These files are generated by the build system and contain private
 * and public build configuration.
 */
#include "config.h"
#include <libxml/xmlversion.h>
#else
/*
 * Due to some Autotools limitations, this variable must be passed as
 * compiler flag. Define a default value if the macro wasn't set by the
 * build system.
 */
#ifndef SYSCONFDIR
  #define SYSCONFDIR "/etc"
#endif

#if !defined(_WIN32) && \
    !defined(__CYGWIN__) && \
    (defined(__clang__) || \
     (defined(__GNUC__) && (__GNUC__ >= 4)))
  #define XML_HIDDEN __attribute__((visibility("hidden")))
#else
  #define XML_HIDDEN
#endif

#if defined(__clang__) || \
    (defined(__GNUC__) && (__GNUC__ >= 8) && !defined(__EDG__))
  #define ATTRIBUTE_NO_SANITIZE(arg) __attribute__((no_sanitize(arg)))
#else
  #define ATTRIBUTE_NO_SANITIZE(arg)
#endif

#ifdef __clang__
  #if __clang_major__ >= 12
    #define ATTRIBUTE_NO_SANITIZE_INTEGER \
      ATTRIBUTE_NO_SANITIZE("unsigned-integer-overflow") \
      ATTRIBUTE_NO_SANITIZE("unsigned-shift-base")
  #else
    #define ATTRIBUTE_NO_SANITIZE_INTEGER \
      ATTRIBUTE_NO_SANITIZE("unsigned-integer-overflow")
  #endif
#else
  #define ATTRIBUTE_NO_SANITIZE_INTEGER
#endif

#endif /* #if defined(macintosh) */
#endif /* ! __XML_LIBXML_H__ */
