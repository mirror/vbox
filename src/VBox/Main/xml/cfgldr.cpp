/** @file
 *
 * CFGLDR - Configuration Loader
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Define STANDALONE_TEST for making a executable that
 * will load and parse a XML file
 */
// #define STANDALONE_TEST

/** @page pg_cfgldr       CFGLDR - The Configuration Loader
 *
 * The configuration loader loads and keeps the configuration of VBox
 * session. It will be used by VBox components to retrieve configuration
 * values at startup and to change values if necessary.
 *
 * When VBox is started, it must call CFGLDRLoad to load global
 * VBox configuration and then VM configuration.
 *
 * Handles then used by VBox components to retrieve configuration
 * values. Components must query their values at startup.
 * Configuration values can be changed only by respective
 * component and the component must call CFGLDRSet*, to change the
 * value in configuration file.
 *
 * Values are accessed only by name. It is important for components to
 * use CFGLDRQuery* only at startup for performance reason.
 *
 * All CFGLDR* functions that take a CFGHANDLE or CFGNODE as their first
 * argument return the VERR_INVALID_HANDLE status code if the argument is
 * null handle (i.e. its value is zero).
 */

#define VBOX_XML_WRITER_FILTER
#define VBOX_XML_XSLT
#define _CRT_SECURE_NO_DEPRECATE

#define LOG_GROUP LOG_GROUP_MAIN
#include <VBox/log.h>

#include <VBox/err.h>

#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/path.h>
#include <iprt/file.h>
#include <iprt/time.h>
#include <iprt/alloc.h>

#include <xercesc/util/PlatformUtils.hpp>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMBuilder.hpp>
#include <xercesc/dom/DOMWriter.hpp>

#include <xercesc/framework/LocalFileFormatTarget.hpp>

#include <xercesc/util/XMLUni.hpp>
#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/util/BinInputStream.hpp>
#include <xercesc/util/BinMemInputStream.hpp>

#ifdef VBOX_XML_WRITER_FILTER
#include <xercesc/dom/DOMWriterFilter.hpp>
#endif

#ifdef VBOX_XML_XSLT

#include <xalanc/Include/PlatformDefinitions.hpp>
#include <xalanc/XalanTransformer/XalanTransformer.hpp>
#include <xalanc/XalanTransformer/XercesDOMWrapperParsedSource.hpp>
#include <xalanc/XercesParserLiaison/XercesParserLiaison.hpp>
#include <xalanc/XercesParserLiaison/XercesDOMSupport.hpp>
#include <xalanc/XercesParserLiaison/FormatterToXercesDOM.hpp>
#include <xalanc/XSLT/ProblemListener.hpp>

XALAN_CPP_NAMESPACE_USE

// generated file
#include "SettingsConverter_xsl.h"

#endif // VBOX_XML_XSLT

// Little hack so we don't have to include the COM stuff
// Note that those defines need to be made before we include
// cfgldr.h so that the prototypes can be compiled.
#ifndef BSTR
#define OLECHAR wchar_t
#define BSTR void*
#if defined(RT_OS_WINDOWS)
extern "C" BSTR __stdcall SysAllocString(const OLECHAR* sz);
#else
extern "C" BSTR SysAllocString(const OLECHAR* sz);
#endif
#endif

#include "Logging.h"

#include <VBox/cfgldr.h>

#include <string.h>
#include <stdio.h> // for sscanf

#ifdef STANDALONE_TEST
# include <stdlib.h>
# include <iprt/runtime.h>
#endif

XERCES_CPP_NAMESPACE_USE

// Helpers
////////////////////////////////////////////////////////////////////////////////

inline unsigned char fromhex (RTUTF16 hexdigit)
{
    if (hexdigit >= '0' && hexdigit <= '9')
        return hexdigit - '0';
    if (hexdigit >= 'A' && hexdigit <= 'F')
        return hexdigit - 'A' + 0xA;
    if (hexdigit >= 'a' && hexdigit <= 'f')
        return hexdigit - 'a' + 0xa;

    return 0xFF; // error indicator
}

inline RTUTF16 tohex (unsigned char ch)
{
    return (ch < 0xA) ? ch + '0' : ch - 0xA + 'A';
}

/**
 * Converts a string of hex digits to memory bytes.
 *
 * @param puszValue String of hex digits to convert.
 * @param pvValue   Where to store converted bytes.
 * @param cbValue   Size of the @a pvValue array.
 * @param pcbValue  Where to store the actual number of stored bytes.
 *
 * @return IPRT status code.
 */
int wstr_to_bin (PCRTUTF16 puszValue, void *pvValue, unsigned cbValue, unsigned *pcbValue)
{
    int rc = VINF_SUCCESS;

    unsigned count = 0;
    unsigned char *dst = (unsigned char *) pvValue;

    while (*puszValue)
    {
        unsigned char b = fromhex (*puszValue);

        if (b == 0xFF)
        {
            /* it was not a valid hex digit */
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }

        if (count < cbValue)
        {
            *dst = b;
        }

        puszValue++;

        if (!*puszValue)
        {
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }

        b = fromhex (*puszValue++);

        if (b == 0xFF)
        {
            /* it was not a valid hex digit */
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }

        if (count < cbValue)
        {
            *dst = ((*dst) << 4) + b;
            dst++;
        }

        count++;
    }

    *pcbValue = count;

    return rc;
}

/**
 * Converts memory bytes to a null-terminated string of hex values.
 *
 * @param pvValue   Memory array to convert.
 * @param cbValue   Number of bytes in the @a pvValue array.
 * @param puszValue Where to store the pointer to the resulting string.
 *                  On success, this string should be freed using RTUtf16Free().
 *
 * @return IPRT status code.
 */
static int bin_to_wstr (const void *pvValue, unsigned cbValue, PRTUTF16 *puszValue)
{
    int rc = VINF_SUCCESS;

    /* each byte will produce two hex digits and there will be nul
     * terminator */
    *puszValue = (PRTUTF16) RTMemTmpAlloc (sizeof (RTUTF16) * (cbValue * 2 + 1));

    if (!*puszValue)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        unsigned         i = 0;
        unsigned char *src = (unsigned char *) pvValue;
        PRTUTF16 dst       = *puszValue;

        for (; i < cbValue; i++, src++)
        {
            *dst++ = tohex ((*src) >> 4);
            *dst++ = tohex ((*src) & 0xF);
        }

        *dst = '\0';
    }

    return rc;
}

// CfgNode
////////////////////////////////////////////////////////////////////////////////

class CfgNode
{
    private:
    friend class CfgLoader;
        CfgLoader *pConfiguration;
        CfgNode *next;
        CfgNode *prev;

        DOMNode *pdomnode;

        CfgNode (CfgLoader *pcfg);
        virtual ~CfgNode ();

        int resolve (DOMNode *root, const char *pszName, unsigned uIndex, unsigned flags);

        int getValueString (const char *pszName, PRTUTF16 *ppwszValue);
        int setValueString (const char *pszName, PRTUTF16 pwszValue);

        DOMNode *findChildText (void);

    public:

        enum {
            fSearch = 0,
            fCreateIfNotExists = 1,
            fAppend = 2
        };

        static int ReleaseNode (CfgNode *pnode);
        static int DeleteNode (CfgNode *pnode);

        int CreateChildNode (const char *pszName, CfgNode **ppnode);
        int AppendChildNode (const char *pszName, CfgNode **ppnode);

        int GetChild (const char *pszName, unsigned uIndex, CfgNode **ppnode);
        int CountChildren (const char *pszChildName, unsigned *pCount);


        int QueryUInt32 (const char *pszName, uint32_t *pulValue);
        int SetUInt32   (const char *pszName, uint32_t ulValue, unsigned int uiBase = 0);
        int QueryUInt64 (const char *pszName, uint64_t *pullValue);
        int SetUInt64   (const char *pszName, uint64_t ullValue, unsigned int uiBase = 0);

        int QueryInt32  (const char *pszName, int32_t *plValue);
        int SetInt32    (const char *pszName, int32_t lValue, unsigned int uiBase = 0);
        int QueryInt64  (const char *pszName, int64_t *pllValue);
        int SetInt64    (const char *pszName, int64_t llValue, unsigned int uiBase = 0);

        int QueryUInt16 (const char *pszName, uint16_t *puhValue);
        int SetUInt16   (const char *pszName, uint16_t uhValue, unsigned int uiBase = 0);

        int QueryBin    (const char *pszName, void *pvValue, unsigned cbValue, unsigned *pcbValue);
        int SetBin      (const char *pszName, const void *pvValue, unsigned cbValue);
        int QueryString (const char *pszName, void **pValue, unsigned cbValue, unsigned *pcbValue, bool returnUtf16);
        int SetString   (const char *pszName, const char *pszValue, unsigned cbValue, bool isUtf16);

        int QueryBool   (const char *pszName, bool *pfValue);
        int SetBool     (const char *pszName, bool fValue);

        int DeleteAttribute (const char *pszName);
};

// CfgLoader
////////////////////////////////////////////////////////////////////////////////

class CfgLoader
{
    private:

        friend class CfgNode;

        PRTUTF16 pwszOriginalFilename;
        RTFILE hOriginalFileHandle;     /** r=bird: this is supposed to mean 'handle to the orignal file handle'? The name is
                                         * overloaded with too many 'handles'... That goes for those hFileHandle parameters too.
                                         * hOriginalFile / FileOriginal and hFile / File should do by a long way. */
        CfgNode *pfirstnode;

        DOMBuilder *builder;
        DOMNode *root;

        int getNode (DOMNode *prootnode, const char *pszName, unsigned uIndex, CfgNode **ppnode, unsigned flags);

        DOMDocument *Document(void) { return static_cast<DOMDocument *>(root); };

    public:

        CfgLoader ();
        virtual ~CfgLoader ();

        int Load (const char *pszFileName, RTFILE hFileHandle,
                  const char *pszExternalSchemaLocation, bool bDoNamespaces,
                  PFNCFGLDRENTITYRESOLVER pfnEntityResolver,
                  char **ppszErrorMessage);
        int Save (const char *pszFilename, RTFILE hFileHandle,
                  char **ppszErrorMessage);
        int Create ();
#ifdef VBOX_XML_XSLT
        int Transform (const char *pszTemlateLocation,
                       PFNCFGLDRENTITYRESOLVER pfnEntityResolver,
                       char **ppszErrorMessage);
#endif

        static int FreeConfiguration (CfgLoader *pcfg);

        int CreateNode (const char *pszName, CfgNode **ppnode);

        int GetNode (const char *pszName, unsigned uIndex, CfgNode **ppnode);
};

// VBoxWriterFilter
////////////////////////////////////////////////////////////////////////////////

#ifdef VBOX_XML_WRITER_FILTER
class VBoxWriterFilter : public DOMWriterFilter
{
    public:
        VBoxWriterFilter (unsigned long whatToShow = DOMNodeFilter::SHOW_ALL);
        ~VBoxWriterFilter () {};

        virtual short acceptNode (const DOMNode*) const;
        virtual unsigned long getWhatToShow () const { return fWhatToShow; };
        virtual void setWhatToShow (unsigned long toShow) { fWhatToShow = toShow; };

    private:
        unsigned long fWhatToShow;
};

VBoxWriterFilter::VBoxWriterFilter(unsigned long whatToShow)
    :
    fWhatToShow (whatToShow)
{
}

short VBoxWriterFilter::acceptNode(const DOMNode* node) const
{
    switch (node->getNodeType())
    {
        case DOMNode::TEXT_NODE:
        {
            /* Reject empty nodes */
            const XMLCh *pxmlch = node->getNodeValue ();

            if (pxmlch)
            {
                while (*pxmlch != chNull)
                {
                    if (   *pxmlch == chLF
                        || *pxmlch == chCR
                        || *pxmlch == chSpace
                        || *pxmlch == chHTab
                       )
                    {
                        pxmlch++;
                        continue;
                    }

                    break;
                }

                if (*pxmlch != chNull)
                {
                    /* Accept the node because it contains non space characters */
                    return DOMNodeFilter::FILTER_ACCEPT;
                }
            }

            return DOMNodeFilter::FILTER_REJECT;
        }
    }

    return DOMNodeFilter::FILTER_ACCEPT;
}

#endif

// CfgLdrInputSource
////////////////////////////////////////////////////////////////////////////////

/**
 *  A wrapper around RTFILE or a char buffer that acts like a DOMInputSource
 *  and therefore can be with DOMBuilder instances.
 */
class CfgLdrInputSource : public DOMInputSource
{
public:

    CfgLdrInputSource (PCFGLDRENTITY pcEntity, const char *pcszSystemId);
    virtual ~CfgLdrInputSource() { release(); }

    // Functions introduced in DOM Level 3

    const XMLCh *getEncoding () const { return NULL; }
    const XMLCh *getPublicId () const { return NULL; }
    const XMLCh *getSystemId () const { return (const XMLCh *)m_pwszSystemId; }
    const XMLCh *getBaseURI () const { return (const XMLCh *)m_pwszBaseURI; }

    void setEncoding (const XMLCh *const /* encodingStr */)
        { AssertMsgFailed (("Not implemented!\n")); }
    void setPublicId (const XMLCh *const /* publicId */)
        { AssertMsgFailed (("Not implemented!\n")); }
    void setSystemId (const XMLCh *const /* systemId */)
        { AssertMsgFailed (("Not implemented!\n")); }
    void setBaseURI (const XMLCh *const /* baseURI */)
        { AssertMsgFailed (("Not implemented!\n")); }

    // Non-standard Extension

    BinInputStream *makeStream() const;
    void setIssueFatalErrorIfNotFound (const bool /* flag */) {}
    bool getIssueFatalErrorIfNotFound() const { return true; }
    void release();

private:

    class FileHandleInputStream : public BinInputStream
    {
    public:

        FileHandleInputStream (RTFILE hFileHandle);

        unsigned int curPos() const;
        unsigned int readBytes (XMLByte *const toFill, const unsigned int maxToRead);

    private:

        RTFILE m_hFileHandle;
        uint64_t m_cbPos;
    };

    void init (const char *pcszSystemId);

    CFGLDRENTITY m_entity;

    PRTUTF16 m_pwszSystemId;
    PRTUTF16 m_pwszBaseURI;
};

CfgLdrInputSource::CfgLdrInputSource (PCFGLDRENTITY pcEntity,
                                      const char *pcszSystemId) :
    m_pwszSystemId (NULL), m_pwszBaseURI (NULL)
{
    Assert (pcEntity && pcEntity->enmType != CFGLDRENTITYTYPE_INVALID);
    // make a copy of the entity descriptor
    m_entity = *pcEntity;

    Assert (pcszSystemId);
    int rc = RTStrToUtf16 (pcszSystemId, &m_pwszSystemId);
    AssertRC (rc);

    char *pszBaseURI = NULL;
    pszBaseURI = RTStrDup (pcszSystemId);
    Assert (pszBaseURI);
    RTPathStripFilename (pszBaseURI);

    rc = RTStrToUtf16 (pszBaseURI, &m_pwszBaseURI);
    AssertRC (rc);

    RTStrFree(pszBaseURI);
}

void CfgLdrInputSource::release()
{
    switch (m_entity.enmType)
    {
        case CFGLDRENTITYTYPE_HANDLE:
            if (m_entity.u.handle.bClose)
                RTFileClose (m_entity.u.handle.hFile);
            break;
        case CFGLDRENTITYTYPE_MEMORY:
            if (m_entity.u.memory.bFree)
                RTMemTmpFree (m_entity.u.memory.puchBuf);
            break;
        default:
            break;
    };

    m_entity.enmType = CFGLDRENTITYTYPE_INVALID;

    if (m_pwszBaseURI)
    {
        RTUtf16Free (m_pwszBaseURI);
        m_pwszBaseURI = NULL;
    }
    if (m_pwszSystemId)
    {
        RTUtf16Free (m_pwszSystemId);
        m_pwszSystemId = NULL;
    }
}

BinInputStream *CfgLdrInputSource::makeStream() const
{
    BinInputStream *stream = NULL;

    switch (m_entity.enmType)
    {
        case CFGLDRENTITYTYPE_HANDLE:
            stream = new FileHandleInputStream (m_entity.u.handle.hFile);
            break;
        case CFGLDRENTITYTYPE_MEMORY:
            // puchBuf is neither copied nor destructed by BinMemInputStream
            stream = new BinMemInputStream (m_entity.u.memory.puchBuf,
                                            m_entity.u.memory.cbBuf,
                                            BinMemInputStream::BufOpt_Reference);
            break;
        default:
            AssertMsgFailed (("Invalid resolver entity type!\n"));
            break;
    };

    return stream;
}

CfgLdrInputSource::FileHandleInputStream::FileHandleInputStream (RTFILE hFileHandle) :
    m_hFileHandle (hFileHandle),
    m_cbPos (0)
{
}

unsigned int CfgLdrInputSource::FileHandleInputStream::curPos() const
{
    AssertMsg (!(m_cbPos >> 32), ("m_cbPos exceeds 32 bits (%16Xll)\n", m_cbPos));
    return (unsigned int)m_cbPos;
}

unsigned int CfgLdrInputSource::FileHandleInputStream::readBytes (
    XMLByte *const toFill, const unsigned int maxToRead
)
{
    /// @todo (dmik) trhow the appropriate exceptions if we fail to write

    int rc;
    NOREF (rc);

    // memorize the current position
    uint64_t cbOriginalPos = RTFileTell (m_hFileHandle);
    Assert (cbOriginalPos != ~0ULL);
    // set the new position
    rc = RTFileSeek (m_hFileHandle, m_cbPos, RTFILE_SEEK_BEGIN, NULL);
    AssertRC (rc);

    // read from the file
    size_t cbRead = 0;
    rc = RTFileRead (m_hFileHandle, toFill, maxToRead, &cbRead);
    AssertRC (rc);

    // adjust the private position
    m_cbPos += cbRead;
    // restore the current position
    rc = RTFileSeek (m_hFileHandle, cbOriginalPos, RTFILE_SEEK_BEGIN, NULL);
    AssertRC (rc);

    return (unsigned int)cbRead;
}

// CfgLdrFormatTarget
////////////////////////////////////////////////////////////////////////////////

class CfgLdrFormatTarget : public XMLFormatTarget
{
public:

    CfgLdrFormatTarget (PCFGLDRENTITY pcEntity);
    ~CfgLdrFormatTarget();

    // virtual XMLFormatTarget methods
    void writeChars (const XMLByte *const toWrite, const unsigned int count,
                     XMLFormatter *const formatter);
    void flush() {}

private:

    CFGLDRENTITY m_entity;
};

CfgLdrFormatTarget::CfgLdrFormatTarget (PCFGLDRENTITY pcEntity)
{
    Assert (pcEntity && pcEntity->enmType != CFGLDRENTITYTYPE_INVALID);
    // make a copy of the entity descriptor
    m_entity = *pcEntity;

    switch (m_entity.enmType)
    {
        case CFGLDRENTITYTYPE_HANDLE:
            int rc;
            // start writting from the beginning
            rc = RTFileSeek (m_entity.u.handle.hFile, 0, RTFILE_SEEK_BEGIN, NULL);
            AssertRC (rc);
            NOREF (rc);
            break;
        case CFGLDRENTITYTYPE_MEMORY:
            AssertMsgFailed (("Unsupported entity type!\n"));
            break;
        default:
            break;
    };
}

CfgLdrFormatTarget::~CfgLdrFormatTarget()
{
    switch (m_entity.enmType)
    {
        case CFGLDRENTITYTYPE_HANDLE:
        {
            int rc;
            // truncate the file upto the size actually written
            uint64_t cbPos = RTFileTell (m_entity.u.handle.hFile);
            Assert (cbPos != ~0ULL);
            rc = RTFileSetSize (m_entity.u.handle.hFile, cbPos);
            AssertRC (rc);
            // reset the position to he beginning
            rc = RTFileSeek (m_entity.u.handle.hFile, 0, RTFILE_SEEK_BEGIN, NULL);
            AssertRC (rc);
            NOREF (rc);
            if (m_entity.u.handle.bClose)
                RTFileClose (m_entity.u.handle.hFile);
            break;
        }
        case CFGLDRENTITYTYPE_MEMORY:
            break;
        default:
            break;
    };
}

void CfgLdrFormatTarget::writeChars (const XMLByte *const toWrite,
                                     const unsigned int count,
                                     XMLFormatter *const /* formatter */)
{
    /// @todo (dmik) trhow the appropriate exceptions if we fail to write

    switch (m_entity.enmType)
    {
        case CFGLDRENTITYTYPE_HANDLE:
            int rc;
            rc = RTFileWrite (m_entity.u.handle.hFile, toWrite, count, NULL);
            AssertRC (rc);
            NOREF (rc);
            break;
        case CFGLDRENTITYTYPE_MEMORY:
            AssertMsgFailed (("Unsupported entity type!\n"));
            break;
        default:
            AssertMsgFailed (("Invalid entity type!\n"));
            break;
    };
}

// CfgLdrEntityResolver
////////////////////////////////////////////////////////////////////////////////

/**
 *  A wrapper around FNCFGLDRENTITYRESOLVER callback that acts like a
 *  DOMEntityResolver and therefore can be used with DOMBuilder instances.
 */
class CfgLdrEntityResolver : public DOMEntityResolver
{
public:

    CfgLdrEntityResolver (PFNCFGLDRENTITYRESOLVER pfnEntityResolver) :
        m_pfnEntityResolver (pfnEntityResolver) {}

    // Functions introduced in DOM Level 2
    DOMInputSource *resolveEntity (const XMLCh *const publicId,
                                   const XMLCh *const systemId,
                                   const XMLCh *const baseURI);

private:

    PFNCFGLDRENTITYRESOLVER m_pfnEntityResolver;
};

DOMInputSource *CfgLdrEntityResolver::resolveEntity (const XMLCh *const publicId,
                                                     const XMLCh *const systemId,
                                                     const XMLCh *const baseURI)
{
    if (!m_pfnEntityResolver)
        return NULL;

    DOMInputSource *source = NULL;
    int rc = VINF_SUCCESS;

    char *pszPublicId = NULL;
    char *pszSystemId = NULL;
    char *pszBaseURI = NULL;

    if (publicId)
        rc = RTUtf16ToUtf8 (publicId, &pszPublicId);
    if (VBOX_SUCCESS (rc))
    {
        if (systemId)
            rc = RTUtf16ToUtf8 (systemId, &pszSystemId);
        if (VBOX_SUCCESS (rc))
        {
            if (baseURI)
                rc = RTUtf16ToUtf8 (baseURI, &pszBaseURI);
            if (VBOX_SUCCESS (rc))
            {
                CFGLDRENTITY entity;
                rc = m_pfnEntityResolver (pszPublicId, pszSystemId, pszBaseURI,
                                          &entity);
                if (rc == VINF_SUCCESS)
                    source = new CfgLdrInputSource (&entity, pszSystemId);
            }
        }
    }

    if (pszBaseURI)
        RTStrFree (pszBaseURI);
    if (pszSystemId)
        RTStrFree (pszSystemId);
    if (pszPublicId)
        RTStrFree (pszPublicId);

    return source;
}

// CfgLdrErrorHandler
////////////////////////////////////////////////////////////////////////////////

/**
 *  An error handler that accumulates all error messages in a single UTF-8 string.
 */
class CfgLdrErrorHandler : public DOMErrorHandler
#ifdef VBOX_XML_XSLT
                         , public ProblemListener
#endif
{
public:

    CfgLdrErrorHandler();
    ~CfgLdrErrorHandler();

    bool hasErrors() { return m_pszBuf != NULL; }

    /** Transfers ownership of the string to the caller and resets the handler */
    char *takeErrorMessage() {
        char *pszBuf = m_pszBuf;
        m_pszBuf = NULL;
        return pszBuf;
    }

    // Functions introduced in DOM Level 3
    bool handleError (const DOMError &domError);

#ifdef VBOX_XML_XSLT
    // Xalan ProblemListener interface
    void setPrintWriter (PrintWriter *pw) {}
    void problem (eProblemSource where, eClassification classification,
                  const XalanNode *sourceNode, const ElemTemplateElement *styleNode,
                  const XalanDOMString &msg, const XalanDOMChar *uri,
                  int lineNo, int charOffset);
#endif

private:

    char *m_pszBuf;
};

CfgLdrErrorHandler::CfgLdrErrorHandler() :
    m_pszBuf (NULL)
{
}

CfgLdrErrorHandler::~CfgLdrErrorHandler()
{
    if (m_pszBuf)
        RTMemTmpFree (m_pszBuf);
}

bool CfgLdrErrorHandler::handleError (const DOMError &domError)
{
    const char *pszSeverity = NULL;
    switch (domError.getSeverity())
    {
        case DOMError::DOM_SEVERITY_WARNING:        pszSeverity = "WARNING: ";
        case DOMError::DOM_SEVERITY_ERROR:          pszSeverity = "ERROR: ";
        case DOMError::DOM_SEVERITY_FATAL_ERROR:    pszSeverity = "FATAL ERROR: ";
    }

    char *pszLocation = NULL;
    const DOMLocator *pLocation = domError.getLocation();
    if (pLocation)
    {
        static const char Location[] = "\nLocation: '%s', line %d, column %d";

        char *pszURI = NULL;
        if (pLocation->getURI())
            RTUtf16ToUtf8 (pLocation->getURI(), &pszURI);

        size_t cbLocation = sizeof (Location) +
            (pszURI ? strlen (pszURI) : 10 /* NULL */) +
            10 + 10 + 1 /* line & column & \0 */;
        pszLocation = (char *) RTMemTmpAllocZ (cbLocation);
        RTStrPrintf (pszLocation, cbLocation, Location,
                     pszURI,
                     pLocation->getLineNumber(), pLocation->getColumnNumber());

        if (pszURI)
            RTStrFree (pszURI);
    }

    LogFlow (("CfgLdrErrorHandler::handleError():\n  %s%ls%s\n",
              pszSeverity, domError.getMessage(), pszLocation));

    char *pszMsg = NULL;
    if (domError.getMessage())
        RTUtf16ToUtf8 (domError.getMessage(), &pszMsg);

    size_t cbNewBuf = (m_pszBuf ? strlen (m_pszBuf) : 0) +
                      (pszSeverity ? strlen (pszSeverity) : 0) +
                      (pszMsg ? strlen (pszMsg) : 0) +
                      (pszLocation ? strlen (pszLocation) : 0);
    char *pszNewBuf = (char *) RTMemTmpAllocZ (cbNewBuf + 2 /* \n + \0 */);

    if (m_pszBuf)
    {
        strcpy (pszNewBuf, m_pszBuf);
        strcat (pszNewBuf, "\n");
    }
    if (pszSeverity)
        strcat (pszNewBuf, pszSeverity);
    if (pszMsg)
        strcat (pszNewBuf, pszMsg);
    if (pszLocation)
        strcat (pszNewBuf, pszLocation);

    if (m_pszBuf)
        RTMemTmpFree (m_pszBuf);
    m_pszBuf = pszNewBuf;

    if (pszLocation)
        RTMemTmpFree (pszLocation);
    if (pszMsg)
        RTStrFree (pszMsg);

    // fail on any error when possible
    return false;
}

#ifdef VBOX_XML_XSLT
void CfgLdrErrorHandler::problem (eProblemSource where, eClassification classification,
              const XalanNode *sourceNode, const ElemTemplateElement *styleNode,
              const XalanDOMString &msg, const XalanDOMChar *uri,
              int lineNo, int charOffset)
{
    const char *pszClass = NULL;
    switch (classification)
    {
        case eMESSAGE:  pszClass = "INFO: ";
        case eWARNING:  pszClass = "WARNING: ";
        case eERROR:    pszClass = "ERROR: ";
    }

    LogFlow (("CfgLdrErrorHandler::problem():\n  %s%ls\n", pszClass, msg.c_str()));

    char *pszMsg = NULL;
    if (msg.c_str())
        RTUtf16ToUtf8 (msg.c_str(), &pszMsg);

    size_t cbNewBuf = (m_pszBuf ? strlen (m_pszBuf) : 0) +
                      (pszClass ? strlen (pszClass) : 0) +
                      (pszMsg ? strlen (pszMsg) : 0);
    char *pszNewBuf = (char *) RTMemTmpAllocZ (cbNewBuf + 2 /* \n + \0 */);

    if (m_pszBuf)
    {
        strcpy (pszNewBuf, m_pszBuf);
        strcat (pszNewBuf, "\n");
    }
    if (pszClass)
        strcat (pszNewBuf, pszClass);
    if (pszMsg)
        strcat (pszNewBuf, pszMsg);

    if (m_pszBuf)
        RTStrFree (m_pszBuf);

    m_pszBuf = RTStrDup (pszNewBuf);

    if (pszNewBuf)
        RTMemTmpFree (pszNewBuf);
    if (pszMsg)
        RTStrFree (pszMsg);
}
#endif

//
////////////////////////////////////////////////////////////////////////////////

static int xmlInitialized = 0;

static int initXML (void)
{
    try
    {
        XMLPlatformUtils::Initialize();
    }

    catch(...)
    {
        return 0;
    }

    return (xmlInitialized = 1);
}

static void terminateXML (void)
{
    if (xmlInitialized)
    {
        XMLPlatformUtils::Terminate();
        xmlInitialized = 0;
    }
}

/* CfgLoader implementation */
CfgLoader::CfgLoader ()
    :
    pwszOriginalFilename (NULL),
    hOriginalFileHandle (NIL_RTFILE),
    pfirstnode (NULL),
    builder (NULL),
    root (NULL)
{
}

CfgLoader::~CfgLoader ()
{
    if (pwszOriginalFilename)
    {
        RTUtf16Free (pwszOriginalFilename);
    }

    if (builder)
    {
        /* Configuration was parsed from a file.
         * Parser owns root and will delete root.
         */
        builder->release();
    }
    else if (root)
    {
        /* This is new, just created configuration.
         * root is new object created by DOMImplementation::createDocument
         * We have to delete root.
         */
        root->release();
    }
}

int CfgLoader::Load (const char *pszFileName, RTFILE hFileHandle,
                     const char *pszExternalSchemaLocation, bool bDoNamespaces,
                     PFNCFGLDRENTITYRESOLVER pfnEntityResolver,
                     char **ppszErrorMessage)
{
    if (!xmlInitialized)
        return VERR_NOT_SUPPORTED;

    Assert (!root && !pwszOriginalFilename);
    if (root || pwszOriginalFilename)
        return VERR_ALREADY_LOADED;

    static const XMLCh LS[] = { chLatin_L, chLatin_S, chNull };
    DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation (LS);
    if (!impl)
        return VERR_NOT_SUPPORTED;

    //  note:
    //  member variables allocated here (builder, pwszOriginalFilename) are not
    //  freed in case of error because CFGLDRLoad() that calls this method will
    //  delete this instance (and all members) if we return a failure from here

    builder = static_cast <DOMImplementationLS *> (impl)->
        createDOMBuilder (DOMImplementationLS::MODE_SYNCHRONOUS, 0);
    if (!builder)
        return VERR_NOT_SUPPORTED;

    int rc = VINF_SUCCESS;

    if (ppszErrorMessage)
        *ppszErrorMessage = NULL;

    // set parser's features
    Assert (builder->canSetFeature (XMLUni::fgDOMDatatypeNormalization, true));
    if (builder->canSetFeature (XMLUni::fgDOMDatatypeNormalization, true))
        builder->setFeature (XMLUni::fgDOMDatatypeNormalization, true);
    else
        return VERR_NOT_SUPPORTED;
    if (bDoNamespaces)
    {
        Assert (builder->canSetFeature (XMLUni::fgDOMNamespaces, true));
        if (builder->canSetFeature (XMLUni::fgDOMNamespaces, true))
            builder->setFeature (XMLUni::fgDOMNamespaces, true);
        else
            return VERR_NOT_SUPPORTED;
    }
    if (pszExternalSchemaLocation)
    {
        // set validation related features & properties
        Assert (builder->canSetFeature (XMLUni::fgDOMValidation, true));
        if (builder->canSetFeature (XMLUni::fgDOMValidation, true))
            builder->setFeature (XMLUni::fgDOMValidation, true);
        else
            return VERR_NOT_SUPPORTED;
        Assert (builder->canSetFeature (XMLUni::fgXercesSchema, true));
        if (builder->canSetFeature (XMLUni::fgXercesSchema, true))
            builder->setFeature (XMLUni::fgXercesSchema, true);
        else
            return VERR_NOT_SUPPORTED;
        Assert (builder->canSetFeature (XMLUni::fgXercesSchemaFullChecking, true));
        if (builder->canSetFeature (XMLUni::fgXercesSchemaFullChecking, true))
            builder->setFeature (XMLUni::fgXercesSchemaFullChecking, true);
        else
            return VERR_NOT_SUPPORTED;

        PRTUTF16 pwszExternalSchemaLocation = NULL;
        rc = RTStrToUtf16 (pszExternalSchemaLocation, &pwszExternalSchemaLocation);
        if (VBOX_FAILURE (rc))
            return rc;

        if (bDoNamespaces)
        {
            // set schema that supports namespaces
            builder->setProperty (XMLUni::fgXercesSchemaExternalSchemaLocation,
                                  pwszExternalSchemaLocation);
        }
        else
        {
            // set schema that doesn't support namespaces
            builder->setProperty (XMLUni::fgXercesSchemaExternalNoNameSpaceSchemaLocation,
                                  pwszExternalSchemaLocation);
        }

        RTUtf16Free (pwszExternalSchemaLocation);
    }

    hOriginalFileHandle = hFileHandle;
    rc = RTStrToUtf16 (pszFileName, &pwszOriginalFilename);
    if (VBOX_FAILURE (rc))
        return rc;

    CfgLdrEntityResolver entityResolver (pfnEntityResolver);
    builder->setEntityResolver (&entityResolver);

    CfgLdrErrorHandler errorHandler;
    builder->setErrorHandler (&errorHandler);

    try
    {
        if (hFileHandle != NIL_RTFILE)
        {
            CFGLDRENTITY entity;
            entity.enmType = CFGLDRENTITYTYPE_HANDLE;
            entity.u.handle.hFile = hFileHandle;
            entity.u.handle.bClose = false;
            CfgLdrInputSource source (&entity, pszFileName);
            root = builder->parse (source);
        }
        else
        {
            root = builder->parseURI (pwszOriginalFilename);
        }
    }
    catch (...)
    {
        rc = VERR_OPEN_FAILED;
    }

    if (errorHandler.hasErrors())
    {
        // this will transfer ownership of the string
        if (ppszErrorMessage)
            *ppszErrorMessage = errorHandler.takeErrorMessage();
        rc = VERR_OPEN_FAILED;
    }

    builder->setErrorHandler (NULL);
    builder->setEntityResolver (NULL);

    return rc;
}

int CfgLoader::Save (const char *pszFilename, RTFILE hFileHandle,
                     char **ppszErrorMessage)
{
    if (!pszFilename && !pwszOriginalFilename &&
        hFileHandle == NIL_RTFILE && hOriginalFileHandle == NIL_RTFILE)
    {
        // no explicit handle/filename specified and the configuration
        // was created from scratch
        return VERR_INVALID_PARAMETER;
    }

    static const XMLCh LS[] = { chLatin_L, chLatin_S, chNull };
    DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation (LS);
    if (!impl)
        return VERR_NOT_SUPPORTED;

    DOMWriter *writer = static_cast <DOMImplementationLS *> (impl)->createDOMWriter();
    if (!writer)
        return VERR_NOT_SUPPORTED;

    int rc = VINF_SUCCESS;

    if (ppszErrorMessage)
        *ppszErrorMessage = NULL;

#ifdef VBOX_XML_WRITER_FILTER
    VBoxWriterFilter theFilter (DOMNodeFilter::SHOW_TEXT);
    writer->setFilter (&theFilter);
#endif

    writer->setEncoding (XMLUni::fgUTF8EncodingString);

    // set feature if the serializer supports the feature/mode
    if (writer->canSetFeature(XMLUni::fgDOMWRTDiscardDefaultContent, true))
        writer->setFeature(XMLUni::fgDOMWRTDiscardDefaultContent, true);
    if (writer->canSetFeature (XMLUni::fgDOMWRTFormatPrettyPrint, true))
        writer->setFeature (XMLUni::fgDOMWRTFormatPrettyPrint, true);

    CfgLdrErrorHandler errorHandler;
    writer->setErrorHandler (&errorHandler);

    try
    {
        if (hFileHandle != NIL_RTFILE || hOriginalFileHandle != NIL_RTFILE)
        {
            CFGLDRENTITY entity;
            entity.enmType = CFGLDRENTITYTYPE_HANDLE;
            entity.u.handle.hFile = hFileHandle != NIL_RTFILE ? hFileHandle :
                                    hOriginalFileHandle;
            entity.u.handle.bClose = false;
            CfgLdrFormatTarget target (&entity);
            writer->writeNode (&target, *root);
        }
        else
        {
            PRTUTF16 pwszFilename = NULL;
            if (pszFilename)
                rc = RTStrToUtf16 (pszFilename, &pwszFilename);
            if (VBOX_SUCCESS (rc))
            {
                LocalFileFormatTarget target (pwszFilename ? pwszFilename :
                                              pwszOriginalFilename);
                if (pwszFilename)
                    RTUtf16Free (pwszFilename);

                writer->writeNode (&target, *root);
            }
        }
    }
    catch(...)
    {
        rc = VERR_FILE_IO_ERROR;
    }

    if (errorHandler.hasErrors())
    {
        // this will transfer ownership of the string
        if (ppszErrorMessage)
            *ppszErrorMessage = errorHandler.takeErrorMessage();
        rc = VERR_FILE_IO_ERROR;
    }

    writer->release();

    if (hFileHandle != NIL_RTFILE || hOriginalFileHandle != NIL_RTFILE)
        (void)RTFileFlush(hFileHandle != NIL_RTFILE ? hFileHandle :
                          hOriginalFileHandle);

    return rc;
}

#ifdef VBOX_XML_XSLT
int CfgLoader::Transform (const char *pszTemlateLocation,
                          PFNCFGLDRENTITYRESOLVER pfnEntityResolver,
                          char **ppszErrorMessage)
{
    AssertReturn (strcmp (pszTemlateLocation, "SettingsConverter.xsl") == 0,
                  VERR_NOT_SUPPORTED);
    AssertReturn (pfnEntityResolver == NULL,
                  VERR_NOT_SUPPORTED);

    int rc = VINF_SUCCESS;

    if (ppszErrorMessage)
        *ppszErrorMessage = NULL;

    XalanTransformer::initialize();

    XalanTransformer xalan;

    // input stream to read from SettingsConverter_xsl
    struct SettingsConverterStream : public XSLTInputSource
    {
        SettingsConverterStream()
        {
            XMLCh *id = XMLString::transcode ("SettingsConverter.xsl");
            setSystemId (id);
            setPublicId (id);
            XMLString::release (&id);
        }

        BinInputStream *makeStream () const
        {
            return new BinMemInputStream (g_abSettingsConverter_xsl,
                                          g_cbSettingsConverter_xsl);
        }
    };

    CfgLdrErrorHandler errorHandler;
    xalan.setProblemListener (&errorHandler);

    try
    {
        // target is the DOM tree
        DOMDocument *newRoot =
            DOMImplementation::getImplementation()->createDocument();
        FormatterToXercesDOM formatter (newRoot, 0);

        // source is the DOM tree
        XercesDOMSupport support;
        XercesParserLiaison liaison;
        const XercesDOMWrapperParsedSource parsedSource (
                                   Document(), liaison, support,
                                   XalanDOMString (pwszOriginalFilename));

        // stylesheet
        SettingsConverterStream xsl;

        int xrc = xalan.transform (parsedSource, xsl, formatter);

        if (xrc)
        {
            LogFlow(("xalan.transform() = %d (%s)\n", xrc, xalan.getLastError()));

            newRoot->release();
            rc = VERR_FILE_IO_ERROR;
        }
        else
        {
            // release the builder and the old document, if any
            if (builder)
            {
                builder->release();
                builder = 0;
                root = 0;
            }
            else if (root)
            {
                root->release();
                root = 0;
            }

            root = newRoot;

            // Xalan 1.9.0 (and 1.10.0) is stupid bacause disregards the
            // XSLT specs and ignores the exclude-result-prefixes stylesheet
            // attribute, flooding all the elements with stupid xmlns
            // specifications. Here we remove them.
            XMLCh *xmlnsName = XMLString::transcode ("xmlns");
            XMLCh *xmlnsVBox = XMLString::transcode ("http://www.innotek.de/VirtualBox-settings");
            DOMNodeIterator *iter =
                newRoot->createNodeIterator (newRoot, DOMNodeFilter::SHOW_ELEMENT,
                                             NULL, false);
            DOMNode *node = NULL;
            while ((node = iter->nextNode()) != NULL)
            {
                DOMElement *elem = static_cast <DOMElement *> (node);
                if (elem->getParentNode() == newRoot)
                    continue;

                const XMLCh *xmlns = elem->getAttribute (xmlnsName);
                if (xmlns == NULL)
                    continue;
                if (xmlns[0] == 0 ||
                    XMLString::compareString (xmlns, xmlnsVBox) == 0)
                {
                    elem->removeAttribute (xmlnsName);
                }
            }
            XMLString::release (&xmlnsVBox);
            XMLString::release (&xmlnsName);
        }
    }
    catch (...)
    {
        rc = VERR_FILE_IO_ERROR;
    }

    if (VBOX_FAILURE (rc))
    {
        // this will transfer ownership of the string
        if (ppszErrorMessage)
        {
            if (xalan.getLastError())
                *ppszErrorMessage = RTStrDup (xalan.getLastError());
            else
                *ppszErrorMessage = errorHandler.takeErrorMessage();
        }
    }

    XalanTransformer::terminate();

    return rc;
}
#endif

int CfgLoader::Create()
{
    if (!xmlInitialized)
    {
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;

    static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };
    DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation(gLS);

    if (impl)
    {
        // creating an empty doc is a non-standard extension to DOM specs.
        // we're using it since we're bound to Xerces anyway.
        root = impl->createDocument();
    }
    if (!root)
    {
        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

int CfgLoader::FreeConfiguration (CfgLoader *pcfg)
{
    int rc = VINF_SUCCESS;

    while (pcfg->pfirstnode)
    {
        CfgNode::ReleaseNode (pcfg->pfirstnode);
    }

    delete pcfg;

    return rc;
}

int CfgLoader::CreateNode (const char *pszName, CfgNode **ppnode)
{
    return getNode (root, pszName, 0, ppnode, CfgNode::fCreateIfNotExists);
}

int CfgLoader::GetNode (const char *pszName, unsigned uIndex, CfgNode **ppnode)
{
    return getNode (root, pszName, uIndex, ppnode, CfgNode::fSearch);
}

int CfgLoader::getNode (DOMNode *prootnode, const char *pszName, unsigned uIndex, CfgNode **ppnode, unsigned flags)
{
    int rc = VINF_SUCCESS;

    CfgNode *pnode = new CfgNode (this);

    if (!pnode)
    {
        rc = VERR_NO_MEMORY;
    }
    else if (!prootnode)
    {
        rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        rc = pnode->resolve (prootnode, pszName, uIndex, flags);
    }

    if (VBOX_SUCCESS(rc))
    {
        pnode->next = pfirstnode;
        if (pfirstnode)
        {
            pfirstnode->prev = pnode;
        }
        pfirstnode = pnode;

        *ppnode = pnode;
    }
    else
    {
        if (pnode)
        {
             delete pnode;
        }
    }

    return rc;
}

/* CfgNode implementation */
CfgNode::CfgNode (CfgLoader *pcfg)
    :
    pConfiguration (pcfg),
    next (NULL),
    prev (NULL)
{
}

CfgNode::~CfgNode ()
{
}

int CfgNode::ReleaseNode (CfgNode *pnode)
{
    int rc = VINF_SUCCESS;

    if (pnode->next)
    {
        pnode->next->prev = pnode->prev;
    }

    if (pnode->prev)
    {
        pnode->prev->next = pnode->next;
    }
    else
    {
        pnode->pConfiguration->pfirstnode = pnode->next;
    }

    delete pnode;

    return rc;
}

int CfgNode::DeleteNode (CfgNode *pnode)
{
    int rc = VINF_SUCCESS;

    DOMNode *pparent = pnode->pdomnode->getParentNode ();

    pparent->removeChild (pnode->pdomnode);

    pnode->pdomnode = 0;

    ReleaseNode (pnode);

    return rc;
}

int CfgNode::resolve (DOMNode *root, const char *pszName, unsigned uIndex, unsigned flags)
{
    static const char *pcszEmptyName = "";

    if (!root)
    {
        return VERR_PATH_NOT_FOUND;
    }

    if (!pszName)
    {
        // special case for resolving any child
        pszName = pcszEmptyName;
    }

    if ((flags & (fCreateIfNotExists | fAppend)) && !(*pszName))
    {
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;

    PRTUTF16 pwszName = NULL;

    rc = RTStrToUtf16 (pszName, &pwszName);

    if (VBOX_SUCCESS(rc))
    {
        XMLCh *puszComponent = pwszName;

        bool lastComponent = false;

        int lastindex = XMLString::indexOf (puszComponent, '/');

        if (lastindex == -1)
        {
            lastindex = XMLString::stringLen (puszComponent);
            lastComponent = true;
        }

        rc = VERR_PATH_NOT_FOUND;

        for (;;)
        {
            DOMNode *child = 0, *first = 0;

            if (!lastComponent || !(flags & fAppend))
            {
                for (child = root->getFirstChild(); child != 0; child=child->getNextSibling())
                {
                    if (child->getNodeType () == DOMNode::ELEMENT_NODE)
                    {
                        if (!lastindex || XMLString::compareNString (child->getNodeName (), puszComponent, lastindex) == 0)
                        {
                            if (lastComponent)
                            {
                                if (uIndex == 0)
                                {
                                    pdomnode = child;
                                    rc = VINF_SUCCESS;
                                    break;
                                }
                                uIndex--;
                                continue;
                            }
                            else
                            {
                                if (!first)
                                {
                                    first = child;
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            if (first)
            {
                if (child)
                {
                    // some element in the path (except the last) has
                    // siblingswith the same name
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }
                root = child = first;
            }

            if (!child)
            {
                if (flags & (fCreateIfNotExists | fAppend))
                {
                    // Extract the component name to a temporary buffer
                    RTUTF16 uszName[256];

                    memcpy (uszName, puszComponent, lastindex * sizeof(uszName[0]));
                    uszName[lastindex] = 0;

                    try
                    {
                        DOMElement *elem = pConfiguration->Document()->createElement(uszName);
                        root = root->appendChild(elem);
                    }

                    catch (...)
                    {
                        Log(( "Error creating element [%ls]\n", uszName ));
                        rc = VERR_CFG_NO_VALUE;
                        break;
                    }

                    if (lastComponent)
                    {
                        pdomnode = root;
                        rc = VINF_SUCCESS;
                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            puszComponent += lastindex;
            if (*puszComponent)
            {
                puszComponent++;
            }

            if (!*puszComponent)
            {
                break;
            }

            lastindex = XMLString::indexOf (puszComponent, '/');

            if (lastindex == -1)
            {
                lastindex = XMLString::stringLen (puszComponent);
                lastComponent = true;
            }
        }

        RTUtf16Free (pwszName);
    }

    return rc;
}

int CfgNode::GetChild (const char *pszName, unsigned uIndex, CfgNode **ppnode)
{
    return pConfiguration->getNode (pdomnode, pszName, uIndex, ppnode, fSearch);
}

int CfgNode::CreateChildNode (const char *pszName, CfgNode **ppnode)
{
    return pConfiguration->getNode (pdomnode, pszName, 0, ppnode, fCreateIfNotExists);
}

int CfgNode::AppendChildNode (const char *pszName, CfgNode **ppnode)
{
    return pConfiguration->getNode (pdomnode, pszName, 0, ppnode, fAppend);
}

int CfgNode::CountChildren (const char *pszChildName, unsigned *pCount)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszChildName = NULL;

    if (pszChildName)
    {
        rc = RTStrToUtf16 (pszChildName, &pwszChildName);
    }

    if (VBOX_SUCCESS(rc))
    {
        DOMNode *child;

        unsigned count = 0;

        for (child = pdomnode->getFirstChild(); child != 0; child=child->getNextSibling())
        {
           if (child->getNodeType () == DOMNode::ELEMENT_NODE)
           {
              if (pwszChildName == NULL)
              {
                  count++;
              }
              else if (XMLString::compareString (child->getNodeName (), pwszChildName) == 0)
              {
                  count++;
              }
           }
        }

        if (pwszChildName)
        {
            RTUtf16Free (pwszChildName);
        }

        *pCount = count;
    }

    return rc;
}

DOMNode *CfgNode::findChildText (void)
{
    DOMNode *child = NULL;

    for (child = pdomnode->getFirstChild(); child != 0; child=child->getNextSibling())
    {
        if (child->getNodeType () == DOMNode::TEXT_NODE)
        {
            break;
        }
    }

    return child;
}

/**
 * Gets the value of the given attribute as a UTF-16 string.
 * The returned string is owned by CfgNode, the caller must not free it.
 *
 * @param pszName       Attribute name.
 * @param ppwszValue    Where to store a pointer to the attribute value.
 *
 * @return IPRT status code.
 */
int CfgNode::getValueString (const char *pszName, PRTUTF16 *ppwszValue)
{
    int rc = VINF_SUCCESS;

    PCRTUTF16 pwszValue = NULL;

    if (!pszName)
    {
        DOMNode *ptext = findChildText ();

        if (ptext)
        {
            pwszValue = ptext->getNodeValue ();
        }
    }
    else
    {
        PRTUTF16 pwszName = NULL;

        rc = RTStrToUtf16 (pszName, &pwszName);

        if (VBOX_SUCCESS(rc))
        {
            DOMAttr *attr = (static_cast<DOMElement *>(pdomnode))->getAttributeNode (pwszName);
            if (attr)
            {
                pwszValue = attr->getValue ();
            }

            RTUtf16Free (pwszName);
        }
    }

    if (!pwszValue)
    {
        *ppwszValue = NULL;
        rc = VERR_CFG_NO_VALUE;
    }
    else
    {
        *ppwszValue = (PRTUTF16)pwszValue;
    }

    return rc;
}

int CfgNode::setValueString (const char *pszName, PRTUTF16 pwszValue)
{
    int rc = VINF_SUCCESS;

    if (!pszName)
    {
        DOMText *val = NULL;

        try
        {
            val = pConfiguration->Document ()->createTextNode(pwszValue);
        }

        catch (...)
        {
            rc = VERR_CFG_NO_VALUE;
        }

        if (VBOX_SUCCESS(rc) && val)
        {
            try
            {
                DOMNode *poldtext = findChildText ();

                if (poldtext)
                {
                    pdomnode->replaceChild (val, poldtext);
                    poldtext->release();
                }
                else
                {
                    pdomnode->appendChild (val);
                }
            }

            catch (...)
            {
                rc = VERR_CFG_NO_VALUE;
                val->release();
            }
        }
    }
    else
    {
        PRTUTF16 pwszName = NULL;

        rc = RTStrToUtf16 (pszName, &pwszName);

        if (VBOX_SUCCESS(rc))
        {
            try
            {
                static_cast<DOMElement *>(pdomnode)->setAttribute (pwszName, pwszValue);
            }

            catch (...)
            {
                rc = VERR_CFG_NO_VALUE;
            }
        }
    }

    return rc;
}

int CfgNode::QueryUInt32 (const char *pszName, uint32_t *pulValue)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;

    rc = getValueString (pszName, &pwszValue);
    if (VBOX_SUCCESS(rc))
    {
        uint32_t value = 0;
        char *pszValue = NULL;

        rc = RTUtf16ToUtf8 (pwszValue, &pszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = RTStrToUInt32Ex (pszValue, NULL, 0, &value);
            if (VBOX_SUCCESS(rc))
            {
                *pulValue = value;
            }

            RTStrFree (pszValue);
        }
    }

    return rc;
}

int CfgNode::SetUInt32 (const char *pszName, uint32_t ulValue, unsigned int uiBase)
{
    int rc = VINF_SUCCESS;

    char szValue [64];

    rc = RTStrFormatNumber (szValue, (uint64_t) ulValue, uiBase, 0, 0,
                            RTSTR_F_32BIT | RTSTR_F_SPECIAL);
    if (VBOX_SUCCESS (rc))
    {
        PRTUTF16 pwszValue = NULL;

        rc = RTStrToUtf16 (szValue, &pwszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = setValueString (pszName, pwszValue);
            RTUtf16Free (pwszValue);
        }
    }

    return rc;
}

int CfgNode::QueryUInt64 (const char *pszName, uint64_t *pullValue)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;

    rc = getValueString (pszName, &pwszValue);
    if (VBOX_SUCCESS(rc))
    {
        uint64_t value = 0;
        char *pszValue = NULL;

        rc = RTUtf16ToUtf8 (pwszValue, &pszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = RTStrToUInt64Ex (pszValue, NULL, 0, &value);
            if (VBOX_SUCCESS(rc))
            {
                *pullValue = value;
            }

            RTStrFree (pszValue);
        }
    }

    return rc;
}

int CfgNode::SetUInt64 (const char *pszName, uint64_t ullValue, unsigned int uiBase)
{
    int rc = VINF_SUCCESS;

    char szValue [64];

    rc = RTStrFormatNumber (szValue, ullValue, uiBase, 0, 0,
                            RTSTR_F_64BIT | RTSTR_F_SPECIAL);
    if (VBOX_SUCCESS (rc))
    {
        PRTUTF16 pwszValue = NULL;

        rc = RTStrToUtf16 (szValue, &pwszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = setValueString (pszName, pwszValue);
            RTUtf16Free (pwszValue);
        }
    }

    return rc;
}

int CfgNode::QueryInt32 (const char *pszName, int32_t *plValue)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;

    rc = getValueString (pszName, &pwszValue);
    if (VBOX_SUCCESS(rc))
    {
        int32_t value = 0;
        char *pszValue = NULL;

        rc = RTUtf16ToUtf8 (pwszValue, &pszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = RTStrToInt32Ex (pszValue, NULL, 0, &value);
            if (VBOX_SUCCESS(rc))
            {
                *plValue = value;
            }

            RTStrFree (pszValue);
        }
    }

    return rc;
}

int CfgNode::SetInt32 (const char *pszName, int32_t lValue, unsigned int uiBase)
{
    int rc = VINF_SUCCESS;

    char szValue [64];

    rc = RTStrFormatNumber (szValue, (uint64_t) lValue, uiBase, 0, 0,
                            RTSTR_F_32BIT | RTSTR_F_VALSIGNED | RTSTR_F_SPECIAL);
    if (VBOX_SUCCESS (rc))
    {
        PRTUTF16 pwszValue = NULL;

        rc = RTStrToUtf16 (szValue, &pwszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = setValueString (pszName, pwszValue);
            RTUtf16Free (pwszValue);
        }
    }

    return rc;
}

int CfgNode::QueryInt64 (const char *pszName, int64_t *pllValue)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;

    rc = getValueString (pszName, &pwszValue);
    if (VBOX_SUCCESS(rc))
    {
        int64_t value = 0;
        char *pszValue = NULL;

        rc = RTUtf16ToUtf8 (pwszValue, &pszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = RTStrToInt64Ex (pszValue, NULL, 0, &value);
            if (VBOX_SUCCESS(rc))
            {
                *pllValue = value;
            }

            RTStrFree (pszValue);
        }
    }

    return rc;
}

int CfgNode::SetInt64 (const char *pszName, int64_t llValue, unsigned int uiBase)
{
    int rc = VINF_SUCCESS;

    char szValue [64];

    rc = RTStrFormatNumber (szValue, (uint64_t) llValue, uiBase, 0, 0,
                            RTSTR_F_64BIT | RTSTR_F_VALSIGNED | RTSTR_F_SPECIAL);
    if (VBOX_SUCCESS (rc))
    {
        PRTUTF16 pwszValue = NULL;

        rc = RTStrToUtf16 (szValue, &pwszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = setValueString (pszName, pwszValue);
            RTUtf16Free (pwszValue);
        }
    }

    return rc;
}

int CfgNode::QueryUInt16 (const char *pszName, uint16_t *puhValue)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;

    rc = getValueString (pszName, &pwszValue);
    if (VBOX_SUCCESS(rc))
    {
        uint16_t value = 0;
        char *pszValue = NULL;

        rc = RTUtf16ToUtf8 (pwszValue, &pszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = RTStrToUInt16Ex (pszValue, NULL, 0, &value);
            if (VBOX_SUCCESS(rc))
            {
                *puhValue = value;
            }

            RTStrFree (pszValue);
        }
    }

    return rc;
}

int CfgNode::SetUInt16 (const char *pszName, uint16_t uhValue, unsigned int uiBase)
{
    int rc = VINF_SUCCESS;

    char szValue [64];

    rc = RTStrFormatNumber (szValue, (uint64_t) uhValue, uiBase, 0, 0,
                            RTSTR_F_16BIT | RTSTR_F_SPECIAL);
    if (VBOX_SUCCESS (rc))
    {
        PRTUTF16 pwszValue = NULL;

        rc = RTStrToUtf16 (szValue, &pwszValue);
        if (VBOX_SUCCESS (rc))
        {
            rc = setValueString (pszName, pwszValue);
            RTUtf16Free (pwszValue);
        }
    }

    return rc;
}

int CfgNode::QueryBin (const char *pszName, void *pvValue, unsigned cbValue, unsigned *pcbValue)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;

    rc = getValueString (pszName, &pwszValue);
    if (VBOX_SUCCESS(rc))
    {
        if ( (XMLString::stringLen (pwszValue) / 2) > cbValue)
        {
            rc = VERR_BUFFER_OVERFLOW;
        }
        else if (!pvValue)
        {
            rc = VERR_INVALID_POINTER;
        }
        else
        {
            rc = wstr_to_bin (pwszValue, pvValue, cbValue, pcbValue);
        }
    }

    return rc;
}

int CfgNode::SetBin (const char *pszName, const void *pvValue, unsigned cbValue)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;

    rc = bin_to_wstr (pvValue, cbValue, &pwszValue);
    if (VBOX_SUCCESS (rc))
    {
        rc = setValueString (pszName, pwszValue);
        RTUtf16Free (pwszValue);
    }

    return rc;
}

int CfgNode::QueryString (const char *pszName, void **pValue, unsigned cbValue, unsigned *pcbValue, bool returnUtf16)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;

    // start with 0 bytes return value
    if (pcbValue)
        *pcbValue = 0;

    rc = getValueString (pszName, &pwszValue);

    if (VBOX_SUCCESS(rc))
    {
        if (returnUtf16)
        {
            // make a copy
            *pValue = (void*)SysAllocString((const OLECHAR*)pwszValue);
        } else
        {
            char *psz = NULL;

            rc = RTUtf16ToUtf8 (pwszValue, &psz);

            if (VBOX_SUCCESS(rc))
            {
                unsigned l = strlen (psz) + 1; // take trailing nul to account

                *pcbValue = l;

                if (l > cbValue)
                {
                    rc = VERR_BUFFER_OVERFLOW;
                }
                else if (!*pValue)
                {
                    rc = VERR_INVALID_POINTER;
                }
                else
                {
                    memcpy (*pValue, psz, l);
                }

                RTStrFree (psz);
            }
        }
    }
    return rc;
}

int CfgNode::SetString (const char *pszName, const char *pszValue, unsigned cbValue, bool isUtf16)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;

    if (isUtf16)
        pwszValue = (PRTUTF16)pszValue;
    else
        rc = RTStrToUtf16 (pszValue, &pwszValue);

    if (VBOX_SUCCESS (rc))
    {
        rc = setValueString (pszName, pwszValue);

        if (!isUtf16)
            RTUtf16Free (pwszValue);
    }

    return rc;
}

int CfgNode::QueryBool (const char *pszName, bool *pfValue)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszValue = NULL;
    rc = getValueString (pszName, &pwszValue);
    if (VBOX_SUCCESS (rc))
    {
        char *pszValue = NULL;
        rc = RTUtf16ToUtf8 (pwszValue, &pszValue);
        if (VBOX_SUCCESS (rc))
        {
            if (    !stricmp (pszValue, "true")
                ||  !stricmp (pszValue, "yes")
                ||  !stricmp (pszValue, "on"))
                *pfValue = true;
            else if (    !stricmp (pszValue, "false")
                     ||  !stricmp (pszValue, "no")
                     ||  !stricmp (pszValue, "off"))
                *pfValue = false;
            else
                rc = VERR_CFG_INVALID_FORMAT;
            RTStrFree (pszValue);
        }
    }

    return rc;
}

int CfgNode::SetBool (const char *pszName, bool fValue)
{
    return SetString (pszName, fValue ? "true" : "false", fValue ? 4 : 5, false);
}

int CfgNode::DeleteAttribute (const char *pszName)
{
    int rc = VINF_SUCCESS;

    PRTUTF16 pwszName = NULL;

    rc = RTStrToUtf16 (pszName, &pwszName);

    if (VBOX_SUCCESS (rc))
    {
        try
        {
            (static_cast<DOMElement *>(pdomnode))->removeAttribute (pwszName);
        }

        catch (...)
        {
            rc = VERR_CFG_NO_VALUE;
        }

        RTUtf16Free (pwszName);
    }

    return rc;
}

/* Configuration loader public entry points.
 */

CFGLDRR3DECL(int) CFGLDRCreate(CFGHANDLE *phcfg)
{
    if (!phcfg)
    {
        return VERR_INVALID_POINTER;
    }

    CfgLoader *pcfgldr = new CfgLoader ();

    if (!pcfgldr)
    {
        return VERR_NO_MEMORY;
    }

    int rc = pcfgldr->Create();

    if (VBOX_SUCCESS(rc))
    {
        *phcfg = pcfgldr;
    }
    else
    {
        delete pcfgldr;
    }

    return rc;
}

CFGLDRR3DECL(int) CFGLDRLoad (CFGHANDLE *phcfg,
                              const char *pszFileName, RTFILE hFileHandle,
                              const char *pszExternalSchemaLocation, bool bDoNamespaces,
                              PFNCFGLDRENTITYRESOLVER pfnEntityResolver,
                              char **ppszErrorMessage)
{
    if (!phcfg || !pszFileName)
        return VERR_INVALID_POINTER;

    CfgLoader *pcfgldr = new CfgLoader();
    if (!pcfgldr)
        return VERR_NO_MEMORY;

    int rc = pcfgldr->Load (pszFileName, hFileHandle,
                            pszExternalSchemaLocation, bDoNamespaces,
                            pfnEntityResolver, ppszErrorMessage);

    if (VBOX_SUCCESS(rc))
        *phcfg = pcfgldr;
    else
        delete pcfgldr;

    return rc;
}

CFGLDRR3DECL(int) CFGLDRFree(CFGHANDLE hcfg)
{
    if (!hcfg)
    {
        return VERR_INVALID_HANDLE;
    }

    CfgLoader::FreeConfiguration (hcfg);

    return VINF_SUCCESS;
}

CFGLDRR3DECL(int) CFGLDRSave(CFGHANDLE hcfg,
                             char **ppszErrorMessage)
{
    if (!hcfg)
    {
        return VERR_INVALID_HANDLE;
    }
    return hcfg->Save (NULL, NIL_RTFILE, ppszErrorMessage);
}

CFGLDRR3DECL(int) CFGLDRSaveAs(CFGHANDLE hcfg,
                               const char *pszFilename, RTFILE hFileHandle,
                               char **ppszErrorMessage)
{
    if (!hcfg)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pszFilename)
    {
        return VERR_INVALID_POINTER;
    }
    return hcfg->Save (pszFilename, hFileHandle, ppszErrorMessage);
}

CFGLDRR3DECL(int) CFGLDRTransform (CFGHANDLE hcfg,
                                   const char *pszTemlateLocation,
                                   PFNCFGLDRENTITYRESOLVER pfnEntityResolver,
                                   char **ppszErrorMessage)
{
#ifdef VBOX_XML_XSLT
    if (!hcfg)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pszTemlateLocation)
    {
        return VERR_INVALID_POINTER;
    }
    return hcfg->Transform (pszTemlateLocation, pfnEntityResolver, ppszErrorMessage);
#else
    return VERR_NOT_SUPPORTED;
#endif
}

CFGLDRR3DECL(int) CFGLDRGetNode(CFGHANDLE hcfg, const char *pszName, unsigned uIndex, CFGNODE *phnode)
{
    if (!hcfg)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!phnode)
    {
        return VERR_INVALID_POINTER;
    }
    return hcfg->GetNode (pszName, uIndex, phnode);
}

CFGLDRR3DECL(int) CFGLDRGetChildNode(CFGNODE hparent, const char *pszName, unsigned uIndex, CFGNODE *phnode)
{
    if (!hparent)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!phnode)
    {
        return VERR_INVALID_POINTER;
    }
    return hparent->GetChild (pszName, uIndex, phnode);
}

CFGLDRR3DECL(int) CFGLDRCreateNode(CFGHANDLE hcfg, const char *pszName, CFGNODE *phnode)
{
    if (!hcfg)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!phnode || !pszName)
    {
        return VERR_INVALID_POINTER;
    }
    return hcfg->CreateNode (pszName, phnode);
}

CFGLDRR3DECL(int) CFGLDRCreateChildNode(CFGNODE hparent, const char *pszName, CFGNODE *phnode)
{
    if (!hparent)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!phnode || !pszName)
    {
        return VERR_INVALID_POINTER;
    }
    return hparent->CreateChildNode (pszName, phnode);
}

CFGLDRR3DECL(int) CFGLDRAppendChildNode(CFGNODE hparent, const char *pszName, CFGNODE *phnode)
{
    if (!hparent)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!phnode || !pszName)
    {
        return VERR_INVALID_POINTER;
    }
    return hparent->AppendChildNode (pszName, phnode);
}

CFGLDRR3DECL(int) CFGLDRReleaseNode(CFGNODE hnode)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return CfgNode::ReleaseNode (hnode);
}

CFGLDRR3DECL(int) CFGLDRDeleteNode(CFGNODE hnode)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return CfgNode::DeleteNode (hnode);
}

CFGLDRR3DECL(int) CFGLDRCountChildren(CFGNODE hnode, const char *pszChildName, unsigned *pCount)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pCount)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->CountChildren (pszChildName, pCount);
}

CFGLDRR3DECL(int) CFGLDRQueryUInt32(CFGNODE hnode, const char *pszName, uint32_t *pulValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pulValue)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->QueryUInt32 (pszName, pulValue);
}

CFGLDRR3DECL(int) CFGLDRSetUInt32(CFGNODE hnode, const char *pszName, uint32_t ulValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetUInt32 (pszName, ulValue);
}

CFGLDRR3DECL(int) CFGLDRSetUInt32Ex(CFGNODE hnode, const char *pszName, uint32_t ulValue, unsigned int uiBase)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetUInt32 (pszName, ulValue, uiBase);
}

CFGLDRR3DECL(int) CFGLDRQueryUInt64(CFGNODE hnode, const char *pszName, uint64_t *pullValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pullValue)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->QueryUInt64 (pszName, pullValue);
}

CFGLDRR3DECL(int) CFGLDRSetUInt64(CFGNODE hnode, const char *pszName, uint64_t ullValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetUInt64 (pszName, ullValue);
}

CFGLDRR3DECL(int) CFGLDRSetUInt64Ex(CFGNODE hnode, const char *pszName, uint64_t ullValue, unsigned int uiBase)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetUInt64 (pszName, ullValue, uiBase);
}

CFGLDRR3DECL(int) CFGLDRQueryInt32(CFGNODE hnode, const char *pszName, int32_t *plValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->QueryInt32 (pszName, plValue);
}

CFGLDRR3DECL(int) CFGLDRSetInt32(CFGNODE hnode, const char *pszName, int32_t lValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetInt32 (pszName, lValue);
}

CFGLDRR3DECL(int) CFGLDRSetInt32Ex(CFGNODE hnode, const char *pszName, int32_t lValue, unsigned int uiBase)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetInt32 (pszName, lValue, uiBase);
}

CFGLDRR3DECL(int) CFGLDRQueryInt64(CFGNODE hnode, const char *pszName, int64_t *pllValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->QueryInt64 (pszName, pllValue);
}

CFGLDRR3DECL(int) CFGLDRSetInt64(CFGNODE hnode, const char *pszName, int64_t llValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetInt64 (pszName, llValue);
}

CFGLDRR3DECL(int) CFGLDRSetInt64Ex(CFGNODE hnode, const char *pszName, int64_t llValue, unsigned int uiBase)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetInt64 (pszName, llValue, uiBase);
}

CFGLDRR3DECL(int) CFGLDRQueryUInt16(CFGNODE hnode, const char *pszName, uint16_t *puhValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!puhValue)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->QueryUInt16 (pszName, puhValue);
}

CFGLDRR3DECL(int) CFGLDRSetUInt16(CFGNODE hnode, const char *pszName, uint16_t uhValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetUInt16 (pszName, uhValue);
}

CFGLDRR3DECL(int) CFGLDRSetUInt16Ex(CFGNODE hnode, const char *pszName, uint16_t uhValue, unsigned int uiBase)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    return hnode->SetUInt16 (pszName, uhValue, uiBase);
}

CFGLDRR3DECL(int) CFGLDRQueryBin(CFGNODE hnode, const char *pszName, void *pvValue, unsigned cbValue, unsigned *pcbValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pcbValue)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->QueryBin (pszName, pvValue, cbValue, pcbValue);
}

CFGLDRR3DECL(int) CFGLDRSetBin(CFGNODE hnode, const char *pszName, void *pvValue, unsigned cbValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pvValue)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->SetBin (pszName, pvValue, cbValue);
}

CFGLDRR3DECL(int) CFGLDRQueryString(CFGNODE hnode, const char *pszName, char *pszValue, unsigned cbValue, unsigned *pcbValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pcbValue)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->QueryString (pszName, (void**)&pszValue, cbValue, pcbValue, false);
}

CFGLDRR3DECL(int) CFGLDRQueryBSTR(CFGNODE hnode, const char *pszName, BSTR *ppwszValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!ppwszValue)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->QueryString(pszName, (void**)ppwszValue, 0, NULL, true);
}

CFGLDRR3DECL(int) CFGLDRQueryUUID(CFGNODE hnode, const char *pszName, PRTUUID pUUID)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pUUID)
    {
        return VERR_INVALID_POINTER;
    }

    // we need it as UTF8
    unsigned size;
    int rc;
    rc = CFGLDRQueryString(hnode, pszName, NULL, 0, &size);
    if (rc == VERR_BUFFER_OVERFLOW)
    {
        char *uuidUtf8 = new char[size];
        rc = CFGLDRQueryString(hnode, pszName, uuidUtf8, size, &size);
        if (VBOX_SUCCESS(rc))
        {
            // remove the curly brackets
            uuidUtf8[strlen(uuidUtf8) - 1] = '\0';
            rc = RTUuidFromStr(pUUID, &uuidUtf8[1]);
        }
        delete[] uuidUtf8;
    }
    return rc;
}

CFGLDRR3DECL(int) CFGLDRSetUUID(CFGNODE hnode, const char *pszName, PCRTUUID pUuid)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pUuid)
    {
        return VERR_INVALID_HANDLE;
    }

    // UUID + curly brackets
    char strUuid[RTUUID_STR_LENGTH + 2 * sizeof(char)];
    strUuid[0] = '{';
    RTUuidToStr((const PRTUUID)pUuid, &strUuid[1], RTUUID_STR_LENGTH);
    strcat(strUuid, "}");
    return hnode->SetString (pszName, strUuid, strlen (strUuid), false);
}

CFGLDRR3DECL(int) CFGLDRSetString(CFGNODE hnode, const char *pszName, const char *pszValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pszValue)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->SetString (pszName, pszValue, strlen (pszValue), false);
}

CFGLDRR3DECL(int) CFGLDRSetBSTR(CFGNODE hnode, const char *pszName, const BSTR bstrValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!bstrValue)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->SetString (pszName, (char*)bstrValue, RTUtf16Len((PCRTUTF16)bstrValue), true);
}

CFGLDRR3DECL(int) CFGLDRQueryBool(CFGNODE hnode, const char *pszName, bool *pfValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pfValue)
    {
        return VERR_INVALID_POINTER;
    }

    return hnode->QueryBool (pszName, pfValue);
}

CFGLDRR3DECL(int) CFGLDRSetBool(CFGNODE hnode, const char *pszName, bool fValue)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }

    return hnode->SetBool (pszName, fValue);
}

CFGLDRR3DECL(int) CFGLDRQueryDateTime(CFGNODE hnode, const char *pszName, int64_t *pi64Value)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pi64Value)
    {
        return VERR_INVALID_POINTER;
    }

    /* query as UTF8 string */
    unsigned size = 0;
    int rc = CFGLDRQueryString(hnode, pszName, NULL, 0, &size);
    if (rc != VERR_BUFFER_OVERFLOW)
        return rc;

    char *pszValue = new char[size];
    char *pszBuf = new char[size];
    rc = CFGLDRQueryString(hnode, pszName, pszValue, size, &size);
    if (VBOX_SUCCESS(rc)) do
    {
        /* Parse xsd:dateTime. The format is:
         * '-'? yyyy '-' mm '-' dd 'T' hh ':' mm ':' ss ('.' s+)? (zzzzzz)?
         * where zzzzzz is: (('+' | '-') hh ':' mm) | 'Z' */
        uint32_t yyyy = 0;
        uint16_t mm = 0, dd = 0, hh = 0, mi = 0, ss = 0;
        if (sscanf(pszValue, "%d-%hu-%huT%hu:%hu:%hu%s",
                   &yyyy, &mm, &dd, &hh, &mi, &ss, pszBuf) != 7)
        {
            rc = VERR_PARSE_ERROR;
            break;
        }

        /* currently, we accept only the UTC timezone ('Z'),
         * ignoring fractional seconds, if present */
        if (pszBuf[0] == 'Z' ||
            (pszBuf[0] == '.' && pszBuf[strlen(pszBuf)-1] == 'Z'))
        {
            /* start with an error */
            rc = VERR_PARSE_ERROR;

            RTTIME time = { yyyy, (uint8_t) mm, 0, 0, (uint8_t) dd,
                            (uint8_t) hh, (uint8_t) mi, (uint8_t) ss, 0,
                            RTTIME_FLAGS_TYPE_UTC, 0 };
            if (RTTimeNormalize (&time))
            {
                RTTIMESPEC timeSpec;
                if (RTTimeImplode (&timeSpec, &time))
                {
                    *pi64Value = RTTimeSpecGetMilli (&timeSpec);
                    rc = VINF_SUCCESS;
                }
            }
        }
        else
            rc = VERR_PARSE_ERROR;
    }
    while (0);

    delete[] pszBuf;
    delete[] pszValue;

    return rc;
}

CFGLDRR3DECL(int) CFGLDRSetDateTime(CFGNODE hnode, const char *pszName, int64_t i64Value)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }

    RTTIMESPEC timeSpec;
    RTTimeSpecSetMilli (&timeSpec, i64Value);
    RTTIME time;
    if (!RTTimeExplode (&time, &timeSpec))
        return VERR_PARSE_ERROR;

    /* Store xsd:dateTime. The format is:
     * '-'? yyyy '-' mm '-' dd 'T' hh ':' mm ':' ss ('.' s+)? (zzzzzz)?
     * where zzzzzz is: (('+' | '-') hh ':' mm) | 'Z' */
    char aszBuf [256];
    RTStrPrintf(aszBuf, sizeof(aszBuf),
                "%04ld-%02hd-%02hdT%02hd:%02hd:%02hdZ",
                time.i32Year, (uint16_t) time.u8Month, (uint16_t) time.u8MonthDay,
                (uint16_t) time.u8Hour, (uint16_t) time.u8Minute, (uint16_t) time.u8Second);

    return hnode->SetString (pszName, aszBuf, strlen (aszBuf), false);
}

CFGLDRR3DECL(int) CFGLDRDeleteAttribute(CFGNODE hnode, const char *pszName)
{
    if (!hnode)
    {
        return VERR_INVALID_HANDLE;
    }
    if (!pszName)
    {
        return VERR_INVALID_POINTER;
    }
    return hnode->DeleteAttribute (pszName);
}

CFGLDRR3DECL(int) CFGLDRInitialize (void)
{
    int rc = VINF_SUCCESS;

    if (!initXML ())
    {
        rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

CFGLDRR3DECL(void) CFGLDRShutdown (void)
{
    /// @todo delete CfgLoaders
    terminateXML ();
}

#ifdef STANDALONE_TEST

#include <iprt/runtime.h>

int main(int argc, char **argv)
{
    Log(("Configuration loader standalone test\n"));

    CFGHANDLE hcfg = 0;
    CFGNODE hnode = 0;
    unsigned count = 0;
    unsigned i;
    char *cfgFilename = "vboxcfg.xml";
    char *cfgFilenameSaved = "testas.xml";

    /*
     * Initialize the VBox runtime without loading
     * the kernel support driver
     */
    int rc = RTR3Init(false);
    if (VBOX_FAILURE(rc))
    {
        Log(("RTInit failed %d\n", rc));
        goto l_exit_0;
    }

    rc = CFGLDRInitialize();
    if (VBOX_FAILURE(rc))
    {
        Log(("Initialize failed %d\n", rc));
        goto l_exit_0;
    }

    rc = CFGLDRLoad(&hcfg, cfgFilename, "vboxcfg.xsd");
    if (VBOX_FAILURE(rc))
    {
        Log(("Load failed %d\n", rc));
        goto l_exit_0;
    }

    printf("Configuration loaded from %s\n", cfgFilename);

    rc = CFGLDRCreateNode(hcfg, "Configuration/Something/DeviceManager/DeviceList", &hnode);
    if (VBOX_FAILURE(rc))
    {
        Log(("CreateNode failed %d\n", rc));
        goto l_exit_1;
    }
    rc = CFGLDRSetString(hnode, "GUID", "testtestte");
    rc = CFGLDRReleaseNode(hnode);

    rc = CFGLDRGetNode(hcfg, "Configuration/Managers/DeviceManager/DeviceList", 0, &hnode);
    if (VBOX_FAILURE(rc))
    {
        Log(("GetNode failed %d\n", rc));
        goto l_exit_1;
    }

    rc = CFGLDRCountChildren(hnode, "Device", &count);
    if (VBOX_FAILURE(rc))
    {
        Log(("CountChildren failed %d\n", rc));
        goto l_exit_2;
    }

    Log(("Number of child nodes %d\n", count));

    for (i = 0; i < count; i++)
    {
        CFGNODE hchild = 0;
        unsigned cbValue;
        char szValue[256];

        rc = CFGLDRGetChildNode(hnode, "Device", i, &hchild);
        if (VBOX_FAILURE(rc))
        {
            Log(("GetChildNode failed %d\n", rc));
            goto l_exit_2;
        }

        unsigned dummy;
        rc = CFGLDRCountChildren(hchild, NULL, &dummy);
        Log(("Number of child nodes of Device %d\n", dummy));

        int32_t value;

        rc = CFGLDRQueryInt32(hchild, "int", &value);
        Log(("Child node %d (rc = %d): int = %d\n", i, rc, value));

        rc = CFGLDRQueryString(hchild, NULL, szValue, sizeof (szValue), &cbValue);
        if (VBOX_FAILURE(rc))
        {
            Log(("QueryString failed %d\n", rc));
        }
        Log(("Child node %d: (length = %d) = '%s'\n", i, cbValue, szValue));

        rc = CFGLDRSetString(hchild, "GUID", "testtesttest");
        if (VBOX_FAILURE(rc))
        {
            Log(("SetString failed %d\n", rc));
        }

        rc = CFGLDRDeleteAttribute(hchild, "int");
        Log(("Attribute delete %d (rc = %d)\n", i, rc));

        CFGLDRSetBin(hchild, "Bin", (void *)CFGLDRSetBin, 100);
        CFGLDRSetInt32(hchild, "int32", 1973);
//        CFGLDRSetUInt64(hchild, "uint64", 0x1973);

        CFGNODE hnew = 0;
        CFGLDRCreateChildNode(hchild, "testnode", &hnew);
        rc = CFGLDRSetString(hchild, NULL, "her");
        if (VBOX_FAILURE(rc))
        {
            Log(("_SetString failed %d\n", rc));
        }
        rc = CFGLDRSetString(hnew, NULL, "neher");
        if (VBOX_FAILURE(rc))
        {
            Log(("+SetString failed %d\n", rc));
        }
        CFGLDRReleaseNode(hchild);
    }

    rc = CFGLDRSaveAs(hcfg, cfgFilenameSaved);
    if (VBOX_FAILURE(rc))
    {
        Log(("SaveAs failed %d\n", rc));
        goto l_exit_2;
    }

    Log(("Configuration saved as %s\n", cfgFilenameSaved));

l_exit_2:

    rc = CFGLDRReleaseNode(hnode);
    if (VBOX_FAILURE(rc))
    {
        Log(("ReleaseNode failed %d\n", rc));
    }

l_exit_1:

    rc = CFGLDRFree(hcfg);
    if (VBOX_FAILURE(rc))
    {
        Log(("Load failed %d\n", rc));
    }

l_exit_0:

    CFGLDRShutdown();

    Log(("Test completed."));
    return rc;
}
#endif
