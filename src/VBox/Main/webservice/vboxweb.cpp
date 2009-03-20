/**
 * vboxweb.cpp:
 *      hand-coded parts of the webservice server. This is linked with the
 *      generated code in out/.../src/VBox/Main/webservice/methodmaps.cpp
 *      (and static gSOAP server code) to implement the actual webservice
 *      server, to which clients can connect.
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

// vbox headers
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>
#include <VBox/com/EventQueue.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/err.h>
#include <VBox/VRDPAuth.h>
#include <VBox/version.h>
#include <VBox/log.h>

#include <iprt/lock.h>
#include <iprt/rand.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/ctype.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/ldr.h>

// workaround for compile problems on gcc 4.1
#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif

// gSOAP headers (must come after vbox includes because it checks for conflicting defs)
#include "soapH.h"

// standard headers
#include <map>
#include <sstream>

#ifdef __GNUC__
#pragma GCC visibility pop
#endif

// shared webservice header
#include "vboxweb.h"

// include generated namespaces table
#include "vboxwebsrv.nsmap"

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

ComPtr<IVirtualBox>     g_pVirtualBox = NULL;

// generated strings in methodmaps.cpp
extern const char       *g_pcszISession,
                        *g_pcszIVirtualBox;

#define DEFAULT_TIMEOUT_SECS 300
#define DEFAULT_TIMEOUT_SECS_STRING "300"

int                     g_iWatchdogTimeoutSecs = DEFAULT_TIMEOUT_SECS;
int                     g_iWatchdogCheckInterval = 5;

const char              *g_pcszBindToHost = NULL;        // host; NULL = current machine
unsigned int            g_uBindToPort = 18083;          // port
unsigned int            g_uBacklog = 100;               // backlog = max queue size for requests

bool                    g_fVerbose = false;             // be verbose
PRTSTREAM               g_pstrLog = NULL;

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
bool                    g_fDaemonize = false;           // run in background.
#endif

/****************************************************************************
 *
 * Writeable global variables
 *
 ****************************************************************************/

RTLockMtx           g_mutexAuthLib;

// this mutex protects all of the below
RTLockMtx           g_mutexSessions;

SessionsMap         g_mapSessions;
ULONG64             g_iMaxManagedObjectID = 0;
ULONG64             g_cManagedObjects = 0;

/****************************************************************************
 *
 *  main
 *
 ****************************************************************************/

static const RTGETOPTDEF g_aOptions[]
    = {
        { "--help",             'h', RTGETOPT_REQ_NOTHING },
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
        { "--background",       'b', RTGETOPT_REQ_NOTHING },
#endif
        { "--host",             'H', RTGETOPT_REQ_STRING },
        { "--port",             'p', RTGETOPT_REQ_UINT32 },
        { "--timeout",          't', RTGETOPT_REQ_UINT32 },
        { "--check-interval",   'i', RTGETOPT_REQ_UINT32 },
        { "--verbose",          'v', RTGETOPT_REQ_NOTHING },
        { "--logfile",          'F', RTGETOPT_REQ_STRING },
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

            case 't':
                pcszDescr = "Session timeout in seconds; 0 = disable timeouts (" DEFAULT_TIMEOUT_SECS_STRING ").";
            break;

            case 'i':
                pcszDescr = "Frequency of timeout checks in seconds (5).";
            break;

            case 'v':
                pcszDescr = "Be verbose.";
            break;

            case 'F':
                pcszDescr = "Name of file to write log to (no file).";
            break;
        }

        RTStrmPrintf(g_pStdErr, "%-23s%s\n", str.c_str(), pcszDescr);
    }
}

void WebLog(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    RTPrintfV(pszFormat, args);
    va_end(args);

    if (g_pstrLog)
    {
        va_list args2;
        va_start(args2, pszFormat);
        RTStrmPrintfV(g_pstrLog, pszFormat, args);
        va_end(args2);

        RTStrmFlush(g_pstrLog);
    }
}

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


/**
 * Start up the webservice server. This keeps running and waits
 * for incoming SOAP connections.
 *
 * @param argc
 * @param argv[]
 * @return
 */
int main(int argc, char* argv[])
{
    int rc;

    // intialize runtime
    RTR3Init();

    RTStrmPrintf(g_pStdErr, "Sun xVM VirtualBox Webservice Version %s\n"
                            "(C) 2005-2009 Sun Microsystems, Inc.\n"
                            "All rights reserved.\n", VBOX_VERSION_STRING);

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aOptions, RT_ELEMENTS(g_aOptions), 1, 0 /* fFlags */);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'H':
                g_pcszBindToHost = ValueUnion.psz;
            break;

            case 'p':
                g_uBindToPort = ValueUnion.u32;
            break;

            case 't':
                g_iWatchdogTimeoutSecs = ValueUnion.u32;
            break;

            case 'i':
                g_iWatchdogCheckInterval = ValueUnion.u32;
            break;

            case 'F':
            {
                int rc2 = RTStrmOpen(ValueUnion.psz, "a", &g_pstrLog);
                if (rc2)
                {
                    RTPrintf("Error: Cannot open log file \"%s\" for writing, error %d.\n", ValueUnion.psz, rc2);
                    exit(2);
                }

                WebLog("Sun xVM VirtualBox Webservice Version %s\n"
                       "Opened log file \"%s\"\n", VBOX_VERSION_STRING, ValueUnion.psz);
            }
            break;

            case 'h':
                DisplayHelp();
                exit(0);
            break;

            case 'v':
                g_fVerbose = true;
            break;

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
            case 'b':
                g_fDaemonize = true;
            break;
#endif
            case VINF_GETOPT_NOT_OPTION:
                RTStrmPrintf(g_pStdErr, "unhandled parameter: %s\n", ValueUnion.psz);
            return 1;

            default:
                if (c > 0)
                {
                    if (RT_C_IS_GRAPH(c))
                        RTStrmPrintf(g_pStdErr, "unhandled option: -%c", c);
                    else
                        RTStrmPrintf(g_pStdErr, "unhandled option: %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    RTStrmPrintf(g_pStdErr, "unknown option: %s", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    RTStrmPrintf(g_pStdErr, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    RTStrmPrintf(g_pStdErr, "%Rrs", c);
                exit(1);
            break;
        }
    }

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
    if (g_fDaemonize)
    {
        rc = RTProcDaemonize(false /* fNoChDir */, false /* fNoClose */,
                             NULL);
        if (RT_FAILURE(rc))
        {
            RTStrmPrintf(g_pStdErr, "vboxwebsrv: failed to daemonize, rc=%Rrc. exiting.\n", rc);
            exit(1);
        }
    }
#endif

    // intialize COM/XPCOM
    rc = com::Initialize();
    if (FAILED(rc))
    {
        RTPrintf("ERROR: failed to initialize COM!\n");
        return rc;
    }

    ComPtr<ISession> session;

    rc = g_pVirtualBox.createLocalObject(CLSID_VirtualBox);
    if (FAILED(rc))
        RTPrintf("ERROR: failed to create the VirtualBox object!\n");
    else
    {
        rc = session.createInprocObject(CLSID_Session);
        if (FAILED(rc))
            RTPrintf("ERROR: failed to create a session object!\n");
    }

    if (FAILED(rc))
    {
        com::ErrorInfo info;
        if (!info.isFullAvailable() && !info.isBasicAvailable())
        {
            com::GluePrintRCMessage(rc);
            RTPrintf("Most likely, the VirtualBox COM server is not running or failed to start.\n");
        }
        else
            com::GluePrintErrorInfo(info);
        return rc;
    }

    if (g_iWatchdogTimeoutSecs > 0)
    {
        // start our watchdog thread
        RTTHREAD  tWatchdog;
        if (RTThreadCreate(&tWatchdog,
                           fntWatchdog,
                           NULL,
                           32*1024,
                           RTTHREADTYPE_MAIN_WORKER,
                           0,
                           "Watchdog"))
        {
            RTStrmPrintf(g_pStdErr, "[!] Cannot start watchdog thread\n");
            exit(1);
        }
    }

    // set up gSOAP
    struct soap soap;
    soap_init(&soap);

    soap.bind_flags |= SO_REUSEADDR;
            // avoid EADDRINUSE on bind()

    int m, s; // master and slave sockets
    m = soap_bind(&soap,
                  g_pcszBindToHost,     // host: current machine
                  g_uBindToPort,     // port
                  g_uBacklog);     // backlog = max queue size for requests
    if (m < 0)
        WebLogSoapError(&soap);
    else
    {
        WebLog("Socket connection successful: host = %s, port = %u, master socket = %d\n",
               (g_pcszBindToHost) ? g_pcszBindToHost : "default (localhost)",
               g_uBindToPort,
               m);

        for (unsigned long long i = 1;
             ;
             i++)
        {
            s = soap_accept(&soap);
            if (s < 0)
            {
                WebLogSoapError(&soap);
                break;
            }

            WebLog("%llu: accepted connection from IP=%lu.%lu.%lu.%lu socket=%d... ",
                   i,
                   (soap.ip>>24)&0xFF,
                   (soap.ip>>16)&0xFF,
                   (soap.ip>>8)&0xFF,
                   soap.ip&0xFF,
                   s);

            // enclose the entire RPC call in the sessions lock
            // so that the watchdog cannot destroy COM objects
            // while the RPC is ongoing
            RTLock lock(g_mutexSessions);
            // now process the RPC request (this goes into the
            // generated code in methodmaps.cpp with all the COM calls)
            if (soap_serve(&soap) != SOAP_OK)
            {
                WebLogSoapError(&soap);
            }
            lock.release();

            WebLog("Request served\n");

            soap_destroy(&soap); // clean up class instances
            soap_end(&soap); // clean up everything and close socket
        }
    }
    soap_done(&soap); // close master socket and detach environment

    com::Shutdown();
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
    WEBDEBUG(("Watchdog thread started\n"));

    while (1)
    {
        WEBDEBUG(("Watchdog: sleeping %d seconds\n", g_iWatchdogCheckInterval));
        RTThreadSleep(g_iWatchdogCheckInterval*1000);

        time_t                      tNow;
        time(&tNow);

        RTLock lock(g_mutexSessions);
        WEBDEBUG(("Watchdog: checking %d sessions\n", g_mapSessions.size()));

        SessionsMap::iterator
            it = g_mapSessions.begin(),
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
        lock.release();
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
                    const std::string &str,
                    int extype,
                    void *ex)
{
    // raise the fault
    soap_sender_fault(soap, str.c_str(), NULL);

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

    /* std::ostringstream ostr;
    ostr << std::hex << ex->badObjectID; */

    std::string str("VirtualBox error: ");
    str += "Invalid managed object reference \"" + obj + "\"";

    RaiseSoapFault(soap,
                   str,
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
    const char *pcsz;
    if ((pcsz = ustr.raw()))
        return pcsz;
    return "";
}

/**
 * Return a safe C++ string from the given COM UUID,
 * without crashing if the UUID is empty.
 * @param bstr
 * @return
 */
std::string ConvertComString(const com::Guid &bstr)
{
    com::Utf8Str ustr(bstr);
    const char *pcsz;
    if ((pcsz = ustr.raw()))
        return pcsz;
    return "";
}

/**
 * Raises a SOAP runtime fault.
 *
 * @param pObj
 */
void RaiseSoapRuntimeFault(struct soap *soap,
                           HRESULT apirc,
                           IUnknown *pObj)
{
    com::ErrorInfo info(pObj);

    WEBDEBUG(("   error, raising SOAP exception\n"));

    RTStrmPrintf(g_pStdErr, "API return code:            0x%08X (%Rhrc)\n", apirc, apirc);
    RTStrmPrintf(g_pStdErr, "COM error info result code: 0x%lX\n", info.getResultCode());
    RTStrmPrintf(g_pStdErr, "COM error info text:        %ls\n", info.getText().raw());

    // allocated our own soap fault struct
    _vbox__RuntimeFault *ex = soap_new__vbox__RuntimeFault(soap, 1);
    ex->resultCode = info.getResultCode();
    ex->text = ConvertComString(info.getText());
    ex->component = ConvertComString(info.getComponent());
    ex->interfaceID = ConvertComString(info.getInterfaceID());

    // compose descriptive message
    std::ostringstream ostr;
    ostr << std::hex << ex->resultCode;

    std::string str("VirtualBox error: ");
    str += ex->text;
    str += " (0x";
    str += ostr.str();
    str += ")";

    RaiseSoapFault(soap,
                   str,
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
 * Preconditions: Caller must have locked g_mutexSessions.
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
    g_mapSessions[_uSessionID] = this;
}

/**
 * Destructor. Cleans up and destroys all contained managed object references on the way.
 *
 * Preconditions: Caller must have locked g_mutexSessions.
 */
WebServiceSession::~WebServiceSession()
{
    // delete us from global map first so we can't be found
    // any more while we're cleaning up
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

    ManagedObjectsMapById::iterator
        it,
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
 *  Authenticate the username and password against an authentification authority.
 *
 *  @return 0 if the user was successfully authenticated, or an error code
 *  otherwise.
 */

int WebServiceSession::authenticate(const char *pcszUsername,
                                    const char *pcszPassword)
{
    int rc = VERR_WEB_NOT_AUTHENTICATED;

    RTLock lock(g_mutexAuthLib);

    static bool fAuthLibLoaded = false;
    static PVRDPAUTHENTRY pfnAuthEntry = NULL;
    static PVRDPAUTHENTRY2 pfnAuthEntry2 = NULL;

    if (!fAuthLibLoaded)
    {
        // retrieve authentication library from system properties
        ComPtr<ISystemProperties> systemProperties;
        g_pVirtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());

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
                rc = RTLdrLoad(filename.raw(), &hlibAuth);
                if (RT_FAILURE(rc))
                {
                    WEBDEBUG(("%s() Failed to load external authentication library. Error code: %Rrc\n", __FUNCTION__, rc));
                    break;
                }

                if (RT_FAILURE(rc = RTLdrGetSymbol(hlibAuth, "VRDPAuth2", (void**)&pfnAuthEntry2)))
                    WEBDEBUG(("%s(): Could not resolve import '%s'. Error code: %Rrc\n", __FUNCTION__, "VRDPAuth2", rc));

                if (RT_FAILURE(rc = RTLdrGetSymbol(hlibAuth, "VRDPAuth", (void**)&pfnAuthEntry)))
                    WEBDEBUG(("%s(): Could not resolve import '%s'. Error code: %Rrc\n", __FUNCTION__, "VRDPAuth", rc));

                if (pfnAuthEntry || pfnAuthEntry2)
                    fAuthLibLoaded = true;

            } while (0);
        }
    }

    rc = VERR_WEB_NOT_AUTHENTICATED;
    VRDPAuthResult result;
    if (pfnAuthEntry2)
    {
        result = pfnAuthEntry2(NULL, VRDPAuthGuestNotAsked, pcszUsername, pcszPassword, NULL, true, 0);
        WEBDEBUG(("%s(): result of VRDPAuth2(): %d\n", __FUNCTION__, result));
        if (result == VRDPAuthAccessGranted)
            rc = 0;
    }
    else if (pfnAuthEntry)
    {
        result = pfnAuthEntry(NULL, VRDPAuthGuestNotAsked, pcszUsername, pcszPassword, NULL);
        WEBDEBUG(("%s(): result of VRDPAuth(%s, [%d]): %d\n", __FUNCTION__, pcszUsername, strlen(pcszPassword), result));
        if (result == VRDPAuthAccessGranted)
            rc = 0;
    }
    else if (fAuthLibLoaded)
        // fAuthLibLoaded = true but both pointers are NULL:
        // then the authlib was "null" and auth was disabled
        rc = 0;
    else
    {
        WEBDEBUG(("Could not resolve VRDPAuth2 or VRDPAuth entry point"));
    }

    if (!rc)
    {
        do
        {
            // now create the ISession object that this webservice session can use
            // (and of which IWebsessionManager::getSessionObject returns a managed object reference)
            ComPtr<ISession> session;
            if (FAILED(rc = session.createInprocObject(CLSID_Session)))
            {
                WEBDEBUG(("ERROR: cannot create session object!"));
                break;
            }

            _pISession = new ManagedObjectRef(*this, g_pcszISession, session);

            if (g_fVerbose)
            {
                ISession *p = session;
                std::string strMOR = _pISession->toWSDL();
                WEBDEBUG(("   * %s: created session object with comptr 0x%lX, MOR = %s\n", __FUNCTION__, p, strMOR.c_str()));
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
 *  Preconditions: Caller must have locked g_mutexSessions.
 *
 * @param pcu pointer to a COM object.
 * @return The existing ManagedObjectRef that represents the COM object, or NULL if there's none yet.
 */
ManagedObjectRef* WebServiceSession::findRefFromPtr(const ComPtr<IUnknown> &pcu)
{
    IUnknown *p = pcu;
    uintptr_t ulp = (uintptr_t)p;
    ManagedObjectRef *pRef;
    // WEBDEBUG(("   %s: looking up 0x%lX\n", __FUNCTION__, ulp));
    ManagedObjectsMapByPtr::iterator it = _pp->_mapManagedObjectsByPtr.find(ulp);
    if (it != _pp->_mapManagedObjectsByPtr.end())
    {
        pRef = it->second;
        WSDLT_ID id = pRef->toWSDL();
        WEBDEBUG(("   %s: found existing ref %s for COM obj 0x%lX\n", __FUNCTION__, id.c_str(), ulp));
    }
    else
        pRef = NULL;
    return pRef;
}

/**
 * Static method which attempts to find the session for which the given managed
 * object reference was created, by splitting the reference into the session and
 * object IDs and then looking up the session object for that session ID.
 *
 * Preconditions: Caller must have locked g_mutexSessions.
 *
 * @param id Managed object reference (with combined session and object IDs).
 * @return
 */
WebServiceSession* WebServiceSession::findSessionFromRef(const WSDLT_ID &id)
{
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
WSDLT_ID WebServiceSession::getSessionObject() const
{
    return _pISession->toWSDL();
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

/**
 *
 */
void WebServiceSession::DumpRefs()
{
    WEBDEBUG(("   dumping object refs:\n"));
    ManagedObjectsIteratorById
        iter = _pp->_mapManagedObjectsById.begin(),
        end = _pp->_mapManagedObjectsById.end();
    for (;
        iter != end;
        ++iter)
    {
        ManagedObjectRef *pRef = iter->second;
        uint64_t id = pRef->getID();
        void *p = pRef->getComPtr();
        WEBDEBUG(("     objid %llX: comptr 0x%lX\n", id, p));
    }
}

/****************************************************************************
 *
 *  class ManagedObjectRef
 *
 ****************************************************************************/

/**
 *  Constructor, which assigns a unique ID to this managed object
 *  reference and stores it two global hashs:
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
 *  This does _not_ check whether another instance already
 *  exists in the hash. This gets called only from the
 *  createRefFromObject() template function in vboxweb.h, which
 *  does perform that check.
 *
 *  Preconditions: Caller must have locked g_mutexSessions.
 *
 * @param pObj
 */
ManagedObjectRef::ManagedObjectRef(WebServiceSession &session,
                                   const char *pcszInterface,
                                   const ComPtr<IUnknown> &pc)
    : _session(session),
      _pObj(pc),
      _pcszInterface(pcszInterface)
{
    ComPtr<IUnknown> pcUnknown(pc);
    _ulp = (uintptr_t)(IUnknown*)pcUnknown;

    _id = ++g_iMaxManagedObjectID;
    // and count globally
    ULONG64 cTotal = ++g_cManagedObjects;           // raise global count and make a copy for the debug message below

    char sz[34];
    MakeManagedObjectRef(sz, session._uSessionID, _id);
    _strID = sz;

    session._pp->_mapManagedObjectsById[_id] = this;
    session._pp->_mapManagedObjectsByPtr[_ulp] = this;

    session.touch();

    WEBDEBUG(("   * %s: MOR created for ulp 0x%lX (%s), new ID is %llX; now %lld objects total\n", __FUNCTION__, _ulp, pcszInterface, _id, cTotal));
}

/**
 * Destructor; removes the instance from the global hash of
 * managed objects.
 *
 * Preconditions: Caller must have locked g_mutexSessions.
 */
ManagedObjectRef::~ManagedObjectRef()
{
    ULONG64 cTotal = --g_cManagedObjects;

    WEBDEBUG(("   * %s: deleting MOR for ID %llX (%s); now %lld objects total\n", __FUNCTION__, _id, _pcszInterface, cTotal));

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
 * Converts the ID of this managed object reference to string
 * form, for returning with SOAP data or similar.
 *
 * @return The ID in string form.
 */
WSDLT_ID ManagedObjectRef::toWSDL() const
{
    return _strID;
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
                                    ManagedObjectRef **pRef)
{
    int rc = 0;

    do
    {
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

        WEBDEBUG(("   %s(): sessid %llX, objid %llX\n", __FUNCTION__, sessid, objid));
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
    HRESULT rc = SOAP_OK;
    WEBDEBUG(("\n-- entering %s\n", __FUNCTION__));

    do {
        ManagedObjectRef *pRef;
        if (!ManagedObjectRef::findRefFromId(req->_USCOREthis, &pRef))
            resp->returnval = pRef->getInterfaceName();

    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (rc)
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
    HRESULT rc = SOAP_OK;
    WEBDEBUG(("\n-- entering %s\n", __FUNCTION__));

    do {
        ManagedObjectRef *pRef;
        if ((rc = ManagedObjectRef::findRefFromId(req->_USCOREthis, &pRef)))
        {
            RaiseSoapInvalidObjectFault(soap, req->_USCOREthis);
            break;
        }

        WEBDEBUG(("   found reference; deleting!\n"));
        delete pRef;
            // this removes the object from all stacks; since
            // there's a ComPtr<> hidden inside the reference,
            // this should also invoke Release() on the COM
            // object
    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (rc)
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
 * Preconditions: Caller must have locked g_mutexSessions.
 * Since this gets called from main() like other SOAP method
 * implementations, this is ensured.
 *
 * @param
 * @param vbox__IWebsessionManager_USCORElogon
 * @param vbox__IWebsessionManager_USCORElogonResponse
 * @return
 */
int __vbox__IWebsessionManager_USCORElogon(
        struct soap*,
        _vbox__IWebsessionManager_USCORElogon *req,
        _vbox__IWebsessionManager_USCORElogonResponse *resp)
{
    HRESULT rc = SOAP_OK;
    WEBDEBUG(("\n-- entering %s\n", __FUNCTION__));

    do {
        // create new session; the constructor stores the new session
        // in the global map automatically
        WebServiceSession *pSession = new WebServiceSession();

        // authenticate the user
        if (!(pSession->authenticate(req->username.c_str(),
                                     req->password.c_str())))
        {
            // in the new session, create a managed object reference (moref) for the
            // global VirtualBox object; this encodes the session ID in the moref so
            // that it will be implicitly be included in all future requests of this
            // webservice client
            ManagedObjectRef *pRef = new ManagedObjectRef(*pSession, g_pcszIVirtualBox, g_pVirtualBox);
            resp->returnval = pRef->toWSDL();
            WEBDEBUG(("VirtualBox object ref is %s\n", resp->returnval.c_str()));
        }
    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (rc)
        return SOAP_FAULT;
    return SOAP_OK;
}

/**
 * Returns the ISession object that was created for the webservice client
 * on logon.
 *
 * Preconditions: Caller must have locked g_mutexSessions.
 * Since this gets called from main() like other SOAP method
 * implementations, this is ensured.
 */
int __vbox__IWebsessionManager_USCOREgetSessionObject(
        struct soap*,
        _vbox__IWebsessionManager_USCOREgetSessionObject *req,
        _vbox__IWebsessionManager_USCOREgetSessionObjectResponse *resp)
{
    HRESULT rc = SOAP_OK;
    WEBDEBUG(("\n-- entering %s\n", __FUNCTION__));

    do {
        WebServiceSession* pSession;
        if ((pSession = WebServiceSession::findSessionFromRef(req->refIVirtualBox)))
        {
            resp->returnval = pSession->getSessionObject();
        }
    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (rc)
        return SOAP_FAULT;
    return SOAP_OK;
}

/**
 * hard-coded implementation for IWebsessionManager::logoff.
 *
 * Preconditions: Caller must have locked g_mutexSessions.
 * Since this gets called from main() like other SOAP method
 * implementations, this is ensured.
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
    HRESULT rc = SOAP_OK;
    WEBDEBUG(("\n-- entering %s\n", __FUNCTION__));

    do {
        WebServiceSession* pSession;
        if ((pSession = WebServiceSession::findSessionFromRef(req->refIVirtualBox)))
        {
            delete pSession;
                // destructor cleans up

            WEBDEBUG(("session destroyed, %d sessions left open\n", g_mapSessions.size()));
        }
    } while (0);

    WEBDEBUG(("-- leaving %s, rc: 0x%lX\n", __FUNCTION__, rc));
    if (rc)
        return SOAP_FAULT;
    return SOAP_OK;
}
