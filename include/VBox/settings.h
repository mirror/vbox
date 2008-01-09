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

#ifndef ___VBox_settings_h
#define ___VBox_settings_h

#include <iprt/cdefs.h>
#include <iprt/cpputils.h>
#include <iprt/string.h>

#include <list>
#include <memory>
#include <limits>

/* these conflict with numeric_digits<>::min and max */
#undef min
#undef max

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/time.h>

#include <stdarg.h>


/** @defgroup   grp_settings    Settings File Manipulation API
 * @{
 *
 * The Settings File Manipulation API allows to maintain a configuration file
 * that contains "name-value" pairs grouped under named keys which are in turn
 * organized in a hierarchical tree-like structure:
 *
 * @code
 * <RootKey>
 *  <Key1 attr1="value" attr2=""/>
 *  <Key2 attr1="value">
 *   <SubKey1>SubKey1_Value</SubKey1>
 *   <SubKey2 attr1="value">SubKey2_Value</SubKey2>
 *   Key2_Value
 *  </Key2>
 * </RootKey>
 * @endcode
 *
 * All strings this API manipulates with are zero-terminated arrays of @c char
 * in UTF-8 encoding. Strings returned by the API are owned by the API unless
 * explicitly stated otherwise. Strings passed to the API are accessed by the
 * API only during the given API call unless explicitly stated otherwise. If
 * necessary, the API will make a copy of the supplied string.
 *
 * Error reprting is perfomed using C++ exceptions. All exceptions thrown by
 * this API are derived from settings::Error. This doesn't cover exceptions
 * that may be thrown by third-party library calls made by this API.
 *
 * All public classes represented by this API that support copy operations
 * (i.e. may be created or assigned from other instsances of the same class),
 * such as Key and Value classes, implement shallow copies and use this mode by
 * default. It means two things:
 *
 * 1. Instances of these classes can be freely copied around and used as return
 *    values. All copies will share the same internal data block (using the
 *    reference counting technique) so that the copy operation is cheap, both
 *    in terms of memory and speed.
 *
 * 2. Since copied instances share the same data, an attempt to change data in
 *    the original will be reflected in all existing copies.
 *
 * Making deep copies or detaching the existing shallow copy from its original
 * is not yet supported.
 *
 * Due to some (not propely studied) libxml2 limitations, the Settings File
 * API is not thread-safe. Therefore, the API caller must provide
 * serialization for threads using this API simultaneously. Note though that
 * if the libxml2 library is (even imlicitly) used on some other thread which
 * doesn't use this API (e.g. third-party code), it may lead to resource
 * conflicts (followed by crashes, memory corruption etc.). A proper solution
 * for these conflicts is to be found.
 *
 * In order to load a settings file the program creates a TreeBackend instance
 * using one of the specific backends (e.g. XmlTreeBackend) and then passes an
 * Input stream object (e.g. File or MemoryBuf) to the TreeBackend::read()
 * method to parse the stream and build the settings tree. On success, the
 * program uses the TreeBackend::rootKey() method to access the root key of
 * the settings tree. The root key provides access to the whole tree of
 * settings through the methods of the Key class which allow to read, change
 * and create new key values. Below is an example that uses the XML backend to
 * load the settings tree, then modifies it and then saves the modifications.
 *
 * @code
    using namespace settings;

    try
    {
        File file (File::ReadWrite, "myfile.xml");
        XmlTreeBackend tree;

        // load the tree, parse it and validate using the XML schema
        tree.read (aFile, "myfile.xsd", XmlTreeBackend::Read_AddDefaults);

        // get the root key
        Key root = tree.key();
        printf ("root=%s\n", root.name());

        // enumerate all child keys of the root key named Foo
        Key::list children = root.keys ("Foo");
        for (Key::list::const_iterator it = children.begin();
             it != children.end();
             ++ it)
        {
            // get the "level" attribute
            int level = (*it).value <int> ("level");
            if (level > 5)
            {
                // if so, create a "Bar" key if it doesn't exist yet 
                Key bar = (*it).createKey ("Bar");
                // set the "date" attribute
                RTTIMESPEC now;
                RTTimeNow (&now);
                bar.setValue <RTTIMESPEC> ("date", now);
            }
            else if (level < 2)
            {
                // if its below 2, delete the whole "Foo" key
                (*it).zap();
            }
        }

        // save the tree on success (the second try is to distinguish between
        // stream load and save errors)
        try
        {
            aTree.write (aFile);
        }
        catch (const EIPRTFailure &err)
        {
            // this is an expected exception that may happen in case of stream
            // read or write errors
            printf ("Could not save the settings file '%s' (%Vrc)");
                    file.uri(), err.rc());

            return FAILURE;
        }

        return SUCCESS;
    }
    catch (const EIPRTFailure &err)
    {
        // this is an expected exception that may happen in case of stream
        // read or write errors
        printf ("Could not load the settings file '%s' (%Vrc)");
                file.uri(), err.rc());
    }
    catch (const XmlTreeBackend::Error &err)
    {
        // this is an XmlTreeBackend specific exception exception that may
        // happen in case of XML parse or validation errors
        printf ("Could not load the settings file '%s'.\n%s"),
                file.uri(), err.what() ? err.what() : "Unknown error");
    }
    catch (const std::exception &err)
    {
        // the rest is unexpected (e.g. should not happen unless you
        // specifically wish so for some reason and therefore allow for a
        // situation that may throw one of these from within the try block
        // above)
        AssertMsgFailed ("Unexpected exception '%s' (%s)\n",
                         typeid (err).name(), err.what());
    catch (...)
    {
        // this is even more unexpected, and no any useful info here
        AssertMsgFailed ("Unexpected exception\n");
    }

    return FAILURE;
 * @endcode
 *
 * Note that you can get a raw (string) value of the attribute using the
 * Key::stringValue() method but often it's simpler and better to use the
 * templated Key::value<>() method that can convert the string to a value of
 * the given type for you (and throw exceptions when the converison is not
 * possible). Similarly, the Key::setStringValue() methid is used to set a raw
 * string value and there is a templated Key::setValue<>() method to set a
 * typed value which will implicitly convert it to a string.
 *
 * Currently, types supported by Key::value<>() and Key::setValue<>() include
 * all C and IPRT integer types, bool and RTTIMESPEC (represented as isoDate
 * in XML). You can always add support for your own types by creating
 * additional specializations of the FromString<>() and ToString<>() templates
 * in the settings namespace (see the real examples in this header).
 *
 * See individual funciton, class and method descriptions to get more details
 * on the Settings File Manipulation API.
 */

#ifndef IN_RING3
# error "There are no settings APIs available in Ring-0 Context!"
#else /* IN_RING3 */

/** @def IN_VBOXSETTINGS_R3
 * Used to indicate whether we're inside the same link module as the
 * XML Settings File Manipulation API.
 *
 * @todo should go to a separate common include together with VBOXXML2_CLASS
 * once there becomes more than one header in the VBoxXML2 library.
 */
#ifdef IN_VBOXSETTINGS_R3
# define VBOXSETTINGS_CLASS DECLEXPORT_CLASS
#else
# define VBOXSETTINGS_CLASS DECLIMPORT_CLASS
#endif

/*
 * Shut up MSVC complaining that auto_ptr[_ref] template instantiations (as a
 * result of private data member declarations of some classes below) need to
 * be exported too to in order to be accessible by clients. I don't
 *
 * The alternative is to instantiate a template before the data member
 * declaration with the VBOXSETTINGS_CLASS prefix, but the standard disables
 * explicit instantiations in a foreign namespace. However, a declaration
 * like:
 *
 *   template class VBOXSETTINGS_CLASS std::auto_ptr <Data>;
 *
 * right before the member declaration makes MSVC happy too, but this is not a
 * valid C++ construct (and G++ spits it out). So, for now we just disable the
 * warning and will come back to this problem one dat later.
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

/**
 * Settings File Manipulation API namespace.
 */
namespace settings
{

// Helpers
//////////////////////////////////////////////////////////////////////////////

/** 
 * Temporary holder for the formatted string.
 *
 * Instances of this class are used for passing the formatted string as an
 * argument to an Error constructor or to another function that takes
 * <tr>const char *</tr> and makes a copy of the string it points to.
 */
class VBOXSETTINGS_CLASS FmtStr
{
public:

    /** 
     * Creates a formatted string using the format string and a set of
     * printf-like arguments.
     */
    FmtStr (const char *aFmt, ...)
    {
        va_list args;
        va_start (args, aFmt);
        RTStrAPrintfV (&mStr, aFmt, args);
        va_end (args);
    }

    ~FmtStr() { RTStrFree (mStr); }

    operator const char *() { return mStr; }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (FmtStr)

    char *mStr;
};

// Exceptions
//////////////////////////////////////////////////////////////////////////////

/**
 * Base exception class.
 */
class VBOXSETTINGS_CLASS Error : public std::exception
{
public:

    Error (const char *aMsg = NULL) 
        : m (aMsg ? Str::New (aMsg) : NULL) {}

    virtual ~Error() throw() {}

    void setWhat (const char *aMsg) { m = aMsg ? Str::New (aMsg) : NULL; }

    const char *what() const throw() { return m.is_null() ? NULL : m->str; }

private:

    /** smart string with support for reference counting */
    struct Str
    {
        size_t ref() { return ++ refs; }
        size_t unref() { return -- refs; }

        size_t refs;
        char str [1];

        static Str *New (const char *aStr)
        {
            Str *that = (Str *) RTMemAllocZ (sizeof (Str) + strlen (aStr));
            AssertReturn (that, NULL);
            strcpy (that->str, aStr);
            return that;
        }

        void operator delete (void *that, size_t) { RTMemFree (that); }
    };

    stdx::auto_ref_ptr <Str> m;
};

class VBOXSETTINGS_CLASS LogicError : public Error
{
public:

    LogicError (const char *aMsg = NULL) : Error (aMsg) {}

    LogicError (RT_SRC_POS_DECL)
    {
        char *msg = NULL;
        RTStrAPrintf (&msg, "In '%s', '%s' at #%d",
                      pszFunction, pszFile, iLine);
        setWhat (msg);
        RTStrFree (msg);
    }
};

class VBOXSETTINGS_CLASS RuntimeError : public Error
{
public:

    RuntimeError (const char *aMsg = NULL) : Error (aMsg) {}
};

// Logical errors
//////////////////////////////////////////////////////////////////////////////

class VBOXSETTINGS_CLASS ENotImplemented : public LogicError
{
public:

    ENotImplemented (const char *aMsg = NULL) : LogicError (aMsg) {}
    ENotImplemented (RT_SRC_POS_DECL) : LogicError (RT_SRC_POS_ARGS) {}
};

class VBOXSETTINGS_CLASS EInvalidArg : public LogicError
{
public:

    EInvalidArg (const char *aMsg = NULL) : LogicError (aMsg) {}
    EInvalidArg (RT_SRC_POS_DECL) : LogicError (RT_SRC_POS_ARGS) {}
};

class VBOXSETTINGS_CLASS ENoKey : public LogicError
{
public:

    ENoKey (const char *aMsg = NULL) : LogicError (aMsg) {}
};

class VBOXSETTINGS_CLASS ENoValue : public LogicError
{
public:

    ENoValue (const char *aMsg = NULL) : LogicError (aMsg) {}
};

// Runtime errors
//////////////////////////////////////////////////////////////////////////////

class VBOXSETTINGS_CLASS ENoMemory : public RuntimeError, public std::bad_alloc
{
public:

    ENoMemory (const char *aMsg = NULL) : RuntimeError (aMsg) {}
    virtual ~ENoMemory() throw() {}
};

class VBOXSETTINGS_CLASS EIPRTFailure : public RuntimeError
{
public:

    EIPRTFailure (const char *aMsg = NULL) : RuntimeError (aMsg) {}

    EIPRTFailure (int aRC) : mRC (aRC) {}
    int rc() const { return mRC; }

private:

    int mRC;
};

class VBOXSETTINGS_CLASS ENoConversion : public RuntimeError
{
public:

    ENoConversion (const char *aMsg = NULL) : RuntimeError (aMsg) {}
};

// string -> type conversions
//////////////////////////////////////////////////////////////////////////////

/** @internal
 * Helper for the FromString() template, doesn't need to be called directly.
 */
DECLEXPORT (uint64_t) FromStringInteger (const char *aValue, bool aSigned,
                                         int aBits, uint64_t aMin, uint64_t aMax);

/** 
 * Generic template function to perform a conversion of an UTF-8 string to an
 * arbitrary value of type @a T.
 *
 * This generic template is implenented only for 8-, 16-, 32- and 64- bit
 * signed and unsigned integers where it uses RTStrTo[U]Int64() to perform the
 * conversion. For all other types it throws an ENotImplemented
 * exception. Individual template specializations for known types should do
 * the conversion job.
 *
 * If the conversion is not possible (for example the string format is wrong
 * or meaningless for the given type), this template will throw an
 * ENoConversion exception. All specializations must do the same.
 *
 * If the @a aValue argument is NULL, this method will throw an ENoValue
 * exception. All specializations must do the same.
 *
 * @param aValue Value to convert.
 * 
 * @return Result of conversion.
 */
template <typename T>
T FromString (const char *aValue)
{
    if (std::numeric_limits <T>::is_integer)
    {
        bool sign = std::numeric_limits <T>::is_signed;
        int bits = std::numeric_limits <T>::digits + (sign ? 1 : 0);

        return (T) FromStringInteger (aValue, sign, bits,
                                      (uint64_t) std::numeric_limits <T>::min(),
                                      (uint64_t) std::numeric_limits <T>::max());
    }

    throw ENotImplemented (RT_SRC_POS);
}

/**
 * Specialization of FromString for bool.
 *
 * Converts "true", "yes", "on" to true and "false", "no", "off" to false.
 */
template<> DECLEXPORT (bool) FromString <bool> (const char *aValue);

/**
 * Specialization of FromString for RTTIMESPEC.
 *
 * Converts the date in ISO format (<YYYY>-<MM>-<DD>T<hh>:<mm>:<ss>[timezone])
 * to a RTTIMESPEC value. Currently, the timezone must always be Z (UTC).
 */
template<> DECLEXPORT (RTTIMESPEC) FromString <RTTIMESPEC> (const char *aValue);

/** 
 * Converts a string of hex digits to memory bytes.
 * 
 * @param aValue String to convert.
 * @param aLen   Where to store the length of the returned memory
 *               block (may be NULL).
 * 
 * @return Result of conversion (a block of @a aLen bytes).
 */
DECLEXPORT (stdx::char_auto_ptr) FromString (const char *aValue, size_t *aLen);

// type -> string conversions
//////////////////////////////////////////////////////////////////////////////

/** @internal
 * Helper for the ToString() template, doesn't need to be called directly.
 */
DECLEXPORT (stdx::char_auto_ptr)
ToStringInteger (uint64_t aValue, unsigned int aBase,
                 bool aSigned, int aBits);

/** 
 * Generic template function to perform a conversion of an arbitrary value to
 * an UTF-8 string.
 *
 * This generic template is implemented only for 8-, 16-, 32- and 64- bit
 * signed and unsigned integers where it uses RTStrFormatNumber() to perform
 * the conversion. For all other types it throws an ENotImplemented
 * exception. Individual template specializations for known types should do
 * the conversion job. If the conversion is not possible (for example the
 * given value doesn't have a string representation), the relevant
 * specialization should throw an ENoConversion exception.
 *
 * If the @a aValue argument's value would convert to a NULL string, this
 * method will throw an ENoValue exception. All specializations must do the
 * same.
 *
 * @param aValue Value to convert.
 * @param aExtra Extra flags to define additional formatting. In case of
 *               integer types, it's the base used for string representation.
 *
 * @return Result of conversion.
 */
template <typename T>
stdx::char_auto_ptr ToString (const T &aValue, unsigned int aExtra = 0)
{
    if (std::numeric_limits <T>::is_integer)
    {
        bool sign = std::numeric_limits <T>::is_signed;
        int bits = std::numeric_limits <T>::digits + (sign ? 1 : 0);

        return ToStringInteger (aValue, aExtra, sign, bits);
    }

    throw ENotImplemented (RT_SRC_POS);
}

/**
 * Specialization of ToString for bool.
 *
 * Converts true to "true" and false to "false". @a aExtra is not used.
 */
template<> DECLEXPORT (stdx::char_auto_ptr)
ToString <bool> (const bool &aValue, unsigned int aExtra);

/**
 * Specialization of ToString for RTTIMESPEC.
 *
 * Converts the RTTIMESPEC value to the date string in ISO format
 * (<YYYY>-<MM>-<DD>T<hh>:<mm>:<ss>[timezone]). Currently, the timezone will
 * always be Z (UTC).
 *
 * @a aExtra is not used.
 */
template<> DECLEXPORT (stdx::char_auto_ptr)
ToString <RTTIMESPEC> (const RTTIMESPEC &aValue, unsigned int aExtra);

/** 
 * Converts memory bytes to a null-terminated string of hex values.
 * 
 * @param aData Pointer to the memory block.
 * @param aLen  Length of the memory block.
 * 
 * @return Result of conversion.
 */
DECLEXPORT (stdx::char_auto_ptr) ToString (const void *aData, size_t aLen);

// the rest
//////////////////////////////////////////////////////////////////////////////

/** 
 * The Key class represents a settings key.
 *
 * Every settings key has a name and zero or more uniquely named values
 * (attributes). There is a special attribute with a NULL name that is called
 * a key value.
 *
 * Besides values, settings keys may contain other settings keys. This way,
 * settings keys form a tree-like (or a directory-like) hierarchy of keys. Key
 * names do not need to be unique even if they belong to the same parent key
 * which allows to have an array of keys of the same name.
 *
 * @note Key and Value objects returned by methods of the Key and TreeBackend
 * classes are owned by the given TreeBackend instance and may refer to data
 * that becomes invalid when this TreeBackend instance is destroyed.
 */
class VBOXSETTINGS_CLASS Key
{
public:

    typedef std::list <Key> List;

    /**
     * Key backend interface used to perform actual key operations.
     *
     * This interface is implemented by backends that provide specific ways of
     * storing settings keys.
     */
    class VBOXSETTINGS_CLASS Backend : public stdx::auto_ref
    {
    public:

        /** Performs the Key::name() function. */
        virtual const char *name() const = 0;

        /** Performs the Key::setName() function. */
        virtual void setName (const char *aName) = 0;

        /** Performs the Key::stringValue() function. */
        virtual const char *value (const char *aName) const = 0;

        /** Performs the Key::setStringValue() function. */
        virtual void setValue (const char *aName, const char *aValue) = 0;

        /** Performs the Key::keys() function. */
        virtual List keys (const char *aName = NULL) const = 0;

        /** Performs the Key::findKey() function. */
        virtual Key findKey (const char *aName) const = 0;

        /** Performs the Key::appendKey() function. */
        virtual Key appendKey (const char *aName) = 0;

        /** Performs the Key::zap() function. */
        virtual void zap() = 0;

        /**
         * Returns an opaque value that uniquely represents the position of
         * this key on the tree which is used to compare two keys. Two or more
         * keys may return the same value only if they actually represent the
         * same key (i.e. they have the same list of parents and children).
         */
        virtual void *position() const = 0;
    };

    /** 
     * Creates a new key object. If @a aBackend is @c NULL then a null key is
     * created.
     *
     * Regular API users should never need to call this method with something
     * other than NULL argument (which is the default).
     * 
     * @param aBackend      Key backend to use.
     */
    Key (Backend *aBackend = NULL) : m (aBackend) {}

    /** 
     * Returns @c true if this key is null.
     */
    bool isNull() const { return m.is_null(); }

    /** 
     * Makes this object a null key.
     *
     * Note that as opposed to #zap(), this methid does not delete the key from
     * the list of children of its parent key.
     */
    void setNull() { m = NULL; }

    /** 
     * Returns the name of this key.
     * Returns NULL if this object a null (uninitialized) key.
     */
    const char *name() const { return m.is_null() ? NULL : m->name(); }

    /** 
     * Sets the name of this key.
     * 
     * @param aName New key name.
     */
    void setName (const char *aName) { if (!m.is_null()) m->setName (aName); }

    /** 
     * Returns the value of the attribute with the given name as an UTF-8
     * string. Returns @c NULL if there is no attribute with the given name.
     *
     * @param aName     Name of the attribute. NULL may be used to
     *                  get the key value.
     */
    const char *stringValue (const char *aName) const
    {
        return m.is_null() ? NULL : m->value (aName);
    }

    /** 
     * Sets the value of the attribute with the given name from an UTF-8
     * string. This method will do a copy of the supplied @a aValue string.
     * 
     * @param aName     Name of the attribute. NULL may be used to
     *                  set the key value.
     * @param aValue    New value of the attribute. NULL may be used to
     *                  delete the value instead of setting it.
     */
    void setStringValue (const char *aName, const char *aValue)
    {
        if (!m.is_null()) m->setValue (aName, aValue);
    }

    /** 
     * Returns the value of the attribute with the given name as an object of
     * type @a T. Throws ENoValue if there is no attribute with the given
     * name.
     *
     * This function calls #stringValue() to get the string representation of
     * the attribute and then calls the FromString() template to convert this
     * string to a value of the given type.
     * 
     * @param aName     Name of the attribute. NULL may be used to
     *                  get the key value.
     */
    template <typename T>
    T value (const char *aName) const
    {
        try
        {
            return FromString <T> (stringValue (aName));
        }
        catch (const ENoValue &)
        {
            throw ENoValue (FmtStr ("No such attribute '%s'", aName));
        }
    }

    /** 
     * Returns the value of the attribute with the given name as an object of
     * type @a T. Returns the given default value if there is no attribute
     * with the given name.
     *
     * This function calls #stringValue() to get the string representation of
     * the attribute and then calls the FromString() template to convert this
     * string to a value of the given type.
     * 
     * @param aName     Name of the attribute. NULL may be used to
     *                  get the key value.
     * @param aDefault  Default value to return for the missing attribute.
     */
    template <typename T>
    T valueOr (const char *aName, const T &aDefault) const
    {
        try
        {
            return FromString <T> (stringValue (aName));
        }
        catch (const ENoValue &)
        {
            return aDefault;
        }
    }

    /** 
     * Sets the value of the attribute with the given name from an object of
     * type @a T. This method will do a copy of data represented by @a aValue
     * when necessary.
     *
     * This function converts the given value to a string using the ToString()
     * template and then calls #setStringValue().
     * 
     * @param aName     Name of the attribute. NULL may be used to
     *                  set the key value.
     * @param aValue    New value of the attribute.
     * @param aExtra    Extra field used by some types to specify additional
     *                  details for storing the value as a string (such as the
     *                  base for decimal numbers).
     */
    template <typename T>
    void setValue (const char *aName, const T &aValue, unsigned int aExtra = 0)
    {
        try
        {
            stdx::char_auto_ptr value = ToString (aValue, aExtra);
            setStringValue (aName, value.get());
        }
        catch (const ENoValue &)
        {
            throw ENoValue (FmtStr ("No value for attribute '%s'", aName));
        }
    }

    /** 
     * Sets the value of the attribute with the given name from an object of
     * type @a T. If the value of the @a aValue object equals to the value of
     * the given @a aDefault object, then the attribute with the given name
     * will be deleted instead of setting its value to @a aValue.
     *
     * This function converts the given value to a string using the ToString()
     * template and then calls #setStringValue().
     * 
     * @param aName     Name of the attribute. NULL may be used to
     *                  set the key value.
     * @param aValue    New value of the attribute.
     * @param aDefault  Default value to compare @a aValue to.
     * @param aExtra    Extra field used by some types to specify additional
     *                  details for storing the value as a string (such as the
     *                  base for decimal numbers).
     */
    template <typename T>
    void setValueOr (const char *aName, const T &aValue, const T &aDefault,
                     unsigned int aExtra = 0)
    {
        if (aValue == aDefault)
            zapValue (aName);
        else
            setValue <T> (aName, aValue, aExtra);
    }

    /** 
     * Deletes the value of the attribute with the given name.
     * Shortcut to <tt>setStringValue(aName, NULL)</tt>.
     */
    void zapValue (const char *aName) { setStringValue (aName, NULL); }

    /** 
     * Returns the key value.
     * Shortcut to <tt>stringValue (NULL)</tt>.
     */
    const char *keyStringValue() const { return stringValue (NULL); }

    /** 
     * Sets the key value.
     * Shortcut to <tt>setStringValue (NULL, aValue)</tt>.
     */
    void setKeyStringValue (const char *aValue) { setStringValue (NULL, aValue); }

    /** 
     * Returns the key value.
     * Shortcut to <tt>value (NULL)</tt>.
     */
    template <typename T>
    T keyValue() const { return value <T> (NULL); }

    /** 
     * Returns the key value or the given default if the key value is NULL.
     * Shortcut to <tt>value (NULL)</tt>.
     */
    template <typename T>
    T keyValueOr (const T &aDefault) const { return valueOr <T> (NULL, aDefault); }

    /** 
     * Sets the key value.
     * Shortcut to <tt>setValue (NULL, aValue, aExtra)</tt>.
     */
    template <typename T>
    void setKeyValue (const T &aValue, unsigned int aExtra = 0)
    {
        setValue <T> (NULL, aValue, aExtra);
    }

    /** 
     * Sets the key value.
     * Shortcut to <tt>setValueOr (NULL, aValue, aDefault)</tt>.
     */
    template <typename T>
    void setKeyValueOr (const T &aValue, const T &aDefault,
                        unsigned int aExtra = 0)
    {
        setValueOr <T> (NULL, aValue, aDefault, aExtra);
    }

    /** 
     * Deletes the key value.
     * Shortcut to <tt>zapValue (NULL)</tt>.
     */
    void zapKeyValue () { zapValue (NULL); }

    /** 
     * Returns a list of all child keys named @a aName.
     *
     * If @a aname is @c NULL, returns a list of all child keys.
     * 
     * @param aName     Child key name to list.
     */
    List keys (const char *aName = NULL) const
    {
        return m.is_null() ? List() : m->keys (aName); 
    };

    /** 
     * Returns the first child key with the given name.
     *
     * Throws ENoKey if no child key with the given name exists.
     * 
     * @param aName     Child key name.
     */
    Key key (const char *aName) const
    {
        Key key = findKey (aName);
        if (key.isNull())
            throw ENoKey (FmtStr ("No such key '%s'", aName));
        return key;
    }

    /** 
     * Returns the first child key with the given name.
     *
     * As opposed to #key(), this method will not throw an exception if no
     * child key with the given name exists, but return a null key instead.
     * 
     * @param aName     Child key name.
     */
    Key findKey (const char *aName) const
    {
        return m.is_null() ? Key() : m->findKey (aName); 
    }

    /** 
     * Creates a key with the given name as a child of this key and returns it
     * to the caller.
     *
     * If one or more child keys with the given name already exist, no new key
     * is created but the first matching child key is returned.
     * 
     * @param aName Name of the child key to create.
     */
    Key createKey (const char *aName)
    {
        Key key = findKey (aName);
        if (key.isNull())
            key = appendKey (aName);
        return key;
    }

    /** 
     * Appends a key with the given name to the list of child keys of this key
     * and returns the appended key to the caller.
     * 
     * @param aName Name of the child key to create.
     */
    Key appendKey (const char *aName)
    {
        return m.is_null() ? Key() : m->appendKey (aName); 
    }

    /** 
     * Deletes this key.
     *
     * The deleted key is removed from the list of child keys of its parent
     * key and becomes a null object.
     */
    void zap()
    {
        if (!m.is_null())
        {
            m->zap();
            setNull();
        }
    }

    /**
     * Compares this object with the given object and returns @c true if both
     * represent the same key on the settings tree or if both are null
     * objects.
     *
     * @param that Object to compare this object with.
     */
    bool operator== (const Key &that) const
    {
        return m == that.m ||
               (!m.is_null() && !that.m.is_null() &&
                m->position() == that.m->position());
    }

    /** 
     * Counterpart to operator==().
     */
    bool operator!= (const Key &that) const { return !operator== (that); }

private:

    stdx::auto_ref_ptr <Backend> m;

    friend class TreeBackend;
};

/** 
 * The Stream class is a base class for I/O streams.
 */
class VBOXSETTINGS_CLASS Stream
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
class VBOXSETTINGS_CLASS Input : virtual public Stream
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
class VBOXSETTINGS_CLASS Output : virtual public Stream
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

/**
 * The TreeBackend class represents a storage backend used to read a settings
 * tree from and write it to a stream.
 *
 * @note All Key objects returned by any of the TreeBackend methods (and by
 * methods of returned Key objects) are owned by the given TreeBackend
 * instance. When this instance is destroyed, all Key objects become invalid
 * and an attempt to access Key data will cause the program crash.
 */
class VBOXSETTINGS_CLASS TreeBackend
{
public:

    /** 
     * Reads and parses the given input stream.
     *
     * On success, the previous settings tree owned by this backend (if any)
     * is deleted.
     *
     * The optional schema URI argument determines the name of the schema to
     * use for input validation. If the schema URI is NULL then the validation
     * is not performed. Note that you may set a custom input resolver if you
     * want to provide the input stream for the schema file (and for other
     * external entities) instead of letting the backend to read the specified
     * URI directly.
     *
     * This method will set the read/write position to the beginning of the
     * given stream before reading. After the stream has been successfully
     * parsed, the position will be set back to the beginning.
     * 
     * @param aInput        Input stream.
     * @param aSchema       Schema URI to use for input stream validation.
     * @param aFlags        Optional bit flags.
     */
    void read (Input &aInput, const char *aSchema = NULL, int aFlags = 0)
    {
        aInput.setPos (0);
        rawRead (aInput, aSchema, aFlags);
        aInput.setPos (0);
    }

    /** 
     * Reads and parses the given input stream in a raw fashion.
     *
     * Currently, the only difference from #read() is that this method doesn't
     * set the stream position to the beginnign before and after reading but
     * instead leaves it as is in both cases which can give unexpected
     * results.
     *
     * @see read()
     */
    virtual void rawRead (Input &aInput, const char *aSchema = NULL,
                          int aFlags = 0) = 0;

    /** 
     * Writes the current settings tree to the given output stream.
     * 
     * This method will set the read/write position to the beginning of the
     * given stream before writing. After the settings have been successfully
     * written to the stream, the stream will be truncated at the position
     * following the last byte written by this method anc ghd position will be
     * set back to the beginning.
     * 
     * @param aOutput       Output stream.
     */
    void write (Output &aOutput)
    {
        aOutput.setPos (0);
        rawWrite (aOutput);
        aOutput.truncate();
        aOutput.setPos (0);
    }

    /** 
     * Writes the current settings tree to the given output stream in a raw
     * fashion.
     *
     * Currently, the only difference from #write() is that this method
     * doesn't set the stream position to the beginning before and after
     * reading and doesn't thruncate the stream, but instead leaves it as is
     * in both cases which can give unexpected results.
     *
     * @see write()
     */
    virtual void rawWrite (Output &aOutput) = 0;

    /** 
     * Deletes the current settings tree.
     */
    virtual void reset() = 0;

    /** 
     * Returns the root settings key.
     */
    virtual Key &rootKey() const = 0;

protected:

    static Key::Backend *GetKeyBackend (const Key &aKey) { return aKey.m.raw(); }
};

//////////////////////////////////////////////////////////////////////////////

/**
 * The File class is a stream implementation that reads from and writes to
 * regular files.
 *
 * The File class uses IPRT File API for file operations.
 */
class VBOXSETTINGS_CLASS File : public Input, public Output
{
public:

    /**
     * Possible file access modes.
     */
    enum Mode { Read, Write, ReadWrite };

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
     * Uses the given file handle to perform file operations. The given file
     * handle must be already open and the @a aMode argument must match the
     * actual mode of the file handle (otherwise unexpected errors will
     * occur).
     *
     * The read/write position of the given handle will be reset to the
     * beginning of the file on success.
     *
     * Note that the given file handle will not be automatically closed upon
     * this object destruction.
     *
     * @param aHandle   Open file handle.
     * @param aMode     File mode of the open file handle.
     * @param aFileName File name (for reference).
     */
    File (Mode aMode, RTFILE aHandle, const char *aFileName = NULL);

    /**
     * Destrroys the File object. If the object was created from a file name
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
class VBOXSETTINGS_CLASS MemoryBuf : public Input
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

class XmlKeyBackend;

/**
 * The XmlTreeBackend class uses XML markup to store settings trees.
 */
class VBOXSETTINGS_CLASS XmlTreeBackend : public TreeBackend
{
public:

    /** Flags for TreeBackend::read(). */
    enum
    {
        /** 
         * Sbstitute default values for missing attributes that have defaults
         * in the XML schema. Otherwise, stringValue() will return NULL for
         * such attributes.
         */
        Read_AddDefaults = RT_BIT (0),
    };

    /** 
     * The InputResolver class represents an input resolver, the service used to
     * provide input streams for external entities given an URL and entity ID.
     */
    class VBOXSETTINGS_CLASS InputResolver
    {
    public:

        /**
         * Returns a newly allocated input stream for the given arguments. The
         * caller will delete the returned object when no more necessary.
         * 
         * @param aURI  URI of the external entity.
         * @param aID   ID of the external entity (may be NULL).
         * 
         * @return      Input stream created using @c new or NULL to indicate
         *              a wrong URI/ID pair.
         *
         * @todo Return by value after implementing the copy semantics for
         * Input subclasses.
         */
        virtual Input *resolveEntity (const char *aURI, const char *aID) = 0;
    };


    /** 
     * The Error class represents errors that may happen when parsing or
     * validating the XML document representing the settings tree.
     */
    class VBOXSETTINGS_CLASS Error : public RuntimeError
    {
    public:

        Error (const char *aMsg = NULL) : RuntimeError (aMsg) {}
    };

    XmlTreeBackend();
    ~XmlTreeBackend();

    /** 
     * Sets an external entity resolver used to provide input streams for
     * entities referred to by the XML document being parsed.
     *
     * The given resolver object must exist as long as this instance exists or
     * until a different resolver is set using setInputResolver() or reset
     * using resetInputResolver().
     * 
     * @param aResolver Resolver to use.
     */
    void setInputResolver (InputResolver &aResolver);

    /** 
     * Resets the entity resolver to the default resolver. The default
     * resolver provides support for 'file:' and 'http:' protocols.
     */
    void resetInputResolver();

    void rawRead (Input &aInput, const char *aSchema = NULL, int aFlags = 0);
    void rawWrite (Output &aOutput);
    void reset();
    Key &rootKey() const;

private:

    class XmlError;

    /* Obscure class data */
    struct Data;
    std::auto_ptr <Data> m;

    /* auto_ptr data doesn't have proper copy semantics */
    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (XmlTreeBackend)

    static int ReadCallback (void *aCtxt, char *aBuf, int aLen);
    static int WriteCallback (void *aCtxt, const char *aBuf, int aLen);
    static int CloseCallback (void *aCtxt);

    static void ValidityErrorCallback (void *aCtxt, const char *aMsg, ...);
    static void ValidityWarningCallback (void *aCtxt, const char *aMsg, ...);
    static void StructuredErrorCallback (void *aCtxt, xmlErrorPtr aErr);

    static xmlParserInput *ExternalEntityLoader (const char *aURI,
                                                 const char *aID,
                                                 xmlParserCtxt *aCtxt);

    static XmlTreeBackend *sThat;

    static XmlKeyBackend *GetKeyBackend (const Key &aKey)
    { return (XmlKeyBackend *) TreeBackend::GetKeyBackend (aKey); }
};

} /* namespace settings */

#if defined(_MSC_VER)
#pragma warning (default:4251)
#endif

#endif /* IN_RING3 */

/** @} */

#endif /* ___VBox_settings_h */
