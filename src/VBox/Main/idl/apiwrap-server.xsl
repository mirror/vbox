<?xml version="1.0"?>

<!--
    apiwrap-server.xsl:
        XSLT stylesheet that generates C++ API wrappers (server side) from
        VirtualBox.xidl.

     Copyright (C) 2010-2013 Oracle Corporation

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
 * Copyright (C) 2011-2013 Oracle Corporation
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
    <xsl:if test="$extends and not($extends='$unknown') and not($extends='$dispatched') and not($extends='$errorinfo')">
        <xsl:call-template name="emitCOMInterfaces">
            <xsl:with-param name="iface" select="//interface[@name=$extends]"/>
        </xsl:call-template>
    </xsl:if>
</xsl:template>

<xsl:template match="interface" mode="classheader">
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
    <xsl:value-of select="concat('    VBOX_SCRIPTABLE_IMPL(', @name, ')&#10;')"/>
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
    <xsl:text>    END_COM_MAP()

</xsl:text>
    <xsl:value-of select="concat('    DECLARE_EMPTY_CTOR_DTOR(', substring(@name, 2), 'Wrap)&#10;')"/>
</xsl:template>

<xsl:template match="interface" mode="classfooter">
    <xsl:text>};

</xsl:text>
    <xsl:value-of select="concat('#endif // !', substring(@name, 2), 'Wrap_H_&#10;')"/>
</xsl:template>

<xsl:template match="interface" mode="codeheader">
    <xsl:value-of select="concat('#define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_MAIN_', translate(substring(@name, 2), $G_lowerCase, $G_upperCase), '&#10;&#10;')"/>
    <xsl:value-of select="concat('#include &quot;', substring(@name, 2), 'Wrap.h&quot;&#10;')"/>
    <xsl:text>#include "Logging.h"

</xsl:text>
</xsl:template>

<xsl:template match="interface" mode="codefooter">
    <xsl:text>#ifdef VBOX_WITH_XPCOM
</xsl:text>
    <xsl:value-of select="concat('NS_DECL_CLASSINFO(', substring(@name, 2), 'Wrap)&#10;NS_IMPL_THREADSAFE_ISUPPORTS1_CI(', substring(@name, 2), 'Wrap, ', @name, ')&#10;')"/>
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

<xsl:template name="translatepublictype">
    <xsl:param name="type"/>
    <xsl:param name="dir"/>

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
</xsl:template>

<xsl:template name="translatewrappedtype">
    <xsl:param name="type"/>
    <xsl:param name="dir"/>
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
</xsl:template>

<xsl:template name="translatefmtspectype">
    <xsl:param name="type"/>
    <xsl:param name="dir"/>
    <xsl:param name="safearray"/>
    <xsl:param name="mod"/>

    <!-- get C format string for IDL type from table in typemap-shared.inc.xsl -->
    <xsl:variable name="wrapfmt" select="exsl:node-set($G_aSharedTypes)/type[@idlname=$type]/@gluefmt"/>
    <xsl:choose>
        <xsl:when test="$mod='ref' and $dir!='in'">
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

<xsl:template match="attribute/@type | param/@type" mode="public">
    <xsl:param name="dir"/>

    <xsl:variable name="gluetype">
        <xsl:call-template name="translatepublictype">
            <xsl:with-param name="type" select="."/>
            <xsl:with-param name="dir" select="$dir"/>
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
    <xsl:param name="mod"/>

    <xsl:if test="$mod!='ref' and ($dir='out' or $dir='ret')">
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
        <xsl:with-param name="safearray" select="../@safearray"/>
        <xsl:with-param name="mod" select="$mod"/>
    </xsl:call-template>
</xsl:template>

<xsl:template match="attribute/@type | param/@type" mode="logparamval">
    <xsl:param name="dir"/>
    <xsl:param name="mod"/>

    <xsl:choose>
        <xsl:when test="../@safearray='yes' and $mod!='ref'">
            <xsl:text>ComSafeArraySize(</xsl:text>
            <xsl:if test="$mod!='ref' and $dir!='in'">
                <xsl:text>*</xsl:text>
            </xsl:if>
        </xsl:when>
        <xsl:when test="$mod!='ref' and $dir!='in'">
            <xsl:text>*</xsl:text>
        </xsl:when>
    </xsl:choose>
    <xsl:text>a</xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="../@name"/>
    </xsl:call-template>
    <xsl:choose>
        <xsl:when test="../@safearray='yes' and $mod!='ref'">
            <xsl:text>)</xsl:text>
        </xsl:when>
    </xsl:choose>
</xsl:template>

<xsl:template match="attribute/@type | param/@type" mode="paramvalconversion">
    <xsl:param name="dir"/>

    <xsl:variable name="gluetype">
        <xsl:call-template name="translatepublictype">
            <xsl:with-param name="type" select="."/>
            <xsl:with-param name="dir" select="$dir"/>
        </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="type" select="."/>
    <xsl:variable name="thatif" select="../../../..//interface[@name=$type]"/>
    <xsl:choose>
        <xsl:when test="$type='$unknown'">
            <xsl:if test="../@safearray='yes'">
                <xsl:text>Array</xsl:text>
            </xsl:if>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>ComTypeInConverter&lt;IUnknown&gt;(</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>ComTypeOutConverter&lt;IUnknown&gt;(</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
            <xsl:if test="../@safearray='yes'">
                <xsl:choose>
                    <xsl:when test="$dir='in'">
                        <xsl:text>ComSafeArrayInArg(</xsl:text>
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:text>ComSafeArrayOutArg(</xsl:text>
                    </xsl:otherwise>
                </xsl:choose>
            </xsl:if>
        </xsl:when>
        <xsl:when test="$thatif">
            <xsl:if test="../@safearray='yes'">
                <xsl:text>Array</xsl:text>
            </xsl:if>
            <xsl:variable name="thatifname" select="$thatif/@name"/>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>ComTypeInConverter</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>ComTypeOutConverter</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
            <xsl:value-of select="concat('&lt;', $thatifname, '&gt;(')"/>
            <xsl:if test="../@safearray='yes'">
                <xsl:choose>
                    <xsl:when test="$dir='in'">
                        <xsl:text>ComSafeArrayInArg(</xsl:text>
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:text>ComSafeArrayOutArg(</xsl:text>
                    </xsl:otherwise>
                </xsl:choose>
            </xsl:if>
        </xsl:when>
        <xsl:when test="$type='wstring'">
            <xsl:if test="../@safearray='yes'">
                <xsl:text>Array</xsl:text>
            </xsl:if>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>BSTRInConverter(</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>BSTROutConverter(</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
            <xsl:if test="../@safearray='yes'">
                <xsl:choose>
                    <xsl:when test="$dir='in'">
                        <xsl:text>ComSafeArrayInArg(</xsl:text>
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:text>ComSafeArrayOutArg(</xsl:text>
                    </xsl:otherwise>
                </xsl:choose>
            </xsl:if>
        </xsl:when>
        <xsl:when test="$type='uuid'">
            <xsl:if test="../@safearray='yes'">
                <xsl:text>Array</xsl:text>
            </xsl:if>
            <xsl:choose>
                <xsl:when test="$dir='in'">
                    <xsl:text>UuidInConverter(</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>UuidOutConverter(</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
            <xsl:if test="../@safearray='yes'">
                <xsl:choose>
                    <xsl:when test="$dir='in'">
                        <xsl:text>ComSafeArrayInArg(</xsl:text>
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:text>ComSafeArrayOutArg(</xsl:text>
                    </xsl:otherwise>
                </xsl:choose>
            </xsl:if>
        </xsl:when>
        <xsl:otherwise>
            <xsl:if test="../@safearray='yes'">
                <xsl:text>Array</xsl:text>
                <xsl:choose>
                    <xsl:when test="$dir='in'">
                        <xsl:text>InConverter</xsl:text>
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:text>OutConverter</xsl:text>
                    </xsl:otherwise>
                </xsl:choose>
                <xsl:value-of select="concat('&lt;', $gluetype, '&gt;(')"/>
                <xsl:choose>
                    <xsl:when test="$dir='in'">
                        <xsl:text>ComSafeArrayInArg(</xsl:text>
                    </xsl:when>
                    <xsl:otherwise>
                        <xsl:text>ComSafeArrayOutArg(</xsl:text>
                    </xsl:otherwise>
                </xsl:choose>
            </xsl:if>
        </xsl:otherwise>
    </xsl:choose>
    <xsl:text>a</xsl:text>
    <xsl:call-template name="capitalize">
        <xsl:with-param name="str" select="../@name"/>
    </xsl:call-template>
    <xsl:choose>
        <xsl:when test="$type='$unknown' or $thatif">
            <xsl:choose>
                <xsl:when test="../@safearray='yes'">
                    <xsl:text>)).array()</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>).ptr()</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>
        <xsl:when test="$type='wstring'">
            <xsl:choose>
                <xsl:when test="../@safearray='yes'">
                    <xsl:text>)).array()</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>).str()</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>
        <xsl:when test="$type='uuid'">
            <xsl:choose>
                <xsl:when test="../@safearray='yes'">
                    <xsl:text>)).array()</xsl:text>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:text>).uuid()</xsl:text>
                </xsl:otherwise>
            </xsl:choose>
        </xsl:when>
        <xsl:otherwise>
            <xsl:if test="../@safearray='yes'">
                <xsl:text>)).array()</xsl:text>
            </xsl:if>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit attribute
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="attribute" mode="public">
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
</xsl:template>

<xsl:template match="attribute" mode="wrapped">
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
</xsl:template>

<xsl:template match="attribute" mode="code">
    <xsl:param name="topclass"/>

    <xsl:variable name="attrbasename">
        <xsl:call-template name="capitalize">
            <xsl:with-param name="str" select="@name"/>
        </xsl:call-template>
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
        <xsl:with-param name="mod" select="'ref'"/>
    </xsl:apply-templates>
    <xsl:text>\n", this, </xsl:text>
    <xsl:value-of select="concat('&quot;', $topclass, '::get', $attrbasename, '&quot;, ')"/>
    <xsl:apply-templates select="@type" mode="logparamval">
        <xsl:with-param name="dir" select="'out'"/>
        <xsl:with-param name="mod" select="'ref'"/>
    </xsl:apply-templates>
    <xsl:text>));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
        CheckComArgOutPointerValidThrow(a</xsl:text>
    <xsl:value-of select="$attrbasename"/>
    <xsl:text>);

        AutoCaller autoCaller(this);
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
    <xsl:apply-templates select="@type" mode="paramvalconversion">
        <xsl:with-param name="dir" select="'out'"/>
    </xsl:apply-templates>
    <xsl:text>);
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    LogRelFlow(("{%p} %s: leave </xsl:text>
    <xsl:apply-templates select="@type" mode="logparamtext">
        <xsl:with-param name="dir" select="'out'"/>
    </xsl:apply-templates>
    <xsl:text> hrc=%Rhrc\n", this, </xsl:text>
    <xsl:value-of select="concat('&quot;', $topclass, '::get', $attrbasename, '&quot;, ')"/>
    <xsl:apply-templates select="@type" mode="logparamval">
        <xsl:with-param name="dir" select="'out'"/>
    </xsl:apply-templates>
    <xsl:text>, hrc));
    return hrc;
}

</xsl:text>
    <xsl:if test="not(@readonly) or @readonly!='yes'">
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
        </xsl:apply-templates>
        <xsl:text>\n", this, </xsl:text>
        <xsl:value-of select="concat('&quot;', $topclass, '::set', $attrbasename, '&quot;, ')"/>
        <xsl:apply-templates select="@type" mode="logparamval">
            <xsl:with-param name="dir" select="'in'"/>
        </xsl:apply-templates>
        <xsl:text>));

    VirtualBoxBase::clearError();

    HRESULT hrc;

    try
    {
        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc()))
            throw autoCaller.rc();

</xsl:text>
        <xsl:value-of select="concat('        hrc = set', $attrbasename, '(')"/>
        <xsl:if test="$passAutoCaller = 'true'">
            <xsl:text>autoCaller, </xsl:text>
        </xsl:if>
        <xsl:apply-templates select="@type" mode="paramvalconversion">
            <xsl:with-param name="dir" select="'in'"/>
        </xsl:apply-templates>
        <xsl:text>);
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    LogRelFlow(("{%p} %s: leave hrc=%Rhrc\n", this, </xsl:text>
        <xsl:value-of select="concat('&quot;', $topclass, '::set', $attrbasename, '&quot;, ')"/>
        <xsl:text>hrc));
    return hrc;
}

</xsl:text>
    </xsl:if>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit all attributes of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitAttributes">
    <xsl:param name="iface"/>
    <xsl:param name="topclass"/>
    <xsl:param name="pmode"/>

    <!-- first recurse to emit all base interfaces -->
    <xsl:variable name="extends" select="$iface/@extends"/>
    <xsl:if test="$extends and not($extends='$unknown') and not($extends='$dispatched') and not($extends='$errorinfo')">
        <xsl:call-template name="emitAttributes">
            <xsl:with-param name="iface" select="//interface[@name=$extends]"/>
            <xsl:with-param name="topclass" select="$topclass"/>
            <xsl:with-param name="pmode" select="$pmode"/>
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
            <xsl:apply-templates select="$iface/attribute" mode="public"/>
        </xsl:when>
        <xsl:when test="$pmode='wrapped'">
            <xsl:apply-templates select="$iface/attribute" mode="wrapped"/>
        </xsl:when>
        <xsl:when test="$pmode='code'">
            <xsl:apply-templates select="$iface/attribute" mode="code">
                <xsl:with-param name="topclass" select="$topclass"/>
            </xsl:apply-templates>
        </xsl:when>
        <xsl:otherwise/>
    </xsl:choose>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit method
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="method" mode="public">
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
</xsl:template>

<xsl:template match="method" mode="wrapped">
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
</xsl:template>

<xsl:template match="method" mode="code">
    <xsl:param name="topclass"/>

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
            <xsl:with-param name="mod" select="'ref'"/>
        </xsl:apply-templates>
    </xsl:for-each>
    <xsl:text>\n", this</xsl:text>
    <xsl:value-of select="concat(', &quot;', $topclass, '::', @name, '&quot;')"/>
    <xsl:for-each select="param">
        <xsl:text>, </xsl:text>
        <xsl:apply-templates select="@type" mode="logparamval">
            <xsl:with-param name="dir" select="@dir"/>
            <xsl:with-param name="mod" select="'ref'"/>
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
        AutoCaller autoCaller(this);
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
        <xsl:apply-templates select="@type" mode="paramvalconversion">
            <xsl:with-param name="dir" select="@dir"/>
        </xsl:apply-templates>
        <xsl:if test="not(position()=last())">
            <xsl:text>,
               </xsl:text>
            <xsl:value-of select="$methodindent"/>
        </xsl:if>
    </xsl:for-each>
    <xsl:text>);
    }
    catch (HRESULT hrc2)
    {
        hrc = hrc2;
    }
    catch (...)
    {
        hrc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    LogRelFlow(("{%p} %s: leave</xsl:text>
    <xsl:for-each select="param">
        <xsl:if test="@dir!='in'">
            <xsl:text> </xsl:text>
            <xsl:apply-templates select="@type" mode="logparamtext">
                <xsl:with-param name="dir" select="@dir"/>
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
            </xsl:apply-templates>
        </xsl:if>
    </xsl:for-each>
    <xsl:text>, hrc));
    return hrc;
}

</xsl:text>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  emit all methods of an interface
  - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="emitMethods">
    <xsl:param name="iface"/>
    <xsl:param name="topclass"/>
    <xsl:param name="pmode"/>

    <!-- first recurse to emit all base interfaces -->
    <xsl:variable name="extends" select="$iface/@extends"/>
    <xsl:if test="$extends and not($extends='$unknown') and not($extends='$dispatched') and not($extends='$errorinfo')">
        <xsl:call-template name="emitMethods">
            <xsl:with-param name="iface" select="//interface[@name=$extends]"/>
            <xsl:with-param name="topclass" select="$topclass"/>
            <xsl:with-param name="pmode" select="$pmode"/>
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
            <xsl:apply-templates select="$iface/method" mode="public"/>
        </xsl:when>
        <xsl:when test="$pmode='wrapped'">
            <xsl:apply-templates select="$iface/method" mode="wrapped"/>
        </xsl:when>
        <xsl:when test="$pmode='code'">
            <xsl:apply-templates select="$iface/method" mode="code">
                <xsl:with-param name="topclass" select="$topclass"/>
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
            <xsl:apply-templates select="." mode="classheader"/>

            <!-- interface attributes/methods (public) -->
            <xsl:call-template name="emitInterfaceDecls">
                <xsl:with-param name="iface" select="$iface"/>
                <xsl:with-param name="pmode" select="'public'"/>
            </xsl:call-template>

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

            <xsl:apply-templates select="." mode="classfooter"/>
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

    <xsl:value-of select="concat('DEFINE_EMPTY_CTOR_DTOR(', substring($iface/@name, 2), 'Wrap)&#10;&#10;')"/>

    <!-- attributes -->
    <xsl:call-template name="emitAttributes">
        <xsl:with-param name="iface" select="$iface"/>
        <xsl:with-param name="topclass" select="substring($iface/@name, 2)"/>
        <xsl:with-param name="pmode" select="'code'"/>
    </xsl:call-template>

    <!-- methods -->
    <xsl:call-template name="emitMethods">
        <xsl:with-param name="iface" select="$iface"/>
        <xsl:with-param name="topclass" select="substring($iface/@name, 2)"/>
        <xsl:with-param name="pmode" select="'code'"/>
    </xsl:call-template>
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
            <xsl:apply-templates select="." mode="codeheader"/>

            <!-- @todo special thread logging for API methods returning IProgress??? would be very usefulcurrently nothing, maybe later some generic FinalConstruct/... implementation -->

            <!-- interface attributes/methods (public) -->
            <xsl:call-template name="emitInterfaceDefs">
                <xsl:with-param name="iface" select="$iface"/>
            </xsl:call-template>

            <!-- auxiliary methods (public) -->
            <xsl:call-template name="emitAuxMethodDefs">
                <xsl:with-param name="iface" select="$iface"/>
            </xsl:call-template>

            <xsl:apply-templates select="." mode="codefooter"/>
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
  library match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="interface">
    <xsl:if test="not(@supportsErrorInfo='no')">
        <xsl:call-template name="emitHeader">
            <xsl:with-param name="iface" select="."/>
        </xsl:call-template>

        <xsl:call-template name="emitCode">
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
