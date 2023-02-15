<?xml version="1.0" encoding="UTF-8" ?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!--
 | (C) Copyright IBM Corporation 2005 - 2006. All Rights Reserved.
 *-->
<xsl:stylesheet version="1.0" 
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - Rules for DITA elements
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*[contains(@class,' map/map ')]" mode="dbdita-chapter">
  <xsl:apply-templates select="*[contains(@class,' map/topicref ')]"
      mode="dbdita-chapter"/>
</xsl:template>

<xsl:template match="*[contains(@class,' map/map ')]" mode="dbdita-section">
  <xsl:apply-templates select="*[contains(@class,' map/topicref ')]"
      mode="dbdita-section"/>
</xsl:template>

<xsl:template match="*[contains(@class,' map/topicref ') and
      contains(@class,' bookmap/')]"
    mode="dbdita-chapter">
  <xsl:apply-templates select="."/>
</xsl:template>

<xsl:template match="*[contains(@class,' map/topicref ') and
      contains(@class,' bookmap/')]"
    mode="dbdita-section">
  <xsl:call-template name="topicref">
    <xsl:with-param name="element" select="'section'"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[contains(@class,' map/topicref ') and
      not(contains(@class,' bookmap/'))]"
    mode="dbdita-chapter">
  <xsl:call-template name="topicref">
    <xsl:with-param name="element" select="'chapter'"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[contains(@class,' map/topicref ') and
      not(contains(@class,' bookmap/'))]"
    mode="dbdita-section">
  <xsl:apply-templates select="."/>
</xsl:template>

<xsl:template match="*[contains(@class,' topic/topic ')]"
    mode="dbdita-chapter">
  <xsl:apply-templates select=".">
    <xsl:with-param name="element" select="'chapter'"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="*[contains(@class,' topic/topic ')]"
    mode="dbdita-section">
  <xsl:apply-templates select="."/>
</xsl:template>

</xsl:stylesheet>

