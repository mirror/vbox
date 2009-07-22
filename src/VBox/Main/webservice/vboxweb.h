/*
 * vboxweb.h:
 *      header file for "real" web server code.
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

/****************************************************************************
 *
 * debug macro
 *
 ****************************************************************************/

void WebLog(const char *pszFormat, ...);

// #ifdef DEBUG
#define WEBDEBUG(a) if (g_fVerbose) { WebLog a; }
// #else
// #define WEBDEBUG(a) do { } while(0)
// #endif

/****************************************************************************
 *
 * global variables
 *
 ****************************************************************************/

extern ComPtr <IVirtualBox> G_pVirtualBox;
extern bool g_fVerbose;

extern PRTSTREAM g_pstrLog;

/****************************************************************************
 *
 * typedefs
 *
 ****************************************************************************/

// type used by gSOAP-generated code
typedef std::string WSDLT_ID;               // combined managed object ref (session ID plus object ID)
typedef std::string vbox__uuid;

// type used internally by our class
// typedef WSDLT_ID ManagedObjectID;

// #define VBOXWEB_ERROR_BASE                      10000
// #define VBOXWEB_INVALID_MANAGED_OBJECT          (VBOXWEB_ERROR_BASE + 0)
// #define VBOXWEB_INVALID_MANAGED_OBJECT_TYPE     (VBOXWEB_ERROR_BASE + 1)

/****************************************************************************
 *
 * SOAP exceptions
 *
 ****************************************************************************/

void RaiseSoapInvalidObjectFault(struct soap *soap, WSDLT_ID obj);

void RaiseSoapRuntimeFault(struct soap *soap, HRESULT apirc, IUnknown *pObj);

/****************************************************************************
 *
 * conversion helpers
 *
 ****************************************************************************/

std::string ConvertComString(const com::Bstr &bstr);

std::string ConvertComString(const com::Guid &bstr);

/****************************************************************************
 *
 * managed object reference classes
 *
 ****************************************************************************/

class WebServiceSessionPrivate;
class ManagedObjectRef;

/**
 *
 */
class WebServiceSession
{
    friend class ManagedObjectRef;

    private:
        uint64_t                    _uSessionID;
        WebServiceSessionPrivate    *_pp;
        bool                        _fDestructing;

        ManagedObjectRef            *_pISession;

        time_t                      _tLastObjectLookup;

        // hide the copy constructor because we're not copyable
        WebServiceSession(const WebServiceSession &copyFrom);

    public:
        WebServiceSession();

        ~WebServiceSession();

        int authenticate(const char *pcszUsername,
                         const char *pcszPassword);

        ManagedObjectRef* findRefFromPtr(const ComPtr<IUnknown> &pcu);

        uint64_t getID() const
        {
            return _uSessionID;
        }

        WSDLT_ID getSessionObject() const;

        void touch();

        time_t getLastObjectLookup() const
        {
            return _tLastObjectLookup;
        }

        static WebServiceSession* findSessionFromRef(const WSDLT_ID &id);

        void DumpRefs();
};

/**
 *  ManagedObjectRef is used to map COM pointers to object IDs
 *  within a session. Such object IDs are 64-bit integers.
 *
 *  When a webservice method call is invoked on an object, it
 *  has an opaque string called a "managed object reference". Such
 *  a string consists of a session ID combined with an object ID.
 *
 */
class ManagedObjectRef
{
    protected:
        // owning session:
        WebServiceSession           &_session;

        // value:
        ComPtr<IUnknown>            _pObj;
        const char                  *_pcszInterface;

        // keys:
        uint64_t                    _id;
        uintptr_t                   _ulp;

        // long ID as string
        WSDLT_ID                    _strID;

    public:
        ManagedObjectRef(WebServiceSession &session,
                         const char *pcszInterface,
                         const ComPtr<IUnknown> &obj);
        ~ManagedObjectRef();

        uint64_t getID()
        {
            return _id;
        }

        ComPtr<IUnknown> getComPtr()
        {
            return _pObj;
        }

        WSDLT_ID toWSDL() const;
        const char* getInterfaceName() const
        {
            return _pcszInterface;
        }

        static int findRefFromId(const WSDLT_ID &id,
                                 ManagedObjectRef **pRef,
                                 bool fNullAllowed);

        static ManagedObjectRef* findFromPtr(ComPtr<IUnknown> pcu);
        static ManagedObjectRef* create(const WSDLT_ID &idParent,
                                        ComPtr<IUnknown> pcu);

};

/**
 * Template function that resolves a managed object reference to a COM pointer
 * of the template class T. Gets called from tons of generated code in
 * methodmaps.cpp.
 *
 * This is a template function so that we can support ComPtr's for arbitrary
 * interfaces and automatically verify that the managed object reference on
 * the internal stack actually is of the expected interface.
 *
 * @param soap
 * @param id in: integer managed object reference, as passed in by web service client
 * @param pComPtr out: reference to COM pointer object that receives the com pointer,
 *                if SOAP_OK is returned
 * @param fNullAllowed in: if true, then this func returns a NULL COM pointer if an
 *                empty MOR is passed in (i.e. NULL pointers are allowed). If false,
 *                then this fails; this will be false when called for the "this"
 *                argument of method calls, which really shouldn't be NULL.
 * @return error code or SOAP_OK if no error
 */
template <class T>
int findComPtrFromId(struct soap *soap,
                     const WSDLT_ID &id,
                     ComPtr<T> &pComPtr,
                     bool fNullAllowed)
{
    int rc;
    ManagedObjectRef *pRef;
    if ((rc = ManagedObjectRef::findRefFromId(id, &pRef, fNullAllowed)))
        RaiseSoapInvalidObjectFault(soap, id);
    else
    {
        if (fNullAllowed && pRef == NULL)
        {
          pComPtr.setNull();
          return 0;
        }

        // pRef->getComPtr returns a ComPtr<IUnknown>; by casting it to
        // ComPtr<T>, we implicitly do a queryInterface() call
        if (pComPtr = pRef->getComPtr())
            return 0;

        WEBDEBUG(("    Interface not supported for object reference %s, which is of class %s\n", id.c_str(), pRef->getInterfaceName()));
        rc = VERR_WEB_UNSUPPORTED_INTERFACE;
        RaiseSoapInvalidObjectFault(soap, id);      // @todo better message
    }

    return rc;
}

/**
 * Template function that creates a new managed object for the given COM
 * pointer of the template class T. If a reference already exists for the
 * given pointer, then that reference's ID is returned instead.
 *
 * @param idParent managed object reference of calling object; used to extract session ID
 * @param pc COM object for which to create a reference
 * @return existing or new managed object reference
 */
template <class T>
WSDLT_ID createOrFindRefFromComPtr(const WSDLT_ID &idParent,
                                   const char *pcszInterface,
                                   const ComPtr<T> &pc)
{
    // NULL comptr should return NULL MOR
    if (pc.isNull())
    {
        WEBDEBUG(("   createOrFindRefFromComPtr(): returning empty MOR for NULL COM pointer\n"));
        return "";
    }

    WebServiceSession* pSession;
    if ((pSession = WebServiceSession::findSessionFromRef(idParent)))
    {
        // WEBDEBUG(("\n-- found session for %s\n", idParent.c_str()));
        ManagedObjectRef *pRef;
        if (    ((pRef = pSession->findRefFromPtr(pc)))
             || ((pRef = new ManagedObjectRef(*pSession, pcszInterface, pc)))
           )
            return pRef->toWSDL();
    }

    return 0;
}

