/**
 * vboxweb.cpp:
 *      hand-coded parts of the webservice server. This is linked with the
 *      generated code in out/.../src/VBox/Main/webservice/methodmaps.cpp
 *      (plus static gSOAP server code) to implement the actual webservice
 *      server, to which clients can connect.
 *
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

// shared webservice header
#include "vboxweb.h"

// vbox headers
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/string.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>
#include <VBox/com/listeners.h>
#include <VBox/VBoxAuth.h>
#include <VBox/version.h>
#include <VBox/log.h>

#include <package-generated.h>

#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/message.h>
#include <iprt/process.h>
#include <iprt/rand.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <iprt/path.h>
#include <iprt/system.h>
#include <iprt/base64.h>
#include <iprt/stream.h>
#include <iprt/asm.h>

// workaround for compile problems on gcc 4.1
#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif

// gSOAP headers (must come after vbox includes because it checks for conflicting defs)
#include "soapH.h"

// standard headers
#include <map>
#include <list>

#ifdef __GNUC__
#pragma GCC visibility pop
#endif

// include generated namespaces table
#include "vboxwebsrv.nsmap"

RT_C_DECLS_BEGIN

// declarations for the generated WSDL text
extern DECLIMPORT(const unsigned char) g_abVBoxWebWSDL[];
extern DECLIMPORT(const unsigned) g_cbVBoxWebWSDL;

RT_C_DECLS_END

static void WebLogSoapError(struct soap *soap);

/****************************************************************************
 *
 * private typedefs
 *
 ****************************************************************************/

typedef std::map<uint64_t, ManagedObjectRef*>
            ManagedObjectsMapById;
typedef std::map<uint64_t, ManagedObjectRef*>::iterator
            ManagedObjectsIteratorById;
typedef std::map<uintptr_t, ManagedObjectRef*>
            ManagedObjectsMapByPtr;

typedef std::map<uint64_t, WebServiceSession*>
            SessionsMap;
typedef std::map<uint64_t, WebServiceSession*>::iterator
            SessionsMapIterator;

int fntWatchdog(RTTHREAD ThreadSelf, void *pvUser);

/****************************************************************************
 *
 * Read-only global variables
 *
 ****************************************************************************/

static ComPtr<IVirtualBoxClient> g_pVirtualBoxClient = NULL;

// generated strings in methodmaps.cpp
extern const char       *g_pcszISession,
                        *g_pcszIVirtualBox;

// globals for vboxweb command-line arguments
#define DEFAULT_TIMEOUT_SECS 300
#define DEFAULT_TIMEOUT_SECS_STRING "300"
int                     g_iWatchdogTimeoutSecs = DEFAULT_TIMEOUT_SECS;
int                     g_iWatchdogCheckInterval = 5;

const char              *g_pcszBindToHost = NULL;       // host; NULL = localhost
unsigned int            g_uBindToPort = 18083;          // port
unsigned int            g_uBacklog = 100;               // backlog = max queue size for requests

#ifdef WITH_OPENSSL
bool                    g_fSSL = false;                 // if SSL is enabled
const char              *g_pcszKeyFile = NULL;          // server key file
const char              *g_pcszPassword = NULL;         // password for server key
const char              *g_pcszCACert = NULL;           // file with trusted CA certificates
const char              *g_pcszCAPath = NULL;           // directory with trusted CA certificates
const char              *g_pcszDHFile = NULL;           // DH file name or DH key length in bits, NULL=use RSA
const char              *g_pcszRandFile = NULL;         // file with random data seed
const char              *g_pcszSID = "vboxwebsrv";      // server ID for SSL session cache
#endif /* WITH_OPENSSL */

unsigned int            g_cMaxWorkerThreads = 100;      // max. no. of worker threads
unsigned int            g_cMaxKeepAlive = 100;          // maximum number of soap requests in one connection

const char              *g_pcszAuthentication = NULL;   // web service authentication

uint32_t                g_cHistory = 10;                // enable log rotation, 10 files
uint32_t                g_uHistoryFileTime = RT_SEC_1DAY; // max 1 day per file
uint64_t                g_uHistoryFileSize = 100 * _1M; // max 100MB per file
bool                    g_fVerbose = false;             // be verbose

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
bool                    g_fDaemonize = false;           // run in background.
#endif

const WSDLT_ID          g_EmptyWSDLID;                  // for NULL MORs

/****************************************************************************
 *
 * Writeable global variables
 *
 ****************************************************************************/

// The one global SOAP queue created by main().
class SoapQ;
SoapQ               *g_pSoapQ = NULL;

// this mutex protects the auth lib and authentication
util::WriteLockHandle  *g_pAuthLibLockHandle;

// this mutex protects the global VirtualBox reference below
static util::RWLockHandle *g_pVirtualBoxLockHandle;

static ComPtr<IVirtualBox> g_pVirtualBox = NULL;

// this mutex protects all of the below
util::WriteLockHandle  *g_pSessionsLockHandle;

SessionsMap         g_mapSessions;
ULONG64             g_iMaxManagedObjectID = 0;
ULONG64             g_cManagedObjects = 0;

// this mutex protects g_mapThreads
util::RWLockHandle  *g_pThreadsLockHandle;

// this mutex synchronizes logging
util::WriteLockHandle *g_pWebLogLockHandle;

// Threads map, so we can quickly map an RTTHREAD struct to a logger prefix
typedef std::map<RTTHREAD, com::Utf8Str> ThreadsMap;
ThreadsMap          g_mapThreads;

/****************************************************************************
 *
 *  Command line help
 *
 ****************************************************************************/

static const RTGETOPTDEF g_aOptions[]
    = {
        { "--help",             'h', RTGETOPT_REQ_NOTHING }, /* for DisplayHelp() */
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
        { "--background",       'b', RTGETOPT_REQ_NOTHING },
#endif
        { "--host",             'H', RTGETOPT_REQ_STRING },
        { "--port",             'p', RTGETOPT_REQ_UINT32 },
#ifdef WITH_OPENSSL
        { "--ssl",              's', RTGETOPT_REQ_NOTHING },
        { "--keyfile",          'K', RTGETOPT_REQ_STRING },
        { "--passwordfile",     'a', RTGETOPT_REQ_STRING },
        { "--cacert",           'c', RTGETOPT_REQ_STRING },
        { "--capath",           'C', RTGETOPT_REQ_STRING },
        { "--dhfile",           'D', RTGETOPT_REQ_STRING },
        { "--randfile",         'r', RTGETOPT_REQ_STRING },
#endif /* WITH_OPENSSL */
        { "--timeout",          't', RTGETOPT_REQ_UINT32 },
        { "--check-interval",   'i', RTGETOPT_REQ_UINT32 },
        { "--threads",          'T', RTGETOPT_REQ_UINT32 },
        { "--keepalive",        'k', RTGETOPT_REQ_UINT32 },
        { "--authentication",   'A', RTGETOPT_REQ_STRING },
        { "--verbose",          'v', RTGETOPT_REQ_NOTHING },
        { "--pidfile",          'P', RTGETOPT_REQ_STRING },
        { "--logfile",          'F', RTGETOPT_REQ_STRING },
        { "--logrotate",        'R', RTGETOPT_REQ_UINT32 },
        { "--logsize",          'S', RTGETOPT_REQ_UINT64 },
        { "--loginterval",      'I', RTGETOPT_REQ_UINT32 }
    };

void DisplayHelp()
{
    RTStrmPrintf(g_pStdErr, "\nUsage: vboxwebsrv [options]\n\nSupported options (default values in brackets):\n");
    for (unsigned i = 0;
         i < RT_ELEMENTS(g_aOptions);
         ++i)
    {
        std::string str(g_aOptions[i].pszLong);
        str += ", -";
        str += g_aOptions[i].iShort;
        str += ":";

        const char *pcszDescr = "";

        switch (g_aOptions[i].iShort)
        {
            case 'h':
                pcszDescr = "Print this help message and exit.";
                break;

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
            case 'b':
                pcszDescr = "Run in background (daemon mode).";
                break;
#endif

            case 'H':
                pcszDescr = "The host to bind to (localhost).";
                break;

            case 'p':
                pcszDescr = "The port to bind to (18083).";
                break;

#ifdef WITH_OPENSSL
            case 's':
                pcszDescr = "Enable SSL/TLS encryption.";
                break;

            case 'K':
                pcszDescr = "Server key and certificate file, PEM format (\"\").";
                break;

            case 'a':
                pcszDescr = "File name for password to server key (\"\").";
                break;

            case 'c':
                pcszDescr = "CA certificate file, PEM format (\"\").";
                break;

            case 'C':
                pcszDescr = "CA certificate path (\"\").";
                break;

            case 'D':
                pcszDescr = "DH file name or DH key length in bits (\"\").";
                break;

            case 'r':
                pcszDescr = "File containing seed for random number generator (\"\").";
                break;
#endif /* WITH_OPENSSL */

            case 't':
                pcszDescr = "Session timeout in seconds; 0 = disable timeouts (" DEFAULT_TIMEOUT_SECS_STRING ").";
                break;

            case 'T':
                pcszDescr = "Maximum number of worker threads to run in parallel (100).";
                break;

            case 'k':
                pcszDescr = "Maximum number of requests before a socket will be closed (100).";
                break;

            case 'A':
                pcszDescr = "Authentication method for the webservice (\"\").";
                break;

            case 'i':
                pcszDescr = "Frequency of timeout checks in seconds (5).";
                break;

            case 'v':
                pcszDescr = "Be verbose.";
                break;

            case 'P':
                pcszDescr = "Name of the PID file which is created when the daemon was started.";
                break;

            case 'F':
                pcszDescr = "Name of file to write log to (no file).";
                break;

            case 'R':
                pcszDescr = "Number of log files (0 disables log rotation).";
                break;

            case 'S':
                pcszDescr = "Maximum size of a log file to trigger rotation (bytes).";
                break;

            case 'I':
                pcszDescr = "Maximum time interval to trigger log rotation (seconds).";
                break;
        }

        RTStrmPrintf(g_pStdErr, "%-23s%s\n", str.c_str(), pcszDescr);
    }
}

/****************************************************************************
 *
 * SoapQ, SoapThread (multithreading)
 *
 ****************************************************************************/

class SoapQ;

class SoapThread
{
public:
    /**
     * Constructor. Creates the new thread and makes it call process() for processing the queue.
     * @param u Thread number. (So we can count from 1 and be readable.)
     * @param q SoapQ instance which has the queue to process.
     * @param soap struct soap instance from main() which we copy here.
     */
    SoapThread(size_t u,
               SoapQ &q,
               const struct soap *soap)
        : m_u(u),
          m_strThread(com::Utf8StrFmt("SQW%02d", m_u)),
          m_pQ(&q)
    {
        // make a copy of the soap struct for the new thread
        m_soap = soap_copy(soap);
        m_soap->fget = fnHttpGet;

        /* The soap.max_keep_alive value can be set to the maximum keep-alive calls allowed,
         * which is important to avoid a client from holding a thread indefinitely.
         * http://www.cs.fsu.edu/~engelen/soapdoc2.html#sec:keepalive
         *
         * Strings with 8-bit content can hold ASCII (default) or UTF8. The latter is
         * possible by enabling the SOAP_C_UTFSTRING flag.
         */
        soap_set_omode(m_soap, SOAP_IO_KEEPALIVE | SOAP_C_UTFSTRING);
        soap_set_imode(m_soap, SOAP_IO_KEEPALIVE | SOAP_C_UTFSTRING);
        m_soap->max_keep_alive = g_cMaxKeepAlive;

        int rc = RTThreadCreate(&m_pThread,
                                fntWrapper,
                                this,             // pvUser
                                0,               // cbStack,
                                RTTHREADTYPE_MAIN_HEAVY_WORKER,
                                0,
                                m_strThread.c_str());
        if (RT_FAILURE(rc))
        {
            RTMsgError("Cannot start worker thread %d: %Rrc\n", u, rc);
            exit(1);
        }
    }

    void process();

    static int fnHttpGet(struct soap *soap)
    {
        char *s = strchr(soap->path, '?');
        if (!s || strcmp(s, "?wsdl"))
            return SOAP_GET_METHOD;
        soap_response(soap, SOAP_HTML);
        soap_send_raw(soap, (const char *)g_abVBoxWebWSDL, g_cbVBoxWebWSDL);
        soap_end_send(soap);
        return SOAP_OK;
    }

    /**
     * Static function that can be passed to RTThreadCreate and that calls
     * process() on the SoapThread instance passed as the thread parameter.
     * @param pThread
     * @param pvThread
     * @return
     */
    static int fntWrapper(RTTHREAD pThread, void *pvThread)
    {
        SoapThread *pst = (SoapThread*)pvThread;
        pst->process();     // this never returns really
        return 0;
    }

    size_t          m_u;            // thread number
    com::Utf8Str    m_strThread;    // thread name ("SoapQWrkXX")
    SoapQ           *m_pQ;          // the single SOAP queue that all the threads service
    struct soap     *m_soap;        // copy of the soap structure for this thread (from soap_copy())
    RTTHREAD        m_pThread;      // IPRT thread struct for this thread
};

/**
 * SOAP queue encapsulation. There is only one instance of this, to
 * which add() adds a queue item (called on the main thread),
 * and from which get() fetch items, called from each queue thread.
 */
class SoapQ
{
public:

    /**
     * Constructor. Creates the soap queue.
     * @param pSoap
     */
    SoapQ(const struct soap *pSoap)
        : m_soap(pSoap),
          m_mutex(util::LOCKCLASS_OBJECTSTATE),     // lowest lock order, no other may be held while this is held
          m_cIdleThreads(0)
    {
        RTSemEventMultiCreate(&m_event);
    }

    ~SoapQ()
    {
        RTSemEventMultiDestroy(m_event);
    }

    /**
     * Adds the given socket to the SOAP queue and posts the
     * member event sem to wake up the workers. Called on the main thread
     * whenever a socket has work to do. Creates a new SOAP thread on the
     * first call or when all existing threads are busy.
     * @param s Socket from soap_accept() which has work to do.
     */
    uint32_t add(int s)
    {
        uint32_t cItems;
        util::AutoWriteLock qlock(m_mutex COMMA_LOCKVAL_SRC_POS);

        // if no threads have yet been created, or if all threads are busy,
        // create a new SOAP thread
        if (    !m_cIdleThreads
                // but only if we're not exceeding the global maximum (default is 100)
             && (m_llAllThreads.size() < g_cMaxWorkerThreads)
           )
        {
            SoapThread *pst = new SoapThread(m_llAllThreads.size() + 1,
                                             *this,
                                             m_soap);
            m_llAllThreads.push_back(pst);
            util::AutoWriteLock thrLock(g_pThreadsLockHandle COMMA_LOCKVAL_SRC_POS);
            g_mapThreads[pst->m_pThread] = com::Utf8StrFmt("[%3u]", pst->m_u);
            ++m_cIdleThreads;
        }

        // enqueue the socket of this connection and post eventsem so that
        // one of the threads (possibly the one just created) can pick it up
        m_llSocketsQ.push_back(s);
        cItems = m_llSocketsQ.size();
        qlock.release();

        // unblock one of the worker threads
        RTSemEventMultiSignal(m_event);

        return cItems;
    }

    /**
     * Blocks the current thread until work comes in; then returns
     * the SOAP socket which has work to do. This reduces m_cIdleThreads
     * by one, and the caller MUST call done() when it's done processing.
     * Called from the worker threads.
     * @param cIdleThreads out: no. of threads which are currently idle (not counting the caller)
     * @param cThreads out: total no. of SOAP threads running
     * @return
     */
    int get(size_t &cIdleThreads, size_t &cThreads)
    {
        while (1)
        {
            // wait for something to happen
            RTSemEventMultiWait(m_event, RT_INDEFINITE_WAIT);

            util::AutoWriteLock qlock(m_mutex COMMA_LOCKVAL_SRC_POS);
            if (m_llSocketsQ.size())
            {
                int socket = m_llSocketsQ.front();
                m_llSocketsQ.pop_front();
                cIdleThreads = --m_cIdleThreads;
                cThreads = m_llAllThreads.size();

                // reset the multi event only if the queue is now empty; otherwise
                // another thread will also wake up when we release the mutex and
                // process another one
                if (m_llSocketsQ.size() == 0)
                    RTSemEventMultiReset(m_event);

                qlock.release();

                return socket;
            }

            // nothing to do: keep looping
        }
    }

    /**
     * To be called by a worker thread after fetching an item from the
     * queue via get() and having finished its lengthy processing.
     */
    void done()
    {
        util::AutoWriteLock qlock(m_mutex COMMA_LOCKVAL_SRC_POS);
        ++m_cIdleThreads;
    }

    const struct soap       *m_soap;            // soap structure created by main(), passed to constructor

    util::WriteLockHandle   m_mutex;
    RTSEMEVENTMULTI         m_event;            // posted by add(), blocked on by get()

    std::list<SoapThread*>  m_llAllThreads;     // all the threads created by the constructor
    size_t                  m_cIdleThreads;     // threads which are currently idle (statistics)

    // A std::list abused as a queue; this contains the actual jobs to do,
    // each int being a socket from soap_accept()
    std::list<int>          m_llSocketsQ;
};

/**
 * Thread function for each of the SOAP queue worker threads. This keeps
 * running, blocks on the event semaphore in SoapThread.SoapQ and picks
 * up a socket from the queue therein, which has been put there by
 * beginProcessing().
 */
void SoapThread::process()
{
    WebLog("New SOAP thread started\n");

    while (1)
    {
        // wait for a socket to arrive on the queue
        size_t cIdleThreads = 0, cThreads = 0;
        m_soap->socket = m_pQ->get(cIdleThreads, cThreads);

        WebLog("Processing connection from IP=%lu.%lu.%lu.%lu socket=%d (%d out of %d threads idle)\n",
               (m_soap->ip >> 24) & 0xFF,
               (m_soap->ip >> 16) & 0xFF,
               (m_soap->ip >> 8)  & 0xFF,
               m_soap->ip         & 0xFF,
               m_soap->socket,
               cIdleThreads,
               cThreads);

        // Ensure that we don't get stuck indefinitely for connections using
        // keepalive, otherwise stale connections tie up worker threads.
        m_soap->send_timeout = 60;
        m_soap->recv_timeout = 60;
        // process the request; this goes into the COM code in methodmaps.cpp
        do {
#ifdef WITH_OPENSSL
            if (g_fSSL && soap_ssl_accept(m_soap))
            {
                WebLogSoapError(m_soap);
                break;
            }
#endif /* WITH_OPENSSL */
            soap_serve(m_soap);
        } while (0);

        soap_destroy(m_soap); // clean up class instances
        soap_end(m_soap); // clean up everything and close socket

        // tell the queue we're idle again
        m_pQ->done();
    }
}

/****************************************************************************
 *
 * VirtualBoxClient event listener
 *
 ****************************************************************************/

class VirtualBoxClientEventListener
{
public:
    VirtualBoxClientEventListener()
    {
    }

    virtual ~VirtualBoxClientEventListener()
    {
    }

    HRESULT init()
    {
       return S_OK;
    }

    void uninit()
    {
    }


    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent *aEvent)
    {
        switch (aType)
        {
            case VBoxEventType_OnVBoxSVCAvailabilityChanged:
            {
                ComPtr<IVBoxSVCAvailabilityChangedEvent> pVSACEv = aEvent;
                Assert(pVSACEv);
                BOOL fAvailable = FALSE;
                pVSACEv->COMGETTER(Available)(&fAvailable);
                if (!fAvailable)
                {
                    WebLog("VBoxSVC became unavailable\n");
                    {
                        util::AutoWriteLock vlock(g_pVirtualBoxLockHandle COMMA_LOCKVAL_SRC_POS);
                        g_pVirtualBox = NULL;
                    }
                    {
                        // we're messing with sessions, so lock them
                        util::AutoWriteLock lock(g_pSessionsLockHandle COMMA_LOCKVAL_SRC_POS);
                        WEBDEBUG(("SVC unavailable: deleting %d sessions\n", g_mapSessions.size()));

                        SessionsMap::iterator it = g_mapSessions.begin(),
                                              itEnd = g_mapSessions.end();
                        while (it != itEnd)
                        {
                            WebServiceSession *pSession = it->second;
                            WEBDEBUG(("SVC unavailable: Session %llX stale, deleting\n", pSession->getID()));
                            delete pSession;
                            it = g_mapSessions.begin();
                        }
                    }
                }
                else
                {
                    WebLog("VBoxSVC became available\n");
                    util::AutoWriteLock vlock(g_pVirtualBoxLockHandle COMMA_LOCKVAL_SRC_POS);
                    HRESULT hrc = g_pVirtualBoxClient->COMGETTER(VirtualBox)(g_pVirtualBox.asOutParam());
                    AssertComRC(hrc);
                }
                break;
            }
            default:
                AssertFailed();
        }

        return S_OK;
    }

private:
};

typedef ListenerImpl<VirtualBoxClientEventListener> VirtualBoxClientEventListenerImpl;

VBOX_LISTENER_DECLARE(VirtualBoxClientEventListenerImpl)

/**
 * Implementation for WEBLOG macro defined in vboxweb.h; this prints a message
 * to the console and optionally to the file that may have been given to the
 * vboxwebsrv command line.
 * @param pszFormat
 */
void WebLog(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    LogRel(("%s", psz));

    RTStrFree(psz);
}

/**
 * Helper for printing SOAP error messages.
 * @param soap
 */
/*static*/
void WebLogSoapError(struct soap *soap)
{
    if (soap_check_state(soap))
    {
        WebLog("Error: soap struct not initialized\n");
        return;
    }

    const char *pcszFaultString = *soap_faultstring(soap);
    const char **ppcszDetail = soap_faultcode(soap);
    WebLog("#### SOAP FAULT: %s [%s]\n",
           pcszFaultString ? pcszFaultString : "[no fault string available]",
           (ppcszDetail && *ppcszDetail) ? *ppcszDetail : "no details available");
}

static void WebLogHeaderFooter(PRTLOGGER pLoggerRelease, RTLOGPHASE enmPhase, PFNRTLOGPHASEMSG pfnLog)
{
    /* some introductory information */
    static RTTIMESPEC s_TimeSpec;
    char szTmp[256];
    if (enmPhase == RTLOGPHASE_BEGIN)
        RTTimeNow(&s_TimeSpec);
    RTTimeSpecToString(&s_TimeSpec, szTmp, sizeof(szTmp));

    switch (enmPhase)
    {
        case RTLOGPHASE_BEGIN:
        {
            pfnLog(pLoggerRelease,
                   "VirtualBox web service %s r%u %s (%s %s) release log\n"
#ifdef VBOX_BLEEDING_EDGE
                   "EXPERIMENTAL build " VBOX_BLEEDING_EDGE "\n"
#endif
                   "Log opened %s\n",
                   VBOX_VERSION_STRING, RTBldCfgRevision(), VBOX_BUILD_TARGET,
                   __DATE__, __TIME__, szTmp);

            int vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Product: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Release: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Version: %s\n", szTmp);
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Service Pack: %s\n", szTmp);

            /* the package type is interesting for Linux distributions */
            char szExecName[RTPATH_MAX];
            char *pszExecName = RTProcGetExecutablePath(szExecName, sizeof(szExecName));
            pfnLog(pLoggerRelease,
                   "Executable: %s\n"
                   "Process ID: %u\n"
                   "Package type: %s"
#ifdef VBOX_OSE
                   " (OSE)"
#endif
                   "\n",
                   pszExecName ? pszExecName : "unknown",
                   RTProcSelf(),
                   VBOX_PACKAGE_STRING);
            break;
        }

        case RTLOGPHASE_PREROTATE:
            pfnLog(pLoggerRelease, "Log rotated - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_POSTROTATE:
            pfnLog(pLoggerRelease, "Log continuation - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_END:
            pfnLog(pLoggerRelease, "End of log file - Log started %s\n", szTmp);
            break;

        default:
            /* nothing */;
    }
}

#ifdef WITH_OPENSSL
/****************************************************************************
 *
 * OpenSSL convenience functions for multithread support
 *
 ****************************************************************************/

static RTCRITSECT *g_pSSLMutexes = NULL;

struct CRYPTO_dynlock_value
{
    RTCRITSECT mutex;
};

static unsigned long CRYPTO_id_function()
{
    return RTThreadNativeSelf();
}

static void CRYPTO_locking_function(int mode, int n, const char * /*file*/, int /*line*/)
{
    if (mode & CRYPTO_LOCK)
        RTCritSectEnter(&g_pSSLMutexes[n]);
    else
        RTCritSectLeave(&g_pSSLMutexes[n]);
}

static struct CRYPTO_dynlock_value *CRYPTO_dyn_create_function(const char * /*file*/, int /*line*/)
{
    static uint32_t s_iCritSectDynlock = 0;
    struct CRYPTO_dynlock_value *value = (struct CRYPTO_dynlock_value *)RTMemAlloc(sizeof(struct CRYPTO_dynlock_value));
    if (value)
        RTCritSectInitEx(&value->mutex, RTCRITSECT_FLAGS_NO_LOCK_VAL,
                         NIL_RTLOCKVALCLASS, RTLOCKVAL_SUB_CLASS_NONE,
                         "openssl-dyn-%u", ASMAtomicIncU32(&s_iCritSectDynlock) - 1);

    return value;
}

static void CRYPTO_dyn_lock_function(int mode, struct CRYPTO_dynlock_value *value, const char * /*file*/, int /*line*/)
{
    if (mode & CRYPTO_LOCK)
        RTCritSectEnter(&value->mutex);
    else
        RTCritSectLeave(&value->mutex);
}

static void CRYPTO_dyn_destroy_function(struct CRYPTO_dynlock_value *value, const char * /*file*/, int /*line*/)
{
    if (value)
    {
        RTCritSectDelete(&value->mutex);
        free(value);
    }
}

static int CRYPTO_thread_setup()
{
    int num_locks = CRYPTO_num_locks();
    g_pSSLMutexes = (RTCRITSECT *)RTMemAlloc(num_locks * sizeof(RTCRITSECT));
    if (!g_pSSLMutexes)
        return SOAP_EOM;

    for (int i = 0; i < num_locks; i++)
    {
        int rc = RTCritSectInitEx(&g_pSSLMutexes[i], RTCRITSECT_FLAGS_NO_LOCK_VAL,
                                  NIL_RTLOCKVALCLASS, RTLOCKVAL_SUB_CLASS_NONE,
                                  "openssl-%d", i);
        if (RT_FAILURE(rc))
        {
            for ( ; i >= 0; i--)
                RTCritSectDelete(&g_pSSLMutexes[i]);
            RTMemFree(g_pSSLMutexes);
            g_pSSLMutexes = NULL;
            return SOAP_EOM;
        }
    }

    CRYPTO_set_id_callback(CRYPTO_id_function);
    CRYPTO_set_locking_callback(CRYPTO_locking_function);
    CRYPTO_set_dynlock_create_callback(CRYPTO_dyn_create_function);
    CRYPTO_set_dynlock_lock_callback(CRYPTO_dyn_lock_function);
    CRYPTO_set_dynlock_destroy_callback(CRYPTO_dyn_destroy_function);

    return SOAP_OK;
}

static void CRYPTO_thread_cleanup()
{
    if (!g_pSSLMutexes)
        return;

    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    CRYPTO_set_dynlock_create_callback(NULL);
    CRYPTO_set_dynlock_lock_callback(NULL);
    CRYPTO_set_dynlock_destroy_callback(NULL);

    int num_locks = CRYPTO_num_locks();
    for (int i = 0; i < num_locks; i++)
        RTCritSectDelete(&g_pSSLMutexes[i]);

    RTMemFree(g_pSSLMutexes);
    g_pSSLMutexes = NULL;
}
#endif /* WITH_OPENSSL */

/****************************************************************************
 *
 * SOAP queue pumper thread
 *
 ****************************************************************************/

void doQueuesLoop()
{
#ifdef WITH_OPENSSL
    if (g_fSSL && CRYPTO_thread_setup())
    {
        WebLog("Failed to set up OpenSSL thread mutex!");
        exit(RTEXITCODE_FAILURE);
    }
#endif /* WITH_OPENSSL */

    // set up gSOAP
    struct soap soap;
    soap_init(&soap);

#ifdef WITH_OPENSSL
    if (g_fSSL && soap_ssl_server_context(&soap, SOAP_SSL_DEFAULT, g_pcszKeyFile,
                                         g_pcszPassword, g_pcszCACert, g_pcszCAPath,
                                         g_pcszDHFile, g_pcszRandFile, g_pcszSID))
    {
        WebLogSoapError(&soap);
        exit(RTEXITCODE_FAILURE);
    }
#endif /* WITH_OPENSSL */

    soap.bind_flags |= SO_REUSEADDR;
            // avoid EADDRINUSE on bind()

    int m, s; // master and slave sockets
    m = soap_bind(&soap,
                  g_pcszBindToHost ? g_pcszBindToHost : "localhost",    // safe default host
                  g_uBindToPort,    // port
                  g_uBacklog);      // backlog = max queue size for requests
    if (m < 0)
        WebLogSoapError(&soap);
    else
    {
        WebLog("Socket connection successful: host = %s, port = %u, %smaster socket = %d\n",
               (g_pcszBindToHost) ? g_pcszBindToHost : "default (localhost)",
               g_uBindToPort,
#ifdef WITH_OPENSSL
               g_fSSL ? "SSL, " : "",
#else /* !WITH_OPENSSL */
               "",
#endif /*!WITH_OPENSSL */
               m);

        // initialize thread queue, mutex and eventsem
        g_pSoapQ = new SoapQ(&soap);

        for (uint64_t i = 1;
             ;
             i++)
        {
            // call gSOAP to handle incoming SOAP connection
            s = soap_accept(&soap);
            if (s < 0)
            {
                WebLogSoapError(&soap);
                continue;
            }

            // add the socket to the queue and tell worker threads to
            // pick up the job
            size_t cItemsOnQ = g_pSoapQ->add(s);
            WebLog("Request %llu on socket %d queued for processing (%d items on Q)\n", i, s, cItemsOnQ);
        }
    }
    soap_done(&soap); // close master socket and detach environment

#ifdef WITH_OPENSSL
    if (g_fSSL)
        CRYPTO_thread_cleanup();
#endif /* WITH_OPENSSL */
}

/**
 * Thread function for the "queue pumper" thread started from main(). This implements
 * the loop that takes SOAP calls from HTTP and serves them by handing sockets to the
 * SOAP queue worker threads.
 */
int fntQPumper(RTTHREAD ThreadSelf, void *pvUser)
{
    // store a log prefix for this thread
    util::AutoWriteLock thrLock(g_pThreadsLockHandle COMMA_LOCKVAL_SRC_POS);
    g_mapThreads[RTThreadSelf()] = "[ P ]";
    thrLock.release();

    doQueuesLoop();

    return 0;
}

#ifdef RT_OS_WINDOWS
// Required for ATL
static CComModule _Module;
#endif


/**
 * Start up the webservice server. This keeps running and waits
 * for incoming SOAP connections; for each request that comes in,
 * it calls method implementation code, most of it in the generated
 * code in methodmaps.cpp.
 *
 * @param argc
 * @param argv[]
 * @return
 */
int main(int argc, char *argv[])
{
    // initialize runtime
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    // store a log prefix for this thread
    g_mapThreads[RTThreadSelf()] = "[M  ]";

    RTStrmPrintf(g_pStdErr, VBOX_PRODUCT " web service version " VBOX_VERSION_STRING "\n"
                            "(C) 2005-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
                            "All rights reserved.\n");

    int c;
    const char *pszLogFile = NULL;
    const char *pszPidFile = NULL;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aOptions, RT_ELEMENTS(g_aOptions), 1, 0 /*fFlags*/);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'H':
                if (!ValueUnion.psz || !*ValueUnion.psz)
                {
                    /* Normalize NULL/empty string to NULL, which will be
                     * interpreted as "localhost" below. */
                    g_pcszBindToHost = NULL;
                }
                else
                    g_pcszBindToHost = ValueUnion.psz;
                break;

            case 'p':
                g_uBindToPort = ValueUnion.u32;
                break;

#ifdef WITH_OPENSSL
            case 's':
                g_fSSL = true;
                break;

            case 'K':
                g_pcszKeyFile = ValueUnion.psz;
                break;

            case 'a':
                if (ValueUnion.psz[0] == '\0')
                    g_pcszPassword = NULL;
                else
                {
                    PRTSTREAM StrmIn;
                    if (!strcmp(ValueUnion.psz, "-"))
                        StrmIn = g_pStdIn;
                    else
                    {
                        int vrc = RTStrmOpen(ValueUnion.psz, "r", &StrmIn);
                        if (RT_FAILURE(vrc))
                            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open password file (%s, %Rrc)", ValueUnion.psz, vrc);
                    }
                    char szPasswd[512];
                    int vrc = RTStrmGetLine(StrmIn, szPasswd, sizeof(szPasswd));
                    if (RT_FAILURE(vrc))
                        return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to read password (%s, %Rrc)", ValueUnion.psz, vrc);
                    g_pcszPassword = RTStrDup(szPasswd);
                    memset(szPasswd, '\0', sizeof(szPasswd));
                    if (StrmIn != g_pStdIn)
                        RTStrmClose(StrmIn);
                }
                break;

            case 'c':
                g_pcszCACert = ValueUnion.psz;
                break;

            case 'C':
                g_pcszCAPath = ValueUnion.psz;
                break;

            case 'D':
                g_pcszDHFile = ValueUnion.psz;
                break;

            case 'r':
                g_pcszRandFile = ValueUnion.psz;
                break;
#endif /* WITH_OPENSSL */

            case 't':
                g_iWatchdogTimeoutSecs = ValueUnion.u32;
                break;

            case 'i':
                g_iWatchdogCheckInterval = ValueUnion.u32;
                break;

            case 'F':
                pszLogFile = ValueUnion.psz;
                break;

            case 'R':
                g_cHistory = ValueUnion.u32;
                break;

            case 'S':
                g_uHistoryFileSize = ValueUnion.u64;
                break;

            case 'I':
                g_uHistoryFileTime = ValueUnion.u32;
                break;

            case 'P':
                pszPidFile = ValueUnion.psz;
                break;

            case 'T':
                g_cMaxWorkerThreads = ValueUnion.u32;
                break;

            case 'k':
                g_cMaxKeepAlive = ValueUnion.u32;
                break;

            case 'A':
                g_pcszAuthentication = ValueUnion.psz;
                break;

            case 'h':
                DisplayHelp();
                return 0;

            case 'v':
                g_fVerbose = true;
                break;

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
            case 'b':
                g_fDaemonize = true;
                break;
#endif
            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;

            default:
                rc = RTGetOptPrintError(c, &ValueUnion);
                return rc;
        }
    }

    /* create release logger */
    PRTLOGGER pLoggerRelease;
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    RTUINT fFlags = RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    fFlags |= RTLOGFLAGS_USECRLF;
#endif
    char szError[RTPATH_MAX + 128] = "";
    int vrc = RTLogCreateEx(&pLoggerRelease, fFlags, "all",
                            "VBOXWEBSRV_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups, RTLOGDEST_STDOUT,
                            WebLogHeaderFooter, g_cHistory, g_uHistoryFileSize, g_uHistoryFileTime,
                            szError, sizeof(szError), pszLogFile);
    if (RT_SUCCESS(vrc))
    {
        /* register this logger as the release logger */
        RTLogRelSetDefaultInstance(pLoggerRelease);

        /* Explicitly flush the log in case of VBOXWEBSRV_RELEASE_LOG=buffered. */
        RTLogFlush(pLoggerRelease);
    }
    else
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", szError, vrc);

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
    if (g_fDaemonize)
    {
        /* prepare release logging */
        char szLogFile[RTPATH_MAX];

        rc = com::GetVBoxUserHomeDirectory(szLogFile, sizeof(szLogFile));
        if (RT_FAILURE(rc))
             return RTMsgErrorExit(RTEXITCODE_FAILURE, "could not get base directory for logging: %Rrc", rc);
        rc = RTPathAppend(szLogFile, sizeof(szLogFile), "vboxwebsrv.log");
        if (RT_FAILURE(rc))
             return RTMsgErrorExit(RTEXITCODE_FAILURE, "could not construct logging path: %Rrc", rc);

        rc = RTProcDaemonizeUsingFork(false /* fNoChDir */, false /* fNoClose */, pszPidFile);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to daemonize, rc=%Rrc. exiting.", rc);

        /* create release logger */
        PRTLOGGER pLoggerReleaseFile;
        static const char * const s_apszGroupsFile[] = VBOX_LOGGROUP_NAMES;
        RTUINT fFlagsFile = RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        fFlagsFile |= RTLOGFLAGS_USECRLF;
#endif
        char szErrorFile[RTPATH_MAX + 128] = "";
        int vrc = RTLogCreateEx(&pLoggerReleaseFile, fFlagsFile, "all",
                                "VBOXWEBSRV_RELEASE_LOG", RT_ELEMENTS(s_apszGroupsFile), s_apszGroupsFile, RTLOGDEST_FILE,
                                WebLogHeaderFooter, g_cHistory, g_uHistoryFileSize, g_uHistoryFileTime,
                                szErrorFile, sizeof(szErrorFile), szLogFile);
        if (RT_SUCCESS(vrc))
        {
            /* register this logger as the release logger */
            RTLogRelSetDefaultInstance(pLoggerReleaseFile);

            /* Explicitly flush the log in case of VBOXWEBSRV_RELEASE_LOG=buffered. */
            RTLogFlush(pLoggerReleaseFile);
        }
        else
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", szErrorFile, vrc);
    }
#endif

    // initialize SOAP SSL support if enabled
#ifdef WITH_OPENSSL
    if (g_fSSL)
        soap_ssl_init();
#endif /* WITH_OPENSSL */

    // initialize COM/XPCOM
    HRESULT hrc = com::Initialize();
    if (FAILED(hrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to initialize COM! hrc=%Rhrc\n", hrc);

    hrc = g_pVirtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
    if (FAILED(hrc))
    {
        RTMsgError("failed to create the VirtualBoxClient object!");
        com::ErrorInfo info;
        if (!info.isFullAvailable() && !info.isBasicAvailable())
        {
            com::GluePrintRCMessage(hrc);
            RTMsgError("Most likely, the VirtualBox COM server is not running or failed to start.");
        }
        else
            com::GluePrintErrorInfo(info);
        return RTEXITCODE_FAILURE;
    }

    hrc = g_pVirtualBoxClient->COMGETTER(VirtualBox)(g_pVirtualBox.asOutParam());
    if (FAILED(hrc))
    {
        RTMsgError("Failed to get VirtualBox object (rc=%Rhrc)!", hrc);
        return RTEXITCODE_FAILURE;
    }

    // set the authentication method if requested
    if (g_pVirtualBox && g_pcszAuthentication && g_pcszAuthentication[0])
    {
        ComPtr<ISystemProperties> pSystemProperties;
        g_pVirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
        if (pSystemProperties)
            pSystemProperties->COMSETTER(WebServiceAuthLibrary)(com::Bstr(g_pcszAuthentication).raw());
    }

    /* VirtualBoxClient events registration. */
    ComPtr<IEventListener> vboxClientListener;
    {
        ComPtr<IEventSource> pES;
        CHECK_ERROR(g_pVirtualBoxClient, COMGETTER(EventSource)(pES.asOutParam()));
        ComObjPtr<VirtualBoxClientEventListenerImpl> clientListener;
        clientListener.createObject();
        clientListener->init(new VirtualBoxClientEventListener());
        vboxClientListener = clientListener;
        com::SafeArray<VBoxEventType_T> eventTypes;
        eventTypes.push_back(VBoxEventType_OnVBoxSVCAvailabilityChanged);
        CHECK_ERROR(pES, RegisterListener(vboxClientListener, ComSafeArrayAsInParam(eventTypes), true));
    }

    // create the global mutexes
    g_pAuthLibLockHandle = new util::WriteLockHandle(util::LOCKCLASS_WEBSERVICE);
    g_pVirtualBoxLockHandle = new util::RWLockHandle(util::LOCKCLASS_WEBSERVICE);
    g_pSessionsLockHandle = new util::WriteLockHandle(util::LOCKCLASS_WEBSERVICE);
    g_pThreadsLockHandle = new util::RWLockHandle(util::LOCKCLASS_OBJECTSTATE);
    g_pWebLogLockHandle = new util::WriteLockHandle(util::LOCKCLASS_WEBSERVICE);

    // SOAP queue pumper thread
    rc = RTThreadCreate(NULL,
                        fntQPumper,
                        NULL,        // pvUser
                        0,           // cbStack (default)
                        RTTHREADTYPE_MAIN_WORKER,
                        0,           // flags
                        "SQPmp");
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Cannot start SOAP queue pumper thread: %Rrc", rc);

    // watchdog thread
    if (g_iWatchdogTimeoutSecs > 0)
    {
        // start our watchdog thread
        rc = RTThreadCreate(NULL,
                            fntWatchdog,
                            NULL,
                            0,
                            RTTHREADTYPE_MAIN_WORKER,
                            0,
                            "Watchdog");
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Cannot start watchdog thread: %Rrc", rc);
    }

    com::EventQueue *pQ = com::EventQueue::getMainEventQueue();
    for (;;)
    {
        // we have to process main event queue
        WEBDEBUG(("Pumping COM event queue\n"));
        rc = pQ->processEventQueue(RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
            RTMsgError("processEventQueue -> %Rrc", rc);
    }

    /* VirtualBoxClient events unregistration. */
    if (vboxClientListener)
    {
        ComPtr<IEventSource> pES;
        CHECK_ERROR(g_pVirtualBoxClient, COMGETTER(EventSource)(pES.asOutParam()));
        if (!pES.isNull())
            CHECK_ERROR(pES, UnregisterListener(vboxClientListener));
        vboxClientListener.setNull();
    }

    com::Shutdown();

    return 0;
}

/****************************************************************************
 *
 * Watchdog thread
 *
 ****************************************************************************/

/**
 * Watchdog thread, runs in the background while the webservice is alive.
 *
 * This gets started by main() and runs in the background to check all sessions
 * for whether they have been no requests in a configurable timeout period. In
 * that case, the session is automatically logged off.
 */
int fntWatchdog(RTTHREAD ThreadSelf, void *pvUser)
{
    // store a log prefix for this thread
    util::AutoWriteLock thrLock(g_pThreadsLockHandle COMMA_LOCKVAL_SRC_POS);
    g_mapThreads[RTThreadSelf()] = "[W  ]";
    thrLock.release();

    WEBDEBUG(("Watchdog thread started\n"));

    while (1)
    {
        WEBDEBUG(("Watchdog: sleeping %d seconds\n", g_iWatchdogCheckInterval));
        RTThreadSleep(g_iWatchdogCheckInterval * 1000);

        time_t                      tNow;
        time(&tNow);

        // we're messing with sessions, so lock them
        util::AutoWriteLock lock(g_pSessionsLockHandle COMMA_LOCKVAL_SRC_POS);
        WEBDEBUG(("Watchdog: checking %d sessions\n", g_mapSessions.size()));

        SessionsMap::iterator it = g_mapSessions.begin(),
                              itEnd = g_mapSessions.end();
        while (it != itEnd)
        {
            WebServiceSession *pSession = it->second;
            WEBDEBUG(("Watchdog: tNow: %d, session timestamp: %d\n", tNow, pSession->getLastObjectLookup()));
            if (   tNow
                 > pSession->getLastObjectLookup() + g_iWatchdogTimeoutSecs
               )
            {
                WEBDEBUG(("Watchdog: Session %llX timed out, deleting\n", pSession->getID()));
                delete pSession;
                it = g_mapSessions.begin();
            }
            else
                ++it;
        }

        // re-set the authentication method in case it has been changed
        if (g_pVirtualBox && g_pcszAuthentication && g_pcszAuthentication[0])
        {
            ComPtr<ISystemProperties> pSystemProperties;
            g_pVirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
            if (pSystemProperties)
                pSystemProperties->COMSETTER(WebServiceAuthLibrary)(com::Bstr(g_pcszAuthentication).raw());
        }
    }

    WEBDEBUG(("Watchdog thread ending\n"));
    return 0;
}

/****************************************************************************
 *
 * SOAP exceptions
 *
 ****************************************************************************/

/**
 * Helper function to raise a SOAP fault. Called by the other helper
 * functions, which raise specific SOAP faults.
 *
 * @param soap
 * @param str
 * @param extype
 * @param ex
 */
void RaiseSoapFault(struct soap *soap,
                    const char *pcsz,
                    int extype,
                    void *ex)
{
    // raise the fault
    soap_sender_fault(soap, pcsz, NULL);

    struct SOAP_ENV__Detail *pDetail = (struct SOAP_ENV__Detail*)soap_malloc(soap, sizeof(struct SOAP_ENV__Detail));

    // without the following, gSOAP crashes miserably when sending out the
    // data because it will try to serialize all fields (stupid documentation)
    memset(pDetail, 0, sizeof(struct SOAP_ENV__Detail));

    // fill extended info depending on SOAP version
    if (soap->version == 2) // SOAP 1.2 is used
    {
        soap->fault->SOAP_ENV__Detail = pDetail;
        soap->fault->SOAP_ENV__Detail->__type = extype;
        soap->fault->SOAP_ENV__Detail->fault = ex;
        soap->fault->SOAP_ENV__Detail->__any = NULL; // no other XML data
    }
    else
    {
        soap->fault->detail = pDetail;
        soap->fault->detail->__type = extype;
        soap->fault->detail->fault = ex;
        soap->fault->detail->__any = NULL; // no other XML data
    }
}

/**
 * Raises a SOAP fault that signals that an invalid object was passed.
 *
 * @param soap
 * @param obj
 */
void RaiseSoapInvalidObjectFault(struct soap *soap,
                                 WSDLT_ID obj)
{
    _vbox__InvalidObjectFault *ex = soap_new__vbox__InvalidObjectFault(soap, 1);
    ex->badObjectID = obj;

    std::string str("VirtualBox error: ");
    str += "Invalid managed object reference \"" + obj + "\"";

    RaiseSoapFault(soap,
                   str.c_str(),
                   SOAP_TYPE__vbox__InvalidObjectFault,
                   ex);
}

/**
 * Return a safe C++ string from the given COM string,
 * without crashing if the COM string is empty.
 * @param bstr
 * @return
 */
std::string ConvertComString(const com::Bstr &bstr)
{
    com::Utf8Str ustr(bstr);
    return ustr.c_str();        // @todo r=dj since the length is known, we can probably use a better std::string allocator
}

/**
 * Return a safe C++ string from the given COM UUID,
 * without crashing if the UUID is empty.
 * @param bstr
 * @return
 */
std::string ConvertComString(const com::Guid &uuid)
{
    com::Utf8Str ustr(uuid.toString());
    return ustr.c_str();        // @todo r=dj since the length is known, we can probably use a better std::string allocator
}

/** Code to handle string <-> byte arrays base64 conversion. */
std::string Base64EncodeByteArray(ComSafeArrayIn(BYTE, aData))
{

    com::SafeArray<BYTE> sfaData(ComSafeArrayInArg(aData));
    ssize_t cbData = sfaData.size();

    if (cbData == 0)
        return "";

    ssize_t cchOut = RTBase64EncodedLength(cbData);

    RTCString aStr;

    aStr.reserve(cchOut+1);
    int rc = RTBase64Encode(sfaData.raw(), cbData,
                            aStr.mutableRaw(), aStr.capacity(),
                            NULL);
    AssertRC(rc);
    aStr.jolt();

    return aStr.c_str();
}

void Base64DecodeByteArray(std::string& aStr, ComSafeArrayOut(BYTE, aData))
{
    const char* pszStr = aStr.c_str();
    ssize_t cbOut = RTBase64DecodedSize(pszStr, NULL);

    Assert(cbOut > 0);

    com::SafeArray<BYTE> result(cbOut);
    int rc = RTBase64Decode(pszStr, result.raw(), cbOut, NULL, NULL);
    AssertRC(rc);

    result.detachTo(ComSafeArrayOutArg(aData));
}

/**
 * Raises a SOAP runtime fault. Implementation for the RaiseSoapRuntimeFault template
 * function in vboxweb.h.
 *
 * @param pObj
 */
void RaiseSoapRuntimeFault2(struct soap *soap,
                            HRESULT apirc,
                            IUnknown *pObj,
                            const com::Guid &iid)
{
    com::ErrorInfo info(pObj, iid.ref());

    WEBDEBUG(("   error, raising SOAP exception\n"));

    RTStrmPrintf(g_pStdErr, "API return code:            0x%08X (%Rhrc)\n", apirc, apirc);
    RTStrmPrintf(g_pStdErr, "COM error info result code: 0x%lX\n", info.getResultCode());
    RTStrmPrintf(g_pStdErr, "COM error info text:        %ls\n", info.getText().raw());

    // allocated our own soap fault struct
    _vbox__RuntimeFault *ex = soap_new__vbox__RuntimeFault(soap, 1);
    // some old vbox methods return errors without setting an error in the error info,
    // so use the error info code if it's set and the HRESULT from the method otherwise
    if (S_OK == (ex->resultCode = info.getResultCode()))
        ex->resultCode = apirc;
    ex->text = ConvertComString(info.getText());
    ex->component = ConvertComString(info.getComponent());
    ex->interfaceID = ConvertComString(info.getInterfaceID());

    // compose descriptive message
    com::Utf8StrFmt str("VirtualBox error: %s (0x%lX)", ex->text.c_str(), ex->resultCode);

    RaiseSoapFault(soap,
                   str.c_str(),
                   SOAP_TYPE__vbox__RuntimeFault,
                   ex);
}

/****************************************************************************
 *
 *  splitting and merging of object IDs
 *
 ****************************************************************************/

uint64_t str2ulonglong(const char *pcsz)
{
    uint64_t u = 0;
    RTStrToUInt64Full(pcsz, 16, &u);
    return u;
}

/**
 * Splits a managed object reference (in string form, as
 * passed in from a SOAP method call) into two integers for
 * session and object IDs, respectively.
 *
 * @param id
 * @param sessid
 * @param objid
 * @return
 */
bool SplitManagedObjectRef(const WSDLT_ID &id,
                           uint64_t *pSessid,
                           uint64_t *pObjid)
{
    // 64-bit numbers in hex have 16 digits; hence
    // the object-ref string must have 16 + "-" + 16 characters
    std::string str;
    if (    (id.length() == 33)
         && (id[16] == '-')
       )
    {
        char psz[34];
        memcpy(psz, id.c_str(), 34);
        psz[16] = '\0';
        if (pSessid)
            *pSessid = str2ulonglong(psz);
        if (pObjid)
            *pObjid = str2ulonglong(psz + 17);
        return true;
    }

    return false;
}

/**
 * Creates a managed object reference (in string form) from
 * two integers representing a session and object ID, respectively.
 *
 * @param sz Buffer with at least 34 bytes space to receive MOR string.
 * @param sessid
 * @param objid
 * @return
 */
void MakeManagedObjectRef(char *sz,
                          uint64_t &sessid,
                          uint64_t &objid)
{
    RTStrFormatNumber(sz, sessid, 16, 16, 0, RTSTR_F_64BIT | RTSTR_F_ZEROPAD);
    sz[16] = '-';
    RTStrFormatNumber(sz + 17, objid, 16, 16, 0, RTSTR_F_64BIT | RTSTR_F_ZEROPAD);
}

/****************************************************************************
 *
 *  class WebServiceSession
 *
 ****************************************************************************/

class WebServiceSessionPrivate
{
    public:
        ManagedObjectsMapById       _mapManagedObjectsById;
        ManagedObjectsMapByPtr      _mapManagedObjectsByPtr;
};

/**
 * Constructor for the session object.
 *
 * Preconditions: Caller must have locked g_pSessionsLockHandle.
 *
 * @param username
 * @param password
 */
WebServiceSession::WebServiceSession()
    : _fDestructing(false),
      _pISession(NULL),
      _tLastObjectLookup(0)
{
    _pp = new WebServiceSessionPrivate;
    _uSessionID = RTRandU64();

    // register this session globally
    Assert(g_pSessionsLockHandle->isWriteLockOnCurrentThread());
    g_mapSessions[_uSessionID] = this;
}

/**
 * Destructor. Cleans up and destroys all contained managed object references on the way.
 *
 * Preconditions: Caller must have locked g_pSessionsLockHandle.
 */
WebServiceSession::~WebServiceSession()
{
    // delete us from global map first so we can't be found
    // any more while we're cleaning up
    Assert(g_pSessionsLockHandle->isWriteLockOnCurrentThread());
    g_mapSessions.erase(_uSessionID);

    // notify ManagedObjectRef destructor so it won't
    // remove itself from the maps; this avoids rebalancing
    // the map's tree on every delete as well
    _fDestructing = true;

    // if (_pISession)
    // {
    //     delete _pISession;
    //     _pISession = NULL;
    // }

    ManagedObjectsMapById::iterator it,
                                    end = _pp->_mapManagedObjectsById.end();
    for (it = _pp->_mapManagedObjectsById.begin();
         it != end;
         ++it)
    {
        ManagedObjectRef *pRef = it->second;
        delete pRef;        // this frees the contained ComPtr as well
    }

    delete _pp;
}

/**
 *  Authenticate the username and password against an authentication authority.
 *
 *  @return 0 if the user was successfully authenticated, or an error code
 *  otherwise.
 */

int WebServiceSession::authenticate(const char *pcszUsername,
                                    const char *pcszPassword,
                                    IVirtualBox **ppVirtualBox)
{
    int rc = VERR_WEB_NOT_AUTHENTICATED;
    ComPtr<IVirtualBox> pVirtualBox;
    {
        util::AutoReadLock vlock(g_pVirtualBoxLockHandle COMMA_LOCKVAL_SRC_POS);
        pVirtualBox = g_pVirtualBox;
    }
    if (pVirtualBox.isNull())
        return rc;
    pVirtualBox.queryInterfaceTo(ppVirtualBox);

    util::AutoReadLock lock(g_pAuthLibLockHandle COMMA_LOCKVAL_SRC_POS);

    static bool fAuthLibLoaded = false;
    static PAUTHENTRY pfnAuthEntry = NULL;
    static PAUTHENTRY2 pfnAuthEntry2 = NULL;
    static PAUTHENTRY3 pfnAuthEntry3 = NULL;

    if (!fAuthLibLoaded)
    {
        // retrieve authentication library from system properties
        ComPtr<ISystemProperties> systemProperties;
        pVirtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());

        com::Bstr authLibrary;
        systemProperties->COMGETTER(WebServiceAuthLibrary)(authLibrary.asOutParam());
        com::Utf8Str filename = authLibrary;

        WEBDEBUG(("external authentication library is '%ls'\n", authLibrary.raw()));

        if (filename == "null")
            // authentication disabled, let everyone in:
            fAuthLibLoaded = true;
        else
        {
            RTLDRMOD hlibAuth = 0;
            do
            {
                rc = RTLdrLoad(filename.c_str(), &hlibAuth);
                if (RT_FAILURE(rc))
                {
                    WEBDEBUG(("%s() Failed to load external authentication library. Error code: %Rrc\n", __FUNCTION__, rc));
                    break;
                }

                if (RT_FAILURE(rc = RTLdrGetSymbol(hlibAuth, AUTHENTRY3_NAME, (void**)&pfnAuthEntry3)))
                {
                    WEBDEBUG(("%s(): Could not resolve import '%s'. Error code: %Rrc\n", __FUNCTION__, AUTHENTRY3_NAME, rc));

                    if (RT_FAILURE(rc = RTLdrGetSymbol(hlibAuth, AUTHENTRY2_NAME, (void**)&pfnAuthEntry2)))
                    {
                        WEBDEBUG(("%s(): Could not resolve import '%s'. Error code: %Rrc\n", __FUNCTION__, AUTHENTRY2_NAME, rc));

                        if (RT_FAILURE(rc = RTLdrGetSymbol(hlibAuth, AUTHENTRY_NAME, (void**)&pfnAuthEntry)))
                            WEBDEBUG(("%s(): Could not resolve import '%s'. Error code: %Rrc\n", __FUNCTION__, AUTHENTRY_NAME, rc));
                    }
                }

                if (pfnAuthEntry || pfnAuthEntry2 || pfnAuthEntry3)
                    fAuthLibLoaded = true;

            } while (0);
        }
    }

    rc = VERR_WEB_NOT_AUTHENTICATED;
    AuthResult result;
    if (pfnAuthEntry3)
    {
        result = pfnAuthEntry3("webservice", NULL, AuthGuestNotAsked, pcszUsername, pcszPassword, NULL, true, 0);
        WEBDEBUG(("%s(): result of AuthEntry(): %d\n", __FUNCTION__, result));
        if (result == AuthResultAccessGranted)
            rc = 0;
    }
    else if (pfnAuthEntry2)
    {
        result = pfnAuthEntry2(NULL, AuthGuestNotAsked, pcszUsername, pcszPassword, NULL, true, 0);
        WEBDEBUG(("%s(): result of VRDPAuth2(): %d\n", __FUNCTION__, result));
        if (result == AuthResultAccessGranted)
            rc = 0;
    }
    else if (pfnAuthEntry)
    {
        result = pfnAuthEntry(NULL, AuthGuestNotAsked, pcszUsername, pcszPassword, NULL);
        WEBDEBUG(("%s(): result of VRDPAuth(%s, [%d]): %d\n", __FUNCTION__, pcszUsername, strlen(pcszPassword), result));
        if (result == AuthResultAccessGranted)
            rc = 0;
    }
    else if (fAuthLibLoaded)
        // fAuthLibLoaded = true but both pointers are NULL:
        // then the authlib was "null" and auth was disabled
        rc = 0;
    else
    {
        WEBDEBUG(("Could not resolve AuthEntry, VRDPAuth2 or VRDPAuth entry point"));
    }

    lock.release();

    if (!rc)
    {
        do
        {
            // now create the ISession object that this webservice session can use
            // (and of which IWebsessionManager::getSessionObject returns a managed object reference)
            ComPtr<ISession> session;
            rc = g_pVirtualBoxClient->COMGETTER(Session)(session.asOutParam());
            if (FAILED(rc))
            {
                WEBDEBUG(("ERROR: cannot create session object!"));
                break;
            }

            ComPtr<IUnknown> p2 = session;
            _pISession = new ManagedObjectRef(*this,
                                              p2,                               // IUnknown *pobjUnknown
                                              session,                          // void *pobjInterface
                                              com::Guid(COM_IIDOF(ISession)),
                                              g_pcszISession);

            if (g_fVerbose)
            {
                ISession *p = session;
                WEBDEBUG(("   * %s: created session object with comptr 0x%lX, MOR = %s\n", __FUNCTION__, p, _pISession->getWSDLID().c_str()));
            }
        } while (0);
    }

    return rc;
}

/**
 *  Look up, in this session, whether a ManagedObjectRef has already been
 *  created for the given COM pointer.
 *
 *  Note how we require that a ComPtr<IUnknown> is passed, which causes a
 *  queryInterface call when the caller passes in a different type, since
 *  a ComPtr<IUnknown> will point to something different than a
 *  ComPtr<IVirtualBox>, for example. As we store the ComPtr<IUnknown> in
 *  our private hash table, we must search for one too.
 *
 * Preconditions: Caller must have locked g_pSessionsLockHandle.
 *
 * @param pcu pointer to a COM object.
 * @return The existing ManagedObjectRef that represents the COM object, or NULL if there's none yet.
 */
ManagedObjectRef* WebServiceSession::findRefFromPtr(const IUnknown *pObject)
{
    Assert(g_pSessionsLockHandle->isWriteLockOnCurrentThread());

    uintptr_t ulp = (uintptr_t)pObject;
    // WEBDEBUG(("   %s: looking up 0x%lX\n", __FUNCTION__, ulp));
    ManagedObjectsMapByPtr::iterator it = _pp->_mapManagedObjectsByPtr.find(ulp);
    if (it != _pp->_mapManagedObjectsByPtr.end())
    {
        ManagedObjectRef *pRef = it->second;
        WEBDEBUG(("   %s: found existing ref %s (%s) for COM obj 0x%lX\n", __FUNCTION__, pRef->getWSDLID().c_str(), pRef->getInterfaceName(), ulp));
        return pRef;
    }

    return NULL;
}

/**
 * Static method which attempts to find the session for which the given managed
 * object reference was created, by splitting the reference into the session and
 * object IDs and then looking up the session object for that session ID.
 *
 * Preconditions: Caller must have locked g_pSessionsLockHandle in read mode.
 *
 * @param id Managed object reference (with combined session and object IDs).
 * @return
 */
WebServiceSession* WebServiceSession::findSessionFromRef(const WSDLT_ID &id)
{
    Assert(g_pSessionsLockHandle->isWriteLockOnCurrentThread());

    WebServiceSession *pSession = NULL;
    uint64_t sessid;
    if (SplitManagedObjectRef(id,
                              &sessid,
                              NULL))
    {
        SessionsMapIterator it = g_mapSessions.find(sessid);
        if (it != g_mapSessions.end())
            pSession = it->second;
    }
    return pSession;
}

/**
 *
 */
const WSDLT_ID& WebServiceSession::getSessionWSDLID() const
{
    return _pISession->getWSDLID();
}

/**
 * Touches the webservice session to prevent it from timing out.
 *
 * Each webservice session has an internal timestamp that records
 * the last request made to it from the client that started it.
 * If no request was made within a configurable timeframe, then
 * the client is logged off automatically,
 * by calling IWebsessionManager::logoff()
 */
void WebServiceSession::touch()
{
    time(&_tLastObjectLookup);
}


/****************************************************************************
 *
 *  class ManagedObjectRef
 *
 ****************************************************************************/

/**
 *  Constructor, which assigns a unique ID to this managed object
 *  reference and stores it two global hashes:
 *
 *   a) G_mapManagedObjectsById, which maps ManagedObjectID's to
 *      instances of this class; this hash is then used by the
 *      findObjectFromRef() template function in vboxweb.h
 *      to quickly retrieve the COM object from its managed
 *      object ID (mostly in the context of the method mappers
 *      in methodmaps.cpp, when a web service client passes in
 *      a managed object ID);
 *
 *   b) G_mapManagedObjectsByComPtr, which maps COM pointers to
 *      instances of this class; this hash is used by
 *      createRefFromObject() to quickly figure out whether an
 *      instance already exists for a given COM pointer.
 *
 *  This constructor calls AddRef() on the given COM object, and
 *  the destructor will call Release(). We require two input pointers
 *  for that COM object, one generic IUnknown* pointer which is used
 *  as the map key, and a specific interface pointer (e.g. IMachine*)
 *  which must support the interface given in guidInterface. All
 *  three values are returned by getPtr(), which gives future callers
 *  a chance to reuse the specific interface pointer without having
 *  to call QueryInterface, which can be expensive.
 *
 *  This does _not_ check whether another instance already
 *  exists in the hash. This gets called only from the
 *  createOrFindRefFromComPtr() template function in vboxweb.h, which
 *  does perform that check.
 *
 * Preconditions: Caller must have locked g_pSessionsLockHandle.
 *
 * @param session Session to which the MOR will be added.
 * @param pobjUnknown Pointer to IUnknown* interface for the COM object; this will be used in the hashes.
 * @param pobjInterface Pointer to a specific interface for the COM object, described by guidInterface.
 * @param guidInterface Interface which pobjInterface points to.
 * @param pcszInterface String representation of that interface (e.g. "IMachine") for readability and logging.
 */
ManagedObjectRef::ManagedObjectRef(WebServiceSession &session,
                                   IUnknown *pobjUnknown,
                                   void *pobjInterface,
                                   const com::Guid &guidInterface,
                                   const char *pcszInterface)
    : _session(session),
      _pobjUnknown(pobjUnknown),
      _pobjInterface(pobjInterface),
      _guidInterface(guidInterface),
      _pcszInterface(pcszInterface)
{
    Assert(pobjUnknown);
    Assert(pobjInterface);

    // keep both stubs alive while this MOR exists (matching Release() calls are in destructor)
    uint32_t cRefs1 = pobjUnknown->AddRef();
    uint32_t cRefs2 = ((IUnknown*)pobjInterface)->AddRef();
    _ulp = (uintptr_t)pobjUnknown;

    Assert(g_pSessionsLockHandle->isWriteLockOnCurrentThread());
    _id = ++g_iMaxManagedObjectID;
    // and count globally
    ULONG64 cTotal = ++g_cManagedObjects;           // raise global count and make a copy for the debug message below

    char sz[34];
    MakeManagedObjectRef(sz, session._uSessionID, _id);
    _strID = sz;

    session._pp->_mapManagedObjectsById[_id] = this;
    session._pp->_mapManagedObjectsByPtr[_ulp] = this;

    session.touch();

    WEBDEBUG(("   * %s: MOR created for %s*=0x%lX (IUnknown*=0x%lX; COM refcount now %RI32/%RI32), new ID is %llX; now %lld objects total\n",
              __FUNCTION__,
              pcszInterface,
              pobjInterface,
              pobjUnknown,
              cRefs1,
              cRefs2,
              _id,
              cTotal));
}

/**
 * Destructor; removes the instance from the global hash of
 * managed objects. Calls Release() on the contained COM object.
 *
 * Preconditions: Caller must have locked g_pSessionsLockHandle.
 */
ManagedObjectRef::~ManagedObjectRef()
{
    Assert(g_pSessionsLockHandle->isWriteLockOnCurrentThread());
    ULONG64 cTotal = --g_cManagedObjects;

    Assert(_pobjUnknown);
    Assert(_pobjInterface);

    // we called AddRef() on both interfaces, so call Release() on
    // both as well, but in reverse order
    uint32_t cRefs2 = ((IUnknown*)_pobjInterface)->Release();
    uint32_t cRefs1 = _pobjUnknown->Release();
    WEBDEBUG(("   * %s: deleting MOR for ID %llX (%s; COM refcount now %RI32/%RI32); now %lld objects total\n", __FUNCTION__, _id, _pcszInterface, cRefs1, cRefs2, cTotal));

    // if we're being destroyed from the session's destructor,
    // then that destructor is iterating over the maps, so
    // don't remove us there! (data integrity + speed)
    if (!_session._fDestructing)
    {
        WEBDEBUG(("   * %s: removing from session maps\n", __FUNCTION__));
        _session._pp->_mapManagedObjectsById.erase(_id);
        if (_session._pp->_mapManagedObjectsByPtr.erase(_ulp) != 1)
            WEBDEBUG(("   WARNING: could not find %llX in _mapManagedObjectsByPtr\n", _ulp));
    }
}

/**
 * Static helper method for findObjectFromRef() template that actually
 * looks up the object from a given integer ID.
 *
 * This has been extracted into this non-template function to reduce
 * code bloat as we have the actual STL map lookup only in this function.
 *
 * This also "touches" the timestamp in the session whose ID is encoded
 * in the given integer ID, in order to prevent the session from timing
 * out.
 *
 * Preconditions: Caller must have locked g_mutexSessions.
 *
 * @param strId
 * @param iter
 * @return
 */
int ManagedObjectRef::findRefFromId(const WSDLT_ID &id,
                                    ManagedObjectRef **pRef,
                                    bool fNullAllowed)
{
    int rc = 0;

    do
    {
        // allow NULL (== empty string) input reference, which should return a NULL pointer
        if (!id.length() && fNullAllowed)
        {
            *pRef = NULL;
            return 0;
        }

        uint64_t sessid;
        uint64_t objid;
        WEBDEBUG(("   %s(): looking up objref %s\n", __FUNCTION__, id.c_str()));
        if (!SplitManagedObjectRef(id,
                                   &sessid,
                                   &objid))
        {
            rc = VERR_WEB_INVALID_MANAGED_OBJECT_REFERENCE;
            break;
        }

        SessionsMapIterator it = g_mapSessions.find(sessid);
        if (it == g_mapSessions.end())
        {
            WEBDEBUG(("   %s: cannot find session for objref %s\n", __FUNCTION__, id.c_str()));
            rc = VERR_WEB_INVALID_SESSION_ID;
            break;
        }

        WebServiceSession *pSess = it->second;
        // "touch" session to prevent it from timing out
        pSess->touch();

        ManagedObjectsIteratorById iter = pSess->_pp->_mapManagedObjectsById.find(objid);
        if (iter == pSess->_pp->_mapManagedObjectsById.end())
        {
            WEBDEBUG(("   %s: cannot find comobj for objref %s\n", __FUNCTION__, id.c_str()));
            rc = VERR_WEB_INVALID_OBJECT_ID;
            break;
        }

        *pRef = iter->second;

    } while (0);

    return rc;
}

/****************************************************************************
 *
 * interface IManagedObjectRef
 *
 ****************************************************************************/

/**
 * This is the hard-coded implementation for the IManagedObjectRef::getInterfaceName()
 * that our WSDL promises to our web service clients. This method returns a
 * string describing the interface that this managed object reference
 * supports, e.g. "IMachine".
 *
 * @param soap
 * @param req
 * @param resp
 * @return
 */
int __vbox__IManagedObjectRef_USCOREgetInterfaceName(
    struct soap *soap,
    _vbox__IManagedObjectRef_USCOREgetInterfaceName *req,
    _vbox__IManagedObjectRef_USCOREgetInterfaceNameResponse *resp)
{
    HRESULT rc = S_OK;
    WEBDEBUG(("-- entering %s\n", __FUNCTION__));

    do
    {
        // findRefFromId require the lock
        util::AutoWriteLock lock(g_pSessionsLockHandle COMMA_LOCKVAL_SRC_POS);

        ManagedObjectRef *pRef;
        if (!ManagedObjectRef::findRefFromId(req->_USCOREthis, &pRef, false))
            resp->returnval = pRef->getInterfaceName();

    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (FAILED(rc))
        return SOAP_FAULT;
    return SOAP_OK;
}

/**
 * This is the hard-coded implementation for the IManagedObjectRef::release()
 * that our WSDL promises to our web service clients. This method releases
 * a managed object reference and removes it from our stacks.
 *
 * @param soap
 * @param req
 * @param resp
 * @return
 */
int __vbox__IManagedObjectRef_USCORErelease(
    struct soap *soap,
    _vbox__IManagedObjectRef_USCORErelease *req,
    _vbox__IManagedObjectRef_USCOREreleaseResponse *resp)
{
    HRESULT rc = S_OK;
    WEBDEBUG(("-- entering %s\n", __FUNCTION__));

    do
    {
        // findRefFromId and the delete call below require the lock
        util::AutoWriteLock lock(g_pSessionsLockHandle COMMA_LOCKVAL_SRC_POS);

        ManagedObjectRef *pRef;
        if ((rc = ManagedObjectRef::findRefFromId(req->_USCOREthis, &pRef, false)))
        {
            RaiseSoapInvalidObjectFault(soap, req->_USCOREthis);
            break;
        }

        WEBDEBUG(("   found reference; deleting!\n"));
        // this removes the object from all stacks; since
        // there's a ComPtr<> hidden inside the reference,
        // this should also invoke Release() on the COM
        // object
        delete pRef;
    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (FAILED(rc))
        return SOAP_FAULT;
    return SOAP_OK;
}

/****************************************************************************
 *
 * interface IWebsessionManager
 *
 ****************************************************************************/

/**
 * Hard-coded implementation for IWebsessionManager::logon. As opposed to the underlying
 * COM API, this is the first method that a webservice client must call before the
 * webservice will do anything useful.
 *
 * This returns a managed object reference to the global IVirtualBox object; into this
 * reference a session ID is encoded which remains constant with all managed object
 * references returned by other methods.
 *
 * This also creates an instance of ISession, which is stored internally with the
 * webservice session and can be retrieved with IWebsessionManager::getSessionObject
 * (__vbox__IWebsessionManager_USCOREgetSessionObject). In order for the
 * VirtualBox web service to do anything useful, one usually needs both a
 * VirtualBox and an ISession object, for which these two methods are designed.
 *
 * When the webservice client is done, it should call IWebsessionManager::logoff. This
 * will clean up internally (destroy all remaining managed object references and
 * related COM objects used internally).
 *
 * After logon, an internal timeout ensures that if the webservice client does not
 * call any methods, after a configurable number of seconds, the webservice will log
 * off the client automatically. This is to ensure that the webservice does not
 * drown in managed object references and eventually deny service. Still, it is
 * a much better solution, both for performance and cleanliness, for the webservice
 * client to clean up itself.
 *
 * @param
 * @param vbox__IWebsessionManager_USCORElogon
 * @param vbox__IWebsessionManager_USCORElogonResponse
 * @return
 */
int __vbox__IWebsessionManager_USCORElogon(
        struct soap *soap,
        _vbox__IWebsessionManager_USCORElogon *req,
        _vbox__IWebsessionManager_USCORElogonResponse *resp)
{
    HRESULT rc = S_OK;
    WEBDEBUG(("-- entering %s\n", __FUNCTION__));

    do
    {
        // WebServiceSession constructor tinkers with global MOR map and requires a write lock
        util::AutoWriteLock lock(g_pSessionsLockHandle COMMA_LOCKVAL_SRC_POS);

        // create new session; the constructor stores the new session
        // in the global map automatically
        WebServiceSession *pSession = new WebServiceSession();
        ComPtr<IVirtualBox> pVirtualBox;

        // authenticate the user
        if (!(pSession->authenticate(req->username.c_str(),
                                     req->password.c_str(),
                                     pVirtualBox.asOutParam())))
        {
            // in the new session, create a managed object reference (MOR) for the
            // global VirtualBox object; this encodes the session ID in the MOR so
            // that it will be implicitly be included in all future requests of this
            // webservice client
            ComPtr<IUnknown> p2 = pVirtualBox;
            if (pVirtualBox.isNull() || p2.isNull())
            {
                rc = E_FAIL;
                break;
            }
            ManagedObjectRef *pRef = new ManagedObjectRef(*pSession,
                                                          p2,                       // IUnknown *pobjUnknown
                                                          pVirtualBox,              // void *pobjInterface
                                                          COM_IIDOF(IVirtualBox),
                                                          g_pcszIVirtualBox);
            resp->returnval = pRef->getWSDLID();
            WEBDEBUG(("VirtualBox object ref is %s\n", resp->returnval.c_str()));
        }
        else
            rc = E_FAIL;
    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (FAILED(rc))
        return SOAP_FAULT;
    return SOAP_OK;
}

/**
 * Returns the ISession object that was created for the webservice client
 * on logon.
 */
int __vbox__IWebsessionManager_USCOREgetSessionObject(
        struct soap*,
        _vbox__IWebsessionManager_USCOREgetSessionObject *req,
        _vbox__IWebsessionManager_USCOREgetSessionObjectResponse *resp)
{
    HRESULT rc = S_OK;
    WEBDEBUG(("-- entering %s\n", __FUNCTION__));

    do
    {
        // findSessionFromRef needs lock
        util::AutoWriteLock lock(g_pSessionsLockHandle COMMA_LOCKVAL_SRC_POS);

        WebServiceSession* pSession;
        if ((pSession = WebServiceSession::findSessionFromRef(req->refIVirtualBox)))
            resp->returnval = pSession->getSessionWSDLID();

    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (FAILED(rc))
        return SOAP_FAULT;
    return SOAP_OK;
}

/**
 * hard-coded implementation for IWebsessionManager::logoff.
 *
 * @param
 * @param vbox__IWebsessionManager_USCORElogon
 * @param vbox__IWebsessionManager_USCORElogonResponse
 * @return
 */
int __vbox__IWebsessionManager_USCORElogoff(
        struct soap*,
        _vbox__IWebsessionManager_USCORElogoff *req,
        _vbox__IWebsessionManager_USCORElogoffResponse *resp)
{
    HRESULT rc = S_OK;
    WEBDEBUG(("-- entering %s\n", __FUNCTION__));

    do
    {
        // findSessionFromRef and the session destructor require the lock
        util::AutoWriteLock lock(g_pSessionsLockHandle COMMA_LOCKVAL_SRC_POS);

        WebServiceSession* pSession;
        if ((pSession = WebServiceSession::findSessionFromRef(req->refIVirtualBox)))
        {
            delete pSession;
                // destructor cleans up

            WEBDEBUG(("session destroyed, %d sessions left open\n", g_mapSessions.size()));
        }
    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (FAILED(rc))
        return SOAP_FAULT;
    return SOAP_OK;
}
