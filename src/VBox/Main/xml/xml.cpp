/** @file
 * VirtualBox XML Manipulation API.
 */

/*
 * Copyright (C) 2007-2008 Sun Microsystems, Inc.
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

#include "Logging.h"

#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/lock.h>
#include <iprt/string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/globals.h>
#include <libxml/xmlIO.h>
#include <libxml/xmlsave.h>
#include <libxml/uri.h>

#include <libxml/xmlschemas.h>

#include <string>

#include "VBox/xml.h"


/**
 * Global module initialization structure.
 *
 * The constructor and destructor of this structure are used to perform global
 * module initiaizaton and cleanup. Thee must be only one global variable of
 * this structure.
 */
static
class Global
{
public:

    Global()
    {
        /* Check the parser version. The docs say it will kill the app if
         * there is a serious version mismatch, but I couldn't find it in the
         * source code (it only prints the error/warning message to the console) so
         * let's leave it as is for informational purposes. */
        LIBXML_TEST_VERSION

        /* Init libxml */
        xmlInitParser();

        /* Save the default entity resolver before someone has replaced it */
        sxml.defaultEntityLoader = xmlGetExternalEntityLoader();
    }

    ~Global()
    {
        /* Shutdown libxml */
        xmlCleanupParser();
    }

    struct
    {
        xmlExternalEntityLoader defaultEntityLoader;

        /** Used to provide some thread safety missing in libxml2 (see e.g.
         *  XmlTreeBackend::read()) */
        RTLockMtx lock;
    }
    sxml;  /* XXX naming this xml will break with gcc-3.3 */
}
gGlobal;



namespace xml
{

//////////////////////////////////////////////////////////////////////////////
// Exceptions
//////////////////////////////////////////////////////////////////////////////

LogicError::LogicError(RT_SRC_POS_DECL)
    : Error(NULL)
{
    char *msg = NULL;
    RTStrAPrintf(&msg, "In '%s', '%s' at #%d",
                 pszFunction, pszFile, iLine);
    setWhat(msg);
    RTStrFree(msg);
}

XmlError::XmlError(xmlErrorPtr aErr)
{
    if (!aErr)
        throw EInvalidArg (RT_SRC_POS);

    char *msg = Format (aErr);
    setWhat (msg);
    RTStrFree (msg);
}

/**
 * Composes a single message for the given error. The caller must free the
 * returned string using RTStrFree() when no more necessary.
 */
// static
char *XmlError::Format(xmlErrorPtr aErr)
{
    const char *msg = aErr->message ? aErr->message : "<none>";
    size_t msgLen = strlen (msg);
    /* strip spaces, trailing EOLs and dot-like char */
    while (msgLen && strchr (" \n.?!", msg [msgLen - 1]))
        -- msgLen;

    char *finalMsg = NULL;
    RTStrAPrintf (&finalMsg, "%.*s.\nLocation: '%s', line %d (%d), column %d",
                    msgLen, msg, aErr->file, aErr->line, aErr->int1, aErr->int2);

    return finalMsg;
}

EIPRTFailure::EIPRTFailure(int aRC)
    : RuntimeError(NULL),
      mRC(aRC)
{
    char *newMsg = NULL;
    RTStrAPrintf(&newMsg, "Runtime error: %d (%s)", aRC, RTErrGetShort(aRC));
    setWhat(newMsg);
    RTStrFree(newMsg);
}

//////////////////////////////////////////////////////////////////////////////
// File Class
//////////////////////////////////////////////////////////////////////////////

struct File::Data
{
    Data()
        : fileName (NULL), handle (NIL_RTFILE), opened (false) {}

    char *fileName;
    RTFILE handle;
    bool opened : 1;
};

File::File(Mode aMode, const char *aFileName)
    : m (new Data())
{
    m->fileName = RTStrDup (aFileName);
    if (m->fileName == NULL)
        throw ENoMemory();

    unsigned flags = 0;
    switch (aMode)
    {
        case Mode_Read:
            flags = RTFILE_O_READ;
            break;
        case Mode_Write:
            flags = RTFILE_O_WRITE | RTFILE_O_CREATE;
            break;
        case Mode_ReadWrite:
            flags = RTFILE_O_READ | RTFILE_O_WRITE;
    }

    int vrc = RTFileOpen (&m->handle, aFileName, flags);
    if (RT_FAILURE (vrc))
        throw EIPRTFailure (vrc);

    m->opened = true;
}

File::File (RTFILE aHandle, const char *aFileName /* = NULL */)
    : m (new Data())
{
    if (aHandle == NIL_RTFILE)
        throw EInvalidArg (RT_SRC_POS);

    m->handle = aHandle;

    if (aFileName)
    {
        m->fileName = RTStrDup (aFileName);
        if (m->fileName == NULL)
            throw ENoMemory();
    }

    setPos (0);
}

File::~File()
{
    if (m->opened)
        RTFileClose (m->handle);

    RTStrFree (m->fileName);
}

const char *File::uri() const
{
    return m->fileName;
}

uint64_t File::pos() const
{
    uint64_t p = 0;
    int vrc = RTFileSeek (m->handle, 0, RTFILE_SEEK_CURRENT, &p);
    if (RT_SUCCESS (vrc))
        return p;

    throw EIPRTFailure (vrc);
}

void File::setPos (uint64_t aPos)
{
    uint64_t p = 0;
    unsigned method = RTFILE_SEEK_BEGIN;
    int vrc = VINF_SUCCESS;

    /* check if we overflow int64_t and move to INT64_MAX first */
    if (((int64_t) aPos) < 0)
    {
        vrc = RTFileSeek (m->handle, INT64_MAX, method, &p);
        aPos -= (uint64_t) INT64_MAX;
        method = RTFILE_SEEK_CURRENT;
    }
    /* seek the rest */
    if (RT_SUCCESS (vrc))
        vrc = RTFileSeek (m->handle, (int64_t) aPos, method, &p);
    if (RT_SUCCESS (vrc))
        return;

    throw EIPRTFailure (vrc);
}

int File::read (char *aBuf, int aLen)
{
    size_t len = aLen;
    int vrc = RTFileRead (m->handle, aBuf, len, &len);
    if (RT_SUCCESS (vrc))
        return len;

    throw EIPRTFailure (vrc);
}

int File::write (const char *aBuf, int aLen)
{
    size_t len = aLen;
    int vrc = RTFileWrite (m->handle, aBuf, len, &len);
    if (RT_SUCCESS (vrc))
        return len;

    throw EIPRTFailure (vrc);

    return -1 /* failure */;
}

void File::truncate()
{
    int vrc = RTFileSetSize (m->handle, pos());
    if (RT_SUCCESS (vrc))
        return;

    throw EIPRTFailure (vrc);
}

//////////////////////////////////////////////////////////////////////////////
// MemoryBuf Class
//////////////////////////////////////////////////////////////////////////////

struct MemoryBuf::Data
{
    Data()
        : buf (NULL), len (0), uri (NULL), pos (0) {}

    const char *buf;
    size_t len;
    char *uri;

    size_t pos;
};

MemoryBuf::MemoryBuf (const char *aBuf, size_t aLen, const char *aURI /* = NULL */)
    : m (new Data())
{
    if (aBuf == NULL)
        throw EInvalidArg (RT_SRC_POS);

    m->buf = aBuf;
    m->len = aLen;
    m->uri = RTStrDup (aURI);
}

MemoryBuf::~MemoryBuf()
{
    RTStrFree (m->uri);
}

const char *MemoryBuf::uri() const
{
    return m->uri;
}

uint64_t MemoryBuf::pos() const
{
    return m->pos;
}

void MemoryBuf::setPos (uint64_t aPos)
{
    size_t pos = (size_t) aPos;
    if ((uint64_t) pos != aPos)
        throw EInvalidArg();

    if (pos > m->len)
        throw EInvalidArg();

    m->pos = pos;
}

int MemoryBuf::read (char *aBuf, int aLen)
{
    if (m->pos >= m->len)
        return 0 /* nothing to read */;

    size_t len = m->pos + aLen < m->len ? aLen : m->len - m->pos;
    memcpy (aBuf, m->buf + m->pos, len);
    m->pos += len;

    return len;
}

/*
 * GlobalLock
 *
 *
 */

struct GlobalLock::Data
{
    PFNEXTERNALENTITYLOADER pOldLoader;
    RTLock lock;

    Data()
        : pOldLoader(NULL),
          lock(gGlobal.sxml.lock)
    {
    }
};

GlobalLock::GlobalLock()
    : m(new Data())
{
}

GlobalLock::~GlobalLock()
{
    if (m->pOldLoader)
        xmlSetExternalEntityLoader(m->pOldLoader);
}

void GlobalLock::setExternalEntityLoader(PFNEXTERNALENTITYLOADER pLoader)
{
    m->pOldLoader = xmlGetExternalEntityLoader();
    xmlSetExternalEntityLoader(pLoader);
}

// static
xmlParserInput* GlobalLock::callDefaultLoader(const char *aURI,
                                              const char *aID,
                                              xmlParserCtxt *aCtxt)
{
    return gGlobal.sxml.defaultEntityLoader(aURI, aID, aCtxt);
}

/*
 * XmlParserBase
 *
 *
 */

XmlParserBase::XmlParserBase()
{
    m_ctxt = xmlNewParserCtxt();
    if (m_ctxt == NULL)
        throw ENoMemory();
}

XmlParserBase::~XmlParserBase()
{
    xmlFreeParserCtxt (m_ctxt);
    m_ctxt = NULL;
}

/*
 * XmlFileParser
 *
 *
 */

struct XmlFileParser::Data
{
    xmlParserCtxtPtr ctxt;
    std::string strXmlFilename;

    Data()
    {
        if (!(ctxt = xmlNewParserCtxt()))
            throw xml::ENoMemory();
    }

    ~Data()
    {
        xmlFreeParserCtxt(ctxt);
        ctxt = NULL;
    }
};

XmlFileParser::XmlFileParser()
    : XmlParserBase(),
      m(new Data())
{
}

XmlFileParser::~XmlFileParser()
{
}

struct ReadContext
{
    File file;
    std::string error;

    ReadContext(const char *pcszFilename)
        : file(File::Mode_Read, pcszFilename)
    {
    }

    void setError(const xml::Error &x)
    {
        error = x.what();
    }

    void setError(const std::exception &x)
    {
        error = x.what();
    }
};

void XmlFileParser::read(const char *pcszFilename)
{
    GlobalLock lock();
//     global.setExternalEntityLoader(ExternalEntityLoader);

    xmlDocPtr doc = NULL;
    ReadContext *pContext = NULL;

    m->strXmlFilename = pcszFilename;

    try
    {
        pContext = new ReadContext(pcszFilename);
        doc = xmlCtxtReadIO(m->ctxt,
                            ReadCallback,
                            CloseCallback,
                            pContext,
                            pcszFilename,
                            NULL,       // encoding
                            XML_PARSE_NOBLANKS);
        if (doc == NULL)
        {
            throw XmlError(xmlCtxtGetLastError(m->ctxt));
        }

        xmlFreeDoc(doc);
        doc = NULL;

        delete pContext;
        pContext = NULL;
    }
    catch (...)
    {
        if (doc != NULL)
            xmlFreeDoc(doc);

        if (pContext)
            delete pContext;

        throw;
    }
}

// static
int XmlFileParser::ReadCallback(void *aCtxt, char *aBuf, int aLen)
{
    ReadContext *pContext = static_cast<ReadContext*>(aCtxt);

    /* To prevent throwing exceptions while inside libxml2 code, we catch
     * them and forward to our level using a couple of variables. */

    try
    {
        return pContext->file.read(aBuf, aLen);
    }
    catch (const xml::EIPRTFailure &err) { pContext->setError(err); }
    catch (const xml::Error &err) { pContext->setError(err); }
    catch (const std::exception &err) { pContext->setError(err); }
    catch (...) { pContext->setError(xml::LogicError(RT_SRC_POS)); }

    return -1 /* failure */;
}

int XmlFileParser::CloseCallback(void *aCtxt)
{
    /// @todo to be written

    return -1;
}


} // end namespace xml


