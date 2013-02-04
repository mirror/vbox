<xsl:stylesheet version = '1.0'
     xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
     xmlns:vbox="http://www.virtualbox.org/"
     xmlns:exsl="http://exslt.org/common"
     extension-element-prefixes="exsl">

<!--

    genjifaces.xsl:
        XSLT stylesheet that generates Java XPCOM bridge intreface code from VirtualBox.xidl.

    Copyright (C) 2012 Oracle Corporation

    This file is part of VirtualBox Open Source Edition (OSE), as
    available from http://www.virtualbox.org. This file is free software;
    you can redistribute it and/or modify it under the terms of the GNU
    General Public License (GPL) as published by the Free Software
    Foundation, in version 2 as it comes in the "COPYING" file of the
    VirtualBox OSE distribution. VirtualBox OSE is distributed in the
    hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:output
  method="text"
  version="1.0"
  encoding="utf-8"
  indent="no"/>

<!-- - - - - - - - - - - - - - - - - - - - - - -
  global XSLT variables
 - - - - - - - - - - - - - - - - - - - - - - -->

<xsl:variable name="G_xsltFilename" select="'genjifaces.xsl'" />

<xsl:template name="uppercase">
  <xsl:param name="str" select="."/>
  <xsl:value-of select="translate($str, 'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')" />
</xsl:template>

<xsl:template name="capitalize">
  <xsl:param name="str" select="."/>
  <xsl:value-of select="
        concat(translate(substring($str,1,1),'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ'),
               substring($str,2))"/>
</xsl:template>

<xsl:template name="makeGetterName">
  <xsl:param name="attrname" />
  <xsl:variable name="capsname">
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="$attrname" />
  </xsl:call-template>
  </xsl:variable>
  <xsl:value-of select="concat('get', $capsname)" />
</xsl:template>

<xsl:template name="makeSetterName">
  <xsl:param name="attrname" />
  <xsl:variable name="capsname">
    <xsl:call-template name="capitalize">
      <xsl:with-param name="str" select="$attrname" />
  </xsl:call-template>
  </xsl:variable>
  <xsl:value-of select="concat('set', $capsname)" />
</xsl:template>

<xsl:template name="fileheader">
  <xsl:param name="name" />
  <xsl:text>/**
 *  Copyright (C) 2010 Oracle Corporation
 *
 *  This file is part of VirtualBox Open Source Edition (OSE), as
 *  available from http://www.virtualbox.org. This file is free software;
 *  you can redistribute it and/or modify it under the terms of the GNU
 *  General Public License (GPL) as published by the Free Software
 *  Foundation, in version 2 as it comes in the "COPYING" file of the
 *  VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 *  hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
</xsl:text>
  <xsl:value-of select="concat(' * ',$name)"/>
<xsl:text>
 *
 * DO NOT EDIT! This is a generated file.
 * Generated from: src/VBox/Main/idl/VirtualBox.xidl (VirtualBox's interface definitions in XML)
 * Generator: src/VBox/src/libs/xpcom18a4/java/tools/genjifaces.xsl
 */

</xsl:text>
</xsl:template>

<xsl:template name="startFile">
  <xsl:param name="file" />

  <xsl:value-of select="concat('&#10;// ##### BEGINFILE &quot;', $file, '&quot;&#10;&#10;')" />
  <xsl:call-template name="fileheader">
    <xsl:with-param name="name" select="$file" />
  </xsl:call-template>

  <xsl:value-of select="       'package org.mozilla.interfaces;&#10;&#10;'" />
</xsl:template>

<xsl:template name="endFile">
 <xsl:param name="file" />
 <xsl:value-of select="concat('&#10;// ##### ENDFILE &quot;', $file, '&quot;&#10;&#10;')" />
</xsl:template>


<xsl:template name="emitHandwritten">

 <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'nsISupports.java'" />
 </xsl:call-template>

 <xsl:text><![CDATA[
public interface nsISupports
{
  public static final String NS_ISUPPORTS_IID =
    "{00000000-0000-0000-c000-000000000046}";

  public nsISupports queryInterface(String arg1);

}

]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'nsISupports.java'" />
 </xsl:call-template>

<xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'nsIComponentManager.java'" />
 </xsl:call-template>

 <xsl:text><![CDATA[
public interface nsIComponentManager extends nsISupports
{
  public static final String NS_ICOMPONENTMANAGER_IID =
    "{a88e5a60-205a-4bb1-94e1-2628daf51eae}";

  public nsISupports getClassObject(String arg1, String arg2);

  public nsISupports getClassObjectByContractID(String arg1, String arg2);

  public nsISupports createInstance(String arg1, nsISupports arg2, String arg3);

  public nsISupports createInstanceByContractID(String arg1, nsISupports arg2, String arg3);
}

]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'nsIComponentManager.java'" />
 </xsl:call-template>

<xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'nsIServiceManager.java'" />
 </xsl:call-template>

 <xsl:text><![CDATA[
public interface nsIServiceManager extends nsISupports
{
  public static final String NS_ISERVICEMANAGER_IID =
    "{8bb35ed9-e332-462d-9155-4a002ab5c958}";

  public nsISupports getService(String arg1, String arg2);

  public nsISupports getServiceByContractID(String arg1, String arg2);

  public boolean isServiceInstantiated(String arg1, String arg2);

  public boolean isServiceInstantiatedByContractID(String arg1, String arg2);
}

]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'nsIServiceManager.java'" />
 </xsl:call-template>

<xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'nsIComponentRegistrar.java'" />
 </xsl:call-template>

 <xsl:text><![CDATA[
public interface nsIComponentRegistrar extends nsISupports
{
   public static final String NS_ICOMPONENTREGISTRAR_IID =
    "{2417cbfe-65ad-48a6-b4b6-eb84db174392}";

   // No methods - placeholder
}

]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'nsIComponentRegistrar.java'" />
 </xsl:call-template>


<xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'nsIFile.java'" />
 </xsl:call-template>

 <xsl:text><![CDATA[
public interface nsIFile extends nsISupports
{
  public static final String NS_IFILE_IID =
    "{c8c0a080-0868-11d3-915f-d9d889d48e3c}";

  // No methods - placeholder
}

]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'nsIFile.java'" />
 </xsl:call-template>

<xsl:call-template name="startFile">
    <xsl:with-param name="file" select="'nsILocalFile.java'" />
 </xsl:call-template>

 <xsl:text><![CDATA[
public interface nsILocalFile extends nsIFile
{
  public static final String NS_ILOCALFILE_IID =
    "{aa610f20-a889-11d3-8c81-000064657374}";

  // No methods - placeholder
}

]]></xsl:text>

 <xsl:call-template name="endFile">
   <xsl:with-param name="file" select="'nsILocalFile.java'" />
 </xsl:call-template>

</xsl:template>

<xsl:template name="genEnum">
  <xsl:param name="enumname" />
  <xsl:param name="filename" />

  <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="$filename" />
  </xsl:call-template>

  <xsl:value-of select="concat('public interface ', $enumname, ' {&#10;&#10;')" />

  <xsl:variable name="uppername">
    <xsl:call-template name="uppercase">
      <xsl:with-param name="str" select="$enumname" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:value-of select="concat('  public static final String ', $uppername, '_IID = &#10;',
                               '     &quot;{',@uuid, '}&quot;;&#10;&#10;')" />

  <xsl:for-each select="const">
    <xsl:variable name="enumconst" select="@name" />
    <xsl:value-of select="concat('  public static final long ', @name, ' = ', @value, 'L;&#10;&#10;')" />
  </xsl:for-each>

  <xsl:value-of select="'}&#10;&#10;'" />

  <xsl:call-template name="endFile">
    <xsl:with-param name="file" select="$filename" />
  </xsl:call-template>

</xsl:template>

<xsl:template name="typeIdl2Back">
  <xsl:param name="type" />
  <xsl:param name="safearray" />
  <xsl:param name="forceelem" />

  <xsl:variable name="needarray" select="($safearray='yes') and not($forceelem='yes')" />

  <xsl:choose>
    <xsl:when test="$type='unsigned long long'">
      <!-- stupid, rewrite the bridge -->
      <xsl:value-of select="'double'" />
    </xsl:when>

    <xsl:when test="$type='long long'">
      <xsl:value-of select="'long'" />
    </xsl:when>

    <xsl:when test="$type='unsigned long'">
      <xsl:value-of select="'long'" />
    </xsl:when>

    <xsl:when test="$type='long'">
      <xsl:value-of select="'int'" />
    </xsl:when>

    <xsl:when test="$type='unsigned short'">
      <xsl:value-of select="'int'" />
    </xsl:when>

    <xsl:when test="$type='short'">
      <xsl:value-of select="'short'" />
    </xsl:when>

    <xsl:when test="$type='octet'">
      <xsl:value-of select="'byte'" />
    </xsl:when>

    <xsl:when test="$type='boolean'">
      <xsl:value-of select="'boolean'" />
    </xsl:when>

    <xsl:when test="$type='$unknown'">
      <xsl:value-of select="'nsISupports'"/>
    </xsl:when>

    <xsl:when test="$type='wstring'">
      <xsl:value-of select="'String'" />
    </xsl:when>

    <xsl:when test="$type='uuid'">
      <xsl:value-of select="'String'" />
    </xsl:when>

    <xsl:when test="//interface[@name=$type]">
      <xsl:value-of select="$type" />
    </xsl:when>

    <xsl:when test="//enum[@name=$type]">
      <xsl:value-of select="'long'" />
    </xsl:when>

  </xsl:choose>

  <xsl:if test="$needarray">
    <xsl:value-of select="'[]'" />
  </xsl:if>

</xsl:template>

<xsl:template name="genIface">
  <xsl:param name="ifname" />
  <xsl:param name="filename" />

  <xsl:call-template name="startFile">
    <xsl:with-param name="file" select="$filename" />
  </xsl:call-template>

  <xsl:variable name="extendsidl" select="//interface[@name=$ifname]/@extends" />

  <xsl:variable name="extends">
    <xsl:choose>
      <xsl:when test="($extendsidl = '$unknown') or ($extendsidl = '$dispatched') or ($extendsidl = '$errorinfo')">
        <xsl:value-of select="'nsISupports'" />
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$extendsidl" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:value-of select="concat('public interface ', $ifname, ' extends ', $extends, ' {&#10;&#10;')" />

  <xsl:variable name="uppername">
    <xsl:call-template name="uppercase">
      <xsl:with-param name="str" select="$ifname" />
    </xsl:call-template>
  </xsl:variable>

  <xsl:value-of select="concat('  public static final String ', $uppername, '_IID =&#10;',
                               '    &quot;{',@uuid, '}&quot;;&#10;&#10;')" />

  <xsl:for-each select="attribute">
    <xsl:variable name="attrname" select="@name" />
    <xsl:variable name="attrtype" select="@type" />

    <xsl:variable name="gettername">
      <xsl:call-template name="makeGetterName">
        <xsl:with-param name="attrname" select="$attrname" />
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="backtype">
      <xsl:call-template name="typeIdl2Back">
        <xsl:with-param name="type" select="$attrtype" />
        <xsl:with-param name="safearray" select="@safearray" />
      </xsl:call-template>
    </xsl:variable>

    <xsl:variable name="callparam">
      <xsl:if test="@safearray='yes'">
        <xsl:value-of select="concat('  long[] ',  @name, 'Size')" />
      </xsl:if>
    </xsl:variable>

    <xsl:value-of select="concat('  public ', $backtype, ' ', $gettername, '(',$callparam,');&#10;&#10;')" />

    <xsl:if test="not(@readonly='yes')">
      <xsl:variable name="settername">
        <xsl:call-template name="makeSetterName">
          <xsl:with-param name="attrname" select="$attrname" />
        </xsl:call-template>
      </xsl:variable>
      <xsl:value-of select="concat('  public void ',  $settername, '(', $backtype, ' arg1);&#10;&#10;')" />
    </xsl:if>

  </xsl:for-each>

  <xsl:for-each select="method">
    <xsl:variable name="methodname" select="@name" />
    <xsl:variable name="returnidltype" select="param[@dir='return']/@type" />
    <xsl:variable name="returnidlsafearray" select="param[@dir='return']/@safearray" />

    <xsl:variable name="returntype">
      <xsl:choose>
        <xsl:when test="$returnidltype">
          <xsl:call-template name="typeIdl2Back">
            <xsl:with-param name="type" select="$returnidltype" />
            <xsl:with-param name="safearray" select="$returnidlsafearray" />
          </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
          <xsl:text>void</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:value-of select="concat('  public ', $returntype, ' ', $methodname, '(')" />
    <xsl:for-each select="param">
      <xsl:variable name="paramtype">
        <xsl:call-template name="typeIdl2Back">
          <xsl:with-param name="type" select="@type" />
          <xsl:with-param name="safearray" select="@safearray" />
        </xsl:call-template>
      </xsl:variable>
      <xsl:choose>
        <xsl:when test="(@safearray='yes') and (@dir='return')">
          <xsl:value-of select="concat('long[] ', @name)" />
        </xsl:when>
        <xsl:when test="(@safearray='yes') and (@dir='out')">
          <xsl:value-of select="concat('long[] ', @name, 'Size, ', $paramtype, '[] ', @name)" />
        </xsl:when>
        <xsl:when test="(@safearray='yes') and (@dir='in') and (@type='octet')">
          <xsl:value-of select="concat($paramtype, ' ', @name)" />
        </xsl:when>
        <xsl:when test="(@safearray='yes') and (@dir='in')">
          <xsl:value-of select="concat('long ', @name, 'Size, ', $paramtype, ' ', @name)" />
        </xsl:when>
        <xsl:when test="@dir='out'">
          <xsl:value-of select="concat($paramtype, '[] ', @name)" />
        </xsl:when>
         <xsl:when test="@dir='in'">
           <xsl:value-of select="concat($paramtype, ' ', @name)" />
         </xsl:when>
      </xsl:choose>
      <xsl:if test="not(position()=last()) and not(following-sibling::param[1]/@dir='return' and not(following-sibling::param[1]/@safearray='yes'))">
        <xsl:value-of select="', '" />
      </xsl:if>
    </xsl:for-each>
    <xsl:value-of select="       ');&#10;&#10;'" />

  </xsl:for-each>

  <xsl:value-of select="'}&#10;&#10;'" />

  <xsl:call-template name="endFile">
    <xsl:with-param name="file" select="$filename" />
  </xsl:call-template>

</xsl:template>


<xsl:template match="/">

  <!-- Handwritten files -->
  <xsl:call-template name="emitHandwritten"/>

   <!-- Enums -->
  <xsl:for-each select="//enum">
    <xsl:call-template name="genEnum">
      <xsl:with-param name="enumname" select="@name" />
      <xsl:with-param name="filename" select="concat(@name, '.java')" />
    </xsl:call-template>
  </xsl:for-each>

  <!-- Interfaces -->
  <xsl:for-each select="//interface">
    <xsl:variable name="self_target" select="current()/ancestor::if/@target"/>
    <xsl:variable name="module" select="current()/ancestor::module/@name"/>

    <!-- We don't need WSDL-specific interfaces here -->
    <xsl:if test="not($self_target='wsdl') and not($module)">
      <xsl:call-template name="genIface">
        <xsl:with-param name="ifname" select="@name" />
        <xsl:with-param name="filename" select="concat(@name, '.java')" />
      </xsl:call-template>
    </xsl:if>

  </xsl:for-each>

</xsl:template>

</xsl:stylesheet>
