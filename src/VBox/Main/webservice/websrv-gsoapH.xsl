<?xml version="1.0"?>

<!--
    websrv-gsoapH.xsl:
        XSLT stylesheet that generates vboxweb_gsoapH.h from
        the WSDL file previously generated from VirtualBox.xidl.
        The gsoap.h "header" file can then be fed into gSOAP's
        soapcpp2 to create web service client headers and server
        stubs.

        (The reason for this appears to be that gSOAP predates
        WSDL and thus needed some format to describe the syntax
        of a web service. gSOAP these days comes with wsdl2h,
        which converts a WSDL file to gSOAP's "header" format,
        but that has license problems and so we generate the
        gSOAP "header" ourselves via XSLT.)

     Copyright (C) 2006-2008 Sun Microsystems, Inc.

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

<xsl:stylesheet
  version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema">

  <xsl:param name="G_argDebug" />

  <xsl:output method="text"/>

  <xsl:strip-space elements="*"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:variable name="G_xsltFilename" select="'websrv-gsoapH.xsl'" />

<xsl:include href="websrv-shared.inc.xsl" />

<!-- collect all interfaces with "wsmap='suppress'" in a global variable for
     quick lookup -->
<xsl:variable name="G_setSuppressedInterfaces"
              select="//interface[@wsmap='suppress']" />

<!--
    emitConvertedType:
    first type converter (from XIDL type to SOAP/C++ input types),
    used for generating the argument lists with method implementation
    headers.
    -->
<xsl:template name="emitConvertedType">
  <xsl:param name="type" />

  <xsl:choose>
    <xsl:when test="$type='wstring'">std::string</xsl:when>
    <xsl:when test="$type='boolean'">bool</xsl:when>
    <xsl:when test="$type='double'">double</xsl:when>
    <xsl:when test="$type='float'">float</xsl:when>
    <!-- <xsl:when test="$type='octet'">byte</xsl:when> -->
    <xsl:when test="$type='short'">short</xsl:when>
    <xsl:when test="$type='unsigned short'">unsigned short</xsl:when>
    <xsl:when test="$type='long'">long</xsl:when>
    <xsl:when test="$type='long long'">long long</xsl:when>
    <xsl:when test="$type='unsigned long'">unsigned long</xsl:when>
    <xsl:when test="$type='unsigned long long'">unsigned long long</xsl:when>
    <xsl:when test="$type='result'">unsigned long</xsl:when>
    <xsl:when test="$type='uuid'">std::string</xsl:when>
    <xsl:when test="$type='global'"><xsl:value-of select="$G_typeObjectRef_gsoapH" /></xsl:when>
    <xsl:when test="$type='managed'"><xsl:value-of select="$G_typeObjectRef_gsoapH" /></xsl:when>
    <xsl:when test="$type='explicit'"><xsl:value-of select="$G_typeObjectRef_gsoapH" /></xsl:when>
    <!-- not a standard type: then it better be one of the types defined in the XIDL -->
    <xsl:when test="//enum[@name=$type]">
      <xsl:value-of select="concat('enum vbox__', $type)" />
    </xsl:when>
    <xsl:when test="//collection[@name=$type]">
      <xsl:value-of select="concat('vbox__ArrayOf', //collection[@name=$type]/@type, '*')" />
    </xsl:when>
    <xsl:when test="//interface[@name=$type]">
      <!-- the type is one of our own interfaces: then it must have a wsmap attr -->
      <xsl:variable name="wsmap" select="(//interface[@name=$type]/@wsmap) | (//collection[@name=$type]/@wsmap)" />
      <xsl:choose>
        <xsl:when test="$wsmap='global'"><xsl:value-of select="$G_typeObjectRef_gsoapH" /></xsl:when>
        <xsl:when test="$wsmap='managed'"><xsl:value-of select="$G_typeObjectRef_gsoapH" /></xsl:when>
        <xsl:when test="$wsmap='explicit'"><xsl:value-of select="$G_typeObjectRef_gsoapH" /></xsl:when>
        <xsl:when test="$wsmap='struct'"><xsl:value-of select="concat('vbox__', $type, '*')" /></xsl:when>
      </xsl:choose>
    </xsl:when>
  </xsl:choose>
</xsl:template>

<xsl:template name="convertTypeAndEmitPartOrElement">
  <xsl:param name="ifname" />
  <xsl:param name="methodname" />
  <xsl:param name="name" />
  <xsl:param name="type" />

  <xsl:call-template name="debugMsg"><xsl:with-param name="msg" select="concat('....', $type, ' ', $name)" /></xsl:call-template>

  <xsl:value-of select="concat('    ', '')"/>
  <xsl:call-template name="emitConvertedType">
    <xsl:with-param name="ifname" select="$ifname" />
    <xsl:with-param name="methodname" select="$methodname" />
    <xsl:with-param name="type" select="$type" />
  </xsl:call-template>
  <xsl:value-of select="concat(' ', $name, ' 1;')"/>
  <xsl:call-template name="emitNewline" />
</xsl:template>

<xsl:template name="emitRequestArgs">
  <xsl:param name="_ifname" />          <!-- interface name -->
  <xsl:param name="_wsmap" />           <!-- interface's wsmap attribute -->
  <xsl:param name="_methodname" />
  <xsl:param name="_params" />
  <xsl:param name="_valuetype" />       <!-- optional, for attribute setter messages -->

  <xsl:call-template name="debugMsg"><xsl:with-param name="msg" select="concat('..', $_ifname, '::', $_methodname, ': ', 'emitRequestArgs')" /></xsl:call-template>
  <!-- first parameter will be object on which method is called, depending on wsmap attribute -->
  <xsl:choose>
    <xsl:when test="($_wsmap='managed') or ($_wsmap='explicit')">
      <xsl:call-template name="convertTypeAndEmitPartOrElement">
        <xsl:with-param name="ifname" select="$_ifname" />
        <xsl:with-param name="methodname" select="$_methodname" />
        <xsl:with-param name="name" select="$G_nameObjectRefEncoded" />
        <xsl:with-param name="type" select="$_wsmap" />
      </xsl:call-template>
    </xsl:when>
  </xsl:choose>
  <!-- now for the real parameters, if any -->
  <xsl:for-each select="$_params">
    <!-- <xsl:value-of select="concat('// param &quot;', @name, '&quot;: direction &quot;', @dir, '&quot;')" />
    <xsl:call-template name="emitNewline" />  -->
    <!-- emit only parts for "in" parameters -->
    <xsl:if test="@dir='in'">
      <xsl:call-template name="convertTypeAndEmitPartOrElement">
        <xsl:with-param name="ifname" select="$_ifname" />
        <xsl:with-param name="methodname" select="$_methodname" />
        <xsl:with-param name="name" select="@name" />
        <xsl:with-param name="type" select="@type" />
      </xsl:call-template>
    </xsl:if>
  </xsl:for-each>
  <xsl:if test="$_valuetype">
    <xsl:call-template name="convertTypeAndEmitPartOrElement">
      <xsl:with-param name="ifname" select="$_ifname" />
      <xsl:with-param name="methodname" select="$_methodname" />
      <xsl:with-param name="name" select="@name" />
      <xsl:with-param name="type" select="@type" />
    </xsl:call-template>
  </xsl:if>
</xsl:template>

<xsl:template name="emitResultArgs">
  <xsl:param name="_ifname" />
  <xsl:param name="_methodname" />
  <xsl:param name="_params" />          <!-- set of parameter elements -->
  <xsl:param name="_resulttype" />      <!-- for attribute getter methods only -->

  <xsl:call-template name="debugMsg"><xsl:with-param name="msg" select="concat('..', $_ifname, '::', $_methodname, ': ', 'emitResultArgs')" /></xsl:call-template>
  <xsl:choose>
    <xsl:when test="$_resulttype">
      <xsl:call-template name="convertTypeAndEmitPartOrElement">
        <xsl:with-param name="ifname" select="$_ifname" />
        <xsl:with-param name="methodname" select="$_methodname" />
        <xsl:with-param name="name" select="$G_result" />
        <xsl:with-param name="type" select="$_resulttype" />
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:for-each select="$_params">
        <!-- emit only parts for "out" parameters -->
        <xsl:if test="@dir='out'">
          <xsl:call-template name="convertTypeAndEmitPartOrElement">
            <xsl:with-param name="ifname" select="$_ifname" />
            <xsl:with-param name="methodname" select="$_methodname" />
            <xsl:with-param name="name"><xsl:value-of select="@name" /></xsl:with-param>
            <xsl:with-param name="type"><xsl:value-of select="@type" /></xsl:with-param>
          </xsl:call-template>
        </xsl:if>
        <xsl:if test="@dir='return'">
          <xsl:call-template name="convertTypeAndEmitPartOrElement">
            <xsl:with-param name="ifname" select="$_ifname" />
            <xsl:with-param name="methodname" select="$_methodname" />
            <xsl:with-param name="name"><xsl:value-of select="$G_result" /></xsl:with-param>
            <xsl:with-param name="type"><xsl:value-of select="@type" /></xsl:with-param>
          </xsl:call-template>
        </xsl:if>
      </xsl:for-each>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - -
  root match
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="/idl">
  <xsl:text><![CDATA[
/* DO NOT EDIT! This is a generated file.
 * Generated from: src/VBox/Main/idl/VirtualBox.xidl (VirtualBox's interface definitions in XML)
 * Generator: src/VBox/Main/webservice/websrv-gsoapH.xsl
 *
 * Note: This is not a real C/C++ header file. Instead, gSOAP uses files like this
 * one -- with a pseudo-C-header syntax -- to describe a web service API.
 */

// STL vector containers
#import "stlvector.h"

]]></xsl:text>

    <xsl:value-of select="concat('//gsoap vbox  schema namespace: ', $G_targetNamespace)" />
    <xsl:value-of select="concat('//gsoap vbox  schema form:  unqualified', '')" />

  <xsl:apply-templates />
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  if
 - - - - - - - - - - - - - - - - - - - - - - -->

<!--
 *  ignore all |if|s except those for WSDL target
-->
<xsl:template match="if">
    <xsl:if test="@target='wsdl'">
        <xsl:apply-templates/>
    </xsl:if>
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  cpp
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="cpp">
<!--  ignore this -->
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  library
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="library">
  <xsl:text>
/****************************************************************************
 *
 * forward declarations
 *
 ****************************************************************************/
</xsl:text>

  <xsl:call-template name="emitNewline" />
  <xsl:text>// *** interfaces with wsmap="struct"</xsl:text>
  <xsl:call-template name="emitNewline" />

  <xsl:for-each select="//interface[@wsmap='struct']">
    <xsl:value-of select="concat('class vbox__', @name, ';')" />
    <xsl:call-template name="emitNewline" />
  </xsl:for-each>

  <xsl:call-template name="emitNewline" />
  <xsl:text>// *** collections</xsl:text>
  <xsl:call-template name="emitNewline" />

  <xsl:for-each select="//collection">
    <xsl:value-of select="concat('class vbox__ArrayOf', @type, ';')" />
    <xsl:call-template name="emitNewline" />
  </xsl:for-each>

  <xsl:if test="$G_basefmt='document'">
    <xsl:call-template name="emitNewline" />
    <xsl:text>// elements for message arguments (parts)</xsl:text>
    <xsl:call-template name="emitNewline" />

    <xsl:for-each select="//interface">
      <xsl:variable name="ifname"><xsl:value-of select="@name" /></xsl:variable>
      <xsl:variable name="wsmap"><xsl:value-of select="@wsmap" /></xsl:variable>

      <xsl:if test='not( ($wsmap="suppress") or ($wsmap="struct") )'>
        <xsl:value-of select="concat('// Interface ', $ifname)" />
        <xsl:call-template name="emitNewline" />

        <xsl:for-each select="attribute">
          <xsl:variable name="attrname"><xsl:value-of select="@name" /></xsl:variable>
          <xsl:variable name="attrtype"><xsl:value-of select="@type" /></xsl:variable>
          <xsl:variable name="attrreadonly"><xsl:value-of select="@readonly" /></xsl:variable>
          <xsl:choose>
            <xsl:when test="( $attrtype=($G_setSuppressedInterfaces/@name) )">
              <xsl:value-of select="concat('// skipping attribute ', $attrtype, ' for it is of a suppressed type')" />
              <xsl:call-template name="emitNewline" />
            </xsl:when>
            <xsl:otherwise>
              <xsl:choose>
                <xsl:when test="@readonly='yes'">
                  <xsl:value-of select="concat('// readonly attribute ', $ifname, '::', $attrname)" />
                </xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="concat('// read/write attribute ', $ifname, '::', $attrname)" />
                </xsl:otherwise>
              </xsl:choose>
              <xsl:call-template name="emitNewline" />
              <!-- aa) emit getter -->
              <xsl:variable name="attrGetter"><xsl:call-template name="makeGetterName"><xsl:with-param name="attrname" select="$attrname" /></xsl:call-template></xsl:variable>
              <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $attrGetter, $G_requestMessageElementSuffix, ';')" />
              <xsl:call-template name="emitNewline" />
              <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $attrGetter, $G_responseMessageElementSuffix, ';')" />
              <xsl:call-template name="emitNewline" />
              <!-- bb) emit setter if the attribute is read/write -->
              <xsl:if test="not($attrreadonly='yes')">
                <xsl:variable name="attrSetter"><xsl:call-template name="makeSetterName"><xsl:with-param name="attrname" select="$attrname" /></xsl:call-template></xsl:variable>
                <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $attrSetter, $G_requestMessageElementSuffix, ';')" />
                <xsl:call-template name="emitNewline" />
                <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $attrSetter, $G_responseMessageElementSuffix, ';')" />
                <xsl:call-template name="emitNewline" />
              </xsl:if>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:for-each> <!-- select="attribute" -->
        <xsl:for-each select="method">
          <xsl:variable name="methodname"><xsl:value-of select="@name" /></xsl:variable>
          <!-- skip this method if it has parameters of a type that has wsmap="suppress" -->
          <xsl:choose>
            <xsl:when test="param[@type=($G_setSuppressedInterfaces/@name)]">
              <xsl:value-of select="concat('// skipping method ', $ifname, '::', $methodname, ' for it has parameters with suppressed types')" />
              <xsl:call-template name="emitNewline" />
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="concat('// method ', $ifname, '::', $methodname)" />
              <xsl:call-template name="emitNewline" />
              <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $methodname, $G_requestMessageElementSuffix, ';')" />
              <xsl:call-template name="emitNewline" />
              <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $methodname, $G_responseMessageElementSuffix, ';')" />
              <xsl:call-template name="emitNewline" />
            </xsl:otherwise>
          </xsl:choose>
        </xsl:for-each>
      </xsl:if>
    </xsl:for-each>
  </xsl:if> <!-- document style -->

  <xsl:text>
/****************************************************************************
 *
 * faults
 *
 ****************************************************************************/
  </xsl:text>

  <xsl:text>
class _vbox__InvalidObjectFault
{ public:
    std::string badObjectID  1;
    struct soap *soap;
};

class _vbox__RuntimeFault
{ public:
    long resultCode 1;
    std::string interfaceID 1;
    std::string component 1;
    std::string text 1;
    struct soap *soap;
};

struct SOAP_ENV__Detail
{
    _vbox__InvalidObjectFault *vbox__InvalidObjectFault;
    _vbox__RuntimeFault *vbox__RuntimeFault;
    int __type;
    void *fault;
    _XML __any;
};</xsl:text>
  <xsl:call-template name="emitNewline" />

  <xsl:text>
/****************************************************************************
 *
 * enums
 *
 ****************************************************************************/
</xsl:text>

  <xsl:for-each select="//enum">
    <xsl:call-template name="emitNewline" />
    <xsl:variable name="enumname" select="@name" />
    <xsl:value-of select="concat('enum vbox__', $enumname)" />
    <xsl:call-template name="emitNewline" />
    <xsl:text>{</xsl:text>
    <xsl:call-template name="emitNewline" />
    <xsl:for-each select="const">
      <xsl:if test="position() > 1">
        <xsl:text>,</xsl:text>
        <xsl:call-template name="emitNewline" />
      </xsl:if>
      <xsl:value-of select="concat('    vbox__', $enumname, '__')" />
      <!-- escape all "_" in @name -->
      <xsl:call-template name="escapeUnderscores">
        <xsl:with-param name="string" select="@name" />
      </xsl:call-template>
    </xsl:for-each>
    <xsl:call-template name="emitNewline" />
    <xsl:text>};</xsl:text>
    <xsl:call-template name="emitNewline" />
  </xsl:for-each>

  <xsl:text>
/****************************************************************************
 *
 * structs
 *
 ****************************************************************************/
</xsl:text>

  <xsl:for-each select="//interface[@wsmap='struct']">
    <xsl:call-template name="emitNewline" />
    <xsl:value-of select="concat('// interface ', @name, ' as struct: ')" />
    <xsl:call-template name="emitNewline" />

    <xsl:value-of select="concat('class vbox__', @name)" />
    <xsl:call-template name="emitNewline" />
    <xsl:text>{ public:</xsl:text>
    <xsl:call-template name="emitNewline" />

    <xsl:for-each select="attribute">
      <xsl:text>    </xsl:text>
      <xsl:call-template name="emitConvertedType"><xsl:with-param name="type" select="@type" /></xsl:call-template>
      <xsl:value-of select="concat(' ', @name, ' 1;')" />
      <xsl:call-template name="emitNewline" />
    </xsl:for-each>

    <xsl:text>    struct soap *soap;</xsl:text>
    <xsl:call-template name="emitNewline" />
    <xsl:text>};</xsl:text>
    <xsl:call-template name="emitNewline" />
  </xsl:for-each>

  <xsl:text>
/****************************************************************************
 *
 * arrays
 *
 ****************************************************************************/
</xsl:text>

  <xsl:for-each select="//collection">
    <xsl:call-template name="emitNewline" />
    <xsl:value-of select="concat('class vbox__ArrayOf', @type)" />
    <xsl:call-template name="emitNewline" />
    <xsl:text>{ public:</xsl:text>
    <xsl:call-template name="emitNewline" />
    <xsl:text>    std::vector&lt;</xsl:text>
    <xsl:call-template name="emitConvertedType"><xsl:with-param name="type" select="@type" /></xsl:call-template>
    <xsl:text>&gt; array 0;</xsl:text>
    <xsl:call-template name="emitNewline" />

    <xsl:text>    struct soap *soap;</xsl:text>
    <xsl:call-template name="emitNewline" />
    <xsl:text>};</xsl:text>
    <xsl:call-template name="emitNewline" />
  </xsl:for-each>


  <xsl:if test="$G_basefmt='document'">
    <xsl:text>
/****************************************************************************
 *
 * elements for message arguments (parts); generated for WSDL 'document' style
 *
 ****************************************************************************/

</xsl:text>

    <xsl:for-each select="//interface">
      <xsl:variable name="ifname"><xsl:value-of select="@name" /></xsl:variable>
      <xsl:variable name="wsmap"><xsl:value-of select="@wsmap" /></xsl:variable>

      <xsl:if test='not( ($wsmap="suppress") or ($wsmap="struct") )'>
        <xsl:call-template name="emitNewline" />
        <xsl:value-of select="concat('// Interface ', $ifname)" />
        <xsl:call-template name="emitNewline" />

        <xsl:call-template name="debugMsg"><xsl:with-param name="msg" select="concat($ifname, ' interface: ', 'all attributes')" /></xsl:call-template>
        <xsl:for-each select="attribute">
          <xsl:variable name="attrname"><xsl:value-of select="@name" /></xsl:variable>
          <xsl:variable name="attrtype"><xsl:value-of select="@type" /></xsl:variable>
          <xsl:variable name="attrreadonly"><xsl:value-of select="@readonly" /></xsl:variable>
          <xsl:call-template name="emitNewline" />
          <xsl:choose>
            <xsl:when test="( $attrtype=($G_setSuppressedInterfaces/@name) )">
              <xsl:value-of select="concat('// skipping attribute ', $attrtype, ' for it is of a suppressed type')" />
              <xsl:call-template name="emitNewline" />
            </xsl:when>
            <xsl:otherwise>
              <xsl:choose>
                <xsl:when test="@readonly='yes'">
                  <xsl:value-of select="concat('// readonly attribute ', $ifname, '::', $attrname)" />
                </xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="concat('// read/write attribute ', $ifname, '::', $attrname)" />
                </xsl:otherwise>
              </xsl:choose>
              <xsl:call-template name="emitNewline" />
              <!-- aa) emit getter -->
              <xsl:variable name="attrGetter"><xsl:call-template name="makeGetterName"><xsl:with-param name="attrname" select="$attrname" /></xsl:call-template></xsl:variable>
              <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $attrGetter, $G_requestMessageElementSuffix)" />
              <xsl:call-template name="emitNewline" />
              <xsl:text>{ public:</xsl:text>
              <xsl:call-template name="emitNewline" />
              <xsl:call-template name="emitRequestArgs">
                <xsl:with-param name="_ifname" select="$ifname" />
                <xsl:with-param name="_wsmap" select="$wsmap" />
                <xsl:with-param name="_methodname" select="$attrGetter" />
                <xsl:with-param name="_params" select="/.." />
                <!-- <xsl:with-param name="_valuetype" select="$attrtype" /> -->
              </xsl:call-template>
              <xsl:text>    struct soap *soap;</xsl:text>
              <xsl:call-template name="emitNewline" />
              <xsl:text>};</xsl:text>
              <xsl:call-template name="emitNewline" />

              <xsl:call-template name="emitNewline" />
              <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $attrGetter, $G_responseMessageElementSuffix)" />
              <xsl:call-template name="emitNewline" />
              <xsl:text>{ public:</xsl:text>
              <xsl:call-template name="emitNewline" />
              <xsl:call-template name="emitResultArgs">
                <xsl:with-param name="_ifname" select="$ifname" />
                <xsl:with-param name="_methodname" select="$attrGetter" />
                <xsl:with-param name="_params" select="/.." />
                <xsl:with-param name="_resulttype" select="$attrtype" />
              </xsl:call-template>
              <xsl:text>    struct soap *soap;</xsl:text>
              <xsl:call-template name="emitNewline" />
              <xsl:text>};</xsl:text>
              <xsl:call-template name="emitNewline" />
              <!-- bb) emit setter if the attribute is read/write -->
              <xsl:if test="not($attrreadonly='yes')">
                <xsl:variable name="attrSetter"><xsl:call-template name="makeSetterName"><xsl:with-param name="attrname" select="$attrname" /></xsl:call-template></xsl:variable>
                <xsl:call-template name="emitNewline" />
                <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $attrSetter, $G_requestMessageElementSuffix)" />
                <xsl:call-template name="emitNewline" />
                <xsl:text>{ public:</xsl:text>
                <xsl:call-template name="emitRequestArgs">
                  <xsl:with-param name="_ifname" select="$ifname" />
                  <xsl:with-param name="_wsmap" select="$wsmap" />
                  <xsl:with-param name="_methodname" select="$attrSetter" />
                  <xsl:with-param name="_params" select="/.." />
                  <xsl:with-param name="_valuetype" select="$attrtype" />
                </xsl:call-template>
                <xsl:call-template name="emitNewline" />
                <xsl:text>    struct soap *soap;</xsl:text>
                <xsl:call-template name="emitNewline" />
                <xsl:text>};</xsl:text>
                <xsl:call-template name="emitNewline" />
                <xsl:call-template name="emitNewline" />
                <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $attrSetter, $G_responseMessageElementSuffix)" />
                <xsl:call-template name="emitNewline" />
                <xsl:text>{ public:</xsl:text>
                <xsl:call-template name="emitNewline" />
                <xsl:call-template name="emitResultArgs">
                  <xsl:with-param name="_ifname" select="$ifname" />
                  <xsl:with-param name="_methodname" select="$attrSetter" />
                  <xsl:with-param name="_params" select="/.." />
                </xsl:call-template>
                <xsl:text>    struct soap *soap;</xsl:text>
                <xsl:call-template name="emitNewline" />
                <xsl:text>};</xsl:text>
                <xsl:call-template name="emitNewline" />
              </xsl:if>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:for-each> <!-- select="attribute" -->

        <xsl:call-template name="debugMsg"><xsl:with-param name="msg" select="concat($ifname, ' interface: ', 'all methods')" /></xsl:call-template>
        <xsl:for-each select="method">
          <xsl:variable name="methodname"><xsl:value-of select="@name" /></xsl:variable>
          <xsl:call-template name="emitNewline" />
          <!-- skip this method if it has parameters of a type that has wsmap="suppress" -->
          <xsl:choose>
            <xsl:when test="param[@type=($G_setSuppressedInterfaces/@name)]">
              <xsl:value-of select="concat('// skipping method ', $ifname, '::', $methodname, ' for it has parameters with suppressed types')" />
              <xsl:call-template name="emitNewline" />
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="concat('// method ', $ifname, '::', $methodname)" />
              <xsl:call-template name="emitNewline" />
              <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $methodname, $G_requestMessageElementSuffix)" />
              <xsl:call-template name="emitNewline" />
              <xsl:text>{ public:</xsl:text>
              <xsl:call-template name="emitNewline" />
              <xsl:call-template name="emitRequestArgs">
                <xsl:with-param name="_ifname" select="$ifname" />
                <xsl:with-param name="_wsmap" select="$wsmap" />
                <xsl:with-param name="_methodname" select="$methodname" />
                <xsl:with-param name="_params" select="param" />
              </xsl:call-template>
              <xsl:text>    struct soap *soap;</xsl:text>
              <xsl:call-template name="emitNewline" />
              <xsl:text>};</xsl:text>
              <xsl:call-template name="emitNewline" />
              <!-- <xsl:if test="(count(param[@dir='out'] | param[@dir='return']) > 0)"> -->
                <xsl:call-template name="emitNewline" />
                <xsl:value-of select="concat('class _vbox__', $ifname, '_USCORE', $methodname, $G_responseMessageElementSuffix)" />
                <xsl:call-template name="emitNewline" />
                <xsl:text>{ public:</xsl:text>
                <xsl:call-template name="emitNewline" />
                <xsl:call-template name="emitResultArgs">
                  <xsl:with-param name="_ifname" select="$ifname" />
                  <xsl:with-param name="_wsmap" select="$wsmap" />
                  <xsl:with-param name="_methodname" select="$methodname" />
                  <xsl:with-param name="_params" select="param" />
                </xsl:call-template>
                <xsl:text>    struct soap *soap;</xsl:text>
                <xsl:call-template name="emitNewline" />
                <xsl:text>};</xsl:text>
                <xsl:call-template name="emitNewline" />
              <!-- </xsl:if> -->
            </xsl:otherwise>
          </xsl:choose>
        </xsl:for-each>
      </xsl:if>
    </xsl:for-each> <!-- interface (element declarations -->

    <xsl:text>
/****************************************************************************
 *
 * service descriptions
 *
 ****************************************************************************/

</xsl:text>

    <xsl:value-of select="concat('//gsoap vbox service name: vbox', $G_bindingSuffix)" />
    <xsl:call-template name="emitNewline" />
    <xsl:value-of select="concat('//gsoap vbox service type: vbox', $G_portTypeSuffix)" />
    <xsl:call-template name="emitNewline" />
    <xsl:value-of select="concat('//gsoap vbox service namespace: ', $G_targetNamespace, $G_targetNamespaceSeparator)" />
    <xsl:call-template name="emitNewline" />
    <xsl:value-of select="concat('//gsoap vbox service transport: ', 'http://schemas.xmlsoap.org/soap/http')" />
    <xsl:call-template name="emitNewline" />

  </xsl:if> <!-- document style -->

  <xsl:apply-templates />

</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  class
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="module/class">
<!--  swallow -->
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  enum
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="enum">
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  const
 - - - - - - - - - - - - - - - - - - - - - - -->

<!--
<xsl:template match="const">
  <xsl:apply-templates />
</xsl:template>
-->

<!-- - - - - - - - - - - - - - - - - - - - - - -
  desc
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="desc">
<!--  swallow -->
</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  note
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="note">
  <xsl:apply-templates />
</xsl:template>

<xsl:template name="emitMethod">
  <xsl:param name="ifname" />
  <xsl:param name="methodname" />
  <xsl:param name="fOutParams" />

  <xsl:variable name="methodname2" select="concat($ifname, '_USCORE', $methodname)" />

  <xsl:call-template name="emitNewline" />
  <xsl:value-of select="concat('//gsoap vbox service method-style:    ', $methodname2, ' ', $G_basefmt)" />
  <xsl:call-template name="emitNewline" />
  <xsl:value-of select="concat('//gsoap vbox service method-encoding: ', $methodname2, ' ', $G_parmfmt)" />
  <xsl:call-template name="emitNewline" />
  <xsl:value-of select="concat('//gsoap vbox service method-action:   ', $methodname2, ' &quot;&quot;')" />
  <xsl:call-template name="emitNewline" />
  <xsl:value-of select="concat('//gsoap vbox service method-fault:    ', $methodname2, ' vbox__InvalidObjectFault')" />
  <xsl:call-template name="emitNewline" />
  <xsl:value-of select="concat('//gsoap vbox service method-fault:    ', $methodname2, ' vbox__RuntimeFault')" />
  <xsl:call-template name="emitNewline" />
  <xsl:value-of select="concat('int __vbox__', $methodname2, '(')" />
  <xsl:call-template name="emitNewline" />
  <xsl:value-of select="concat('    _vbox__', $methodname2, $G_requestMessageElementSuffix, '* vbox__', $ifname, '_USCORE', $methodname, $G_requestMessageElementSuffix)" />
  <xsl:if test="$fOutParams">
    <xsl:text>,</xsl:text>
    <xsl:call-template name="emitNewline" />
    <xsl:value-of select="concat('    _vbox__', $methodname2, $G_responseMessageElementSuffix, '* vbox__', $ifname, '_USCORE', $methodname, $G_responseMessageElementSuffix)" />
  </xsl:if>
  <xsl:call-template name="emitNewline" />
  <xsl:text>);</xsl:text>
  <xsl:call-template name="emitNewline" />

</xsl:template>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  interface
  - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:template match="interface">
  <!-- remember the interface name in local variables -->
  <xsl:variable name="ifname"><xsl:value-of select="@name" /></xsl:variable>
  <xsl:variable name="wsmap"><xsl:value-of select="@wsmap" /></xsl:variable>

  <!-- we can save ourselves verifying the interface here as it's already
       done in the WSDL converter -->

  <xsl:if test='not( ($wsmap="suppress") or ($wsmap="struct") )'>
    <xsl:text>
/****************************************************************************
 *
 * interface </xsl:text>
<xsl:value-of select="$ifname" />
<xsl:text>
 *
 ****************************************************************************/
</xsl:text>

    <!--
      here come the attributes
    -->
    <xsl:for-each select="attribute">
      <xsl:variable name="attrname"><xsl:value-of select="@name" /></xsl:variable>
      <xsl:variable name="attrtype"><xsl:value-of select="@type" /></xsl:variable>
      <xsl:variable name="attrreadonly"><xsl:value-of select="@readonly" /></xsl:variable>
      <xsl:call-template name="debugMsg"><xsl:with-param name="msg" select="concat('messages for ', $ifname, '::', $attrname, ': attribute of type &quot;', $attrtype, '&quot;, readonly: ', $attrreadonly)" /></xsl:call-template>
      <!-- skip this attribute if it has parameters of a type that has wsmap="suppress" -->
      <xsl:choose>
        <xsl:when test="( $attrtype=($G_setSuppressedInterfaces/@name) )">
          <xsl:comment><xsl:value-of select="concat('skipping attribute ', $attrtype, ' for it is of a suppressed type')" /></xsl:comment>
        </xsl:when>
        <xsl:otherwise>
          <xsl:choose>
            <xsl:when test="@readonly='yes'">
              <xsl:comment> readonly attribute <xsl:copy-of select="$ifname" />::<xsl:copy-of select="$attrname" /> </xsl:comment>
            </xsl:when>
            <xsl:otherwise>
              <xsl:comment> read/write attribute <xsl:copy-of select="$ifname" />::<xsl:copy-of select="$attrname" /> </xsl:comment>
            </xsl:otherwise>
          </xsl:choose>
          <!-- aa) get method: emit request and result -->
          <xsl:variable name="attrGetter"><xsl:call-template name="makeGetterName"><xsl:with-param name="attrname" select="$attrname" /></xsl:call-template></xsl:variable>
          <xsl:call-template name="emitMethod">
            <xsl:with-param name="ifname" select="$ifname" />
            <xsl:with-param name="methodname" select="$attrGetter" />
            <xsl:with-param name="fOutParams" select="1" />
          </xsl:call-template>
          <!-- bb) emit a set method if the attribute is read/write -->
          <xsl:if test="not($attrreadonly='yes')">
            <xsl:variable name="attrSetter"><xsl:call-template name="makeSetterName"><xsl:with-param name="attrname" select="$attrname" /></xsl:call-template></xsl:variable>
            <xsl:call-template name="emitMethod">
              <xsl:with-param name="ifname" select="$ifname" />
              <xsl:with-param name="methodname" select="$attrSetter" />
              <xsl:with-param name="fOutParams" select="1" />
            </xsl:call-template>
          </xsl:if>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each> <!-- select="attribute" -->

    <!--
      here come the real methods
    -->

    <xsl:for-each select="method">
      <xsl:variable name="methodname"><xsl:value-of select="@name" /></xsl:variable>

      <xsl:choose>
        <xsl:when test="param[@type=($G_setSuppressedInterfaces/@name)]">
          <xsl:comment><xsl:value-of select="concat('skipping method ', $methodname, ' for it has parameters with suppressed types')" /></xsl:comment>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="emitMethod">
            <xsl:with-param name="ifname" select="$ifname" />
            <xsl:with-param name="methodname" select="$methodname" />
            <xsl:with-param name="fOutParams" select="1" /> <!-- (count(param[@dir='out'] | param[@dir='return']) > 0)" /> -->
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:if>

</xsl:template>

</xsl:stylesheet>
