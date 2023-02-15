<?xml version="1.0" encoding="UTF-8"?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!-- (c) Copyright IBM Corp. 2006 All Rights Reserved. -->

<xsl:stylesheet version="1.0" 
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:ditaarch="http://dita.oasis-open.org/architecture/2005/"
                xmlns:saxon="http://icl.com/saxon"
                xmlns:xalan="http://xml.apache.org/xalan/redirect"
                extension-element-prefixes="saxon xalan">

<xsl:output method="xml"
      encoding="utf-8"
      indent="no"
      doctype-public="-//OASIS//DTD DocBook XML V4.4//EN"
      doctype-system="http://www.oasis-open.org/docbook/xml/4.4/docbookx.dtd"
/>

<xsl:param name="DITAEXT"     select="'.dita'"/>
<xsl:param name="DBEXT"       select="'.xml'"/>
<xsl:param name="HTMLEXT"     select="'.html'"/>
<xsl:param name="BUILDINFIX"  select="'_GENERATED'"/>


<!-- key on references to DocBook content -->
<xsl:key name="docbookref"
  match="*[contains(@class,' map/topicref ') and @href and @format='docbook']"
  use="@href"/>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - update DocBook files
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="/">
  <xsl:apply-templates select=".//*[contains(@class,' map/topicref ') and
      @href and @format='docbook' and
      generate-id(.)=generate-id(key('docbookref',@href)[1])]"
    mode="docbookref"/>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - push context into DocBook files
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*[contains(@class,' map/topicref ') and @href and
      @format='docbook']" mode="docbookref">
  <xsl:apply-templates select="document(@href,/)/*" mode="docbookobj">
    <xsl:with-param name="filepath" select="@href"/>
    <xsl:with-param name="contexts" select="key('docbookref', @href)"/>
  </xsl:apply-templates>
</xsl:template>

<xsl:template match="article|refentry|section" mode="docbookobj">
  <xsl:param name="filepath"/>
  <xsl:param name="contexts"/>
  <xsl:variable name="filebase" select="substring($filepath, 1, 
      string-length($filepath) - string-length($DBEXT))"/>
  <xsl:variable name="outfile"
      select="concat($filebase, $BUILDINFIX, $DBEXT)"/>
  <xsl:call-template name="writeout">
    <xsl:with-param name="filename" select="$outfile"/>
    <xsl:with-param name="content">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>
    <xsl:apply-templates select="*|text()|comment()|processing-instruction()"/>
    <xsl:if test="$contexts">
      <!-- REALLY, SHOULD HANDLE topicgroup WITH collection-type OF FAMILY --> 
      <xsl:variable name="relatedlinks"
          select="$contexts[parent::*[contains(@class,' map/relcell ')]]"/>
      <!-- navigation contexts -->
      <xsl:for-each select="$contexts[parent::*[
          (contains(@class,' map/map ') or
              (contains(@class,' map/topicref ') and @href)) and
          (not(@toc) or @toc='yes')]]">
        <simplelist role="navigationlinks">
        <xsl:apply-templates
              select="parent::*[contains(@class,' map/topicref ')]"
              mode="docbook-link">
          <xsl:with-param name="role" select="'parent'"/>
        </xsl:apply-templates>
        <xsl:apply-templates select="preceding-sibling::*[
            contains(@class,' map/topicref ') and @href and 
            (not(@toc) or @toc='yes')][1]"
            mode="docbook-link">
          <xsl:with-param name="role" select="'preceding'"/>
        </xsl:apply-templates>
        <xsl:apply-templates select="following-sibling::*[
            contains(@class,' map/topicref ') and @href and 
            (not(@toc) or @toc='yes')][1]"
            mode="docbook-link">
          <xsl:with-param name="role" select="'following'"/>
        </xsl:apply-templates>
        <xsl:apply-templates select="*[contains(@class,' map/topicref ') and
            @href and (not(@toc) or @toc='yes')]" mode="docbook-link">
          <xsl:with-param name="role" select="'child'"/>
        </xsl:apply-templates>
        </simplelist>
      </xsl:for-each>
      <!-- related link contexts -->
      <xsl:if test="$relatedlinks">
        <simplelist role="relatedlinks">
        <xsl:for-each select="$relatedlinks">
          <xsl:apply-templates select="
                  parent::*[contains(@class,' map/relcell ')] /
                      preceding-sibling::*[contains(@class,' map/relcell ')] /
                      *[contains(@class,' map/topicref ') and @href] |
                  parent::*[contains(@class,' map/relcell ')] /
                      following-sibling::*[contains(@class,' map/relcell ')] /
                      *[contains(@class,' map/topicref ') and @href]"
              mode="docbook-link">
            <xsl:with-param name="role" select="'related'"/>
          </xsl:apply-templates>
        </xsl:for-each>
        </simplelist>
      </xsl:if>
    </xsl:if>
  </xsl:copy>
    </xsl:with-param>
  </xsl:call-template>
</xsl:template>

<xsl:template match="*[contains(@class,' map/topicref ') and @href]"
    mode="docbook-link">
  <xsl:param name="role"/>
  <member role="{$role}">
    <ulink>
      <xsl:attribute name="url">
        <xsl:apply-templates select="." mode="html-filename"/>
      </xsl:attribute>
      <xsl:value-of select="@navtitle"/>
    </ulink>
  </member>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - utilities
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*[contains(@class,' map/topicref ') and @href and
      (not(@format) or @format='dita')]" mode="html-filename">
  <xsl:value-of select="concat(
    substring(@href, 1, string-length(@href) - string-length($DITAEXT)),
    $HTMLEXT)"/>
</xsl:template>

<xsl:template match="*[contains(@class,' map/topicref ') and @href and
      (@format='html' or @format='pdf')]" mode="html-filename">
  <xsl:value-of select="@href"/>
</xsl:template>

<xsl:template match="*[contains(@class,' map/topicref ') and @href and
      @format='docbook']" mode="html-filename">
  <xsl:value-of select="concat(
    substring(@href, 1, string-length(@href) - string-length($DBEXT)),
    $BUILDINFIX,
    $HTMLEXT)"/>
</xsl:template>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - default copy actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template match="*">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>
    <xsl:apply-templates select="*|text()|comment()|processing-instruction()"/>
  </xsl:copy>
</xsl:template>

<xsl:template match="@*|text()|comment()|processing-instruction()">
  <xsl:copy/>
</xsl:template>

<!-- suppress copy for defaulted attributes -->
<xsl:template match="@class|@ditaarch:DITAArchVersion|@domains"/>


<!-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   - write actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -->
<xsl:template name="writeout">
  <xsl:param name="filename"/>
  <xsl:param name="content"/>
  <xsl:choose>
  <xsl:when test="element-available('saxon:output')">
    <saxon:output href="{$filename}"
        method="xml"
        encoding="utf-8"
        indent="no">
      <xsl:copy-of select="$content"/>
    </saxon:output>
  </xsl:when>
  <xsl:when test="element-available('xalan:write')">
    <xalan:write file="{$filename}">
      <xsl:copy-of select="$content"/>
    </xalan:write>
  </xsl:when>
  <xsl:otherwise>
    <xsl:message terminate="yes">
      <xsl:text>Cannot write</xsl:text>
    </xsl:message>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

</xsl:stylesheet>
