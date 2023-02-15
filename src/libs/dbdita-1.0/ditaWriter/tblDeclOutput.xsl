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

   <xsl:template match="*" mode="topic.colspec.out">
      <xsl:param name="container"/>
      <colspec>
         <xsl:apply-templates match="*" mode="topic.colnum.in">
            <xsl:with-param name="container" select="' topic/colspec '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.colname.in">
            <xsl:with-param name="container" select="' topic/colspec '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.align.in">
            <xsl:with-param name="container" select="' topic/colspec '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.colwidth.in">
            <xsl:with-param name="container" select="' topic/colspec '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.colrowsep.attributes.in">
            <xsl:with-param name="container" select="' topic/colspec '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.char.in">
            <xsl:with-param name="container" select="' topic/colspec '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.charoff.in">
            <xsl:with-param name="container" select="' topic/colspec '"/>
         </xsl:apply-templates>
      </colspec>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.out">
      <xsl:param name="container"/>
      <entry>
         <xsl:apply-templates match="*" mode="topic.namest.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.nameend.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.spanname.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.colname.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.morerows.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.colrowsep.attributes.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.align.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.valign.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.rev.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.outputclass.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.entry.text.in">
            <xsl:with-param name="container" select="' topic/entry '"/>
         </xsl:apply-templates>
      </entry>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.out">
      <xsl:param name="container"/>
      <row>
         <xsl:apply-templates match="*" mode="topic.rowsep.in">
            <xsl:with-param name="container" select="' topic/row '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.valign.in">
            <xsl:with-param name="container" select="' topic/row '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.outputclass.in">
            <xsl:with-param name="container" select="' topic/row '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.univ.attributes.in">
            <xsl:with-param name="container" select="' topic/row '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.entry.in">
            <xsl:with-param name="container" select="' topic/row '"/>
         </xsl:apply-templates>
      </row>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.out">
      <xsl:param name="container"/>
      <table>
         <xsl:apply-templates match="*" mode="topic.colrowsep.attributes.in">
            <xsl:with-param name="container" select="' topic/table '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.pgwide.in">
            <xsl:with-param name="container" select="' topic/table '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.rowheader.in">
            <xsl:with-param name="container" select="' topic/table '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.outputclass.in">
            <xsl:with-param name="container" select="' topic/table '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.display.attributes.in">
            <xsl:with-param name="container" select="' topic/table '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.univ.attributes.in">
            <xsl:with-param name="container" select="' topic/table '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.title.in">
            <xsl:with-param name="container" select="' topic/table '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.desc.in">
            <xsl:with-param name="container" select="' topic/table '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.tgroup.in">
            <xsl:with-param name="container" select="' topic/table '"/>
         </xsl:apply-templates>
      </table>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.out">
      <xsl:param name="container"/>
      <tbody>
         <xsl:apply-templates match="*" mode="topic.valign.in">
            <xsl:with-param name="container" select="' topic/tbody '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.outputclass.in">
            <xsl:with-param name="container" select="' topic/tbody '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.univ.attributes.in">
            <xsl:with-param name="container" select="' topic/tbody '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.row.in">
            <xsl:with-param name="container" select="' topic/tbody '"/>
         </xsl:apply-templates>
      </tbody>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.out">
      <xsl:param name="container"/>
      <tgroup>
         <xsl:apply-templates match="*" mode="topic.cols.in">
            <xsl:with-param name="container" select="' topic/tgroup '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.colrowsep.attributes.in">
            <xsl:with-param name="container" select="' topic/tgroup '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.align.in">
            <xsl:with-param name="container" select="' topic/tgroup '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.outputclass.in">
            <xsl:with-param name="container" select="' topic/tgroup '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.univ.attributes.in">
            <xsl:with-param name="container" select="' topic/tgroup '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.colspec.in">
            <xsl:with-param name="container" select="' topic/tgroup '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.thead.in">
            <xsl:with-param name="container" select="' topic/tgroup '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.tbody.in">
            <xsl:with-param name="container" select="' topic/tgroup '"/>
         </xsl:apply-templates>
      </tgroup>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.out">
      <xsl:param name="container"/>
      <thead>
         <xsl:apply-templates match="*" mode="topic.valign.in">
            <xsl:with-param name="container" select="' topic/thead '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.outputclass.in">
            <xsl:with-param name="container" select="' topic/thead '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.univ.attributes.in">
            <xsl:with-param name="container" select="' topic/thead '"/>
         </xsl:apply-templates>
         <xsl:apply-templates match="*" mode="topic.row.in">
            <xsl:with-param name="container" select="' topic/thead '"/>
         </xsl:apply-templates>
      </thead>
   </xsl:template>

</xsl:stylesheet>
