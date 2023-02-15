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

   <xsl:template match="*" mode="ut-d.area.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' ut-d/area '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.out">
      <area>
         <xsl:apply-templates select="." mode="ut-d.area.atts.in"/>
         <xsl:apply-templates select="." mode="ut-d.area.content.in"/>
      </area>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' ut-d/coords '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.out">
      <coords>
         <xsl:apply-templates select="." mode="ut-d.coords.atts.in"/>
         <xsl:apply-templates select="." mode="ut-d.coords.content.in"/>
      </coords>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' ut-d/imagemap '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.out">
      <imagemap>
         <xsl:apply-templates select="." mode="ut-d.imagemap.atts.in"/>
         <xsl:apply-templates select="." mode="ut-d.imagemap.content.in"/>
      </imagemap>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' ut-d/shape '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.out">
      <shape>
         <xsl:apply-templates select="." mode="ut-d.shape.atts.in"/>
         <xsl:apply-templates select="." mode="ut-d.shape.content.in"/>
      </shape>
   </xsl:template>


<!--= = = DEFAULT ELEMENT INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="ut-d.area.atts.in">
      <xsl:apply-templates select="." mode="ut-d.area.univ.atts.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.univ.atts.in">
      <xsl:apply-templates select="." mode="ut-d.area.id.atts.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.id.atts.in">
      <xsl:apply-templates select="." mode="ut-d.area.id.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.select.atts.in">
      <xsl:apply-templates select="." mode="ut-d.area.platform.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.product.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.audience.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.rev.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.importance.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.content.in">
      <xsl:apply-templates select="." mode="ut-d.area.ut-d.shape.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.ut-d.coords.in"/>
      <xsl:apply-templates select="." mode="ut-d.area.topic.xref.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.ut-d.shape.in">
      <xsl:apply-templates select="." mode="ut-d.shape.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' ut-d/area '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.ut-d.coords.in">
      <xsl:apply-templates select="." mode="ut-d.coords.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' ut-d/area '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.area.topic.xref.in">
      <xsl:apply-templates select="." mode="topic.xref.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' ut-d/area '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.atts.in">
      <xsl:apply-templates select="." mode="ut-d.coords.univ.atts.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.keyref.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.univ.atts.in">
      <xsl:apply-templates select="." mode="ut-d.coords.id.atts.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.id.atts.in">
      <xsl:apply-templates select="." mode="ut-d.coords.id.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.select.atts.in">
      <xsl:apply-templates select="." mode="ut-d.coords.platform.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.product.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.audience.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.rev.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.importance.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.coords.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.coords.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.atts.in">
      <xsl:apply-templates select="." mode="ut-d.imagemap.univ.atts.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.display.atts.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.univ.atts.in">
      <xsl:apply-templates select="." mode="ut-d.imagemap.id.atts.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.id.atts.in">
      <xsl:apply-templates select="." mode="ut-d.imagemap.id.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.select.atts.in">
      <xsl:apply-templates select="." mode="ut-d.imagemap.platform.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.product.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.audience.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.rev.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.importance.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.display.atts.in">
      <xsl:apply-templates select="." mode="ut-d.imagemap.scale.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.frame.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.content.in">
      <xsl:apply-templates select="." mode="ut-d.imagemap.topic.image.in"/>
      <xsl:apply-templates select="." mode="ut-d.imagemap.ut-d.area.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.topic.image.in">
      <xsl:apply-templates select="." mode="topic.image.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' ut-d/imagemap '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.imagemap.ut-d.area.in">
      <xsl:apply-templates select="." mode="ut-d.area.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' ut-d/imagemap '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.atts.in">
      <xsl:apply-templates select="." mode="ut-d.shape.univ.atts.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.keyref.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.univ.atts.in">
      <xsl:apply-templates select="." mode="ut-d.shape.id.atts.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.id.atts.in">
      <xsl:apply-templates select="." mode="ut-d.shape.id.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.select.atts.in">
      <xsl:apply-templates select="." mode="ut-d.shape.platform.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.product.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.audience.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.rev.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.importance.att.in"/>
      <xsl:apply-templates select="." mode="ut-d.shape.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ut-d.shape.content.in">
      <xsl:apply-templates select="*|text()" mode="ut-d.shape.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="ut-d.shape.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' ut-d/shape '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>



</xsl:stylesheet>
