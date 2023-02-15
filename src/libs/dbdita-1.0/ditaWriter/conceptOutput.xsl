<?xml version="1.0" encoding="utf-8" standalone="no"?>
<!--
 | LICENSE: This file is part of the DITA Open Toolkit project hosted on
 |          Sourceforge.net. See the accompanying license.txt file for
 |          applicable licenses.
 *-->
<!--
 | (C) Copyright IBM Corporation 2006. All Rights Reserved.
 *-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:gsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!--= = = ELEMENT OUTPUT RULES  = = = = = = = = = =-->

   <xsl:template match="*" mode="concept.conbody.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' concept/conbody '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="concept.conbody.out">
      <conbody>
         <xsl:apply-templates select="." mode="concept.conbody.atts.in"/>
         <xsl:apply-templates select="." mode="concept.conbody.content.in"/>
      </conbody>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' concept/concept '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.out">
      <concept>
         <xsl:apply-templates select="." mode="concept.concept.atts.in"/>
         <xsl:apply-templates select="." mode="concept.concept.content.in"/>
      </concept>
   </xsl:template>


<!--= = = DEFAULT ELEMENT INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="concept.conbody.atts.in">
      <xsl:apply-templates select="." mode="concept.conbody.id.atts.in"/>
      <xsl:apply-templates select="." mode="concept.conbody.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="concept.conbody.id.atts.in">
      <xsl:apply-templates select="." mode="concept.conbody.id.att.in"/>
      <xsl:apply-templates select="." mode="concept.conbody.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="concept.conbody.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.conbody.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.conbody.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.conbody.content.in">
      <xsl:apply-templates select="*" mode="concept.conbody.child"/>
   </xsl:template>

   <xsl:template match="*" mode="concept.conbody.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' concept/conbody '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.atts.in">
      <xsl:attribute name="xml:lang">
         <xsl:text>en-us</xsl:text>
      </xsl:attribute>
      <xsl:apply-templates select="." mode="concept.concept.select.atts.in"/>
      <xsl:apply-templates select="." mode="concept.concept.id.att.in"/>
      <xsl:apply-templates select="." mode="concept.concept.conref.att.in"/>
      <xsl:apply-templates select="." mode="concept.concept.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.select.atts.in">
      <xsl:apply-templates select="." mode="concept.concept.platform.att.in"/>
      <xsl:apply-templates select="." mode="concept.concept.product.att.in"/>
      <xsl:apply-templates select="." mode="concept.concept.audience.att.in"/>
      <xsl:apply-templates select="." mode="concept.concept.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="concept.concept.rev.att.in"/>
      <xsl:apply-templates select="." mode="concept.concept.importance.att.in"/>
      <xsl:apply-templates select="." mode="concept.concept.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.content.in">
      <xsl:apply-templates select="." mode="concept.concept.topic.title.in"/>
      <xsl:apply-templates select="." mode="concept.concept.topic.titlealts.in"/>
      <xsl:apply-templates select="." mode="concept.concept.topic.shortdesc.in"/>
      <xsl:apply-templates select="." mode="concept.concept.topic.prolog.in"/>
      <xsl:apply-templates select="." mode="concept.concept.concept.conbody.in"/>
      <xsl:apply-templates select="." mode="concept.concept.topic.related-links.in"/>
      <xsl:apply-templates select="." mode="concept.concept.topic.topic.in"/>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.topic.title.in">
      <xsl:apply-templates select="." mode="topic.title.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' concept/concept '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.topic.titlealts.in">
      <xsl:apply-templates select="." mode="topic.titlealts.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' concept/concept '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.topic.shortdesc.in">
      <xsl:apply-templates select="." mode="topic.shortdesc.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' concept/concept '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.topic.prolog.in">
      <xsl:apply-templates select="." mode="topic.prolog.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' concept/concept '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.concept.conbody.in">
      <xsl:apply-templates select="." mode="concept.conbody.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' concept/concept '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.topic.related-links.in">
      <xsl:apply-templates select="." mode="topic.related-links.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' concept/concept '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="concept.concept.topic.topic.in">
      <xsl:apply-templates select="." mode="topic.topic.in">
         <xsl:with-param name="container" select="' concept/concept '"/>
      </xsl:apply-templates>
   </xsl:template>



</xsl:stylesheet>
