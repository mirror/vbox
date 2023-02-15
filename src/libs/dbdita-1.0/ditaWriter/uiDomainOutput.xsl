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

   <xsl:template match="*" mode="ui-d.menucascade.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' ui-d/menucascade '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.out">
      <menucascade>
         <xsl:apply-templates select="." mode="ui-d.menucascade.atts.in"/>
         <xsl:apply-templates select="." mode="ui-d.menucascade.content.in"/>
      </menucascade>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' ui-d/screen '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.out">
      <screen>
         <xsl:apply-templates select="." mode="ui-d.screen.atts.in"/>
         <xsl:apply-templates select="." mode="ui-d.screen.content.in"/>
      </screen>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' ui-d/shortcut '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.out">
      <shortcut>
         <xsl:apply-templates select="." mode="ui-d.shortcut.atts.in"/>
         <xsl:apply-templates select="." mode="ui-d.shortcut.content.in"/>
      </shortcut>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' ui-d/uicontrol '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.out">
      <uicontrol>
         <xsl:apply-templates select="." mode="ui-d.uicontrol.atts.in"/>
         <xsl:apply-templates select="." mode="ui-d.uicontrol.content.in"/>
      </uicontrol>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' ui-d/wintitle '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.out">
      <wintitle>
         <xsl:apply-templates select="." mode="ui-d.wintitle.atts.in"/>
         <xsl:apply-templates select="." mode="ui-d.wintitle.content.in"/>
      </wintitle>
   </xsl:template>


<!--= = = DEFAULT ELEMENT INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="ui-d.menucascade.atts.in">
      <xsl:apply-templates select="." mode="ui-d.menucascade.univ.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.keyref.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.univ.atts.in">
      <xsl:apply-templates select="." mode="ui-d.menucascade.id.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.id.atts.in">
      <xsl:apply-templates select="." mode="ui-d.menucascade.id.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.select.atts.in">
      <xsl:apply-templates select="." mode="ui-d.menucascade.platform.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.product.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.audience.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.rev.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.importance.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.menucascade.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.content.in">
      <xsl:apply-templates select="." mode="ui-d.menucascade.ui-d.uicontrol.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.menucascade.ui-d.uicontrol.in">
      <xsl:apply-templates select="." mode="ui-d.uicontrol.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' ui-d/menucascade '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.atts.in">
      <xsl:apply-templates select="." mode="ui-d.screen.univ.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.display.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.univ.atts.in">
      <xsl:apply-templates select="." mode="ui-d.screen.id.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.id.atts.in">
      <xsl:apply-templates select="." mode="ui-d.screen.id.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.select.atts.in">
      <xsl:apply-templates select="." mode="ui-d.screen.platform.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.product.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.audience.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.rev.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.importance.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.display.atts.in">
      <xsl:apply-templates select="." mode="ui-d.screen.scale.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.frame.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.screen.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.screen.content.in">
      <xsl:apply-templates select="*|text()" mode="ui-d.screen.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="ui-d.screen.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' ui-d/screen '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.atts.in">
      <xsl:apply-templates select="." mode="ui-d.shortcut.univ.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.keyref.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.univ.atts.in">
      <xsl:apply-templates select="." mode="ui-d.shortcut.id.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.id.atts.in">
      <xsl:apply-templates select="." mode="ui-d.shortcut.id.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.select.atts.in">
      <xsl:apply-templates select="." mode="ui-d.shortcut.platform.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.product.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.audience.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.rev.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.importance.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.shortcut.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.shortcut.content.in">
      <xsl:apply-templates select="*|text()" mode="ui-d.shortcut.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="ui-d.shortcut.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' ui-d/shortcut '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.atts.in">
      <xsl:apply-templates select="." mode="ui-d.uicontrol.univ.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.keyref.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.univ.atts.in">
      <xsl:apply-templates select="." mode="ui-d.uicontrol.id.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.id.atts.in">
      <xsl:apply-templates select="." mode="ui-d.uicontrol.id.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.select.atts.in">
      <xsl:apply-templates select="." mode="ui-d.uicontrol.platform.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.product.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.audience.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.rev.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.importance.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.uicontrol.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.uicontrol.content.in">
      <xsl:apply-templates select="*|text()" mode="ui-d.uicontrol.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="ui-d.uicontrol.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' ui-d/uicontrol '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.atts.in">
      <xsl:apply-templates select="." mode="ui-d.wintitle.univ.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.keyref.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.univ.atts.in">
      <xsl:apply-templates select="." mode="ui-d.wintitle.id.atts.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.id.atts.in">
      <xsl:apply-templates select="." mode="ui-d.wintitle.id.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.select.atts.in">
      <xsl:apply-templates select="." mode="ui-d.wintitle.platform.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.product.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.audience.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.rev.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.importance.att.in"/>
      <xsl:apply-templates select="." mode="ui-d.wintitle.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="ui-d.wintitle.content.in">
      <xsl:apply-templates select="*|text()" mode="ui-d.wintitle.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="ui-d.wintitle.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' ui-d/wintitle '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>



</xsl:stylesheet>
