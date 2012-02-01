/* $Id$ */
/** @file
 * IPRT - XML Manipulation API.
 */

/*
 * Copyright (C) 2007-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/cpp/lock.h>
#include <iprt/cpp/xml.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/globals.h>
#include <libxml/xmlIO.h>
#include <libxml/xmlsave.h>
#include <libxml/uri.h>

#include <libxml/xmlschemas.h>

#include <map>
#include <boost/shared_ptr.hpp>

////////////////////////////////////////////////////////////////////////////////
//
// globals
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Global module initialization structure. This is to wrap non-reentrant bits
 * of libxml, among other things.
 *
 * The constructor and destructor of this structure are used to perform global
 * module initialization and cleanup. There must be only one global variable of
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
        RTCLockMtx lock;
    }
    sxml;  /* XXX naming this xml will break with gcc-3.3 */
}
gGlobal;



namespace xml
{

////////////////////////////////////////////////////////////////////////////////
//
// Exceptions
//
////////////////////////////////////////////////////////////////////////////////

LogicError::LogicError(RT_SRC_POS_DECL)
    : RTCError(NULL)
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
        throw EInvalidArg(RT_SRC_POS);

    char *msg = Format(aErr);
    setWhat(msg);
    RTStrFree(msg);
}

/**
 * Composes a single message for the given error. The caller must free the
 * returned string using RTStrFree() when no more necessary.
 */
// static
char *XmlError::Format(xmlErrorPtr aErr)
{
    const char *msg = aErr->message ? aErr->message : "<none>";
    size_t msgLen = strlen(msg);
    /* strip spaces, trailing EOLs and dot-like char */
    while (msgLen && strchr(" \n.?!", msg [msgLen - 1]))
        --msgLen;

    char *finalMsg = NULL;
    RTStrAPrintf(&finalMsg, "%.*s.\nLocation: '%s', line %d (%d), column %d",
                 msgLen, msg, aErr->file, aErr->line, aErr->int1, aErr->int2);

    return finalMsg;
}

EIPRTFailure::EIPRTFailure(int aRC, const char *pcszContext, ...)
    : RuntimeError(NULL),
      mRC(aRC)
{
    char *pszContext2;
    va_list args;
    va_start(args, pcszContext);
    RTStrAPrintfV(&pszContext2, pcszContext, args);
    char *newMsg;
    RTStrAPrintf(&newMsg, "%s: %d (%s)", pszContext2, aRC, RTErrGetShort(aRC));
    setWhat(newMsg);
    RTStrFree(newMsg);
    RTStrFree(pszContext2);
}

////////////////////////////////////////////////////////////////////////////////
//
// File Class
//
//////////////////////////////////////////////////////////////////////////////

struct File::Data
{
    Data()
        : handle(NIL_RTFILE), opened(false)
    { }

    RTCString strFileName;
    RTFILE handle;
    bool opened : 1;
    bool flushOnClose : 1;
};

File::File(Mode aMode, const char *aFileName, bool aFlushIt /* = false */)
    : m(new Data())
{
    m->strFileName = aFileName;
    m->flushOnClose = aFlushIt;

    uint32_t flags = 0;
    switch (aMode)
    {
        /** @todo change to RTFILE_O_DENY_WRITE where appropriate. */
        case Mode_Read:
            flags = RTFILE_O_READ      | RTFILE_O_OPEN           | RTFILE_O_DENY_NONE;
            break;
        case Mode_WriteCreate:      // fail if file exists
            flags = RTFILE_O_WRITE     | RTFILE_O_CREATE         | RTFILE_O_DENY_NONE;
            break;
        case Mode_Overwrite:        // overwrite if file exists
            flags = RTFILE_O_WRITE     | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE;
            break;
        case Mode_ReadWrite:
            flags = RTFILE_O_READWRITE | RTFILE_O_OPEN           | RTFILE_O_DENY_NONE;;
    }

    int vrc = RTFileOpen(&m->handle, aFileName, flags);
    if (RT_FAILURE(vrc))
        throw EIPRTFailure(vrc, "Runtime error opening '%s' for reading", aFileName);

    m->opened = true;
    m->flushOnClose = aFlushIt && (flags & RTFILE_O_ACCESS_MASK) != RTFILE_O_READ;
}

File::File(RTFILE aHandle, const char *aFileName /* = NULL */, bool aFlushIt /* = false */)
    : m(new Data())
{
    if (aHandle == NIL_RTFILE)
        throw EInvalidArg(RT_SRC_POS);

    m->handle = aHandle;

    if (aFileName)
        m->strFileName = aFileName;

    m->flushOnClose = aFlushIt;

    setPos(0);
}

File::~File()
{
    if (m->flushOnClose)
    {
        RTFileFlush(m->handle);
        if (!m->strFileName.isEmpty())
            RTDirFlushParent(m->strFileName.c_str());
    }

    if (m->opened)
        RTFileClose(m->handle);
    delete m;
}

const char* File::uri() const
{
    return m->strFileName.c_str();
}

uint64_t File::pos() const
{
    uint64_t p = 0;
    int vrc = RTFileSeek(m->handle, 0, RTFILE_SEEK_CURRENT, &p);
    if (RT_SUCCESS(vrc))
        return p;

    throw EIPRTFailure(vrc, "Runtime error seeking in file '%s'", m->strFileName.c_str());
}

void File::setPos(uint64_t aPos)
{
    uint64_t p = 0;
    unsigned method = RTFILE_SEEK_BEGIN;
    int vrc = VINF_SUCCESS;

    /* check if we overflow int64_t and move to INT64_MAX first */
    if ((int64_t)aPos < 0)
    {
        vrc = RTFileSeek(m->handle, INT64_MAX, method, &p);
        aPos -= (uint64_t)INT64_MAX;
        method = RTFILE_SEEK_CURRENT;
    }
    /* seek the rest */
    if (RT_SUCCESS(vrc))
        vrc = RTFileSeek(m->handle, (int64_t) aPos, method, &p);
    if (RT_SUCCESS(vrc))
        return;

    throw EIPRTFailure(vrc, "Runtime error seeking in file '%s'", m->strFileName.c_str());
}

int File::read(char *aBuf, int aLen)
{
    size_t len = aLen;
    int vrc = RTFileRead(m->handle, aBuf, len, &len);
    if (RT_SUCCESS(vrc))
        return (int)len;

    throw EIPRTFailure(vrc, "Runtime error reading from file '%s'", m->strFileName.c_str());
}

int File::write(const char *aBuf, int aLen)
{
    size_t len = aLen;
    int vrc = RTFileWrite (m->handle, aBuf, len, &len);
    if (RT_SUCCESS (vrc))
        return (int)len;

    throw EIPRTFailure(vrc, "Runtime error writing to file '%s'", m->strFileName.c_str());

    return -1 /* failure */;
}

void File::truncate()
{
    int vrc = RTFileSetSize (m->handle, pos());
    if (RT_SUCCESS (vrc))
        return;

    throw EIPRTFailure(vrc, "Runtime error truncating file '%s'", m->strFileName.c_str());
}

////////////////////////////////////////////////////////////////////////////////
//
// MemoryBuf Class
//
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
    size_t off = (size_t) aPos;
    if ((uint64_t) off != aPos)
        throw EInvalidArg();

    if (off > m->len)
        throw EInvalidArg();

    m->pos = off;
}

int MemoryBuf::read (char *aBuf, int aLen)
{
    if (m->pos >= m->len)
        return 0 /* nothing to read */;

    size_t len = m->pos + aLen < m->len ? aLen : m->len - m->pos;
    memcpy (aBuf, m->buf + m->pos, len);
    m->pos += len;

    return (int)len;
}

////////////////////////////////////////////////////////////////////////////////
//
// GlobalLock class
//
////////////////////////////////////////////////////////////////////////////////

struct GlobalLock::Data
{
    PFNEXTERNALENTITYLOADER pOldLoader;
    RTCLock lock;

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
    delete m;
    m = NULL;
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

////////////////////////////////////////////////////////////////////////////////
//
// Node class
//
////////////////////////////////////////////////////////////////////////////////

struct Node::Data
{
    struct compare_const_char
    {
        bool operator()(const char* s1, const char* s2) const
        {
            return strcmp(s1, s2) < 0;
        }
    };

    // attributes, if this is an element; can be empty
    typedef std::map<const char*, boost::shared_ptr<AttributeNode>, compare_const_char > AttributesMap;
    AttributesMap attribs;

    // child elements, if this is an element; can be empty
    typedef std::list< boost::shared_ptr<Node> > InternalNodesList;
    InternalNodesList children;
};

Node::Node(EnumType type,
           Node *pParent,
           xmlNode *plibNode,
           xmlAttr *plibAttr)
    : m_Type(type),
      m_pParent(pParent),
      m_plibNode(plibNode),
      m_plibAttr(plibAttr),
      m_pcszNamespacePrefix(NULL),
      m_pcszNamespaceHref(NULL),
      m_pcszName(NULL),
      m(new Data)
{
}

Node::~Node()
{
    delete m;
}

/**
 * Private implementation.
 * @param elmRoot
 */
void Node::buildChildren(const ElementNode &elmRoot)       // private
{
    // go thru this element's attributes
    xmlAttr *plibAttr = m_plibNode->properties;
    while (plibAttr)
    {
        const char *pcszKey;
        boost::shared_ptr<AttributeNode> pNew(new AttributeNode(elmRoot, this, plibAttr, &pcszKey));
        // store
        m->attribs[pcszKey] = pNew;

        plibAttr = plibAttr->next;
    }

    // go thru this element's child elements
    xmlNodePtr plibNode = m_plibNode->children;
    while (plibNode)
    {
        boost::shared_ptr<Node> pNew;

        if (plibNode->type == XML_ELEMENT_NODE)
            pNew = boost::shared_ptr<Node>(new ElementNode(&elmRoot, this, plibNode));
        else if (plibNode->type == XML_TEXT_NODE)
            pNew = boost::shared_ptr<Node>(new ContentNode(this, plibNode));
        if (pNew)
        {
            // store
            m->children.push_back(pNew);

            // recurse for this child element to get its own children
            pNew->buildChildren(elmRoot);
        }

        plibNode = plibNode->next;
    }
}

/**
 * Returns the name of the node, which is either the element name or
 * the attribute name. For other node types it probably returns NULL.
 * @return
 */
const char* Node::getName() const
{
    return m_pcszName;
}

/**
 * Variant of nameEquals that checks the namespace as well.
 * @param pcszNamespace
 * @param pcsz
 * @return
 */
bool Node::nameEquals(const char *pcszNamespace, const char *pcsz) const
{
    if (m_pcszName == pcsz)
        return true;
    if (m_pcszName == NULL)
        return false;
    if (pcsz == NULL)
        return false;
    if (strcmp(m_pcszName, pcsz))
        return false;

    // name matches: then check namespaces as well
    if (!pcszNamespace)
        return true;
    // caller wants namespace:
    if (!m_pcszNamespacePrefix)
        // but node has no namespace:
        return false;
    return !strcmp(m_pcszNamespacePrefix, pcszNamespace);
}

/**
 * Returns the value of a node. If this node is an attribute, returns
 * the attribute value; if this node is an element, then this returns
 * the element text content.
 * @return
 */
const char* Node::getValue() const
{
    if (    (m_plibAttr)
         && (m_plibAttr->children)
       )
        // libxml hides attribute values in another node created as a
        // single child of the attribute node, and it's in the content field
        return (const char*)m_plibAttr->children->content;

    if (    (m_plibNode)
         && (m_plibNode->children)
       )
        return (const char*)m_plibNode->children->content;

    return NULL;
}

/**
 * Copies the value of a node into the given integer variable.
 * Returns TRUE only if a value was found and was actually an
 * integer of the given type.
 * @return
 */
bool Node::copyValue(int32_t &i) const
{
    const char *pcsz;
    if (    ((pcsz = getValue()))
         && (VINF_SUCCESS == RTStrToInt32Ex(pcsz, NULL, 10, &i))
       )
        return true;

    return false;
}

/**
 * Copies the value of a node into the given integer variable.
 * Returns TRUE only if a value was found and was actually an
 * integer of the given type.
 * @return
 */
bool Node::copyValue(uint32_t &i) const
{
    const char *pcsz;
    if (    ((pcsz = getValue()))
         && (VINF_SUCCESS == RTStrToUInt32Ex(pcsz, NULL, 10, &i))
       )
        return true;

    return false;
}

/**
 * Copies the value of a node into the given integer variable.
 * Returns TRUE only if a value was found and was actually an
 * integer of the given type.
 * @return
 */
bool Node::copyValue(int64_t &i) const
{
    const char *pcsz;
    if (    ((pcsz = getValue()))
         && (VINF_SUCCESS == RTStrToInt64Ex(pcsz, NULL, 10, &i))
       )
        return true;

    return false;
}

/**
 * Copies the value of a node into the given integer variable.
 * Returns TRUE only if a value was found and was actually an
 * integer of the given type.
 * @return
 */
bool Node::copyValue(uint64_t &i) const
{
    const char *pcsz;
    if (    ((pcsz = getValue()))
         && (VINF_SUCCESS == RTStrToUInt64Ex(pcsz, NULL, 10, &i))
       )
        return true;

    return false;
}

/**
 * Returns the line number of the current node in the source XML file.
 * Useful for error messages.
 * @return
 */
int Node::getLineNumber() const
{
    if (m_plibAttr)
        return m_pParent->m_plibNode->line;

    return m_plibNode->line;
}

/**
 * Private element constructor.
 * @param pelmRoot
 * @param pParent
 * @param plibNode
 */
ElementNode::ElementNode(const ElementNode *pelmRoot,
                         Node *pParent,
                         xmlNode *plibNode)
    : Node(IsElement,
           pParent,
           plibNode,
           NULL)
{
    if (!(m_pelmRoot = pelmRoot))
        // NULL passed, then this is the root element
        m_pelmRoot = this;

    m_pcszName = (const char*)plibNode->name;

    if (plibNode->ns)
    {
        m_pcszNamespacePrefix = (const char*)m_plibNode->ns->prefix;
        m_pcszNamespaceHref = (const char*)m_plibNode->ns->href;
    }
}

/**
 * Builds a list of direct child elements of the current element that
 * match the given string; if pcszMatch is NULL, all direct child
 * elements are returned.
 * @param children out: list of nodes to which children will be appended.
 * @param pcszMatch in: match string, or NULL to return all children.
 * @return Number of items appended to the list (0 if none).
 */
int ElementNode::getChildElements(ElementNodesList &children,
                                  const char *pcszMatch /*= NULL*/)
    const
{
    int i = 0;
    for (Data::InternalNodesList::iterator it = m->children.begin();
         it != m->children.end();
         ++it)
    {
        // export this child node if ...
        Node *p = it->get();
        if (p->isElement())
            if (    (!pcszMatch)    // the caller wants all nodes or
                 || (!strcmp(pcszMatch, p->getName())) // the element name matches
               )
            {
                children.push_back(static_cast<ElementNode*>(p));
                ++i;
            }
    }
    return i;
}

/**
 * Returns the first child element whose name matches pcszMatch.
 *
 * @param pcszNamespace Namespace prefix (e.g. "vbox") or NULL to match any namespace.
 * @param pcszMatch Element name to match.
 * @return
 */
const ElementNode* ElementNode::findChildElement(const char *pcszNamespace,
                                                 const char *pcszMatch)
    const
{
    Data::InternalNodesList::const_iterator
        it,
        last = m->children.end();
    for (it = m->children.begin();
         it != last;
         ++it)
    {
        if ((**it).isElement())
        {
            ElementNode *pelm = static_cast<ElementNode*>((*it).get());
            if (pelm->nameEquals(pcszNamespace, pcszMatch))
                return pelm;
        }
    }

    return NULL;
}

/**
 * Returns the first child element whose "id" attribute matches pcszId.
 * @param pcszId identifier to look for.
 * @return child element or NULL if not found.
 */
const ElementNode* ElementNode::findChildElementFromId(const char *pcszId) const
{
    Data::InternalNodesList::const_iterator
        it,
        last = m->children.end();
    for (it = m->children.begin();
         it != last;
         ++it)
    {
        if ((**it).isElement())
        {
            ElementNode *pelm = static_cast<ElementNode*>((*it).get());
            const AttributeNode *pAttr;
            if (    ((pAttr = pelm->findAttribute("id")))
                 && (!strcmp(pAttr->getValue(), pcszId))
               )
                return pelm;
        }
    }

    return NULL;
}

/**
 * Looks up the given attribute node in this element's attribute map.
 *
 * With respect to namespaces, the internal attributes map stores namespace
 * prefixes with attribute names only if the attribute uses a non-default
 * namespace. As a result, the following rules apply:
 *
 *  -- To find attributes from a non-default namespace, pcszMatch must not
 *     be prefixed with a namespace.
 *
 *  -- To find attributes from the default namespace (or if the document does
 *     not use namespaces), pcszMatch must be prefixed with the namespace
 *     prefix and a colon.
 *
 * For example, if the document uses the "vbox:" namespace by default, you
 * must omit "vbox:" from pcszMatch to find such attributes, whether they
 * are specifed in the xml or not.
 *
 * @param pcszMatch
 * @return
 */
const AttributeNode* ElementNode::findAttribute(const char *pcszMatch) const
{
    Data::AttributesMap::const_iterator it;

    it = m->attribs.find(pcszMatch);
    if (it != m->attribs.end())
        return it->second.get();

    return NULL;
}

/**
 * Convenience method which attempts to find the attribute with the given
 * name and returns its value as a string.
 *
 * @param pcszMatch name of attribute to find (see findAttribute() for namespace remarks)
 * @param ppcsz out: attribute value
 * @return TRUE if attribute was found and str was thus updated.
 */
bool ElementNode::getAttributeValue(const char *pcszMatch, const char *&ppcsz) const
{
    const Node* pAttr;
    if ((pAttr = findAttribute(pcszMatch)))
    {
        ppcsz = pAttr->getValue();
        return true;
    }

    return false;
}

/**
 * Convenience method which attempts to find the attribute with the given
 * name and returns its value as a string.
 *
 * @param pcszMatch name of attribute to find (see findAttribute() for namespace remarks)
 * @param str out: attribute value; overwritten only if attribute was found
 * @return TRUE if attribute was found and str was thus updated.
 */
bool ElementNode::getAttributeValue(const char *pcszMatch, RTCString &str) const
{
    const Node* pAttr;
    if ((pAttr = findAttribute(pcszMatch)))
    {
        str = pAttr->getValue();
        return true;
    }

    return false;
}

/**
 * Like getAttributeValue (ministring variant), but makes sure that all backslashes
 * are converted to forward slashes.
 * @param pcszMatch
 * @param str
 * @return
 */
bool ElementNode::getAttributeValuePath(const char *pcszMatch, RTCString &str) const
{
    if (getAttributeValue(pcszMatch, str))
    {
        str.findReplace('\\', '/');
        return true;
    }

    return false;
}

/**
 * Convenience method which attempts to find the attribute with the given
 * name and returns its value as a signed integer. This calls
 * RTStrToInt32Ex internally and will only output the integer if that
 * function returns no error.
 *
 * @param pcszMatch name of attribute to find (see findAttribute() for namespace remarks)
 * @param i out: attribute value; overwritten only if attribute was found
 * @return TRUE if attribute was found and str was thus updated.
 */
bool ElementNode::getAttributeValue(const char *pcszMatch, int32_t &i) const
{
    const char *pcsz;
    if (    (getAttributeValue(pcszMatch, pcsz))
         && (VINF_SUCCESS == RTStrToInt32Ex(pcsz, NULL, 0, &i))
       )
        return true;

    return false;
}

/**
 * Convenience method which attempts to find the attribute with the given
 * name and returns its value as an unsigned integer.This calls
 * RTStrToUInt32Ex internally and will only output the integer if that
 * function returns no error.
 *
 * @param pcszMatch name of attribute to find (see findAttribute() for namespace remarks)
 * @param i out: attribute value; overwritten only if attribute was found
 * @return TRUE if attribute was found and str was thus updated.
 */
bool ElementNode::getAttributeValue(const char *pcszMatch, uint32_t &i) const
{
    const char *pcsz;
    if (    (getAttributeValue(pcszMatch, pcsz))
         && (VINF_SUCCESS == RTStrToUInt32Ex(pcsz, NULL, 0, &i))
       )
        return true;

    return false;
}

/**
 * Convenience method which attempts to find the attribute with the given
 * name and returns its value as a signed long integer. This calls
 * RTStrToInt64Ex internally and will only output the integer if that
 * function returns no error.
 *
 * @param pcszMatch name of attribute to find (see findAttribute() for namespace remarks)
 * @param i out: attribute value
 * @return TRUE if attribute was found and str was thus updated.
 */
bool ElementNode::getAttributeValue(const char *pcszMatch, int64_t &i) const
{
    const char *pcsz;
    if (    (getAttributeValue(pcszMatch, pcsz))
         && (VINF_SUCCESS == RTStrToInt64Ex(pcsz, NULL, 0, &i))
       )
        return true;

    return false;
}

/**
 * Convenience method which attempts to find the attribute with the given
 * name and returns its value as an unsigned long integer.This calls
 * RTStrToUInt64Ex internally and will only output the integer if that
 * function returns no error.
 *
 * @param pcszMatch name of attribute to find (see findAttribute() for namespace remarks)
 * @param i out: attribute value; overwritten only if attribute was found
 * @return TRUE if attribute was found and str was thus updated.
 */
bool ElementNode::getAttributeValue(const char *pcszMatch, uint64_t &i) const
{
    const char *pcsz;
    if (    (getAttributeValue(pcszMatch, pcsz))
         && (VINF_SUCCESS == RTStrToUInt64Ex(pcsz, NULL, 0, &i))
       )
        return true;

    return false;
}

/**
 * Convenience method which attempts to find the attribute with the given
 * name and returns its value as a boolean. This accepts "true", "false",
 * "yes", "no", "1" or "0" as valid values.
 *
 * @param pcszMatch name of attribute to find (see findAttribute() for namespace remarks)
 * @param f out: attribute value; overwritten only if attribute was found
 * @return TRUE if attribute was found and str was thus updated.
 */
bool ElementNode::getAttributeValue(const char *pcszMatch, bool &f) const
{
    const char *pcsz;
    if (getAttributeValue(pcszMatch, pcsz))
    {
        if (    (!strcmp(pcsz, "true"))
             || (!strcmp(pcsz, "yes"))
             || (!strcmp(pcsz, "1"))
           )
        {
            f = true;
            return true;
        }
        if (    (!strcmp(pcsz, "false"))
             || (!strcmp(pcsz, "no"))
             || (!strcmp(pcsz, "0"))
           )
        {
            f = false;
            return true;
        }
    }

    return false;
}

/**
 * Creates a new child element node and appends it to the list
 * of children in "this".
 *
 * @param pcszElementName
 * @return
 */
ElementNode* ElementNode::createChild(const char *pcszElementName)
{
    // we must be an element, not an attribute
    if (!m_plibNode)
        throw ENodeIsNotElement(RT_SRC_POS);

    // libxml side: create new node
    xmlNode *plibNode;
    if (!(plibNode = xmlNewNode(NULL,        // namespace
                                (const xmlChar*)pcszElementName)))
        throw std::bad_alloc();
    xmlAddChild(m_plibNode, plibNode);

    // now wrap this in C++
    ElementNode *p = new ElementNode(m_pelmRoot, this, plibNode);
    boost::shared_ptr<ElementNode> pNew(p);
    m->children.push_back(pNew);

    return p;
}


/**
 * Creates a content node and appends it to the list of children
 * in "this".
 *
 * @param pcszContent
 * @return
 */
ContentNode* ElementNode::addContent(const char *pcszContent)
{
    // libxml side: create new node
    xmlNode *plibNode;
    if (!(plibNode = xmlNewText((const xmlChar*)pcszContent)))
        throw std::bad_alloc();
    xmlAddChild(m_plibNode, plibNode);

    // now wrap this in C++
    ContentNode *p = new ContentNode(this, plibNode);
    boost::shared_ptr<ContentNode> pNew(p);
    m->children.push_back(pNew);

    return p;
}

/**
 * Sets the given attribute; overloaded version for const char *.
 *
 * If an attribute with the given name exists, it is overwritten,
 * otherwise a new attribute is created. Returns the attribute node
 * that was either created or changed.
 *
 * @param pcszName
 * @param pcszValue
 * @return
 */
AttributeNode* ElementNode::setAttribute(const char *pcszName, const char *pcszValue)
{
    AttributeNode *pattrReturn;
    Data::AttributesMap::const_iterator it;

    it = m->attribs.find(pcszName);
    if (it == m->attribs.end())
    {
        // libxml side: xmlNewProp creates an attribute
        xmlAttr *plibAttr = xmlNewProp(m_plibNode, (xmlChar*)pcszName, (xmlChar*)pcszValue);

        // C++ side: create an attribute node around it
        const char *pcszKey;
        boost::shared_ptr<AttributeNode> pNew(new AttributeNode(*m_pelmRoot, this, plibAttr, &pcszKey));
        // store
        m->attribs[pcszKey] = pNew;
        pattrReturn = pNew.get();
    }
    else
    {
        // overwrite existing libxml attribute node
        xmlAttrPtr plibAttr = xmlSetProp(m_plibNode, (xmlChar*)pcszName, (xmlChar*)pcszValue);

        // and fix our existing C++ side around it
        boost::shared_ptr<AttributeNode> pattr = it->second;
        pattr->m_plibAttr = plibAttr;       // in case the xmlAttrPtr is different, I'm not sure

        pattrReturn = pattr.get();
    }

    return pattrReturn;

}

/**
 * Like setAttribute (ministring variant), but replaces all backslashes with forward slashes
 * before calling that one.
 * @param pcszName
 * @param strValue
 * @return
 */
AttributeNode* ElementNode::setAttributePath(const char *pcszName, const RTCString &strValue)
{
    RTCString strTemp(strValue);
    strTemp.findReplace('\\', '/');
    return setAttribute(pcszName, strTemp.c_str());
}

/**
 * Sets the given attribute; overloaded version for int32_t.
 *
 * If an attribute with the given name exists, it is overwritten,
 * otherwise a new attribute is created. Returns the attribute node
 * that was either created or changed.
 *
 * @param pcszName
 * @param i
 * @return
 */
AttributeNode* ElementNode::setAttribute(const char *pcszName, int32_t i)
{
    char szValue[12];  // negative sign + 10 digits + \0
    RTStrPrintf(szValue, sizeof(szValue), "%RI32", i);
    AttributeNode *p = setAttribute(pcszName, szValue);
    return p;
}

/**
 * Sets the given attribute; overloaded version for uint32_t.
 *
 * If an attribute with the given name exists, it is overwritten,
 * otherwise a new attribute is created. Returns the attribute node
 * that was either created or changed.
 *
 * @param pcszName
 * @param u
 * @return
 */
AttributeNode* ElementNode::setAttribute(const char *pcszName, uint32_t u)
{
    char szValue[11];  // 10 digits + \0
    RTStrPrintf(szValue, sizeof(szValue), "%RU32", u);
    AttributeNode *p = setAttribute(pcszName, szValue);
    return p;
}

/**
 * Sets the given attribute; overloaded version for int64_t.
 *
 * If an attribute with the given name exists, it is overwritten,
 * otherwise a new attribute is created. Returns the attribute node
 * that was either created or changed.
 *
 * @param pcszName
 * @param i
 * @return
 */
AttributeNode* ElementNode::setAttribute(const char *pcszName, int64_t i)
{
    char szValue[21];  // negative sign + 19 digits + \0
    RTStrPrintf(szValue, sizeof(szValue), "%RI64", i);
    AttributeNode *p = setAttribute(pcszName, szValue);
    return p;
}

/**
 * Sets the given attribute; overloaded version for uint64_t.
 *
 * If an attribute with the given name exists, it is overwritten,
 * otherwise a new attribute is created. Returns the attribute node
 * that was either created or changed.
 *
 * @param pcszName
 * @param u
 * @return
 */
AttributeNode* ElementNode::setAttribute(const char *pcszName, uint64_t u)
{
    char szValue[21];  // 20 digits + \0
    RTStrPrintf(szValue, sizeof(szValue), "%RU64", u);
    AttributeNode *p = setAttribute(pcszName, szValue);
    return p;
}

/**
 * Sets the given attribute to the given uint32_t, outputs a hexadecimal string.
 *
 * If an attribute with the given name exists, it is overwritten,
 * otherwise a new attribute is created. Returns the attribute node
 * that was either created or changed.
 *
 * @param pcszName
 * @param u
 * @return
 */
AttributeNode* ElementNode::setAttributeHex(const char *pcszName, uint32_t u)
{
    char szValue[11];  // "0x" + 8 digits + \0
    RTStrPrintf(szValue, sizeof(szValue), "0x%RX32", u);
    AttributeNode *p = setAttribute(pcszName, szValue);
    return p;
}

/**
 * Sets the given attribute; overloaded version for bool.
 *
 * If an attribute with the given name exists, it is overwritten,
 * otherwise a new attribute is created. Returns the attribute node
 * that was either created or changed.
 *
 * @param pcszName
 * @param i
 * @return
 */
AttributeNode* ElementNode::setAttribute(const char *pcszName, bool f)
{
    return setAttribute(pcszName, (f) ? "true" : "false");
}

/**
 * Private constructor for a new attribute node. This one is special:
 * in ppcszKey, it returns a pointer to a string buffer that should be
 * used to index the attribute correctly with namespaces.
 *
 * @param pParent
 * @param elmRoot
 * @param plibAttr
 * @param ppcszKey
 */
AttributeNode::AttributeNode(const ElementNode &elmRoot,
                             Node *pParent,
                             xmlAttr *plibAttr,
                             const char **ppcszKey)
    : Node(IsAttribute,
           pParent,
           NULL,
           plibAttr)
{
    m_pcszName = (const char*)plibAttr->name;

    *ppcszKey = m_pcszName;

    if (    plibAttr->ns
         && plibAttr->ns->prefix
       )
    {
        m_pcszNamespacePrefix = (const char*)plibAttr->ns->prefix;
        m_pcszNamespaceHref = (const char*)plibAttr->ns->href;

        if (    !elmRoot.m_pcszNamespaceHref
             || (strcmp(m_pcszNamespaceHref, elmRoot.m_pcszNamespaceHref))
           )
        {
            // not default namespace:
            m_strKey = m_pcszNamespacePrefix;
            m_strKey.append(':');
            m_strKey.append(m_pcszName);

            *ppcszKey = m_strKey.c_str();
        }
    }
}

ContentNode::ContentNode(Node *pParent, xmlNode *plibNode)
    : Node(IsContent,
           pParent,
           plibNode,
           NULL)
{
}

/*
 * NodesLoop
 *
 */

struct NodesLoop::Data
{
    ElementNodesList listElements;
    ElementNodesList::const_iterator it;
};

NodesLoop::NodesLoop(const ElementNode &node, const char *pcszMatch /* = NULL */)
{
    m = new Data;
    node.getChildElements(m->listElements, pcszMatch);
    m->it = m->listElements.begin();
}

NodesLoop::~NodesLoop()
{
    delete m;
}


/**
 * Handy convenience helper for looping over all child elements. Create an
 * instance of NodesLoop on the stack and call this method until it returns
 * NULL, like this:
 * <code>
 *      xml::ElementNode node;               // should point to an element
 *      xml::NodesLoop loop(node, "child");  // find all "child" elements under node
 *      const xml::ElementNode *pChild = NULL;
 *      while (pChild = loop.forAllNodes())
 *          ...;
 * </code>
 * @return
 */
const ElementNode* NodesLoop::forAllNodes() const
{
    const ElementNode *pNode = NULL;

    if (m->it != m->listElements.end())
    {
        pNode = *(m->it);
        ++(m->it);
    }

    return pNode;
}

////////////////////////////////////////////////////////////////////////////////
//
// Document class
//
////////////////////////////////////////////////////////////////////////////////

struct Document::Data
{
    xmlDocPtr   plibDocument;
    ElementNode *pRootElement;
    ElementNode *pComment;

    Data()
    {
        plibDocument = NULL;
        pRootElement = NULL;
        pComment = NULL;
    }

    ~Data()
    {
        reset();
    }

    void reset()
    {
        if (plibDocument)
        {
            xmlFreeDoc(plibDocument);
            plibDocument = NULL;
        }
        if (pRootElement)
        {
            delete pRootElement;
            pRootElement = NULL;
        }
        if (pComment)
        {
            delete pComment;
            pComment = NULL;
        }
    }

    void copyFrom(const Document::Data *p)
    {
        if (p->plibDocument)
        {
            plibDocument = xmlCopyDoc(p->plibDocument,
                                      1);      // recursive == copy all
        }
    }
};

Document::Document()
    : m(new Data)
{
}

Document::Document(const Document &x)
    : m(new Data)
{
    m->copyFrom(x.m);
}

Document& Document::operator=(const Document &x)
{
    m->reset();
    m->copyFrom(x.m);
    return *this;
}

Document::~Document()
{
    delete m;
}

/**
 * private method to refresh all internal structures after the internal pDocument
 * has changed. Called from XmlFileParser::read(). m->reset() must have been
 * called before to make sure all members except the internal pDocument are clean.
 */
void Document::refreshInternals() // private
{
    m->pRootElement = new ElementNode(NULL, NULL, xmlDocGetRootElement(m->plibDocument));

    m->pRootElement->buildChildren(*m->pRootElement);
}

/**
 * Returns the root element of the document, or NULL if the document is empty.
 * Const variant.
 * @return
 */
const ElementNode* Document::getRootElement() const
{
    return m->pRootElement;
}

/**
 * Returns the root element of the document, or NULL if the document is empty.
 * Non-const variant.
 * @return
 */
ElementNode* Document::getRootElement()
{
    return m->pRootElement;
}

/**
 * Creates a new element node and sets it as the root element. This will
 * only work if the document is empty; otherwise EDocumentNotEmpty is thrown.
 */
ElementNode* Document::createRootElement(const char *pcszRootElementName,
                                         const char *pcszComment /* = NULL */)
{
    if (m->pRootElement || m->plibDocument)
        throw EDocumentNotEmpty(RT_SRC_POS);

    // libxml side: create document, create root node
    m->plibDocument = xmlNewDoc((const xmlChar*)"1.0");
    xmlNode *plibRootNode;
    if (!(plibRootNode = xmlNewNode(NULL,        // namespace
                                    (const xmlChar*)pcszRootElementName)))
        throw std::bad_alloc();
    xmlDocSetRootElement(m->plibDocument, plibRootNode);
    // now wrap this in C++
    m->pRootElement = new ElementNode(NULL, NULL, plibRootNode);

    // add document global comment if specified
    if (pcszComment != NULL)
    {
        xmlNode *pComment;
        if (!(pComment = xmlNewDocComment(m->plibDocument,
                                          (const xmlChar *)pcszComment)))
            throw std::bad_alloc();
        xmlAddPrevSibling(plibRootNode, pComment);
        // now wrap this in C++
        m->pComment = new ElementNode(NULL, NULL, pComment);
    }

    return m->pRootElement;
}

////////////////////////////////////////////////////////////////////////////////
//
// XmlParserBase class
//
////////////////////////////////////////////////////////////////////////////////

XmlParserBase::XmlParserBase()
{
    m_ctxt = xmlNewParserCtxt();
    if (m_ctxt == NULL)
        throw std::bad_alloc();
}

XmlParserBase::~XmlParserBase()
{
    xmlFreeParserCtxt (m_ctxt);
    m_ctxt = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// XmlMemParser class
//
////////////////////////////////////////////////////////////////////////////////

XmlMemParser::XmlMemParser()
    : XmlParserBase()
{
}

XmlMemParser::~XmlMemParser()
{
}

/**
 * Parse the given buffer and fills the given Document object with its contents.
 * Throws XmlError on parsing errors.
 *
 * The document that is passed in will be reset before being filled if not empty.
 *
 * @param pvBuf in: memory buffer to parse.
 * @param cbSize in: size of the memory buffer.
 * @param strFilename in: name fo file to parse.
 * @param doc out: document to be reset and filled with data according to file contents.
 */
void XmlMemParser::read(const void* pvBuf, size_t cbSize,
                        const RTCString &strFilename,
                        Document &doc)
{
    GlobalLock lock;
//     global.setExternalEntityLoader(ExternalEntityLoader);

    const char *pcszFilename = strFilename.c_str();

    doc.m->reset();
    if (!(doc.m->plibDocument = xmlCtxtReadMemory(m_ctxt,
                                                  (const char*)pvBuf,
                                                  (int)cbSize,
                                                  pcszFilename,
                                                  NULL,       // encoding = auto
                                                  XML_PARSE_NOBLANKS | XML_PARSE_NONET)))
        throw XmlError(xmlCtxtGetLastError(m_ctxt));

    doc.refreshInternals();
}

////////////////////////////////////////////////////////////////////////////////
//
// XmlMemWriter class
//
////////////////////////////////////////////////////////////////////////////////

XmlMemWriter::XmlMemWriter()
  : m_pBuf(0)
{
}

XmlMemWriter::~XmlMemWriter()
{
    if (m_pBuf)
        xmlFree(m_pBuf);
}

void XmlMemWriter::write(const Document &doc, void **ppvBuf, size_t *pcbSize)
{
    if (m_pBuf)
    {
        xmlFree(m_pBuf);
        m_pBuf = 0;
    }
    int size;
    xmlDocDumpFormatMemory(doc.m->plibDocument, (xmlChar**)&m_pBuf, &size, 1);
    *ppvBuf = m_pBuf;
    *pcbSize = size;
}

////////////////////////////////////////////////////////////////////////////////
//
// XmlFileParser class
//
////////////////////////////////////////////////////////////////////////////////

struct XmlFileParser::Data
{
    RTCString strXmlFilename;

    Data()
    {
    }

    ~Data()
    {
    }
};

XmlFileParser::XmlFileParser()
    : XmlParserBase(),
      m(new Data())
{
}

XmlFileParser::~XmlFileParser()
{
    delete m;
    m = NULL;
}

struct IOContext
{
    File file;
    RTCString error;

    IOContext(const char *pcszFilename, File::Mode mode, bool fFlush = false)
        : file(mode, pcszFilename, fFlush)
    {
    }

    void setError(const RTCError &x)
    {
        error = x.what();
    }

    void setError(const std::exception &x)
    {
        error = x.what();
    }
};

struct ReadContext : IOContext
{
    ReadContext(const char *pcszFilename)
        : IOContext(pcszFilename, File::Mode_Read)
    {
    }
};

struct WriteContext : IOContext
{
    WriteContext(const char *pcszFilename, bool fFlush)
        : IOContext(pcszFilename, File::Mode_Overwrite, fFlush)
    {
    }
};

/**
 * Reads the given file and fills the given Document object with its contents.
 * Throws XmlError on parsing errors.
 *
 * The document that is passed in will be reset before being filled if not empty.
 *
 * @param strFilename in: name fo file to parse.
 * @param doc out: document to be reset and filled with data according to file contents.
 */
void XmlFileParser::read(const RTCString &strFilename,
                         Document &doc)
{
    GlobalLock lock;
//     global.setExternalEntityLoader(ExternalEntityLoader);

    m->strXmlFilename = strFilename;
    const char *pcszFilename = strFilename.c_str();

    ReadContext context(pcszFilename);
    doc.m->reset();
    if (!(doc.m->plibDocument = xmlCtxtReadIO(m_ctxt,
                                              ReadCallback,
                                              CloseCallback,
                                              &context,
                                              pcszFilename,
                                              NULL,       // encoding = auto
                                              XML_PARSE_NOBLANKS | XML_PARSE_NONET)))
        throw XmlError(xmlCtxtGetLastError(m_ctxt));

    doc.refreshInternals();
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
    catch (const RTCError &err) { pContext->setError(err); }
    catch (const std::exception &err) { pContext->setError(err); }
    catch (...) { pContext->setError(xml::LogicError(RT_SRC_POS)); }

    return -1 /* failure */;
}

int XmlFileParser::CloseCallback(void *aCtxt)
{
    /// @todo to be written
    NOREF(aCtxt);

    return -1;
}

////////////////////////////////////////////////////////////////////////////////
//
// XmlFileWriter class
//
////////////////////////////////////////////////////////////////////////////////

struct XmlFileWriter::Data
{
    Document *pDoc;
};

XmlFileWriter::XmlFileWriter(Document &doc)
{
    m = new Data();
    m->pDoc = &doc;
}

XmlFileWriter::~XmlFileWriter()
{
    delete m;
}

void XmlFileWriter::writeInternal(const char *pcszFilename, bool fSafe)
{
    WriteContext context(pcszFilename, fSafe);

    GlobalLock lock;

    /* serialize to the stream */
    xmlIndentTreeOutput = 1;
    xmlTreeIndentString = "  ";
    xmlSaveNoEmptyTags = 0;

    xmlSaveCtxtPtr saveCtxt;
    if (!(saveCtxt = xmlSaveToIO(WriteCallback,
                                 CloseCallback,
                                 &context,
                                 NULL,
                                 XML_SAVE_FORMAT)))
        throw xml::LogicError(RT_SRC_POS);

    long rc = xmlSaveDoc(saveCtxt, m->pDoc->m->plibDocument);
    if (rc == -1)
    {
        /* look if there was a forwarded exception from the lower level */
//         if (m->trappedErr.get() != NULL)
//             m->trappedErr->rethrow();

        /* there must be an exception from the Output implementation,
         * otherwise the save operation must always succeed. */
        throw xml::LogicError(RT_SRC_POS);
    }

    xmlSaveClose(saveCtxt);
}

void XmlFileWriter::write(const char *pcszFilename, bool fSafe)
{
    if (!fSafe)
        writeInternal(pcszFilename, fSafe);
    else
    {
        /* Empty string and directory spec must be avoid. */
        if (RTPathFilename(pcszFilename) == NULL)
            throw xml::LogicError(RT_SRC_POS);

        /* Construct both filenames first to ease error handling.  */
        char szTmpFilename[RTPATH_MAX];
        int rc = RTStrCopy(szTmpFilename, sizeof(szTmpFilename) - strlen(s_pszTmpSuff), pcszFilename);
        if (RT_FAILURE(rc))
            throw EIPRTFailure(rc, "RTStrCopy");
        strcat(szTmpFilename, s_pszTmpSuff);

        char szPrevFilename[RTPATH_MAX];
        rc = RTStrCopy(szPrevFilename, sizeof(szPrevFilename) - strlen(s_pszPrevSuff), pcszFilename);
        if (RT_FAILURE(rc))
            throw EIPRTFailure(rc, "RTStrCopy");
        strcat(szPrevFilename, s_pszPrevSuff);

        /* Write the XML document to the temporary file.  */
        writeInternal(szTmpFilename, fSafe);

        /* Make a backup of any existing file (ignore failure). */
        uint64_t cbPrevFile;
        rc = RTFileQuerySize(pcszFilename, &cbPrevFile);
        if (RT_SUCCESS(rc) && cbPrevFile >= 16)
            RTFileRename(pcszFilename, szPrevFilename, RTPATHRENAME_FLAGS_REPLACE);

        /* Commit the temporary file. Just leave the tmp file behind on failure. */
        rc = RTFileRename(szTmpFilename, pcszFilename, RTPATHRENAME_FLAGS_REPLACE);
        if (RT_FAILURE(rc))
            throw EIPRTFailure(rc, "Failed to replace '%s' with '%s'", pcszFilename, szTmpFilename);

        /* Flush the directory changes (required on linux at least). */
        RTPathStripFilename(szTmpFilename);
        rc = RTDirFlush(szTmpFilename);
        AssertMsg(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED || rc == VERR_NOT_IMPLEMENTED, ("%Rrc\n", rc));
    }
}

int XmlFileWriter::WriteCallback(void *aCtxt, const char *aBuf, int aLen)
{
    WriteContext *pContext = static_cast<WriteContext*>(aCtxt);

    /* To prevent throwing exceptions while inside libxml2 code, we catch
     * them and forward to our level using a couple of variables. */
    try
    {
        return pContext->file.write(aBuf, aLen);
    }
    catch (const xml::EIPRTFailure &err) { pContext->setError(err); }
    catch (const RTCError &err) { pContext->setError(err); }
    catch (const std::exception &err) { pContext->setError(err); }
    catch (...) { pContext->setError(xml::LogicError(RT_SRC_POS)); }

    return -1 /* failure */;
}

int XmlFileWriter::CloseCallback(void *aCtxt)
{
    /// @todo to be written
    NOREF(aCtxt);

    return -1;
}

/*static*/ const char * const XmlFileWriter::s_pszTmpSuff  = "-tmp";
/*static*/ const char * const XmlFileWriter::s_pszPrevSuff = "-prev";


} // end namespace xml

