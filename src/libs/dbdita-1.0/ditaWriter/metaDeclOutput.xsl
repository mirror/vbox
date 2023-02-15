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

   <xsl:template match="*" mode="topic.audience.out">
      <xsl:param name="container"/>
      <audience>
         <xsl:apply-templates match="*" mode="topic.type.in">
            <xsl:with-param name="container" select="' topic/audience '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.othertype.in">
            <xsl:with-param name="container" select="' topic/audience '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.job.in">
            <xsl:with-param name="container" select="' topic/audience '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.otherjob.in">
            <xsl:with-param name="container" select="' topic/audience '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.experiencelevel.in">
            <xsl:with-param name="container" select="' topic/audience '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.name.in">
            <xsl:with-param name="container" select="' topic/audience '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.select.attributes.in">
            <xsl:with-param name="container" select="' topic/audience '"/>
         </xsl:apply-templates>
      </audience>
   </xsl:template>

   <xsl:template match="*" mode="topic.author.out">
      <xsl:param name="container"/>
      <author>
         <xsl:apply-templates match="*" mode="topic.href.in">
            <xsl:with-param name="container" select="' topic/author '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.keyref.in">
            <xsl:with-param name="container" select="' topic/author '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.type.in">
            <xsl:with-param name="container" select="' topic/author '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.author.text.in">
            <xsl:with-param name="container" select="' topic/author '"/>
         </xsl:apply-templates>
      </author>
   </xsl:template>

   <xsl:template match="*" mode="topic.brand.out">
      <xsl:param name="container"/>
      <brand>
         <xsl:apply-templates match="*" mode="topic.brand.text.in">
            <xsl:with-param name="container" select="' topic/brand '"/>
         </xsl:apply-templates>
      </brand>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.out">
      <xsl:param name="container"/>
      <category>
         <xsl:apply-templates match="*" mode="topic.select.attributes.in">
            <xsl:with-param name="container" select="' topic/category '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.category.text.in">
            <xsl:with-param name="container" select="' topic/category '"/>
         </xsl:apply-templates>
      </category>
   </xsl:template>

   <xsl:template match="*" mode="topic.component.out">
      <xsl:param name="container"/>
      <component>
         <xsl:apply-templates match="*" mode="topic.component.text.in">
            <xsl:with-param name="container" select="' topic/component '"/>
         </xsl:apply-templates>
      </component>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyholder.out">
      <xsl:param name="container"/>
      <copyholder>
         <xsl:apply-templates match="*" mode="topic.copyholder.text.in">
            <xsl:with-param name="container" select="' topic/copyholder '"/>
         </xsl:apply-templates>
      </copyholder>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyright.out">
      <xsl:param name="container"/>
      <copyright>
         <xsl:apply-templates match="*" mode="topic.type.in">
            <xsl:with-param name="container" select="' topic/copyright '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.copyryear.in">
            <xsl:with-param name="container" select="' topic/copyright '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.copyrholder.in">
            <xsl:with-param name="container" select="' topic/copyright '"/>
         </xsl:apply-templates>
      </copyright>
   </xsl:template>

   <xsl:template match="*" mode=".copyryear.out">
      <xsl:param name="container"/>
      <copyryear>
         <xsl:apply-templates match="*" mode="topic.year.in">
            <xsl:with-param name="container" select="' topic/copyryear '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.select.attributes.in">
            <xsl:with-param name="container" select="' topic/copyryear '"/>
         </xsl:apply-templates>
      </copyryear>
   </xsl:template>

   <xsl:template match="*" mode="topic.created.out">
      <xsl:param name="container"/>
      <created>
         <xsl:apply-templates match="*" mode="topic.date.in">
            <xsl:with-param name="container" select="' topic/created '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.golive.in">
            <xsl:with-param name="container" select="' topic/created '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.expiry.in">
            <xsl:with-param name="container" select="' topic/created '"/>
         </xsl:apply-templates>
      </created>
   </xsl:template>

   <xsl:template match="*" mode="topic.critdates.out">
      <xsl:param name="container"/>
      <critdates>
         <xsl:apply-templates match="*" mode="topic.created.in">
            <xsl:with-param name="container" select="' topic/critdates '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.revised.in">
            <xsl:with-param name="container" select="' topic/critdates '"/>
         </xsl:apply-templates>
      </critdates>
   </xsl:template>

   <xsl:template match="*" mode="topic.featnum.out">
      <xsl:param name="container"/>
      <featnum>
         <xsl:apply-templates match="*" mode="topic.featnum.text.in">
            <xsl:with-param name="container" select="' topic/featnum '"/>
         </xsl:apply-templates>
      </featnum>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.out">
      <xsl:param name="container"/>
      <indexterm>
         <xsl:apply-templates match="*" mode="topic.keyref.in">
            <xsl:with-param name="container" select="' topic/indexterm '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.univ.attributes.in">
            <xsl:with-param name="container" select="' topic/indexterm '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.indexterm.text.in">
            <xsl:with-param name="container" select="' topic/indexterm '"/>
         </xsl:apply-templates>
      </indexterm>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.out">
      <xsl:param name="container"/>
      <keywords>
         <xsl:apply-templates match="*" mode="topic.id.attributes.in">
            <xsl:with-param name="container" select="' topic/keywords '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.select.attributes.in">
            <xsl:with-param name="container" select="' topic/keywords '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.keywords.choice.in">
            <xsl:with-param name="container" select="' topic/keywords '"/>
         </xsl:apply-templates>
      </keywords>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.out">
      <xsl:param name="container"/>
      <othermeta>
         <xsl:apply-templates match="*" mode="topic.name.in">
            <xsl:with-param name="container" select="' topic/othermeta '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.content.in">
            <xsl:with-param name="container" select="' topic/othermeta '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.translate-content.in">
            <xsl:with-param name="container" select="' topic/othermeta '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.select.attributes.in">
            <xsl:with-param name="container" select="' topic/othermeta '"/>
         </xsl:apply-templates>
      </othermeta>
   </xsl:template>

   <xsl:template match="*" mode="topic.permissions.out">
      <xsl:param name="container"/>
      <permissions>
         <xsl:apply-templates match="*" mode="topic.view.in">
            <xsl:with-param name="container" select="' topic/permissions '"/>
         </xsl:apply-templates>
      </permissions>
   </xsl:template>

   <xsl:template match="*" mode="topic.platform.out">
      <xsl:param name="container"/>
      <platform>
         <xsl:apply-templates match="*" mode="topic.platform.text.in">
            <xsl:with-param name="container" select="' topic/platform '"/>
         </xsl:apply-templates>
      </platform>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.out">
      <xsl:param name="container"/>
      <prodinfo>
         <xsl:apply-templates match="*" mode="topic.select.attributes.in">
            <xsl:with-param name="container" select="' topic/prodinfo '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.prodname.in">
            <xsl:with-param name="container" select="' topic/prodinfo '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.vrmlist.in">
            <xsl:with-param name="container" select="' topic/prodinfo '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.prodinfo.content.in">
            <xsl:with-param name="container" select="' topic/prodinfo '"/>
         </xsl:apply-templates>
      </prodinfo>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodname.out">
      <xsl:param name="container"/>
      <prodname>
         <xsl:apply-templates match="*" mode="topic.prodname.text.in">
            <xsl:with-param name="container" select="' topic/prodname '"/>
         </xsl:apply-templates>
      </prodname>
   </xsl:template>

   <xsl:template match="*" mode="topic.prognum.out">
      <xsl:param name="container"/>
      <prognum>
         <xsl:apply-templates match="*" mode="topic.prognum.text.in">
            <xsl:with-param name="container" select="' topic/prognum '"/>
         </xsl:apply-templates>
      </prognum>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.out">
      <xsl:param name="container"/>
      <publisher>
         <xsl:apply-templates match="*" mode="topic.href.in">
            <xsl:with-param name="container" select="' topic/publisher '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.keyref.in">
            <xsl:with-param name="container" select="' topic/publisher '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.select.attributes.in">
            <xsl:with-param name="container" select="' topic/publisher '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.publisher.text.in">
            <xsl:with-param name="container" select="' topic/publisher '"/>
         </xsl:apply-templates>
      </publisher>
   </xsl:template>

   <xsl:template match="*" mode="topic.resourceid.out">
      <xsl:param name="container"/>
      <resourceid>
         <xsl:apply-templates match="*" mode="topic.id.in">
            <xsl:with-param name="container" select="' topic/resourceid '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.appname.in">
            <xsl:with-param name="container" select="' topic/resourceid '"/>
         </xsl:apply-templates>
      </resourceid>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.out">
      <xsl:param name="container"/>
      <revised>
         <xsl:apply-templates match="*" mode="topic.modified.in">
            <xsl:with-param name="container" select="' topic/revised '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.golive.in">
            <xsl:with-param name="container" select="' topic/revised '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.expiry.in">
            <xsl:with-param name="container" select="' topic/revised '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.select.attributes.in">
            <xsl:with-param name="container" select="' topic/revised '"/>
         </xsl:apply-templates>
      </revised>
   </xsl:template>

   <xsl:template match="*" mode="topic.series.out">
      <xsl:param name="container"/>
      <series>
         <xsl:apply-templates match="*" mode="topic.series.text.in">
            <xsl:with-param name="container" select="' topic/series '"/>
         </xsl:apply-templates>
      </series>
   </xsl:template>

   <xsl:template match="*" mode="topic.source.out">
      <xsl:param name="container"/>
      <source>
         <xsl:apply-templates match="*" mode="topic.href.in">
            <xsl:with-param name="container" select="' topic/source '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.keyref.in">
            <xsl:with-param name="container" select="' topic/source '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.source.text.in">
            <xsl:with-param name="container" select="' topic/source '"/>
         </xsl:apply-templates>
      </source>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrm.out">
      <xsl:param name="container"/>
      <vrm>
         <xsl:apply-templates match="*" mode="topic.version.in">
            <xsl:with-param name="container" select="' topic/vrm '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.release.in">
            <xsl:with-param name="container" select="' topic/vrm '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.modification.in">
            <xsl:with-param name="container" select="' topic/vrm '"/>
         </xsl:apply-templates>
      </vrm>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrmlist.out">
      <xsl:param name="container"/>
      <vrmlist>
         <xsl:apply-templates match="*" mode="topic.vrm.in">
            <xsl:with-param name="container" select="' topic/vrmlist '"/>
         </xsl:apply-templates>
      </vrmlist>
   </xsl:template>

</xsl:stylesheet>
