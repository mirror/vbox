/* -*- indent-tabs-mode: nil; -*- */
#ifndef _LWIP_PROXY_VBOX_COMPAT_H_
#define _LWIP_PROXY_VBOX_COMPAT_H_

/*
 * Provide VBOX stuff to standalone proxy to make it possible to just
 * drop sources from standalone repo into VBOX repo as-is.
 */
#ifdef VBOX
#error This file must not be used when building for VBox
#endif

#ifdef __FreeBSD__
#define RT_OS_FREEBSD 1
#endif

#ifdef __linux__
#define RT_OS_LINUX 1
#endif

#ifdef __NetBSD__
#define RT_OS_NETBSD 1
#endif

#if defined(__sun__) && defined(__svr4__)
#define RT_OS_SOLARIS 1
#endif

#endif /* _LWIP_PROXY_VBOX_COMPAT_H_ */
