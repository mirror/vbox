/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_environment.h"
#include "cr_error.h"
#include "cr_string.h"
#include "cr_net.h"
#include "cr_process.h"

#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

#ifndef IN_GUEST
#define LOG_GROUP LOG_GROUP_SHARED_CROPENGL
#include <VBox/log.h>
#endif

static char my_hostname[256];
#ifdef WINDOWS
static HANDLE my_pid;
#else
static int my_pid = 0;
#endif
static int canada = 0;
static int swedish_chef = 0;
static int australia = 0;
static int warnings_enabled = 1;

void __getHostInfo( void )
{
    char *temp;
    if ( crGetHostname( my_hostname, sizeof( my_hostname ) ) )
    {
        crStrcpy( my_hostname, "????" );
    }
    temp = crStrchr( my_hostname, '.' );
    if (temp)
    {
        *temp = '\0';
    }
    my_pid = crGetPID();
}

static void __crCheckCanada(void)
{
    static int first = 1;
    if (first)
    {
        const char *env = crGetenv( "CR_CANADA" );
        if (env)
            canada = 1;
        first = 0;
    }
}

static void __crCheckSwedishChef(void)
{
    static int first = 1;
    if (first)
    {
        const char *env = crGetenv( "CR_SWEDEN" );
        if (env)
            swedish_chef = 1;
        first = 0;
    }
}

static void __crCheckAustralia(void)
{
    static int first = 1;
    if (first)
    {
        const char *env = crGetenv( "CR_AUSTRALIA" );
        const char *env2 = crGetenv( "CR_AUSSIE" );
        if (env || env2)
            australia = 1;
        first = 0;
    }
}

static void outputChromiumMessage( FILE *output, char *str )
{
    fprintf( output, "%s%s%s%s\n", str, 
            swedish_chef ? " BORK BORK BORK!" : "",
            canada ? ", eh?" : "",
            australia ? ", mate!" : ""
            );
    fflush( output );
}

#ifdef WINDOWS
static void crRedirectIOToConsole()
{
    int hConHandle;
    HANDLE StdHandle;
    FILE *fp;

    AllocConsole();

    StdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    hConHandle = _open_osfhandle((long)StdHandle, _O_TEXT);
    fp = _fdopen( hConHandle, "w" );
    *stdout = *fp;
    *stderr = *fp;

    StdHandle = GetStdHandle(STD_INPUT_HANDLE);
    hConHandle = _open_osfhandle((long)StdHandle, _O_TEXT);
    fp = _fdopen( hConHandle, "r" );
    *stdin = *fp;
}
#endif


DECLEXPORT(void) crError( char *format, ... )
{
    va_list args;
    static char txt[8092];
    int offset;
#ifdef WINDOWS
    DWORD err;
#endif

    __crCheckCanada();
    __crCheckSwedishChef();
    __crCheckAustralia();
    if (!my_hostname[0])
        __getHostInfo();
#ifdef WINDOWS
    if ((err = GetLastError()) != 0 && crGetenv( "CR_WINDOWS_ERRORS" ) != NULL )
    {
        static char buf[8092], *temp;

        SetLastError(0);
        sprintf( buf, "err=%d", err );

        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, err,
                MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                (LPTSTR) &temp, 0, NULL );
        if ( temp )
        {
            crStrncpy( buf, temp, sizeof(buf)-1 );
            buf[sizeof(buf)-1] = 0;
        }

        temp = buf + crStrlen(buf) - 1;
        while ( temp > buf && isspace( *temp ) )
        {
            *temp = '\0';
            temp--;
        }

        offset = sprintf( txt, "\t-----------------------\n\tWindows ERROR: %s\n\t----------------------\nCR Error(%s:%d): ", buf, my_hostname, my_pid );
    }
    else
    {
        offset = sprintf( txt, "OpenGL Error: ");
    }
#else
    offset = sprintf( txt, "OpenGL Error: " );
#endif
    va_start( args, format );
    vsprintf( txt + offset, format, args );
#if defined(IN_GUEST) || defined(DEBUG_leo) || defined(DEBUG_ll158262)
    outputChromiumMessage( stderr, txt );
#else
    LogRel(("%s\n", txt));
#endif
#ifdef WINDOWS
    if (crGetenv( "CR_GUI_ERROR" ) != NULL)
    {
        MessageBox( NULL, txt, "Chromium Error", MB_OK );
    }
    else
    {   
#endif
        va_end( args );
#ifdef WINDOWS
    }
#if !defined(DEBUG_leo) && !defined(DEBUG_ll158262)
    if (crGetenv( "CR_DEBUG_ON_ERROR" ) != NULL)
#endif
    {
        DebugBreak();
    }
#endif

#ifdef IN_GUEST
    /* Give chance for things to close down */
    raise( SIGTERM );

    exit(1);
#endif
}

void crEnableWarnings(int onOff)
{          
    warnings_enabled = onOff;
}

DECLEXPORT(void) crWarning( char *format, ... )
{
    if (warnings_enabled) {
        va_list args;
        static char txt[8092];
        int offset;

        __crCheckCanada();
        __crCheckSwedishChef();
        __crCheckAustralia();
        if (!my_hostname[0])
            __getHostInfo();
        offset = sprintf( txt, "OpenGL Warning: ");
        va_start( args, format );
        vsprintf( txt + offset, format, args );
#if defined(IN_GUEST) || defined(DEBUG_leo) || defined(DEBUG_ll158262)
        outputChromiumMessage( stderr, txt );
#else
        LogRel(("%s\n", txt));
#endif
        va_end( args );
    }
}

DECLEXPORT(void) crInfo( char *format, ... )
{
    va_list args;
    static char txt[8092];
    int offset;

    __crCheckCanada();
    __crCheckSwedishChef();
    __crCheckAustralia();
    if (!my_hostname[0])
        __getHostInfo();
    offset = sprintf( txt, "OpenGL Info: ");
    va_start( args, format );
    vsprintf( txt + offset, format, args );
#if defined(IN_GUEST) || defined(DEBUG_leo) || defined(DEBUG_ll158262)
    outputChromiumMessage( stderr, txt );
#else
    LogRel(("%s\n", txt));
#endif
    va_end( args );
}

DECLEXPORT(void) crDebug( char *format, ... )
{
    va_list args;
    static char txt[8092];
    int offset;
#ifdef WINDOWS
    DWORD err;
#endif
    static FILE *output;
    static int first_time = 1;
    static int silent = 0;

    if (first_time)
    {
        const char *fname = crGetenv( "CR_DEBUG_FILE" );
        first_time = 0;
        if (fname)
        {
            char debugFile[1000], *p;
            crStrcpy(debugFile, fname);
            p = crStrstr(debugFile, "%p");
            if (p) {
                /* replace %p with process number */
                unsigned long n = (unsigned long) crGetPID();
                sprintf(p, "%lu", n);
            }
            fname = debugFile;
            output = fopen( fname, "w" );
            if (!output)
            {
                crError( "Couldn't open debug log %s", fname ); 
            }
        }
        else
        {
#if defined(WINDOWS) && defined(IN_GUEST) && (defined(DEBUG_leo) || defined(DEBUG_ll158262))
            crRedirectIOToConsole();
#endif
            output = stderr;
        }
#ifndef DEBUG
        /* Release mode: only emit crDebug messages if CR_DEBUG
         * or CR_DEBUG_FILE is set.
         */
        if (!fname && !crGetenv("CR_DEBUG"))
            silent = 1;
#endif
    }

    if (silent)
        return;

    __crCheckCanada();
    __crCheckSwedishChef();
    __crCheckAustralia();
    if (!my_hostname[0])
        __getHostInfo();

#ifdef WINDOWS
    if ((err = GetLastError()) != 0 && crGetenv( "CR_WINDOWS_ERRORS" ) != NULL )
    {
        static char buf[8092], *temp;

        SetLastError(0);
        sprintf( buf, "err=%d", err );

        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, err,
                MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                (LPTSTR) &temp, 0, NULL );
        if ( temp )
        {
            crStrncpy( buf, temp, sizeof(buf)-1 );
            buf[sizeof(buf)-1] = 0;
        }

        temp = buf + crStrlen(buf) - 1;
        while ( temp > buf && isspace( *temp ) )
        {
            *temp = '\0';
            temp--;
        }

        offset = sprintf( txt, "\t-----------------------\n\tWindows ERROR: %s\n\t-----------------\nCR Debug(%s:%d): ", buf, my_hostname, my_pid );
    }
    else
    {
        offset = sprintf( txt, "[0x%x] OpenGL Debug: ", crThreadID());
    }
#else
    offset = sprintf( txt, "[0x%lx] OpenGL Debug: ", crThreadID());
#endif
    va_start( args, format );
    vsprintf( txt + offset, format, args );
#if defined(IN_GUEST) || defined(DEBUG_leo) || defined(DEBUG_ll158262)
    outputChromiumMessage( output, txt );
#else
    if (output==stderr)
    {
        LogRel(("%s\n", txt));
    }
    else
    {
        outputChromiumMessage(output, txt);
    }
#endif
    va_end( args );
}
