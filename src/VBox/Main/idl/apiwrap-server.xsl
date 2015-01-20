<?xml version="1.0"?>

<!--
    apiwrap-server.xsl:
        XSLT stylesheet that generates C++ API wrappers (server side) from
        VirtualBox.xidl.

    Copyright (C) 2010-2014 Oracle Corporation

    This file is part of VirtualBox Open Source Edition (OSE), as
    available from http://www.virtualbox.org. This file is free software;
    you can redistribute it and/or modify it under the terms of the GNU
    General Public License (GPL) as published by the Free Software
    Foundation, in version 2 as it comes in the "COPYING" file of the
    VirtualBox OSE distribution. VirtualBox OSE is distributed in the
    hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:exsl="http://exslt.org/common"
    extension-element-prefixes="exsl">

<xsl:output method="text"/>

<xsl:strip-space elements="*"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:variable name="G_xsltFilename" select="'apiwrap-server.xsl'"/>

<xsl:variable name="G_root" select="/"/>

<xsl:include href="typemap-shared.inc.xsl"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
templates for file separation
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="interface" mode="listfile">
    <xsl:param name="file"/>

    <xsl:value-of select="concat('&#9;', $file, ' \&#10;')"/>
</xsl:template>

<xsl:template match="interface" mode="startfile">
    <xsl:param name="file"/>

    <xsl:value-of select="concat('&#10;// ##### BEGINFILE &quot;', $file, '&quot;&#10;')"/>
</xsl:template>

<xsl:template match="interface" mode="endfile">
    <xsl:param name="file"/>

    <xsl:value-of select="concat('&#10;// ##### ENDFILE &quot;', $file, '&quot;&#10;')"/>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
templates for file headers/footers
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template name="fileheader">
    <xsl:param name="class"/>
    <xsl:param name="name"/>
    <xsl:param name="type"/>

    <xsl:text>/** @file
 *
</xsl:text>
    <xsl:value-of select="concat(' * VirtualBox API class wrapper ', $type, ' for I', $class, '.')"/>
    <xsl:text>
 *
 * DO NOT EDIT! This is a generated file.
 * Generated from: src/VBox/Main/idl/VirtualBox.xidl
 * Generator: src/VBox/Main/idl/apiwrap-server.xsl
 */

/**
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

</xsl:text>
</xsl:template>

<xsl:template name="emitCOMInterfaces">
    <xsl:param name="iface"/>

    <xsl:value-of select="concat('        COM_INTERFACE_ENTRY(', $iface/@name, ')&#10;')"/>
    <!-- now recurse to emit all base interfaces -->
    <xsl:variable name="extends" select="$iface/@extends"/>
    <xsl:if test="$extends and not($extends='$unknown') and not($extends='$errorinfo')">
        <xsl:call-template name="emitCOMInterfaces">
            <xsl:with-param name="iface" select="//interface[@name=$extends]"/>
        </xsl:call-template>
    </xsl:if>
</xsl:template>

<xsl:template match="interface" mode="classheader">
    <xsl:param name="addinterfaces"/>
    <xsl:value-of select="concat('#ifndef ', substring(@name, 2), 'Wrap_H_&#10;')"/>
    <xsl:value-of select="concat('#define ', substring(@name, 2), 'Wrap_H_')"/>
    <xsl:text>

#include "VirtualBoxBase.h"
#include "Wrapper.h"

</xsl:text>
    <xsl:value-of select="concat('class ATL_NO_VTABLE ', substring(@name, 2), 'Wrap:')"/>
    <xsl:text>
    public VirtualBoxBase,
</xsl:text>
    <xsl:value-of select="concat('    VBOX_SCRIPTABLE_IMPL(', @name, ')')"/>
    <xsl:if test="count(exsl:node-set($addinterfaces)/token) > 0">
        <xsl:text>,</xsl:text>
    </xsl:if>
    <xsl:text>&#10;</xsl:text>
    <xsl:for-each select="exsl:node-set($addinterfaces)/token">
        <xsl:value-of select="concat('    VBOX_SCRIPTABLE_IMPL(', text(), ')')"/>
        <xsl:if test="not(position()=last())">
            <xsl:text>,</xsl:text>
        </xsl:if>
        <xsl:text>&#10;</xsl:text>
    </xsl:for-each>
    <xsl:text>{
    Q_OBJECT

public:
</xsl:text>
    <xsl:value-of select="concat('    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(', substring(@name, 2), 'Wrap, ', @name, ')&#10;')"/>
    <xsl:value-of select="concat('    DECLARE_NOT_AGGREGATABLE(', substring(@name, 2), 'Wrap)&#10;')"/>
    <xsl:text>    DECLARE_PROTECT_FINAL_CONSTRUCT()

</xsl:text>
    <xsl:value-of select="concat('    BEGIN_COM_MAP(', substring(@name, 2), 'Wrap)&#10;')"/>
    <xsl:text>        COM_INTERFACE_ENTRY(ISupportErrorInfo)
</xsl:text>
    <xsl:call-template name="emitCOMInterfaces">
        <xsl:with-param name="iface" select="."/>
    </xsl:call-template>
    <xsl:value-of select="concat('        COM_INTERFACE_ENTRY2(IDispatch, ', @name, ')&#10;')"/>
    <xsl:variable name="manualAddInterfaces">
        <xsl:call-template name="checkoption">
            <xsl:with-param name="optionlist" select="@wrap-hint-server"/>
            <xsl:with-param name="option" select="'manualaddinterfaces'"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:if test="not($manualAddInterfaces = 'true')">
        <xsl:for-each select="exsl:node-set($addinterfaces)/token">
            <!-- This is super tricky, as the for-each switches to the node
                 set, which means the normal document isn't available any more.
                 So need to use the global root node to get back into the
                 documemt to find corresponding interface data. -->
            <xsl:variable name="addifname">
                <xsl:value-of select="string(.)"/>
            </xsl:variable>
            <xsl:call-template name="emitCOMInterfaces">
                <xsl:with-param name="iface" select="$G_root//interface[@name=$addifname]"/>
            </xsl:call-template>
        </xsl:for-each>
    </xsl:if>
    <xsl:text>        COM_INTERFACE_ENTRY_AGGREGATE(IID_IMarshal, m_pUnkMarshaler.p)
    END_COM_MAP()

</xsl:text>
    <xsl:value-of select="concat('    DECLARE_EMPTY_CTOR_DTOR(', substring(@name, 2), 'Wrap)&#10;')"/>
</xsl:template>

<xsl:template match="interface" mode="classfooter">
    <xsl:param name="addinterfaces"/>
    <xsl:text>};

</xsl:text>
    <xsl:value-of select="concat('#endif // !', substring(@name, 2), 'Wrap_H_&#10;')"/>
</xsl:template>

<xsl:template match="interface" mode="codeheader">
    <xsl:param name="addinterfaces"/>
    <xsl:value-of select="concat('#define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_MAIN_', translate(substring(@name, 2), $G_lowerCase, $G_upperCase), '&#10;&#10;')"/>
    <xsl:value-of select="concat('#include &quot;', substring(@name, 2), 'Wrap.h&quot;&#10;')"/>
    <xsl:text>#include "Logging.h"
#ifdef VBOX_WITH_DTRACE_R3_MAIN
# include "dtrace/VBoxAPI.h"
#endif

</xsl:text>
</xsl:template>

<xsl:template name="emitISupports">
    <xsl:param name="classname"/>
    <xsl:param name="extends"/>
    <xsl:param name="addinterfaces"/>
    <xsl:param name="depth"/>
    <xsl:param name="interfacelist"/>

    <xsl:choose>
        <xsl:when test="$extends and not($extends='$unknown') and not($extends='$errorinfo')">
            <xsl:variable name="newextends" select="//interface[@name=$extends]/@extends"/>
            <xsl:variable name="newiflist" select="concat($interfacelist, ', ', $extends)"/>
            <xsl:call-template name="emitISupports">
                <xsl:with-param name="classname" select="$classname"/>
                <xsl:with-param name="extends" select="$newextends"/>
                <xsl:with-param name="addinterfaces" select="$addinterfaces"/>
                <xsl:with-param name="depth" select="$depth + 1"/>
                <xsl:with-param name="interfacelist" select="$newiflist"/>
            </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
            <xsl:variable name="addinterfaces_ns" select="exsl:node-set($addinterfaces)"/>
            <xsl:choose>
                <xsl:when test="count($addinterfaces_ns/token) > 0">
                    <xsl:variable name="addifname" select="$addinterfaces_ns/token[1]"/>
                    <xsl:variable name="addif" select="//interface[@name=$addifname]/@extends"/>
                    <xsl:variable name="newextends" select="$addif/@extends"/>
                    <xsl:variable name="newaddinterfaces" select="$addinterfaces_ns/token[position() > 1]"/>
                    <xsl:variable name="newiflist" select="concat($interfacelist, ', ', $addifname)"/>
                    <xsl:call-template name="emitISupports">
                        <xsl:with-param name="classname" select="$classname"/>
                        <xsl:with-param name="extends" select="$newextends"/>
                        <xsl:with-param name="addinterfaces" select="$newaddinterfaces"/>
                        <xsl:with-param name="depth" select="$depth + 1"/>
                        <xsl:with-param name="interfacelist" select="$newiflist"/>
                    </xsl:call-template>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:value-of select="concat('NS_IMPL_THREADSAFE_ISUPPORTS', $depth, '_CI(', $classname, ', ', $interfacelist, ')&#10;')"/>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<xsl:template match="interface" mode="codefooter">
    <xsl:param name="addinterfaces"/>
    <xsl:text>#ifdef VBOX_WITH_XPCOM
</xsl:text>
    <xsl:value-of select="concat('NS_DECL_CLASSINFO(', substring(@name, 2), 'Wrap)&#10;')"/>

    <xsl:variable name="manualAddInterfaces">
        <xsl:call-template name="checkoption">
            <xsl:with-param name="optionlist" select="@wrap-hint-server"/>
            <xsl:with-param name="option" select="'manualaddinterfaces'"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:choose>
        <xsl:when test="$manualAddInterfaces = 'true'">
            <xsl:variable name="nulladdinterfaces"></xsl:variable>
            <xsl:call-template name="emitISupports">
                <xsl:with-param name="classname" select="concat(substring(@name, 2), 'Wrap')"/>
                <xsl:with-param name="extends" select="@extends"/>
                <xsl:with-param name="addinterfaces" select="$nulladdinterfaces"/>
                <xsl:with-param name="depth" select="1"/>
                <xsl:with-param name="interfacelist" select="@name"/>
            </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
            <xsl:call-template name="emitISupports">
                <xsl:with-param name="classname" select="concat(substring(@name, 2), 'Wrap')"/>
                <xsl:with-param name="extends" select="@extends"/>
                <xsl:with-param name="addinterfaces" select="$addinterfaces"/>
                <xsl:with-param name="depth" select="1"/>
                <xsl:with-param name="interfacelist" select="@name"/>
            </xsl:call-template>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:text>#endif // VBOX_WITH_XPCOM
</xsl:text>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  templates for dealing with names and parameters
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template name="tospace">
    <xsl:param name="str"/>
    <xsl:value-of select="translate($str, 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_', '                                                               ')"/>
</xsl:template>

<xsl:template name="checkoption">
    <xsl:param name="optionlist"/>
    <xsl:param name="option"/>
    <xsl:value-of select="string-length($option) > 0 and contains(concat(',', translate($optionlist, ' ', ''), ','), concat(',', $option, ','))"/>
</xsl:template>

<xsl:template name="getattrlist">
    <xsl:param name="val"/>
    <xsl:param name="separator" select="','"/>

    <xsl:if test="$val and $val != ''">
        <xsl:choose>
            <xsl:when test="contains($val,$separator)">
                <token>
                    <xsl:value-of select="substring-before($val,$separator)"/>
                </token>
                <xsl:call-template name="getattrlist">
                    <xsl:with-param name="val" select="substring-after($val,$separator)"/>
                    <xsl:with-param name="separator" select="$separator"/>
                </xsl:call-template>
            </xsl:when>
            <xsl:otherwise>
                <token><xsl:value-of select="$val"/></token>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:if>
</xsl:template>

<xsl:template name="translatepublictype">
    <xsl:param name="type"/>
    <xsl:param name="dir"/>
    <xsl:param name="mod"/>

    <!-- get C++ glue type from IDL type from table in typemap-shared.inc.xsl -->
    <xsl:variable name="gluetypefield" select="exsl:node-set($G_aSharedTypes)/type[@idlname=$type]/@gluename"/>
    <xsl:choose>
        <xsl:when test="$type='wstring' or $type='uuid'">
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>IN_BSTR</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>BSTR</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>
        <xsl:when test="string-length($gluetypefield)">
            <xsl:value-of select="$gluetypefield"/>
        </xsl:when>
        <xsl:when test="//enum[@name=$type]">
            <xsl:value-of select="concat($type, '_T')"/>
        </xsl:when>
        <xsl:when test="$type='$unknown'">
            <xsl:text>IUnknown *</xsl:text>
        </xsl:when>
        <xsl:when test="//interface[@name=$type]">
            <xsl:variable name="thatif" select="//interface[@name=$type]"/>
            <xsl:variable name="thatifname" select="$thatif/@name"/>
            <xsl:value-of select="concat($thatifname, ' *')"/>
        </xsl:when>
        <xsl:otherwise>
            <xsl:call-template name="fatalError">
                <xsl:with-param name="msg" select="concat('translatepublictype: Type &quot;', $type, '&quot; is not supported.')"/>
            </xsl:call-template>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="$mod='ptr'">
        <xsl:text> *</xsl:text>
    </xsl:if>
</xsl:template>

<xsl:template name="translatewrappedtype">
    <xsl:param name="type"/>
    <xsl:param name="dir"/>
    <xsl:param name="mod"/>
    <xsl:param name="safearray"/>

    <!-- get C++ wrap type from IDL type from table in typemap-shared.inc.xsl -->
    <xsl:variable name="wraptypefield" select="exsl:node-set($G_aSharedTypes)/type[@idlname=$type]/@gluename"/>
    <xsl:choose>
        <xsl:when test="$type='wstring'">
            <xsl:if test="$dir='in' and not($safearray='yes')">
                <xsl:text>const </xsl:text>
            </xsl:if>
            <xsl:text>com::Utf8Str &amp;</xsl:text>
        </xsl:when>
        <xsl:when test="$type='uuid'">
            <xsl:if test="$dir='in'">
                <xsl:text>const </xsl:text>
            </xsl:if>
            <xsl:text>com::Guid &amp;</xsl:text>
        </xsl:when>
        <xsl:when test="string-length($wraptypefield)">
            <xsl:value-of select="$wraptypefield"/>
        </xsl:when>
        <xsl:when test="//enum[@name=$type]">
            <xsl:value-of select="concat($type, '_T')"/>
        </xsl:when>
        <xsl:when test="$type='$unknown'">
            <xsl:if test="$dir='in' and not($safearray='yes')">
                <xsl:text>const </xsl:text>
            </xsl:if>
            <xsl:text>ComPtr&lt;IUnknown&gt; &amp;</xsl:text>
        </xsl:when>
        <xsl:when test="//interface[@name=$type]">
            <xsl:variable name="thatif" select="//interface[@name=$type]"/>
            <xsl:variable name="thatifname" select="$thatif/@name"/>
            <xsl:if test="$dir='in' and not($safearray='yes')">
                <xsl:text>const </xsl:text>
            </xsl:if>
            <xsl:value-of select="concat('ComPtr&lt;', $thatifname, '&gt; &amp;')"/>
        </xsl:when>
        <xsl:otherwise>
            <xsl:call-template name="fatalError">
                <xsl:with-param name="msg" select="concat('translatewrappedtype: Type &quot;', $type, '&quot; is not supported.')"/>
            </xsl:call-template>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="$mod='ptr'">
        <xsl:text> *</xsl:text>
    </xsl:if>
</xsl:template>

<xsl:template name="translatefmtspectype">
    <xsl:param name="type"/>
    <xsl:param name="dir"/>
    <xsl:param name="mod"/>
    <xsl:param name="safearray"/>
    <xsl:param name="isref"/>

    <!-- get C format string for IDL type from table in typemap-shared.inc.xsl -->
    <xsl:variable name="wrapfmt" select="exsl:node-set($G_aSharedTypes)/type[@idlname=$type]/@gluefmt"/>
    <xsl:choose>
        <xsl:when test="$mod='ptr' or ($isref='yes' and $dir!='in')">
            <xsl:text>%p</xsl:text>
        </xsl:when>
        <xsl:when test="$safearray='yes'">
            <xsl:text>%zu</xsl:text>
        </xsl:when>
        <xsl:when test="string-length($wrapfmt)">
            <xsl:value-of select="$wrapfmt"/>
        </xsl:when>
        <xsl:when test="//enum[@name=$type]">
            <xsl:text>%RU32</xsl:text>
        </xsl:when>
        <xsl:when test="$type='$unknown'">
            <xsl:text>%p</xsl:text>
        </xsl:when>
        <xsl:when test="//interface[@name=$type]">
            <xsl:text>%p</xsl:text>
        </xsl:when>
        <xsl:otherwise>
            <xsl:call-template name="fatalError">
                <xsl:with-param name="msg" select="concat('translatefmtcpectype: Type &quot;', $type, '&quot; is not supported.')"/>
            </xsl:call-template>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<xsl:template name="translatedtracetype">
    <xsl:param name="type"/>
    <xsl:param name="dir"/>
    <xsl:param name="mod"/>

    <!-- get dtrace probe type from IDL type from table in typemap-shared.inc.xsl -->
    <xsl:variable name="dtracetypefield" select="exsl:node-set($G_aSharedTypes)/type[@idlname=$type]/@dtracename"/>
    <xsl:choose>
        <xsl:when test="string-length($dtracetypefield)">
            <xsl:value-of select="$dtracetypefield"/>
        </xsl:when>
        <xsl:when test="//enum[@name=$type]">
            <!-- <xsl:value-of select="concat($type, '_T')"/> - later we can emit enums into dtrace the library -->
            <xsl:text>int</xsl:text>
        </xsl:when>
        <xsl:when test="$type='$unknown'">
            <!-- <xsl:text>struct IUnknown *</xsl:text> -->
            <xsl:text>void *</xsl:text>
        </xsl:when>
        <xsl:when test="//interface[@name=$type]">
            <!--
            <xsl:variable name="thatif" select="//interface[@name=$type]"/>
            <xsl:variable name="thatifname" select="$thatif/@name"/>
            <xsl:value-of select="concat('struct ', $thatifname, ' *')"/>
            -->
            <xsl:text>void *</xsl:text>
        </xsl:when>
        <xsl:otherwise>
            <xsl:call-template name="fatalError">
                <xsl:with-param name="msg" select="concat('translatedtracetype Type &quot;', $type, '&quot; is not supported.')"/>
            </xsl:call-template>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="$mod='ptr'">
        <xsl:text> *</xsl:text>
    </xsl:if>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  templates for handling entire interfaces and their contents
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template name="emitInterface">
    <xsl:param name="iface"/>

    <xsl:variable name="addinterfaces">
        <xsl:call-template name="getattrlist">
            <xsl:with-param name="val" select="$iface/@wrap-hint-server-addinterfaces"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:call-template name="emitHeader">
        <xsl:with-param name="iface" select="$iface"/>
        <xsl:with-param name="addinterfaces" select="$addinterfaces"/>
    </xsl:call-template>

    <xsl:call-template name="emitCode">
        <xsl:with-param name="iface" select="$iface"/>
        <xsl:with-param name="addinterfaces" select="$addinterfaces"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="attribute/@type | param/@type" mode="public">
    <xsl:param name="dir"/>

    <xsl:variable name="gluetype">
        <xsl:call-template name="translatepublictype">
            <xsl:with-param name="type" select="."/>
            <xsl:with-param name="dir" select="$dir"/>
            <xsl:with-param name="mod" select="../@mod"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:if test="../@safearray='yes'">
        <xsl:choose>
            <xsl:when test="$dir='in'">
                <xsl:text>ComSafeArrayIn(</xsl:text>
            </xsl:when>
            <xsl:otherwise>
                <xsl:text>ComSafeArrayOut(</xsl:text>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:if>
    <xsl:value-of select="$gluetype"/>
    <xsl:choose>
        <xsl:when test="../@safearray='yes'">
            <xsl:text>, </xsl:text>
        </xsl:when>
        <xsl:otherwise>
            <xsl:if test="substring($gluetype,string-length($gluetype))!='*'">
                <xsl:text> </xsl:text>
            </xsl:if>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                </xsl:when>
                <xsl:otherwise>
                    <xsl:value-of select="'*'"/>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:text>a</xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="../@name"/>
    </xsl:call-template>
    <xsl:if test="../@safearray='yes'">
        <xsl:value-of select="')'"/>
    </xsl:if>
</xsl:template>

<xsl:template match="attribute/@type | param/@type" mode="wrapped">
    <xsl:param name="dir"/>

    <xsl:variable name="wraptype">
        <xsl:call-template name="translatewrappedtype">
            <xsl:with-param name="type" select="."/>
            <xsl:with-param name="dir" select="$dir"/>
            <xsl:with-param name="mod" select="../@mod"/>
            <xsl:with-param name="safearray" select="../@safearray"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:choose>
        <xsl:when test="../@safearray='yes'">
            <xsl:if test="$dir='in'">
                <xsl:text>const </xsl:text>
            </xsl:if>
            <xsl:text>std::vector&lt;</xsl:text>
            <xsl:choose>
                <xsl:when test="substring($wraptype,string-length($wraptype))='&amp;'">
                    <xsl:variable name="wraptype2">
                        <xsl:value-of select="substring($wraptype,1,string-length($wraptype)-2)"/>
                    </xsl:variable>

                    <xsl:choose>
                        <xsl:when test="substring($wraptype2,string-length($wraptype2))='&gt;'">
                            <xsl:value-of select="concat($wraptype2, ' ')"/>
                        </xsl:when>
                        <xsl:otherwise>
                            <xsl:value-of select="$wraptype2"/>
                        </xsl:otherwise>
                    </xsl:choose>
                </xsl:when>
                <xsl:when test="substring($wraptype,string-length($wraptype))='&gt;'">
                    <xsl:value-of select="concat($wraptype, ' ')"/>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:value-of select="$wraptype"/>
                </xsl:otherwise>
            </xsl:choose>
            <xsl:text>&gt; &amp;</xsl:text>
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="$wraptype"/>
            <xsl:if test="substring($wraptype,string-length($wraptype))!='&amp;'">
                <xsl:if test="substring($wraptype,string-length($wraptype))!='*'">
                    <xsl:text> </xsl:text>
                </xsl:if>
                <xsl:choose>
                    <xsl:when test="$dir='in'">
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:value-of select="'*'"/>
                    </xsl:otherwise>
                </xsl:choose>
            </xsl:if>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:text>a</xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="../@name"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="attribute/@type | param/@type" mode="logparamtext">
    <xsl:param name="dir"/>
    <xsl:param name="isref"/>

    <xsl:if test="$isref!='yes' and ($dir='out' or $dir='ret')">
        <xsl:text>*</xsl:text>
    </xsl:if>
    <xsl:text>a</xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="../@name"/>
    </xsl:call-template>
    <xsl:text>=</xsl:text>
    <xsl:call-template name="translatefmtspectype">
        <xsl:with-param name="type" select="."/>
        <xsl:with-param name="dir" select="$dir"/>
        <xsl:with-param name="mod" select="../@mod"/>
        <xsl:with-param name="safearray" select="../@safearray"/>
        <xsl:with-param name="isref" select="$isref"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="attribute/@type | param/@type" mode="logparamval">
    <xsl:param name="dir"/>
    <xsl:param name="isref"/>

    <xsl:choose>
        <xsl:when test="../@safearray='yes' and $isref!='yes'">
            <xsl:text>ComSafeArraySize(</xsl:text>
            <xsl:if test="$isref!='yes' and $dir!='in'">
                <xsl:text>*</xsl:text>
            </xsl:if>
        </xsl:when>
        <xsl:when test="$isref!='yes' and $dir!='in'">
            <xsl:text>*</xsl:text>
        </xsl:when>
    </xsl:choose>
    <xsl:text>a</xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="../@name"/>
    </xsl:call-template>
    <xsl:choose>
        <xsl:when test="../@safearray='yes' and $isref!='yes'">
            <xsl:text>)</xsl:text>
        </xsl:when>
    </xsl:choose>
</xsl:template>

<xsl:template match="attribute/@type | param/@type" mode="dtraceparamval">
    <xsl:param name="dir"/>

    <xsl:variable name="viatmpvar">
        <xsl:call-template name="paramconversionviatmp">
            <xsl:with-param name="dir" select="$dir"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:variable name="type" select="."/>
    <xsl:choose>
        <xsl:when test="$viatmpvar = 'yes'">
            <xsl:variable name="tmpname">
                <xsl:text>Tmp</xsl:text>
                <xsl:call-template name="capitalize">
                    <xsl:with-param name="str" select="../@name"/>
                </xsl:call-template>
            </xsl:variable>

            <xsl:choose>
                <xsl:when test="../@safearray = 'yes'">
                    <xsl:text>(uint32_t)</xsl:text>
                    <xsl:value-of select="$tmpname"/>
                    <xsl:text>.array().size(), </xsl:text>
                    <!-- Later:
                    <xsl:value-of select="concat($tmpname, '.array().data(), ')"/>
                    -->
                    <xsl:text>NULL /*for now*/</xsl:text>
                </xsl:when>
                <xsl:when test="$type = 'wstring'">
                    <xsl:value-of select="$tmpname"/>
                    <xsl:text>.str().c_str()</xsl:text>
                </xsl:when>
                <xsl:when test="$type = 'uuid'">
                    <xsl:value-of select="$tmpname"/>
                    <xsl:text>.uuid().toStringCurly().c_str()</xsl:text>
                </xsl:when>
                <xsl:when test="$type = '$unknown'">
                    <xsl:text>(void *)</xsl:text>
                    <xsl:value-of select="$tmpname"/>
                    <xsl:text>.ptr()</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:variable name="thatif" select="//interface[@name=$type]"/>
                    <xsl:choose>
                        <xsl:when test="$thatif">
                            <xsl:text>(void *)</xsl:text>
                            <xsl:value-of select="$tmpname"/>
                            <xsl:text>.ptr()</xsl:text>
                        </xsl:when>
                        <xsl:otherwise>
                            <xsl:value-of select="$tmpname"/>
                        </xsl:otherwise>
                    </xsl:choose>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>

        <xsl:otherwise>
            <xsl:if test="$dir != 'in'">
                <xsl:text>*</xsl:text>
            </xsl:if>
            <xsl:text>a</xsl:text>
            <xsl:call-template name="capitalize">
                <xsl:with-param name="str" select="../@name"/>
            </xsl:call-template>

            <xsl:if test="$type = 'boolean'">
                <xsl:text> != FALSE</xsl:text>
            </xsl:if>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<!-- Same as dtraceparamval except no temporary variables are used (they are out of scope). -->
<xsl:template match="attribute/@type | param/@type" mode="dtraceparamvalnotmp">
    <xsl:param name="dir"/>

    <xsl:variable name="viatmpvar">
        <xsl:call-template name="paramconversionviatmp">
            <xsl:with-param name="dir" select="$dir"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:variable name="type" select="."/>
    <xsl:choose>
        <xsl:when test="$viatmpvar = 'yes'">
            <xsl:if test="../@safearray = 'yes'">
                <xsl:text>0, </xsl:text>
            </xsl:if>
            <xsl:text>0</xsl:text>
        </xsl:when>

        <xsl:otherwise>
            <xsl:if test="$dir != 'in'">
                <xsl:text>*</xsl:text>
            </xsl:if>
            <xsl:text>a</xsl:text>
            <xsl:call-template name="capitalize">
                <xsl:with-param name="str" select="../@name"/>
            </xsl:call-template>

            <xsl:if test="$type = 'boolean'">
                <xsl:text> != FALSE</xsl:text>
            </xsl:if>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<xsl:template match="attribute/@type | param/@type" mode="dtraceparamdecl">
    <xsl:param name="dir"/>

    <xsl:variable name="gluetype">
        <xsl:call-template name="translatedtracetype">
            <xsl:with-param name="type" select="."/>
            <xsl:with-param name="dir" select="$dir"/>
            <xsl:with-param name="mod" select="../@mod"/>
        </xsl:call-template>
    </xsl:variable>

    <!-- Safe arrays get an extra size parameter. -->
    <xsl:if test="../@safearray='yes'">
        <xsl:text>uint32_t a_c</xsl:text>
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="../@name"/>
        </xsl:call-template>
        <xsl:text>, </xsl:text>
    </xsl:if>

    <xsl:value-of select="$gluetype"/>
    <xsl:choose>
        <xsl:when test="../@safearray='yes'">
            <xsl:text> *a_pa</xsl:text>
        </xsl:when>
        <xsl:otherwise>
            <xsl:if test="substring($gluetype,string-length($gluetype))!='*'">
                <xsl:text> </xsl:text>
            </xsl:if>
            <xsl:text>a_</xsl:text>
        </xsl:otherwise>
    </xsl:choose>

    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="../@name"/>
    </xsl:call-template>
</xsl:template>

<!-- Call this to determine whether a temporary conversion variable is used for the current parameter.
Returns empty if not needed, non-empty ('yes') if needed. -->
<xsl:template name="paramconversionviatmp">
    <xsl:param name="dir"/>
    <xsl:variable name="type" select="."/>
    <xsl:variable name="thatif" select="//interface[@name=$type]"/>
    <xsl:if test="$thatif or $type = 'wstring' or $type = '$unknown' or $type = 'uuid' or ../@safearray = 'yes'">
        <xsl:text>yes</xsl:text>
    </xsl:if>
</xsl:template>

<!-- Call this to get the argument conversion class, if any is needed. -->
<xsl:template name="paramconversionclass">
    <xsl:param name="dir"/>

    <xsl:variable name="gluetype">
        <xsl:call-template name="translatepublictype">
            <xsl:with-param name="type" select="."/>
            <xsl:with-param name="dir" select="$dir"/>
            <xsl:with-param name="mod" select="../@mod"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="type" select="."/>
    <xsl:variable name="thatif" select="//interface[@name=$type]"/>

    <xsl:choose>
        <xsl:when test="$type='$unknown'">
            <xsl:if test="../@safearray='yes'">
                <xsl:text>Array</xsl:text>
            </xsl:if>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>ComTypeInConverter&lt;IUnknown&gt;</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>ComTypeOutConverter&lt;IUnknown&gt;</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>

        <xsl:when test="$thatif">
            <xsl:if test="../@safearray='yes'">
                <xsl:text>Array</xsl:text>
            </xsl:if>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>ComTypeInConverter</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>ComTypeOutConverter</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
            <xsl:variable name="thatifname" select="$thatif/@name"/>
            <xsl:value-of select="concat('&lt;', $thatifname, '&gt;')"/>
        </xsl:when>

        <xsl:when test="$type='wstring'">
            <xsl:if test="../@safearray='yes'">
                <xsl:text>Array</xsl:text>
            </xsl:if>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>BSTRInConverter</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>BSTROutConverter</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>

        <xsl:when test="$type='uuid'">
            <xsl:if test="../@safearray='yes'">
                <xsl:text>Array</xsl:text>
            </xsl:if>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>UuidInConverter</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>UuidOutConverter</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>

        <xsl:when test="../@safearray='yes'">
            <xsl:text>Array</xsl:text>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>InConverter</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>OutConverter</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
            <xsl:value-of select="concat('&lt;', $gluetype, '&gt;')"/>
        </xsl:when>
    </xsl:choose>
</xsl:template>

<!-- Emits code for converting the parameter to a temporary variable. -->
<xsl:template match="attribute/@type | param/@type" mode="paramvalconversion2tmpvar">
    <xsl:param name="dir"/>

    <xsl:variable name="conversionclass">
        <xsl:call-template name="paramconversionclass">
            <xsl:with-param name="dir" select="$dir"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:if test="$conversionclass != ''">
        <xsl:value-of select="$conversionclass"/>
        <xsl:text> Tmp</xsl:text>
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="../@name"/>
        </xsl:call-template>
        <xsl:text>(</xsl:text>
        <xsl:if test="../@safearray = 'yes'">
            <xsl:choose>
                <xsl:when test="$dir = 'in'">
                    <xsl:text>ComSafeArrayInArg(</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>ComSafeArrayOutArg(</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:if>
        <xsl:text>a</xsl:text>
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="../@name"/>
        </xsl:call-template>
        <xsl:if test="../@safearray = 'yes'">
            <xsl:text>)</xsl:text>
        </xsl:if>
        <xsl:text>);</xsl:text>
    </xsl:if>

</xsl:template>

<!-- Partner to paramvalconversion2tmpvar that emits the parameter when calling call the internal worker method. -->
<xsl:template match="attribute/@type | param/@type" mode="paramvalconversionusingtmp">
    <xsl:param name="dir"/>

    <xsl:variable name="viatmpvar">
        <xsl:call-template name="paramconversionviatmp">
            <xsl:with-param name="dir" select="$dir"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="type" select="."/>

    <xsl:choose>
        <xsl:when test="$viatmpvar = 'yes'">
            <xsl:text>Tmp</xsl:text>
            <xsl:call-template name="capitalize">
                <xsl:with-param name="str" select="../@name"/>
            </xsl:call-template>

            <xsl:choose>
                <xsl:when test="../@safearray='yes'">
                    <xsl:text>.array()</xsl:text>
                </xsl:when>
                <xsl:when test="$type = 'wstring'">
                    <xsl:text>.str()</xsl:text>
                </xsl:when>
                <xsl:when test="$type = 'uuid'">
                    <xsl:text>.uuid()</xsl:text>
                </xsl:when>
                <xsl:when test="$type = '$unknown'">
                    <xsl:text>.ptr()</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:variable name="thatif" select="//interface[@name=$type]"/>
                    <xsl:if test="$thatif">
                        <xsl:text>.ptr()</xsl:text>
                    </xsl:if>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>

        <xsl:otherwise>
            <xsl:text>a</xsl:text>
            <xsl:call-template name="capitalize">
                <xsl:with-param name="str" select="../@name"/>
            </xsl:call-template>

            <!-- Make sure BOOL values we pass down are either TRUE or FALSE. -->
            <xsl:if test="$type = 'boolean' and $dir = 'in'">
                <xsl:text> != FALSE</xsl:text>
            </xsl:if>
        </xsl:otherwise>
    </xsl:choose>

</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit attribute
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="attribute" mode="public">
    <xsl:param name="target"/>

    <xsl:call-template name="emitTargetBegin">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>

    <xsl:variable name="attrbasename">
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:value-of select="concat('    STDMETHOD(COMGETTER(', $attrbasename, '))(')"/>
    <xsl:apply-templates select="@type" mode="public">
        <xsl:with-param name="dir" select="'out'"/>
    </xsl:apply-templates>
    <xsl:text>);
</xsl:text>

    <xsl:if test="not(@readonly) or @readonly!='yes'">
        <xsl:value-of select="concat('    STDMETHOD(COMSETTER(', $attrbasename, '))(')"/>
        <xsl:apply-templates select="@type" mode="public">
            <xsl:with-param name="dir" select="'in'"/>
        </xsl:apply-templates>
        <xsl:text>);
</xsl:text>
    </xsl:if>

    <xsl:call-template name="emitTargetEnd">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="attribute" mode="wrapped">
    <xsl:param name="target"/>

    <xsl:call-template name="emitTargetBegin">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>

    <xsl:variable name="attrbasename">
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:value-of select="concat('    virtual HRESULT get', $attrbasename, '(')"/>
    <xsl:variable name="passAutoCaller">
        <xsl:call-template name="checkoption">
            <xsl:with-param name="optionlist" select="@wrap-hint-server"/>
            <xsl:with-param name="option" select="'passcaller'"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:if test="$passAutoCaller = 'true'">
        <xsl:text>AutoCaller &amp;aAutoCaller, </xsl:text>
    </xsl:if>
    <xsl:apply-templates select="@type" mode="wrapped">
        <xsl:with-param name="dir" select="'out'"/>
    </xsl:apply-templates>
    <xsl:text>) = 0;
</xsl:text>

    <xsl:if test="not(@readonly) or @readonly!='yes'">
        <xsl:value-of select="concat('    virtual HRESULT set', $attrbasename, '(')"/>
        <xsl:if test="$passAutoCaller = 'true'">
            <xsl:text>AutoCaller &amp;aAutoCaller, </xsl:text>
        </xsl:if>
        <xsl:apply-templates select="@type" mode="wrapped">
            <xsl:with-param name="dir" select="'in'"/>
        </xsl:apply-templates>
        <xsl:text>) = 0;
</xsl:text>
    </xsl:if>

    <xsl:call-template name="emitTargetEnd">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="attribute" mode="code">
    <xsl:param name="topclass"/>
    <xsl:param name="dtracetopclass"/>
    <xsl:param name="target"/>

    <xsl:call-template name="emitTargetBegin">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>

    <xsl:variable name="attrbasename">
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="limitedAutoCaller">
        <xsl:call-template name="checkoption">
            <xsl:with-param name="optionlist" select="@wrap-hint-server"/>
            <xsl:with-param name="option" select="'limitedcaller'"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:variable name="dtraceattrname">
        <xsl:choose>
            <xsl:when test="@dtracename">
                <xsl:value-of select="@dtracename"/>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="$attrbasename"/>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:variable>

    <xsl:value-of select="concat('STDMETHODIMP ', $topclass, 'Wrap::COMGETTER(', $attrbasename, ')(')"/>
    <xsl:apply-templates select="@type" mode="public">
        <xsl:with-param name="dir" select="'out'"/>
    </xsl:apply-templates>
    <xsl:text>)
{
    LogRelFlow(("{%p} %s: enter </xsl:text>
    <xsl:apply-templates select="@type" mode="logparamtext">
        <xsl:with-param name="dir" select="'out'"/>
        <xsl:with-param name="isref" select="'yes'"/>
    </xsl:apply-templates>
    <xsl:text>\n", this, </xsl:text>
    <xsl:value-of select="concat('&quot;', $topclass, '::get', $attrbasename, '&quot;, ')"/>
    <xsl:apply-templates select="@type" mode="logparamval">
        <xsl:with-param name="dir" select="'out'"/>
        <xsl:with-param name="isref" select="'yes'"/>
    </xsl:apply-templates>
    <xsl:text>));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
        CheckComArgOutPointerValidThrow(a</xsl:text>
    <xsl:value-of select="$attrbasename"/>
    <xsl:text>);
        </xsl:text>
    <xsl:apply-templates select="@type" mode="paramvalconversion2tmpvar">
        <xsl:with-param name="dir" select="'out'"/>
    </xsl:apply-templates>
    <xsl:if test="$attrbasename != 'MidlDoesNotLikEmptyInterfaces'">
        <xsl:text>
#ifdef VBOX_WITH_DTRACE_R3_MAIN
        /* dtrace probe </xsl:text>
        <!-- <xsl:value-of select="concat($dtracetopclass, '__get__', $dtraceattrname, '__enter(struct ', $topclass)"/> -->
        <xsl:value-of select="concat($dtracetopclass, '__get__', $dtraceattrname, '__enter(void')"/>
        <xsl:text> *a_pThis); */
        </xsl:text>
        <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_GET_', $dtraceattrname, '_ENTER('), $G_lowerCase, $G_upperCase)"/>
        <xsl:text>this);
#endif</xsl:text>
    </xsl:if>
    <xsl:text>

        </xsl:text>
    <xsl:choose>
      <xsl:when test="$limitedAutoCaller = 'true'">
        <xsl:text>AutoLimitedCaller</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>AutoCaller</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text> autoCaller(this);
        if (FAILED(autoCaller.rc()))
            throw autoCaller.rc();

</xsl:text>
    <xsl:value-of select="concat('        hrc = get', $attrbasename, '(')"/>
    <xsl:variable name="passAutoCaller">
        <xsl:call-template name="checkoption">
            <xsl:with-param name="optionlist" select="@wrap-hint-server"/>
            <xsl:with-param name="option" select="'passcaller'"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:if test="$passAutoCaller = 'true'">
        <xsl:text>autoCaller, </xsl:text>
    </xsl:if>
    <xsl:apply-templates select="@type" mode="paramvalconversionusingtmp">
        <xsl:with-param name="dir" select="'out'"/>
    </xsl:apply-templates>
    <xsl:text>);
</xsl:text>
    <xsl:if test="$attrbasename != 'MidlDoesNotLikEmptyInterfaces'">
        <xsl:text>
#ifdef VBOX_WITH_DTRACE_R3_MAIN
        /* dtrace probe </xsl:text>
        <!-- <xsl:value-of select="concat($dtracetopclass, '__get__', $dtraceattrname, '__return(struct ', $topclass, ' *a_pThis')"/> -->
        <xsl:value-of select="concat($dtracetopclass, '__get__', $dtraceattrname, '__return(void *a_pThis')"/>
        <xsl:text>, uint32_t a_hrc, int32_t enmWhy, </xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamdecl">
            <xsl:with-param name="dir">out</xsl:with-param>
        </xsl:apply-templates>
        <xsl:text>); */
            </xsl:text>
        <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_GET_', $dtraceattrname, '_RETURN('), $G_lowerCase, $G_upperCase)"/>
        <xsl:text>this, hrc, 0 /*normal*/,</xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamval">
            <xsl:with-param name="dir">out</xsl:with-param>
        </xsl:apply-templates>
        <xsl:text>);
#endif
</xsl:text>
    </xsl:if>
    <xsl:text>
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;</xsl:text>
    <xsl:if test="$attrbasename != 'MidlDoesNotLikEmptyInterfaces'">
        <xsl:text>
#ifdef VBOX_WITH_DTRACE_R3_MAIN
        </xsl:text>
    <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_GET_', $dtraceattrname, '_RETURN('), $G_lowerCase, $G_upperCase)"/>
    <xsl:text>this, hrc, 1 /*hrc exception*/,</xsl:text>
    <xsl:apply-templates select="@type" mode="dtraceparamvalnotmp">
        <xsl:with-param name="dir">out</xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>);
#endif
</xsl:text>
    </xsl:if>
    <xsl:text>
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);</xsl:text>
    <xsl:if test="$attrbasename != 'MidlDoesNotLikEmptyInterfaces'">
        <xsl:text>
#ifdef VBOX_WITH_DTRACE_R3_MAIN
        </xsl:text>
    <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_GET_', $dtraceattrname, '_RETURN('), $G_lowerCase, $G_upperCase)"/>
    <xsl:text>this, hrc, 9 /*unhandled exception*/,</xsl:text>
    <xsl:apply-templates select="@type" mode="dtraceparamvalnotmp">
        <xsl:with-param name="dir">out</xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>);
#endif
</xsl:text>
    </xsl:if>
    <xsl:text>
    }

    LogRelFlow(("{%p} %s: leave </xsl:text>
    <xsl:apply-templates select="@type" mode="logparamtext">
        <xsl:with-param name="dir" select="'out'"/>
        <xsl:with-param name="isref" select="''"/>
    </xsl:apply-templates>
    <xsl:text> hrc=%Rhrc\n", this, </xsl:text>
    <xsl:value-of select="concat('&quot;', $topclass, '::get', $dtraceattrname, '&quot;, ')"/>
    <xsl:apply-templates select="@type" mode="logparamval">
        <xsl:with-param name="dir" select="'out'"/>
        <xsl:with-param name="isref" select="''"/>
    </xsl:apply-templates>
    <xsl:text>, hrc));
    return hrc;
}
</xsl:text>
    <xsl:if test="not(@readonly) or @readonly!='yes'">
    <xsl:text>
</xsl:text>
        <xsl:value-of select="concat('STDMETHODIMP ', $topclass, 'Wrap::COMSETTER(', $attrbasename, ')(')"/>
        <xsl:apply-templates select="@type" mode="public">
            <xsl:with-param name="dir" select="'in'"/>
        </xsl:apply-templates>
        <!-- @todo check in parameters if possible -->
        <xsl:text>)
{
    LogRelFlow(("{%p} %s: enter </xsl:text>
        <xsl:apply-templates select="@type" mode="logparamtext">
            <xsl:with-param name="dir" select="'in'"/>
            <xsl:with-param name="isref" select="''"/>
        </xsl:apply-templates>
        <xsl:text>\n", this, </xsl:text>
        <xsl:value-of select="concat('&quot;', $topclass, '::set', $attrbasename, '&quot;, ')"/>
        <xsl:apply-templates select="@type" mode="logparamval">
            <xsl:with-param name="dir" select="'in'"/>
            <xsl:with-param name="isref" select="''"/>
        </xsl:apply-templates>
        <xsl:text>));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
        </xsl:text>
        <xsl:apply-templates select="@type" mode="paramvalconversion2tmpvar">
            <xsl:with-param name="dir" select="'in'"/>
        </xsl:apply-templates>
        <xsl:text>

#ifdef VBOX_WITH_DTRACE_R3_MAIN
        /* dtrace probe </xsl:text>
        <!-- <xsl:value-of select="concat($topclass, '__set__', $dtraceattrname, '__enter(struct ', $topclass, ' *a_pThis, ')"/>-->
        <xsl:value-of select="concat($topclass, '__set__', $dtraceattrname, '__enter(void *a_pThis, ')"/>
        <xsl:apply-templates select="@type" mode="dtraceparamdecl">
            <xsl:with-param name="dir" select="'in'"/>
        </xsl:apply-templates>
        <xsl:text>); */
        </xsl:text>
        <xsl:value-of select="translate(concat('VBOXAPI_', $topclass, '_SET_', $dtraceattrname, '_ENTER('), $G_lowerCase, $G_upperCase)"/>
        <xsl:text>this, </xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamval">
            <xsl:with-param name="dir">in</xsl:with-param>
        </xsl:apply-templates>
        <xsl:text>);
#endif

        </xsl:text>
        <xsl:choose>
          <xsl:when test="$limitedAutoCaller = 'true'">
            <xsl:text>AutoLimitedCaller</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:text>AutoCaller</xsl:text>
          </xsl:otherwise>
        </xsl:choose>
        <xsl:text> autoCaller(this);
        if (FAILED(autoCaller.rc()))
            throw autoCaller.rc();

</xsl:text>
        <xsl:value-of select="concat('        hrc = set', $attrbasename, '(')"/>
        <xsl:if test="$passAutoCaller = 'true'">
            <xsl:text>autoCaller, </xsl:text>
        </xsl:if>
        <xsl:apply-templates select="@type" mode="paramvalconversionusingtmp">
            <xsl:with-param name="dir" select="'in'"/>
        </xsl:apply-templates>
        <xsl:text>);

#ifdef VBOX_WITH_DTRACE_R3_MAIN
        /* dtrace probe </xsl:text>
        <!-- <xsl:value-of select="concat($dtracetopclass, '__set__', $dtraceattrname, '__return(struct ', $topclass, ' *a_pThis')"/> -->
        <xsl:value-of select="concat($dtracetopclass, '__set__', $dtraceattrname, '__return(void *a_pThis')"/>
        <xsl:text>, uint32_t a_hrc, int32_t enmWhy, </xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamdecl">
            <xsl:with-param name="dir">in</xsl:with-param>
        </xsl:apply-templates>
        <xsl:text>); */
            </xsl:text>
        <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_SET_', $dtraceattrname, '_RETURN('), $G_lowerCase, $G_upperCase)"/>
        <xsl:text>this, hrc, 0 /*normal*/,</xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamval">
            <xsl:with-param name="dir">in</xsl:with-param>
        </xsl:apply-templates>
        <xsl:text>);
#endif
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
#ifdef VBOX_WITH_DTRACE_R3_MAIN
        </xsl:text>
    <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_SET_', $dtraceattrname, '_RETURN('), $G_lowerCase, $G_upperCase)"/>
    <xsl:text>this, hrc, 1 /*hrc exception*/,</xsl:text>
    <xsl:apply-templates select="@type" mode="dtraceparamvalnotmp">
        <xsl:with-param name="dir">in</xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>);
#endif
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
#ifdef VBOX_WITH_DTRACE_R3_MAIN
        </xsl:text>
    <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_SET_', $dtraceattrname, '_RETURN('), $G_lowerCase, $G_upperCase)"/>
    <xsl:text>this, hrc, 9 /*unhandled exception*/,</xsl:text>
    <xsl:apply-templates select="@type" mode="dtraceparamvalnotmp">
        <xsl:with-param name="dir">in</xsl:with-param>
    </xsl:apply-templates>
    <xsl:text>);
#endif
    }

    LogRelFlow(("{%p} %s: leave hrc=%Rhrc\n", this, </xsl:text>
        <xsl:value-of select="concat('&quot;', $topclass, '::set', $attrbasename, '&quot;, ')"/>
        <xsl:text>hrc));
    return hrc;
}
</xsl:text>
    </xsl:if>

    <xsl:call-template name="emitTargetEnd">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>

    <xsl:text>
</xsl:text>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit all attributes of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitAttributes">
    <xsl:param name="iface"/>
    <xsl:param name="topclass"/>
    <xsl:param name="dtracetopclass"/>
    <xsl:param name="pmode"/>

    <!-- first recurse to emit all base interfaces -->
    <xsl:variable name="extends" select="$iface/@extends"/>
    <xsl:if test="$extends and not($extends='$unknown') and not($extends='$errorinfo')">
        <xsl:call-template name="emitAttributes">
            <xsl:with-param name="iface" select="//interface[@name=$extends]"/>
            <xsl:with-param name="topclass" select="$topclass"/>
            <xsl:with-param name="pmode" select="$pmode"/>
            <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
        </xsl:call-template>
    </xsl:if>

    <xsl:choose>
        <xsl:when test="$pmode='code'">
            <xsl:text>//
</xsl:text>
            <xsl:value-of select="concat('// ', $iface/@name, ' properties')"/>
            <xsl:text>
//

</xsl:text>
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="concat('&#10;    // ', $pmode, ' ', $iface/@name, ' properties&#10;')"/>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:choose>
        <xsl:when test="$pmode='public'">
            <xsl:apply-templates select="$iface/attribute | $iface/if" mode="public">
                <xsl:with-param name="emitmode" select="'attribute'"/>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:when test="$pmode='wrapped'">
            <xsl:apply-templates select="$iface/attribute | $iface/if" mode="wrapped">
                <xsl:with-param name="emitmode" select="'attribute'"/>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:when test="$pmode='code'">
            <xsl:apply-templates select="$iface/attribute | $iface/if" mode="code">
                <xsl:with-param name="topclass" select="$topclass"/>
                <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
                <xsl:with-param name="emitmode" select="'attribute'"/>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:otherwise/>
    </xsl:choose>
</xsl:template>

<xsl:template name="emitTargetBegin">
    <xsl:param name="target"/>

    <xsl:choose>
        <xsl:when test="$target = 'xpidl'">
            <xsl:text>#ifdef VBOX_WITH_XPCOM
</xsl:text>
        </xsl:when>
        <xsl:when test="$target = 'midl'">
            <xsl:text>#ifndef VBOX_WITH_XPCOM
</xsl:text>
        </xsl:when>
        <xsl:otherwise/>
    </xsl:choose>
</xsl:template>

<xsl:template name="emitTargetEnd">
    <xsl:param name="target"/>

    <xsl:choose>
        <xsl:when test="$target = 'xpidl'">
            <xsl:text>#endif /* VBOX_WITH_XPCOM */
</xsl:text>
        </xsl:when>
        <xsl:when test="$target = 'midl'">
            <xsl:text>#endif /* !VBOX_WITH_XPCOM */
</xsl:text>
        </xsl:when>
        <xsl:otherwise/>
    </xsl:choose>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit method
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="method" mode="public">
    <xsl:param name="target"/>

    <xsl:call-template name="emitTargetBegin">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>

    <xsl:variable name="methodindent">
      <xsl:call-template name="tospace">
          <xsl:with-param name="str" select="@name"/>
      </xsl:call-template>
    </xsl:variable>

    <xsl:text>    STDMETHOD(</xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>)(</xsl:text>
    <xsl:for-each select="param">
        <xsl:apply-templates select="@type" mode="public">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
        <xsl:if test="not(position()=last())">
            <xsl:text>,
                </xsl:text>
            <xsl:value-of select="$methodindent"/>
        </xsl:if>
    </xsl:for-each>
    <xsl:text>);
</xsl:text>

    <xsl:call-template name="emitTargetEnd">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="method" mode="wrapped">
    <xsl:param name="target"/>

    <xsl:call-template name="emitTargetBegin">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>

    <xsl:variable name="methodindent">
        <xsl:call-template name="tospace">
            <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
    </xsl:variable>

    <xsl:text>    virtual HRESULT </xsl:text>
    <xsl:call-template name="uncapitalize">
        <xsl:with-param name="str" select="@name"/>
    </xsl:call-template>
    <xsl:text>(</xsl:text>
    <xsl:variable name="passAutoCaller">
        <xsl:call-template name="checkoption">
            <xsl:with-param name="optionlist" select="@wrap-hint-server"/>
            <xsl:with-param name="option" select="'passcaller'"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:if test="$passAutoCaller = 'true'">
        <xsl:text>AutoCaller &amp;aAutoCaller</xsl:text>
        <xsl:if test="count(param) > 0">
            <xsl:text>,
                     </xsl:text>
            <xsl:value-of select="$methodindent"/>
        </xsl:if>
    </xsl:if>
    <xsl:for-each select="param">
        <xsl:apply-templates select="@type" mode="wrapped">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
        <xsl:if test="not(position()=last())">
            <xsl:text>,
                     </xsl:text>
            <xsl:value-of select="$methodindent"/>
        </xsl:if>
    </xsl:for-each>
    <xsl:text>) = 0;
</xsl:text>

    <xsl:call-template name="emitTargetEnd">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="method" mode="code">
    <xsl:param name="topclass"/>
    <xsl:param name="dtracetopclass"/>
    <xsl:param name="target"/>

    <xsl:call-template name="emitTargetBegin">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>

    <xsl:variable name="methodindent">
      <xsl:call-template name="tospace">
          <xsl:with-param name="str" select="@name"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="methodclassindent">
      <xsl:call-template name="tospace">
          <xsl:with-param name="str" select="concat($topclass, @name)"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="methodbasename">
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="limitedAutoCaller">
        <xsl:call-template name="checkoption">
            <xsl:with-param name="optionlist" select="@wrap-hint-server"/>
            <xsl:with-param name="option" select="'limitedcaller'"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="dtracemethodname">
        <xsl:choose>
            <xsl:when test="@dtracename">
                <xsl:value-of select="@dtracename"/>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="@name"/>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:variable>

    <xsl:value-of select="concat('STDMETHODIMP ', $topclass, 'Wrap::', $methodbasename, '(')"/>
    <xsl:for-each select="param">
        <xsl:apply-templates select="@type" mode="public">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
        <xsl:if test="not(position()=last())">
            <xsl:text>,
                    </xsl:text>
            <xsl:value-of select="$methodclassindent"/>
        </xsl:if>
    </xsl:for-each>
    <xsl:text>)
{
    LogRelFlow(("{%p} %s:enter</xsl:text>
    <xsl:for-each select="param">
        <xsl:text> </xsl:text>
        <xsl:apply-templates select="@type" mode="logparamtext">
            <xsl:with-param name="dir" select="@dir"/>
            <xsl:with-param name="isref" select="'yes'"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>\n", this</xsl:text>
    <xsl:value-of select="concat(', &quot;', $topclass, '::', @name, '&quot;')"/>
    <xsl:for-each select="param">
        <xsl:text>, </xsl:text>
        <xsl:apply-templates select="@type" mode="logparamval">
            <xsl:with-param name="dir" select="@dir"/>
            <xsl:with-param name="isref" select="'yes'"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
</xsl:text>
    <!-- @todo check in parameters if possible -->
    <xsl:for-each select="param">
        <xsl:if test="@dir!='in'">
            <xsl:text>        CheckComArgOutPointerValidThrow(a</xsl:text>
            <xsl:call-template name="capitalize">
                <xsl:with-param name="str" select="@name"/>
            </xsl:call-template>
            <xsl:text>);
</xsl:text>
        </xsl:if>
    </xsl:for-each>
<xsl:text>
</xsl:text>
    <xsl:for-each select="param">
        <xsl:text>
        </xsl:text>
        <xsl:apply-templates select="@type" mode="paramvalconversion2tmpvar">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>
#ifdef VBOX_WITH_DTRACE_R3_MAIN
        /* dtrace probe </xsl:text>
    <xsl:variable name="dtracenamehack"> <!-- Ugly hack to deal with Session::assignMachine and similar. -->
        <xsl:if test="name(..) = 'if'">
            <xsl:value-of select="concat('__', ../@target)"/>
        </xsl:if>
    </xsl:variable>
    <!-- <xsl:value-of select="concat($dtracetopclass, '__', $dtracemethodname, $dtracenamehack, '__enter(struct ', $dtracetopclass, ' *a_pThis')"/> -->
    <xsl:value-of select="concat($dtracetopclass, '__', $dtracemethodname, $dtracenamehack, '__enter(void *a_pThis')"/>
    <xsl:for-each select="param[@dir='in']">
        <xsl:text>, </xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamdecl">
            <xsl:with-param name="dir" select="'@dir'"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>); */
        </xsl:text>
    <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_', $dtracemethodname, substring($dtracenamehack, 2), '_ENTER('), $G_lowerCase, $G_upperCase)"/>
    <xsl:text>this</xsl:text>
    <xsl:for-each select="param[@dir='in']">
        <xsl:text>, </xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamval">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>);
#endif

        </xsl:text>
    <xsl:choose>
      <xsl:when test="$limitedAutoCaller = 'true'">
        <xsl:text>AutoLimitedCaller</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>AutoCaller</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text> autoCaller(this);
        if (FAILED(autoCaller.rc()))
            throw autoCaller.rc();

</xsl:text>
    <xsl:value-of select="concat('        hrc = ', @name, '(')"/>
    <xsl:variable name="passAutoCaller">
        <xsl:call-template name="checkoption">
            <xsl:with-param name="optionlist" select="@wrap-hint-server"/>
            <xsl:with-param name="option" select="'passcaller'"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:if test="$passAutoCaller = 'true'">
        <xsl:text>autoCaller</xsl:text>
        <xsl:if test="count(param) > 0">
            <xsl:text>,
               </xsl:text>
            <xsl:value-of select="$methodindent"/>
        </xsl:if>
    </xsl:if>
    <xsl:for-each select="param">
        <xsl:apply-templates select="@type" mode="paramvalconversionusingtmp">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
        <xsl:if test="not(position()=last())">
            <xsl:text>,
               </xsl:text>
            <xsl:value-of select="$methodindent"/>
        </xsl:if>
    </xsl:for-each>
    <xsl:text>);

#ifdef VBOX_WITH_DTRACE_R3_MAIN
        /* dtrace probe </xsl:text>
    <!-- <xsl:value-of select="concat($dtracetopclass, '__', $dtracemethodname, '__return(struct ', $dtracetopclass, ' *a_pThis')"/> -->
    <xsl:value-of select="concat($dtracetopclass, '__', $dtracemethodname, $dtracenamehack, '__return(void *a_pThis')"/>
    <xsl:text>, uint32_t a_hrc, int32_t enmWhy</xsl:text>
    <xsl:for-each select="param">
        <xsl:text>, </xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamdecl">
            <xsl:with-param name="dir" select="'@dir'"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>); */
        </xsl:text>
    <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_', $dtracemethodname, substring($dtracenamehack, 2), '_RETURN('), $G_lowerCase, $G_upperCase)"/>
    <xsl:text>this, hrc, 0 /*normal*/</xsl:text>
    <xsl:for-each select="param">
        <xsl:text>, </xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamval">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>);
#endif
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
#ifdef VBOX_WITH_DTRACE_R3_MAIN
        </xsl:text>
    <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_', $dtracemethodname, substring($dtracenamehack, 2), '_RETURN('), $G_lowerCase, $G_upperCase)"/>
    <xsl:text>this, hrc, 1 /*hrc exception*/</xsl:text>
    <xsl:for-each select="param">
        <xsl:text>, </xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamvalnotmp">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>);
#endif
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
#ifdef VBOX_WITH_DTRACE_R3_MAIN
        </xsl:text>
    <xsl:value-of select="translate(concat('VBOXAPI_', $dtracetopclass, '_', $dtracemethodname, substring($dtracenamehack, 2), '_RETURN('), $G_lowerCase, $G_upperCase)"/>
    <xsl:text>this, hrc, 9 /*unhandled exception*/</xsl:text>
    <xsl:for-each select="param">
        <xsl:text>, </xsl:text>
        <xsl:apply-templates select="@type" mode="dtraceparamvalnotmp">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>);
#endif
    }

    LogRelFlow(("{%p} %s: leave</xsl:text>
    <xsl:for-each select="param">
        <xsl:if test="@dir!='in'">
            <xsl:text> </xsl:text>
            <xsl:apply-templates select="@type" mode="logparamtext">
                <xsl:with-param name="dir" select="@dir"/>
                <xsl:with-param name="isref" select="''"/>
            </xsl:apply-templates>
        </xsl:if>
    </xsl:for-each>
    <xsl:text> hrc=%Rhrc\n", this</xsl:text>
    <xsl:value-of select="concat(', &quot;', $topclass, '::', @name, '&quot;')"/>
    <xsl:for-each select="param">
        <xsl:if test="@dir!='in'">
            <xsl:text>, </xsl:text>
            <xsl:apply-templates select="@type" mode="logparamval">
                <xsl:with-param name="dir" select="@dir"/>
                <xsl:with-param name="isref" select="''"/>
            </xsl:apply-templates>
        </xsl:if>
    </xsl:for-each>
    <xsl:text>, hrc));
    return hrc;
}
</xsl:text>

    <xsl:call-template name="emitTargetEnd">
        <xsl:with-param name="target" select="$target"/>
    </xsl:call-template>

    <xsl:text>
</xsl:text>
</xsl:template>

<xsl:template name="emitIf">
    <xsl:param name="passmode"/>
    <xsl:param name="target"/>
    <xsl:param name="topclass"/>
    <xsl:param name="emitmode"/>
    <xsl:param name="dtracetopclass"/>

    <xsl:if test="($target = 'xpidl') or ($target = 'midl')">
        <xsl:choose>
            <xsl:when test="$filelistonly=''">
                <xsl:choose>
                    <xsl:when test="$passmode='public'">
                        <xsl:choose>
                            <xsl:when test="$emitmode='method'">
                                <xsl:apply-templates select="method" mode="public">
                                    <xsl:with-param name="target" select="$target"/>
                                </xsl:apply-templates>
                            </xsl:when>
                            <xsl:when test="$emitmode='attribute'">
                                <xsl:apply-templates select="attribute" mode="public">
                                    <xsl:with-param name="target" select="$target"/>
                                </xsl:apply-templates>
                            </xsl:when>
                            <xsl:otherwise/>
                        </xsl:choose>
                    </xsl:when>
                    <xsl:when test="$passmode='wrapped'">
                        <xsl:choose>
                            <xsl:when test="$emitmode='method'">
                                <xsl:apply-templates select="method" mode="wrapped">
                                    <xsl:with-param name="target" select="$target"/>
                                </xsl:apply-templates>
                            </xsl:when>
                            <xsl:when test="$emitmode='attribute'">
                                <xsl:apply-templates select="attribute" mode="wrapped">
                                    <xsl:with-param name="target" select="$target"/>
                                </xsl:apply-templates>
                            </xsl:when>
                            <xsl:otherwise/>
                        </xsl:choose>
                    </xsl:when>
                    <xsl:when test="$passmode='code'">
                        <xsl:choose>
                            <xsl:when test="$emitmode='method'">
                                <xsl:apply-templates select="method" mode="code">
                                    <xsl:with-param name="target" select="$target"/>
                                    <xsl:with-param name="topclass" select="$topclass"/>
                                    <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
                                </xsl:apply-templates>
                            </xsl:when>
                            <xsl:when test="$emitmode='attribute'">
                                <xsl:apply-templates select="attribute" mode="code">
                                    <xsl:with-param name="target" select="$target"/>
                                    <xsl:with-param name="topclass" select="$topclass"/>
                                    <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
                                </xsl:apply-templates>
                            </xsl:when>
                            <xsl:otherwise/>
                        </xsl:choose>
                    </xsl:when>
                    <xsl:otherwise/>
                </xsl:choose>
            </xsl:when>
            <xsl:otherwise>
                <xsl:apply-templates/>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:if>
</xsl:template>

<xsl:template match="if" mode="public">
    <xsl:param name="emitmode"/>

    <xsl:call-template name="emitIf">
        <xsl:with-param name="passmode" select="'public'"/>
        <xsl:with-param name="target" select="@target"/>
        <xsl:with-param name="emitmode" select="$emitmode"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="if" mode="wrapped">
    <xsl:param name="emitmode"/>

    <xsl:call-template name="emitIf">
        <xsl:with-param name="passmode" select="'wrapped'"/>
        <xsl:with-param name="target" select="@target"/>
        <xsl:with-param name="emitmode" select="$emitmode"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="if" mode="code">
    <xsl:param name="topclass"/>
    <xsl:param name="emitmode"/>
    <xsl:param name="dtracetopclass"/>

    <xsl:call-template name="emitIf">
        <xsl:with-param name="passmode" select="'code'"/>
        <xsl:with-param name="target" select="@target"/>
        <xsl:with-param name="emitmode" select="$emitmode"/>
        <xsl:with-param name="topclass" select="$topclass"/>
        <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
    </xsl:call-template>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit all methods of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitMethods">
    <xsl:param name="iface"/>
    <xsl:param name="topclass"/>
    <xsl:param name="pmode"/>
    <xsl:param name="dtracetopclass"/>

    <!-- first recurse to emit all base interfaces -->
    <xsl:variable name="extends" select="$iface/@extends"/>
    <xsl:if test="$extends and not($extends='$unknown') and not($extends='$errorinfo')">
        <xsl:call-template name="emitMethods">
            <xsl:with-param name="iface" select="//interface[@name=$extends]"/>
            <xsl:with-param name="topclass" select="$topclass"/>
            <xsl:with-param name="pmode" select="$pmode"/>
            <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
        </xsl:call-template>
    </xsl:if>

    <xsl:choose>
        <xsl:when test="$pmode='code'">
            <xsl:text>//
</xsl:text>
            <xsl:value-of select="concat('// ', $iface/@name, ' methods')"/>
            <xsl:text>
//

</xsl:text>
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="concat('&#10;    // ', $pmode, ' ', $iface/@name, ' methods&#10;')"/>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:choose>
        <xsl:when test="$pmode='public'">
            <xsl:apply-templates select="$iface/method | $iface/if" mode="public">
                <xsl:with-param name="emitmode" select="'method'"/>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:when test="$pmode='wrapped'">
            <xsl:apply-templates select="$iface/method | $iface/if" mode="wrapped">
                <xsl:with-param name="emitmode" select="'method'"/>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:when test="$pmode='code'">
            <xsl:apply-templates select="$iface/method | $iface/if" mode="code">
                <xsl:with-param name="topclass" select="$topclass"/>
                <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
                <xsl:with-param name="emitmode" select="'method'"/>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:otherwise/>
    </xsl:choose>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit all attributes and methods declarations of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitInterfaceDecls">
    <xsl:param name="iface"/>
    <xsl:param name="pmode"/>

    <!-- attributes -->
    <xsl:call-template name="emitAttributes">
        <xsl:with-param name="iface" select="$iface"/>
        <xsl:with-param name="pmode" select="$pmode"/>
    </xsl:call-template>

    <!-- methods -->
    <xsl:call-template name="emitMethods">
        <xsl:with-param name="iface" select="$iface"/>
        <xsl:with-param name="pmode" select="$pmode"/>
    </xsl:call-template>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit auxiliary method declarations of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitAuxMethodDecls">
    <xsl:param name="iface"/>
    <!-- currently nothing, maybe later some generic FinalConstruct/... helper declaration for ComObjPtr -->
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit the header file of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitHeader">
    <xsl:param name="iface"/>
    <xsl:param name="addinterfaces"/>

    <xsl:variable name="filename" select="concat(substring(@name, 2), 'Wrap.h')"/>

    <xsl:choose>
        <xsl:when test="$filelistonly=''">
            <xsl:apply-templates select="$iface" mode="startfile">
                <xsl:with-param name="file" select="$filename"/>
            </xsl:apply-templates>
            <xsl:call-template name="fileheader">
                <xsl:with-param name="name" select="$filename"/>
                <xsl:with-param name="class" select="substring(@name, 2)"/>
                <xsl:with-param name="type" select="'header'"/>
            </xsl:call-template>
            <xsl:apply-templates select="$iface" mode="classheader">
                <xsl:with-param name="addinterfaces" select="$addinterfaces"/>
            </xsl:apply-templates>

            <!-- interface attributes/methods (public) -->
            <xsl:call-template name="emitInterfaceDecls">
                <xsl:with-param name="iface" select="$iface"/>
                <xsl:with-param name="pmode" select="'public'"/>
            </xsl:call-template>

            <xsl:for-each select="exsl:node-set($addinterfaces)/token">
                <!-- This is super tricky, as the for-each switches to the node
                     set, which means the normal document isn't available any
                     more. So need to use the global root node to get back into
                     the documemt to find corresponding interface data. -->
                <xsl:variable name="addifname">
                    <xsl:value-of select="string(.)"/>
                </xsl:variable>
                <xsl:call-template name="emitInterfaceDecls">
                    <xsl:with-param name="iface" select="$G_root//interface[@name=$addifname]"/>
                    <xsl:with-param name="pmode" select="'public'"/>
                </xsl:call-template>
            </xsl:for-each>

            <!-- auxiliary methods (public) -->
            <xsl:call-template name="emitAuxMethodDecls">
                <xsl:with-param name="iface" select="$iface"/>
            </xsl:call-template>

            <!-- switch to private -->
            <xsl:text>
private:</xsl:text>

            <!-- wrapped interface attributes/methods (private) -->
            <xsl:call-template name="emitInterfaceDecls">
                <xsl:with-param name="iface" select="$iface"/>
                <xsl:with-param name="pmode" select="'wrapped'"/>
            </xsl:call-template>

            <xsl:for-each select="exsl:node-set($addinterfaces)/token">
                <!-- This is super tricky, as the for-each switches to the node
                     set, which means the normal document isn't available any
                     more. So need to use the global root node to get back into
                     the documemt to find corresponding interface data. -->
                <xsl:variable name="addifname">
                    <xsl:value-of select="string(.)"/>
                </xsl:variable>
                <xsl:call-template name="emitInterfaceDecls">
                    <xsl:with-param name="iface" select="$G_root//interface[@name=$addifname]"/>
                    <xsl:with-param name="pmode" select="'wrapped'"/>
                </xsl:call-template>
            </xsl:for-each>

            <xsl:apply-templates select="$iface" mode="classfooter"/>
            <xsl:apply-templates select="$iface" mode="endfile">
                <xsl:with-param name="file" select="$filename"/>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:otherwise>
            <xsl:apply-templates select="$iface" mode="listfile">
                <xsl:with-param name="file" select="$filename"/>
            </xsl:apply-templates>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit all attributes and methods definitions of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitInterfaceDefs">
    <xsl:param name="iface"/>
    <xsl:param name="addinterfaces"/>

    <xsl:value-of select="concat('DEFINE_EMPTY_CTOR_DTOR(', substring($iface/@name, 2), 'Wrap)&#10;&#10;')"/>

    <xsl:variable name="dtracetopclass">
        <xsl:choose>
            <xsl:when test="$iface/@dtracename"><xsl:value-of select="$iface/@dtracename"/></xsl:when>
            <xsl:otherwise><xsl:value-of select="substring($iface/@name, 2)"/></xsl:otherwise>
        </xsl:choose>
    </xsl:variable>

    <!-- attributes -->
    <xsl:call-template name="emitAttributes">
        <xsl:with-param name="iface" select="$iface"/>
        <xsl:with-param name="topclass" select="substring($iface/@name, 2)"/>
        <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
        <xsl:with-param name="pmode" select="'code'"/>
    </xsl:call-template>

    <xsl:for-each select="exsl:node-set($addinterfaces)/token">
        <!-- This is super tricky, as the for-each switches to the node set,
             which means the normal document isn't available any more. So need
             to use the global root node to get back into the documemt to find
             corresponding interface data. -->
        <xsl:variable name="addifname">
            <xsl:value-of select="string(.)"/>
        </xsl:variable>
        <xsl:call-template name="emitAttributes">
            <xsl:with-param name="iface" select="$G_root//interface[@name=$addifname]"/>
            <xsl:with-param name="topclass" select="substring($iface/@name, 2)"/>
            <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
            <xsl:with-param name="pmode" select="'code'"/>
        </xsl:call-template>
    </xsl:for-each>

    <!-- methods -->
    <xsl:call-template name="emitMethods">
        <xsl:with-param name="iface" select="$iface"/>
        <xsl:with-param name="topclass" select="substring($iface/@name, 2)"/>
        <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
        <xsl:with-param name="pmode" select="'code'"/>
    </xsl:call-template>

    <xsl:for-each select="exsl:node-set($addinterfaces)/token">
        <!-- This is super tricky, as the for-each switches to the node set,
             which means the normal document isn't available any more. So need
             to use the global root node to get back into the documemt to find
             corresponding interface data. -->
        <xsl:variable name="addifname">
            <xsl:value-of select="string(.)"/>
        </xsl:variable>
        <xsl:call-template name="emitMethods">
            <xsl:with-param name="iface" select="$G_root//interface[@name=$addifname]"/>
            <xsl:with-param name="topclass" select="substring($iface/@name, 2)"/>
            <xsl:with-param name="dtracetopclass" select="$dtracetopclass"/>
            <xsl:with-param name="pmode" select="'code'"/>
        </xsl:call-template>
    </xsl:for-each>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit auxiliary method declarations of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitAuxMethodDefs">
    <xsl:param name="iface"/>
    <!-- currently nothing, maybe later some generic FinalConstruct/... implementation -->
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit the code file of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitCode">
    <xsl:param name="iface"/>
    <xsl:param name="addinterfaces"/>

    <xsl:variable name="filename" select="concat(substring(@name, 2), 'Wrap.cpp')"/>

    <xsl:choose>
        <xsl:when test="$filelistonly=''">
            <xsl:apply-templates select="$iface" mode="startfile">
                <xsl:with-param name="file" select="$filename"/>
            </xsl:apply-templates>
            <xsl:call-template name="fileheader">
                <xsl:with-param name="name" select="$filename"/>
                <xsl:with-param name="class" select="substring(@name, 2)"/>
                <xsl:with-param name="type" select="'code'"/>
            </xsl:call-template>
            <xsl:apply-templates select="$iface" mode="codeheader">
                <xsl:with-param name="addinterfaces" select="$addinterfaces"/>
            </xsl:apply-templates>

            <!-- interface attributes/methods (public) -->
            <xsl:call-template name="emitInterfaceDefs">
                <xsl:with-param name="iface" select="$iface"/>
                <xsl:with-param name="addinterfaces" select="$addinterfaces"/>
            </xsl:call-template>

            <!-- auxiliary methods (public) -->
            <xsl:call-template name="emitAuxMethodDefs">
                <xsl:with-param name="iface" select="$iface"/>
            </xsl:call-template>

            <xsl:apply-templates select="$iface" mode="codefooter">
                <xsl:with-param name="addinterfaces" select="$addinterfaces"/>
            </xsl:apply-templates>
            <xsl:apply-templates select="$iface" mode="endfile">
                <xsl:with-param name="file" select="$filename"/>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:otherwise>
            <xsl:apply-templates select="$iface" mode="listfile">
                <xsl:with-param name="file" select="$filename"/>
            </xsl:apply-templates>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  wildcard match, ignore everything which has no explicit match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="*"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  ignore all if tags except those for XPIDL or MIDL target
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="if">
    <xsl:if test="(@target = 'xpidl') or (@target = 'midl')">
        <xsl:apply-templates/>
    </xsl:if>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  interface match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="interface">
    <xsl:if test="not(@internal='yes') and not(@supportsErrorInfo='no')">
        <xsl:call-template name="emitInterface">
            <xsl:with-param name="iface" select="."/>
        </xsl:call-template>
    </xsl:if>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  library match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="library">
    <xsl:apply-templates/>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  root match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="/idl">
    <xsl:choose>
        <xsl:when test="$filelistonly=''">
        </xsl:when>
        <xsl:otherwise>
            <xsl:value-of select="concat($filelistonly, ' := \&#10;')"/>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:apply-templates/>
    <xsl:choose>
        <xsl:when test="$filelistonly=''">
        </xsl:when>
        <xsl:otherwise>
            <xsl:text>
</xsl:text>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

</xsl:stylesheet>
<!-- vi: set tabstop=4 shiftwidth=4 expandtab: -->
