<?xml version="1.0" encoding="utf-8" standalone="no"?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!--
 | (C) Copyright IBM Corporation 2006. All Rights Reserved.
 *-->
<xsl:stylesheet version="1.0" 
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:xs="http://www.w3.org/2001/XMLSchema">

<xsl:output
    method="xml"
    indent="yes"
    omit-xml-declaration="no"
    standalone="no"/>

<xsl:template match="/">
  <xsl:apply-templates select="*"/>
</xsl:template>

<xsl:template match="xs:schema">
  <xsl:copy>
    <xsl:copy-of select="@*"/>
    <xsl:apply-templates select="xs:complexType|xs:group[contains(@name,'.cnt')]|
            xs:attributeGroup|xs:include|xs:redefine"/>
  </xsl:copy>  
</xsl:template>

<xsl:template match="xs:include">
  <xsl:copy>
    <xsl:copy-of select="@*"/>
    <xsl:apply-templates select="document(@schemaLocation)"/>
  </xsl:copy>  
</xsl:template>

<xsl:template match="xs:redefine">
  <xsl:copy>
    <xsl:copy-of select="@*"/>
    <xsl:apply-templates select="document(@schemaLocation)"/>
  </xsl:copy>  
</xsl:template>

<xsl:template
      match="xs:group[contains(@name,'.cnt')]|xs:complexType|xs:attributeGroup">
  <xsl:copy-of select="."/>
</xsl:template>

</xsl:stylesheet>
