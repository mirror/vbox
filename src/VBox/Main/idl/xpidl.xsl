<?xml version="1.0"?>

<!--
 *  A template to generate a XPCOM IDL compatible interface definition file
 *  from the generic interface definition expressed in XML.

     Copyright (C) 2006-2007 innotek GmbH

     This file is part of VirtualBox Open Source Edition (OSE), as
     available from http://www.virtualbox.org. This file is free software;
     you can redistribute it and/or modify it under the terms of the GNU
     General Public License (GPL) as published by the Free Software
     Foundation, in version 2 as it comes in the "COPYING" file of the
     VirtualBox OSE distribution. VirtualBox OSE is distributed in the
     hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
//  templates
/////////////////////////////////////////////////////////////////////////////
-->


<!--
 *  header
-->
<xsl:template match="/idl">
  <xsl:text>
/*
 *  DO NOT EDIT! This is a generated file.
 *
 *  XPCOM IDL (XPIDL) definition for VirualBox Main API (COM interfaces)
 *  generated from XIDL (XML interface definition).
 *
 *  Source    : src/VBox/Main/idl/VirtualBox.xidl
 *  Generator : src/VBox/Main/idl/xpidl.xsl
 */

#include "nsISupports.idl"
#include "nsIException.idl"
</xsl:text>
  <!-- native typedefs for the 'mod="ptr"' attribute -->
  <xsl:text>
[ptr] native booeanPtr  (PRBool);
[ptr] native octetPtr   (PRUint8);
[ptr] native shortPtr   (PRInt16);
[ptr] native ushortPtr  (PRUint16);
[ptr] native longPtr    (PRInt32);
[ptr] native llongPtr   (PRInt64);
[ptr] native ulongPtr   (PRUint32);
[ptr] native ullongPtr  (PRUint64);
<!-- charPtr is already defined in nsrootidl.idl -->
<!-- [ptr] native charPtr    (char) -->
[ptr] native stringPtr  (string);
[ptr] native wcharPtr   (wchar);
[ptr] native wstringPtr (wstring);

</xsl:text>
  <xsl:apply-templates/>
</xsl:template>


<!--
 *  ignore all |if|s except those for XPIDL target
-->
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


<!--
 *  cpp_quote
-->
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
    support of #ifdef and stuff.
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
  <!-- forward declarations -->
  <xsl:apply-templates select="if | interface | collection | enumerator" mode="forward"/>
  <xsl:text>&#x0A;</xsl:text>
  <!-- all enums go first -->
  <xsl:apply-templates select="enum | if/enum"/>
  <!-- everything else but enums -->
  <xsl:apply-templates select="*[not(self::enum) and not(self::if[enum])]"/>
</xsl:template>


<!--
 *  forward declarations
-->
<xsl:template match="interface | collection | enumerator" mode="forward">
  <xsl:text>interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  interfaces
-->
<xsl:template match="interface">[
    uuid(<xsl:value-of select="@uuid"/>),
    scriptable
]
<xsl:text>interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> : </xsl:text>
  <xsl:choose>
      <xsl:when test="@extends='$unknown'">nsISupports</xsl:when>
      <xsl:when test="@extends='$dispatched'">nsISupports</xsl:when>
      <xsl:when test="@extends='$errorinfo'">nsIException</xsl:when>
      <xsl:otherwise><xsl:value-of select="@extends"/></xsl:otherwise>
  </xsl:choose>
  <xsl:text>&#x0A;{&#x0A;</xsl:text>
  <!-- attributes (properties) -->
  <xsl:apply-templates select="attribute"/>
  <!-- methods -->
  <xsl:apply-templates select="method"/>
  <!-- 'if' enclosed elements, unsorted -->
  <xsl:apply-templates select="if"/>
  <!-- -->
  <xsl:text>}; /* interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> */&#x0A;&#x0A;</xsl:text>
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
    <!-- attributes using native types must be non-scriptable -->
    <xsl:text>    [noscript]&#x0A;</xsl:text>
  </xsl:if>
  <xsl:choose>
    <!-- safearray pseudo attribute -->
    <xsl:when test="@safearray='yes'">
      <!-- getter -->
      <xsl:text>    void get</xsl:text>
      <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name"/>
      </xsl:call-template>
      <xsl:text> (&#x0A;</xsl:text>
      <!-- array size -->
      <xsl:text>        out unsigned long </xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>Size,&#x0A;</xsl:text>
      <!-- array pointer -->
      <xsl:text>        [array, size_is(</xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>Size), retval] out </xsl:text>
      <xsl:apply-templates select="@type"/>
      <xsl:text> </xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>&#x0A;    );&#x0A;</xsl:text>
      <!-- setter -->
      <xsl:if test="not(@readonly='yes')">
        <xsl:text>    void set</xsl:text>
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
        <xsl:apply-templates select="@type"/>
        <xsl:text> </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#x0A;    );&#x0A;</xsl:text>
      </xsl:if>
    </xsl:when>
    <!-- normal attribute -->
    <xsl:otherwise>
      <xsl:text>    </xsl:text>
      <xsl:if test="@readonly='yes'">
        <xsl:text>readonly </xsl:text>
      </xsl:if>
      <xsl:text>attribute </xsl:text>
      <xsl:apply-templates select="@type"/>
      <xsl:text> </xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>;&#x0A;</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:apply-templates select="@if" mode="end"/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>

<!--
 *  methods
-->
<xsl:template match="interface//method | collection//method">
  <xsl:apply-templates select="@if" mode="begin"/>
  <xsl:if test="param/@mod='ptr'">
    <!-- methods using native types must be non-scriptable -->
    <xsl:text>    [noscript]&#x0A;</xsl:text>
  </xsl:if>
  <xsl:text>    void </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:if test="param">
    <xsl:text> (&#x0A;</xsl:text>
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
    <xsl:text>();&#x0A;</xsl:text>
  </xsl:if>
  <xsl:apply-templates select="@if" mode="end"/>
  <xsl:text>&#x0A;</xsl:text>
</xsl:template>


<!--
 *  co-classes
-->
<xsl:template match="module/class">
  <!-- class and contract id -->
  <xsl:text>%{C++&#x0A;</xsl:text>
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
  <xsl:text>// for compatibility with Win32&#x0A;</xsl:text>
  <xsl:text>#define CLSID_</xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> (nsCID) NS_</xsl:text>
  <xsl:call-template name="uppercase">
    <xsl:with-param name="str" select="@name"/>
  </xsl:call-template>
  <xsl:text>_CID&#x0A;</xsl:text>
  <xsl:text>%}&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  enumerators
-->
<xsl:template match="enumerator">[
    uuid(<xsl:value-of select="@uuid"/>),
    scriptable
]
<xsl:text>interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> : nsISupports&#x0A;{&#x0A;</xsl:text>
  <!-- HasMore -->
  <xsl:text>    void hasMore ([retval] out boolean more);&#x0A;&#x0A;</xsl:text>
  <!-- GetNext -->
  <xsl:text>    void getNext ([retval] out </xsl:text>
  <xsl:apply-templates select="@type"/>
  <xsl:text> next);&#x0A;&#x0A;</xsl:text>
  <!-- -->
  <xsl:text>}; /* interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> */&#x0A;&#x0A;</xsl:text>
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
  </xsl:if>[
    uuid(<xsl:value-of select="@uuid"/>),
    scriptable
]
<xsl:text>interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> : nsISupports&#x0A;{&#x0A;</xsl:text>
  <!-- Count -->
  <xsl:text>    readonly attribute unsigned long count;&#x0A;&#x0A;</xsl:text>
  <!-- GetItemAt -->
  <xsl:text>    void getItemAt (in unsigned long index, [retval] out </xsl:text>
  <xsl:apply-templates select="@type"/>
  <xsl:text> item);&#x0A;&#x0A;</xsl:text>
  <!-- Enumerate -->
  <xsl:text>    void enumerate ([retval] out </xsl:text>
  <xsl:apply-templates select="@enumerator"/>
  <xsl:text> enumerator);&#x0A;&#x0A;</xsl:text>
  <!-- other extra attributes (properties) -->
  <xsl:apply-templates select="attribute"/>
  <!-- other extra methods -->
  <xsl:apply-templates select="method"/>
  <!-- 'if' enclosed elements, unsorted -->
  <xsl:apply-templates select="if"/>
  <!-- -->
  <xsl:text>}; /* interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text> */&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  enums
-->
<xsl:template match="enum">[
    uuid(<xsl:value-of select="@uuid"/>),
    scriptable
]
<xsl:text>interface </xsl:text>
  <xsl:value-of select="@name"/>
  <xsl:text>&#x0A;{&#x0A;</xsl:text>
  <xsl:for-each select="const">
    <xsl:text>    const PRUint32 </xsl:text>
    <xsl:value-of select="@name"/> = <xsl:value-of select="@value"/>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:for-each>
  <xsl:text>};&#x0A;&#x0A;</xsl:text>
  <!-- -->
  <xsl:value-of select="concat('/* cross-platform type name for ', @name, ' */&#x0A;')"/>
  <xsl:text>%{C++&#x0A;</xsl:text>
  <xsl:value-of select="concat('#define ', @name, '_T', ' ',
                               'PRUint32&#x0A;')"/>
  <xsl:text>%}&#x0A;&#x0A;</xsl:text>
  <!-- -->
  <xsl:value-of select="concat('/* cross-platform constants for ', @name, ' */&#x0A;')"/>
  <xsl:text>%{C++&#x0A;</xsl:text>
  <xsl:for-each select="const">
    <xsl:value-of select="concat('#define ', ../@name, '_', @name, ' ',
                                 ../@name, '::', @name, '&#x0A;')"/>
  </xsl:for-each>
  <xsl:text>%}&#x0A;&#x0A;</xsl:text>
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
        <xsl:when test="@dir='in'">in </xsl:when>
        <xsl:when test="@dir='out'">out </xsl:when>
        <xsl:when test="@dir='return'">out </xsl:when>
        <xsl:otherwise>in </xsl:otherwise>
      </xsl:choose>
      <xsl:text>unsigned long </xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>Size,&#x0A;</xsl:text>
      <!-- array pointer -->
      <xsl:text>        [array, size_is(</xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>Size)</xsl:text>
      <xsl:choose>
        <xsl:when test="@dir='in'">] in </xsl:when>
        <xsl:when test="@dir='out'">] out </xsl:when>
        <xsl:when test="@dir='return'"> , retval] out </xsl:when>
        <xsl:otherwise>] in </xsl:otherwise>
      </xsl:choose>
      <xsl:apply-templates select="@type"/>
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
        <xsl:when test="@dir='in'">in </xsl:when>
        <xsl:when test="@dir='out'">out </xsl:when>
        <xsl:when test="@dir='return'">[retval] out </xsl:when>
        <xsl:otherwise>in </xsl:otherwise>
      </xsl:choose>
      <xsl:apply-templates select="@type"/>
      <xsl:text> </xsl:text>
      <xsl:value-of select="@name"/>
    </xsl:otherwise>
  </xsl:choose>
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
            <xsl:when test=".='boolean'">booeanPtr</xsl:when>
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
                <xsl:when test="../@dir='in'">
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

</xsl:stylesheet>

