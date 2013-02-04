/** @file
 * IPRT - XML Helper APIs.
 */

/*
 * Copyright (C) 2007-2012 Oracle Corporation
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

#ifndef ___iprt_xml_h
#define ___iprt_xml_h

#ifndef IN_RING3
# error "There are no XML APIs available in Ring-0 Context!"
#endif

#include <list>
#include <memory>

#include <iprt/cpp/exception.h>

/** @defgroup grp_rt_cpp_xml    C++ XML support
 * @ingroup grp_rt_cpp
 * @{
 */

/* Forwards */
typedef struct _xmlParserInput xmlParserInput;
typedef xmlParserInput *xmlParserInputPtr;
typedef struct _xmlParserCtxt xmlParserCtxt;
typedef xmlParserCtxt *xmlParserCtxtPtr;
typedef struct _xmlError xmlError;
typedef xmlError *xmlErrorPtr;

typedef struct _xmlAttr xmlAttr;
typedef struct _xmlNode xmlNode;

/** @} */

namespace xml
{

/**
 * @addtogroup grp_rt_cpp_xml
 * @{
 */

// Exceptions
//////////////////////////////////////////////////////////////////////////////

class RT_DECL_CLASS LogicError : public RTCError
{
public:

    LogicError(const char *aMsg = NULL)
        : RTCError(aMsg)
    {}

    LogicError(RT_SRC_POS_DECL);
};

class RT_DECL_CLASS RuntimeError : public RTCError
{
public:

    RuntimeError(const char *aMsg = NULL)
        : RTCError(aMsg)
    {}
};

class RT_DECL_CLASS XmlError : public RuntimeError
{
public:
    XmlError(xmlErrorPtr aErr);

    static char* Format(xmlErrorPtr aErr);
};

// Logical errors
//////////////////////////////////////////////////////////////////////////////

class RT_DECL_CLASS ENotImplemented : public LogicError
{
public:
    ENotImplemented(const char *aMsg = NULL) : LogicError(aMsg) {}
    ENotImplemented(RT_SRC_POS_DECL) : LogicError(RT_SRC_POS_ARGS) {}
};

class RT_DECL_CLASS EInvalidArg : public LogicError
{
public:
    EInvalidArg(const char *aMsg = NULL) : LogicError(aMsg) {}
    EInvalidArg(RT_SRC_POS_DECL) : LogicError(RT_SRC_POS_ARGS) {}
};

class RT_DECL_CLASS EDocumentNotEmpty : public LogicError
{
public:
    EDocumentNotEmpty(const char *aMsg = NULL) : LogicError(aMsg) {}
    EDocumentNotEmpty(RT_SRC_POS_DECL) : LogicError(RT_SRC_POS_ARGS) {}
};

class RT_DECL_CLASS ENodeIsNotElement : public LogicError
{
public:
    ENodeIsNotElement(const char *aMsg = NULL) : LogicError(aMsg) {}
    ENodeIsNotElement(RT_SRC_POS_DECL) : LogicError(RT_SRC_POS_ARGS) {}
};

// Runtime errors
//////////////////////////////////////////////////////////////////////////////

class RT_DECL_CLASS EIPRTFailure : public RuntimeError
{
public:

    EIPRTFailure(int aRC, const char *pcszContext, ...);

    int rc() const
    {
        return mRC;
    }

private:
    int mRC;
};

/**
 * The Stream class is a base class for I/O streams.
 */
class RT_DECL_CLASS Stream
{
public:

    virtual ~Stream() {}

    virtual const char *uri() const = 0;

    /**
     * Returns the current read/write position in the stream. The returned
     * position is a zero-based byte offset from the beginning of the file.
     *
     * Throws ENotImplemented if this operation is not implemented for the
     * given stream.
     */
    virtual uint64_t pos() const = 0;

    /**
     * Sets the current read/write position in the stream.
     *
     * @param aPos Zero-based byte offset from the beginning of the stream.
     *
     * Throws ENotImplemented if this operation is not implemented for the
     * given stream.
     */
    virtual void setPos (uint64_t aPos) = 0;
};

/**
 * The Input class represents an input stream.
 *
 * This input stream is used to read the settings tree from.
 * This is an abstract class that must be subclassed in order to fill it with
 * useful functionality.
 */
class RT_DECL_CLASS Input : virtual public Stream
{
public:

    /**
     * Reads from the stream to the supplied buffer.
     *
     * @param aBuf Buffer to store read data to.
     * @param aLen Buffer length.
     *
     * @return Number of bytes read.
     */
    virtual int read (char *aBuf, int aLen) = 0;
};

/**
 *
 */
class RT_DECL_CLASS Output : virtual public Stream
{
public:

    /**
     * Writes to the stream from the supplied buffer.
     *
     * @param aBuf Buffer to write data from.
     * @param aLen Buffer length.
     *
     * @return Number of bytes written.
     */
    virtual int write (const char *aBuf, int aLen) = 0;

    /**
     * Truncates the stream from the current position and upto the end.
     * The new file size will become exactly #pos() bytes.
     *
     * Throws ENotImplemented if this operation is not implemented for the
     * given stream.
     */
    virtual void truncate() = 0;
};


//////////////////////////////////////////////////////////////////////////////

/**
 * The File class is a stream implementation that reads from and writes to
 * regular files.
 *
 * The File class uses IPRT File API for file operations. Note that IPRT File
 * API is not thread-safe. This means that if you pass the same RTFILE handle to
 * different File instances that may be simultaneously used on different
 * threads, you should care about serialization; otherwise you will get garbage
 * when reading from or writing to such File instances.
 */
class RT_DECL_CLASS File : public Input, public Output
{
public:

    /**
     * Possible file access modes.
     */
    enum Mode { Mode_Read, Mode_WriteCreate, Mode_Overwrite, Mode_ReadWrite };

    /**
     * Opens a file with the given name in the given mode. If @a aMode is Read
     * or ReadWrite, the file must exist. If @a aMode is Write, the file must
     * not exist. Otherwise, an EIPRTFailure excetion will be thrown.
     *
     * @param aMode     File mode.
     * @param aFileName File name.
     * @param aFlushIt  Whether to flush a writable file before closing it.
     */
    File(Mode aMode, const char *aFileName, bool aFlushIt = false);

    /**
     * Uses the given file handle to perform file operations. This file
     * handle must be already open in necessary mode (read, or write, or mixed).
     *
     * The read/write position of the given handle will be reset to the
     * beginning of the file on success.
     *
     * Note that the given file handle will not be automatically closed upon
     * this object destruction.
     *
     * @note It you pass the same RTFILE handle to more than one File instance,
     *       please make sure you have provided serialization in case if these
     *       instasnces are to be simultaneously used by different threads.
     *       Otherwise you may get garbage when reading or writing.
     *
     * @param aHandle   Open file handle.
     * @param aFileName File name (for reference).
     * @param aFlushIt  Whether to flush a writable file before closing it.
     */
    File(RTFILE aHandle, const char *aFileName = NULL, bool aFlushIt = false);

    /**
     * Destroys the File object. If the object was created from a file name
     * the corresponding file will be automatically closed. If the object was
     * created from a file handle, it will remain open.
     */
    virtual ~File();

    const char *uri() const;

    uint64_t pos() const;
    void setPos(uint64_t aPos);

    /**
     * See Input::read(). If this method is called in wrong file mode,
     * LogicError will be thrown.
     */
    int read(char *aBuf, int aLen);

    /**
     * See Output::write(). If this method is called in wrong file mode,
     * LogicError will be thrown.
     */
    int write(const char *aBuf, int aLen);

    /**
     * See Output::truncate(). If this method is called in wrong file mode,
     * LogicError will be thrown.
     */
    void truncate();

private:

    /* Obscure class data */
    struct Data;
    Data *m;

    /* auto_ptr data doesn't have proper copy semantics */
    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (File)
};

/**
 * The MemoryBuf class represents a stream implementation that reads from the
 * memory buffer.
 */
class RT_DECL_CLASS MemoryBuf : public Input
{
public:

    MemoryBuf (const char *aBuf, size_t aLen, const char *aURI = NULL);

    virtual ~MemoryBuf();

    const char *uri() const;

    int read(char *aBuf, int aLen);
    uint64_t pos() const;
    void setPos(uint64_t aPos);

private:
    /* Obscure class data */
    struct Data;
    Data *m;

    /* auto_ptr data doesn't have proper copy semantics */
    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(MemoryBuf)
};


/*
 * GlobalLock
 *
 *
 */

typedef xmlParserInput* FNEXTERNALENTITYLOADER(const char *aURI,
                                               const char *aID,
                                               xmlParserCtxt *aCtxt);
typedef FNEXTERNALENTITYLOADER *PFNEXTERNALENTITYLOADER;

class RT_DECL_CLASS GlobalLock
{
public:
    GlobalLock();
    ~GlobalLock();

    void setExternalEntityLoader(PFNEXTERNALENTITYLOADER pFunc);

    static xmlParserInput* callDefaultLoader(const char *aURI,
                                             const char *aID,
                                             xmlParserCtxt *aCtxt);

private:
    /* Obscure class data. */
    struct Data;
    struct Data *m;
};

class ElementNode;
typedef std::list<const ElementNode*> ElementNodesList;

class AttributeNode;

class ContentNode;

/**
 * Node base class. Cannot be used directly, but ElementNode, ContentNode and
 * AttributeNode derive from this. This does implement useful public methods though.
 */
class RT_DECL_CLASS Node
{
public:
    ~Node();

    const char* getName() const;
    const char* getPrefix() const;
    bool nameEquals(const char *pcszNamespace, const char *pcsz) const;
    bool nameEquals(const char *pcsz) const
    {
        return nameEquals(NULL, pcsz);
    }

    const char* getValue() const;
    bool copyValue(int32_t &i) const;
    bool copyValue(uint32_t &i) const;
    bool copyValue(int64_t &i) const;
    bool copyValue(uint64_t &i) const;

    int getLineNumber() const;

    int isElement() const
    {
        return m_Type == IsElement;
    }

protected:
    typedef enum {IsElement, IsAttribute, IsContent} EnumType;

    EnumType    m_Type;
    Node        *m_pParent;
    xmlNode     *m_plibNode;            // != NULL if this is an element or content node
    xmlAttr     *m_plibAttr;            // != NULL if this is an attribute node
    const char  *m_pcszNamespacePrefix; // not always set
    const char  *m_pcszNamespaceHref;   // full http:// spec
    const char  *m_pcszName;            // element or attribute name, points either into plibNode or plibAttr;
                                        // NULL if this is a content node

    // hide the default constructor so people use only our factory methods
    Node(EnumType type,
         Node *pParent,
         xmlNode *plibNode,
         xmlAttr *plibAttr);
    Node(const Node &x);      // no copying

    void buildChildren(const ElementNode &elmRoot);

    /* Obscure class data */
    struct Data;
    Data *m;

    friend class AttributeNode;
};

/**
 *  Node subclass that represents an element.
 *
 *  For elements, Node::getName() returns the element name, and Node::getValue()
 *  returns the text contents, if any.
 *
 *  Since the Node constructor is private, one can create element nodes
 *  only through the following factory methods:
 *
 *  --  Document::createRootElement()
 *  --  ElementNode::createChild()
 */
class RT_DECL_CLASS ElementNode : public Node
{
public:
    int getChildElements(ElementNodesList &children,
                         const char *pcszMatch = NULL) const;

    const ElementNode* findChildElement(const char *pcszNamespace,
                                        const char *pcszMatch) const;
    const ElementNode* findChildElement(const char *pcszMatch) const
    {
        return findChildElement(NULL, pcszMatch);
    }
    const ElementNode* findChildElementFromId(const char *pcszId) const;

    const AttributeNode* findAttribute(const char *pcszMatch) const;
    bool getAttributeValue(const char *pcszMatch, const char *&ppcsz) const;
    bool getAttributeValue(const char *pcszMatch, RTCString &str) const;
    bool getAttributeValuePath(const char *pcszMatch, RTCString &str) const;
    bool getAttributeValue(const char *pcszMatch, int32_t &i) const;
    bool getAttributeValue(const char *pcszMatch, uint32_t &i) const;
    bool getAttributeValue(const char *pcszMatch, int64_t &i) const;
    bool getAttributeValue(const char *pcszMatch, uint64_t &i) const;
    bool getAttributeValue(const char *pcszMatch, bool &f) const;

    ElementNode* createChild(const char *pcszElementName);

    ContentNode* addContent(const char *pcszContent);
    ContentNode* addContent(const RTCString &strContent)
    {
        return addContent(strContent.c_str());
    }

    AttributeNode* setAttribute(const char *pcszName, const char *pcszValue);
    AttributeNode* setAttribute(const char *pcszName, const RTCString &strValue)
    {
        return setAttribute(pcszName, strValue.c_str());
    }
    AttributeNode* setAttributePath(const char *pcszName, const RTCString &strValue);
    AttributeNode* setAttribute(const char *pcszName, int32_t i);
    AttributeNode* setAttribute(const char *pcszName, uint32_t i);
    AttributeNode* setAttribute(const char *pcszName, int64_t i);
    AttributeNode* setAttribute(const char *pcszName, uint64_t i);
    AttributeNode* setAttributeHex(const char *pcszName, uint32_t i);
    AttributeNode* setAttribute(const char *pcszName, bool f);

protected:
    // hide the default constructor so people use only our factory methods
    ElementNode(const ElementNode *pelmRoot, Node *pParent, xmlNode *plibNode);
    ElementNode(const ElementNode &x);      // no copying

    const ElementNode *m_pelmRoot;

    friend class Node;
    friend class Document;
    friend class XmlFileParser;
};

/**
 * Node subclass that represents content (non-element text).
 *
 * Since the Node constructor is private, one can create new content nodes
 * only through the following factory methods:
 *
 *  --  ElementNode::addContent()
 */
class RT_DECL_CLASS ContentNode : public Node
{
public:

protected:
    // hide the default constructor so people use only our factory methods
    ContentNode(Node *pParent, xmlNode *plibNode);
    ContentNode(const ContentNode &x);      // no copying

    friend class Node;
    friend class ElementNode;
};

/**
 * Node subclass that represents an attribute of an element.
 *
 * For attributes, Node::getName() returns the attribute name, and Node::getValue()
 * returns the attribute value, if any.
 *
 * Since the Node constructor is private, one can create new attribute nodes
 * only through the following factory methods:
 *
 *  --  ElementNode::setAttribute()
 */
class RT_DECL_CLASS AttributeNode : public Node
{
public:

protected:
    // hide the default constructor so people use only our factory methods
    AttributeNode(const ElementNode &elmRoot,
                  Node *pParent,
                  xmlAttr *plibAttr,
                  const char **ppcszKey);
    AttributeNode(const AttributeNode &x);      // no copying

    RTCString    m_strKey;

    friend class Node;
    friend class ElementNode;
};

/**
 * Handy helper class with which one can loop through all or some children
 * of a particular element. See NodesLoop::forAllNodes() for details.
 */
class RT_DECL_CLASS NodesLoop
{
public:
    NodesLoop(const ElementNode &node, const char *pcszMatch = NULL);
    ~NodesLoop();
    const ElementNode* forAllNodes() const;

private:
    /* Obscure class data */
    struct Data;
    Data *m;
};

/**
 * The XML document class. An instance of this needs to be created by a user
 * of the XML classes and then passed to
 *
 * --   XmlMemParser or XmlFileParser to read an XML document; those classes then
 *      fill the caller's Document with ElementNode, ContentNode and AttributeNode
 *      instances. The typical sequence then is:
 * @code
    Document doc;
    XmlFileParser parser;
    parser.read("file.xml", doc);
    Element *pelmRoot = doc.getRootElement();
   @endcode
 *
 * --   XmlMemWriter or XmlFileWriter to write out an XML document after it has
 *      been created and filled. Example:
 *
 * @code
    Document doc;
    Element *pelmRoot = doc.createRootElement();
    // add children
    xml::XmlFileWriter writer(doc);
    writer.write("file.xml", true);
   @endcode
 */
class RT_DECL_CLASS Document
{
public:
    Document();
    ~Document();

    Document(const Document &x);
    Document& operator=(const Document &x);

    const ElementNode* getRootElement() const;
    ElementNode* getRootElement();

    ElementNode* createRootElement(const char *pcszRootElementName,
                                   const char *pcszComment = NULL);

private:
    friend class XmlMemParser;
    friend class XmlFileParser;
    friend class XmlMemWriter;
    friend class XmlFileWriter;

    void refreshInternals();

    /* Obscure class data */
    struct Data;
    Data *m;
};

/*
 * XmlParserBase
 *
 */

class RT_DECL_CLASS XmlParserBase
{
protected:
    XmlParserBase();
    ~XmlParserBase();

    xmlParserCtxtPtr m_ctxt;
};

/*
 * XmlMemParser
 *
 */

class RT_DECL_CLASS XmlMemParser : public XmlParserBase
{
public:
    XmlMemParser();
    ~XmlMemParser();

    void read(const void* pvBuf, size_t cbSize, const RTCString &strFilename, Document &doc);
};

/*
 * XmlFileParser
 *
 */

class RT_DECL_CLASS XmlFileParser : public XmlParserBase
{
public:
    XmlFileParser();
    ~XmlFileParser();

    void read(const RTCString &strFilename, Document &doc);

private:
    /* Obscure class data */
    struct Data;
    struct Data *m;

    static int ReadCallback(void *aCtxt, char *aBuf, int aLen);
    static int CloseCallback (void *aCtxt);
};

/*
 * XmlMemParser
 *
 */

class RT_DECL_CLASS XmlMemWriter
{
public:
    XmlMemWriter();
    ~XmlMemWriter();

    void write(const Document &doc, void** ppvBuf, size_t *pcbSize);

private:
    void* m_pBuf;
};

/*
 * XmlFileWriter
 *
 */

class RT_DECL_CLASS XmlFileWriter
{
public:
    XmlFileWriter(Document &doc);
    ~XmlFileWriter();

    /**
     * Writes the XML document to the specified file.
     *
     * @param   pcszFilename    The name of the output file.
     * @param   fSafe           If @c true, some extra safety precautions will be
     *                          taken when writing the file:
     *                              -# The file is written with a '-tmp' suffix.
     *                              -# It is flushed to disk after writing.
     *                              -# Any original file is renamed to '-prev'.
     *                              -# The '-tmp' file is then renamed to the
     *                                 specified name.
     *                              -# The directory changes are flushed to disk.
     *                          The suffixes are available via s_pszTmpSuff and
     *                          s_pszPrevSuff.
     */
    void write(const char *pcszFilename, bool fSafe);

    static int WriteCallback(void *aCtxt, const char *aBuf, int aLen);
    static int CloseCallback(void *aCtxt);

    /** The suffix used by XmlFileWriter::write() for the temporary file. */
    static const char * const s_pszTmpSuff;
    /** The suffix used by XmlFileWriter::write() for the previous (backup) file. */
    static const char * const s_pszPrevSuff;

private:
    void writeInternal(const char *pcszFilename, bool fSafe);

    /* Obscure class data */
    struct Data;
    Data *m;
};

#if defined(_MSC_VER)
#pragma warning (default:4251)
#endif

/** @} */

} // end namespace xml

#endif /* !___iprt_xml_h */

