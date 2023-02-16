<?xml version="1.0" encoding="utf-8"?><!-- This file is part of the DITA Open Toolkit project hosted on 
     Sourceforge.net. See the accompanying license.txt file for 
     applicable licenses.--><!-- (c) Copyright IBM Corp. 2004, 2005 All Rights Reserved. --><xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:number="urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0" xmlns:presentation="urn:oasis:names:tc:opendocument:xmlns:presentation:1.0" xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" xmlns:chart="urn:oasis:names:tc:opendocument:xmlns:chart:1.0" xmlns:dr3d="urn:oasis:names:tc:opendocument:xmlns:dr3d:1.0" xmlns:math="http://www.w3.org/1998/Math/MathML" xmlns:form="urn:oasis:names:tc:opendocument:xmlns:form:1.0" xmlns:script="urn:oasis:names:tc:opendocument:xmlns:script:1.0" xmlns:dom="http://www.w3.org/2001/xml-events" xmlns:xforms="http://www.w3.org/2002/xforms" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:anim="urn:oasis:names:tc:opendocument:xmlns:animation:1.0" xmlns:smil="urn:oasis:names:tc:opendocument:xmlns:smil-compatible:1.0" xmlns:prodtools="http://www.ibm.com/xmlns/prodtools" xmlns:random="org.dita.dost.util.RandomUtils" exclude-result-prefixes="random" version="1.1">
  
  
  <xsl:import href="plugin:org.dita.base:xsl/common/output-message.xsl"></xsl:import>
  <xsl:import href="plugin:org.dita.base:xsl/common/dita-utilities.xsl"></xsl:import>
  <xsl:import href="plugin:org.dita.base:xsl/common/related-links.xsl"></xsl:import>
  <xsl:import href="plugin:org.dita.base:xsl/common/dita-textonly.xsl"></xsl:import>
  <xsl:import href="plugin:org.dita.base:xsl/common/flag.xsl"></xsl:import>
  <!--
  <xsl:import href="xslodt/flag-old.xsl"/>
  -->
  <xsl:import href="xslodt/dita2odt-utilities.xsl"></xsl:import>
  <xsl:import href="xslodt/dita2odt-table.xsl"></xsl:import>
  <xsl:import href="xslodt/dita2odt-lists.xsl"></xsl:import>
  <xsl:import href="xslodt/dita2odt-img.xsl"></xsl:import>
  
  <xsl:import href="xslodt/dita2odt-map.xsl"></xsl:import>
  <xsl:import href="xslodt/dita2odtImpl.xsl"></xsl:import>
  <xsl:import href="xslodt/dita2odt-concept.xsl"></xsl:import>
  <xsl:import href="xslodt/dita2odt-gloss.xsl"></xsl:import>
  <xsl:import href="xslodt/dita2odt-task.xsl"></xsl:import>
  <xsl:import href="xslodt/dita2odt-reference.xsl"></xsl:import>
  <xsl:import href="xslodt/hi-d.xsl"></xsl:import>
  <xsl:import href="xslodt/pr-d.xsl"></xsl:import>
  <xsl:import href="xslodt/sw-d.xsl"></xsl:import>
  <xsl:import href="xslodt/ui-d.xsl"></xsl:import>
  <!--
  <xsl:import href="xslodt/common/vars.xsl"/>
  -->
  <xsl:import href="xslodt/commons.xsl"></xsl:import>
  <xsl:import href="xslodt/dita2odt-links.xsl"></xsl:import>

  <xsl:import href="xslodt/dita2odt-flagging.xsl"></xsl:import>

  <!-- 
  <xsl:include href="xslodt/dita2odt-relinks.xsl"/>
  -->
  
  


<!-- =========== DEFAULT VALUES FOR EXTERNALLY MODIFIABLE PARAMETERS =========== -->
<xsl:param name="include.rellinks"></xsl:param>
<xsl:param name="DRAFT" select="&apos;no&apos;"></xsl:param>
<xsl:param name="OUTPUTDIR" select="&apos;&apos;"></xsl:param>
<xsl:param name="FILTERFILE"></xsl:param>
<!-- default "hide index entries" processing parameter ('no' = hide them)-->
<xsl:param name="INDEXSHOW" select="&apos;no&apos;"></xsl:param><!-- "no" and "yes" are valid values; non-'yes' is ignored -->

<xsl:param name="FILEREF">file:///</xsl:param>
  
<!-- Transform type, such as 'xhtml', 'htmlhelp', or 'eclipsehelp' -->
<xsl:param name="TRANSTYPE" select="&apos;odt&apos;"></xsl:param>

<!-- for now, disable breadcrumbs pending link group descision -->
<xsl:param name="BREADCRUMBS" select="&apos;no&apos;"></xsl:param> <!-- "no" and "yes" are valid values; non-'yes' is ignored -->

<!-- the year for the copyright -->
<xsl:param name="YEAR" select="&apos;2010&apos;"></xsl:param>

<!-- the file name (file name and extension only - no path) of the document being transformed.
  Needed to help with debugging.
  default is 'myfile.xml')-->

<xsl:param name="FILENAME"></xsl:param>
<xsl:param name="FILEDIR"></xsl:param>
<xsl:param name="CURRENTFILE">
  <xsl:choose>
    <!-- ditamap -->
    <xsl:when test="contains($FILENAME, &apos;_MERGED&apos;)">
      <xsl:value-of select="concat($FILEDIR, &apos;/&apos;, substring-before($FILENAME, &apos;_MERGED&apos;), &apos;.ditamap&apos;)"></xsl:value-of>
    </xsl:when>
    <!-- topic -->
    <xsl:otherwise>
      <xsl:value-of select="concat($FILEDIR, &apos;/&apos;, $FILENAME)"></xsl:value-of>
    </xsl:otherwise>
  </xsl:choose>
</xsl:param>
  

<!-- Debug mode - enables XSL debugging xsl-messages.
  Needed to help with debugging.
  default is 'no')-->
<xsl:param name="DBG" select="&apos;no&apos;"></xsl:param> <!-- "no" and "yes" are valid values; non-'yes' is ignored -->
<!--embedding images as binary data -->
<xsl:param name="ODTIMGEMBED" select="&apos;yes&apos;"></xsl:param>

<!-- DITAEXT file extension name of dita topic file -->
<xsl:param name="DITAEXT" select="&apos;.xml&apos;"></xsl:param>

<!-- Name of the keyref file that contains key definitions -->
<xsl:param name="KEYREF-FILE" select="concat($WORKDIR,$PATH2PROJ,&apos;keydef.xml&apos;)"></xsl:param>

<xsl:param name="BASEDIR"></xsl:param>

<xsl:param name="TEMPDIR"></xsl:param>
  
<xsl:param name="includeRelatedLinkRoles" select="concat(&apos; &apos;, normalize-space($include.rellinks), &apos; &apos;)"></xsl:param>
  
<xsl:variable name="tempfiledir">
  <xsl:choose>
    <xsl:when test="contains($TEMPDIR, &apos;:\&apos;) or contains($TEMPDIR, &apos;:/&apos;)">
      <!--xsl:value-of select="concat($FILEREF,'/')"/-->
      <xsl:value-of select="&apos;file:/&apos;"></xsl:value-of><xsl:value-of select="concat($TEMPDIR, &apos;/&apos;)"></xsl:value-of>
    </xsl:when>
    <xsl:when test="starts-with($TEMPDIR, &apos;/&apos;)">
      <xsl:value-of select="&apos;file://&apos;"></xsl:value-of><xsl:value-of select="concat($TEMPDIR, &apos;/&apos;)"></xsl:value-of>
    </xsl:when>
    <xsl:when test="starts-with($BASEDIR, &apos;/&apos;)">
      <xsl:value-of select="&apos;file://&apos;"></xsl:value-of><xsl:value-of select="concat($BASEDIR, &apos;/&apos;)"></xsl:value-of><xsl:value-of select="concat($TEMPDIR, &apos;/&apos;)"></xsl:value-of>
    </xsl:when>
    <xsl:otherwise>
      <!--xsl:value-of select="concat($FILEREF,'/')"/-->
      <xsl:value-of select="&apos;file:/&apos;"></xsl:value-of><xsl:value-of select="concat($BASEDIR, &apos;/&apos;)"></xsl:value-of><xsl:value-of select="concat($TEMPDIR, &apos;/&apos;)"></xsl:value-of>
    </xsl:otherwise>
  </xsl:choose>
</xsl:variable>
  
<!-- current file with path -->
<xsl:variable name="currentfile" select="concat($tempfiledir, $CURRENTFILE)"></xsl:variable>
<!-- the working directory that contains the document being transformed.
  Needed as a directory prefix for the @conref "document()" function calls.
  default is '../doc/')-->
<xsl:variable name="WORKDIR">
  <xsl:apply-templates select="document($currentfile, /)/processing-instruction(&apos;workdir-uri&apos;)[1]" mode="get-work-dir"></xsl:apply-templates>
</xsl:variable>

<!-- the path back to the project. Used for c.gif, delta.gif, css to allow user's to have
  these files in 1 location. -->
<xsl:variable name="PATH2PROJ">
  <xsl:apply-templates select="document($currentfile, /)/processing-instruction(&apos;path2project-uri&apos;)[1]" mode="get-path2project"></xsl:apply-templates>
</xsl:variable>


<xsl:output method="xml" encoding="UTF-8" indent="yes"></xsl:output>
<xsl:strip-space elements="*"></xsl:strip-space>

  <xsl:attribute-set name="root">
    <xsl:attribute name="office:version">1.1</xsl:attribute>
  </xsl:attribute-set>
  
  <xsl:template match="/">
    <office:document-content xsl:use-attribute-sets="root">
      <xsl:call-template name="root"></xsl:call-template>
    </office:document-content>
  </xsl:template>
  
  <xsl:template name="root">
    <!--xsl:call-template name="gen-list-table"/-->
    <office:scripts></office:scripts>
    <office:automatic-styles>
      <xsl:if test="//*[contains(@class, &apos; topic/table &apos;)]|//*[contains(@class, &apos; topic/simpletable &apos;)]">
          <xsl:call-template name="create_table_cell_styles"></xsl:call-template>
      </xsl:if>
      
      <xsl:apply-templates select="//text()|//*[contains(@class, &apos; topic/state &apos;)]" mode="create_hi_style"></xsl:apply-templates>
      
      <xsl:call-template name="create_flagging_styles"></xsl:call-template>
      
    </office:automatic-styles>
    <office:body>
      <office:text>
        <text:sequence-decls>
          <text:sequence-decl text:display-outline-level="0" text:name="Illustration"></text:sequence-decl>
          <text:sequence-decl text:display-outline-level="0" text:name="Table"></text:sequence-decl>
          <text:sequence-decl text:display-outline-level="0" text:name="Text"></text:sequence-decl>
          <text:sequence-decl text:display-outline-level="0" text:name="Drawing"></text:sequence-decl>
        </text:sequence-decls>
        
        
        
        
        <xsl:choose>
          <!-- bookmap -->
          <xsl:when test="$mapType = &apos;bookmap&apos;">
            <xsl:call-template name="create_book_title"></xsl:call-template>
            
            <xsl:call-template name="create_book_abstract"></xsl:call-template>
            
            <xsl:call-template name="create_book_notices"></xsl:call-template>
          </xsl:when>
          <!-- normal map -->
          <xsl:when test="$mapType = &apos;ditamap&apos;">
            <xsl:call-template name="create_map_title"></xsl:call-template>
          </xsl:when>
          <!-- topic -->
          <xsl:otherwise>
            <xsl:call-template name="create_topic_title"></xsl:call-template>
          </xsl:otherwise>
          
        </xsl:choose>
        
        
        <xsl:call-template name="create_toc"></xsl:call-template>
        
        
        <xsl:apply-templates></xsl:apply-templates>
      </office:text>
    </office:body>
  </xsl:template>
  
</xsl:stylesheet>