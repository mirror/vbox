<!--
    websrv-shared.inc.xsl:
        this gets included from the other websrv-*.xsl XSLT stylesheets
        so we can share some definitions that must be the same for
        all of them (like method prefixes/suffices).

     Copyright (C) 2006-2007 Sun Microsystems, Inc.

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
  targetNamespace="http://schemas.xmlsoap.org/wsdl/"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  xmlns:soap="http://schemas.xmlsoap.org/wsdl/soap/"
  xmlns:vbox="http://www.virtualbox.org/">

<xsl:variable name="G_xsltIncludeFilename" select="'websrv-shared.inc.xsl'" />

<!-- target namespace; this must match the xmlns:vbox in stylesheet opening tags! -->
<xsl:variable name="G_targetNamespace"
              select='"http://www.virtualbox.org/"' />
<xsl:variable name="G_targetNamespaceSeparator"
              select='""' />

<!-- ENCODING SCHEME

    See: http://www-128.ibm.com/developerworks/webservices/library/ws-whichwsdl/

    Essentially "document" style means that each SOAP message is a complete and
    self-explanatory document that does not rely on outside information for
    validation.

    By contrast, the (older) "RPC" style allows for much shorter SOAP messages
    that do not contain validation info like all types that are used, but then
    again, caller and receiver must have agreed on a valid format in some other way.
    With RPC, WSDL typically looks like this:

            <message name="myMethodRequest">
                <part name="x" type="xsd:int"/>
                <part name="y" type="xsd:float"/>
            </message>

    This is why today "document" style is preferred. However, with document style,
    one _cannot_ use "type" in <part> elements. Instead, one must use "element"
    attributes that refer to <element> items in the type section. Like this:

        <types>
            <schema>
                <element name="xElement" type="xsd:int"/>
                <element name="yElement" type="xsd:float"/>
            </schema>
        </types>

        <message name="myMethodRequest">
            <part name="x" element="xElement"/>
            <part name="y" element="yElement"/>
        </message>

    The "encoded" and "literal" sub-styles then only determine whether the
    individual types in the soap messages carry additional information in
    attributes. "Encoded" was only used with RPC styles, really, and even that
    is not widely supported any more.

-->
<!-- These are the settings: all the other XSLTs react on this and are supposed
     to be able to generate both valid RPC and document-style code. The only
     allowed values are 'rpc' or 'document'. -->
<xsl:variable name="G_basefmt"
              select='"document"' />
<xsl:variable name="G_parmfmt"
              select='"literal"' />
<!-- <xsl:variable name="G_basefmt"
              select='"rpc"' />
<xsl:variable name="G_parmfmt"
              select='"encoded"' /> -->

<!-- with document style, this is how we name the request and return element structures -->
<xsl:variable name="G_requestElementVarName"
              select='"req"' />
<xsl:variable name="G_responseElementVarName"
              select='"resp"' />
<!-- this is how we name the result parameter in messages -->
<xsl:variable name="G_result"
              select='"returnval"' />

<!-- we represent interface attributes by creating "get" and "set" methods; these
     are the prefixes we use for that -->
<xsl:variable name="G_attributeGetPrefix"
              select='"get"' />
<xsl:variable name="G_attributeSetPrefix"
              select='"set"' />
<!-- separator between class name and method/attribute name; would be "::" in C++
     but i'm unsure whether WSDL appreciates that (WSDL only) -->
<xsl:variable name="G_classSeparator"
              select='"_"' />
<!-- for each interface method, we need to create both a "request" and a "response"
     message; these are the suffixes we append to the method names for that -->
<xsl:variable name="G_methodRequest"
              select='"RequestMsg"' />
<xsl:variable name="G_methodResponse"
              select='"ResultMsg"' />
<!-- suffix for element declarations that describe request message parameters (WSDL only) -->
<xsl:variable name="G_requestMessageElementSuffix"
              select='""' />
<!-- suffix for element declarations that describe request message parameters (WSDL only) -->
<xsl:variable name="G_responseMessageElementSuffix"
              select='"Response"' />
<!-- suffix for portType names (WSDL only) -->
<xsl:variable name="G_portTypeSuffix"
              select='"PortType"' />
<!-- suffix for binding names (WSDL only) -->
<xsl:variable name="G_bindingSuffix"
              select='"Binding"' />
<!-- schema type to use for object references; while it is theoretically
     possible to use a self-defined type (e.g. some vboxObjRef type that's
     really an int), gSOAP gets a bit nasty and creates complicated structs
     for function parameters when these types are used as output parameters.
     So we just use "int" even though it's not as lucid.
     One setting is for the WSDL emitter, one for the C++ emitter -->
<!--
<xsl:variable name="G_typeObjectRef"
              select='"xsd:unsignedLong"' />
<xsl:variable name="G_typeObjectRef_gsoapH"
              select='"ULONG64"' />
<xsl:variable name="G_typeObjectRef_CPP"
              select='"WSDLT_ID"' />
-->
<xsl:variable name="G_typeObjectRef"
              select='"xsd:string"' />
<xsl:variable name="G_typeObjectRef_gsoapH"
              select='"std::string"' />
<xsl:variable name="G_typeObjectRef_CPP"
              select='"std::string"' />
<!-- and what to call first the object parameter -->
<xsl:variable name="G_nameObjectRef"
              select='"_this"' />
<!-- gSOAP encodes underscores with USCORE so this is used in our C++ code -->
<xsl:variable name="G_nameObjectRefEncoded"
              select='"_USCOREthis"' />

<!-- type to represent enums with in C++ COM callers -->
<xsl:variable name="G_funcPrefixInputEnumConverter"
              select='"EnumSoap2Com_"' />
<xsl:variable name="G_funcPrefixOutputEnumConverter"
              select='"EnumCom2Soap_"' />

<xsl:variable name="G_aSharedTypes">
  <type idlname="octet"              xmlname="unsignedByte"  cname="unsigned char"    gluename="BYTE"    javaname="Short" />
  <type idlname="boolean"            xmlname="boolean"       cname="bool"             gluename="BOOL"    javaname="Boolean" />
  <type idlname="short"              xmlname="short"         cname="short"            gluename="SHORT"   javaname="Short" />
  <type idlname="unsigned short"     xmlname="unsignedShort" cname="unsigned short"   gluename="USHORT"  javaname="Integer" />
  <type idlname="long"               xmlname="int"           cname="int"              gluename="LONG"    javaname="Integer" />
  <type idlname="unsigned long"      xmlname="unsignedInt"   cname="unsigned int"     gluename="ULONG"   javaname="Long" />
  <type idlname="long long"          xmlname="long"          cname="LONG64"           gluename="LONG64"  javaname="Long" />
  <type idlname="unsigned long long" xmlname="unsignedLong"  cname="ULONG64"          gluename="ULONG64" javaname="BigInteger" />
  <type idlname="double"             xmlname="double"        cname="double"           gluename=""        javaname="Double" />
  <type idlname="float"              xmlname="float"         cname="float"            gluename=""        javaname="Float" />
  <type idlname="wstring"            xmlname="string"        cname="std::string"      gluename=""        javaname="String" />
  <type idlname="uuid"               xmlname="string"        cname="std::string"      gluename=""        javaname="String" />
  <type idlname="result"             xmlname="unsignedInt"   cname="unsigned int"     gluename="HRESULT" javaname="Long" />
</xsl:variable>

<!--
    warning:
  -->

<xsl:template name="warning">
  <xsl:param name="msg" />

  <xsl:message terminate="no">
    <xsl:value-of select="concat('[', $G_xsltFilename, '] Warning in ', $msg)" />
  </xsl:message>
</xsl:template>

<!--
    fatalError:
  -->

<xsl:template name="fatalError">
  <xsl:param name="msg" />

  <xsl:message terminate="yes">
    <xsl:value-of select="concat('[', $G_xsltFilename, '] Error in ', $msg)" />
  </xsl:message>
</xsl:template>

<!--
    debugMsg
    -->

<xsl:template name="debugMsg">
  <xsl:param name="msg" />

  <xsl:if test="$G_argDebug">
    <xsl:message terminate="no">
      <xsl:value-of select="concat('[', $G_xsltFilename, '] ', $msg)" />
    </xsl:message>
  </xsl:if>
</xsl:template>

<!--
    uncapitalize
    -->

<xsl:template name="uncapitalize">
  <xsl:param name="str" select="."/>
  <xsl:value-of select="
        concat(
            translate(substring($str,1,1),'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz'),
            substring($str,2)
        )
  "/>
</xsl:template>
<!--
    uncapitalize in the way JAX-WS understands, see #2910
    -->

<xsl:template name="uncapitalize2">
  <xsl:param name="str" select="."/>
  <xsl:variable name="strlen">
    <xsl:value-of select="string-length($str)"/>
  </xsl:variable>
  <xsl:choose>
    <xsl:when test="$strlen>1">
     <xsl:choose>
       <xsl:when test="contains('ABCDEFGHIJKLMNOPQRSTUVWXYZ',substring($str,1,1))
                       and
                       contains('ABCDEFGHIJKLMNOPQRSTUVWXYZ',substring($str,2,1))">
         <xsl:variable name="cdr">
           <xsl:call-template name="uncapitalize2">
             <xsl:with-param name="str" select="substring($str,2)"/>
           </xsl:call-template>
         </xsl:variable>
         <xsl:value-of select="
           concat(
            translate(substring($str,1,1),
                      'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
                      'abcdefghijklmnopqrstuvwxyz'),
            $cdr
           )
           "/>
         </xsl:when>
         <xsl:otherwise>
           <!--<xsl:value-of select="concat(substring($str,1,1),$cdr)"/>-->
           <xsl:value-of select="$str"/>
         </xsl:otherwise>
     </xsl:choose>
    </xsl:when>
    <xsl:when test="$strlen=1">
      <xsl:value-of select="
                            translate($str,
                            'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
                            'abcdefghijklmnopqrstuvwxyz')
                            "/>
    </xsl:when>
    <xsl:otherwise>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>
<!--
    capitalize
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
    makeGetterName:
    -->
<xsl:template name="makeGetterName">
  <xsl:param name="attrname" />
  <xsl:variable name="capsname"><xsl:call-template name="capitalize"><xsl:with-param name="str" select="$attrname" /></xsl:call-template></xsl:variable>
  <xsl:value-of select="concat($G_attributeGetPrefix, $capsname)" />
</xsl:template>

<!--
    makeSetterName:
    -->
<xsl:template name="makeSetterName">
  <xsl:param name="attrname" />
  <xsl:variable name="capsname"><xsl:call-template name="capitalize"><xsl:with-param name="str" select="$attrname" /></xsl:call-template></xsl:variable>
  <xsl:value-of select="concat($G_attributeSetPrefix, $capsname)" />
</xsl:template>

<!--
    makeJaxwsMethod: compose idevInterfaceMethod out of IDEVInterface::method
    -->
<xsl:template name="makeJaxwsMethod">
  <xsl:param name="ifname" />
  <xsl:param name="methodname" />
  <xsl:variable name="uncapsif"><xsl:call-template name="uncapitalize2"><xsl:with-param name="str" select="$ifname" /></xsl:call-template></xsl:variable>
  <xsl:variable name="capsmethod"><xsl:call-template name="capitalize"><xsl:with-param name="str" select="$methodname" /></xsl:call-template></xsl:variable>
  <xsl:value-of select="concat($uncapsif, $capsmethod)" />
</xsl:template>


<!--
    makeJaxwsMethod2: compose iInterfaceMethod out of IInterface::method
    -->
<xsl:template name="makeJaxwsMethod2">
  <xsl:param name="ifname" />
  <xsl:param name="methodname" />
  <xsl:variable name="uncapsif"><xsl:call-template name="uncapitalize"><xsl:with-param name="str" select="$ifname" /></xsl:call-template></xsl:variable>
  <xsl:variable name="capsmethod"><xsl:call-template name="capitalize"><xsl:with-param name="str" select="$methodname" /></xsl:call-template></xsl:variable>
  <xsl:value-of select="concat($uncapsif, $capsmethod)" />
</xsl:template>

<!--
    emitNewline:
    -->
<xsl:template name="emitNewline">
  <xsl:text>
</xsl:text>
</xsl:template>

<!--
    emitNewlineIndent8:
    -->
<xsl:template name="emitNewlineIndent8">
  <xsl:text>
        </xsl:text>
</xsl:template>

<!--
    escapeUnderscores
    -->
<xsl:template name="escapeUnderscores">
  <xsl:param name="string" />
  <xsl:if test="contains($string, '_')">
    <xsl:value-of select="substring-before($string, '_')" />_USCORE<xsl:call-template name="escapeUnderscores"><xsl:with-param name="string"><xsl:value-of select="substring-after($string, '_')" /></xsl:with-param></xsl:call-template>
  </xsl:if>
  <xsl:if test="not(contains($string, '_'))"><xsl:value-of select="$string" />
  </xsl:if>
</xsl:template>

</xsl:stylesheet>
