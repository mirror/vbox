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

#if defined(WINDOWS)
# define CR_DEBUG_TO_CONSOLE_ENABLE
#endif

#if defined(WINDOWS) && defined(IN_GUEST)
# ifndef CR_DEBUG_TO_BACKDOOR_ENABLE
#  error "CR_DEBUG_TO_BACKDOOR_ENABLE is expected!"
# endif
#else
# ifdef CR_DEBUG_TO_BACKDOOR_ENABLE
#  error "CR_DEBUG_TO_BACKDOOR_ENABLE is NOT expected!"
# endif
#endif


#ifdef CR_DEBUG_TO_BACKDOOR_ENABLE
# include <VBoxDispMpLogger.h>
# include <iprt/err.h>
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

#ifdef DEBUG_misha
//int g_VBoxFbgFBreakDdi = 0;
#define DebugBreak() Assert(0)
#endif

void __getHostInfo( void )
{
    char *temp;
    /* on windows guests we're typically get called in a context of VBoxOGL!DllMain ( which calls VBoxOGLcrutil!crNetInit ),
     * which may lead to deadlocks..
     * Avoid it as it is needed for debugging purposes only */
#if !defined(IN_GUEST) || !defined(RT_OS_WINDOWS)
    if ( crGetHostname( my_hostname, sizeof( my_hostname ) ) )
#endif
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

#if defined(DEBUG) && defined(WINDOWS) /* && (!defined(DEBUG_misha) || !defined(IN_GUEST) ) */
    OutputDebugString(str);
    OutputDebugString("\n");
#endif
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


DECLEXPORT(void) crError(const char *format, ... )
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
#if defined(IN_GUEST)
    crDebug("%s", txt);
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
#if !defined(DEBUG_leo) && !defined(DEBUG_ll158262) && !defined(DEBUG_misha)
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

DECLEXPORT(void) crWarning(const char *format, ... )
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
#if defined(IN_GUEST)
        crDebug("%s", txt);
        outputChromiumMessage( stderr, txt );
#else
        LogRel(("%s\n", txt));
#endif
        va_end( args );

#if defined(WINDOWS) && defined(DEBUG) && !defined(IN_GUEST)
        DebugBreak();
#endif
    }
}

DECLEXPORT(void) crInfo(const char *format, ... )
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
#if defined(IN_GUEST)
    crDebug("%s", txt);
    outputChromiumMessage( stderr, txt );
#else
    LogRel(("%s\n", txt));
#endif
    va_end( args );
}

DECLEXPORT(void) crDebug(const char *format, ... )
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
#ifdef CR_DEBUG_TO_BACKDOOR_ENABLE
    static int logBackdoor = 0;
#endif

    if (first_time)
    {
        const char *fname = crGetenv( "CR_DEBUG_FILE" );
        const char *fnamePrefix = crGetenv( "CR_DEBUG_FILE_PREFIX" );
        char str[2048];
#ifdef CR_DEBUG_TO_CONSOLE_ENABLE
        int logToConsole = 0;
#endif
#ifdef CR_DEBUG_TO_BACKDOOR_ENABLE
        if (crGetenv( "CR_DEBUG_TO_BACKDOOR" ))
        {
            int rc = VBoxDispMpLoggerInit();
            if (RT_SUCCESS(rc))
                logBackdoor = 1;
        }
#endif

        if (!fname && fnamePrefix)
        {
            char pname[1024];
            if (strlen(fnamePrefix) < sizeof (str) - sizeof (pname) - 20)
            {
                crGetProcName(pname, 1024);
                sprintf(str, "%s_%s_%d.txt", fnamePrefix, pname,
#ifdef RT_OS_WINDOWS
                        GetCurrentProcessId()
#else
                        crGetPID()
#endif
                        );
                fname = &str[0];
            }
        }

        first_time = 0;
        if (fname)
        {
            char debugFile[2048], *p;
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
#ifdef CR_DEBUG_TO_CONSOLE_ENABLE
            if (crGetenv( "CR_DEBUG_TO_CONSOLE" ))
            {
                crRedirectIOToConsole();
                logToConsole = 1;
            }
#endif
            output = stderr;
        }

#if !defined(DEBUG)/* || defined(DEBUG_misha)*/
        /* Release mode: only emit crDebug messages if CR_DEBUG
         * or CR_DEBUG_FILE is set.
         */
        if (!fname && !crGetenv("CR_DEBUG")
#ifdef CR_DEBUG_TO_CONSOLE_ENABLE
                    && !logToConsole
#endif
#ifdef CR_DEBUG_TO_BACKDOOR_ENABLE
                    && !logBackdoor
#endif
                )
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
        offset = sprintf( txt, "[0x%x.0x%x] OpenGL Debug: ", GetCurrentProcessId(), crThreadID());
    }
#else
    offset = sprintf( txt, "[0x%lx.0x%lx] OpenGL Debug: ", crGetPID(), crThreadID());
#endif
    va_start( args, format );
    vsprintf( txt + offset, format, args );
#ifdef CR_DEBUG_TO_BACKDOOR_ENABLE
    if (logBackdoor)
    {
        VBoxDispMpLoggerLog(txt);
    }
#endif
#if defined(IN_GUEST)
    outputChromiumMessage( output, txt );
#else
    if (!output
# ifndef DEBUG_misha
            || output==stderr
# endif
            )
    {
        LogRel(("%s\n", txt));
    }
    else
    {
# ifndef DEBUG_misha
        LogRel(("%s\n", txt));
# endif
        outputChromiumMessage(output, txt);
    }
#endif
    va_end( args );
}
