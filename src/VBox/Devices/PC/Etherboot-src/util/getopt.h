/* getopt (emx+gcc) */

#ifndef _GETOPT_H
#define _GETOPT_H

#if defined (__cplusplus)
extern "C" {
#endif

extern char *optarg;       /* argument of current option                    */
extern int optind;         /* index of next argument; default=0: initialize */
extern int opterr;         /* 0=disable error messages; default=1: enable   */
extern int optopt;         /* option character which caused the error       */
extern char *optswchar;    /* characters introducing options; default="-"   */

extern enum _optmode
{
  GETOPT_UNIX,             /* options at start of argument list (default)   */
  GETOPT_ANY,              /* move non-options to the end                   */
  GETOPT_KEEP              /* return options in order                       */
} optmode;


/* Note: The 2nd argument is not const as GETOPT_ANY reorders the
   array pointed to. */

int getopt (int, char **, __const__ char *);

#if defined (__cplusplus)
}
#endif

#endif /* not _GETOPT_H */
