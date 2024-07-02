/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 1995 Danny Gasparovski.
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#ifndef VBOX
#define DBG_CALL (1 << 0)
#define DBG_MISC (1 << 1)
#define DBG_ERROR (1 << 2)
#define DBG_TFTP (1 << 3)
#define DBG_VERBOSE_CALL (1 << 4)

extern int slirp_debug;

#define DEBUG_CALL(fmt, ...)                      \
    do {                                          \
        if (G_UNLIKELY(slirp_debug & DBG_CALL)) { \
            g_debug(fmt "...", ##__VA_ARGS__);    \
        }                                         \
    } while (0)

#define DEBUG_VERBOSE_CALL(fmt, ...)                      \
    do {                                                  \
        if (G_UNLIKELY(slirp_debug & DBG_VERBOSE_CALL)) { \
            g_debug(fmt "...", ##__VA_ARGS__);            \
        }                                                 \
    } while (0)

#define DEBUG_ARG(fmt, ...)                       \
    do {                                          \
        if (G_UNLIKELY(slirp_debug & DBG_CALL)) { \
            g_debug(" " fmt, ##__VA_ARGS__);      \
        }                                         \
    } while (0)

#define DEBUG_MISC(fmt, ...)                      \
    do {                                          \
        if (G_UNLIKELY(slirp_debug & DBG_MISC)) { \
            g_debug(fmt, ##__VA_ARGS__);          \
        }                                         \
    } while (0)

#define DEBUG_ERROR(fmt, ...)                      \
    do {                                           \
        if (G_UNLIKELY(slirp_debug & DBG_ERROR)) { \
            g_debug(fmt, ##__VA_ARGS__);           \
        }                                          \
    } while (0)

#define DEBUG_TFTP(fmt, ...)                      \
    do {                                          \
        if (G_UNLIKELY(slirp_debug & DBG_TFTP)) { \
            g_debug(fmt, ##__VA_ARGS__);          \
        }                                         \
    } while (0)

#else  /* VBOX */
/*
 * Map these onto logging and make them compatible with Visual C++.
 */
# ifndef LOG_GROUP
#  define LOG_GROUP LOG_GROUP_DRV_NAT
#  include <VBox/log.h>
# endif
# define DEBUG_ERROR(...)           do { LogRel2((__VA_ARGS__)); LogRel2(("\n")); } while (0)
# define DEBUG_CALL(...)            do { LogRel3((__VA_ARGS__)); LogRel3(("\n")); } while (0)
# define DEBUG_VERBOSE_CALL(...)    do { LogRel4((__VA_ARGS__)); LogRel4(("\n")); } while (0)
# define DEBUG_ARG(...)             do { LogRel5((__VA_ARGS__)); LogRel5(("\n")); } while (0)
# define DEBUG_MISC(...)            do { LogRel6((__VA_ARGS__)); LogRel6(("\n")); } while (0)
# define DEBUG_IS_MISC_ENABLED()    LogRelIs6Enabled()
# define DEBUG_TFTP(...)            do { LogRel7((__VA_ARGS__)); LogRel7(("\n")); } while (0)
#endif /* VBOX */

#endif /* DEBUG_H_ */
