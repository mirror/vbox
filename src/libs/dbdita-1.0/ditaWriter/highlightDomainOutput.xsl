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

   <xsl:template match="*" mode="hi-d.b.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' hi-d/b '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.out">
      <b>
         <xsl:apply-templates select="." mode="hi-d.b.atts.in"/>
         <xsl:apply-templates select="." mode="hi-d.b.content.in"/>
      </b>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' hi-d/i '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.out">
      <i>
         <xsl:apply-templates select="." mode="hi-d.i.atts.in"/>
         <xsl:apply-templates select="." mode="hi-d.i.content.in"/>
      </i>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' hi-d/sub '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.out">
      <sub>
         <xsl:apply-templates select="." mode="hi-d.sub.atts.in"/>
         <xsl:apply-templates select="." mode="hi-d.sub.content.in"/>
      </sub>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' hi-d/sup '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.out">
      <sup>
         <xsl:apply-templates select="." mode="hi-d.sup.atts.in"/>
         <xsl:apply-templates select="." mode="hi-d.sup.content.in"/>
      </sup>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' hi-d/tt '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.out">
      <tt>
         <xsl:apply-templates select="." mode="hi-d.tt.atts.in"/>
         <xsl:apply-templates select="." mode="hi-d.tt.content.in"/>
      </tt>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' hi-d/u '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.out">
      <u>
         <xsl:apply-templates select="." mode="hi-d.u.atts.in"/>
         <xsl:apply-templates select="." mode="hi-d.u.content.in"/>
      </u>
   </xsl:template>


<!--= = = DEFAULT ELEMENT INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="hi-d.b.atts.in">
      <xsl:apply-templates select="." mode="hi-d.b.univ.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.b.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.univ.atts.in">
      <xsl:apply-templates select="." mode="hi-d.b.id.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.b.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.id.atts.in">
      <xsl:apply-templates select="." mode="hi-d.b.id.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.b.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.select.atts.in">
      <xsl:apply-templates select="." mode="hi-d.b.platform.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.b.product.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.b.audience.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.b.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.b.rev.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.b.importance.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.b.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.b.content.in">
      <xsl:apply-templates select="*|text()" mode="hi-d.b.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="hi-d.b.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' hi-d/b '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.atts.in">
      <xsl:apply-templates select="." mode="hi-d.i.univ.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.i.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.univ.atts.in">
      <xsl:apply-templates select="." mode="hi-d.i.id.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.i.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.id.atts.in">
      <xsl:apply-templates select="." mode="hi-d.i.id.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.i.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.select.atts.in">
      <xsl:apply-templates select="." mode="hi-d.i.platform.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.i.product.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.i.audience.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.i.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.i.rev.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.i.importance.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.i.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.i.content.in">
      <xsl:apply-templates select="*|text()" mode="hi-d.i.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="hi-d.i.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' hi-d/i '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.atts.in">
      <xsl:apply-templates select="." mode="hi-d.sub.univ.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.sub.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.univ.atts.in">
      <xsl:apply-templates select="." mode="hi-d.sub.id.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.sub.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.id.atts.in">
      <xsl:apply-templates select="." mode="hi-d.sub.id.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sub.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.select.atts.in">
      <xsl:apply-templates select="." mode="hi-d.sub.platform.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sub.product.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sub.audience.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sub.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sub.rev.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sub.importance.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sub.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sub.content.in">
      <xsl:apply-templates select="*|text()" mode="hi-d.sub.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="hi-d.sub.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' hi-d/sub '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.atts.in">
      <xsl:apply-templates select="." mode="hi-d.sup.univ.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.sup.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.univ.atts.in">
      <xsl:apply-templates select="." mode="hi-d.sup.id.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.sup.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.id.atts.in">
      <xsl:apply-templates select="." mode="hi-d.sup.id.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sup.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.select.atts.in">
      <xsl:apply-templates select="." mode="hi-d.sup.platform.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sup.product.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sup.audience.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sup.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sup.rev.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sup.importance.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.sup.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.sup.content.in">
      <xsl:apply-templates select="*|text()" mode="hi-d.sup.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="hi-d.sup.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' hi-d/sup '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.atts.in">
      <xsl:apply-templates select="." mode="hi-d.tt.univ.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.tt.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.univ.atts.in">
      <xsl:apply-templates select="." mode="hi-d.tt.id.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.tt.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.id.atts.in">
      <xsl:apply-templates select="." mode="hi-d.tt.id.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.tt.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.select.atts.in">
      <xsl:apply-templates select="." mode="hi-d.tt.platform.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.tt.product.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.tt.audience.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.tt.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.tt.rev.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.tt.importance.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.tt.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.tt.content.in">
      <xsl:apply-templates select="*|text()" mode="hi-d.tt.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="hi-d.tt.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' hi-d/tt '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.atts.in">
      <xsl:apply-templates select="." mode="hi-d.u.univ.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.u.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.univ.atts.in">
      <xsl:apply-templates select="." mode="hi-d.u.id.atts.in"/>
      <xsl:apply-templates select="." mode="hi-d.u.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.id.atts.in">
      <xsl:apply-templates select="." mode="hi-d.u.id.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.u.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.select.atts.in">
      <xsl:apply-templates select="." mode="hi-d.u.platform.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.u.product.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.u.audience.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.u.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.u.rev.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.u.importance.att.in"/>
      <xsl:apply-templates select="." mode="hi-d.u.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="hi-d.u.content.in">
      <xsl:apply-templates select="*|text()" mode="hi-d.u.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="hi-d.u.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' hi-d/u '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>



</xsl:stylesheet>
