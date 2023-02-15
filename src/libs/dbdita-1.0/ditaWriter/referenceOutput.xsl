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

   <xsl:template match="*" mode="reference.propdesc.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/propdesc '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.out">
      <propdesc>
         <xsl:apply-templates select="." mode="reference.propdesc.atts.in"/>
         <xsl:apply-templates select="." mode="reference.propdesc.content.in"/>
      </propdesc>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/propdeschd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.out">
      <propdeschd>
         <xsl:apply-templates select="." mode="reference.propdeschd.atts.in"/>
         <xsl:apply-templates select="." mode="reference.propdeschd.content.in"/>
      </propdeschd>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/properties '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.out">
      <properties>
         <xsl:apply-templates select="." mode="reference.properties.atts.in"/>
         <xsl:apply-templates select="." mode="reference.properties.content.in"/>
      </properties>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/property '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.out">
      <property>
         <xsl:apply-templates select="." mode="reference.property.atts.in"/>
         <xsl:apply-templates select="." mode="reference.property.content.in"/>
      </property>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/prophead '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.out">
      <prophead>
         <xsl:apply-templates select="." mode="reference.prophead.atts.in"/>
         <xsl:apply-templates select="." mode="reference.prophead.content.in"/>
      </prophead>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/proptype '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.out">
      <proptype>
         <xsl:apply-templates select="." mode="reference.proptype.atts.in"/>
         <xsl:apply-templates select="." mode="reference.proptype.content.in"/>
      </proptype>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/proptypehd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.out">
      <proptypehd>
         <xsl:apply-templates select="." mode="reference.proptypehd.atts.in"/>
         <xsl:apply-templates select="." mode="reference.proptypehd.content.in"/>
      </proptypehd>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/propvalue '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.out">
      <propvalue>
         <xsl:apply-templates select="." mode="reference.propvalue.atts.in"/>
         <xsl:apply-templates select="." mode="reference.propvalue.content.in"/>
      </propvalue>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/propvaluehd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.out">
      <propvaluehd>
         <xsl:apply-templates select="." mode="reference.propvaluehd.atts.in"/>
         <xsl:apply-templates select="." mode="reference.propvaluehd.content.in"/>
      </propvaluehd>
   </xsl:template>

   <xsl:template match="*" mode="reference.refbody.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/refbody '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.refbody.out">
      <refbody>
         <xsl:apply-templates select="." mode="reference.refbody.atts.in"/>
         <xsl:apply-templates select="." mode="reference.refbody.content.in"/>
      </refbody>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/reference '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.out">
      <reference>
         <xsl:apply-templates select="." mode="reference.reference.atts.in"/>
         <xsl:apply-templates select="." mode="reference.reference.content.in"/>
      </reference>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' reference/refsyn '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.out">
      <refsyn>
         <xsl:apply-templates select="." mode="reference.refsyn.atts.in"/>
         <xsl:apply-templates select="." mode="reference.refsyn.content.in"/>
      </refsyn>
   </xsl:template>


<!--= = = DEFAULT ELEMENT INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="reference.propdesc.atts.in">
      <xsl:apply-templates select="." mode="reference.propdesc.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.specentry.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.propdesc.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.id.atts.in">
      <xsl:apply-templates select="." mode="reference.propdesc.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.select.atts.in">
      <xsl:apply-templates select="." mode="reference.propdesc.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdesc.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdesc.content.in">
      <xsl:apply-templates select="." mode="desc.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.atts.in">
      <xsl:apply-templates select="." mode="reference.propdeschd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.specentry.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.propdeschd.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.id.atts.in">
      <xsl:apply-templates select="." mode="reference.propdeschd.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.select.atts.in">
      <xsl:apply-templates select="." mode="reference.propdeschd.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.propdeschd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propdeschd.content.in">
      <xsl:apply-templates select="." mode="tblcell.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.atts.in">
      <xsl:apply-templates select="." mode="reference.properties.display.atts.in"/>
      <xsl:apply-templates select="." mode="reference.properties.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.properties.relcolwidth.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.keycol.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.refcols.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.spectitle.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.display.atts.in">
      <xsl:apply-templates select="." mode="reference.properties.scale.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.frame.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.properties.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.properties.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.id.atts.in">
      <xsl:apply-templates select="." mode="reference.properties.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.select.atts.in">
      <xsl:apply-templates select="." mode="reference.properties.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.properties.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.relcolwidth.att.in">
      <xsl:apply-templates select="." mode="relcolwidth.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.keycol.att.in">
      <xsl:apply-templates select="." mode="keycol.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.refcols.att.in">
      <xsl:apply-templates select="." mode="refcols.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.content.in">
      <xsl:apply-templates select="." mode="reference.properties.reference.prophead.in"/>
      <xsl:apply-templates select="." mode="reference.properties.reference.property.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.reference.prophead.in">
      <xsl:apply-templates select="." mode="reference.prophead.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/properties '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.properties.reference.property.in">
      <xsl:apply-templates select="." mode="reference.property.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' reference/properties '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.atts.in">
      <xsl:apply-templates select="." mode="reference.property.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.property.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.property.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.property.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.id.atts.in">
      <xsl:apply-templates select="." mode="reference.property.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.property.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.select.atts.in">
      <xsl:apply-templates select="." mode="reference.property.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.property.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.property.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.property.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.property.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.property.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.property.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.content.in">
      <xsl:apply-templates select="." mode="reference.property.reference.proptype.in"/>
      <xsl:apply-templates select="." mode="reference.property.reference.propvalue.in"/>
      <xsl:apply-templates select="." mode="reference.property.reference.propdesc.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.reference.proptype.in">
      <xsl:apply-templates select="." mode="reference.proptype.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/property '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.reference.propvalue.in">
      <xsl:apply-templates select="." mode="reference.propvalue.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/property '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.property.reference.propdesc.in">
      <xsl:apply-templates select="." mode="reference.propdesc.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/property '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.atts.in">
      <xsl:apply-templates select="." mode="reference.prophead.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.prophead.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.id.atts.in">
      <xsl:apply-templates select="." mode="reference.prophead.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.select.atts.in">
      <xsl:apply-templates select="." mode="reference.prophead.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.content.in">
      <xsl:apply-templates select="." mode="reference.prophead.reference.proptypehd.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.reference.propvaluehd.in"/>
      <xsl:apply-templates select="." mode="reference.prophead.reference.propdeschd.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.reference.proptypehd.in">
      <xsl:apply-templates select="." mode="reference.proptypehd.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/prophead '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.reference.propvaluehd.in">
      <xsl:apply-templates select="." mode="reference.propvaluehd.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/prophead '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.prophead.reference.propdeschd.in">
      <xsl:apply-templates select="." mode="reference.propdeschd.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/prophead '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.atts.in">
      <xsl:apply-templates select="." mode="reference.proptype.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.specentry.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.proptype.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.id.atts.in">
      <xsl:apply-templates select="." mode="reference.proptype.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.select.atts.in">
      <xsl:apply-templates select="." mode="reference.proptype.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptype.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptype.content.in">
      <xsl:apply-templates select="." mode="ph.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.atts.in">
      <xsl:apply-templates select="." mode="reference.proptypehd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.specentry.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.proptypehd.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.id.atts.in">
      <xsl:apply-templates select="." mode="reference.proptypehd.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.select.atts.in">
      <xsl:apply-templates select="." mode="reference.proptypehd.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.proptypehd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.proptypehd.content.in">
      <xsl:apply-templates select="." mode="tblcell.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.atts.in">
      <xsl:apply-templates select="." mode="reference.propvalue.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.specentry.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.propvalue.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.id.atts.in">
      <xsl:apply-templates select="." mode="reference.propvalue.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.select.atts.in">
      <xsl:apply-templates select="." mode="reference.propvalue.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvalue.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvalue.content.in">
      <xsl:apply-templates select="." mode="ph.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.atts.in">
      <xsl:apply-templates select="." mode="reference.propvaluehd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.specentry.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.propvaluehd.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.id.atts.in">
      <xsl:apply-templates select="." mode="reference.propvaluehd.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.select.atts.in">
      <xsl:apply-templates select="." mode="reference.propvaluehd.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.propvaluehd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.propvaluehd.content.in">
      <xsl:apply-templates select="." mode="tblcell.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.refbody.atts.in">
      <xsl:apply-templates select="." mode="reference.refbody.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.refbody.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.refbody.id.atts.in">
      <xsl:apply-templates select="." mode="reference.refbody.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.refbody.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.refbody.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refbody.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refbody.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refbody.content.in">
      <xsl:apply-templates select="*" mode="reference.refbody.child"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.refbody.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' reference/refbody '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.atts.in">
      <xsl:attribute name="xml:lang">
         <xsl:text>en-us</xsl:text>
      </xsl:attribute>
      <xsl:apply-templates select="." mode="reference.reference.select.atts.in"/>
      <xsl:apply-templates select="." mode="reference.reference.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.reference.conref.att.in"/>
      <xsl:apply-templates select="." mode="reference.reference.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.select.atts.in">
      <xsl:apply-templates select="." mode="reference.reference.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.reference.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.reference.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.reference.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.reference.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.reference.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.reference.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.content.in">
      <xsl:apply-templates select="." mode="reference.reference.topic.title.in"/>
      <xsl:apply-templates select="." mode="reference.reference.topic.titlealts.in"/>
      <xsl:apply-templates select="." mode="reference.reference.topic.shortdesc.in"/>
      <xsl:apply-templates select="." mode="reference.reference.topic.prolog.in"/>
      <xsl:apply-templates select="." mode="reference.reference.reference.refbody.in"/>
      <xsl:apply-templates select="." mode="reference.reference.topic.related-links.in"/>
      <xsl:apply-templates select="." mode="reference.reference.topic.topic.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.topic.title.in">
      <xsl:apply-templates select="." mode="topic.title.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' reference/reference '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.topic.titlealts.in">
      <xsl:apply-templates select="." mode="topic.titlealts.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/reference '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.topic.shortdesc.in">
      <xsl:apply-templates select="." mode="topic.shortdesc.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/reference '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.topic.prolog.in">
      <xsl:apply-templates select="." mode="topic.prolog.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/reference '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.reference.refbody.in">
      <xsl:apply-templates select="." mode="reference.refbody.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/reference '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.topic.related-links.in">
      <xsl:apply-templates select="." mode="topic.related-links.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' reference/reference '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.reference.topic.topic.in">
      <xsl:apply-templates select="." mode="topic.topic.in">
         <xsl:with-param name="container" select="' reference/reference '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.atts.in">
      <xsl:apply-templates select="." mode="reference.refsyn.univ.atts.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.univ.atts.in">
      <xsl:apply-templates select="." mode="reference.refsyn.id.atts.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.id.atts.in">
      <xsl:apply-templates select="." mode="reference.refsyn.id.att.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.select.atts.in">
      <xsl:apply-templates select="." mode="reference.refsyn.platform.att.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.product.att.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.audience.att.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.rev.att.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.importance.att.in"/>
      <xsl:apply-templates select="." mode="reference.refsyn.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="reference.refsyn.content.in">
      <xsl:apply-templates select="." mode="section.cnt.text.in"/>
   </xsl:template>



</xsl:stylesheet>
