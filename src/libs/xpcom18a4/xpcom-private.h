/* xpcom/xpcom-private.h.  Generated automatically by configure.  */
/* The following defines are only used by the xpcom implementation */

#ifndef _XPCOM_PRIVATE_H_
#define _XPCOM_PRIVATE_H_

/* Define to build the static component loader */
#define ENABLE_STATIC_COMPONENT_LOADER 1

/* Define if getpagesize() is available */
#define HAVE_GETPAGESIZE 1

/* Define if iconv() is available */
#ifndef L4ENV
#define HAVE_ICONV 1
#endif

/* Define if iconv() supports const input */
/* #undef HAVE_ICONV_WITH_CONST_INPUT */

/* Define if mbrtowc() is available */
#ifndef L4ENV
#define HAVE_MBRTOWC 1
#endif

/* Define if <sys/mount.h> is present */
#define HAVE_SYS_MOUNT_H 1

/* Define if <sys/vfs.h> is present */
#define HAVE_SYS_VFS_H 1

/* Define if wcrtomb() is available */
#ifndef L4ENV
#define HAVE_WCRTOMB 1
#endif

#endif /* _XPCOM_PRIVATE_H_ */

