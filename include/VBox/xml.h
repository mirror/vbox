/** @file
 * VirtualBox XML helper APIs.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_vboxxml_h
#define ___VBox_vboxxml_h

#ifndef IN_RING3
# error "There are no XML APIs available in Ring-0 Context!"
#else /* IN_RING3 */

/** @def IN_VBOXXML_R3
 * Used to indicate whether we're inside the same link module as the
 * XML Settings File Manipulation API.
 *
 * @todo should go to a separate common include together with VBOXXML2_CLASS
 * once there becomes more than one header in the VBoxXML2 library.
 */
#ifdef DOXYGEN_RUNNING
# define IN_VBOXXML_R3
#endif

/** @def VBOXXML_CLASS
 * Class export/import wrapper. */
#ifdef IN_VBOXXML_R3
# define VBOXXML_CLASS DECLEXPORT_CLASS
#else
# define VBOXXML_CLASS DECLIMPORT_CLASS
#endif

#include "VBox/com/string.h"

/*
 * Shut up MSVC complaining that auto_ptr[_ref] template instantiations (as a
 * result of private data member declarations of some classes below) need to
 * be exported too to in order to be accessible by clients.
 *
 * The alternative is to instantiate a template before the data member
 * declaration with the VBOXXML_CLASS prefix, but the standard disables
 * explicit instantiations in a foreign namespace. In other words, a declaration
 * like:
 *
 *   template class VBOXXML_CLASS std::auto_ptr <Data>;
 *
 * right before the member declaration makes MSVC happy too, but this is not a
 * valid C++ construct (and G++ spits it out). So, for now we just disable the
 * warning and will come back to this problem one day later.
 *
 * We also disable another warning (4275) saying that a DLL-exported class
 * inherits form a non-DLL-exported one (e.g. settings::ENoMemory ->
 * std::bad_alloc). I can't get how it can harm yet.
 */
#if defined(_MSC_VER)
#pragma warning (disable:4251)
#pragma warning (disable:4275)
#endif

/* Forwards */
typedef struct _xmlParserInput xmlParserInput;
typedef xmlParserInput *xmlParserInputPtr;
typedef struct _xmlParserCtxt xmlParserCtxt;
typedef xmlParserCtxt *xmlParserCtxtPtr;
typedef struct _xmlError xmlError;
typedef xmlError *xmlErrorPtr;

namespace xml
{

// Exceptions
//////////////////////////////////////////////////////////////////////////////

/**
 * Base exception class.
 */
class VBOXXML_CLASS Error : public std::exception
{
public:

    Error(const char *aMsg = NULL)
        : m(aMsg) {}

    virtual ~Error() throw() {}

    void setWhat (const char *aMsg) { m = aMsg; }

    const char* what() const throw() { return m.c_str(); }

private:

//     Error() {};     // hide the default constructor to make sure the extended one above is always used

    com::Utf8Str m;
};

class VBOXXML_CLASS LogicError : public Error
{
public:

    LogicError (const char *aMsg = NULL)
        : xml::Error(aMsg)
    {}

    LogicError (RT_SRC_POS_DECL);
};

class VBOXXML_CLASS RuntimeError : public Error
{
public:

    RuntimeError (const char *aMsg = NULL) : Error (aMsg) {}
};

class VBOXXML_CLASS XmlError : public RuntimeError
{
public:
    XmlError(xmlErrorPtr aErr);

    static char *Format(xmlErrorPtr aErr);
};

// Logical errors
//////////////////////////////////////////////////////////////////////////////

class VBOXXML_CLASS ENotImplemented : public LogicError
{
public:
    ENotImplemented(const char *aMsg = NULL) : LogicError(aMsg) {}
    ENotImplemented(RT_SRC_POS_DECL) : LogicError(RT_SRC_POS_ARGS) {}
};

class VBOXXML_CLASS EInvalidArg : public LogicError
{
public:
    EInvalidArg(const char *aMsg = NULL) : LogicError(aMsg) {}
    EInvalidArg(RT_SRC_POS_DECL) : LogicError(RT_SRC_POS_ARGS) {}
};

class VBOXXML_CLASS EDocumentNotEmpty : public LogicError
{
public:
    EDocumentNotEmpty(const char *aMsg = NULL) : LogicError(aMsg) {}
    EDocumentNotEmpty(RT_SRC_POS_DECL) : LogicError(RT_SRC_POS_ARGS) {}
};

class VBOXXML_CLASS ENodeIsNotElement : public LogicError
{
public:
    ENodeIsNotElement(const char *aMsg = NULL) : LogicError(aMsg) {}
    ENodeIsNotElement(RT_SRC_POS_DECL) : LogicError(RT_SRC_POS_ARGS) {}
};

// Runtime errors
//////////////////////////////////////////////////////////////////////////////

class VBOXXML_CLASS ENoMemory : public RuntimeError, public std::bad_alloc
{
public:
    ENoMemory(const char *aMsg = NULL) : RuntimeError (aMsg) {}
    virtual ~ENoMemory() throw() {}
};

class VBOXXML_CLASS EIPRTFailure : public RuntimeError
{
public:

    EIPRTFailure (int aRC);

    int rc() const { return mRC; }

private:
    int mRC;
};


/**
 * The Stream class is a base class for I/O streams.
 */
class VBOXXML_CLASS Stream
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
class VBOXXML_CLASS Input : virtual public Stream
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
class VBOXXML_CLASS Output : virtual public Stream
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
class VBOXXML_CLASS File : public Input, public Output
{
public:

    /**
     * Possible file access modes.
     */
    enum Mode { Mode_Read, Mode_Write, Mode_ReadWrite };

    /**
     * Opens a file with the given name in the given mode. If @a aMode is Read
     * or ReadWrite, the file must exist. If @a aMode is Write, the file must
     * not exist. Otherwise, an EIPRTFailure excetion will be thrown.
     *
     * @param aMode     File mode.
     * @param aFileName File name.
     */
    File (Mode aMode, const char *aFileName);

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
     */
    File (RTFILE aHandle, const char *aFileName = NULL);

    /**
     * Destroys the File object. If the object was created from a file name
     * the corresponding file will be automatically closed. If the object was
     * created from a file handle, it will remain open.
     */
    virtual ~File();

    const char *uri() const;

    uint64_t pos() const;
    void setPos (uint64_t aPos);

    /**
     * See Input::read(). If this method is called in wrong file mode,
     * LogicError will be thrown.
     */
    int read (char *aBuf, int aLen);

    /**
     * See Output::write(). If this method is called in wrong file mode,
     * LogicError will be thrown.
     */
    int write (const char *aBuf, int aLen);

    /**
     * See Output::truncate(). If this method is called in wrong file mode,
     * LogicError will be thrown.
     */
    void truncate();

private:

    /* Obscure class data */
    struct Data;
    std::auto_ptr <Data> m;

    /* auto_ptr data doesn't have proper copy semantics */
    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (File)
};

/**
 * The MemoryBuf class represents a stream implementation that reads from the
 * memory buffer.
 */
class VBOXXML_CLASS MemoryBuf : public Input
{
public:

    MemoryBuf (const char *aBuf, size_t aLen, const char *aURI = NULL);

    virtual ~MemoryBuf();

    const char *uri() const;

    int read (char *aBuf, int aLen);
    uint64_t pos() const;
    void setPos (uint64_t aPos);

private:
    /* Obscure class data */
    struct Data;
    std::auto_ptr <Data> m;

    /* auto_ptr data doesn't have proper copy semantics */
    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (MemoryBuf)
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

class VBOXXML_CLASS GlobalLock
{
public:
    GlobalLock();
    ~GlobalLock();

    void setExternalEntityLoader(PFNEXTERNALENTITYLOADER pFunc);

    static xmlParserInput* callDefaultLoader(const char *aURI,
                                             const char *aID,
                                             xmlParserCtxt *aCtxt);

private:
    /* Obscure class data */
    struct Data;
    std::auto_ptr<Data> m;
};

/**
 * Node:
 *  an XML node, which represents either an element or text content
 *  or an attribute.
 *
 *  For elements, getName() returns the element name, and getValue()
 *  returns the text contents, if any.
 *
 *  For attributes, getName() returns the attribute name, and getValue()
 *  returns the attribute value, if any.
 *
 *  Since the default constructor is private, one can create new nodes
 *  only through factory methods provided by the XML classes. These are:
 *
 *  --  xml::Document::createRootElement()
 *  --  xml::Node::createChild()
 *  --  xml::Node::addContent()
 *  --  xml::Node::setAttribute()
 */

class ElementNode;
typedef std::list<const ElementNode*> ElementNodesList;

class AttributeNode;

class ContentNode;

class VBOXXML_CLASS Node
{
public:
    ~Node();

    const char* getName() const;
    const char* getValue() const;
    bool copyValue(int32_t &i) const;
    bool copyValue(uint32_t &i) const;
    bool copyValue(int64_t &i) const;
    bool copyValue(uint64_t &i) const;

    int getLineNumber() const;

    int isElement()
    {
        return mType == IsElement;
    }

protected:
    typedef enum {IsElement, IsAttribute, IsContent} EnumType;
    EnumType mType;

    // hide the default constructor so people use only our factory methods
    Node(EnumType type);
    Node(const Node &x);      // no copying

    void buildChildren();

    /* Obscure class data */
    struct Data;
    Data *m;
};

class VBOXXML_CLASS ElementNode : public Node
{
public:
    int getChildElements(ElementNodesList &children,
                         const char *pcszMatch = NULL) const;

    const ElementNode* findChildElement(const char *pcszMatch) const;
    const ElementNode* findChildElementFromId(const char *pcszId) const;

    const AttributeNode* findAttribute(const char *pcszMatch) const;
    bool getAttributeValue(const char *pcszMatch, com::Utf8Str &str) const;
    bool getAttributeValue(const char *pcszMatch, int64_t &i) const;
    bool getAttributeValue(const char *pcszMatch, uint64_t &i) const;

    ElementNode* createChild(const char *pcszElementName);
    ContentNode* addContent(const char *pcszContent);
    AttributeNode* setAttribute(const char *pcszName, const char *pcszValue);

protected:
    // hide the default constructor so people use only our factory methods
    ElementNode();
    ElementNode(const ElementNode &x);      // no copying

    friend class Node;
    friend class Document;
    friend class XmlFileParser;
};

class VBOXXML_CLASS ContentNode : public Node
{
public:

protected:
    // hide the default constructor so people use only our factory methods
    ContentNode();
    ContentNode(const ContentNode &x);      // no copying

    friend class Node;
    friend class ElementNode;
};

class VBOXXML_CLASS AttributeNode : public Node
{
public:

protected:
    // hide the default constructor so people use only our factory methods
    AttributeNode();
    AttributeNode(const AttributeNode &x);      // no copying

    friend class Node;
    friend class ElementNode;
};

/*
 * NodesLoop
 *
 */

class VBOXXML_CLASS NodesLoop
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

/*
 * Document
 *
 */

class VBOXXML_CLASS Document
{
public:
    Document();
    ~Document();

    Document(const Document &x);
    Document& operator=(const Document &x);

    const ElementNode* getRootElement() const;

    ElementNode* createRootElement(const char *pcszRootElementName);

private:
    friend class XmlFileParser;
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

class VBOXXML_CLASS XmlParserBase
{
protected:
    XmlParserBase();
    ~XmlParserBase();

    xmlParserCtxtPtr m_ctxt;
};

/*
 * XmlFileParser
 *
 */

class VBOXXML_CLASS XmlFileParser : public XmlParserBase
{
public:
    XmlFileParser();
    ~XmlFileParser();

    void read(const char *pcszFilename, Document &doc);

private:
    /* Obscure class data */
    struct Data;
    std::auto_ptr<Data> m;

    static int ReadCallback(void *aCtxt, char *aBuf, int aLen);
    static int CloseCallback (void *aCtxt);
};

/*
 * XmlFileWriter
 *
 */

class VBOXXML_CLASS XmlFileWriter
{
public:
    XmlFileWriter(Document &doc);
    ~XmlFileWriter();

    void write(const char *pcszFilename);

    static int WriteCallback(void *aCtxt, const char *aBuf, int aLen);
    static int CloseCallback (void *aCtxt);

private:
    /* Obscure class data */
    struct Data;
    Data *m;
};

#if defined(_MSC_VER)
#pragma warning (default:4251)
#endif

#endif /* IN_RING3 */

/** @} */

} // end namespace xml

#endif /* ___VBox_vboxxml_h */
