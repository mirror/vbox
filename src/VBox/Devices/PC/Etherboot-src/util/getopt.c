/* getopt.c (emx+gcc) -- Copyright (c) 1990-1995 by Eberhard Mattes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

char *optarg          = NULL;
int optind            = 1;              /* Default: first call             */
int opterr            = 1;              /* Default: error messages enabled */
char *optswchar       = "-";            /* Default: '-' starts options     */
enum optmode optmode = GETOPT_UNIX;
int optopt;

static char * next_opt;             /* Next character in cluster of options */

static char done;
static char sw_char;
static char init;

static char ** options;            /* List of entries which are options     */
static char ** non_options;        /* List of entries which are not options */
static int options_count;
static int non_options_count;

#define BEGIN do {
#define END   } while (0)

#define PUT(dst) BEGIN \
                  if (optmode == GETOPT_ANY) \
                    dst[dst##_count++] = argv[optind]; \
                 END

/* Note: `argv' is not const as GETOPT_ANY reorders argv[]. */

int getopt (int argc, char * argv[], const char *opt_str)
{
  char c, *q;
  int i, j;

  if (!init || optind == 0)
    {
      if (optind == 0) optind = 1;
      done = 0; init = 1;
      next_opt = "";
      if (optmode == GETOPT_ANY)
        {
          options = (char **)malloc (argc * sizeof (char *));
          non_options = (char **)malloc (argc * sizeof (char *));
          if (options == NULL || non_options == NULL)
            {
              fprintf (stderr, "out of memory in getopt()\n");
              exit (255);
            }
          options_count = 0; non_options_count = 0;
        }
    }
  if (done)
    return -1;
restart:
  optarg = NULL;
  if (*next_opt == 0)
    {
      if (optind >= argc)
        {
          if (optmode == GETOPT_ANY)
            {
              j = 1;
              for (i = 0; i < options_count; ++i)
                argv[j++] = options[i];
              for (i = 0; i < non_options_count; ++i)
                argv[j++] = non_options[i];
              optind = options_count+1;
              free (options); free (non_options);
            }
          done = 1;
          return -1;
        }
      else if (!strchr (optswchar, argv[optind][0]) || argv[optind][1] == 0)
        {
          if (optmode == GETOPT_UNIX)
            {
              done = 1;
              return -1;
            }
          PUT (non_options);
          optarg = argv[optind++];
          if (optmode == GETOPT_ANY)
            goto restart;
          /* optmode==GETOPT_KEEP */
          return 0;
        }
      else if (argv[optind][0] == argv[optind][1] && argv[optind][2] == 0)
        {
          if (optmode == GETOPT_ANY)
            {
              j = 1;
              for (i = 0; i < options_count; ++i)
                argv[j++] = options[i];
              argv[j++] = argv[optind];
              for (i = 0; i < non_options_count; ++i)
                argv[j++] = non_options[i];
              for (i = optind+1; i < argc; ++i)
                argv[j++] = argv[i];
              optind = options_count+2;
              free (options); free (non_options);
            }
          ++optind;
          done = 1;
          return -1;
        }
      else
        {
          PUT (options);
          sw_char = argv[optind][0];
          next_opt = argv[optind]+1;
        }
    }
  c = *next_opt++;
  if (*next_opt == 0)  /* Move to next argument if end of argument reached */
    ++optind;
  if (c == ':' || (q = strchr (opt_str, c)) == NULL)
    {
      if (opterr && opt_str[0] != ':')
        {
          if (c < ' ' || c >= 127)
            fprintf (stderr, "%s: invalid option; character code=0x%.2x\n",
                     argv[0], c);
          else
            fprintf (stderr, "%s: invalid option `%c%c'\n",
                     argv[0], sw_char, c);
        }
      optopt = c;
      return '?';
    }
  if (q[1] == ':')
    {
      if (*next_opt != 0)         /* Argument given */
        {
          optarg = next_opt;
          next_opt = "";
          ++optind;
        }
      else if (q[2] == ':')
        optarg = NULL;           /* Optional argument missing */
      else if (optind >= argc)
        {                         /* Required argument missing */
          if (opterr && opt_str[0] != ':')
            fprintf (stderr, "%s: no argument for `%c%c' option\n",
                     argv[0], sw_char, c);
          optopt = c;
          return (opt_str[0] == ':' ? ':' : '?');
        }
      else
        {
          PUT (options);
          optarg = argv[optind++];
        }
    }
  return c;
}
