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

   <xsl:template match="*" mode="topic.alt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/alt '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.out">
      <alt>
         <xsl:apply-templates select="." mode="topic.alt.atts.in"/>
         <xsl:apply-templates select="." mode="topic.alt.content.in"/>
      </alt>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/audience '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.out">
      <audience>
         <xsl:apply-templates select="." mode="topic.audience.atts.in"/>
      </audience>
   </xsl:template>

   <xsl:template match="*" mode="topic.author.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/author '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.author.out">
      <author>
         <xsl:apply-templates select="." mode="topic.author.atts.in"/>
         <xsl:apply-templates select="." mode="topic.author.content.in"/>
      </author>
   </xsl:template>

   <xsl:template match="*" mode="topic.body.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/body '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.body.out">
      <body>
         <xsl:apply-templates select="." mode="topic.body.atts.in"/>
         <xsl:apply-templates select="." mode="topic.body.content.in"/>
      </body>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/boolean '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.out">
      <boolean>
         <xsl:apply-templates select="." mode="topic.boolean.atts.in"/>
      </boolean>
   </xsl:template>

   <xsl:template match="*" mode="topic.brand.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/brand '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.brand.out">
      <brand>
         <xsl:apply-templates select="." mode="topic.brand.atts.in"/>
         <xsl:apply-templates select="." mode="topic.brand.content.in"/>
      </brand>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/category '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.out">
      <category>
         <xsl:apply-templates select="." mode="topic.category.atts.in"/>
         <xsl:apply-templates select="." mode="topic.category.content.in"/>
      </category>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/cite '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.out">
      <cite>
         <xsl:apply-templates select="." mode="topic.cite.atts.in"/>
         <xsl:apply-templates select="." mode="topic.cite.content.in"/>
      </cite>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/colspec '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.out">
      <colspec>
         <xsl:apply-templates select="." mode="topic.colspec.atts.in"/>
      </colspec>
   </xsl:template>

   <xsl:template match="*" mode="topic.component.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/component '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.component.out">
      <component>
         <xsl:apply-templates select="." mode="topic.component.atts.in"/>
         <xsl:apply-templates select="." mode="topic.component.content.in"/>
      </component>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyrholder.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/copyrholder '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyrholder.out">
      <copyrholder>
         <xsl:apply-templates select="." mode="topic.copyrholder.atts.in"/>
         <xsl:apply-templates select="." mode="topic.copyrholder.content.in"/>
      </copyrholder>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyright.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/copyright '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyright.out">
      <copyright>
         <xsl:apply-templates select="." mode="topic.copyright.atts.in"/>
         <xsl:apply-templates select="." mode="topic.copyright.content.in"/>
      </copyright>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/copyryear '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.out">
      <copyryear>
         <xsl:apply-templates select="." mode="topic.copyryear.atts.in"/>
      </copyryear>
   </xsl:template>

   <xsl:template match="*" mode="topic.created.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/created '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.created.out">
      <created>
         <xsl:apply-templates select="." mode="topic.created.atts.in"/>
      </created>
   </xsl:template>

   <xsl:template match="*" mode="topic.critdates.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/critdates '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.critdates.out">
      <critdates>
         <xsl:apply-templates select="." mode="topic.critdates.atts.in"/>
         <xsl:apply-templates select="." mode="topic.critdates.content.in"/>
      </critdates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/dd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.out">
      <dd>
         <xsl:apply-templates select="." mode="topic.dd.atts.in"/>
         <xsl:apply-templates select="." mode="topic.dd.content.in"/>
      </dd>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/ddhd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.out">
      <ddhd>
         <xsl:apply-templates select="." mode="topic.ddhd.atts.in"/>
         <xsl:apply-templates select="." mode="topic.ddhd.content.in"/>
      </ddhd>
   </xsl:template>

   <xsl:template match="*" mode="topic.desc.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/desc '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.desc.out">
      <desc>
         <xsl:apply-templates select="." mode="topic.desc.atts.in"/>
         <xsl:apply-templates select="." mode="topic.desc.content.in"/>
      </desc>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/dl '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.out">
      <dl>
         <xsl:apply-templates select="." mode="topic.dl.atts.in"/>
         <xsl:apply-templates select="." mode="topic.dl.content.in"/>
      </dl>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/dlentry '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.out">
      <dlentry>
         <xsl:apply-templates select="." mode="topic.dlentry.atts.in"/>
         <xsl:apply-templates select="." mode="topic.dlentry.content.in"/>
      </dlentry>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/dlhead '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.out">
      <dlhead>
         <xsl:apply-templates select="." mode="topic.dlhead.atts.in"/>
         <xsl:apply-templates select="." mode="topic.dlhead.content.in"/>
      </dlhead>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/draft-comment '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.out">
      <draft-comment>
         <xsl:apply-templates select="." mode="topic.draft-comment.atts.in"/>
         <xsl:apply-templates select="." mode="topic.draft-comment.content.in"/>
      </draft-comment>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/dt '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.out">
      <dt>
         <xsl:apply-templates select="." mode="topic.dt.atts.in"/>
         <xsl:apply-templates select="." mode="topic.dt.content.in"/>
      </dt>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/dthd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.out">
      <dthd>
         <xsl:apply-templates select="." mode="topic.dthd.atts.in"/>
         <xsl:apply-templates select="." mode="topic.dthd.content.in"/>
      </dthd>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/entry '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.out">
      <entry>
         <xsl:apply-templates select="." mode="topic.entry.atts.in"/>
         <xsl:apply-templates select="." mode="topic.entry.content.in"/>
      </entry>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/example '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.out">
      <example>
         <xsl:apply-templates select="." mode="topic.example.atts.in"/>
         <xsl:apply-templates select="." mode="topic.example.content.in"/>
      </example>
   </xsl:template>

   <xsl:template match="*" mode="topic.featnum.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/featnum '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.featnum.out">
      <featnum>
         <xsl:apply-templates select="." mode="topic.featnum.atts.in"/>
         <xsl:apply-templates select="." mode="topic.featnum.content.in"/>
      </featnum>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/fig '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.out">
      <fig>
         <xsl:apply-templates select="." mode="topic.fig.atts.in"/>
         <xsl:apply-templates select="." mode="topic.fig.content.in"/>
      </fig>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/figgroup '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.out">
      <figgroup>
         <xsl:apply-templates select="." mode="topic.figgroup.atts.in"/>
         <xsl:apply-templates select="." mode="topic.figgroup.content.in"/>
      </figgroup>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/fn '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.out">
      <fn>
         <xsl:apply-templates select="." mode="topic.fn.atts.in"/>
         <xsl:apply-templates select="." mode="topic.fn.content.in"/>
      </fn>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/image '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.out">
      <image>
         <xsl:apply-templates select="." mode="topic.image.atts.in"/>
         <xsl:apply-templates select="." mode="topic.image.content.in"/>
      </image>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/indexterm '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.out">
      <indexterm>
         <xsl:apply-templates select="." mode="topic.indexterm.atts.in"/>
         <xsl:apply-templates select="." mode="topic.indexterm.content.in"/>
      </indexterm>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/indextermref '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.out">
      <indextermref>
         <xsl:apply-templates select="." mode="topic.indextermref.atts.in"/>
      </indextermref>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/itemgroup '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.out">
      <itemgroup>
         <xsl:apply-templates select="." mode="topic.itemgroup.atts.in"/>
         <xsl:apply-templates select="." mode="topic.itemgroup.content.in"/>
      </itemgroup>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/keyword '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.out">
      <keyword>
         <xsl:apply-templates select="." mode="topic.keyword.atts.in"/>
         <xsl:apply-templates select="." mode="topic.keyword.content.in"/>
      </keyword>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/keywords '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.out">
      <keywords>
         <xsl:apply-templates select="." mode="topic.keywords.atts.in"/>
         <xsl:apply-templates select="." mode="topic.keywords.content.in"/>
      </keywords>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/li '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.out">
      <li>
         <xsl:apply-templates select="." mode="topic.li.atts.in"/>
         <xsl:apply-templates select="." mode="topic.li.content.in"/>
      </li>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/lines '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.out">
      <lines>
         <xsl:apply-templates select="." mode="topic.lines.atts.in"/>
         <xsl:apply-templates select="." mode="topic.lines.content.in"/>
      </lines>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/link '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.out">
      <link>
         <xsl:apply-templates select="." mode="topic.link.atts.in"/>
         <xsl:apply-templates select="." mode="topic.link.content.in"/>
      </link>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkinfo.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/linkinfo '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkinfo.out">
      <linkinfo>
         <xsl:apply-templates select="." mode="topic.linkinfo.atts.in"/>
         <xsl:apply-templates select="." mode="topic.linkinfo.content.in"/>
      </linkinfo>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/linklist '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.out">
      <linklist>
         <xsl:apply-templates select="." mode="topic.linklist.atts.in"/>
         <xsl:apply-templates select="." mode="topic.linklist.content.in"/>
      </linklist>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/linkpool '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.out">
      <linkpool>
         <xsl:apply-templates select="." mode="topic.linkpool.atts.in"/>
         <xsl:apply-templates select="." mode="topic.linkpool.content.in"/>
      </linkpool>
   </xsl:template>

   <xsl:template match="*" mode="topic.linktext.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/linktext '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.linktext.out">
      <linktext>
         <xsl:apply-templates select="." mode="topic.linktext.atts.in"/>
         <xsl:apply-templates select="." mode="topic.linktext.content.in"/>
      </linktext>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/lq '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.out">
      <lq>
         <xsl:apply-templates select="." mode="topic.lq.atts.in"/>
         <xsl:apply-templates select="." mode="topic.lq.content.in"/>
      </lq>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/metadata '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.out">
      <metadata>
         <xsl:apply-templates select="." mode="topic.metadata.atts.in"/>
         <xsl:apply-templates select="." mode="topic.metadata.content.in"/>
      </metadata>
   </xsl:template>

   <xsl:template match="*" mode="topic.navtitle.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/navtitle '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.navtitle.out">
      <navtitle>
         <xsl:apply-templates select="." mode="topic.navtitle.atts.in"/>
         <xsl:apply-templates select="." mode="topic.navtitle.content.in"/>
      </navtitle>
   </xsl:template>

   <xsl:template match="*" mode="topic.no-topic-nesting.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/no-topic-nesting '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.no-topic-nesting.out">
      <no-topic-nesting>
         <xsl:apply-templates select="." mode="topic.no-topic-nesting.atts.in"/>
      </no-topic-nesting>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/note '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.out">
      <note>
         <xsl:apply-templates select="." mode="topic.note.atts.in"/>
         <xsl:apply-templates select="." mode="topic.note.content.in"/>
      </note>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/object '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.out">
      <object>
         <xsl:apply-templates select="." mode="topic.object.atts.in"/>
         <xsl:apply-templates select="." mode="topic.object.content.in"/>
      </object>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/ol '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.out">
      <ol>
         <xsl:apply-templates select="." mode="topic.ol.atts.in"/>
         <xsl:apply-templates select="." mode="topic.ol.content.in"/>
      </ol>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/othermeta '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.out">
      <othermeta>
         <xsl:apply-templates select="." mode="topic.othermeta.atts.in"/>
      </othermeta>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/p '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.out">
      <p>
         <xsl:apply-templates select="." mode="topic.p.atts.in"/>
         <xsl:apply-templates select="." mode="topic.p.content.in"/>
      </p>
   </xsl:template>

   <xsl:template match="*" mode="topic.param.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/param '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.param.out">
      <param>
         <xsl:apply-templates select="." mode="topic.param.atts.in"/>
      </param>
   </xsl:template>

   <xsl:template match="*" mode="topic.permissions.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/permissions '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.permissions.out">
      <permissions>
         <xsl:apply-templates select="." mode="topic.permissions.atts.in"/>
      </permissions>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/ph '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.out">
      <ph>
         <xsl:apply-templates select="." mode="topic.ph.atts.in"/>
         <xsl:apply-templates select="." mode="topic.ph.content.in"/>
      </ph>
   </xsl:template>

   <xsl:template match="*" mode="topic.platform.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/platform '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.platform.out">
      <platform>
         <xsl:apply-templates select="." mode="topic.platform.atts.in"/>
         <xsl:apply-templates select="." mode="topic.platform.content.in"/>
      </platform>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/pre '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.out">
      <pre>
         <xsl:apply-templates select="." mode="topic.pre.atts.in"/>
         <xsl:apply-templates select="." mode="topic.pre.content.in"/>
      </pre>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/prodinfo '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.out">
      <prodinfo>
         <xsl:apply-templates select="." mode="topic.prodinfo.atts.in"/>
         <xsl:apply-templates select="." mode="topic.prodinfo.content.in"/>
      </prodinfo>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodname.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/prodname '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodname.out">
      <prodname>
         <xsl:apply-templates select="." mode="topic.prodname.atts.in"/>
         <xsl:apply-templates select="." mode="topic.prodname.content.in"/>
      </prodname>
   </xsl:template>

   <xsl:template match="*" mode="topic.prognum.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/prognum '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.prognum.out">
      <prognum>
         <xsl:apply-templates select="." mode="topic.prognum.atts.in"/>
         <xsl:apply-templates select="." mode="topic.prognum.content.in"/>
      </prognum>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/prolog '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.out">
      <xsl:if test="$include-prolog">
      <prolog>
         <xsl:apply-templates select="." mode="topic.prolog.atts.in"/>
         <xsl:apply-templates select="." mode="topic.prolog.content.in"/>
      </prolog>
      </xsl:if>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/publisher '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.out">
      <publisher>
         <xsl:apply-templates select="." mode="topic.publisher.atts.in"/>
         <xsl:apply-templates select="." mode="topic.publisher.content.in"/>
      </publisher>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/q '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.out">
      <q>
         <xsl:apply-templates select="." mode="topic.q.atts.in"/>
         <xsl:apply-templates select="." mode="topic.q.content.in"/>
      </q>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/related-links '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.out">
      <related-links>
         <xsl:apply-templates select="." mode="topic.related-links.atts.in"/>
         <xsl:apply-templates select="." mode="topic.related-links.content.in"/>
      </related-links>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/required-cleanup '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.out">
      <required-cleanup>
         <xsl:apply-templates select="." mode="topic.required-cleanup.atts.in"/>
         <xsl:apply-templates select="." mode="topic.required-cleanup.content.in"/>
      </required-cleanup>
   </xsl:template>

   <xsl:template match="*" mode="topic.resourceid.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/resourceid '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.resourceid.out">
      <resourceid>
         <xsl:apply-templates select="." mode="topic.resourceid.atts.in"/>
      </resourceid>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/revised '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.out">
      <revised>
         <xsl:apply-templates select="." mode="topic.revised.atts.in"/>
      </revised>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/row '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.out">
      <row>
         <xsl:apply-templates select="." mode="topic.row.atts.in"/>
         <xsl:apply-templates select="." mode="topic.row.content.in"/>
      </row>
   </xsl:template>

   <xsl:template match="*" mode="topic.searchtitle.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/searchtitle '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.searchtitle.out">
      <searchtitle>
         <xsl:apply-templates select="." mode="topic.searchtitle.atts.in"/>
         <xsl:apply-templates select="." mode="topic.searchtitle.content.in"/>
      </searchtitle>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/section '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.out">
      <section>
         <xsl:apply-templates select="." mode="topic.section.atts.in"/>
         <xsl:apply-templates select="." mode="topic.title.in"/>
         <xsl:apply-templates select="*[not(self::title)]" mode="topic.section.content.in"/>
      </section>
   </xsl:template>

   <xsl:template match="*" mode="topic.series.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/series '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.series.out">
      <series>
         <xsl:apply-templates select="." mode="topic.series.atts.in"/>
         <xsl:apply-templates select="." mode="topic.series.content.in"/>
      </series>
   </xsl:template>

   <xsl:template match="*" mode="topic.shortdesc.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/shortdesc '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.shortdesc.out">
      <shortdesc>
         <xsl:apply-templates select="." mode="topic.shortdesc.atts.in"/>
         <xsl:apply-templates select="." mode="topic.shortdesc.content.in"/>
      </shortdesc>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/simpletable '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.out">
      <simpletable>
         <xsl:apply-templates select="." mode="topic.simpletable.atts.in"/>
         <xsl:apply-templates select="." mode="topic.simpletable.content.in"/>
      </simpletable>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/sl '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.out">
      <sl>
         <xsl:apply-templates select="." mode="topic.sl.atts.in"/>
         <xsl:apply-templates select="." mode="topic.sl.content.in"/>
      </sl>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/sli '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.out">
      <sli>
         <xsl:apply-templates select="." mode="topic.sli.atts.in"/>
         <xsl:apply-templates select="." mode="topic.sli.content.in"/>
      </sli>
   </xsl:template>

   <xsl:template match="*" mode="topic.source.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/source '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.source.out">
      <source>
         <xsl:apply-templates select="." mode="topic.source.atts.in"/>
         <xsl:apply-templates select="." mode="topic.source.content.in"/>
      </source>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/state '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.out">
      <state>
         <xsl:apply-templates select="." mode="topic.state.atts.in"/>
      </state>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/stentry '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.out">
      <stentry>
         <xsl:apply-templates select="." mode="topic.stentry.atts.in"/>
         <xsl:apply-templates select="." mode="topic.stentry.content.in"/>
      </stentry>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/sthead '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.out">
      <sthead>
         <xsl:apply-templates select="." mode="topic.sthead.atts.in"/>
         <xsl:apply-templates select="." mode="topic.sthead.content.in"/>
      </sthead>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/strow '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.out">
      <strow>
         <xsl:apply-templates select="." mode="topic.strow.atts.in"/>
         <xsl:apply-templates select="." mode="topic.strow.content.in"/>
      </strow>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/table '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.out">
      <table>
         <xsl:apply-templates select="." mode="topic.table.atts.in"/>
         <xsl:apply-templates select="." mode="topic.table.content.in"/>
      </table>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/tbody '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.out">
      <tbody>
         <xsl:apply-templates select="." mode="topic.tbody.atts.in"/>
         <xsl:apply-templates select="." mode="topic.tbody.content.in"/>
      </tbody>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/term '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.out">
      <term>
         <xsl:apply-templates select="." mode="topic.term.atts.in"/>
         <xsl:apply-templates select="." mode="topic.term.content.in"/>
      </term>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/tgroup '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.out">
      <tgroup>
         <xsl:apply-templates select="." mode="topic.tgroup.atts.in"/>
         <xsl:apply-templates select="." mode="topic.tgroup.content.in"/>
      </tgroup>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/thead '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.out">
      <thead>
         <xsl:apply-templates select="." mode="topic.thead.atts.in"/>
         <xsl:apply-templates select="." mode="topic.thead.content.in"/>
      </thead>
   </xsl:template>

   <xsl:template match="*" mode="topic.title.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/title '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.title.out">
      <title>
         <xsl:apply-templates select="." mode="topic.title.atts.in"/>
         <xsl:apply-templates select="." mode="topic.title.content.in"/>
      </title>
   </xsl:template>

   <xsl:template match="*" mode="topic.titlealts.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/titlealts '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.titlealts.out">
      <titlealts>
         <xsl:apply-templates select="." mode="topic.titlealts.atts.in"/>
         <xsl:apply-templates select="." mode="topic.titlealts.content.in"/>
      </titlealts>
   </xsl:template>

   <xsl:template match="*" mode="topic.tm.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/tm '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.tm.out">
      <tm>
         <xsl:apply-templates select="." mode="topic.tm.atts.in"/>
         <xsl:apply-templates select="." mode="topic.tm.content.in"/>
      </tm>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/topic '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.out">
      <topic>
         <xsl:apply-templates select="." mode="topic.topic.atts.in"/>
         <xsl:apply-templates select="." mode="topic.topic.content.in"/>
      </topic>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/ul '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.out">
      <ul>
         <xsl:apply-templates select="." mode="topic.ul.atts.in"/>
         <xsl:apply-templates select="." mode="topic.ul.content.in"/>
      </ul>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrm.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/vrm '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrm.out">
      <vrm>
         <xsl:apply-templates select="." mode="topic.vrm.atts.in"/>
      </vrm>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrmlist.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/vrmlist '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrmlist.out">
      <vrmlist>
         <xsl:apply-templates select="." mode="topic.vrmlist.atts.in"/>
         <xsl:apply-templates select="." mode="topic.vrmlist.content.in"/>
      </vrmlist>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' topic/xref '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.out">
      <xref>
         <xsl:apply-templates select="." mode="topic.xref.atts.in"/>
         <xsl:apply-templates select="." mode="topic.xref.content.in"/>
      </xref>
   </xsl:template>

<!--= = = ATTRIBUTE OUTPUT RULES  = = = = = = = = =-->

   <xsl:template match="*" mode="align.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="align">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="alt.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="alt">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="appname.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="appname">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="archive.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="archive">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="audience.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="audience">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="author.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="author">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="callout.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="callout">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="char.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="char">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="charoff.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="charoff">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="classid.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="classid">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="codebase.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="codebase">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="codetype.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="codetype">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="collection-type.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="collection-type">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="colname.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="colname">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="colnum.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="colnum">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="cols.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="cols">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="colsep.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="colsep">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="colwidth.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="colwidth">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="compact.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="compact">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="conref.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="conref">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="content.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="content">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="data.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="data">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="date.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="date">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="declare.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="declare">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="disposition.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="disposition">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="duplicates.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="duplicates">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="expanse.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="expanse">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="experiencelevel.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="experiencelevel">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="expiry.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="expiry">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="format.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="format">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="frame.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="frame">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="golive.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="golive">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="height.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="height">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="href.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="href">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="id.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="id">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="importance.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="importance">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="job.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="job">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="keycol.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="keycol">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="keyref.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="keyref">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="longdescref.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="longdescref">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="mapkeyref.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="mapkeyref">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="modification.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="modification">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="modified.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="modified">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="morerows.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="morerows">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="name.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="name">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="nameend.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="nameend">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="namest.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="namest">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="otherjob.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="otherjob">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="otherprops.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="otherprops">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="otherrole.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="otherrole">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>
<xsl:template match="*" mode="othertype.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="othertype">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
</xsl:template>

<xsl:template match="*" mode="outputclass.att.out">
      <xsl:param name="value"/>
   <xsl:if test="$include-outputclass">
      <xsl:attribute name="outputclass">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:if>
</xsl:template>

   <xsl:template match="*" mode="pgwide.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="pgwide">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="placement.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="placement">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="platform.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="platform">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="product.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="product">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="query.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="query">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="refcols.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="refcols">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="reftitle.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="reftitle">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="relcolwidth.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="relcolwidth">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="release.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="release">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="remap.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="remap">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="rev.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="rev">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="role.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="role">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="rowheader.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="rowheader">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="rowsep.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="rowsep">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="scale.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="scale">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="scope.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="scope">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="spanname.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="spanname">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="specentry.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="specentry">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="spectitle.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="spectitle">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="standby.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="standby">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="state.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="state">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="status.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="status">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="tabindex.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="tabindex">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="time.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="time">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="tmclass.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="tmclass">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="tmowner.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="tmowner">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="tmtype.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="tmtype">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="trademark.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="trademark">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="translate.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="translate">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="translate-content.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="translate-content">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="type.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="type">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="usemap.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="usemap">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="valign.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="valign">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="value.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="value">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="valuetype.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="valuetype">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="version.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="version">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="view.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="view">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="width.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="width">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>

   <xsl:template match="*" mode="year.att.out">
      <xsl:param name="value"/>
      <xsl:attribute name="year">
         <xsl:value-of select="$value"/>
      </xsl:attribute>
   </xsl:template>


<!--= = = DEFAULT ELEMENT INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="topic.alt.atts.in">
      <xsl:apply-templates select="." mode="topic.alt.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.alt.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.alt.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.alt.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.id.atts.in">
      <xsl:apply-templates select="." mode="topic.alt.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.alt.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.select.atts.in">
      <xsl:apply-templates select="." mode="topic.alt.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.alt.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.alt.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.alt.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.alt.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.alt.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.alt.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.alt.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.atts.in">
      <xsl:apply-templates select="." mode="topic.audience.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.audience.type.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.othertype.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.job.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.otherjob.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.experiencelevel.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.name.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.select.atts.in">
      <xsl:apply-templates select="." mode="topic.audience.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.audience.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.othertype.att.in">
      <xsl:apply-templates select="." mode="othertype.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.job.att.in">
      <xsl:apply-templates select="." mode="job.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.otherjob.att.in">
      <xsl:apply-templates select="." mode="otherjob.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.experiencelevel.att.in">
      <xsl:apply-templates select="." mode="experiencelevel.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.audience.name.att.in">
      <xsl:apply-templates select="." mode="name.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.author.atts.in">
      <xsl:apply-templates select="." mode="topic.author.href.att.in"/>
      <xsl:apply-templates select="." mode="topic.author.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.author.type.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.author.href.att.in">
      <xsl:apply-templates select="." mode="href.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.author.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.author.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.author.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.body.atts.in">
      <xsl:apply-templates select="." mode="topic.body.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.body.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.body.id.atts.in">
      <xsl:apply-templates select="." mode="topic.body.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.body.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.body.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.body.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.body.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.body.content.in">
      <xsl:apply-templates select="*" mode="topic.body.child"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.body.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/body '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.atts.in">
      <xsl:apply-templates select="." mode="topic.boolean.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.state.att.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.boolean.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.id.atts.in">
      <xsl:apply-templates select="." mode="topic.boolean.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.select.atts.in">
      <xsl:apply-templates select="." mode="topic.boolean.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.boolean.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.state.att.in">
      <xsl:apply-templates select="." mode="state.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.boolean.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.brand.atts.in"/>

   <xsl:template match="*" mode="topic.brand.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.atts.in">
      <xsl:apply-templates select="." mode="topic.category.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.select.atts.in">
      <xsl:apply-templates select="." mode="topic.category.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.category.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.category.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.category.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.category.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.category.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.category.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.category.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.atts.in">
      <xsl:apply-templates select="." mode="topic.cite.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.cite.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.cite.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.cite.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.cite.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.id.atts.in">
      <xsl:apply-templates select="." mode="topic.cite.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.cite.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.select.atts.in">
      <xsl:apply-templates select="." mode="topic.cite.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.cite.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.cite.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.cite.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.cite.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.cite.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.cite.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.cite.content.in">
      <xsl:apply-templates select="." mode="xrefph.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.atts.in">
      <xsl:apply-templates select="." mode="topic.colspec.colrowsep.atts.in"/>
      <xsl:apply-templates select="." mode="topic.colspec.colnum.att.in"/>
      <xsl:apply-templates select="." mode="topic.colspec.colname.att.in"/>
      <xsl:apply-templates select="." mode="topic.colspec.align.att.in"/>
      <xsl:apply-templates select="." mode="topic.colspec.colwidth.att.in"/>
      <xsl:apply-templates select="." mode="topic.colspec.char.att.in"/>
      <xsl:apply-templates select="." mode="topic.colspec.charoff.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.colrowsep.atts.in">
      <xsl:apply-templates select="." mode="topic.colspec.colsep.att.in"/>
      <xsl:apply-templates select="." mode="topic.colspec.rowsep.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.colsep.att.in">
      <xsl:apply-templates select="." mode="colsep.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.rowsep.att.in">
      <xsl:apply-templates select="." mode="rowsep.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.colnum.att.in">
      <xsl:apply-templates select="." mode="colnum.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.colname.att.in">
      <xsl:apply-templates select="." mode="colname.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.align.att.in">
      <xsl:apply-templates select="." mode="align.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.colwidth.att.in">
      <xsl:apply-templates select="." mode="colwidth.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.char.att.in">
      <xsl:apply-templates select="." mode="char.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.colspec.charoff.att.in">
      <xsl:apply-templates select="." mode="charoff.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.component.atts.in"/>

   <xsl:template match="*" mode="topic.component.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyrholder.atts.in"/>

   <xsl:template match="*" mode="topic.copyrholder.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyright.atts.in">
      <xsl:apply-templates select="." mode="topic.copyright.type.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyright.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyright.content.in">
      <xsl:apply-templates select="." mode="topic.copyright.topic.copyryear.in"/>
      <xsl:apply-templates select="." mode="topic.copyright.topic.copyrholder.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyright.topic.copyryear.in">
      <xsl:apply-templates select="." mode="topic.copyryear.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/copyright '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyright.topic.copyrholder.in">
      <xsl:apply-templates select="." mode="topic.copyrholder.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/copyright '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.atts.in">
      <xsl:apply-templates select="." mode="topic.copyryear.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.copyryear.year.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.select.atts.in">
      <xsl:apply-templates select="." mode="topic.copyryear.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.copyryear.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.copyryear.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.copyryear.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.copyryear.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.copyryear.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.copyryear.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.copyryear.year.att.in">
      <xsl:apply-templates select="." mode="year.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.created.atts.in">
      <xsl:apply-templates select="." mode="topic.created.date.att.in"/>
      <xsl:apply-templates select="." mode="topic.created.golive.att.in"/>
      <xsl:apply-templates select="." mode="topic.created.expiry.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.created.date.att.in">
      <xsl:apply-templates select="." mode="date.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.created.golive.att.in">
      <xsl:apply-templates select="." mode="golive.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.created.expiry.att.in">
      <xsl:apply-templates select="." mode="expiry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.critdates.atts.in"/>

   <xsl:template match="*" mode="topic.critdates.content.in">
      <xsl:apply-templates select="." mode="topic.critdates.topic.created.in"/>
      <xsl:apply-templates select="." mode="topic.critdates.topic.revised.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.critdates.topic.created.in">
      <xsl:apply-templates select="." mode="topic.created.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/critdates '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.critdates.topic.revised.in">
      <xsl:apply-templates select="." mode="topic.revised.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/critdates '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.atts.in">
      <xsl:apply-templates select="." mode="topic.dd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dd.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.dd.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.id.atts.in">
      <xsl:apply-templates select="." mode="topic.dd.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.dd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.select.atts.in">
      <xsl:apply-templates select="." mode="topic.dd.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.dd.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.dd.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.dd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.dd.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.dd.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.dd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dd.content.in">
      <xsl:apply-templates select="." mode="defn.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.atts.in">
      <xsl:apply-templates select="." mode="topic.ddhd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.ddhd.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.ddhd.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.ddhd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.id.atts.in">
      <xsl:apply-templates select="." mode="topic.ddhd.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.ddhd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.select.atts.in">
      <xsl:apply-templates select="." mode="topic.ddhd.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.ddhd.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.ddhd.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.ddhd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.ddhd.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.ddhd.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.ddhd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ddhd.content.in">
      <xsl:apply-templates select="." mode="title.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.desc.atts.in">
      <xsl:apply-templates select="." mode="topic.desc.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.desc.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.desc.id.atts.in">
      <xsl:apply-templates select="." mode="topic.desc.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.desc.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.desc.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.desc.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.desc.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.desc.content.in">
      <xsl:apply-templates select="." mode="desc.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.atts.in">
      <xsl:apply-templates select="." mode="topic.dl.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dl.compact.att.in"/>
      <xsl:apply-templates select="." mode="topic.dl.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.dl.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.dl.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dl.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.id.atts.in">
      <xsl:apply-templates select="." mode="topic.dl.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.dl.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.select.atts.in">
      <xsl:apply-templates select="." mode="topic.dl.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.dl.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.dl.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.dl.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.dl.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.dl.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.dl.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.compact.att.in">
      <xsl:apply-templates select="." mode="compact.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.content.in">
      <xsl:apply-templates select="." mode="topic.dl.topic.dlhead.in"/>
      <xsl:apply-templates select="." mode="topic.dl.topic.dlentry.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.topic.dlhead.in">
      <xsl:apply-templates select="." mode="topic.dlhead.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/dl '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dl.topic.dlentry.in">
      <xsl:apply-templates select="." mode="topic.dlentry.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/dl '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.atts.in">
      <xsl:apply-templates select="." mode="topic.dlentry.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.dlentry.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.id.atts.in">
      <xsl:apply-templates select="." mode="topic.dlentry.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.select.atts.in">
      <xsl:apply-templates select="." mode="topic.dlentry.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.content.in">
      <xsl:apply-templates select="." mode="topic.dlentry.topic.dt.in"/>
      <xsl:apply-templates select="." mode="topic.dlentry.topic.dd.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.topic.dt.in">
      <xsl:apply-templates select="." mode="topic.dt.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/dlentry '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlentry.topic.dd.in">
      <xsl:apply-templates select="." mode="topic.dd.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/dlentry '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.atts.in">
      <xsl:apply-templates select="." mode="topic.dlhead.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.dlhead.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.id.atts.in">
      <xsl:apply-templates select="." mode="topic.dlhead.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.select.atts.in">
      <xsl:apply-templates select="." mode="topic.dlhead.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.content.in">
      <xsl:apply-templates select="." mode="topic.dlhead.topic.dthd.in"/>
      <xsl:apply-templates select="." mode="topic.dlhead.topic.ddhd.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.topic.dthd.in">
      <xsl:apply-templates select="." mode="topic.dthd.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/dlhead '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dlhead.topic.ddhd.in">
      <xsl:apply-templates select="." mode="topic.ddhd.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/dlhead '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.atts.in">
      <xsl:apply-templates select="." mode="topic.draft-comment.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.disposition.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.author.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.time.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.draft-comment.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.id.atts.in">
      <xsl:apply-templates select="." mode="topic.draft-comment.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.select.atts.in">
      <xsl:apply-templates select="." mode="topic.draft-comment.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.draft-comment.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.disposition.att.in">
      <xsl:apply-templates select="." mode="disposition.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.author.att.in">
      <xsl:apply-templates select="." mode="author.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.time.att.in">
      <xsl:apply-templates select="." mode="time.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.draft-comment.content.in">
      <xsl:apply-templates select="*|text()" mode="topic.draft-comment.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="topic.draft-comment.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/draft-comment '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.atts.in">
      <xsl:apply-templates select="." mode="topic.dt.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dt.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.dt.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.dt.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dt.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.id.atts.in">
      <xsl:apply-templates select="." mode="topic.dt.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.dt.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.select.atts.in">
      <xsl:apply-templates select="." mode="topic.dt.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.dt.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.dt.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.dt.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.dt.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.dt.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.dt.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dt.content.in">
      <xsl:apply-templates select="." mode="term.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.atts.in">
      <xsl:apply-templates select="." mode="topic.dthd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dthd.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.dthd.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.dthd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.id.atts.in">
      <xsl:apply-templates select="." mode="topic.dthd.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.dthd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.select.atts.in">
      <xsl:apply-templates select="." mode="topic.dthd.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.dthd.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.dthd.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.dthd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.dthd.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.dthd.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.dthd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.dthd.content.in">
      <xsl:apply-templates select="." mode="title.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.atts.in">
      <xsl:apply-templates select="." mode="topic.entry.colrowsep.atts.in"/>
      <xsl:apply-templates select="." mode="topic.entry.namest.att.in"/>
      <xsl:apply-templates select="." mode="topic.entry.nameend.att.in"/>
      <xsl:apply-templates select="." mode="topic.entry.spanname.att.in"/>
      <xsl:apply-templates select="." mode="topic.entry.colname.att.in"/>
      <xsl:apply-templates select="." mode="topic.entry.morerows.att.in"/>
      <xsl:apply-templates select="." mode="topic.entry.align.att.in"/>
      <xsl:apply-templates select="." mode="topic.entry.valign.att.in"/>
      <xsl:apply-templates select="." mode="topic.entry.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.entry.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.colrowsep.atts.in">
      <xsl:apply-templates select="." mode="topic.entry.colsep.att.in"/>
      <xsl:apply-templates select="." mode="topic.entry.rowsep.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.colsep.att.in">
      <xsl:apply-templates select="." mode="colsep.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.rowsep.att.in">
      <xsl:apply-templates select="." mode="rowsep.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.namest.att.in">
      <xsl:apply-templates select="." mode="namest.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.nameend.att.in">
      <xsl:apply-templates select="." mode="nameend.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.spanname.att.in">
      <xsl:apply-templates select="." mode="spanname.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.colname.att.in">
      <xsl:apply-templates select="." mode="colname.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.morerows.att.in">
      <xsl:apply-templates select="." mode="morerows.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.align.att.in">
      <xsl:apply-templates select="." mode="align.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.valign.att.in">
      <xsl:apply-templates select="." mode="valign.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.entry.content.in">
      <xsl:apply-templates select="." mode="tblcell.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.atts.in">
      <xsl:apply-templates select="." mode="topic.example.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.example.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.example.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.example.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.example.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.id.atts.in">
      <xsl:apply-templates select="." mode="topic.example.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.example.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.select.atts.in">
      <xsl:apply-templates select="." mode="topic.example.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.example.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.example.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.example.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.example.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.example.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.example.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.example.content.in">
      <xsl:apply-templates select="." mode="section.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.featnum.atts.in"/>

   <xsl:template match="*" mode="topic.featnum.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.atts.in">
      <xsl:apply-templates select="." mode="topic.fig.display.atts.in"/>
      <xsl:apply-templates select="." mode="topic.fig.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.fig.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.spectitle.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.display.atts.in">
      <xsl:apply-templates select="." mode="topic.fig.scale.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.frame.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.fig.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.fig.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.id.atts.in">
      <xsl:apply-templates select="." mode="topic.fig.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.select.atts.in">
      <xsl:apply-templates select="." mode="topic.fig.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.fig.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.content.in">
      <xsl:apply-templates select="*" mode="topic.fig.child"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fig.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/fig '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.atts.in">
      <xsl:apply-templates select="." mode="topic.figgroup.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.figgroup.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.figgroup.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.figgroup.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.id.atts.in">
      <xsl:apply-templates select="." mode="topic.figgroup.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.figgroup.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.select.atts.in">
      <xsl:apply-templates select="." mode="topic.figgroup.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.figgroup.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.figgroup.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.figgroup.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.figgroup.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.figgroup.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.figgroup.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.content.in">
      <xsl:apply-templates select="*" mode="topic.figgroup.child"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.figgroup.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/figgroup '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.atts.in">
      <xsl:apply-templates select="." mode="topic.fn.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.fn.callout.att.in"/>
      <xsl:apply-templates select="." mode="topic.fn.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.fn.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.fn.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.id.atts.in">
      <xsl:apply-templates select="." mode="topic.fn.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.fn.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.select.atts.in">
      <xsl:apply-templates select="." mode="topic.fn.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.fn.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.fn.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.fn.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.fn.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.fn.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.fn.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.callout.att.in">
      <xsl:apply-templates select="." mode="callout.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.fn.content.in">
      <xsl:apply-templates select="." mode="fn.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.atts.in">
      <xsl:apply-templates select="." mode="topic.image.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.image.href.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.alt.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.longdescref.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.height.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.width.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.align.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.placement.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.image.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.image.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.id.atts.in">
      <xsl:apply-templates select="." mode="topic.image.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.select.atts.in">
      <xsl:apply-templates select="." mode="topic.image.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.image.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.href.att.in">
      <xsl:apply-templates select="." mode="href.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.alt.att.in">
      <xsl:apply-templates select="." mode="alt.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.longdescref.att.in">
      <xsl:apply-templates select="." mode="longdescref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.height.att.in">
      <xsl:apply-templates select="." mode="height.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.width.att.in">
      <xsl:apply-templates select="." mode="width.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.align.att.in">
      <xsl:apply-templates select="." mode="align.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.placement.att.in">
      <xsl:apply-templates select="." mode="placement.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.content.in">
      <xsl:apply-templates select="." mode="topic.image.topic.alt.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.image.topic.alt.in">
      <xsl:apply-templates select="." mode="topic.alt.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/image '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.atts.in">
      <xsl:apply-templates select="." mode="topic.indexterm.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.indexterm.keyref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.indexterm.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.indexterm.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.id.atts.in">
      <xsl:apply-templates select="." mode="topic.indexterm.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.indexterm.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.select.atts.in">
      <xsl:apply-templates select="." mode="topic.indexterm.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.indexterm.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.indexterm.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.indexterm.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.indexterm.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.indexterm.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.indexterm.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indexterm.content.in">
      <xsl:apply-templates select="*|text()" mode="topic.indexterm.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="topic.indexterm.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/indexterm '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.atts.in">
      <xsl:apply-templates select="." mode="topic.indextermref.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.indextermref.keyref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.indextermref.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.indextermref.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.id.atts.in">
      <xsl:apply-templates select="." mode="topic.indextermref.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.indextermref.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.select.atts.in">
      <xsl:apply-templates select="." mode="topic.indextermref.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.indextermref.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.indextermref.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.indextermref.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.indextermref.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.indextermref.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.indextermref.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.indextermref.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.atts.in">
      <xsl:apply-templates select="." mode="topic.itemgroup.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.itemgroup.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.itemgroup.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.itemgroup.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.id.atts.in">
      <xsl:apply-templates select="." mode="topic.itemgroup.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.itemgroup.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.select.atts.in">
      <xsl:apply-templates select="." mode="topic.itemgroup.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.itemgroup.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.itemgroup.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.itemgroup.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.itemgroup.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.itemgroup.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.itemgroup.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.itemgroup.content.in">
      <xsl:apply-templates select="." mode="itemgroup.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.atts.in">
      <xsl:apply-templates select="." mode="topic.keyword.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.keyword.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.id.atts.in">
      <xsl:apply-templates select="." mode="topic.keyword.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.select.atts.in">
      <xsl:apply-templates select="." mode="topic.keyword.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.keyword.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keyword.content.in">
      <xsl:apply-templates select="*|text()" mode="topic.keyword.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="topic.keyword.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/keyword '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.atts.in">
      <xsl:apply-templates select="." mode="topic.keywords.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.keywords.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.id.atts.in">
      <xsl:apply-templates select="." mode="topic.keywords.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.keywords.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.select.atts.in">
      <xsl:apply-templates select="." mode="topic.keywords.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.keywords.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.keywords.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.keywords.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.keywords.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.keywords.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.keywords.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.content.in">
      <xsl:apply-templates select="*" mode="topic.keywords.child"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.keywords.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/keywords '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.atts.in">
      <xsl:apply-templates select="." mode="topic.li.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.li.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.li.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.li.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.id.atts.in">
      <xsl:apply-templates select="." mode="topic.li.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.li.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.select.atts.in">
      <xsl:apply-templates select="." mode="topic.li.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.li.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.li.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.li.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.li.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.li.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.li.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.li.content.in">
      <xsl:apply-templates select="." mode="listitem.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.atts.in">
      <xsl:apply-templates select="." mode="topic.lines.display.atts.in"/>
      <xsl:apply-templates select="." mode="topic.lines.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.lines.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.display.atts.in">
      <xsl:apply-templates select="." mode="topic.lines.scale.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.frame.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.lines.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.lines.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.id.atts.in">
      <xsl:apply-templates select="." mode="topic.lines.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.select.atts.in">
      <xsl:apply-templates select="." mode="topic.lines.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.lines.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lines.content.in">
      <xsl:apply-templates select="." mode="pre.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.atts.in">
      <xsl:apply-templates select="." mode="topic.link.rel.atts.in"/>
      <xsl:apply-templates select="." mode="topic.link.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.link.href.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.format.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.query.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.scope.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.rel.atts.in">
      <xsl:apply-templates select="." mode="topic.link.type.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.role.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.otherrole.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.role.att.in">
      <xsl:apply-templates select="." mode="role.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.otherrole.att.in">
      <xsl:apply-templates select="." mode="otherrole.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.select.atts.in">
      <xsl:apply-templates select="." mode="topic.link.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.link.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.href.att.in">
      <xsl:apply-templates select="." mode="href.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.format.att.in">
      <xsl:apply-templates select="." mode="format.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.query.att.in">
      <xsl:apply-templates select="." mode="query.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.scope.att.in">
      <xsl:apply-templates select="." mode="scope.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.content.in">
      <xsl:apply-templates select="." mode="topic.link.topic.linktext.in"/>
      <xsl:apply-templates select="." mode="topic.link.topic.desc.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.topic.linktext.in">
      <xsl:apply-templates select="." mode="topic.linktext.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/link '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.link.topic.desc.in">
      <xsl:apply-templates select="." mode="topic.desc.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/link '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkinfo.atts.in"/>

   <xsl:template match="*" mode="topic.linkinfo.content.in">
      <xsl:apply-templates select="." mode="desc.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.atts.in">
      <xsl:apply-templates select="." mode="topic.linklist.rel.atts.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.collection-type.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.duplicates.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.mapkeyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.format.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.scope.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.rel.atts.in">
      <xsl:apply-templates select="." mode="topic.linklist.type.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.role.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.otherrole.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.role.att.in">
      <xsl:apply-templates select="." mode="role.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.otherrole.att.in">
      <xsl:apply-templates select="." mode="otherrole.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.select.atts.in">
      <xsl:apply-templates select="." mode="topic.linklist.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.linklist.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.collection-type.att.in">
      <xsl:apply-templates select="." mode="collection-type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.duplicates.att.in">
      <xsl:apply-templates select="." mode="duplicates.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.mapkeyref.att.in">
      <xsl:apply-templates select="." mode="mapkeyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.format.att.in">
      <xsl:apply-templates select="." mode="format.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.scope.att.in">
      <xsl:apply-templates select="." mode="scope.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.content.in">
      <xsl:apply-templates select="*" mode="topic.linklist.child"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.linklist.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/linklist '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.atts.in">
      <xsl:apply-templates select="." mode="topic.linkpool.rel.atts.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.collection-type.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.duplicates.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.mapkeyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.format.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.scope.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.rel.atts.in">
      <xsl:apply-templates select="." mode="topic.linkpool.type.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.role.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.otherrole.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.role.att.in">
      <xsl:apply-templates select="." mode="role.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.otherrole.att.in">
      <xsl:apply-templates select="." mode="otherrole.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.select.atts.in">
      <xsl:apply-templates select="." mode="topic.linkpool.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.linkpool.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.collection-type.att.in">
      <xsl:apply-templates select="." mode="collection-type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.duplicates.att.in">
      <xsl:apply-templates select="." mode="duplicates.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.mapkeyref.att.in">
      <xsl:apply-templates select="." mode="mapkeyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.format.att.in">
      <xsl:apply-templates select="." mode="format.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.scope.att.in">
      <xsl:apply-templates select="." mode="scope.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.content.in">
      <xsl:apply-templates select="*" mode="topic.linkpool.child"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.linkpool.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/linkpool '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.linktext.atts.in"/>

   <xsl:template match="*" mode="topic.linktext.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.atts.in">
      <xsl:apply-templates select="." mode="topic.lq.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.lq.href.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.type.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.reftitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.lq.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.lq.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.id.atts.in">
      <xsl:apply-templates select="." mode="topic.lq.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.select.atts.in">
      <xsl:apply-templates select="." mode="topic.lq.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.lq.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.href.att.in">
      <xsl:apply-templates select="." mode="href.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.reftitle.att.in">
      <xsl:apply-templates select="." mode="reftitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.lq.content.in">
      <xsl:apply-templates select="." mode="longquote.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.atts.in">
      <xsl:apply-templates select="." mode="topic.metadata.mapkeyref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.mapkeyref.att.in">
      <xsl:apply-templates select="." mode="mapkeyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.content.in">
      <xsl:apply-templates select="." mode="topic.metadata.topic.audience.in"/>
      <xsl:apply-templates select="." mode="topic.metadata.topic.category.in"/>
      <xsl:apply-templates select="." mode="topic.metadata.topic.keywords.in"/>
      <xsl:apply-templates select="." mode="topic.metadata.topic.prodinfo.in"/>
      <xsl:apply-templates select="." mode="topic.metadata.topic.othermeta.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.topic.audience.in">
      <xsl:apply-templates select="." mode="topic.audience.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/metadata '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.topic.category.in">
      <xsl:apply-templates select="." mode="topic.category.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/metadata '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.topic.keywords.in">
      <xsl:apply-templates select="." mode="topic.keywords.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/metadata '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.topic.prodinfo.in">
      <xsl:apply-templates select="." mode="topic.prodinfo.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/metadata '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.metadata.topic.othermeta.in">
      <xsl:apply-templates select="." mode="topic.othermeta.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/metadata '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.navtitle.atts.in">
      <xsl:apply-templates select="." mode="topic.navtitle.id.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.navtitle.id.atts.in">
      <xsl:apply-templates select="." mode="topic.navtitle.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.navtitle.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.navtitle.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.navtitle.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.navtitle.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.no-topic-nesting.atts.in"/>

   <xsl:template match="*" mode="topic.note.atts.in">
      <xsl:apply-templates select="." mode="topic.note.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.note.type.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.othertype.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.note.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.note.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.id.atts.in">
      <xsl:apply-templates select="." mode="topic.note.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.select.atts.in">
      <xsl:apply-templates select="." mode="topic.note.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.note.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.othertype.att.in">
      <xsl:apply-templates select="." mode="othertype.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.note.content.in">
      <xsl:apply-templates select="." mode="note.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.atts.in">
      <xsl:apply-templates select="." mode="topic.object.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.object.declare.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.classid.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.codebase.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.data.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.type.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.codetype.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.archive.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.standby.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.height.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.width.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.usemap.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.name.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.tabindex.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.longdescref.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.object.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.object.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.id.atts.in">
      <xsl:apply-templates select="." mode="topic.object.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.select.atts.in">
      <xsl:apply-templates select="." mode="topic.object.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.object.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.declare.att.in">
      <xsl:apply-templates select="." mode="declare.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.classid.att.in">
      <xsl:apply-templates select="." mode="classid.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.codebase.att.in">
      <xsl:apply-templates select="." mode="codebase.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.data.att.in">
      <xsl:apply-templates select="." mode="data.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.codetype.att.in">
      <xsl:apply-templates select="." mode="codetype.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.archive.att.in">
      <xsl:apply-templates select="." mode="archive.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.standby.att.in">
      <xsl:apply-templates select="." mode="standby.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.height.att.in">
      <xsl:apply-templates select="." mode="height.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.width.att.in">
      <xsl:apply-templates select="." mode="width.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.usemap.att.in">
      <xsl:apply-templates select="." mode="usemap.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.name.att.in">
      <xsl:apply-templates select="." mode="name.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.tabindex.att.in">
      <xsl:apply-templates select="." mode="tabindex.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.longdescref.att.in">
      <xsl:apply-templates select="." mode="longdescref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.content.in">
      <xsl:apply-templates select="." mode="topic.object.topic.desc.in"/>
      <xsl:apply-templates select="." mode="topic.object.topic.param.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.topic.desc.in">
      <xsl:apply-templates select="." mode="topic.desc.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/object '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.object.topic.param.in">
      <xsl:apply-templates select="." mode="topic.param.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/object '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.atts.in">
      <xsl:apply-templates select="." mode="topic.ol.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.ol.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.ol.compact.att.in"/>
      <xsl:apply-templates select="." mode="topic.ol.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.ol.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.ol.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.id.atts.in">
      <xsl:apply-templates select="." mode="topic.ol.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.ol.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.select.atts.in">
      <xsl:apply-templates select="." mode="topic.ol.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.ol.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.ol.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.ol.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.ol.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.ol.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.ol.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.compact.att.in">
      <xsl:apply-templates select="." mode="compact.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.content.in">
      <xsl:apply-templates select="." mode="topic.ol.topic.li.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ol.topic.li.in">
      <xsl:apply-templates select="." mode="topic.li.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/ol '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.atts.in">
      <xsl:apply-templates select="." mode="topic.othermeta.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.othermeta.name.att.in"/>
      <xsl:apply-templates select="." mode="topic.othermeta.content.att.in"/>
      <xsl:apply-templates select="." mode="topic.othermeta.translate-content.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.select.atts.in">
      <xsl:apply-templates select="." mode="topic.othermeta.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.othermeta.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.othermeta.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.othermeta.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.othermeta.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.othermeta.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.othermeta.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.name.att.in">
      <xsl:apply-templates select="." mode="name.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.content.att.in">
      <xsl:apply-templates select="." mode="content.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.othermeta.translate-content.att.in">
      <xsl:apply-templates select="." mode="translate-content.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.atts.in">
      <xsl:apply-templates select="." mode="topic.p.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.p.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.p.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.p.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.id.atts.in">
      <xsl:apply-templates select="." mode="topic.p.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.p.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.select.atts.in">
      <xsl:apply-templates select="." mode="topic.p.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.p.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.p.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.p.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.p.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.p.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.p.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.p.content.in">
      <xsl:apply-templates select="." mode="para.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.param.atts.in">
      <xsl:apply-templates select="." mode="topic.param.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.param.name.att.in"/>
      <xsl:apply-templates select="." mode="topic.param.value.att.in"/>
      <xsl:apply-templates select="." mode="topic.param.valuetype.att.in"/>
      <xsl:apply-templates select="." mode="topic.param.type.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.param.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.param.name.att.in">
      <xsl:apply-templates select="." mode="name.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.param.value.att.in">
      <xsl:apply-templates select="." mode="value.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.param.valuetype.att.in">
      <xsl:apply-templates select="." mode="valuetype.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.param.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.permissions.atts.in">
      <xsl:apply-templates select="." mode="topic.permissions.view.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.permissions.view.att.in">
      <xsl:apply-templates select="." mode="view.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.atts.in">
      <xsl:apply-templates select="." mode="topic.ph.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.ph.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.ph.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.ph.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.ph.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.id.atts.in">
      <xsl:apply-templates select="." mode="topic.ph.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.ph.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.select.atts.in">
      <xsl:apply-templates select="." mode="topic.ph.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.ph.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.ph.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.ph.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.ph.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.ph.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.ph.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ph.content.in">
      <xsl:apply-templates select="." mode="ph.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.platform.atts.in"/>

   <xsl:template match="*" mode="topic.platform.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.atts.in">
      <xsl:apply-templates select="." mode="topic.pre.display.atts.in"/>
      <xsl:apply-templates select="." mode="topic.pre.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.pre.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.spectitle.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.display.atts.in">
      <xsl:apply-templates select="." mode="topic.pre.scale.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.frame.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.pre.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.pre.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.id.atts.in">
      <xsl:apply-templates select="." mode="topic.pre.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.select.atts.in">
      <xsl:apply-templates select="." mode="topic.pre.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.pre.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.pre.content.in">
      <xsl:apply-templates select="." mode="pre.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.atts.in">
      <xsl:apply-templates select="." mode="topic.prodinfo.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.select.atts.in">
      <xsl:apply-templates select="." mode="topic.prodinfo.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.prodinfo.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.prodinfo.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.prodinfo.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.prodinfo.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.prodinfo.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.prodinfo.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.content.in">
      <xsl:apply-templates select="*" mode="topic.prodinfo.child"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodinfo.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/prodinfo '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prodname.atts.in"/>

   <xsl:template match="*" mode="topic.prodname.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.prognum.atts.in"/>

   <xsl:template match="*" mode="topic.prognum.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.atts.in"/>

   <xsl:template match="*" mode="topic.prolog.content.in">
      <xsl:apply-templates select="." mode="topic.prolog.topic.author.in"/>
      <xsl:apply-templates select="." mode="topic.prolog.topic.source.in"/>
      <xsl:apply-templates select="." mode="topic.prolog.topic.publisher.in"/>
      <xsl:apply-templates select="." mode="topic.prolog.topic.copyright.in"/>
      <xsl:apply-templates select="." mode="topic.prolog.topic.critdates.in"/>
      <xsl:apply-templates select="." mode="topic.prolog.topic.permissions.in"/>
      <xsl:apply-templates select="." mode="topic.prolog.topic.metadata.in"/>
      <xsl:apply-templates select="." mode="topic.prolog.topic.resourceid.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.topic.author.in">
      <xsl:apply-templates select="." mode="topic.author.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/prolog '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.topic.source.in">
      <xsl:apply-templates select="." mode="topic.source.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/prolog '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.topic.publisher.in">
      <xsl:apply-templates select="." mode="topic.publisher.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/prolog '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.topic.copyright.in">
      <xsl:apply-templates select="." mode="topic.copyright.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/prolog '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.topic.critdates.in">
      <xsl:apply-templates select="." mode="topic.critdates.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/prolog '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.topic.permissions.in">
      <xsl:apply-templates select="." mode="topic.permissions.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/prolog '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.topic.metadata.in">
      <xsl:apply-templates select="." mode="topic.metadata.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/prolog '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.prolog.topic.resourceid.in">
      <xsl:apply-templates select="." mode="topic.resourceid.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/prolog '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.atts.in">
      <xsl:apply-templates select="." mode="topic.publisher.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.publisher.href.att.in"/>
      <xsl:apply-templates select="." mode="topic.publisher.keyref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.select.atts.in">
      <xsl:apply-templates select="." mode="topic.publisher.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.publisher.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.publisher.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.publisher.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.publisher.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.publisher.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.publisher.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.href.att.in">
      <xsl:apply-templates select="." mode="href.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.publisher.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.atts.in">
      <xsl:apply-templates select="." mode="topic.q.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.q.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.q.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.q.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.id.atts.in">
      <xsl:apply-templates select="." mode="topic.q.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.q.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.select.atts.in">
      <xsl:apply-templates select="." mode="topic.q.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.q.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.q.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.q.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.q.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.q.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.q.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.q.content.in">
      <xsl:apply-templates select="." mode="shortquote.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.atts.in">
      <xsl:apply-templates select="." mode="topic.related-links.rel.atts.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.format.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.scope.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.rel.atts.in">
      <xsl:apply-templates select="." mode="topic.related-links.type.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.role.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.otherrole.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.role.att.in">
      <xsl:apply-templates select="." mode="role.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.otherrole.att.in">
      <xsl:apply-templates select="." mode="otherrole.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.select.atts.in">
      <xsl:apply-templates select="." mode="topic.related-links.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.related-links.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.format.att.in">
      <xsl:apply-templates select="." mode="format.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.scope.att.in">
      <xsl:apply-templates select="." mode="scope.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.content.in">
      <xsl:apply-templates select="*" mode="topic.related-links.child"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.related-links.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/related-links '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.atts.in">
      <xsl:apply-templates select="." mode="topic.required-cleanup.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.remap.att.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.required-cleanup.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.id.atts.in">
      <xsl:apply-templates select="." mode="topic.required-cleanup.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.select.atts.in">
      <xsl:apply-templates select="." mode="topic.required-cleanup.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.required-cleanup.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.remap.att.in">
      <xsl:apply-templates select="." mode="remap.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.required-cleanup.content.in">
      <xsl:apply-templates select="*|text()" mode="topic.required-cleanup.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="topic.required-cleanup.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/required-cleanup '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.resourceid.atts.in">
      <xsl:apply-templates select="." mode="topic.resourceid.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.resourceid.appname.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.resourceid.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.resourceid.appname.att.in">
      <xsl:apply-templates select="." mode="appname.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.atts.in">
      <xsl:apply-templates select="." mode="topic.revised.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.revised.modified.att.in"/>
      <xsl:apply-templates select="." mode="topic.revised.golive.att.in"/>
      <xsl:apply-templates select="." mode="topic.revised.expiry.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.select.atts.in">
      <xsl:apply-templates select="." mode="topic.revised.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.revised.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.revised.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.revised.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.revised.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.revised.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.revised.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.modified.att.in">
      <xsl:apply-templates select="." mode="modified.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.golive.att.in">
      <xsl:apply-templates select="." mode="golive.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.revised.expiry.att.in">
      <xsl:apply-templates select="." mode="expiry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.atts.in">
      <xsl:apply-templates select="." mode="topic.row.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.row.rowsep.att.in"/>
      <xsl:apply-templates select="." mode="topic.row.valign.att.in"/>
      <xsl:apply-templates select="." mode="topic.row.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.row.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.row.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.id.atts.in">
      <xsl:apply-templates select="." mode="topic.row.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.row.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.select.atts.in">
      <xsl:apply-templates select="." mode="topic.row.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.row.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.row.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.row.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.row.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.row.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.row.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.rowsep.att.in">
      <xsl:apply-templates select="." mode="rowsep.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.valign.att.in">
      <xsl:apply-templates select="." mode="valign.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.content.in">
      <xsl:apply-templates select="." mode="topic.row.topic.entry.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.row.topic.entry.in">
      <xsl:apply-templates select="." mode="topic.entry.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/row '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.searchtitle.atts.in">
      <xsl:apply-templates select="." mode="topic.searchtitle.id.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.searchtitle.id.atts.in">
      <xsl:apply-templates select="." mode="topic.searchtitle.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.searchtitle.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.searchtitle.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.searchtitle.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.searchtitle.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.atts.in">
      <xsl:apply-templates select="." mode="topic.section.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.section.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.section.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.section.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.section.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.id.atts.in">
      <xsl:apply-templates select="." mode="topic.section.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.section.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.select.atts.in">
      <xsl:apply-templates select="." mode="topic.section.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.section.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.section.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.section.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.section.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.section.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.section.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.section.content.in">
      <xsl:apply-templates select="." mode="section.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.series.atts.in"/>

   <xsl:template match="*" mode="topic.series.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.shortdesc.atts.in">
      <xsl:apply-templates select="." mode="topic.shortdesc.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.shortdesc.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.shortdesc.id.atts.in">
      <xsl:apply-templates select="." mode="topic.shortdesc.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.shortdesc.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.shortdesc.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.shortdesc.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.shortdesc.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.shortdesc.content.in">
      <xsl:apply-templates select="." mode="title.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.atts.in">
      <xsl:apply-templates select="." mode="topic.simpletable.display.atts.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.relcolwidth.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.keycol.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.refcols.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.spectitle.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.display.atts.in">
      <xsl:apply-templates select="." mode="topic.simpletable.scale.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.frame.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.simpletable.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.id.atts.in">
      <xsl:apply-templates select="." mode="topic.simpletable.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.select.atts.in">
      <xsl:apply-templates select="." mode="topic.simpletable.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.relcolwidth.att.in">
      <xsl:apply-templates select="." mode="relcolwidth.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.keycol.att.in">
      <xsl:apply-templates select="." mode="keycol.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.refcols.att.in">
      <xsl:apply-templates select="." mode="refcols.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.content.in">
      <xsl:apply-templates select="." mode="topic.simpletable.topic.sthead.in"/>
      <xsl:apply-templates select="." mode="topic.simpletable.topic.strow.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.topic.sthead.in">
      <xsl:apply-templates select="." mode="topic.sthead.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/simpletable '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.simpletable.topic.strow.in">
      <xsl:apply-templates select="." mode="topic.strow.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/simpletable '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.atts.in">
      <xsl:apply-templates select="." mode="topic.sl.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.sl.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.sl.compact.att.in"/>
      <xsl:apply-templates select="." mode="topic.sl.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.sl.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.sl.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.id.atts.in">
      <xsl:apply-templates select="." mode="topic.sl.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.sl.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.select.atts.in">
      <xsl:apply-templates select="." mode="topic.sl.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.sl.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.sl.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.sl.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.sl.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.sl.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.sl.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.compact.att.in">
      <xsl:apply-templates select="." mode="compact.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.content.in">
      <xsl:apply-templates select="." mode="topic.sl.topic.sli.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sl.topic.sli.in">
      <xsl:apply-templates select="." mode="topic.sli.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/sl '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.atts.in">
      <xsl:apply-templates select="." mode="topic.sli.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.sli.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.sli.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.sli.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.id.atts.in">
      <xsl:apply-templates select="." mode="topic.sli.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.sli.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.select.atts.in">
      <xsl:apply-templates select="." mode="topic.sli.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.sli.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.sli.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.sli.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.sli.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.sli.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.sli.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sli.content.in">
      <xsl:apply-templates select="." mode="ph.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.source.atts.in">
      <xsl:apply-templates select="." mode="topic.source.href.att.in"/>
      <xsl:apply-templates select="." mode="topic.source.keyref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.source.href.att.in">
      <xsl:apply-templates select="." mode="href.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.source.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.source.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.atts.in">
      <xsl:apply-templates select="." mode="topic.state.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.state.name.att.in"/>
      <xsl:apply-templates select="." mode="topic.state.value.att.in"/>
      <xsl:apply-templates select="." mode="topic.state.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.state.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.state.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.id.atts.in">
      <xsl:apply-templates select="." mode="topic.state.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.state.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.select.atts.in">
      <xsl:apply-templates select="." mode="topic.state.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.state.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.state.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.state.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.state.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.state.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.state.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.name.att.in">
      <xsl:apply-templates select="." mode="name.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.value.att.in">
      <xsl:apply-templates select="." mode="value.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.state.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.atts.in">
      <xsl:apply-templates select="." mode="topic.stentry.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.specentry.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.stentry.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.id.atts.in">
      <xsl:apply-templates select="." mode="topic.stentry.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.select.atts.in">
      <xsl:apply-templates select="." mode="topic.stentry.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.stentry.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.stentry.content.in">
      <xsl:apply-templates select="." mode="tblcell.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.atts.in">
      <xsl:apply-templates select="." mode="topic.sthead.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.sthead.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.sthead.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.sthead.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.id.atts.in">
      <xsl:apply-templates select="." mode="topic.sthead.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.sthead.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.select.atts.in">
      <xsl:apply-templates select="." mode="topic.sthead.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.sthead.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.sthead.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.sthead.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.sthead.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.sthead.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.sthead.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.content.in">
      <xsl:apply-templates select="." mode="topic.sthead.topic.stentry.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.sthead.topic.stentry.in">
      <xsl:apply-templates select="." mode="topic.stentry.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/sthead '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.atts.in">
      <xsl:apply-templates select="." mode="topic.strow.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.strow.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.strow.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.strow.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.id.atts.in">
      <xsl:apply-templates select="." mode="topic.strow.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.strow.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.select.atts.in">
      <xsl:apply-templates select="." mode="topic.strow.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.strow.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.strow.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.strow.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.strow.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.strow.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.strow.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.content.in">
      <xsl:apply-templates select="." mode="topic.strow.topic.stentry.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.strow.topic.stentry.in">
      <xsl:apply-templates select="." mode="topic.stentry.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/strow '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.atts.in">
      <xsl:apply-templates select="." mode="topic.table.colrowsep.atts.in"/>
      <xsl:apply-templates select="." mode="topic.table.display.atts.in"/>
      <xsl:apply-templates select="." mode="topic.table.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.table.pgwide.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.rowheader.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.colrowsep.atts.in">
      <xsl:apply-templates select="." mode="topic.table.colsep.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.rowsep.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.colsep.att.in">
      <xsl:apply-templates select="." mode="colsep.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.rowsep.att.in">
      <xsl:apply-templates select="." mode="rowsep.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.display.atts.in">
      <xsl:apply-templates select="." mode="topic.table.scale.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.frame.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.table.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.table.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.id.atts.in">
      <xsl:apply-templates select="." mode="topic.table.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.select.atts.in">
      <xsl:apply-templates select="." mode="topic.table.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.table.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.pgwide.att.in">
      <xsl:apply-templates select="." mode="pgwide.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.rowheader.att.in">
      <xsl:apply-templates select="." mode="rowheader.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.content.in">
      <xsl:apply-templates select="." mode="topic.table.topic.title.in"/>
      <xsl:apply-templates select="." mode="topic.table.topic.desc.in"/>
      <xsl:apply-templates select="." mode="topic.table.topic.tgroup.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.topic.title.in">
      <xsl:apply-templates select="." mode="topic.title.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/table '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.topic.desc.in">
      <xsl:apply-templates select="." mode="topic.desc.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/table '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.table.topic.tgroup.in">
      <xsl:apply-templates select="." mode="topic.tgroup.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/table '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.atts.in">
      <xsl:apply-templates select="." mode="topic.tbody.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.valign.att.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.tbody.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.id.atts.in">
      <xsl:apply-templates select="." mode="topic.tbody.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.select.atts.in">
      <xsl:apply-templates select="." mode="topic.tbody.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.tbody.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.valign.att.in">
      <xsl:apply-templates select="." mode="valign.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.content.in">
      <xsl:apply-templates select="." mode="topic.tbody.topic.row.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tbody.topic.row.in">
      <xsl:apply-templates select="." mode="topic.row.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/tbody '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.atts.in">
      <xsl:apply-templates select="." mode="topic.term.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.term.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.term.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.term.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.term.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.id.atts.in">
      <xsl:apply-templates select="." mode="topic.term.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.term.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.select.atts.in">
      <xsl:apply-templates select="." mode="topic.term.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.term.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.term.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.term.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.term.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.term.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.term.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.term.content.in">
      <xsl:apply-templates select="*|text()" mode="topic.term.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="topic.term.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/term '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.atts.in">
      <xsl:apply-templates select="." mode="topic.tgroup.colrowsep.atts.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.cols.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.align.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.colrowsep.atts.in">
      <xsl:apply-templates select="." mode="topic.tgroup.colsep.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.rowsep.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.colsep.att.in">
      <xsl:apply-templates select="." mode="colsep.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.rowsep.att.in">
      <xsl:apply-templates select="." mode="rowsep.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.tgroup.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.id.atts.in">
      <xsl:apply-templates select="." mode="topic.tgroup.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.select.atts.in">
      <xsl:apply-templates select="." mode="topic.tgroup.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.cols.att.in">
      <xsl:apply-templates select="." mode="cols.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.align.att.in">
      <xsl:apply-templates select="." mode="align.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.content.in">
      <xsl:apply-templates select="." mode="topic.tgroup.topic.colspec.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.topic.thead.in"/>
      <xsl:apply-templates select="." mode="topic.tgroup.topic.tbody.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.topic.colspec.in">
      <xsl:apply-templates select="." mode="topic.colspec.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/tgroup '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.topic.thead.in">
      <xsl:apply-templates select="." mode="topic.thead.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/tgroup '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tgroup.topic.tbody.in">
      <xsl:apply-templates select="." mode="topic.tbody.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/tgroup '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.atts.in">
      <xsl:apply-templates select="." mode="topic.thead.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.thead.valign.att.in"/>
      <xsl:apply-templates select="." mode="topic.thead.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.thead.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.thead.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.id.atts.in">
      <xsl:apply-templates select="." mode="topic.thead.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.thead.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.select.atts.in">
      <xsl:apply-templates select="." mode="topic.thead.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.thead.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.thead.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.thead.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.thead.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.thead.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.thead.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.valign.att.in">
      <xsl:apply-templates select="." mode="valign.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.content.in">
      <xsl:apply-templates select="." mode="topic.thead.topic.row.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.thead.topic.row.in">
      <xsl:apply-templates select="." mode="topic.row.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/thead '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.title.atts.in">
      <xsl:apply-templates select="." mode="topic.title.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.title.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.title.id.atts.in">
      <xsl:apply-templates select="." mode="topic.title.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.title.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.title.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.title.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.title.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.title.content.in">
      <xsl:apply-templates select="." mode="title.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.titlealts.atts.in">
      <xsl:apply-templates select="." mode="topic.titlealts.id.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.titlealts.id.atts.in">
      <xsl:apply-templates select="." mode="topic.titlealts.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.titlealts.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.titlealts.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.titlealts.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.titlealts.content.in">
      <xsl:apply-templates select="." mode="topic.titlealts.topic.navtitle.in"/>
      <xsl:apply-templates select="." mode="topic.titlealts.topic.searchtitle.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.titlealts.topic.navtitle.in">
      <xsl:apply-templates select="." mode="topic.navtitle.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/titlealts '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.titlealts.topic.searchtitle.in">
      <xsl:apply-templates select="." mode="topic.searchtitle.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/titlealts '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tm.atts.in">
      <xsl:apply-templates select="." mode="topic.tm.trademark.att.in"/>
      <xsl:apply-templates select="." mode="topic.tm.tmowner.att.in"/>
      <xsl:apply-templates select="." mode="topic.tm.tmtype.att.in"/>
      <xsl:apply-templates select="." mode="topic.tm.tmclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.tm.trademark.att.in">
      <xsl:apply-templates select="." mode="trademark.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tm.tmowner.att.in">
      <xsl:apply-templates select="." mode="tmowner.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tm.tmtype.att.in">
      <xsl:apply-templates select="." mode="tmtype.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tm.tmclass.att.in">
      <xsl:apply-templates select="." mode="tmclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.tm.content.in">
      <xsl:apply-templates select="*|text()" mode="topic.tm.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="topic.tm.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/tm '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.atts.in">
      <xsl:attribute name="xml:lang">
         <xsl:text>en-us</xsl:text>
      </xsl:attribute>
      <xsl:apply-templates select="." mode="topic.topic.select.atts.in"/>
      <xsl:apply-templates select="." mode="topic.topic.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.topic.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="topic.topic.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.select.atts.in">
      <xsl:apply-templates select="." mode="topic.topic.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.topic.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.topic.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.topic.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.topic.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.topic.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.topic.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.content.in">
      <xsl:apply-templates select="." mode="topic.topic.topic.title.in"/>
      <xsl:apply-templates select="." mode="topic.topic.topic.titlealts.in"/>
      <xsl:apply-templates select="." mode="topic.topic.topic.shortdesc.in"/>
      <xsl:apply-templates select="." mode="topic.topic.topic.prolog.in"/>
      <xsl:apply-templates select="." mode="topic.topic.topic.body.in"/>
      <xsl:apply-templates select="." mode="topic.topic.topic.related-links.in"/>
      <xsl:apply-templates select="." mode="topic.topic.topic.topic.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.topic.title.in">
      <xsl:apply-templates select="." mode="topic.title.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/topic '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.topic.titlealts.in">
      <xsl:apply-templates select="." mode="topic.titlealts.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/topic '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.topic.shortdesc.in">
      <xsl:apply-templates select="." mode="topic.shortdesc.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/topic '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.topic.prolog.in">
      <xsl:apply-templates select="." mode="topic.prolog.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/topic '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.topic.body.in">
      <xsl:apply-templates select="." mode="topic.body.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/topic '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.topic.related-links.in">
      <xsl:apply-templates select="." mode="topic.related-links.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' topic/topic '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.topic.topic.topic.in">
      <xsl:apply-templates select="." mode="topic.topic.in">
         <xsl:with-param name="container" select="' topic/topic '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.atts.in">
      <xsl:apply-templates select="." mode="topic.ul.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.ul.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="topic.ul.compact.att.in"/>
      <xsl:apply-templates select="." mode="topic.ul.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.ul.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.ul.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.id.atts.in">
      <xsl:apply-templates select="." mode="topic.ul.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.ul.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.select.atts.in">
      <xsl:apply-templates select="." mode="topic.ul.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.ul.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.ul.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.ul.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.ul.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.ul.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.ul.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.compact.att.in">
      <xsl:apply-templates select="." mode="compact.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.content.in">
      <xsl:apply-templates select="." mode="topic.ul.topic.li.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.ul.topic.li.in">
      <xsl:apply-templates select="." mode="topic.li.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/ul '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrm.atts.in">
      <xsl:apply-templates select="." mode="topic.vrm.version.att.in"/>
      <xsl:apply-templates select="." mode="topic.vrm.release.att.in"/>
      <xsl:apply-templates select="." mode="topic.vrm.modification.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrm.version.att.in">
      <xsl:apply-templates select="." mode="version.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrm.release.att.in">
      <xsl:apply-templates select="." mode="release.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrm.modification.att.in">
      <xsl:apply-templates select="." mode="modification.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrmlist.atts.in"/>

   <xsl:template match="*" mode="topic.vrmlist.content.in">
      <xsl:apply-templates select="." mode="topic.vrmlist.topic.vrm.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.vrmlist.topic.vrm.in">
      <xsl:apply-templates select="." mode="topic.vrm.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' topic/vrmlist '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.atts.in">
      <xsl:apply-templates select="." mode="topic.xref.univ.atts.in"/>
      <xsl:apply-templates select="." mode="topic.xref.href.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.keyref.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.type.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.format.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.scope.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.univ.atts.in">
      <xsl:apply-templates select="." mode="topic.xref.id.atts.in"/>
      <xsl:apply-templates select="." mode="topic.xref.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.id.atts.in">
      <xsl:apply-templates select="." mode="topic.xref.id.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.select.atts.in">
      <xsl:apply-templates select="." mode="topic.xref.platform.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.product.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.audience.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.rev.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.importance.att.in"/>
      <xsl:apply-templates select="." mode="topic.xref.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.href.att.in">
      <xsl:apply-templates select="." mode="href.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.type.att.in">
      <xsl:apply-templates select="." mode="type.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.format.att.in">
      <xsl:apply-templates select="." mode="format.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.scope.att.in">
      <xsl:apply-templates select="." mode="scope.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="topic.xref.content.in">
      <xsl:apply-templates select="*|text()" mode="topic.xref.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="topic.xref.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' topic/xref '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

<!--= = = DEFAULT CONTENT GROUP INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="body.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="body.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="body.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="body.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="body.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="defn.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="defn.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="defn.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="defn.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="defn.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="desc.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="desc.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="desc.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="desc.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="desc.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="fig.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="fig.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="fig.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="fig.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="fig.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="fn.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="fn.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="fn.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="fn.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="fn.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="itemgroup.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="itemgroup.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="itemgroup.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="itemgroup.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="itemgroup.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="listitem.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="listitem.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="listitem.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="listitem.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="listitem.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="longquote.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="longquote.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="longquote.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="longquote.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="longquote.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="note.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="note.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="note.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="note.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="note.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="para.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="para.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="para.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="para.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="para.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ph.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="ph.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ph.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="ph.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="ph.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pre.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="pre.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pre.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="pre.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="pre.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="prodinfo.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="prodinfo.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="prodinfo.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="prodinfo.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="prodinfo.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="section.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="section.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="section.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="section.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="section.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="section.notitle.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="section.notitle.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="section.notitle.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="section.notitle.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="section.notitle.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="shortquote.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="shortquote.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="shortquote.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="shortquote.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="shortquote.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="tblcell.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="tblcell.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="tblcell.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="tblcell.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="tblcell.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="term.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="term.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="term.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="term.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="term.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="title.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="title.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="title.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="title.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="title.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="words.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="words.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="words.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="words.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="words.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="xrefph.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="xrefph.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="xrefph.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="xrefph.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="xrefph.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="xreftext.cnt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="*" mode="xreftext.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="xreftext.cnt.text.in">
      <xsl:param name="container"/>
      <xsl:apply-templates select="*|text()" mode="xreftext.cnt.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*|text()" mode="xreftext.cnt.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>


<!--= = = DEFAULT ATTRIBUTE INPUT RULES = = = = = =-->

   <xsl:template match="*" mode="align.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="alt.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="appname.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="archive.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="audience.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="author.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="callout.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="char.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="charoff.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="classid.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="codebase.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="codetype.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="collection-type.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="colname.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="colnum.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="cols.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="colsep.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="colwidth.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="compact.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="conref.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="content.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="data.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="date.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="declare.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="disposition.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="duplicates.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="expanse.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="experiencelevel.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="expiry.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="format.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="frame.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="golive.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="height.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="href.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="id.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="importance.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="job.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="keycol.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="keyref.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="longdescref.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="mapkeyref.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="modification.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="modified.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="morerows.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="name.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="nameend.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="namest.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="otherjob.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="otherprops.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="otherrole.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="othertype.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="outputclass.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="pgwide.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="placement.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="platform.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="product.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="query.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="refcols.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="reftitle.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="relcolwidth.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="release.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="remap.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="rev.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="role.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="rowheader.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="rowsep.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="scale.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="scope.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="spanname.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="specentry.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="spectitle.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="standby.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="state.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="status.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="tabindex.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="time.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="tmclass.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="tmowner.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="tmtype.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="trademark.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="translate.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="translate-content.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="type.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="usemap.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="valign.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="value.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="valuetype.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="version.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="view.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="width.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>

   <xsl:template match="*" mode="year.att.in">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:param name="container"/>
   </xsl:template>


<!--= = = DEFAULT RULES = = = = = = = = = = = = = =-->

   <xsl:template match="*" mode="child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unmatched.child">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template name="check.unmatched.child">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:if test="$REPORT_UNMATCHED='yes'">
         <xsl:comment>
            <xsl:value-of select="name()"/>
            <xsl:text> not processed within </xsl:text>
            <xsl:value-of select="$container"/>
         </xsl:comment>
      </xsl:if>
      <xsl:if test="$PROCESS_CHILDREN='yes'">
         <xsl:apply-templates select="*|text()" mode="child"/>
      </xsl:if>
   </xsl:template>

   <xsl:template match="text()" mode="child">
      <xsl:param name="container"/>
      <xsl:if test="$PROCESS_TEXT='yes'">
         <xsl:copy-of select="."/>
      </xsl:if>
   </xsl:template>

   <xsl:template name="check.unsupplied.input">
      <xsl:param name="input"/>
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:if test="$isRequired='yes'">
         <xsl:message>
            <xsl:text>required </xsl:text>
            <xsl:value-of select="$input"/>
            <xsl:text> not supplied within </xsl:text>
            <xsl:value-of select="$container"/>
         </xsl:message>
      </xsl:if>
      <xsl:if test="$isRequired='yes' or $REPORT_UNMATCHED='yes'">
         <xsl:comment>
            <xsl:value-of select="$input"/>
            <xsl:text> not supplied within </xsl:text>
            <xsl:value-of select="$container"/>
         </xsl:comment>
      </xsl:if>
   </xsl:template>

</xsl:stylesheet>
