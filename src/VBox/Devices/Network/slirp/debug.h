/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#define PRN_STDERR	1
#define PRN_SPRINTF	2

#ifdef VBOX
/* Unused anyway, using VBox Log facility. */
#define dfd NULL
#else /* !VBOX */
extern FILE *dfd;
extern FILE *lfd;
#endif /* !VBOX */
extern int dostats;
extern int slirp_debug;

#define DBG_CALL 0x1
#define DBG_MISC 0x2
#define DBG_ERROR 0x4
#define DEBUG_DEFAULT DBG_CALL|DBG_MISC|DBG_ERROR

#ifndef VBOX
#ifdef DEBUG
#define DEBUG_CALL(x) if (slirp_debug & DBG_CALL) { fprintf(dfd, "%s...\n", x); fflush(dfd); }
#define DEBUG_ARG(x, y) if (slirp_debug & DBG_CALL) { fputc(' ', dfd); fprintf(dfd, x, y); fputc('\n', dfd); fflush(dfd); }
#define DEBUG_ARGS(x) if (slirp_debug & DBG_CALL) { fprintf x ; fflush(dfd); }
#define DEBUG_MISC(x) if (slirp_debug & DBG_MISC) { fprintf x ; fflush(dfd); }
#define DEBUG_ERROR(x) if (slirp_debug & DBG_ERROR) {fprintf x ; fflush(dfd); }


#else

#define DEBUG_CALL(x)
#define DEBUG_ARG(x, y)
#define DEBUG_ARGS(x)
#define DEBUG_MISC(x)
#define DEBUG_ERROR(x)

#endif
#else /* VBOX */

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

#endif  /* VBOX */

void debug_init _P((char *, int));
/*void ttystats _P((struct ttys *)); */
void allttystats _P((void));
#ifdef VBOX
void ipstats _P((PNATState));
void tcpstats _P((PNATState));
void udpstats _P((PNATState));
void icmpstats _P((PNATState));
void mbufstats _P((PNATState));
void sockstats _P((PNATState));
#else /* !VBOX */
void ipstats _P((void));
void vjstats _P((void));
void tcpstats _P((void));
void udpstats _P((void));
void icmpstats _P((void));
void mbufstats _P((void));
void sockstats _P((void));
#endif /* VBOX */
#ifndef VBOX
void slirp_exit _P((int));
#endif /* VBOX */

