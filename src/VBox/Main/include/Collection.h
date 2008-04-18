/** @file
 *
 * VirtualBox collection templates
 */

/*
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

#ifndef ____H_COLLECTION
#define ____H_COLLECTION

#include "VBox/com/defs.h"
#include "VirtualBoxBase.h"

#include <list>
#include <vector>

/**
 *  Template class to create a non-thread safe implementation of the
 *  enumerator over the read-only collection that stores safe  interface pointers
 *  using std::vector. The enumerator is attached to an existing std::vector
 *  storing EnumItem instances, optionally making a copy.
 *
 *  The template also inherits the VirtualBoxSupportErrorInfoImpl template and
 *  therefore provides the error info support.
 *
 *  @param IEnum
 *      enumerator interface to implement. This interface must define
 *      HasMore (BOOL *) and GetNext (IEnumItem **).
 *  @param IEnumItem
 *      enumerator item interface. Pointers of this type are returned
 *      by GetNext().
 *  @param EnumItem
 *      actual enumerator item class. Instances of this class
 *      are stored in the std::vector passed as an argument to
 *      init(). This class must be a ComPtrBase<> template instantiation
 *      or derived from such instantiation.
 *  @param ComponentClass
 *      the only role of this class is to have the following member:
 *      |public: static const wchar_t *getComponentName()|, that returns the
 *      component name (see VirtualBoxSupportErrorInfoImpl template for more info).
 */
template <class IEnum, class IEnumItem, class EnumItem, class ComponentClass>
class ATL_NO_VTABLE IfaceVectorEnumerator :
    public VirtualBoxSupportErrorInfoImpl <IfaceVectorEnumerator <IEnum, IEnumItem,
                                           EnumItem, ComponentClass>, IEnum>,
#ifdef RT_OS_WINDOWS
    public CComObjectRootEx <CComSingleThreadModel>,
#else
    public CComObjectRootEx,
#endif
    public IEnum
{
    Q_OBJECT

public:

    typedef std::vector <EnumItem> Vector;

    DECLARE_NOT_AGGREGATABLE(IfaceVectorEnumerator)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(IfaceVectorEnumerator)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IEnum)
    END_COM_MAP()

    IfaceVectorEnumerator()
    {
        parent = NULL;
        vec = NULL;
        allocated = true;
    }

    virtual ~IfaceVectorEnumerator()
    {
        if (vec && allocated)
            delete vec;
        if (parent)
            parent->Release();
    }

    // public initializer/uninitializer for internal purposes only
    void init (IUnknown *p, const Vector &v, bool readonly = true)
    {
        parent = p;
        if (parent)
            parent->AddRef();
        if (readonly)
            vec = &v;
        else
            vec = new Vector (v);
        allocated = !readonly;
        iter = vec->begin();
    }

    STDMETHOD(HasMore) (BOOL *more)
    {
        if (!more)
            return E_POINTER;
        *more = iter != vec->end();
        return S_OK;
    }

    STDMETHOD(GetNext) (IEnumItem **next)
    {
        if (!next)
            return E_POINTER;
        *next = NULL;
        if (iter == vec->end())
            return this->setError (E_UNEXPECTED, VirtualBoxBase::translate (
                "IfaceVectorEnumerator", "No more elements"));
        typename Vector::value_type item = *iter;
        ++iter;
        return item.queryInterfaceTo (next);
    }

    // for VirtualBoxSupportErrorInfoImpl
    inline static const wchar_t *getComponentName() {
        return ComponentClass::getComponentName();
    }

private:

    IUnknown *parent;
    const Vector *vec;
    bool allocated;
    typename Vector::const_iterator iter;
};

/**
 *  Template class to create a non-thread safe implementation of the
 *  read-only collection that stores interface pointers. The collection is
 *  initialized from the std::list storing CollItem instances by copying
 *  (i.e. making a snapshot of) all list items to std::vector for
 *  optimized random access.
 *
 *  The template also inherits the VirtualBoxSupportErrorInfoImpl template and
 *  therefore provides the error info support.
 *
 *  @param IColl
 *      collection interface to implement. This interface must define
 *      Count(ULONG *), GetItemAt (ULONG, ICollItem**) and Enumerate (IEnum **).
 *  @param ICollItem
 *      collection item interface. Pointers of this type are returned by
 *      GetItemAt().
 *  @param IEnum
 *      enumerator interface. Pointers of this type are returned by Enumerate().
 *  @param CollItem
 *      actual collection item class. Instances of this class
 *      are stored in the std::list passed as an argument to
 *      init() and in the internal std::vector. This class must be a
 *      ComPtrBase<> template instantiation or derived from such instantiation.
 *  @param Enum
 *      enumerator implementation class used to construct a new enumerator.
 *      This class must be a IfaceVectorEnumerator<> template instantiation
 *      with IEnum and IEnumItem arguments exactly the same as in this template,
 *      and with EnumItem argument exactly the same as this template's
 *      CollItem argument.
 *  @param ComponentClass
 *      the only role of this class is to have the following member:
 *      |public: static const wchar_t *getComponentName()|, that returns the
 *      component name (see VirtualBoxSupportErrorInfoImpl template for more info).
 */
template <class IColl, class ICollItem, class IEnum, class CollItem, class Enum,
          class ComponentClass>
class ATL_NO_VTABLE ReadonlyIfaceVector :
    public VirtualBoxSupportErrorInfoImpl <ReadonlyIfaceVector <IColl, ICollItem,
                                           IEnum, CollItem, Enum, ComponentClass>, IColl>,
#ifdef RT_OS_WINDOWS
    public CComObjectRootEx <CComSingleThreadModel>,
#else
    public CComObjectRootEx,
#endif
    public IColl
{
    Q_OBJECT

public:

    typedef std::vector <CollItem> Vector;
    typedef std::list <CollItem> List;

    DECLARE_NOT_AGGREGATABLE(ReadonlyIfaceVector)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(ReadonlyIfaceVector)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IColl)
    END_COM_MAP()

    // public initializer/uninitializer for internal purposes only

    void init (const List &l)
    {
        // create a copy of the list
        vec = Vector (l.begin(), l.end());
    }

    template <class Key>
    void init (const std::map <Key, CollItem> &m)
    {
        // create a copy of the map
        for (typename std::map <Key, CollItem>::const_iterator it = m.begin();
             it != m.end(); ++ it)
            vec.push_back (it->second);
    }

    STDMETHOD(COMGETTER(Count)) (ULONG *count)
    {
        if (!count)
            return E_POINTER;
        *count = (ULONG) vec.size();
        return S_OK;
    }

    STDMETHOD(GetItemAt) (ULONG index, ICollItem **item)
    {
        if (!item)
            return E_POINTER;
        *item = NULL;
        if (index >= vec.size())
            return this->setError (E_INVALIDARG, VirtualBoxBase::translate (
                "ReadonlyIfaceVector", "The specified index is out of range"));
        return vec [index].queryInterfaceTo (item);
    }

    STDMETHOD(Enumerate) (IEnum **enumerator)
    {
        if (!enumerator)
            return E_POINTER;
        *enumerator = NULL;
        ComObjPtr <Enum> enumObj;
        HRESULT rc = enumObj.createObject();
        if (SUCCEEDED (rc))
        {
            enumObj->init ((IColl *) this, vec);
            rc = enumObj.queryInterfaceTo (enumerator);
        }
        return rc;
    }

    // for VirtualBoxSupportErrorInfoImpl
    inline static const wchar_t *getComponentName() {
        return ComponentClass::getComponentName();
    }

protected:

    Vector vec;
};

/**
 *  This macro declares an enumerator class and a collection class that stores
 *  elements of the given class @a itemcls that implements the given
 *  interface @a iface
 *
 *  The the @a itemcls class must be either a ComObjPtr or a ComPtr template
 *  instantiation with the argument being a class that implements the @a iface
 *  interface.
 *
 *  The namespace of the collection class remains opened after
 *  this macro is expanded (i.e. no closing brace with semicolon), which
 *  allows to declare extra collection class members. This namespace
 *  must be closed by the COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END macro.
 *
 *  For example, given |ComObjPtr <OSomeItem>|, |ISomeItem|| and |OSomeItem|
 *  arguments, this macro will generate the following code:
 *
 *  <code>
 *      class OSomeItemEnumerator : public
 *          IfaceVectorEnumerator <ISomeItemEnumerator, ISomeItem, ComObjPtr <OSomeItem>,
 *                                 OSomeItemEnumerator>
 *      {...};
 *      class SomeItemCollection : public
 *          ReadonlyIfaceVector <ISomeItemCollection, ISomeItem, ComObjPtr <OSomeItem>,
 *                               OSomeItemEnumerator, OSomeItemCollection>
 *      {...};
 *  </code>
 *
 *  i.e. it assumes that ISomeItemEnumerator, ISomeItem and ISomeItemCollection
 *  are existing interfaces, and OSomeItem implements the ISomeItem interface.
 *  It also assumes, that std::list passed to SomeItemCollection::init()
 *  stores objects of the @a itemcls class (|ComObjPtr <OSomeItem>| in the
 *  example above).
 *
 *  See descriptions of the above IfaceVectorEnumerator and 
 *  ReadonlyIfaceVector templates for more info.
 *
 *  The generated class also inherits the VirtualBoxSupportTranslation template,
 *  providing the support for translation of string constants within class
 *  members.
 *
 *  The macro is best to be placed in the header after SomeItem class
 *  declaration. The COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END macro must
 *  follow all extra member declarations of the collection class, or right
 *  after this macro if the collection doesn't have extra members.
 *
 *  @param  itemcls     Either ComObjPtr or ComPtr for the class that implements
 *                      the given interface of items to be stored in the
 *                      collection
 *  @param  iface       Interface of items implemented by the @a itemcls class 
 *  @param  prefix      Prefix to apply to generated enumerator and collection
 *                      names.
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN(itemcls, iface, prefix) \
    class prefix##Enumerator \
        : public IfaceVectorEnumerator \
            <iface##Enumerator, iface, itemcls, prefix##Enumerator> \
        , public VirtualBoxSupportTranslation <prefix##Enumerator> \
    { \
        NS_DECL_ISUPPORTS \
        public: static const wchar_t *getComponentName() { \
            return WSTR_LITERAL (prefix) L"Enumerator"; \
        } \
    }; \
    class prefix##Collection \
        : public ReadonlyIfaceVector \
            <iface##Collection, iface, iface##Enumerator, itemcls, prefix##Enumerator, \
             prefix##Collection> \
        , public VirtualBoxSupportTranslation <prefix##Collection> \
    { \
        NS_DECL_ISUPPORTS \
        public: static const wchar_t *getComponentName() { \
            return WSTR_LITERAL (prefix) L"Collection"; \
        }

/**
 *  This macro is a counterpart to COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN
 *  and must be always used to finalize the collection declaration started
 *  by that macro.
 *
 *  Currently the macro just expands to the closing brace with semicolon,
 *  but this might change in the future.
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END(itemcls, iface, prefix) \
    };

/**
 *  This is a "shortcut" macro, for convenience. It expands exactly to:
 *  <code>
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN(itemcls, iface, prefix)
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END(itemcls, iface, prefix)
 *  </code>
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_EX(itemcls, iface, prefix) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN (itemcls, iface, prefix) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END (itemcls, iface, prefix)

/**
 *  This macro declares an enumerator class and a collection class for the
 *  given item class @a c.
 *
 *  It's a convenience macro that deduces all arguments to the
 *  COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN macro from a single @a c
 *  class name argument. Given a class named |SomeItem|, this macro is
 *  equivalent to
 *  <code>
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN (ComObjPtr <SomeItem>, ISomeItem, SomeItem)
 *  </code>
 *  and will generate the following code:
 *  <code>
 *      class OSomeItemEnumerator : public
 *          IfaceVectorEnumerator <ISomeItemEnumerator, ISomeItem, ComObjPtr <SomeItem>,
 *                                 SomeItemEnumerator>
 *      {...};
 *      class SomeItemCollection : public
 *          ReadonlyIfaceVector <ISomeItemCollection, ISomeItem, ComObjPtr <SomeItem>,
 *                               SomeItemEnumerator, SomeItemCollection>
 *      {...};
 *  </code>
 *
 *  See COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN for the detailed
 *  description.
 *
 *  The macro is best to be placed in the header after SomeItem class
 *  declaration. The COM_DECL_READONLY_ENUM_AND_COLLECTION_END macro must follow
 *  all extra member declarations of the collection class, or right after this
 *  macro if the collection doesn't have extra members.
 *
 *  @param c    Component class implementing the interface of items to be stored
 *              in the collection
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN(c) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN (ComObjPtr <c>, I##c, c)

/**
 *  This macro is a counterpart to COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN
 *  and must be always used to finalize the collection declaration started
 *  by that macro.
 *
 *  This is a "shortcut" macro that expands exactly to:
 *  <code>
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END (ComObjPtr <c>, I##c, c)
 *  </code>
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_END(c) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END (ComObjPtr <c>, I##c, c)

/**
 *  This is a "shortcut" macro, for convenience. It expands exactly to:
 *  <code>
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN(c)
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_END(c)
 *  </code>
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION(c) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN(c) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_END(c)

/**
 *  This macro declares an enumerator class and a collection class for the
 *  given item interface @a iface prefixed with the given @a prefix.
 *
 *  It's a convenience macro that deduces all arguments to the
 *  COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN macro from the two given
 *  @a iface and @a prefix arguments. Given an interface named |ISomeItem|,
 *  and a prefix SomeItem this macro is equivalent to
 *  <code>
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN (ComPtr <ISomeItem>, ISomeItem, SomeItem)
 *  </code>
 *  and will generate the following code:
 *  <code>
 *      class OSomeItemEnumerator : public
 *          IfaceVectorEnumerator <ISomeItemEnumerator, ISomeItem, ComPtr <ISomeItem>,
 *                                 SomeItemEnumerator>
 *      {...};
 *      class SomeItemCollection : public
 *          ReadonlyIfaceVector <ISomeItemCollection, ISomeItem, ComPtr <ISomeItem>,
 *                               SomeItemEnumerator, SomeItemCollection>
 *      {...};
 *  </code>
 *
 *  See COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN for the detailed
 *  description.
 *
 *  The macro is best to be placed in the header after SomeItem class
 *  declaration. The COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END macro must follow
 *  all extra member declarations of the collection class, or right after this
 *  macro if the collection doesn't have extra members.
 *
 *  @param prefix   Prefix prepended to the generated collection and
 *                  enumerator classes
 *  @param iface    Interface class
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN(prefix, iface) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN (ComPtr <iface>, iface, prefix)

/**
 *  This macro is a counterpart to COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN
 *  and must be always used to finalize the collection declaration started
 *  by that macro.
 * 
 *  This is a "shortcut" macro that expands exactly to:
 *  <code>
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END (ComObjPtr <c>, I##c, c)
 *  </code>
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END(prefix, iface) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_END (ComPtr <iface>, iface, prefix)

/**
 *  This is a "shortcut" macro, for convenience. It expands exactly to:
 *  <code>
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN(c)
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END(c)
 *  </code>
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_AS(prefix, iface) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN (prefix, iface) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END (prefix, iface)

#ifdef RT_OS_WINDOWS

#define COM_IMPL_READONLY_ENUM_AND_COLLECTION_EX(itemcls, iface, prefix)
#define COM_IMPL_READONLY_ENUM_AND_COLLECTION(c)
#define COM_IMPL_READONLY_ENUM_AND_COLLECTION_AS(prefix, iface)

#else // !RT_OS_WINDOWS

/**
 *  This macro defines nsISupports implementations (i.e. QueryInterface(),
 *  AddRef() and Release()) for the enumerator and collection classes
 *  declared by the COM_DECL_READONLY_ENUM_AND_COLLECTION_EX,
 *  COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN and
 *  COM_DECL_READONLY_ENUM_AND_COLLECTION_EX macros.
 *
 *  See COM_DECL_READONLY_ENUM_AND_COLLECTION_EX_BEGIN for the detailed
 *  description.
 *
 *  The macro should be placed in one of the source files.
 *
 *  @note
 *      this macro is XPCOM-specific and not necessary for MS COM,
 *      so expands to nothing on Win32.
 *
 *  @param  itemcls     Either ComObjPtr or ComPtr for the class that implements
 *                      the given interface of items to be stored in the
 *                      collection
 *  @param  iface       Interface of items implemented by the @a itemcls class 
 *  @param  prefix      Prefix to apply to generated enumerator and collection
 *                      names.
 */
#define COM_IMPL_READONLY_ENUM_AND_COLLECTION_EX(itemcls, iface, prefix) \
    NS_DECL_CLASSINFO(prefix##Collection) \
    NS_IMPL_THREADSAFE_ISUPPORTS1_CI(prefix##Collection, iface##Collection) \
    NS_DECL_CLASSINFO(prefix##Enumerator) \
    NS_IMPL_THREADSAFE_ISUPPORTS1_CI(prefix##Enumerator, iface##Enumerator)

/**
 *  This macro defines nsISupports implementations (i.e. QueryInterface(),
 *  AddRef() and Release()) for the enumerator and collection classes
 *  declared by the COM_DECL_READONLY_ENUM_AND_COLLECTION,
 *  COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN and
 *  COM_DECL_READONLY_ENUM_AND_COLLECTION_END macros.
 *
 *  See COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN for the detailed
 *  description.
 *
 *  The macro should be placed in one of the source files.
 *
 *  @note
 *      this macro is XPCOM-specific and not necessary for MS COM,
 *      so expands to nothing on Win32.
 *
 *  @param c    Component class implementing the interface of items to be stored
 *              in the collection
 */
#define COM_IMPL_READONLY_ENUM_AND_COLLECTION(c) \
    COM_IMPL_READONLY_ENUM_AND_COLLECTION_EX (0, I##c, c) 

/**
 *  This macro defines nsISupports implementations (i.e. QueryInterface(),
 *  AddRef() and Release()) for the enumerator and collection classes
 *  declared by the COM_DECL_READONLY_ENUM_AND_COLLECTION_AS,
 *  COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN and
 *  COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END macros.
 *
 *  See COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN for the detailed
 *  description.
 *
 *  The macro should be placed in one of the source files.
 *
 *  @note
 *      this macro is XPCOM-specific and not necessary for MS COM,
 *      so expands to nothing on Win32.
 *
 *  @param prefix   Prefix prepended to the generated collection and
 *                  enumerator classes
 *  @param iface    Interface class
 */
#define COM_IMPL_READONLY_ENUM_AND_COLLECTION_AS(prefix, iface) \
    COM_IMPL_READONLY_ENUM_AND_COLLECTION_EX (0, iface, prefix) 

#endif // !RT_OS_WINDOWS

#endif // ____H_COLLECTION
