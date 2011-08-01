/* zconf.h -- configuration of the zlib compression library
 * Copyright (C) 1995-2003 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id: zconf.h,v 1.1 2004/11/15 16:42:25 bird Exp $ */

#ifndef ZCONF_H
#define ZCONF_H

/*
 * If you *really* need a unique prefix for all types and library functions,
 * compile with -DZ_PREFIX. The "standard" zlib should be compiled without it.
 */
#ifdef VBOX /* Map public symbols to make sure the build is sane. */
#  define deflateInit_          vboxzlib_deflateInit_
#  define deflate               vboxzlib_deflate
#  define deflateEnd            vboxzlib_deflateEnd
#  define inflateInit_          vboxzlib_inflateInit_
#  define inflate               vboxzlib_inflate
#  define inflateEnd            vboxzlib_inflateEnd
#  define deflateInit2_         vboxzlib_deflateInit2_
#  define deflateSetDictionary  vboxzlib_deflateSetDictionary
#  define deflateCopy           vboxzlib_deflateCopy
#  define deflateReset          vboxzlib_deflateReset
#  define deflatePrime          vboxzlib_deflatePrime
#  define deflateParams         vboxzlib_deflateParams
#  define deflateBound          vboxzlib_deflateBound
#  define inflateInit2_         vboxzlib_inflateInit2_
#  define inflateSetDictionary  vboxzlib_inflateSetDictionary
#  define inflateSync           vboxzlib_inflateSync
#  define inflateSyncPoint      vboxzlib_inflateSyncPoint
#  define inflateCopy           vboxzlib_inflateCopy
#  define inflateReset          vboxzlib_inflateReset
#  define compress              vboxzlib_compress
#  define compress2             vboxzlib_compress2
#  define compressBound         vboxzlib_compressBound
#  define uncompress            vboxzlib_uncompress
#  define adler32               vboxzlib_adler32
#  define crc32                 vboxzlib_crc32
#  define get_crc_table         vboxzlib_get_crc_table

#  define inflateBackInit_      vboxzlib_inflateBackInit_
#  define inflate_fast          vboxzlib_inflate_fast
#  define inflate_table         vboxzlib_inflate_table
#  define zlibVersion           vboxzlib_zlibVersion
#  define zlibCompileFlags      vboxzlib_zlibCompileFlags
#  define z_error               vboxzlib_z_error
#  define zError                vboxzlib_zError
#  define zcalloc               vboxzlib_zcalloc
#  define zcfree                vboxzlib_zcfree
#  define inflateBack           vboxzlib_inflateBack
#  define _tr_init              vboxzlib__tr_init
#  define gzopen                vboxzlib_gzopen
#  define gzdopen               vboxzlib_gzdopen
#  define gzsetparams           vboxzlib_gzsetparams
#  define gzread                vboxzlib_gzread
#  define gzgetc                vboxzlib_gzgetc
#  define gzungetc              vboxzlib_gzungetc
#  define gzgets                vboxzlib_gzgets
#  define gzwrite               vboxzlib_gzwrite
#  define gzprintf              vboxzlib_gzprintf
#  define gzputc                vboxzlib_gzputc
#  define gzputs                vboxzlib_gzputs
#  define gzflush               vboxzlib_gzflush
#  define _tr_stored_block      vboxzlib__tr_stored_block
#  define gzseek                vboxzlib_gzseek
#  define _tr_align             vboxzlib__tr_align
#  define inflateBackEnd        vboxzlib_inflateBackEnd
#  define _tr_flush_block       vboxzlib__tr_flush_block
#  define gzrewind              vboxzlib_gzrewind
#  define gztell                vboxzlib_gztell
#  define gzeof                 vboxzlib_gzeof
#  define _tr_tally             vboxzlib__tr_tally
#  define gzclose               vboxzlib_gzclose
#  define gzerror               vboxzlib_gzerror
#  define gzclearerr            vboxzlib_gzclearerr
#  define z_verbose             vboxzlib_z_verbose
#  define deflate_copyright     vboxzlib_deflate_copyright
#  define inflate_copyright     vboxzlib_inflate_copyright
#  define _dist_code            vboxzlib__dist_code
#  define _length_code          vboxzlib__length_code
#  define z_errmsg              vboxzlib_z_errmsg

#  define Byte                  vboxzlib_Byte
#  define uInt                  vboxzlib_uInt
#  define uLong                 vboxzlib_uLong
#  define Bytef                 vboxzlib_Bytef
#  define charf                 vboxzlib_charf
#  define intf                  vboxzlib_intf
#  define uIntf                 vboxzlib_uIntf
#  define uLongf                vboxzlib_uLongf
#  define voidpf                vboxzlib_voidpf
#  define voidp                 vboxzlib_voidp
#else /* !VBOX */
#ifdef Z_PREFIX
#  define deflateInit_  z_deflateInit_
#  define deflate       z_deflate
#  define deflateEnd    z_deflateEnd
#  define inflateInit_  z_inflateInit_
#  define inflate       z_inflate
#  define inflateEnd    z_inflateEnd
#  define deflateInit2_ z_deflateInit2_
#  define deflateSetDictionary z_deflateSetDictionary
#  define deflateCopy   z_deflateCopy
#  define deflateReset  z_deflateReset
#  define deflatePrime  z_deflatePrime
#  define deflateParams z_deflateParams
#  define deflateBound  z_deflateBound
#  define inflateInit2_ z_inflateInit2_
#  define inflateSetDictionary z_inflateSetDictionary
#  define inflateSync   z_inflateSync
#  define inflateSyncPoint z_inflateSyncPoint
#  define inflateCopy   z_inflateCopy
#  define inflateReset  z_inflateReset
#  define compress      z_compress
#  define compress2     z_compress2
#  define compressBound z_compressBound
#  define uncompress    z_uncompress
#  define adler32       z_adler32
#  define crc32         z_crc32
#  define get_crc_table z_get_crc_table

#  define Byte          z_Byte
#  define uInt          z_uInt
#  define uLong         z_uLong
#  define Bytef         z_Bytef
#  define charf         z_charf
#  define intf          z_intf
#  define uIntf         z_uIntf
#  define uLongf        z_uLongf
#  define voidpf        z_voidpf
#  define voidp         z_voidp
#endif
#endif /*!VBOX*/

#if defined(__MSDOS__) && !defined(MSDOS)
#  define MSDOS
#endif
#if (defined(OS_2) || defined(__OS2__)) && !defined(OS2)
#  define OS2
#endif
#if defined(_WINDOWS) && !defined(WINDOWS)
#  define WINDOWS
#endif
#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32)
#  define WIN32
#endif
#if (defined(_WIN64) || defined(__WIN64__)) && !defined(WIN64)
#  define WIN64
#endif
#if (defined(MSDOS) || defined(OS2) || defined(WINDOWS)) && !defined(WIN32)
#  if !defined(__GNUC__) && !defined(__FLAT__) && !defined(__386__)
#    ifndef SYS16BIT
#      define SYS16BIT
#    endif
#  endif
#endif

/*
 * Compile with -DMAXSEG_64K if the alloc function cannot allocate more
 * than 64k bytes at a time (needed on systems with 16-bit int).
 */
#ifdef SYS16BIT
#  define MAXSEG_64K
#endif
#ifdef MSDOS
#  define UNALIGNED_OK
#endif

#ifdef __STDC_VERSION__
#  ifndef STDC
#    define STDC
#  endif
#  if __STDC_VERSION__ >= 199901L
#    ifndef STDC99
#      define STDC99
#    endif
#  endif
#endif
#if !defined(STDC) && (defined(__STDC__) || defined(__cplusplus))
#  define STDC
#endif
#if !defined(STDC) && (defined(__GNUC__) || defined(__BORLANDC__))
#  define STDC
#endif
#if !defined(STDC) && (defined(MSDOS) || defined(WINDOWS) || defined(WIN32) || defined(WIN64))
#  define STDC
#endif
#if !defined(STDC) && (defined(OS2) || defined(__HOS_AIX__))
#  define STDC
#endif

#if defined(__OS400__) && !defined(STDC)    /* iSeries (formerly AS/400). */
#  define STDC
#endif

#ifndef STDC
#  ifndef const /* cannot use !defined(STDC) && !defined(const) on Mac */
#    define const       /* note: need a more gentle solution here */
#  endif
#endif

/* Some Mac compilers merge all .h files incorrectly: */
#if defined(__MWERKS__)||defined(applec)||defined(THINK_C)||defined(__SC__)
#  define NO_DUMMY_DECL
#endif

/* Maximum value for memLevel in deflateInit2 */
#ifndef MAX_MEM_LEVEL
#  ifdef MAXSEG_64K
#    define MAX_MEM_LEVEL 8
#  else
#    define MAX_MEM_LEVEL 9
#  endif
#endif

/* Maximum value for windowBits in deflateInit2 and inflateInit2.
 * WARNING: reducing MAX_WBITS makes minigzip unable to extract .gz files
 * created by gzip. (Files created by minigzip can still be extracted by
 * gzip.)
 */
#ifndef MAX_WBITS
#  define MAX_WBITS   15 /* 32K LZ77 window */
#endif

/* The memory requirements for deflate are (in bytes):
            (1 << (windowBits+2)) +  (1 << (memLevel+9))
 that is: 128K for windowBits=15  +  128K for memLevel = 8  (default values)
 plus a few kilobytes for small objects. For example, if you want to reduce
 the default memory requirements from 256K to 128K, compile with
     make CFLAGS="-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7"
 Of course this will generally degrade compression (there's no free lunch).

   The memory requirements for inflate are (in bytes) 1 << windowBits
 that is, 32K for windowBits=15 (default value) plus a few kilobytes
 for small objects.
*/

                        /* Type declarations */

#ifndef OF /* function prototypes */
#  ifdef STDC
#    define OF(args)  args
#  else
#    define OF(args)  ()
#  endif
#endif

/* The following definitions for FAR are needed only for MSDOS mixed
 * model programming (small or medium model with some far allocations).
 * This was tested only with MSC; for other MSDOS compilers you may have
 * to define NO_MEMCPY in zutil.h.  If you don't need the mixed model,
 * just define FAR to be empty.
 */
#ifdef SYS16BIT
#  if defined(M_I86SM) || defined(M_I86MM)
     /* MSC small or medium model */
#    define SMALL_MEDIUM
#    ifdef _MSC_VER
#      define FAR _far
#    else
#      define FAR far
#    endif
#  endif
#  if (defined(__SMALL__) || defined(__MEDIUM__))
     /* Turbo C small or medium model */
#    define SMALL_MEDIUM
#    ifdef __BORLANDC__
#      define FAR _far
#    else
#      define FAR far
#    endif
#  endif
#endif

#if defined(WINDOWS) || defined(WIN32) || defined(WIN64)
   /* If building or using zlib as a DLL, define ZLIB_DLL.
    * This is not mandatory, but it offers a little performance increase.
    */
#  ifdef ZLIB_DLL
#    if (defined(WIN32) || defined(WIN64)) && (!defined(__BORLANDC__) || (__BORLANDC__ >= 0x500))
#      ifdef ZLIB_INTERNAL
#        define ZEXTERN extern __declspec(dllexport)
#      else
#        define ZEXTERN extern __declspec(dllimport)
#      endif
#    endif
#  endif  /* ZLIB_DLL */
   /* If building or using zlib with the WINAPI/WINAPIV calling convention,
    * define ZLIB_WINAPI.
    * Caution: the standard ZLIB1.DLL is NOT compiled using ZLIB_WINAPI.
    */
#  ifdef ZLIB_WINAPI
#    ifdef FAR
#      undef FAR
#    endif
#    include <windows.h>
     /* No need for _export, use ZLIB.DEF instead. */
     /* For complete Windows compatibility, use WINAPI, not __stdcall. */
#    define ZEXPORT WINAPI
#    if defined(WIN32) || defined(WIN64)
#      define ZEXPORTVA WINAPIV
#    else
#      define ZEXPORTVA FAR CDECL
#    endif
#  endif
#endif

#if defined (__BEOS__)
#  ifdef ZLIB_DLL
#    ifdef ZLIB_INTERNAL
#      define ZEXPORT   __declspec(dllexport)
#      define ZEXPORTVA __declspec(dllexport)
#    else
#      define ZEXPORT   __declspec(dllimport)
#      define ZEXPORTVA __declspec(dllimport)
#    endif
#  endif
#endif

#ifndef ZEXTERN
#  define ZEXTERN extern
#endif
#ifndef ZEXPORT
#  define ZEXPORT
#endif
#ifndef ZEXPORTVA
#  define ZEXPORTVA
#endif

#ifndef FAR
#  define FAR
#endif

#if !defined(__MACTYPES__)
typedef unsigned char  Byte;  /* 8 bits */
#endif
typedef unsigned int   uInt;  /* 16 bits or more */
typedef unsigned long  uLong; /* 32 bits or more */

#ifdef SMALL_MEDIUM
   /* Borland C/C++ and some old MSC versions ignore FAR inside typedef */
#  define Bytef Byte FAR
#else
   typedef Byte  FAR Bytef;
#endif
typedef char  FAR charf;
typedef int   FAR intf;
typedef uInt  FAR uIntf;
typedef uLong FAR uLongf;

#ifdef STDC
   typedef void const *voidpc;
   typedef void FAR   *voidpf;
   typedef void       *voidp;
#else
   typedef Byte const *voidpc;
   typedef Byte FAR   *voidpf;
   typedef Byte       *voidp;
#endif

#if 0           /* HAVE_UNISTD_H -- this line is updated by ./configure */
#  include <sys/types.h> /* for off_t */
#  include <unistd.h>    /* for SEEK_* and off_t */
#  ifdef VMS
#    include <unixio.h>   /* for off_t */
#  endif
#  define z_off_t  off_t
#endif
#ifndef SEEK_SET
#  define SEEK_SET        0       /* Seek from beginning of file.  */
#  define SEEK_CUR        1       /* Seek from current position.  */
#  define SEEK_END        2       /* Set file pointer to EOF plus "offset" */
#endif
#ifndef z_off_t
#  define  z_off_t long
#endif

#if defined(__OS400__)
#define NO_vsnprintf
#endif

#if defined(__MVS__)
#  define NO_vsnprintf
#  ifdef FAR
#    undef FAR
#  endif
#endif

/* MVS linker does not support external names larger than 8 bytes */
#if defined(__MVS__)
#   pragma map(deflateInit_,"DEIN")
#   pragma map(deflateInit2_,"DEIN2")
#   pragma map(deflateEnd,"DEEND")
#   pragma map(deflateBound,"DEBND")
#   pragma map(inflateInit_,"ININ")
#   pragma map(inflateInit2_,"ININ2")
#   pragma map(inflateEnd,"INEND")
#   pragma map(inflateSync,"INSY")
#   pragma map(inflateSetDictionary,"INSEDI")
#   pragma map(compressBound,"CMBND")
#   pragma map(inflate_table,"INTABL")
#   pragma map(inflate_fast,"INFA")
#   pragma map(inflate_copyright,"INCOPY")
#endif

#endif /* ZCONF_H */
