/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#define PRN_STDERR      1
#define PRN_SPRINTF     2

/* Unused anyway, using VBox Log facility. */
#define dfd NULL

#define DBG_CALL 0x1
#define DBG_MISC 0x2
#define DBG_ERROR 0x4
#define DEBUG_DEFAULT DBG_CALL|DBG_MISC|DBG_ERROR

#include <VBox/log.h>

#ifdef LOG_ENABLED
#define DEBUG_CALL(x)       LogFlow(("%s:\n", x))
#define DEBUG_ARG(x, y)     do { LogFlow((x, y)); LogFlow(("\n")); } while (0)
#define DEBUG_ARGS(x)       __debug_flow x
#define DEBUG_MISC(x)       __debug_log2 x
#define DEBUG_ERROR(x)      __debug_log x

DECLINLINE(void) __debug_flow(FILE *pIgnore, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    LogFlow(("%Nv\n", pszFormat, &args));
    va_end(args);
}

DECLINLINE(void) __debug_log2(FILE *pIgnore, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    Log2(("%Nv\n", pszFormat, &args));
    va_end(args);
}

DECLINLINE(void) __debug_log(FILE *pIgnore, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    Log(("%Nv\n", pszFormat, &args));
    va_end(args);
}

#else  /* !LOG_ENABLED */

#define DEBUG_CALL(x)       do {} while (0)
#define DEBUG_ARG(x, y)     do {} while (0)
#define DEBUG_ARGS(x)       do {} while (0)
#define DEBUG_MISC(x)       do {} while (0)
#define DEBUG_ERROR(x)      do {} while (0)

#endif  /* !LOG_ENABLED */

int debug_init (void);
void ipstats (PNATState);
void tcpstats (PNATState);
void udpstats (PNATState);
void icmpstats (PNATState);
void mbufstats (PNATState);
void sockstats (PNATState);

