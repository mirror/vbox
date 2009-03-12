<?xml version="1.0"?>
<!-- $Id: xpidl.xsl 39869 2008-11-25 13:37:40Z dmik $ -->

<!--
 *  A template to generate a XPCOM IDL compatible interface definition file
 *  from the generic interface definition expressed in XML.

     Copyright (C) 2006-2009 Sun Microsystems, Inc.

     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

     Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
     Clara, CA 95054 USA or visit http://www.sun.com if you need
     additional information or have any questions.
-->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="text"/>

<xsl:strip-space elements="*"/>


<!--
//  helper definitions
/////////////////////////////////////////////////////////////////////////////
-->

<!--
 *  capitalizes the first letter
-->
<xsl:template name="capitalize">
  <xsl:param name="str" select="."/>
  <xsl:value-of select="
    concat(
      translate(substring($str,1,1),'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ'),
      substring($str,2)
    )
  "/>
</xsl:template>

<!--
 *  uncapitalizes the first letter only if the second one is not capital
 *  otherwise leaves the string unchanged
-->
<xsl:template name="uncapitalize">
  <xsl:param name="str" select="."/>
  <xsl:choose>
    <xsl:when test="not(contains('ABCDEFGHIJKLMNOPQRSTUVWXYZ', substring($str,2,1)))">
      <xsl:value-of select="
        concat(
          translate(substring($str,1,1),'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz'),
          substring($str,2)
        )
      "/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:value-of select="string($str)"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!--
 *  translates the string to uppercase
-->
<xsl:template name="uppercase">
  <xsl:param name="str" select="."/>
  <xsl:value-of select="
    translate($str,'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')
  "/>
</xsl:template>


<!--
 *  translates the string to lowercase
-->
<xsl:template name="lowercase">
  <xsl:param name="str" select="."/>
  <xsl:value-of select="
    translate($str,'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz')
  "/>
</xsl:template>


<!--
//  templates
/////////////////////////////////////////////////////////////////////////////
-->


<!--
 *  not explicitly matched elements and attributes
-->
<xsl:template match="*"/>


<!--
 *  header
-->
<xsl:template match="/idl">
  <xsl:text>
/*
 *  DO NOT EDIT! This is a generated file.
 *
 *  XPCOM IDL (XPIDL) definition for VirtualBox Main API (COM interfaces)
 *  generated from XIDL (XML interface definition).
 *
 *  Source    : src/VBox/Main/idl/VirtualBox.xidl
 *  Generator : src/VBox/Main/idl/xpcidl.xsl
 *
 *  This file contains portions from the following Mozilla XPCOM files:
 *      xpcom/include/xpcom/nsID.h
 *      xpcom/include/nsIException.h
 *      xpcom/include/nsprpub/prtypes.h
 *      xpcom/include/xpcom/nsISupportsBase.h
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

#ifndef ___VirtualBox_CXPCOM_h
#define ___VirtualBox_CXPCOM_h

#ifdef __cplusplus
# include "VirtualBox_XPCOM.h"
#else /* !__cplusplus */

#include &lt;stddef.h&gt;
#include "wchar.h"

#if defined(WIN32)

#define PR_EXPORT(__type) extern __declspec(dllexport) __type
#define PR_EXPORT_DATA(__type) extern __declspec(dllexport) __type
#define PR_IMPORT(__type) __declspec(dllimport) __type
#define PR_IMPORT_DATA(__type) __declspec(dllimport) __type

#define PR_EXTERN(__type) extern __declspec(dllexport) __type
#define PR_IMPLEMENT(__type) __declspec(dllexport) __type
#define PR_EXTERN_DATA(__type) extern __declspec(dllexport) __type
#define PR_IMPLEMENT_DATA(__type) __declspec(dllexport) __type

#define PR_CALLBACK
#define PR_CALLBACK_DECL
#define PR_STATIC_CALLBACK(__x) static __x

#elif defined(XP_BEOS)

#define PR_EXPORT(__type) extern __declspec(dllexport) __type
#define PR_EXPORT_DATA(__type) extern __declspec(dllexport) __type
#define PR_IMPORT(__type) extern __declspec(dllexport) __type
#define PR_IMPORT_DATA(__type) extern __declspec(dllexport) __type

#define PR_EXTERN(__type) extern __declspec(dllexport) __type
#define PR_IMPLEMENT(__type) __declspec(dllexport) __type
#define PR_EXTERN_DATA(__type) extern __declspec(dllexport) __type
#define PR_IMPLEMENT_DATA(__type) __declspec(dllexport) __type

#define PR_CALLBACK
#define PR_CALLBACK_DECL
#define PR_STATIC_CALLBACK(__x) static __x

#elif defined(WIN16)

#define PR_CALLBACK_DECL        __cdecl

#if defined(_WINDLL)
#define PR_EXPORT(__type) extern __type _cdecl _export _loadds
#define PR_IMPORT(__type) extern __type _cdecl _export _loadds
#define PR_EXPORT_DATA(__type) extern __type _export
#define PR_IMPORT_DATA(__type) extern __type _export

#define PR_EXTERN(__type) extern __type _cdecl _export _loadds
#define PR_IMPLEMENT(__type) __type _cdecl _export _loadds
#define PR_EXTERN_DATA(__type) extern __type _export
#define PR_IMPLEMENT_DATA(__type) __type _export

#define PR_CALLBACK             __cdecl __loadds
#define PR_STATIC_CALLBACK(__x) static __x PR_CALLBACK

#else /* this must be .EXE */
#define PR_EXPORT(__type) extern __type _cdecl _export
#define PR_IMPORT(__type) extern __type _cdecl _export
#define PR_EXPORT_DATA(__type) extern __type _export
#define PR_IMPORT_DATA(__type) extern __type _export

#define PR_EXTERN(__type) extern __type _cdecl _export
#define PR_IMPLEMENT(__type) __type _cdecl _export
#define PR_EXTERN_DATA(__type) extern __type _export
#define PR_IMPLEMENT_DATA(__type) __type _export

#define PR_CALLBACK             __cdecl __loadds
#define PR_STATIC_CALLBACK(__x) __x PR_CALLBACK
#endif /* _WINDLL */

#elif defined(XP_MAC)

#define PR_EXPORT(__type) extern __declspec(export) __type
#define PR_EXPORT_DATA(__type) extern __declspec(export) __type
#define PR_IMPORT(__type) extern __declspec(export) __type
#define PR_IMPORT_DATA(__type) extern __declspec(export) __type

#define PR_EXTERN(__type) extern __declspec(export) __type
#define PR_IMPLEMENT(__type) __declspec(export) __type
#define PR_EXTERN_DATA(__type) extern __declspec(export) __type
#define PR_IMPLEMENT_DATA(__type) __declspec(export) __type

#define PR_CALLBACK
#define PR_CALLBACK_DECL
#define PR_STATIC_CALLBACK(__x) static __x

#elif defined(XP_OS2) &amp;&amp; defined(__declspec)

#define PR_EXPORT(__type) extern __declspec(dllexport) __type
#define PR_EXPORT_DATA(__type) extern __declspec(dllexport) __type
#define PR_IMPORT(__type) __declspec(dllimport) __type
#define PR_IMPORT_DATA(__type) __declspec(dllimport) __type

#define PR_EXTERN(__type) extern __declspec(dllexport) __type
#define PR_IMPLEMENT(__type) __declspec(dllexport) __type
#define PR_EXTERN_DATA(__type) extern __declspec(dllexport) __type
#define PR_IMPLEMENT_DATA(__type) __declspec(dllexport) __type

#define PR_CALLBACK
#define PR_CALLBACK_DECL
#define PR_STATIC_CALLBACK(__x) static __x

#elif defined(XP_OS2_VACPP)

#define PR_EXPORT(__type) extern __type
#define PR_EXPORT_DATA(__type) extern __type
#define PR_IMPORT(__type) extern __type
#define PR_IMPORT_DATA(__type) extern __type

#define PR_EXTERN(__type) extern __type
#define PR_IMPLEMENT(__type) __type
#define PR_EXTERN_DATA(__type) extern __type
#define PR_IMPLEMENT_DATA(__type) __type
#define PR_CALLBACK _Optlink
#define PR_CALLBACK_DECL
#define PR_STATIC_CALLBACK(__x) static __x PR_CALLBACK

#else /* Unix */

# ifdef VBOX_HAVE_VISIBILITY_HIDDEN
#  define PR_EXPORT(__type) __attribute__((visibility("default"))) extern __type
#  define PR_EXPORT_DATA(__type) __attribute__((visibility("default"))) extern __type
#  define PR_IMPORT(__type) extern __type
#  define PR_IMPORT_DATA(__type) extern __type
#  define PR_EXTERN(__type) __attribute__((visibility("default"))) extern __type
#  define PR_IMPLEMENT(__type) __attribute__((visibility("default"))) __type
#  define PR_EXTERN_DATA(__type) __attribute__((visibility("default"))) extern __type
#  define PR_IMPLEMENT_DATA(__type) __attribute__((visibility("default"))) __type
#  define PR_CALLBACK
#  define PR_CALLBACK_DECL
#  define PR_STATIC_CALLBACK(__x) static __x
# else
#  define PR_EXPORT(__type) extern __type
#  define PR_EXPORT_DATA(__type) extern __type
#  define PR_IMPORT(__type) extern __type
#  define PR_IMPORT_DATA(__type) extern __type
#  define PR_EXTERN(__type) extern __type
#  define PR_IMPLEMENT(__type) __type
#  define PR_EXTERN_DATA(__type) extern __type
#  define PR_IMPLEMENT_DATA(__type) __type
#  define PR_CALLBACK
#  define PR_CALLBACK_DECL
#  define PR_STATIC_CALLBACK(__x) static __x
# endif
#endif

#if defined(_NSPR_BUILD_)
#define NSPR_API(__type) PR_EXPORT(__type)
#define NSPR_DATA_API(__type) PR_EXPORT_DATA(__type)
#else
#define NSPR_API(__type) PR_IMPORT(__type)
#define NSPR_DATA_API(__type) PR_IMPORT_DATA(__type)
#endif

typedef unsigned char PRUint8;
#if (defined(HPUX) &amp;&amp; defined(__cplusplus) \
        &amp;&amp; !defined(__GNUC__) &amp;&amp; __cplusplus &lt; 199707L) \
    || (defined(SCO) &amp;&amp; defined(__cplusplus) \
        &amp;&amp; !defined(__GNUC__) &amp;&amp; __cplusplus == 1L)
typedef char PRInt8;
#else
typedef signed char PRInt8;
#endif

#define PR_INT8_MAX 127
#define PR_INT8_MIN (-128)
#define PR_UINT8_MAX 255U

typedef unsigned short PRUint16;
typedef short PRInt16;

#define PR_INT16_MAX 32767
#define PR_INT16_MIN (-32768)
#define PR_UINT16_MAX 65535U

typedef unsigned int PRUint32;
typedef int PRInt32;
#define PR_INT32(x)  x
#define PR_UINT32(x) x ## U

#define PR_INT32_MAX PR_INT32(2147483647)
#define PR_INT32_MIN (-PR_INT32_MAX - 1)
#define PR_UINT32_MAX PR_UINT32(4294967295)

typedef long PRInt64;
typedef unsigned long PRUint64;
typedef int PRIntn;
typedef unsigned int PRUintn;

typedef double          PRFloat64;
typedef size_t PRSize;

typedef ptrdiff_t PRPtrdiff;

typedef unsigned long PRUptrdiff;

typedef PRIntn PRBool;

#define PR_TRUE 1
#define PR_FALSE 0

typedef PRUint8 PRPackedBool;

/*
** Status code used by some routines that have a single point of failure or
** special status return.
*/
typedef enum { PR_FAILURE = -1, PR_SUCCESS = 0 } PRStatus;

#ifndef __PRUNICHAR__
#define __PRUNICHAR__
#if defined(WIN32) || defined(XP_MAC)
typedef wchar_t PRUnichar;
#else
typedef PRUint16 PRUnichar;
#endif
#endif

typedef long PRWord;
typedef unsigned long PRUword;

#define nsnull 0
typedef PRUint32 nsresult;

#if defined(__GNUC__) &amp;&amp; (__GNUC__ > 2)
#define NS_LIKELY(x)    (__builtin_expect((x), 1))
#define NS_UNLIKELY(x)  (__builtin_expect((x), 0))
#else
#define NS_LIKELY(x)    (x)
#define NS_UNLIKELY(x)  (x)
#endif

#define NS_FAILED(_nsresult) (NS_UNLIKELY((_nsresult) &amp; 0x80000000))
#define NS_SUCCEEDED(_nsresult) (NS_LIKELY(!((_nsresult) &amp; 0x80000000)))

/**
 * An "interface id" which can be used to uniquely identify a given
 * interface.
 * A "unique identifier". This is modeled after OSF DCE UUIDs.
 */

struct nsID {
  PRUint32 m0;
  PRUint16 m1;
  PRUint16 m2;
  PRUint8 m3[8];
};

typedef struct nsID nsID;
typedef nsID nsIID;

struct nsISupports;   /* forward declaration */
struct nsIStackFrame; /* forward declaration */
struct nsIException;  /* forward declaration */
typedef struct nsISupports nsISupports;     /* forward declaration */
typedef struct nsIStackFrame nsIStackFrame; /* forward declaration */
typedef struct nsIException nsIException;   /* forward declaration */

/**
 * IID for the nsISupports interface
 * {00000000-0000-0000-c000-000000000046}
 *
 * To maintain binary compatibility with COM's IUnknown, we define the IID
 * of nsISupports to be the same as that of COM's IUnknown.
 */
#define NS_ISUPPORTS_IID                                                      \
  { 0x00000000, 0x0000, 0x0000,                                               \
    {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46} }

/**
 * Reference count values
 *
 * This is the return type for AddRef() and Release() in nsISupports.
 * IUnknown of COM returns an unsigned long from equivalent functions.
 * The following ifdef exists to maintain binary compatibility with
 * IUnknown.
 */

/**
 * Basic component object model interface. Objects which implement
 * this interface support runtime interface discovery (QueryInterface)
 * and a reference counted memory model (AddRef/Release). This is
 * modelled after the win32 IUnknown API.
 */
struct nsISupports_vtbl {

  /**
   * @name Methods
   */

  /**
   * A run time mechanism for interface discovery.
   * @param aIID         [in]  A requested interface IID
   * @param aInstancePtr [out] A pointer to an interface pointer to
   *                           receive the result.
   * @return            NS_OK if the interface is supported by the associated
   *                          instance, NS_NOINTERFACE if it is not.
   * NS_ERROR_INVALID_POINTER if aInstancePtr is NULL.
   */
  nsresult (*QueryInterface)(nsISupports *this_, const nsID *iid, void **resultp);
  /**
   * Increases the reference count for this interface.
   * The associated instance will not be deleted unless
   * the reference count is returned to zero.
   *
   * @return The resulting reference count.
   */
  nsresult (*AddRef)(nsISupports *this_);

  /**
   * Decreases the reference count for this interface.
   * Generally, if the reference count returns to zero,
   * the associated instance is deleted.
   *
   * @return The resulting reference count.
   */
  nsresult (*Release)(nsISupports *this_);

};

struct nsISupports {
    struct nsISupports_vtbl *vtbl;
};

/* starting interface:    nsIException */
#define NS_IEXCEPTION_IID_STR "f3a8d3b4-c424-4edc-8bf6-8974c983ba78"

#define NS_IEXCEPTION_IID \
  {0xf3a8d3b4, 0xc424, 0x4edc, \
    { 0x8b, 0xf6, 0x89, 0x74, 0xc9, 0x83, 0xba, 0x78 }}

struct nsIException_vtbl {

  /* Methods from the Class nsISupports */
  struct nsISupports_vtbl nsisupports;

  /* readonly attribute string message; */
  nsresult (*GetMessage)(nsIException *this_, PRUnichar * *aMessage);

  /* readonly attribute nsresult (*result; */
  nsresult (*GetResult)(nsIException *this_, nsresult *aResult);

  /* readonly attribute string name; */
  nsresult (*GetName)(nsIException *this_, PRUnichar * *aName);

  /* readonly attribute string filename; */
  nsresult (*GetFilename)(nsIException *this_, PRUnichar * *aFilename);

  /* readonly attribute PRUint32 lineNumber; */
  nsresult (*GetLineNumber)(nsIException *this_, PRUint32 *aLineNumber);

  /* readonly attribute PRUint32 columnNumber; */
  nsresult (*GetColumnNumber)(nsIException *this_, PRUint32 *aColumnNumber);

  /* readonly attribute nsIStackFrame location; */
  nsresult (*GetLocation)(nsIException *this_, nsIStackFrame * *aLocation);

  /* readonly attribute nsIException inner; */
  nsresult (*GetInner)(nsIException *this_, nsIException * *aInner);

  /* readonly attribute nsISupports data; */
  nsresult (*GetData)(nsIException *this_, nsISupports * *aData);

  /* string toString (); */
  nsresult (*ToString)(nsIException *this_, PRUnichar **_retval);
};

struct nsIException {
    struct nsIException_vtbl *vtbl;
};

/* starting interface:    nsIStackFrame */
#define NS_ISTACKFRAME_IID_STR "91d82105-7c62-4f8b-9779-154277c0ee90"

#define NS_ISTACKFRAME_IID \
  {0x91d82105, 0x7c62, 0x4f8b, \
    { 0x97, 0x79, 0x15, 0x42, 0x77, 0xc0, 0xee, 0x90 }}

struct nsIStackFrame_vtbl {

  /* Methods from the Class nsISupports */
  struct nsISupports_vtbl nsisupports;

  /* readonly attribute PRUint32 language; */
  nsresult (*GetLanguage)(nsIStackFrame *this_, PRUint32 *aLanguage);

  /* readonly attribute string languageName; */
  nsresult (*GetLanguageName)(nsIStackFrame *this_, PRUnichar * *aLanguageName);

  /* readonly attribute string filename; */
  nsresult (*GetFilename)(nsIStackFrame *this_, PRUnichar * *aFilename);

  /* readonly attribute string name; */
  nsresult (*GetName)(nsIStackFrame *this_, PRUnichar * *aName);

  /* readonly attribute PRInt32 lineNumber; */
  nsresult (*GetLineNumber)(nsIStackFrame *this_, PRInt32 *aLineNumber);

  /* readonly attribute string sourceLine; */
  nsresult (*GetSourceLine)(nsIStackFrame *this_, PRUnichar * *aSourceLine);

  /* readonly attribute nsIStackFrame caller; */
  nsresult (*GetCaller)(nsIStackFrame *this_, nsIStackFrame * *aCaller);

  /* string toString (); */
  nsresult (*ToString)(nsIStackFrame *this_, PRUnichar **_retval);
};

struct nsIStackFrame {
    struct nsIStackFrame_vtbl *vtbl;
};

</xsl:text>
 <xsl:apply-templates/>
<xsl:text>
#endif /* !__cplusplus */

#ifdef IN_VBOXXPCOMC
# define VBOXXPCOMC_DECL(type)  PR_EXPORT(type)
#else
# define VBOXXPCOMC_DECL(type)  PR_IMPORT(type)
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Function table for dynamic linking.
 * Use VBoxGetFunctions() to obtain the pointer to it.
 */
typedef struct VBOXXPCOMC
{
    /** The size of the structure. */
    unsigned cb;
    /** The structure version. */
    unsigned uVersion;
    void  (*pfnComInitialize)(IVirtualBox **virtualBox, ISession **session);
    void  (*pfnComUninitialize)(void);

    void  (*pfnComUnallocMem)(void *pv);
    void  (*pfnUtf16Free)(PRUnichar *pwszString);
    void  (*pfnUtf8Free)(char *pszString);

    int   (*pfnUtf16ToUtf8)(const PRUnichar *pwszString, char **ppszString);
    int   (*pfnUtf8ToUtf16)(const char *pszString, PRUnichar **ppwszString);

    /** Tail version, same as uVersion. */
    unsigned uEndVersion;
} VBOXXPCOMC;
/** Pointer to a const VBoxXPCOMC function table. */
typedef VBOXXPCOMC const *PCVBOXXPCOM;

/** The current interface version.
 * For use with VBoxGetXPCOMCFunctions and to be found in
 * VBOXXPCOMC::uVersion. */
#define VBOX_XPCOMC_VERSION     0x00010000U

VBOXXPCOMC_DECL(PCVBOXXPCOM) VBoxGetXPCOMCFunctions(unsigned uVersion);
/** Typedef for VBoxGetXPCOMCFunctions. */
typedef PCVBOXXPCOM (*PFNVBOXGETXPCOMCFUNCTIONS)(unsigned uVersion);

/** The symbol name of VBoxGetXPCOMCFunctions. */
#if defined(__OS2__)
# define VBOX_GET_XPCOMC_FUNCTIONS_SYMBOL_NAME   "_VBoxGetXPCOMCFunctions"
#else
# define VBOX_GET_XPCOMC_FUNCTIONS_SYMBOL_NAME   "VBoxGetXPCOMCFunctions"
#endif


#ifdef __cplusplus
}
#endif

#endif /* !___VirtualBox_CXPCOM_h */
</xsl:text>
</xsl:template>

<!--
 *  ignore all |if|s except those for XPIDL target
<xsl:template match="if">
  <xsl:if test="@target='xpidl'">
    <xsl:apply-templates/>
  </xsl:if>
</xsl:template>
<xsl:template match="if" mode="forward">
  <xsl:if test="@target='xpidl'">
    <xsl:apply-templates mode="forward"/>
  </xsl:if>
</xsl:template>
<xsl:template match="if" mode="forwarder">
  <xsl:if test="@target='midl'">
    <xsl:apply-templates mode="forwarder"/>
  </xsl:if>
</xsl:template>

-->

<!--
 *  cpp_quote
<xsl:template match="cpp">
  <xsl:if test="text()">
    <xsl:text>%{C++</xsl:text>
    <xsl:value-of select="text()"/>
    <xsl:text>&#x0A;%}&#x0A;&#x0A;</xsl:text>
  </xsl:if>
  <xsl:if test="not(text()) and @line">
    <xsl:text>%{C++&#x0A;</xsl:text>
    <xsl:value-of select="@line"/>
    <xsl:text>&#x0A;%}&#x0A;&#x0A;</xsl:text>
  </xsl:if>
</xsl:template>
-->


<!--
 *  #if statement (@if attribute)
 *  @note
 *      xpidl doesn't support any preprocessor defines other than #include
 *      (it just ignores them), so the generated IDL will most likely be
 *      invalid. So for now we forbid using @if attributes
-->
<xsl:template match="@if" mode="begin">
  <xsl:message terminate="yes">
    @if attributes are not currently allowed because xpidl lacks
    support for #ifdef and stuff.
  </xsl:message>
  <xsl:text>#if </xsl:text>
  <xsl:value-of select="."/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>
<xsl:template match="@if" mode="end">
  <xsl:text>#endif&#x0A;</xsl:text>
</xsl:template>


<!--
 *  libraries
-->
<xsl:template match="library">
  <!-- result codes -->
  <xsl:text>&#x0A;</xsl:text>
  <xsl:for-each select="result">
    <xsl:apply-templates select="."/>
  </xsl:for-each>
  <xsl:text>&#x0A;&#x0A;</xsl:text>
  <!-- forward declarations -->
  <xsl:apply-templates select="if | interface | collection | enumerator" mode="forward"/>
  <xsl:text>&#x0A;</xsl:text>
  <!-- typedef'ing the struct declarations -->
  <xsl:apply-templates select="if | interface | collection | enumerator" mode="typedef"/>
  <xsl:text>&#x0A;</xsl:text>
  <!-- all enums go first -->
  <xsl:apply-templates select="enum | if/enum"/>
  <!-- everything else but result codes and enums -->
  <xsl:apply-templates select="*[not(self::result or self::enum) and
                                 not(self::if[result] or self::if[enum])]"/>
  <!-- -->
</xsl:template>


<!--
 *  result codes
-->
<xsl:template match="result">
  <xsl:value-of select="concat('#define ',@name,' ',@value)"/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>


<!--
 *  forward declarations
-->
<xsl:template match="interface | collection | enumerator" mode="forward">
  <xsl:text>struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  typedef'ing the struct declarations
-->
<xsl:template match="interface | collection | enumerator" mode="typedef">
  <xsl:text>typedef struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  interfaces
-->
<xsl:template match="interface">
  <xsl:text>/* Start of struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> Declaration */&#x0A;</xsl:text>
  <xsl:text>#define </xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:value-of select="concat('_IID_STR &quot;',@uuid,'&quot;')"/>
  <xsl:text>&#x0A;</xsl:text>
  <xsl:text>#define </xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_IID { \&#x0A;</xsl:text>
  <xsl:text>    0x</xsl:text><xsl:value-of select="substring(@uuid,1,8)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,10,4)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,15,4)"/>
  <xsl:text>, \&#x0A;    </xsl:text>
  <xsl:text>{ 0x</xsl:text><xsl:value-of select="substring(@uuid,20,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,22,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,25,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,27,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,29,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,31,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,33,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,35,2)"/>
  <xsl:text> } \&#x0A;}&#x0A;</xsl:text>
  <xsl:text>struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>_vtbl&#x0A;{&#x0A;</xsl:text>
  <xsl:text>    </xsl:text>
  <xsl:choose>
    <xsl:when test="@extends='$unknown'">struct nsISupports_vtbl nsisupports;</xsl:when>
    <xsl:when test="@extends='$dispatched'">struct nsISupports_vtbl nsisupports;</xsl:when>
    <xsl:when test="@extends='$errorinfo'">struct nsIException_vtbl nsiexception;</xsl:when>
    <xsl:otherwise>
      <xsl:text>struct </xsl:text>
      <xsl:value-of select="@extends"/>
      <xsl:text>_vtbl </xsl:text>
      <xsl:call-template name="lowercase">
        <xsl:with-param name="str" select="@extends"/>
      </xsl:call-template>
      <xsl:text>;</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:text>&#x0A;&#x0A;</xsl:text>
  <!-- attributes (properties) -->
  <xsl:apply-templates select="attribute"/>
  <!-- methods -->
  <xsl:apply-templates select="method"/>
  <!-- 'if' enclosed elements, unsorted -->
  <xsl:apply-templates select="if"/>
  <!-- -->
  <xsl:text>};</xsl:text>
  <xsl:text>&#x0A;&#x0A;</xsl:text>
  <xsl:text>struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>&#x0A;{&#x0A;    struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>_vtbl *vtbl;&#x0A;};&#x0A;</xsl:text>
  <xsl:text>/* End of struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> Declaration */&#x0A;&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  attributes
-->
<xsl:template match="interface//attribute | collection//attribute">
  <xsl:if test="@array">
    <xsl:message terminate="yes">
      <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
      <xsl:text>'array' attributes are not supported, use 'safearray="yes"' instead.</xsl:text>
    </xsl:message>
  </xsl:if>
  <xsl:apply-templates select="@if" mode="begin"/>
  <xsl:if test="@mod='ptr'">
    <!-- attributes using native types must be non-scriptable
    <xsl:text>    [noscript]&#x0A;</xsl:text>-->
  </xsl:if>
  <xsl:choose>
    <!-- safearray pseudo attribute -->
    <xsl:when test="@safearray='yes'">
      <!-- getter -->
      <xsl:text>    nsresult (*Get</xsl:text>
      <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name"/>
      </xsl:call-template>
      <xsl:text>)(</xsl:text>
      <xsl:value-of select="../@name" />
      <xsl:text> *this_, </xsl:text>
      <!-- array size -->
      <xsl:text>PRUint32 *</xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>Size, </xsl:text>
      <!-- array pointer -->
      <xsl:apply-templates select="@type" mode="forwarder"/>
      <xsl:text> **</xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>);&#x0A;</xsl:text>
      <!-- setter -->
      <xsl:if test="not(@readonly='yes')">
        <xsl:text>    nsresult set</xsl:text>
        <xsl:call-template name="capitalize">
          <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
        <xsl:text> (&#x0A;</xsl:text>
        <!-- array size -->
        <xsl:text>        in unsigned long </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>Size,&#x0A;</xsl:text>
        <!-- array pointer -->
        <xsl:text>        [array, size_is(</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>Size)] in </xsl:text>
        <xsl:apply-templates select="@type" mode="forwarder"/>
        <xsl:text> </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#x0A;    );&#x0A;</xsl:text>
      </xsl:if>
    </xsl:when>
    <!-- normal attribute -->
    <xsl:otherwise>
      <xsl:text>    </xsl:text>
      <xsl:if test="@readonly='yes'">
        <xsl:text>nsresult (*Get</xsl:text>
        <xsl:call-template name="capitalize">
          <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
        <xsl:text>)(</xsl:text>
        <xsl:value-of select="../@name" />
        <xsl:text> *this_, </xsl:text>
        <xsl:apply-templates select="@type" mode="forwarder"/>
        <xsl:text> *</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>);&#x0A;</xsl:text>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="@readonly='yes'">
        </xsl:when>
        <xsl:otherwise>
          <xsl:text>nsresult (*Get</xsl:text>
          <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
          </xsl:call-template>
          <xsl:text>)(</xsl:text>
          <xsl:value-of select="../@name" />
          <xsl:text> *this_, </xsl:text>
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:text> *</xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:text>);&#x0A;    </xsl:text>
          <xsl:text>nsresult (*Set</xsl:text>
          <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
          </xsl:call-template>
          <xsl:text>)(</xsl:text>
          <xsl:value-of select="../@name" />
          <xsl:text> *this_, </xsl:text>
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:text> </xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:text>);&#x0A;</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:apply-templates select="@if" mode="end"/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<xsl:template match="interface//attribute | collection//attribute" mode="forwarder">

  <xsl:variable name="parent" select="ancestor::interface | ancestor::collection"/>

  <xsl:apply-templates select="@if" mode="begin"/>

  <!-- getter: COM_FORWARD_Interface_GETTER_Name_TO(smth) -->
  <xsl:text>#define COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_GETTER_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO(smth) NS_IMETHOD Get</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text> (</xsl:text>
  <xsl:if test="@safearray='yes'">
    <xsl:text>PRUint32 * a</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>Size, </xsl:text>
  </xsl:if>
  <xsl:apply-templates select="@type" mode="forwarder"/>
  <xsl:if test="@safearray='yes'">
    <xsl:text> *</xsl:text>
  </xsl:if>
  <xsl:text> * a</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>) { return smth Get</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text> (</xsl:text>
  <xsl:if test="@safearray='yes'">
    <xsl:text>a</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>Size, </xsl:text>
  </xsl:if>
  <xsl:text>a</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>); }&#x0A;</xsl:text>
  <!-- getter: COM_FORWARD_Interface_GETTER_Name_TO_OBJ(obj) -->
  <xsl:text>#define COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_GETTER_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO_OBJ(obj) COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_GETTER_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO ((obj)->)&#x0A;</xsl:text>
  <!-- getter: COM_FORWARD_Interface_GETTER_Name_TO_BASE(base) -->
  <xsl:text>#define COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_GETTER_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO_BASE(base) COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_GETTER_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO (base::)&#x0A;</xsl:text>
  <!-- -->
  <xsl:if test="not(@readonly='yes')">
    <!-- setter: COM_FORWARD_Interface_SETTER_Name_TO(smth) -->
    <xsl:text>#define COM_FORWARD_</xsl:text>
    <xsl:value-of select="$parent/@name"/>
    <xsl:text>_SETTER_</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>_TO(smth) NS_IMETHOD Set</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text> (</xsl:text>
    <xsl:if test="@safearray='yes'">
      <xsl:text>PRUint32 a</xsl:text>
      <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name"/>
      </xsl:call-template>
      <xsl:text>Size, </xsl:text>
    </xsl:if>
    <xsl:if test="not(@safearray='yes') and (@type='string' or @type='wstring')">
      <xsl:text>const </xsl:text>
    </xsl:if>
    <xsl:apply-templates select="@type" mode="forwarder"/>
    <xsl:if test="@safearray='yes'">
      <xsl:text> *</xsl:text>
    </xsl:if>
    <xsl:text> a</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>) { return smth Set</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text> (a</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>); }&#x0A;</xsl:text>
    <!-- setter: COM_FORWARD_Interface_SETTER_Name_TO_OBJ(obj) -->
    <xsl:text>#define COM_FORWARD_</xsl:text>
    <xsl:value-of select="$parent/@name"/>
    <xsl:text>_SETTER_</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>_TO_OBJ(obj) COM_FORWARD_</xsl:text>
    <xsl:value-of select="$parent/@name"/>
    <xsl:text>_SETTER_</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>_TO ((obj)->)&#x0A;</xsl:text>
    <!-- setter: COM_FORWARD_Interface_SETTER_Name_TO_BASE(base) -->
    <xsl:text>#define COM_FORWARD_</xsl:text>
    <xsl:value-of select="$parent/@name"/>
    <xsl:text>_SETTER_</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>_TO_BASE(base) COM_FORWARD_</xsl:text>
    <xsl:value-of select="$parent/@name"/>
    <xsl:text>_SETTER_</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>_TO (base::)&#x0A;</xsl:text>
  </xsl:if>

  <xsl:apply-templates select="@if" mode="end"/>

</xsl:template>


<!--
 *  methods
-->
<xsl:template match="interface//method | collection//method">
  <xsl:apply-templates select="@if" mode="begin"/>
  <xsl:if test="param/@mod='ptr'">
    <!-- methods using native types must be non-scriptable
    <xsl:text>    [noscript]&#x0A;</xsl:text>-->
  </xsl:if>
  <xsl:text>    nsresult (*</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:if test="param">
    <xsl:text>)(&#x0A;</xsl:text>
    <xsl:text>        </xsl:text>
    <xsl:value-of select="../@name" />
    <xsl:text> *this_,&#x0A;</xsl:text>
    <xsl:for-each select="param [position() != last()]">
      <xsl:text>        </xsl:text>
      <xsl:apply-templates select="."/>
      <xsl:text>,&#x0A;</xsl:text>
    </xsl:for-each>
    <xsl:text>        </xsl:text>
    <xsl:apply-templates select="param [last()]"/>
    <xsl:text>&#x0A;    );&#x0A;</xsl:text>
  </xsl:if>
  <xsl:if test="not(param)">
    <xsl:text>)(</xsl:text>
    <xsl:value-of select="../@name" />
    <xsl:text> *this_ );&#x0A;</xsl:text>
  </xsl:if>
  <xsl:apply-templates select="@if" mode="end"/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<xsl:template match="interface//method | collection//method" mode="forwarder">

  <xsl:variable name="parent" select="ancestor::interface | ancestor::collection"/>

  <xsl:apply-templates select="@if" mode="begin"/>

  <xsl:text>#define COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO(smth) NS_IMETHOD </xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:choose>
    <xsl:when test="param">
      <xsl:text> (</xsl:text>
      <xsl:for-each select="param [position() != last()]">
        <xsl:apply-templates select="." mode="forwarder"/>
        <xsl:text>, </xsl:text>
      </xsl:for-each>
      <xsl:apply-templates select="param [last()]" mode="forwarder"/>
      <xsl:text>) { return smth </xsl:text>
      <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name"/>
      </xsl:call-template>
      <xsl:text> (</xsl:text>
      <xsl:for-each select="param [position() != last()]">
        <xsl:if test="@safearray='yes'">
          <xsl:text>a</xsl:text>
          <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
          </xsl:call-template>
          <xsl:text>Size+++, </xsl:text>
        </xsl:if>
        <xsl:text>a</xsl:text>
        <xsl:call-template name="capitalize">
          <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
        <xsl:text>, </xsl:text>
      </xsl:for-each>
      <xsl:if test="param [last()]/@safearray='yes'">
        <xsl:text>a</xsl:text>
        <xsl:call-template name="capitalize">
          <xsl:with-param name="str" select="param [last()]/@name"/>
        </xsl:call-template>
        <xsl:text>Size, </xsl:text>
      </xsl:if>
      <xsl:text>a</xsl:text>
      <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="param [last()]/@name"/>
      </xsl:call-template>
      <xsl:text>); }</xsl:text>
    </xsl:when>
    <xsl:otherwise test="not(param)">
      <xsl:text>() { return smth </xsl:text>
      <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name"/>
      </xsl:call-template>
      <xsl:text>(); }</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:text>&#x0A;</xsl:text>
  <!-- COM_FORWARD_Interface_Method_TO_OBJ(obj) -->
  <xsl:text>#define COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO_OBJ(obj) COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO ((obj)->)&#x0A;</xsl:text>
  <!-- COM_FORWARD_Interface_Method_TO_BASE(base) -->
  <xsl:text>#define COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO_BASE(base) COM_FORWARD_</xsl:text>
  <xsl:value-of select="$parent/@name"/>
  <xsl:text>_</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_TO (base::)&#x0A;</xsl:text>

  <xsl:apply-templates select="@if" mode="end"/>

</xsl:template>


<!--
 *  modules
-->
<xsl:template match="module">
  <xsl:apply-templates select="class"/>
</xsl:template>


<!--
 *  co-classes
-->
<xsl:template match="module/class">
  <!-- class and contract id -->
  <xsl:text>&#x0A;</xsl:text>
  <xsl:text>#define NS_</xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_CID { \&#x0A;</xsl:text>
  <xsl:text>    0x</xsl:text><xsl:value-of select="substring(@uuid,1,8)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,10,4)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,15,4)"/>
  <xsl:text>, \&#x0A;    </xsl:text>
  <xsl:text>{ 0x</xsl:text><xsl:value-of select="substring(@uuid,20,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,22,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,25,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,27,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,29,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,31,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,33,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,35,2)"/>
  <xsl:text> } \&#x0A;}&#x0A;</xsl:text>
  <xsl:text>#define NS_</xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <!-- Contract ID -->
  <xsl:text>_CONTRACTID &quot;@</xsl:text>
  <xsl:value-of select="@namespace"/>
  <xsl:text>/</xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>;1&quot;&#x0A;</xsl:text>
  <!-- CLSID_xxx declarations for XPCOM, for compatibility with Win32 -->
  <xsl:text>/* for compatibility with Win32 */&#x0A;</xsl:text>
  <xsl:text>#define CLSID_</xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> (nsCID) NS_</xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_CID&#x0A;</xsl:text>
  <xsl:text>&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  enumerators
-->
<xsl:template match="enumerator">
  <xsl:text>/* Start of struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> Declaration */&#x0A;</xsl:text>
  <xsl:text>#define </xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:value-of select="concat('_IID_STR &quot;',@uuid,'&quot;')"/>
  <xsl:text>&#x0A;</xsl:text>
  <xsl:text>#define </xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_IID { \&#x0A;</xsl:text>
  <xsl:text>    0x</xsl:text><xsl:value-of select="substring(@uuid,1,8)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,10,4)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,15,4)"/>
  <xsl:text>, \&#x0A;    </xsl:text>
  <xsl:text>{ 0x</xsl:text><xsl:value-of select="substring(@uuid,20,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,22,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,25,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,27,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,29,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,31,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,33,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,35,2)"/>
  <xsl:text> } \&#x0A;}&#x0A;</xsl:text>
  <xsl:text>struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>_vtbl&#x0A;{&#x0A;</xsl:text>
  <xsl:text>    struct nsISupports_vtbl nsisupports;&#x0A;&#x0A;</xsl:text>
  <!-- attributes (properties) -->
  <xsl:text>    nsresult (*HasMore)(</xsl:text>
  <xsl:value-of select="@name" />
  <xsl:text> *this_, PRBool *more);&#x0A;&#x0A;</xsl:text>
  <!-- GetNext -->
  <xsl:text>    nsresult (*GetNext)(</xsl:text>
  <xsl:value-of select="@name" />
  <xsl:text> *this_, </xsl:text>
  <xsl:apply-templates select="@type" mode="forwarder"/>
  <xsl:text> *next);&#x0A;&#x0A;</xsl:text>
  <xsl:text>};</xsl:text>
  <xsl:text>&#x0A;&#x0A;</xsl:text>
  <xsl:text>struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>&#x0A;{&#x0A;    struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>_vtbl *vtbl;&#x0A;};&#x0A;</xsl:text>
  <xsl:text>/* End of struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> Declaration */&#x0A;&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  collections
-->
<xsl:template match="collection">
  <xsl:if test="not(@readonly='yes')">
    <xsl:message terminate="yes">
      <xsl:value-of select="concat(@name,': ')"/>
      <xsl:text>non-readonly collections are not currently supported</xsl:text>
    </xsl:message>
  </xsl:if>
  <xsl:text>/* Start of struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> Declaration */&#x0A;</xsl:text>
  <xsl:text>#define </xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:value-of select="concat('_IID_STR &quot;',@uuid,'&quot;')"/>
  <xsl:text>&#x0A;</xsl:text>
  <xsl:text>#define </xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_IID { \&#x0A;</xsl:text>
  <xsl:text>    0x</xsl:text><xsl:value-of select="substring(@uuid,1,8)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,10,4)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,15,4)"/>
  <xsl:text>, \&#x0A;    </xsl:text>
  <xsl:text>{ 0x</xsl:text><xsl:value-of select="substring(@uuid,20,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,22,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,25,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,27,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,29,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,31,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,33,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,35,2)"/>
  <xsl:text> } \&#x0A;}&#x0A;</xsl:text>
  <xsl:text>struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>_vtbl&#x0A;{&#x0A;</xsl:text>
  <xsl:text>    struct nsISupports_vtbl nsisupports;&#x0A;&#x0A;</xsl:text>
  <!-- Count -->
  <xsl:text>    nsresult (*GetCount)(</xsl:text>
  <xsl:value-of select="@name" />
  <xsl:text> *this_, PRUint32 *aCount);&#x0A;&#x0A;</xsl:text>
  <!-- GetItemAt -->
  <xsl:text>    nsresult (*GetItemAt)(</xsl:text>
  <xsl:value-of select="@name" />
  <xsl:text> *this_, PRUint32 index, </xsl:text>
  <xsl:apply-templates select="@type" mode="forwarder"/>
  <xsl:text> **item);&#x0A;&#x0A;</xsl:text>
  <!-- Enumerate -->
  <xsl:text>    nsresult (*Enumerate)(</xsl:text>
  <xsl:value-of select="@name" />
  <xsl:text> *this_, </xsl:text>
  <xsl:apply-templates select="@enumerator"/>
  <xsl:text> **enumerator);&#x0A;&#x0A;</xsl:text>
  <!-- other extra attributes (properties) -->
  <xsl:apply-templates select="attribute"/>
  <!-- other extra methods -->
  <xsl:apply-templates select="method"/>
  <!-- 'if' enclosed elements, unsorted -->
  <xsl:apply-templates select="if"/>
  <xsl:text>};</xsl:text>
  <xsl:text>&#x0A;&#x0A;</xsl:text>
  <xsl:text>struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>&#x0A;{&#x0A;    struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>_vtbl *vtbl;&#x0A;};&#x0A;</xsl:text>
  <xsl:text>/* End of struct </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> Declaration */&#x0A;&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  enums
-->
<xsl:template match="enum">
  <xsl:text>/* Start of enum </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> Declaration */&#x0A;</xsl:text>
  <xsl:text>#define </xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:value-of select="concat('_IID_STR &quot;',@uuid,'&quot;')"/>
  <xsl:text>&#x0A;</xsl:text>
  <xsl:text>#define </xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_IID { \&#x0A;</xsl:text>
  <xsl:text>    0x</xsl:text><xsl:value-of select="substring(@uuid,1,8)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,10,4)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,15,4)"/>
  <xsl:text>, \&#x0A;    </xsl:text>
  <xsl:text>{ 0x</xsl:text><xsl:value-of select="substring(@uuid,20,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,22,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,25,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,27,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,29,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,31,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,33,2)"/>
  <xsl:text>, 0x</xsl:text><xsl:value-of select="substring(@uuid,35,2)"/>
  <xsl:text> } \&#x0A;}&#x0A;</xsl:text>
  <xsl:text>enum </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>&#x0A;{&#x0A;</xsl:text>
  <xsl:variable name="this" select="."/>
  <xsl:for-each select="const">
    <xsl:text>    </xsl:text>
    <xsl:value-of select="$this/@name"/>
    <xsl:text>_</xsl:text>
    <xsl:value-of select="@name"/> = <xsl:value-of select="@value"/>
    <xsl:if test="position() != last()">
      <xsl:text>,</xsl:text>
    </xsl:if>
    <xsl:text>&#x0A;</xsl:text>
  </xsl:for-each>
  <xsl:text>};&#x0A;</xsl:text>
  <xsl:text>/* End of enum </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> Declaration */&#x0A;&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  method parameters
-->
<xsl:template match="method/param">
  <xsl:choose>
    <!-- safearray parameters -->
    <xsl:when test="@safearray='yes'">
      <!-- array size -->
      <xsl:choose>
        <xsl:when test="@dir='in'">
          <xsl:text>PRUint32 </xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:text>Size,&#x0A;</xsl:text>
        </xsl:when>
        <xsl:when test="@dir='out'">
          <xsl:text>PRUint32 *</xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:text>Size,&#x0A;</xsl:text>
        </xsl:when>
        <xsl:when test="@dir='return'">
          <xsl:text>PRUint32 *</xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:text>Size,&#x0A;</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:text>PRUint32 </xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:text>Size,&#x0A;</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
      <!-- array pointer -->
      <xsl:text>        </xsl:text>
      <xsl:choose>
        <xsl:when test="@dir='in'">
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:text>*</xsl:text>
        </xsl:when>
        <xsl:when test="@dir='out'">
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:if test="@type='wstring'">
            <xsl:text>*</xsl:text>
          </xsl:if>
          <xsl:text>*</xsl:text>
        </xsl:when>
        <xsl:when test="@dir='return'">
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:text>**</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:text>*</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:text> </xsl:text>
      <xsl:value-of select="@name"/>
    </xsl:when>
    <!-- normal and array parameters -->
    <xsl:otherwise>
      <xsl:if test="@array">
        <xsl:if test="@dir='return'">
          <xsl:message terminate="yes">
            <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
            <xsl:text>return 'array' parameters are not supported, use 'safearray="yes"' instead.</xsl:text>
          </xsl:message>
        </xsl:if>
        <xsl:text>[array, </xsl:text>
        <xsl:choose>
          <xsl:when test="../param[@name=current()/@array]">
            <xsl:if test="../param[@name=current()/@array]/@dir != @dir">
              <xsl:message terminate="yes">
                <xsl:value-of select="concat(../../@name,'::',../@name,': ')"/>
                <xsl:value-of select="concat(@name,' and ',../param[@name=current()/@array]/@name)"/>
                <xsl:text> must have the same direction</xsl:text>
              </xsl:message>
            </xsl:if>
            <xsl:text>size_is(</xsl:text>
            <xsl:value-of select="@array"/>
            <xsl:text>)</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:message terminate="yes">
              <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
              <xsl:text>array attribute refers to non-existent param: </xsl:text>
              <xsl:value-of select="@array"/>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>
        <xsl:text>] </xsl:text>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="@dir='in'">
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:text></xsl:text>
        </xsl:when>
        <xsl:when test="@dir='out'">
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:text> *</xsl:text>
        </xsl:when>
        <xsl:when test="@dir='return'">
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:text> *</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates select="@type" mode="forwarder"/>
          <xsl:text></xsl:text>
        </xsl:otherwise>
      </xsl:choose>
      <xsl:text> </xsl:text>
      <xsl:value-of select="@name"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="method/param" mode="forwarder">
  <xsl:if test="@safearray='yes'">
    <xsl:text>PRUint32</xsl:text>
    <xsl:if test="@dir='out' or @dir='return'">
      <xsl:text> *</xsl:text>
    </xsl:if>
    <xsl:text> a</xsl:text>
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>Size, </xsl:text>
  </xsl:if>
  <xsl:apply-templates select="@type" mode="forwarder"/>
  <xsl:if test="@dir='out' or @dir='return'">
    <xsl:text> *</xsl:text>
  </xsl:if>
  <xsl:if test="@safearray='yes'">
    <xsl:text> *</xsl:text>
  </xsl:if>
  <xsl:text> a</xsl:text>
  <xsl:call-template name="capitalize">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
</xsl:template>


<!--
 *  attribute/parameter type conversion
-->
<xsl:template match="
  attribute/@type | param/@type |
  enumerator/@type | collection/@type | collection/@enumerator
">
  <xsl:variable name="self_target" select="current()/ancestor::if/@target"/>

  <xsl:if test="../@array and ../@safearray='yes'">
    <xsl:message terminate="yes">
      <xsl:value-of select="concat(../../../@name,'::',../../@name,'::',../@name,': ')"/>
      <xsl:text>either 'array' or 'safearray="yes"' attribute is allowed, but not both!</xsl:text>
    </xsl:message>
  </xsl:if>

  <xsl:choose>
    <!-- modifiers (ignored for 'enumeration' attributes)-->
    <xsl:when test="name(current())='type' and ../@mod">
      <xsl:choose>
        <xsl:when test="../@mod='ptr'">
          <xsl:choose>
            <!-- standard types -->
            <!--xsl:when test=".='result'">??</xsl:when-->
            <xsl:when test=".='boolean'">booleanPtr</xsl:when>
            <xsl:when test=".='octet'">octetPtr</xsl:when>
            <xsl:when test=".='short'">shortPtr</xsl:when>
            <xsl:when test=".='unsigned short'">ushortPtr</xsl:when>
            <xsl:when test=".='long'">longPtr</xsl:when>
            <xsl:when test=".='long long'">llongPtr</xsl:when>
            <xsl:when test=".='unsigned long'">ulongPtr</xsl:when>
            <xsl:when test=".='unsigned long long'">ullongPtr</xsl:when>
            <xsl:when test=".='char'">charPtr</xsl:when>
            <!--xsl:when test=".='string'">??</xsl:when-->
            <xsl:when test=".='wchar'">wcharPtr</xsl:when>
            <!--xsl:when test=".='wstring'">??</xsl:when-->
            <xsl:otherwise>
              <xsl:message terminate="yes">
                <xsl:value-of select="concat(../../../@name,'::',../../@name,'::',../@name,': ')"/>
                <xsl:text>attribute 'mod=</xsl:text>
                <xsl:value-of select="concat('&quot;',../@mod,'&quot;')"/>
                <xsl:text>' cannot be used with type </xsl:text>
                <xsl:value-of select="concat('&quot;',current(),'&quot;!')"/>
              </xsl:message>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:value-of select="concat(../../../@name,'::',../../@name,'::',../@name,': ')"/>
            <xsl:value-of select="concat('value &quot;',../@mod,'&quot; ')"/>
            <xsl:text>of attribute 'mod' is invalid!</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <!-- no modifiers -->
    <xsl:otherwise>
      <xsl:choose>
        <!-- standard types -->
        <xsl:when test=".='result'">nsresult</xsl:when>
        <xsl:when test=".='boolean'">boolean</xsl:when>
        <xsl:when test=".='octet'">octet</xsl:when>
        <xsl:when test=".='short'">short</xsl:when>
        <xsl:when test=".='unsigned short'">unsigned short</xsl:when>
        <xsl:when test=".='long'">long</xsl:when>
        <xsl:when test=".='long long'">long long</xsl:when>
        <xsl:when test=".='unsigned long'">unsigned long</xsl:when>
        <xsl:when test=".='unsigned long long'">unsigned long long</xsl:when>
        <xsl:when test=".='char'">char</xsl:when>
        <xsl:when test=".='wchar'">wchar</xsl:when>
        <xsl:when test=".='string'">string</xsl:when>
        <xsl:when test=".='wstring'">wstring</xsl:when>
        <!-- UUID type -->
        <xsl:when test=".='uuid'">
          <xsl:choose>
            <xsl:when test="name(..)='attribute'">
              <xsl:choose>
                <xsl:when test="../@readonly='yes'">
                  <xsl:text>nsIDPtr</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                  <xsl:message terminate="yes">
                    <xsl:value-of select="../@name"/>
                    <xsl:text>: Non-readonly uuid attributes are not supported!</xsl:text>
                  </xsl:message>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:when>
            <xsl:when test="name(..)='param'">
              <xsl:choose>
                <xsl:when test="../@dir='in' and not(../@safearray='yes')">
                  <xsl:text>nsIDRef</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                  <xsl:text>nsIDPtr</xsl:text>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:when>
          </xsl:choose>
        </xsl:when>
        <!-- system interface types -->
        <xsl:when test=".='$unknown'">nsISupports</xsl:when>
        <xsl:otherwise>
          <xsl:choose>
            <!-- enum types -->
            <xsl:when test="
              (ancestor::library/enum[@name=current()]) or
              (ancestor::library/if[@target=$self_target]/enum[@name=current()])
            ">
              <xsl:text>PRUint32</xsl:text>
            </xsl:when>
            <!-- custom interface types -->
            <xsl:when test="
              (name(current())='enumerator' and
               ((ancestor::library/enumerator[@name=current()]) or
                (ancestor::library/if[@target=$self_target]/enumerator[@name=current()]))
              ) or
              ((ancestor::library/interface[@name=current()]) or
               (ancestor::library/if[@target=$self_target]/interface[@name=current()])
              ) or
              ((ancestor::library/collection[@name=current()]) or
               (ancestor::library/if[@target=$self_target]/collection[@name=current()])
              )
            ">
              <xsl:value-of select="."/>
            </xsl:when>
            <!-- other types -->
            <xsl:otherwise>
              <xsl:message terminate="yes">
                <xsl:text>Unknown parameter type: </xsl:text>
                <xsl:value-of select="."/>
              </xsl:message>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="
  attribute/@type | param/@type |
  enumerator/@type | collection/@type | collection/@enumerator
" mode="forwarder">

  <xsl:variable name="self_target" select="current()/ancestor::if/@target"/>

  <xsl:choose>
    <!-- modifiers (ignored for 'enumeration' attributes)-->
    <xsl:when test="name(current())='type' and ../@mod">
      <xsl:choose>
        <xsl:when test="../@mod='ptr'">
          <xsl:choose>
            <!-- standard types -->
            <!--xsl:when test=".='result'">??</xsl:when-->
            <xsl:when test=".='boolean'">PRBool *</xsl:when>
            <xsl:when test=".='octet'">PRUint8 *</xsl:when>
            <xsl:when test=".='short'">PRInt16 *</xsl:when>
            <xsl:when test=".='unsigned short'">PRUint16 *</xsl:when>
            <xsl:when test=".='long'">PRInt32 *</xsl:when>
            <xsl:when test=".='long long'">PRInt64 *</xsl:when>
            <xsl:when test=".='unsigned long'">PRUint32 *</xsl:when>
            <xsl:when test=".='unsigned long long'">PRUint64 *</xsl:when>
            <xsl:when test=".='char'">char *</xsl:when>
            <!--xsl:when test=".='string'">??</xsl:when-->
            <xsl:when test=".='wchar'">PRUnichar *</xsl:when>
            <!--xsl:when test=".='wstring'">??</xsl:when-->
          </xsl:choose>
        </xsl:when>
      </xsl:choose>
    </xsl:when>
    <!-- no modifiers -->
    <xsl:otherwise>
      <xsl:choose>
        <!-- standard types -->
        <xsl:when test=".='result'">nsresult</xsl:when>
        <xsl:when test=".='boolean'">PRBool</xsl:when>
        <xsl:when test=".='octet'">PRUint8</xsl:when>
        <xsl:when test=".='short'">PRInt16</xsl:when>
        <xsl:when test=".='unsigned short'">PRUint16</xsl:when>
        <xsl:when test=".='long'">PRInt32</xsl:when>
        <xsl:when test=".='long long'">PRInt64</xsl:when>
        <xsl:when test=".='unsigned long'">PRUint32</xsl:when>
        <xsl:when test=".='unsigned long long'">PRUint64</xsl:when>
        <xsl:when test=".='char'">char</xsl:when>
        <xsl:when test=".='wchar'">PRUnichar</xsl:when>
        <!-- string types -->
        <xsl:when test=".='string'">char *</xsl:when>
        <xsl:when test=".='wstring'">PRUnichar *</xsl:when>
        <!-- UUID type -->
        <xsl:when test=".='uuid'">
          <xsl:choose>
            <xsl:when test="name(..)='attribute'">
              <xsl:choose>
                <xsl:when test="../@readonly='yes'">
                  <xsl:text>nsID *</xsl:text>
                </xsl:when>
              </xsl:choose>
            </xsl:when>
            <xsl:when test="name(..)='param'">
              <xsl:choose>
                <xsl:when test="../@dir='in' and not(../@safearray='yes')">
                  <xsl:text>const nsID *</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                  <xsl:text>nsID *</xsl:text>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:when>
          </xsl:choose>
        </xsl:when>
        <!-- system interface types -->
        <xsl:when test=".='$unknown'">nsISupports *</xsl:when>
        <xsl:otherwise>
          <xsl:choose>
            <!-- enum types -->
            <xsl:when test="
              (ancestor::library/enum[@name=current()]) or
              (ancestor::library/if[@target=$self_target]/enum[@name=current()])
            ">
              <xsl:text>PRUint32</xsl:text>
            </xsl:when>
            <!-- custom interface types -->
            <xsl:when test="
              (name(current())='enumerator' and
               ((ancestor::library/enumerator[@name=current()]) or
                (ancestor::library/if[@target=$self_target]/enumerator[@name=current()]))
              ) or
              ((ancestor::library/interface[@name=current()]) or
               (ancestor::library/if[@target=$self_target]/interface[@name=current()])
              ) or
              ((ancestor::library/collection[@name=current()]) or
               (ancestor::library/if[@target=$self_target]/collection[@name=current()])
              )
            ">
              <xsl:value-of select="."/>
              <xsl:text> *</xsl:text>
            </xsl:when>
            <!-- other types -->
          </xsl:choose>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

</xsl:stylesheet>

