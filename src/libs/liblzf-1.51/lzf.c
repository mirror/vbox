/*
 * Copyright (c) 2000-2005 Marc Alexander Lehmann <schmorp@schmorp.de>
 * 
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 * 
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 *   3.  The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License version 2 (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of the above. If you wish to
 * allow the use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * BSD license, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL. If
 * you do not delete the provisions above, a recipient may use your version
 * of this file under either the BSD or the GPL.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <unistd.h>
#include <getopt.h>

#include "lzf.h"

typedef unsigned char u8;

static void
usage (int ec)
{
  fprintf (stderr, "\n"
           "lzf, a very lightweight compression/decompression filter\n"
           "written by Marc Lehmann <schmorp@schmorp.de> You can find more info at\n"
           "http://liblzf.plan9.de/\n"
           "\n"
 	   "USAGE: lzf -c [-b blocksize] | -d\n"
           "          -c  compress\n"
           "          -d  decompress\n"
           "          -b  specify the blocksize (default 64k-1)\n"
           "\n"
    );

  exit (ec);
}

/*
 * Anatomy: an lzf file consists of any number of blocks in the following format:
 *
 * "ZV\0" 2-byte-usize <uncompressed data>
 * "ZV\1" 2-byte-csize 2-byte-usize <compressed data>
 * "ZV\2" 4-byte-crc32-0xdebb20e3 (NYI)
 * 
 */

static void compress (unsigned int blocksize)
{
  ssize_t us;
  unsigned int cs;
  u8 buff1[64*1024];
  u8 buff2[64*1024];
  u8 header[3+2+2];

  header[0] = 'Z';
  header[1] = 'V';

  for(;;) {
    us = fread (buff1, 1, blocksize, stdin);

    if (us < blocksize)
      {
        if (us == 0)
          break;
        else if (!feof (stdin))
          {
            perror ("compress");
            exit (1);
          }
      }

    cs = lzf_compress (buff1, us, buff2, us - 4);

    if (cs)
      {
        header[2] = 1;
        header[3] = cs >> 8;
        header[4] = cs & 0xff;
        header[5] = us >> 8;
        header[6] = us & 0xff;

        fwrite (header, 3+2+2, 1, stdout);
        fwrite (buff2, cs, 1, stdout);
      }
    else
      {
        header[2] = 0;
        header[3] = us >> 8;
        header[4] = us & 0xff;

        fwrite (header, 3+2, 1, stdout);
        fwrite (buff1, us, 1, stdout);
      }
  } while (!feof (stdin));
}

static void decompress (void)
{
  ssize_t us;
  unsigned int cs;
  u8 buff1[64*1024];
  u8 buff2[64*1024];
  u8 header[3+2+2];

  for(;;) {
    if (fread (header, 3+2, 1, stdin) != 1)
      {
        if (feof (stdin))
          break;
        else
          {
            perror ("decompress");
            exit (1);
          }
      }

    if (header[0] != 'Z' || header[1] != 'V')
      {
        fprintf (stderr, "decompress: invalid stream - no magic number found\n");
        exit (1);
      }

    cs = (header[3] << 8) | header[4];

    if (header[2] == 1)
      {
        if (fread (header+3+2, 2, 1, stdin) != 1)
          {
            perror ("decompress");
            exit (1);
          }

        us = (header[5] << 8) | header[6];

        if (fread (buff1, cs, 1, stdin) != 1)
          {
            perror ("decompress");
            exit (1);
          }

        if (lzf_decompress (buff1, cs, buff2, us) != us)
          {
            fprintf (stderr, "decompress: invalid stream - data corrupted\n");
            exit (1);
          }

        fwrite (buff2, us, 1, stdout);
      }
    else if (header[2] == 0)
      {
        if (fread (buff2, cs, 1, stdin) != 1)
          {
            perror ("decompress");
            exit (1);
          }

        fwrite (buff2, cs, 1, stdout);
      }
    else
      {
        fprintf (stderr, "decompress: invalid stream - unknown block type\n");
        exit (1);
      }
  }
}

int
main (int argc, char *argv[])
{
  int c;
  unsigned int blocksize = 64*1024-1;
  enum { m_compress, m_decompress } mode = m_compress;

  while ((c = getopt (argc, argv, "cdb:h")) != -1)
    switch (c)
      {
      case 'c':
        mode = m_compress;
        break;

      case 'd':
        mode = m_decompress;
        break;

      case 'b':
        blocksize = atol (optarg);
        break;

      case 'h':
        usage (0);

      case ':':
        fprintf (stderr, "required argument missing, use -h\n");
        exit (1);

      case '?':
        fprintf (stderr, "unknown option, use -h\n");
        exit (1);

      default:
        usage (1);
      }

  if (mode == m_compress)
    compress (blocksize);
  else if (mode == m_decompress)
    decompress ();
  else
    abort ();

  return 0;
}
