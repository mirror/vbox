<?xml version="1.0" encoding="utf-8"?><!-- This file is part of the DITA Open Toolkit project hosted on 
     Sourceforge.net. See the accompanying license.txt file for 
     applicable licenses.--><!-- (c) Copyright IBM Corp. 2004, 2005 All Rights Reserved. --><!--  
 | Composite DITA topics to FO

 *--><!-- entities for use in the generated output (Unicode typographic glyphs) --><!-- create some fixed values for now for customizing later --><xsl:stylesheet version="1.0" xmlns:fo="http://www.w3.org/1999/XSL/Format" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <!-- stylesheet imports -->
  <xsl:import href="xslfo/topic2foImpl.xsl"></xsl:import>
  <xsl:import href="xslfo/domains2fo.xsl"></xsl:import>
  <!-- XSL-FO output with XML syntax; no actual doctype for FO -->
  <xsl:output method="xml" version="1.0" indent="yes"></xsl:output>
  <!-- CONTROL PARAMETERS: -->
  <!-- offset -->
  <xsl:param name="basic-start-indent">72pt</xsl:param>
  <xsl:param name="basic-end-indent">24pt</xsl:param>
  <xsl:param name="output-related-links" select="&apos;no&apos;"></xsl:param>
  <!-- GLOBALS: -->
  <xsl:param name="dflt-ext">.jpg</xsl:param>
  <!-- For Antenna House, set to ".jpg" -->
  <!-- Set the prefix for error message numbers -->
  <xsl:variable name="msgprefix">DOTX</xsl:variable>
  <!-- =============== start of override tweaks ============== -->
  <!-- force draft mode on all the time -->
  <xsl:param name="DRAFT" select="&apos;no&apos;"></xsl:param>
  <!-- ====================== template rules for merged content ==================== -->
  <xsl:template match="dita" mode="toplevel">
    <xsl:call-template name="dita-setup"></xsl:call-template>
  </xsl:template>
  <xsl:template match="*[contains(@class,&apos; map/map &apos;)]" mode="toplevel">
    <xsl:call-template name="dita-setup"></xsl:call-template>
  </xsl:template>
  <!-- note that bkinfo provides information for a cover in a bookmap application, 
     hence bkinfo does not need to be instanced.  In an application that processes
     only maps, bkinfo should process as a topic based on default processing. -->
  <xsl:template match="*[contains(@class,&apos; bkinfo/bkinfo &apos;)]" priority="2">
    <!-- no operation for bkinfo -->
  </xsl:template>
  <!-- =========================== overall output organization ========================= -->
  <!-- this template rule defines the overall output organization -->
  <xsl:template name="dita-setup">
    <fo:root xmlns:fo="http://www.w3.org/1999/XSL/Format">
      <!-- get the overall master page defs here -->
      <xsl:call-template name="define-page-masters-dita"></xsl:call-template>
      <fo:bookmark-tree>
        <!-- create FOP outline elements for PDF bookmarks -->
	<xsl:apply-templates mode="outline"></xsl:apply-templates>
      </fo:bookmark-tree>
      <!-- place generated content -->
      <xsl:call-template name="front-covers"></xsl:call-template>
      <!--xsl:call-template name="titlepage-ednotice"/-->
      <xsl:call-template name="generated-frontmatter"></xsl:call-template>
      <!-- place main content (fall through occurs here) -->
      <xsl:call-template name="main-doc3"></xsl:call-template>
      <!-- return to place closing generated content -->
      <!--xsl:call-template name="index-chapter"/-->
      <!--xsl:call-template name="back-covers"/-->
    </fo:root>
  </xsl:template>
  <!-- create FOP outline elements for PDF bookmarks -->
  <xsl:template match="*[contains(@class,&apos; topic/topic &apos;)]" mode="outline">
      <xsl:variable name="id-value">
        <xsl:choose>
          <xsl:when test="@id">
            <xsl:value-of select="@id"></xsl:value-of>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="generate-id()"></xsl:value-of>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      
      <fo:bookmark>
        <xsl:attribute name="internal-destination">
          <!-- use id attribute node to generate anchor for PDF bookmark fix bug#1304859 -->
          <xsl:value-of select="$id-value"></xsl:value-of>
        </xsl:attribute>
        <fo:bookmark-title>
          <!-- if topic contains navtitle, use that as label for PDF bookmark
               otherwise, use title -->
          <xsl:choose>
            <xsl:when test="*[contains(@class,&apos; topic/titlealts &apos;)]/*[contains(@class, &apos; topic/navtitle &apos;)]">
              <xsl:apply-templates select="*[contains(@class,&apos; topic/titlealts &apos;)]/*[contains(@class, &apos; topic/navtitle &apos;)]" mode="text-only"></xsl:apply-templates>
            </xsl:when>
            <xsl:otherwise>
              <xsl:apply-templates select="title" mode="text-only"></xsl:apply-templates>
            </xsl:otherwise>
          </xsl:choose>
        </fo:bookmark-title>
        <xsl:apply-templates select="child::*[contains(@class,&apos; topic/topic &apos;)]" mode="outline"></xsl:apply-templates>
      </fo:bookmark>
  </xsl:template>
  
  <xsl:template match="*" mode="text-only">
    <xsl:apply-templates select="text()|*" mode="text-only"></xsl:apply-templates>
  </xsl:template>
  <xsl:template name="define-page-masters-dita">
    <fo:layout-master-set>
      <!-- master set for chapter pages, first page is the title page -->
      <fo:page-sequence-master master-name="chapter-master">
        <fo:repeatable-page-master-alternatives>
          <fo:conditional-page-master-reference page-position="first" odd-or-even="odd" master-reference="common-page"></fo:conditional-page-master-reference>
          <!-- chapter-first-odd -->
          <fo:conditional-page-master-reference page-position="first" odd-or-even="even" master-reference="common-page"></fo:conditional-page-master-reference>
          <!--chapter-first-even"/-->
          <fo:conditional-page-master-reference page-position="rest" odd-or-even="odd" master-reference="common-page"></fo:conditional-page-master-reference>
          <!--chapter-rest-odd"/-->
          <fo:conditional-page-master-reference page-position="rest" odd-or-even="even" master-reference="common-page"></fo:conditional-page-master-reference>
          <!--chapter-rest-even"/-->
        </fo:repeatable-page-master-alternatives>
      </fo:page-sequence-master>
      <fo:simple-page-master master-name="cover" xsl:use-attribute-sets="common-grid">
        <fo:region-body margin-top="72pt"></fo:region-body>
      </fo:simple-page-master>
      <fo:simple-page-master master-name="common-page" xsl:use-attribute-sets="common-grid">
        <fo:region-body margin-bottom="36pt" margin-top="12pt"></fo:region-body>
        <fo:region-before extent="12pt"></fo:region-before>
        <fo:region-after extent="24pt"></fo:region-after>
      </fo:simple-page-master>
    </fo:layout-master-set>
  </xsl:template>
  <xsl:template name="front-covers">
    <!-- generate an "outside front cover" page (right side) (sheet 1) -->
    <fo:page-sequence master-reference="cover">
      <fo:flow flow-name="xsl-region-body">
        <fo:block text-align="right" font-family="Helvetica">
          <!-- set the title -->
          <fo:block font-size="30pt" font-weight="bold" line-height="140%">
            <xsl:choose>
              <xsl:when test="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]">
                <xsl:value-of select="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]/*[contains(@class,&apos; topic/title &apos;)]"></xsl:value-of>
                <!-- use the id attribute of the bkinfo element as an anchor for a PDF bookmark to the cover page -->
                <xsl:apply-templates select="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]/@id"></xsl:apply-templates>
              </xsl:when>
              <xsl:when test="@title"><xsl:value-of select="@title"></xsl:value-of></xsl:when>
              <xsl:otherwise><xsl:value-of select="//*/title"></xsl:value-of></xsl:otherwise>
            </xsl:choose>
          </fo:block>
          <!-- set the subtitle -->
          <fo:block font-size="24pt" font-weight="bold" line-height="140%" margin-bottom="1in">
            <xsl:value-of select="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]/*[contains(@class,&apos; bkinfo/bktitlealts &apos;)]/*[contains(@class,&apos; bkinfo/bksubtitle &apos;)]"></xsl:value-of>
          </fo:block>
          <!-- place authors as a vertical list -->
          <fo:block font-size="11pt" font-weight="bold" line-height="1.5">
            <xsl:text>[vertical list of authors]</xsl:text>
          </fo:block>
          <xsl:for-each select="//author">
            <xsl:variable name="authorid1" select="generate-id(.)"></xsl:variable>   
			<xsl:variable name="authorid2" select="generate-id(//author[.=current()])"></xsl:variable>
			<xsl:if test="$authorid1=$authorid2">
			  <fo:block font-size="11pt" font-weight="bold" line-height="1.5">
				[<xsl:value-of select="."></xsl:value-of>]
			  </fo:block>
			</xsl:if>            
		  </xsl:for-each>
          <!-- set the brief copyright notice -->
          <fo:block margin-top="3pc" font-size="11pt" font-weight="bold" line-height="normal"> ©    Copyright
                <xsl:value-of select="//*[contains(@class,&apos; bkinfo/orgname &apos;)]"></xsl:value-of>
            <xsl:text></xsl:text>
            <xsl:value-of select="//*[contains(@class,&apos; bkinfo/bkcopyrfirst &apos;)]"></xsl:value-of>,<xsl:value-of select="//*[contains(@class,&apos; bkinfo/bkcopyrlast &apos;)]"></xsl:value-of>. </fo:block>
        </fo:block>
        <!-- Custom cover art/text goes here -->
        <xsl:call-template name="place-cover-art"></xsl:call-template>
        <!-- End of custom art section -->
      </fo:flow>
    </fo:page-sequence>
    <!-- generate an "inside front cover" page (left side) (sheet 2) -->
    <fo:page-sequence master-reference="cover">
      <fo:flow flow-name="xsl-region-body">
        <fo:block xsl:use-attribute-sets="p" color="purple" text-align="center"></fo:block>
      </fo:flow>
    </fo:page-sequence>
  </xsl:template>
  <xsl:template name="place-cover-art">
    <!-- product specific art, etc. -->
    <fo:block margin-top="2pc" font-family="Helvetica" border-style="dashed" border-color="black" border-width="thin" padding="6pt">
      <fo:block font-size="12pt" line-height="100%" margin-top="12pc" margin-bottom="12pc" text-align="center">
        <fo:inline color="purple" font-weight="bold">[cover art/text goes here]</fo:inline>
        <!-- one might imbed SVG directly here for use with FOP, for instance -->
      </fo:block>
    </fo:block>
  </xsl:template>
  <!-- internal title page -->
  <!-- edition notices -->
  <!-- document notice data -->
  <!-- grant of usage data -->
  <!-- copyright info -->
  <!-- disclaimers -->
  <!-- redirect to notices page -->
  <!-- definitions for placement of Front Matter content -->
  <xsl:template name="generated-frontmatter">
    <fo:page-sequence master-reference="common-page" format="i" initial-page-number="1">
      <!-- Static setup for the generated pages -->
      <!-- header -->
      <fo:static-content flow-name="xsl-region-before">
        <!-- SF Bug 1407646: uses the map title when it is specified. 
             If the map title is not specified, we fall back to the 
             title of the first topic. -->
        <xsl:variable name="booktitle">
          <xsl:choose>
            <xsl:when test="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]">
              <xsl:value-of select="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]/*[contains(@class,&apos; topic/title &apos;)]"></xsl:value-of>
              <!-- use the id attribute of the bkinfo element as an anchor for a PDF bookmark to the cover page -->
              <xsl:apply-templates select="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]/@id"></xsl:apply-templates>
            </xsl:when>
            <xsl:when test="@title">
              <xsl:value-of select="@title"></xsl:value-of>
            </xsl:when>
            <xsl:otherwise><xsl:value-of select="//*/title"></xsl:value-of></xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <fo:block font-size="8pt" line-height="8pt">
          <xsl:value-of select="$booktitle"></xsl:value-of>
        </fo:block>
      </fo:static-content>
      <!-- footer -->
      <fo:static-content flow-name="xsl-region-after">
        <fo:block text-align="center" font-size="10pt" font-weight="bold" font-family="Helvetica">
          <fo:page-number></fo:page-number>
        </fo:block>
      </fo:static-content>
      <!-- Flow setup for the Front Matter "body" (new "chapters" start on odd pages) -->
      <fo:flow flow-name="xsl-region-body">
        <!-- first, generate a compulsory Table of Contents -->
        <fo:block line-height="12pt" font-size="10pt" font-family="Helvetica" id="page1-1">
          <fo:block text-align="left" font-family="Helvetica">
            <fo:block>
              <fo:leader color="black" leader-pattern="rule" rule-thickness="3pt" leader-length="2in"></fo:leader>
            </fo:block>
            <fo:block font-size="20pt" font-weight="bold" line-height="140%">
              Contents </fo:block>
            <xsl:call-template name="gen-toc"></xsl:call-template>
          </fo:block>
        </fo:block>
      </fo:flow>
    </fo:page-sequence>
  </xsl:template>
  <xsl:template name="unused-toc">
    <!-- generate the List of Figures -->
    <!-- To be done
        <fo:block text-align="left" font-family="Helvetica" break-before="page">
    <fo:block><fo:leader color="black" leader-pattern="rule" rule-thickness="3pt" leader-length="2in"/></fo:block>
          <fo:block font-size="20pt" font-weight="bold" line-height="140%">
            Figures
          </fo:block>
          <xsl:call-template name="gen-figlist"/>
        </fo:block>
-->
    <!-- generate the List of Tables -->
    <!-- To be done
        <fo:block text-align="left" font-family="Helvetica" break-before="page">
    <fo:block><fo:leader color="black" leader-pattern="rule" rule-thickness="3pt" leader-length="2in"/></fo:block>
          <fo:block font-size="20pt" font-weight="bold" line-height="140%">
            Tables
          </fo:block>
          <xsl:call-template name="gen-tlist"/>
        </fo:block>
-->
    <!-- To be done: while still in Roman numbering, all the bkfrontm content... -->
  </xsl:template>
  <!-- initiate main content processing within basic page "shell" -->
  <xsl:template name="main-doc3">
    <fo:page-sequence master-reference="chapter-master">
      <!-- header: single page -->
      <fo:static-content flow-name="xsl-region-before">
        <!-- SF Bug 1407646: uses the map title when it is specified. 
             If the map title is not specified, we fall back to the 
             title of the first topic. -->
        <!-- book title here -->
        <xsl:variable name="booktitle">
          <xsl:choose>
            <xsl:when test="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]">
              <xsl:value-of select="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]/*[contains(@class,&apos; topic/title &apos;)]"></xsl:value-of>
              <!-- use the id attribute of the bkinfo element as an anchor for a PDF bookmark to the cover page -->
              <xsl:apply-templates select="//*[contains(@class,&apos; bkinfo/bkinfo &apos;)]/@id"></xsl:apply-templates>
            </xsl:when>
            <xsl:when test="@title">
              <xsl:value-of select="@title"></xsl:value-of>
            </xsl:when>
            <xsl:otherwise><xsl:value-of select="//*/title"></xsl:value-of></xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <fo:block font-size="8pt" line-height="8pt">
          <xsl:value-of select="$booktitle"></xsl:value-of>
        </fo:block>
      </fo:static-content>
      <!-- footer static stuff -->
      <fo:static-content flow-name="xsl-region-after">
        <fo:block text-align="center" font-size="10pt" font-weight="bold" font-family="Helvetica">
          <fo:page-number></fo:page-number>
        </fo:block>
      </fo:static-content>
      <!-- special footers for first page of new chapter -->
      <!-- Flow setup for the main content (frontm, body, backm) (new "chapters" start on odd pages) -->
      <fo:flow flow-name="xsl-region-body">
        <!-- chapter body content here -->
        <fo:block text-align="left" font-size="10pt" font-family="Helvetica" break-before="page">
          <xsl:apply-templates></xsl:apply-templates>
        </fo:block>
      </fo:flow>
    </fo:page-sequence>
  </xsl:template>
  <!-- set up common attributes for all page definitions -->
  <xsl:attribute-set name="common-grid">
    <xsl:attribute name="page-width">51pc</xsl:attribute>
    <!-- A4: 210mm -->
    <xsl:attribute name="page-height">66pc</xsl:attribute>
    <!-- A4: 297mm -->
    <xsl:attribute name="margin-top">3pc</xsl:attribute>
    <xsl:attribute name="margin-bottom">3pc</xsl:attribute>
    <xsl:attribute name="margin-left">6pc</xsl:attribute>
    <xsl:attribute name="margin-right">6pc</xsl:attribute>
  </xsl:attribute-set>
  <!-- set up common attributes for all page definitions -->
  <xsl:attribute-set name="maptitle">
    <xsl:attribute name="font-size">16pt</xsl:attribute>
    <xsl:attribute name="font-weight">bold</xsl:attribute>
  </xsl:attribute-set>
  <!-- set up common attributes for all page definitions -->
  <xsl:attribute-set name="mapabstract">
    <xsl:attribute name="margin-top">3pc</xsl:attribute>
    <xsl:attribute name="margin-bottom">3pc</xsl:attribute>
    <xsl:attribute name="margin-left">6pc</xsl:attribute>
    <xsl:attribute name="margin-right">6pc</xsl:attribute>
  </xsl:attribute-set>
  <!-- main toc generator -->
  <xsl:template name="gen-toc">
    <!-- get by main part: body -->
    <xsl:for-each select="//bookmap//*[contains(@class,&apos; topic/topic &apos;)]|//map/*[contains(@class,&apos; topic/topic &apos;)]">
      <fo:block text-align-last="justify" margin-top="6pt" margin-left="4.9pc">
        <fo:inline font-weight="bold">
          <!--Chapter <xsl:number level="any" from="bookmap"/>. -->
          <xsl:value-of select="*[contains(@class,&apos; topic/title &apos;)]"></xsl:value-of>
        </fo:inline>
        <fo:leader leader-pattern="dots"></fo:leader>
        <xsl:variable name="id-value">
          <xsl:choose>
            <xsl:when test="@id">
              <xsl:value-of select="@id"></xsl:value-of>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="generate-id()"></xsl:value-of>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <fo:page-number-citation ref-id="{$id-value}"></fo:page-number-citation>
      </fo:block>
      <xsl:call-template name="get-tce2-section"></xsl:call-template>
    </xsl:for-each>
  </xsl:template>
  <!-- 2nd level header -->
  <xsl:template name="get-tce2-section">
    <xsl:for-each select="*[contains(@class,&apos; topic/topic &apos;)]">
      <fo:block text-align-last="justify" margin-left="7.5pc">
        <fo:inline font-weight="bold">
          <xsl:value-of select="*[contains(@class,&apos; topic/title &apos;)]"></xsl:value-of>
        </fo:inline>
        <fo:leader leader-pattern="dots"></fo:leader>
        <xsl:variable name="id-value">
          <xsl:choose>
            <xsl:when test="@id">
              <xsl:value-of select="@id"></xsl:value-of>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="generate-id()"></xsl:value-of>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <fo:page-number-citation ref-id="{$id-value}"></fo:page-number-citation>
      </fo:block>
      <xsl:call-template name="get-tce3-section"></xsl:call-template>
    </xsl:for-each>
  </xsl:template>
  <!-- 3nd level header -->
  <xsl:template name="get-tce3-section">
    <xsl:for-each select="*[contains(@class,&apos; topic/topic &apos;)]">
      <fo:block text-align-last="justify" margin-left="9pc">
        <xsl:value-of select="*[contains(@class,&apos; topic/title &apos;)]"></xsl:value-of>
        <fo:leader leader-pattern="dots"></fo:leader>
        <xsl:variable name="id-value">
          <xsl:choose>
            <xsl:when test="@id">
              <xsl:value-of select="@id"></xsl:value-of>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="generate-id()"></xsl:value-of>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <fo:page-number-citation ref-id="{$id-value}"></fo:page-number-citation>
      </fo:block>
      <xsl:call-template name="get-tce4-section"></xsl:call-template>
    </xsl:for-each>
  </xsl:template>
  <!-- 4th level header -->
  <xsl:template name="get-tce4-section">
    <xsl:for-each select="bksubsect1">
      <fo:block text-align-last="justify" margin-left="+5.9pc">
        <xsl:value-of select="*/title"></xsl:value-of>
        <fo:leader leader-pattern="dots"></fo:leader>
        <xsl:variable name="id-value">
          <xsl:choose>
            <xsl:when test="@id">
              <xsl:value-of select="@id"></xsl:value-of>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="generate-id()"></xsl:value-of>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <fo:page-number-citation ref-id="{$id-value}"></fo:page-number-citation>
      </fo:block>
      <xsl:call-template name="get-tce5-section"></xsl:call-template>
    </xsl:for-each>
  </xsl:template>
  <!-- 5th level header -->
  <xsl:template name="get-tce5-section">
    <xsl:for-each select="bksubsect2">
      <fo:block text-align-last="justify" margin-left="+5.9pc">
        <xsl:value-of select="*/title"></xsl:value-of>
        <fo:leader leader-pattern="dots"></fo:leader>
        <xsl:variable name="id-value">
          <xsl:choose>
            <xsl:when test="@id">
              <xsl:value-of select="@id"></xsl:value-of>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="generate-id()"></xsl:value-of>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
        <fo:page-number-citation ref-id="{$id-value}"></fo:page-number-citation>
      </fo:block>
      <!--xsl:call-template name="get-tce6-section"/-->
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>