/** @file
 * Settings File Manipulation API.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBox/settings.h"

#include "Logging.h"

#include <iprt/err.h>
#include <iprt/file.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/globals.h>
#include <libxml/xmlIO.h>
#include <libxml/xmlsave.h>
#include <libxml/uri.h>

#include <libxml/xmlschemas.h>

#include <string.h>


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
        xml.defaultEntityLoader = xmlGetExternalEntityLoader();
    }

    ~Global()
    {
        /* Shutdown libxml */
        xmlCleanupParser();
    }

    struct
    {
        xmlExternalEntityLoader defaultEntityLoader;
    }
    xml;
}
gGlobal;


namespace settings
{

// Helpers
////////////////////////////////////////////////////////////////////////////////

inline int sFromHex (char aChar)
{
    if (aChar >= '0' && aChar <= '9')
        return aChar - '0';
    if (aChar >= 'A' && aChar <= 'F')
        return aChar - 'A' + 0xA;
    if (aChar >= 'a' && aChar <= 'f')
        return aChar - 'a' + 0xA;

    throw ENoConversion (FmtStr ("'%c' (0x%02X) is not hex", aChar, aChar));
}

inline char sToHex (int aDigit)
{
    return (aDigit < 0xA) ? aDigit + '0' : aDigit - 0xA + 'A';
}

static char *duplicate_chars (const char *that)
{
    char *result = NULL;
    if (that != NULL)
    {
        size_t len = strlen (that) + 1;
        result = new char [len];
        if (result != NULL)
            memcpy (result, that, len);
    }
    return result;
}

//////////////////////////////////////////////////////////////////////////////
// string -> type conversions
//////////////////////////////////////////////////////////////////////////////

uint64_t FromStringInteger (const char *aValue, bool aSigned,
                            int aBits, uint64_t aMin, uint64_t aMax)
{
    if (aValue == NULL)
        throw ENoValue();

    switch (aBits)
    {
        case 8:
        case 16:
        case 32:
        case 64:
            break;
        default:
            throw ENotImplemented (RT_SRC_POS);
    }

    if (aSigned)
    {
        int64_t result;
        int vrc = RTStrToInt64Full (aValue, 0, &result);
        if (RT_SUCCESS (vrc))
        {
            if (result >= (int64_t) aMin && result <= (int64_t) aMax)
                return (uint64_t) result;
        }
    }
    else
    {
        uint64_t result;
        int vrc = RTStrToUInt64Full (aValue, 0, &result);
        if (RT_SUCCESS (vrc))
        {
            if (result >= aMin && result <= aMax)
                return result;
        }
    }

    throw ENoConversion (FmtStr ("'%s' is not integer", aValue));
}

template<> bool FromString <bool> (const char *aValue)
{
    if (aValue == NULL)
        throw ENoValue();

    if (strcmp (aValue, "true") == 0 ||
        strcmp (aValue, "yes") == 0 ||
        strcmp (aValue, "on") == 0)
        return true;
    else if (strcmp (aValue, "false") == 0 ||
             strcmp (aValue, "no") == 0 ||
             strcmp (aValue, "off") == 0)
        return false;

    throw ENoConversion (FmtStr ("'%s' is not bool", aValue));
}

template<> RTTIMESPEC FromString <RTTIMESPEC> (const char *aValue)
{
    if (aValue == NULL)
        throw ENoValue();

    /* Parse ISO date (xsd:dateTime). The format is:
     * '-'? yyyy '-' mm '-' dd 'T' hh ':' mm ':' ss ('.' s+)? (zzzzzz)?
     * where zzzzzz is: (('+' | '-') hh ':' mm) | 'Z' */
    uint32_t yyyy = 0;
    uint16_t mm = 0, dd = 0, hh = 0, mi = 0, ss = 0;
    char buf [256];
    if (strlen (aValue) > ELEMENTS (buf) - 1 ||
        sscanf (aValue, "%d-%hu-%huT%hu:%hu:%hu%s",
                &yyyy, &mm, &dd, &hh, &mi, &ss, buf) == 7)
    {
        /* currently, we accept only the UTC timezone ('Z'),
         * ignoring fractional seconds, if present */
        if (buf [0] == 'Z' ||
            (buf [0] == '.' && buf [strlen (buf) - 1] == 'Z'))
        {
            RTTIME time = { yyyy, (uint8_t) mm, 0, 0, (uint8_t) dd,
                            (uint8_t) hh, (uint8_t) mi, (uint8_t) ss, 0,
                            RTTIME_FLAGS_TYPE_UTC };
            if (RTTimeNormalize (&time))
            {
                RTTIMESPEC timeSpec;
                if (RTTimeImplode (&timeSpec, &time))
                    return timeSpec;
            }
        }
        else
            throw ENoConversion (FmtStr ("'%s' is not UTC date", aValue));
    }

    throw ENoConversion (FmtStr ("'%s' is not ISO date", aValue));
}

stdx::char_auto_ptr FromString (const char *aValue, size_t *aLen)
{
    if (aValue == NULL)
        throw ENoValue();

    /* each two chars produce one byte */
    size_t len = strlen (aValue) / 2;

    /* therefore, the original length must be even */
    if (len % 2 != 0)
        throw ENoConversion (FmtStr ("'%.*s' is not binary data",
                                     aLen, aValue));

    stdx::char_auto_ptr result (new char [len]);

    const char *src = aValue;
    char *dst = result.get();

    for (size_t i = 0; i < len; ++ i, ++ dst)
    {
        *dst = sFromHex (*src ++) << 4;
        *dst |= sFromHex (*src ++);
    }

    if (aLen != NULL)
        *aLen = len;

    return result;
}

//////////////////////////////////////////////////////////////////////////////
// type -> string conversions
//////////////////////////////////////////////////////////////////////////////

stdx::char_auto_ptr ToStringInteger (uint64_t aValue, unsigned int aBase,
                                     bool aSigned, int aBits)
{
    unsigned int flags = RTSTR_F_SPECIAL;
    if (aSigned)
        flags |= RTSTR_F_VALSIGNED;

    /* maximum is binary representation + terminator */
    size_t len = aBits + 1;

    switch (aBits)
    {
        case 8:
            flags = RTSTR_F_8BIT;
            break;
        case 16:
            flags = RTSTR_F_16BIT;
            break;
        case 32:
            flags = RTSTR_F_32BIT;
            break;
        case 64:
            flags = RTSTR_F_64BIT;
            break;
        default:
            throw ENotImplemented (RT_SRC_POS);
    }

    stdx::char_auto_ptr result (new char [len]);
    int vrc = RTStrFormatNumber (result.get(), aValue, aBase, 0, 0, flags);
    if (RT_SUCCESS (vrc))
        return result;

    throw EIPRTFailure (vrc);
}

template<> stdx::char_auto_ptr ToString <bool> (const bool &aValue,
                                                unsigned int aExtra /* = 0 */)
{
    stdx::char_auto_ptr result (duplicate_chars (aValue ? "true" : "false"));
    return result;
}

template<> stdx::char_auto_ptr ToString <RTTIMESPEC> (const RTTIMESPEC &aValue,
                                                      unsigned int aExtra /* = 0 */)
{
    RTTIME time;
    if (!RTTimeExplode (&time, &aValue))
        throw ENoConversion (FmtStr ("timespec %lld ms is invalid",
                                     RTTimeSpecGetMilli (&aValue)));

    /* Store ISO date (xsd:dateTime). The format is:
     * '-'? yyyy '-' mm '-' dd 'T' hh ':' mm ':' ss ('.' s+)? (zzzzzz)?
     * where zzzzzz is: (('+' | '-') hh ':' mm) | 'Z' */
    char buf [256];
    RTStrPrintf (buf, sizeof (buf),
                "%04ld-%02hd-%02hdT%02hd:%02hd:%02hdZ",
                time.i32Year, (uint16_t) time.u8Month, (uint16_t) time.u8MonthDay,
                (uint16_t) time.u8Hour, (uint16_t) time.u8Minute, (uint16_t) time.u8Second);

    stdx::char_auto_ptr result (duplicate_chars (buf));
    return result;
}

stdx::char_auto_ptr ToString (const char *aData, size_t aLen)
{
    /* each byte will produce two hex digits and there will be a null
     * terminator */
    stdx::char_auto_ptr result (new char [aLen * 2 + 1]);

    const char *src = aData;
    char *dst =  result.get();

    for (size_t i = 0; i < aLen; ++ i, ++ src)
    {
        *dst++ = sToHex ((*src) >> 4);
        *dst++ = sToHex ((*src) & 0xF);
    }

    *dst = '\0';

    return result;
}

//////////////////////////////////////////////////////////////////////////////
// File Class
//////////////////////////////////////////////////////////////////////////////

struct File::Data
{
    Data()
        : fileName (NULL), handle (NIL_RTFILE), opened (false) {}

    Mode mode;
    char *fileName;
    RTFILE handle;
    bool opened : 1;
};

File::File (Mode aMode, const char *aFileName)
    : m (new Data())
{
    m->mode = aMode;

    m->fileName = RTStrDup (aFileName);
    if (m->fileName == NULL)
        throw ENoMemory();

    unsigned flags = 0;
    switch (aMode)
    {
        case Read:
            flags = RTFILE_O_READ;
            break;
        case Write:
            flags = RTFILE_O_WRITE | RTFILE_O_CREATE;
            break;
        case ReadWrite:
            flags = RTFILE_O_READ | RTFILE_O_WRITE;
    }

    int vrc = RTFileOpen (&m->handle, aFileName, flags);
    if (RT_FAILURE (vrc))
        throw EIPRTFailure (vrc);

    m->opened = true;
}

File::File (Mode aMode, RTFILE aHandle, const char *aFileName /* = NULL */ )
    : m (new Data())
{
    if (aHandle == NIL_RTFILE)
        throw EInvalidArg (RT_SRC_POS);

    m->mode = aMode;
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

//////////////////////////////////////////////////////////////////////////////
// XmlKeyBackend Class
//////////////////////////////////////////////////////////////////////////////

class XmlKeyBackend : public Key::Backend
{
public:

    XmlKeyBackend (xmlNodePtr aNode);
    ~XmlKeyBackend();

    const char *name() const;
    void setName (const char *aName);
    const char *value (const char *aName) const;
    void setValue (const char *aName, const char *aValue);

    Key::List keys (const char *aName = NULL) const;
    Key findKey (const char *aName) const;

    Key appendKey (const char *aName);
    void zap();

    void *position() const { return mNode; }

private:

    xmlNodePtr mNode;

    xmlChar *mNodeText;

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (XmlKeyBackend);

    friend class XmlTreeBackend;
};

XmlKeyBackend::XmlKeyBackend (xmlNodePtr aNode)
    : mNode (aNode), mNodeText (NULL)
{
    AssertReturnVoid (mNode);
    AssertReturnVoid (mNode->type == XML_ELEMENT_NODE);
}

XmlKeyBackend::~XmlKeyBackend()
{
    xmlFree (mNodeText);
}

const char *XmlKeyBackend::name() const
{
    return mNode ? (char *) mNode->name : NULL;
}

void XmlKeyBackend::setName (const char *aName)
{
    throw ENotImplemented (RT_SRC_POS);
}

const char *XmlKeyBackend::value (const char *aName) const
{
    if (!mNode)
        return NULL;

    if (aName == NULL)
    {
        /* @todo xmlNodeListGetString (,,1) returns NULL for things like
         * <Foo></Foo> and may want to return "" in this case to distinguish
         * from <Foo/> (where NULL is pretty much expected). */
        if (!mNodeText)
            unconst (mNodeText) =
                xmlNodeListGetString (mNode->doc, mNode->children, 0);
        return (char *) mNodeText;
    }

    xmlAttrPtr attr = xmlHasProp (mNode, (const xmlChar *) aName);
    if (!attr)
        return NULL;

    if (attr->type == XML_ATTRIBUTE_NODE)
    {
        /* @todo for now, we only understand the most common case: only 1 text
         * node comprises the attribute's contents. Otherwise we'd need to
         * return a newly allocated string buffer to the caller that
         * concatenates all text nodes and obey him to free it or provide our
         * own internal map of attribute=value pairs and return const pointers
         * to values from this map. */
        if (attr->children != NULL &&
            attr->children->next == NULL &&
            (attr->children->type == XML_TEXT_NODE ||
             attr->children->type == XML_CDATA_SECTION_NODE))
            return (char *) attr->children->content;
    }
    else if (attr->type == XML_ATTRIBUTE_DECL)
    {
        return (char *) ((xmlAttributePtr) attr)->defaultValue;
    }

    return NULL;
}

void XmlKeyBackend::setValue (const char *aName, const char *aValue)
{
    if (!mNode)
        return;

    if (aName == NULL)
    {
        xmlChar *value = (xmlChar *) aValue;
        if (value != NULL)
        {
            value = xmlEncodeSpecialChars (mNode->doc, value);
            if (value == NULL)
                throw ENoMemory();
        }

        xmlNodeSetContent (mNode, value);

        if (value != (xmlChar *) aValue)
            xmlFree (value);

        /* outdate the node text holder */
        if (mNodeText != NULL)
        {
            xmlFree (mNodeText);
            mNodeText = NULL;
        }

        return;
    }

    if (aValue == NULL)
    {
        /* remove the attribute if it exists */
        xmlAttrPtr attr = xmlHasProp (mNode, (const xmlChar *) aName);
        if (attr != NULL)
        {
            int rc = xmlRemoveProp (attr);
            if (rc != 0)
                throw EInvalidArg (RT_SRC_POS);
        }
        return;
    }

    xmlAttrPtr attr = xmlSetProp (mNode, (const xmlChar *) aName,
                                  (const xmlChar *) aValue);
    if (attr == NULL)
        throw ENoMemory();
}

Key::List XmlKeyBackend::keys (const char *aName /* = NULL */) const
{
    Key::List list;

    if (!mNode)
        return list;

    for (xmlNodePtr node = mNode->children; node; node = node->next)
    {
        if (node->type == XML_ELEMENT_NODE)
        {
            if (aName == NULL ||
                strcmp (aName, (char *) node->name) == 0)
                list.push_back (Key (new XmlKeyBackend (node)));
        }
    }

    return list;
}

Key XmlKeyBackend::findKey (const char *aName) const
{
    Key key;

    if (!mNode)
        return key;

    for (xmlNodePtr node = mNode->children; node; node = node->next)
    {
        if (node->type == XML_ELEMENT_NODE)
        {
            if (aName == NULL ||
                strcmp (aName, (char *) node->name) == 0)
            {
                key = Key (new XmlKeyBackend (node));
                break;
            }
        }
    }

    return key;
}

Key XmlKeyBackend::appendKey (const char *aName)
{
    if (!mNode)
        return Key();

    xmlNodePtr node = xmlNewChild (mNode, NULL, (const xmlChar *) aName, NULL);
    if (node == NULL)
        throw ENoMemory();

    return Key (new XmlKeyBackend (node));
}

void XmlKeyBackend::zap()
{
    if (!mNode)
        return;

    xmlUnlinkNode (mNode);
    xmlFreeNode (mNode);
    mNode = NULL;
}

//////////////////////////////////////////////////////////////////////////////
// XmlTreeBackend Class
//////////////////////////////////////////////////////////////////////////////

class XmlTreeBackend::XmlError : public XmlTreeBackend::Error
{
public:

    XmlError (xmlErrorPtr aErr)
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
    static char *Format (xmlErrorPtr aErr)
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
};

struct XmlTreeBackend::Data
{
    Data() : ctxt (NULL), doc (NULL)
           , inputResolver (NULL) {}

    xmlParserCtxtPtr ctxt;
    xmlDocPtr doc;

    Key root;

    InputResolver *inputResolver;

    std::auto_ptr <stdx::exception_trap_base> trappedErr;

    /**
     * This is to avoid throwing exceptions while in libxml2 code and
     * redirect them to our level instead. Also used to perform clean up
     * by deleting the I/O stream instance and self when requested.
     */
    struct IOCtxt
    {
        IOCtxt (Stream *aStream, std::auto_ptr <stdx::exception_trap_base> &aErr)
            : stream (aStream), deleteStreamOnClose (false)
            , err (aErr) {}

        template <typename T>
        void setErr (const T& aErr) { err.reset (new stdx::exception_trap <T> (aErr)); }

        void resetErr() { err.reset(); }

        Stream *stream;
        bool deleteStreamOnClose;

        std::auto_ptr <stdx::exception_trap_base> &err;
    };

    struct InputCtxt : public IOCtxt
    {
        InputCtxt (Input *aInput, std::auto_ptr <stdx::exception_trap_base> &aErr)
            : IOCtxt (aInput, aErr), input (aInput) {}

        Input *input;
    };

    struct OutputCtxt : public IOCtxt
    {
        OutputCtxt (Output *aOutput, std::auto_ptr <stdx::exception_trap_base> &aErr)
            : IOCtxt (aOutput, aErr), output (aOutput) {}

        Output *output;
    };
};

XmlTreeBackend::XmlTreeBackend()
    : m (new Data())
{
    /* create a parser context */
    m->ctxt = xmlNewParserCtxt();
    if (m->ctxt == NULL)
        throw ENoMemory();
}

XmlTreeBackend::~XmlTreeBackend()
{
    reset();

    xmlFreeParserCtxt (m->ctxt);
    m->ctxt = NULL;
}

void XmlTreeBackend::setInputResolver (InputResolver &aResolver)
{
    m->inputResolver = &aResolver;
}

void XmlTreeBackend::resetInputResolver()
{
    m->inputResolver = NULL;
}

void XmlTreeBackend::rawRead (Input &aInput, const char *aSchema /* = NULL */,
                              int aFlags /* = 0 */)
{
    /* Reset error variables used to memorize exceptions while inside the
     * libxml2 code. */
    m->trappedErr.reset();

    /* Set up an input stream for parsing the document. This will be deleted
     * when the stream is closed by the libxml2 API (e.g. when calling
     * xmlFreeParserCtxt()). */
    Data::InputCtxt *inputCtxt =
        new Data::InputCtxt (&aInput, m->trappedErr);

    /* Set up the external entity resolver. Note that we do it in a
     * thread-unsafe fashion because this stuff is not thread-safe in libxml2.
     * Making it thread-safe would require a) guarding this method with a
     * mutex and b) requiring our API caller not to use libxml2 on some other
     * thread (which is not practically possible). So, our API is not
     * thread-safe for now. */
    xmlExternalEntityLoader oldEntityLoader = xmlGetExternalEntityLoader();
    sThat = this;
    xmlSetExternalEntityLoader (ExternalEntityLoader);

    /* Note: when parsing we use XML_PARSE_NOBLANKS to instruct libxml2 to
     * remove text nodes that contain only blanks. This is important because
     * otherwise xmlSaveDoc() won't be able to do proper indentation on
     * output. */

    /* parse the stream */
    xmlDocPtr doc = xmlCtxtReadIO (m->ctxt,
                                   ReadCallback, CloseCallback,
                                   inputCtxt, aInput.uri(), NULL,
                                   XML_PARSE_NOBLANKS);
    if (doc == NULL)
    {
        /* restore the previous entity resolver */
        xmlSetExternalEntityLoader (oldEntityLoader);
        sThat = NULL;

        /* look if there was a forwared exception from the lower level */
        if (m->trappedErr.get() != NULL)
            m->trappedErr->rethrow();

        throw XmlError (xmlCtxtGetLastError (m->ctxt));
    }

    if (aSchema != NULL)
    {
        xmlSchemaParserCtxtPtr schemaCtxt = NULL;
        xmlSchemaPtr schema = NULL;
        xmlSchemaValidCtxtPtr validCtxt = NULL;
        char *errorStr = NULL;

        /* validate the document */
        try
        {
            bool valid = false;

            schemaCtxt = xmlSchemaNewParserCtxt (aSchema);
            if (schemaCtxt == NULL)
                throw LogicError (RT_SRC_POS);

            /* set our error handlers */
            xmlSchemaSetParserErrors (schemaCtxt, ValidityErrorCallback, 
                                      ValidityWarningCallback, &errorStr);
            xmlSchemaSetParserStructuredErrors (schemaCtxt,
                                                StructuredErrorCallback,
                                                &errorStr);
            /* load schema */
            schema = xmlSchemaParse (schemaCtxt);
            if (schema != NULL)
            {
                validCtxt = xmlSchemaNewValidCtxt (schema);
                if (validCtxt == NULL)
                    throw LogicError (RT_SRC_POS);

                /* instruct to create default attribute's values in the document */
                if (aFlags & Read_AddDefaults)
                    xmlSchemaSetValidOptions (validCtxt, XML_SCHEMA_VAL_VC_I_CREATE);

                /* set our error handlers */
                xmlSchemaSetValidErrors (validCtxt, ValidityErrorCallback,
                                         ValidityWarningCallback, &errorStr);

                /* finally, validate */
                valid = xmlSchemaValidateDoc (validCtxt, doc) == 0;
            }

            if (!valid)
            {
                /* look if there was a forwared exception from the lower level */
                if (m->trappedErr.get() != NULL)
                    m->trappedErr->rethrow();

                if (errorStr == NULL)
                    throw LogicError (RT_SRC_POS);

                throw Error (errorStr);
                /* errorStr is freed in catch(...) below */
            }

            RTStrFree (errorStr);

            xmlSchemaFreeValidCtxt (validCtxt);
            xmlSchemaFree (schema);
            xmlSchemaFreeParserCtxt (schemaCtxt);
        }
        catch (...)
        {
            /* restore the previous entity resolver */
            xmlSetExternalEntityLoader (oldEntityLoader);
            sThat = NULL;

            RTStrFree (errorStr);

            if (validCtxt)
                xmlSchemaFreeValidCtxt (validCtxt);
            if (schema)
                xmlSchemaFree (schema);
            if (schemaCtxt)
                xmlSchemaFreeParserCtxt (schemaCtxt);

            throw;
        }
    }

    /* restore the previous entity resolver */
    xmlSetExternalEntityLoader (oldEntityLoader);
    sThat = NULL;

    /* reset the previous tree on success */
    reset();

    m->doc = doc;
    /* assign the root key */
    m->root = Key (new XmlKeyBackend (xmlDocGetRootElement (m->doc)));
}

void XmlTreeBackend::rawWrite (Output &aOutput)
{
    /* reset error variables used to memorize exceptions while inside the
     * libxml2 code */
    m->trappedErr.reset();

    /* set up an input stream for parsing the document. This will be deleted
     * when the stream is closed by the libxml2 API (e.g. when calling
     * xmlFreeParserCtxt()). */
    Data::OutputCtxt *outputCtxt =
        new Data::OutputCtxt (&aOutput, m->trappedErr);

    /* serialize to the stream */

    xmlIndentTreeOutput = 1;
    xmlTreeIndentString = "  ";
    xmlSaveNoEmptyTags = 0;

    xmlSaveCtxtPtr saveCtxt = xmlSaveToIO (WriteCallback, CloseCallback,
                                           outputCtxt, NULL,
                                           XML_SAVE_FORMAT);
    if (saveCtxt == NULL)
        throw LogicError (RT_SRC_POS);

    long rc = xmlSaveDoc (saveCtxt, m->doc);
    if (rc == -1)
    {
        /* look if there was a forwared exception from the lower level */
        if (m->trappedErr.get() != NULL)
            m->trappedErr->rethrow();

        /* there must be an exception from the Output implementation,
         * otherwise the save operation must always succeed. */
        throw LogicError (RT_SRC_POS);
    }

    xmlSaveClose (saveCtxt);
}

void XmlTreeBackend::reset()
{
    if (m->doc)
    {
        /* reset the root key's node */
        GetKeyBackend (m->root)->mNode = NULL;
        /* free the document*/
        xmlFreeDoc (m->doc);
        m->doc = NULL;
    }
}

Key &XmlTreeBackend::rootKey() const
{
    return m->root;
}

/* static */
int XmlTreeBackend::ReadCallback (void *aCtxt, char *aBuf, int aLen)
{
    AssertReturn (aCtxt != NULL, 0);

    Data::InputCtxt *ctxt = static_cast <Data::InputCtxt *> (aCtxt);

    /* To prevent throwing exceptions while inside libxml2 code, we catch
     * them and forward to our level using a couple of variables. */
    try
    {
        return ctxt->input->read (aBuf, aLen);
    }
    catch (const EIPRTFailure &err) { ctxt->setErr (err); }
    catch (const settings::Error &err) { ctxt->setErr (err); }
    catch (const std::exception &err) { ctxt->setErr (err); }
    catch (...) { ctxt->setErr (LogicError (RT_SRC_POS)); }

    return -1 /* failure */;
}

/* static */
int XmlTreeBackend::WriteCallback (void *aCtxt, const char *aBuf, int aLen)
{
    AssertReturn (aCtxt != NULL, 0);

    Data::OutputCtxt *ctxt = static_cast <Data::OutputCtxt *> (aCtxt);

    /* To prevent throwing exceptions while inside libxml2 code, we catch
     * them and forward to our level using a couple of variables. */
    try
    {
        return ctxt->output->write (aBuf, aLen);
    }
    catch (const EIPRTFailure &err) { ctxt->setErr (err); }
    catch (const settings::Error &err) { ctxt->setErr (err); }
    catch (const std::exception &err) { ctxt->setErr (err); }
    catch (...) { ctxt->setErr (LogicError (RT_SRC_POS)); }

    return -1 /* failure */;
}

/* static */
int XmlTreeBackend::CloseCallback (void *aCtxt)
{
    AssertReturn (aCtxt != NULL, 0);

    Data::IOCtxt *ctxt = static_cast <Data::IOCtxt *> (aCtxt);

    /* To prevent throwing exceptions while inside libxml2 code, we catch
     * them and forward to our level using a couple of variables. */
    try
    {
        /// @todo there is no explicit close semantics in Stream yet
#if 0
        ctxt->stream->close();
#endif

        /* perform cleanup when necessary */ 
        if (ctxt->deleteStreamOnClose)
            delete ctxt->stream;

        delete ctxt;

        return 0 /* success */;
    }
    catch (const EIPRTFailure &err) { ctxt->setErr (err); }
    catch (const settings::Error &err) { ctxt->setErr (err); }
    catch (const std::exception &err) { ctxt->setErr (err); }
    catch (...) { ctxt->setErr (LogicError (RT_SRC_POS)); }

    return -1 /* failure */;
}

/* static */
void XmlTreeBackend::ValidityErrorCallback (void *aCtxt, const char *aMsg, ...)
{
    AssertReturnVoid (aCtxt != NULL);
    AssertReturnVoid (aMsg != NULL);

    char * &str = *(char * *) aCtxt;

    char *newMsg = NULL;
    {
        va_list args;
        va_start (args, aMsg);
        RTStrAPrintfV (&newMsg, aMsg, args);
        va_end (args);
    }

    AssertReturnVoid (newMsg != NULL);

    /* strip spaces, trailing EOLs and dot-like char */
    size_t newMsgLen = strlen (newMsg);
    while (newMsgLen && strchr (" \n.?!", newMsg [newMsgLen - 1]))
        -- newMsgLen;

    if (str == NULL)
    {
        str = newMsg;
        newMsg [newMsgLen] = '\0';
    }
    else
    {
        /* append to the existing string */
        size_t strLen = strlen (str);
        char *newStr = (char *) RTMemRealloc (str, strLen + 2 + newMsgLen + 1);
        AssertReturnVoid (newStr != NULL);

        memcpy (newStr + strLen, ".\n", 2);
        memcpy (newStr + strLen + 2, newMsg, newMsgLen);
        newStr [strLen + 2 + newMsgLen] = '\0';
        str = newStr;
        RTStrFree (newMsg);
    }
}

/* static */
void XmlTreeBackend::ValidityWarningCallback (void *aCtxt, const char *aMsg, ...)
{
    NOREF (aCtxt);
    NOREF (aMsg);
}

/* static */
void XmlTreeBackend::StructuredErrorCallback (void *aCtxt, xmlErrorPtr aErr)
{
    AssertReturnVoid (aCtxt != NULL);
    AssertReturnVoid (aErr != NULL);

    char * &str = *(char * *) aCtxt;

    char *newMsg = XmlError::Format (aErr);
    AssertReturnVoid (newMsg != NULL);

    if (str == NULL)
        str = newMsg;
    else
    {
        /* append to the existing string */
        size_t newMsgLen = strlen (newMsg);
        size_t strLen = strlen (str);
        char *newStr = (char *) RTMemRealloc (str, strLen + newMsgLen + 2);
        AssertReturnVoid (newStr != NULL);

        memcpy (newStr + strLen, ".\n", 2);
        memcpy (newStr + strLen + 2, newMsg, newMsgLen);
        str = newStr;
        RTStrFree (newMsg);
    }
}

/* static */
XmlTreeBackend *XmlTreeBackend::sThat = NULL;

/* static */
xmlParserInputPtr XmlTreeBackend::ExternalEntityLoader (const char *aURI, 
                                                        const char *aID, 
                                                        xmlParserCtxtPtr aCtxt)
{
    AssertReturn (sThat != NULL, NULL);

    if (sThat->m->inputResolver == NULL)
        return gGlobal.xml.defaultEntityLoader (aURI, aID, aCtxt);

    /* To prevent throwing exceptions while inside libxml2 code, we catch
     * them and forward to our level using a couple of variables. */
    try
    {
        Input *input = sThat->m->inputResolver->resolveEntity (aURI, aID);
        if (input == NULL)
            return NULL;

        Data::InputCtxt *ctxt = new Data::InputCtxt (input, sThat->m->trappedErr);
        ctxt->deleteStreamOnClose = true;

        /* create an input buffer with custom hooks */
        xmlParserInputBufferPtr bufPtr =
            xmlParserInputBufferCreateIO (ReadCallback, CloseCallback,
                                          ctxt, XML_CHAR_ENCODING_NONE);
        if (bufPtr)
        {
            /* create an input stream */
            xmlParserInputPtr inputPtr =
                xmlNewIOInputStream (aCtxt, bufPtr, XML_CHAR_ENCODING_NONE);

            if (inputPtr != NULL)
            {
                /* pass over the URI to the stream struct (it's NULL by
                 * default) */
                inputPtr->filename =
                    (char *) xmlCanonicPath ((const xmlChar *) input->uri());
                return inputPtr;
            }
        }

        /* either of libxml calls failed */

        if (bufPtr)
            xmlFreeParserInputBuffer (bufPtr);

        delete input;
        delete ctxt;

        throw ENoMemory();
    }
    catch (const EIPRTFailure &err) { sThat->m->trappedErr.reset (stdx::new_exception_trap (err)); }
    catch (const settings::Error &err) { sThat->m->trappedErr.reset (stdx::new_exception_trap (err)); }
    catch (const std::exception &err) { sThat->m->trappedErr.reset (stdx::new_exception_trap (err)); }
    catch (...) { sThat->m->trappedErr.reset (stdx::new_exception_trap (LogicError (RT_SRC_POS))); }

    return NULL;
}

} /* namespace settings */
