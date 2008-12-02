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

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/globals.h>
#include <libxml/xmlIO.h>
#include <libxml/xmlsave.h>
#include <libxml/uri.h>

#include <libxml/xmlschemas.h>

#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <string.h>

#include "VBox/vboxxml.h"

namespace vboxxml
{

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

File::File (Mode aMode, const char *aFileName)
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
 * VBoxXml
 *
 *
 */

VBoxXmlBase::VBoxXmlBase()
{
    m_ctxt = xmlNewParserCtxt();
    if (m_ctxt == NULL)
        throw ENoMemory();
}

VBoxXmlBase::~VBoxXmlBase()
{
    xmlFreeParserCtxt (m_ctxt);
    m_ctxt = NULL;
}


int ReadCallback (void *aCtxt, char *aBuf, int aLen)
{
    return -1 /* failure */;
}


VBoxXmlFile::VBoxXmlFile()
    : VBoxXmlBase()
{
    xmlDocPtr doc;

}

VBoxXmlFile::~VBoxXmlFile()
{
}

} // namespace vboxxml


