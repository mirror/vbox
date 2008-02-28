<?xml version="1.0"?>

<!--
 *  A template to generate a MS IDL compatible interface definition file
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
 *  MS IDL (MIDL) definition for VirualBox Main API (COM interfaces)
 *  generated from XIDL (XML interface definition).
 *
 *  Source    : src/VBox/Main/idl/VirtualBox.xidl
 *  Generator : src/VBox/Main/idl/midl.xsl
 */
    </xsl:text>
    <xsl:text>&#x0A;</xsl:text>
    <xsl:text>import "unknwn.idl";&#x0A;&#x0A;</xsl:text>
    <xsl:apply-templates/>
</xsl:template>


<!--
 *  ignore all |if|s except those for MIDL target
-->
<xsl:template match="if">
    <xsl:if test="@target='midl'">
        <xsl:apply-templates/>
    </xsl:if>
</xsl:template>
<xsl:template match="if" mode="forward">
    <xsl:if test="@target='midl'">
        <xsl:apply-templates mode="forward"/>
    </xsl:if>
</xsl:template>


<!--
 *  cpp_quote
-->
<xsl:template match="cpp">
    <xsl:text>cpp_quote("</xsl:text>
    <xsl:value-of select="@line"/>
    <xsl:text>")&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  #if statement (@if attribute)
-->
<xsl:template match="@if" mode="begin">
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
<xsl:template match="library">[
    uuid(<xsl:value-of select="@uuid"/>),
    version(<xsl:value-of select="@version"/>),
    helpstring("<xsl:value-of select="@desc"/>")
]
<xsl:text>library </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text>&#x0A;{&#x0A;</xsl:text>
    <xsl:text>&#x0A;importlib("stdole2.tlb");&#x0A;&#x0A;</xsl:text>
    <!-- forward declarations -->
    <xsl:apply-templates select="if | interface | collection | enumerator" mode="forward"/>
    <xsl:text>&#x0A;</xsl:text>
    <!-- all enums go first -->
    <xsl:apply-templates select="enum | if/enum"/>
    <!-- everything else but enums -->
    <xsl:apply-templates select="*[not(self::enum) and not(self::if[enum])]"/>
    <!-- -->
    <xsl:text>}; /* library </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> */&#x0A;&#x0A;</xsl:text>
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
    object,
    dual
]
<xsl:text>interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> : </xsl:text>
    <xsl:choose>
        <xsl:when test="@extends='$unknown'">IUnknown</xsl:when>
        <xsl:when test="@extends='$dispatched'">IDispatch</xsl:when>
        <xsl:when test="@extends='$errorinfo'">IErrorInfo</xsl:when>
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
    <xsl:apply-templates select="@if" mode="begin"/>
    <xsl:if test="@array">
        <xsl:message terminate="yes">
            <xsl:value-of select="concat(../@name,'::',@name,': ')"/>
            <xsl:text>'array' attributes are not supported, use 'safearray="yes"' instead.</xsl:text>
        </xsl:message>
    </xsl:if>
    <!-- getter -->
    <xsl:text>    [propget] HRESULT </xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text> ([out, retval] </xsl:text>
    <xsl:if test="@safearray='yes'">
        <xsl:text>SAFEARRAY(</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="@type"/>
    <xsl:if test="@safearray='yes'">
        <xsl:text>)</xsl:text>
    </xsl:if>
    <xsl:text> * a</xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>);&#x0A;</xsl:text>
    <!-- setter -->
    <xsl:if test="not(@readonly='yes')">
        <xsl:text>    [propput] HRESULT </xsl:text>
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
        <xsl:text> ([in</xsl:text>
        <xsl:if test="@safearray='yes'">
            <!-- VB supports only [in, out], [out] and [out, retval] arrays -->
            <xsl:text>, out</xsl:text>
        </xsl:if>
        <xsl:text>] </xsl:text>
        <xsl:if test="@safearray='yes'">
            <xsl:text>SAFEARRAY(</xsl:text>
        </xsl:if>
        <xsl:apply-templates select="@type"/>
        <xsl:if test="@safearray='yes'">
            <xsl:text>) *</xsl:text>
        </xsl:if>
        <xsl:text> a</xsl:text>
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
        <xsl:text>);&#x0A;</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="@if" mode="end"/>
    <xsl:text>&#x0A;</xsl:text>
</xsl:template>


<!--
 *  methods
-->
<xsl:template match="interface//method | collection//method">
    <xsl:apply-templates select="@if" mode="begin"/>
    <xsl:text>    HRESULT </xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
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
<xsl:template match="module/class">[
    uuid(<xsl:value-of select="@uuid"/>)
]
<xsl:text>coclass </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text>&#x0A;{&#x0A;</xsl:text>
    <xsl:for-each select="interface">
        <xsl:text>    </xsl:text>
        <xsl:if test="@default='yes'">
            <xsl:text>[default] </xsl:text>
        </xsl:if>
        <xsl:text>interface </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>;&#x0A;</xsl:text>
    </xsl:for-each>
    <xsl:text>&#x0A;}; /* coclass </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> */&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  enumerators
-->
<xsl:template match="enumerator">[
    uuid(<xsl:value-of select="@uuid"/>),
    object,
    dual
]
<xsl:text>interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> : IUnknown&#x0A;{&#x0A;</xsl:text>
    <!-- HasMore -->
    <xsl:text>    HRESULT HasMore ([out, retval] BOOL * more);&#x0A;&#x0A;</xsl:text>
    <!-- GetNext -->
    <xsl:text>    HRESULT GetNext ([out, retval] </xsl:text>
    <xsl:apply-templates select="@type"/>
    <xsl:text> * next);&#x0A;&#x0A;</xsl:text>
    <!-- -->
    <xsl:text>&#x0A;}; /* interface </xsl:text>
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
    object,
    dual
]
<xsl:text>interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> : IUnknown&#x0A;{&#x0A;</xsl:text>
    <!-- Count -->
    <xsl:text>    [propget] HRESULT Count ([out, retval] ULONG * count);&#x0A;&#x0A;</xsl:text>
    <!-- GetItemAt -->
    <xsl:text>    HRESULT GetItemAt ([in] ULONG index, [out, retval] </xsl:text>
    <xsl:apply-templates select="@type"/>
    <xsl:text> * item);&#x0A;&#x0A;</xsl:text>
    <!-- Enumerate -->
    <xsl:text>    HRESULT Enumerate ([out, retval] </xsl:text>
    <xsl:apply-templates select="@enumerator"/>
    <xsl:text> * enumerator);&#x0A;&#x0A;</xsl:text>
    <!-- other extra attributes (properties) -->
    <xsl:apply-templates select="attribute"/>
    <!-- other extra methods -->
    <xsl:apply-templates select="method"/>
    <!-- 'if' enclosed elements, unsorted -->
    <xsl:apply-templates select="if"/>
    <!-- -->
    <xsl:text>&#x0A;}; /* interface </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text> */&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  enums
-->
<xsl:template match="enum">[
    uuid(<xsl:value-of select="@uuid"/>),
    v1_enum
]
<xsl:text>typedef enum &#x0A;{&#x0A;</xsl:text>
    <xsl:for-each select="const">
        <xsl:text>    </xsl:text>
        <xsl:value-of select="concat(../@name,'_',@name)"/> = <xsl:value-of select="@value"/>
        <xsl:choose>
            <xsl:when test="position()!=last()"><xsl:text>,&#x0A;</xsl:text></xsl:when>
            <xsl:otherwise><xsl:text>&#x0A;</xsl:text></xsl:otherwise>
        </xsl:choose>
    </xsl:for-each>
    <xsl:text>} </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text>;&#x0A;&#x0A;</xsl:text>
    <!-- -->
    <xsl:value-of select="concat('/* cross-platform type name for ', @name, ' */&#x0A;')"/>
    <xsl:value-of select="concat('cpp_quote(&quot;#define ', @name, '_T', ' ',
                                 @name, '&quot;)&#x0A;&#x0A;')"/>
    <xsl:text>&#x0A;&#x0A;</xsl:text>
</xsl:template>


<!--
 *  method parameters
-->
<xsl:template match="method/param">
    <xsl:text>[</xsl:text>
    <xsl:choose>
        <xsl:when test="@dir='in'">in</xsl:when>
        <xsl:when test="@dir='out'">out</xsl:when>
        <xsl:when test="@dir='return'">out, retval</xsl:when>
        <xsl:otherwise>in</xsl:otherwise>
    </xsl:choose>
    <xsl:if test="@safearray='yes'">
        <!-- VB supports only [in, out], [out] and [out, retval] arrays -->
        <xsl:if test="@dir='in'">, out</xsl:if>
    </xsl:if>
    <xsl:if test="@array">
        <xsl:if test="@dir='return'">
            <xsl:message terminate="yes">
                <xsl:value-of select="concat(../../@name,'::',../@name,'::',@name,': ')"/>
                <xsl:text>return 'array' parameters are not supported, use 'safearray="yes"' instead.</xsl:text>
            </xsl:message>
        </xsl:if>
        <xsl:choose>
            <xsl:when test="../param[@name=current()/@array]">
                <xsl:if test="../param[@name=current()/@array]/@dir != @dir">
                    <xsl:message terminate="yes">
                        <xsl:value-of select="concat(../../@name,'::',../@name,': ')"/>
                        <xsl:value-of select="concat(@name,' and ',../param[@name=current()/@array]/@name)"/>
                        <xsl:text> must have the same direction</xsl:text>
                    </xsl:message>
                </xsl:if>
                <xsl:text>, size_is(</xsl:text>
                    <xsl:if test="@dir='out'">
                        <xsl:text>, *</xsl:text>
                    </xsl:if>
                    <xsl:text>a</xsl:text>
                    <xsl:call-template name="capitalize">
                        <xsl:with-param name="str" select="@array"/>
                    </xsl:call-template>
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
    </xsl:if>
    <xsl:text>] </xsl:text>
    <xsl:if test="@safearray='yes'">
        <xsl:text>SAFEARRAY(</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="@type"/>
    <xsl:if test="@safearray='yes'">
        <xsl:text>)</xsl:text>
    </xsl:if>
    <xsl:if test="@array">
        <xsl:text> *</xsl:text>
    </xsl:if>
    <xsl:if test="@dir='out' or @dir='return' or @safearray='yes'">
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
                        <xsl:when test=".='boolean'">BOOL *</xsl:when>
                        <xsl:when test=".='octet'">BYTE *</xsl:when>
                        <xsl:when test=".='short'">SHORT *</xsl:when>
                        <xsl:when test=".='unsigned short'">USHORT *</xsl:when>
                        <xsl:when test=".='long'">LONG *</xsl:when>
                        <xsl:when test=".='long long'">LONG64 *</xsl:when>
                        <xsl:when test=".='unsigned long'">ULONG *</xsl:when>
                        <xsl:when test=".='unsigned long long'">ULONG64 *</xsl:when>
                        <xsl:when test=".='char'">CHAR *</xsl:when>
                        <!--xsl:when test=".='string'">??</xsl:when-->
                        <xsl:when test=".='wchar'">OLECHAR *</xsl:when>
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
                <xsl:when test=".='result'">HRESULT</xsl:when>
                <xsl:when test=".='boolean'">BOOL</xsl:when>
                <xsl:when test=".='octet'">BYTE</xsl:when>
                <xsl:when test=".='short'">SHORT</xsl:when>
                <xsl:when test=".='unsigned short'">USHORT</xsl:when>
                <xsl:when test=".='long'">LONG</xsl:when>
                <xsl:when test=".='long long'">LONG64</xsl:when>
                <xsl:when test=".='unsigned long'">ULONG</xsl:when>
                <xsl:when test=".='unsigned long long'">ULONG64</xsl:when>
                <xsl:when test=".='char'">CHAR</xsl:when>
                <xsl:when test=".='string'">CHAR *</xsl:when>
                <xsl:when test=".='wchar'">OLECHAR</xsl:when>
                <xsl:when test=".='wstring'">BSTR</xsl:when>
                <!-- UUID type -->
                <xsl:when test=".='uuid'">GUID</xsl:when>
                <!-- system interface types -->
                <xsl:when test=".='$unknown'">IUnknown *</xsl:when>
                <xsl:otherwise>
                    <xsl:choose>
                        <!-- enum types -->
                        <xsl:when test="
                            (ancestor::library/enum[@name=current()]) or
                            (ancestor::library/if[@target=$self_target]/enum[@name=current()])
                        ">
                            <xsl:value-of select="."/>
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
                            <xsl:value-of select="."/><xsl:text> *</xsl:text>
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

