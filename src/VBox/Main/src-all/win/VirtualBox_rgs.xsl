<?xml version="1.0"?>

<!--
 *  A template to generate a RGS resource script that contains
 *  registry definitions necessary to properly register
 *  VirtualBox Main API COM components.

    Copyright (C) 2007-2012 Oracle Corporation

    This file is part of VirtualBox Open Source Edition (OSE), as
    available from http://www.virtualbox.org. This file is free software;
    you can redistribute it and/or modify it under the terms of the GNU
    General Public License (GPL) as published by the Free Software
    Foundation, in version 2 as it comes in the "COPYING" file of the
    VirtualBox OSE distribution. VirtualBox OSE is distributed in the
    hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
-->

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output method="text"/>

<xsl:strip-space elements="*"/>

<!--
//  parameters
/////////////////////////////////////////////////////////////////////////////
-->

<!-- Name of the module to generate the RGS script for -->
<xsl:param name="Module"/>


<!--
//  templates
/////////////////////////////////////////////////////////////////////////////
-->

<!--
 *  header
-->
<xsl:template match="/idl">
HKCR
{
<xsl:apply-templates/>
}
</xsl:template>


<!--
 *  libraries
-->
<xsl:template match="idl/library">
  NoRemove AppID
  {
    {<xsl:value-of select="@appUuid"/>} = s '<xsl:value-of select="@name"/> Application'
  }

<xsl:apply-templates select="module[@name=$Module]/class"/>
</xsl:template>


<!--
 *  classes
-->
<xsl:template match="library//module/class">
  <xsl:variable name="cname" select="concat(//library/@name,'.',@name)"/>
  <xsl:variable name="desc" select="concat(@name,' Class')"/>
  <xsl:text>  </xsl:text>
  <xsl:value-of select="concat($cname,'.1')"/> = s '<xsl:value-of select="$desc"/>'
  {
    CLSID = s '{<xsl:value-of select="@uuid"/>}'
  }
  <xsl:value-of select="$cname"/> = s '<xsl:value-of select="$desc"/>'
  {
    CLSID = s '{<xsl:value-of select="@uuid"/>}'
    CurVer = s '<xsl:value-of select="concat($cname,'.1')"/>'
  }
  NoRemove CLSID
  {
    ForceRemove {<xsl:value-of select="@uuid"/>} = s '<xsl:value-of select="$desc"/>'
    {
      ProgId = s '<xsl:value-of select="concat($cname,'.1')"/>'
      VersionIndependentProgID = s '<xsl:value-of select="$cname"/>'
      <xsl:choose>
        <xsl:when test="../@context='InprocServer'">InprocServer32</xsl:when>
        <xsl:when test="../@context='LocalServer'">LocalServer32</xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:value-of select="concat(../../@name,'::',../@name,': ')"/>
            <xsl:text>module context </xsl:text>
            <xsl:value-of select="concat('&quot;',../@context,'&quot;')"/>
            <xsl:text> is invalid!</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose> = s '%MODULE%'
      <xsl:if test="../@context='InprocServer'">
        <xsl:variable name="tmodel" select="(./@threadingModel | ../@threadingModel)[last()]"/>{
        val ThreadingModel = s '<xsl:choose>
          <xsl:when test="$tmodel='Apartment'">Apartment</xsl:when>
          <xsl:when test="$tmodel='Free'">Free</xsl:when>
          <xsl:when test="$tmodel='Both'">Both</xsl:when>
          <xsl:when test="$tmodel='Neutral'">Neutral</xsl:when>
          <xsl:when test="$tmodel='Single'">Single</xsl:when>
          <xsl:when test="$tmodel='Rental'">Rental</xsl:when>
          <xsl:otherwise>
            <xsl:message terminate="yes">
              <xsl:value-of select="concat(../../@name,'::',@name,': ')"/>
              <xsl:text>class (or module) threading model </xsl:text>
              <xsl:value-of select="concat('&quot;',$tmodel,'&quot;')"/>
              <xsl:text> is invalid!</xsl:text>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>'
      }
      </xsl:if>
      val AppId = s '{<xsl:value-of select="//library/@appUuid"/>}'
      'TypeLib' = s '{<xsl:value-of select="//library/@uuid"/>}'
    }
  }

</xsl:template>


<!--
 *  eat everything else not explicitly matched
-->
<xsl:template match="*">
</xsl:template>


</xsl:stylesheet>
