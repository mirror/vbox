/** @file
 *
 * VirtualBox collection templates
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
#ifdef __WIN__
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
#ifdef __WIN__
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
 *  This macro declares an enumerator class and a collection class for the
 *  given item class. The namespace of the collection class remains opened after
 *  this macro is expanded (i.e. no closing brace with semicolon), thus allowing
 *  one to declare extra collection class members. This namespace
 *  must be closed by the COM_DECL_READONLY_ENUM_AND_COLLECTION_END macro.
 *
 *  For example, given a 'SomeItem' argument, this macro will declare the
 *  following:
 *
 *  <code>
 *      class SomeItemEnumerator : public
 *          IfaceVectorEnumerator <ISomeItemEnumerator, ISomeItem, ComObjPtr <SomeItem>,
 *              SomeItemEnumerator> {...};
 *      class SomeItemCollection : public
 *          ReadonlyIfaceVector <ISomeItemCollection, ISomeItem, ComObjPtr <SomeItem>,
 *              SomeItemEnumerator, SomeItemCollection> {
 *  </code>
 *
 *  i.e. it assumes that ISomeItemEnumerator, ISomeItem and ISomeItemCollection
 *  are existing interfaces, and SomeItem implements the ISomeItem interface.
 *  It also assumes, that std::list passed to SomeItemCollection::init()
 *  stores objects of |ComObjPtr <SomeItem>| class, i.e. safe pointers around
 *  SomeItem instances.
 *
 *  See descriptions of the above two templates for more info.
 *
 *  The generated class also inherits the VirtualBoxSupportTranslation template,
 *  providing the support for translation of string constants within class
 *  members.
 *
 *  The macro is best to be placed in the header after SomeItem class
 *  declaration. The COM_DECL_READONLY_ENUM_AND_COLLECTION_END macro must follow
 *  all extra member declarations of the collection class, or right after this
 *  macro if the collection doesn't have extra members.
 *
 *  @param c    component class implementing the interface of items to be stored
 *              in the collection
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN(c) \
    class c##Enumerator \
        : public IfaceVectorEnumerator \
            <I##c##Enumerator, I##c, ComObjPtr <c>, c##Enumerator> \
        , public VirtualBoxSupportTranslation <c##Enumerator> \
    { \
        NS_DECL_ISUPPORTS \
        public: static const wchar_t *getComponentName() { \
            return WSTR_LITERAL (c) L"Enumerator"; \
        } \
    }; \
    class c##Collection \
        : public ReadonlyIfaceVector \
            <I##c##Collection, I##c, I##c##Enumerator, ComObjPtr <c>, c##Enumerator, \
         c##Collection> \
        , public VirtualBoxSupportTranslation <c##Collection> \
    { \
        NS_DECL_ISUPPORTS \
        public: static const wchar_t *getComponentName() { \
            return WSTR_LITERAL (c) L"Collection"; \
        }

/**
 *  This macro is a counterpart to COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN
 *  and must be always used to finalize the collection declaration started
 *  by that macro.
 *
 *  Currently the macro just expands to the closing brace with semicolon,
 *  but this might change in the future.
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_END(c) \
    };

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
 *  given item class and interface. The namespace of the collection class remains
 *  opened after this macro is expanded (i.e. no closing brace with semicolon),
 *  thus allowing one to declare extra collection class members. This namespace
 *  must be closed by the COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END macro.
 *
 *  For example, given a 'SomeItem' and a 'ISomeItem' arguments, this macro will
 *  declare the following:
 *
 *  <code>
 *      class SomeItemEnumerator_ComponentName {...};
 *      class SomeItemCollection_ComponentName {...};
 *
 *      class SomeItemEnumerator : public
 *          IfaceVectorEnumerator <ISomeItemEnumerator, ISomeItem, ComPtr <ISomeItem>,
 *              SomeItemEnumerator_ComponentName> {...};
 *      class SomeItemCollection : public
 *          ReadonlyIfaceVector <ISomeItemCollection, ISomeItem, ComPtr <ISomeItem>,
 *              SomeItemEnumerator, SomeItemCollection_ComponentName> {...};
 *  </code>
 *
 *  i.e. it assumes that ISomeItemEnumerator, ISomeItem and ISomeItemCollection
 *  are existing interfaces. It also assumes, that std::list passed to
 *  SomeItemCollection::init() stores objects of |ComPtr <ISomeItem>| class, i.e
 *  safe ISomeItem interface pointers.
 *
 *  See descriptions of the above two templates for more info.
 *
 *  The generated class also inherits the VirtualBoxSupportTranslation template,
 *  providing the support for translation of string constants within class
 *  members.
 *
 *  The macro is best to be placed in the header after SomeItem class
 *  declaration. The COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END macro must follow
 *  all extra member declarations of the collection class, or right after this
 *  macro if the collection doesn't have extra members.
 *
 *  @param prefix   the prefix prepended to the generated collection and
 *                  enumerator classes
 *  @param iface    interface class
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN(prefix, iface) \
    class prefix##Enumerator \
        : public IfaceVectorEnumerator \
            <iface##Enumerator, iface, ComPtr <iface>, prefix##Enumerator> \
        , public VirtualBoxSupportTranslation <prefix##Enumerator> \
    { \
        NS_DECL_ISUPPORTS \
        public: static const wchar_t *getComponentName() { \
            return WSTR_LITERAL (prefix) L"Enumerator"; \
        } \
    }; \
    class prefix##Collection \
        : public ReadonlyIfaceVector \
            <iface##Collection, iface, iface##Enumerator, ComPtr <iface>, prefix##Enumerator, \
             prefix##Collection> \
        , public VirtualBoxSupportTranslation <prefix##Collection> \
    { \
        NS_DECL_ISUPPORTS \
        public: static const wchar_t *getComponentName() { \
            return WSTR_LITERAL (prefix) L"Collection"; \
        }

/**
 *  This macro is a counterpart to COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN
 *  and must be always used to finalize the collection declaration started
 *  by that macro.
 *
 *  Currently the macro just expands to the closing brace with semicolon,
 *  but this might change in the future.
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END(prefix, iface) \
    };

/**
 *  This is a "shortcut" macro, for convenience. It expands exactly to:
 *  <code>
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN(c)
 *      COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END(c)
 *  </code>
 */
#define COM_DECL_READONLY_ENUM_AND_COLLECTION_AS(prefix, iface) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_BEGIN(prefix, iface) \
    COM_DECL_READONLY_ENUM_AND_COLLECTION_AS_END(prefix, iface)

#ifdef __WIN__

#define COM_IMPL_READONLY_ENUM_AND_COLLECTION(c)
#define COM_IMPL_READONLY_ENUM_AND_COLLECTION_AS(prefix, iface)
#else // !__WIN__

/**
 *  This macro defines nsISupports implementations (i.e. QueryInterface(),
 *  AddRef() and Release()) for the enumerator and collection classes
 *  declared by the COM_DECL_READONLY_ENUM_AND_COLLECTION macro.
 *
 *  The macro should be placed in one of the source files.
 *
 *  @note
 *      this macro is XPCOM-specific and not necessary for MS COM,
 *      so expands to nothing on Win32.
 *
 *  @param c    component class implementing the item interface
 */
#define COM_IMPL_READONLY_ENUM_AND_COLLECTION(c) \
    NS_DECL_CLASSINFO(c##Collection) \
    NS_IMPL_THREADSAFE_ISUPPORTS1_CI(c##Collection, I##c##Collection) \
    NS_DECL_CLASSINFO(c##Enumerator) \
    NS_IMPL_THREADSAFE_ISUPPORTS1_CI(c##Enumerator, I##c##Enumerator)

/**
 *  This macro defines nsISupports implementations (i.e. QueryInterface(),
 *  AddRef() and Release()) for the enumerator and collection classes
 *  declared by the COM_DECL_READONLY_ENUM_AND_COLLECTION_AS macro.
 *
 *  The macro should be placed in one of the source files.
 *
 *  @note
 *      this macro is XPCOM-specific and not necessary for MS COM,
 *      so expands to nothing on Win32.
 *
 *  @param prefix   the prefix prepended to the generated collection and
 *                  enumerator classes
 *  @param iface    interface class
 */
#define COM_IMPL_READONLY_ENUM_AND_COLLECTION_AS(prefix, iface) \
    NS_DECL_CLASSINFO(prefix##Collection) \
    NS_IMPL_THREADSAFE_ISUPPORTS1_CI(prefix##Collection, iface##Collection) \
    NS_DECL_CLASSINFO(prefix##Enumerator) \
    NS_IMPL_THREADSAFE_ISUPPORTS1_CI(prefix##Enumerator, iface##Enumerator)

#endif // !__WIN__

#endif // ____H_COLLECTION
