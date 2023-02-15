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

   <xsl:template match="*" mode="sw-d.cmdname.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' sw-d/cmdname '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.out">
      <cmdname>
         <xsl:apply-templates select="." mode="sw-d.cmdname.atts.in"/>
         <xsl:apply-templates select="." mode="sw-d.cmdname.content.in"/>
      </cmdname>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' sw-d/filepath '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.out">
      <filepath>
         <xsl:apply-templates select="." mode="sw-d.filepath.atts.in"/>
         <xsl:apply-templates select="." mode="sw-d.filepath.content.in"/>
      </filepath>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' sw-d/msgblock '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.out">
      <msgblock>
         <xsl:apply-templates select="." mode="sw-d.msgblock.atts.in"/>
         <xsl:apply-templates select="." mode="sw-d.msgblock.content.in"/>
      </msgblock>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' sw-d/msgnum '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.out">
      <msgnum>
         <xsl:apply-templates select="." mode="sw-d.msgnum.atts.in"/>
         <xsl:apply-templates select="." mode="sw-d.msgnum.content.in"/>
      </msgnum>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' sw-d/msgph '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.out">
      <msgph>
         <xsl:apply-templates select="." mode="sw-d.msgph.atts.in"/>
         <xsl:apply-templates select="." mode="sw-d.msgph.content.in"/>
      </msgph>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' sw-d/systemoutput '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.out">
      <systemoutput>
         <xsl:apply-templates select="." mode="sw-d.systemoutput.atts.in"/>
         <xsl:apply-templates select="." mode="sw-d.systemoutput.content.in"/>
      </systemoutput>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' sw-d/userinput '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.out">
      <userinput>
         <xsl:apply-templates select="." mode="sw-d.userinput.atts.in"/>
         <xsl:apply-templates select="." mode="sw-d.userinput.content.in"/>
      </userinput>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' sw-d/varname '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.out">
      <varname>
         <xsl:apply-templates select="." mode="sw-d.varname.atts.in"/>
         <xsl:apply-templates select="." mode="sw-d.varname.content.in"/>
      </varname>
   </xsl:template>


<!--= = = DEFAULT ELEMENT INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="sw-d.cmdname.atts.in">
      <xsl:apply-templates select="." mode="sw-d.cmdname.univ.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.keyref.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.univ.atts.in">
      <xsl:apply-templates select="." mode="sw-d.cmdname.id.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.id.atts.in">
      <xsl:apply-templates select="." mode="sw-d.cmdname.id.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.select.atts.in">
      <xsl:apply-templates select="." mode="sw-d.cmdname.platform.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.product.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.audience.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.rev.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.importance.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.cmdname.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.cmdname.content.in">
      <xsl:apply-templates select="*|text()" mode="sw-d.cmdname.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="sw-d.cmdname.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' sw-d/cmdname '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.atts.in">
      <xsl:apply-templates select="." mode="sw-d.filepath.univ.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.filepath.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.univ.atts.in">
      <xsl:apply-templates select="." mode="sw-d.filepath.id.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.filepath.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.id.atts.in">
      <xsl:apply-templates select="." mode="sw-d.filepath.id.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.filepath.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.select.atts.in">
      <xsl:apply-templates select="." mode="sw-d.filepath.platform.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.filepath.product.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.filepath.audience.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.filepath.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.filepath.rev.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.filepath.importance.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.filepath.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.filepath.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgblock.display.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.univ.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.spectitle.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.display.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgblock.scale.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.frame.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.univ.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgblock.id.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.id.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgblock.id.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.select.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgblock.platform.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.product.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.audience.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.rev.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.importance.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgblock.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgblock.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgnum.univ.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.keyref.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.univ.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgnum.id.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.id.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgnum.id.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.select.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgnum.platform.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.product.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.audience.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.rev.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.importance.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgnum.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgnum.content.in">
      <xsl:apply-templates select="*|text()" mode="sw-d.msgnum.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="sw-d.msgnum.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' sw-d/msgnum '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgph.univ.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgph.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.univ.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgph.id.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgph.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.id.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgph.id.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgph.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.select.atts.in">
      <xsl:apply-templates select="." mode="sw-d.msgph.platform.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgph.product.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgph.audience.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgph.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgph.rev.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgph.importance.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.msgph.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.msgph.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.atts.in">
      <xsl:apply-templates select="." mode="sw-d.systemoutput.univ.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.systemoutput.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.univ.atts.in">
      <xsl:apply-templates select="." mode="sw-d.systemoutput.id.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.systemoutput.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.id.atts.in">
      <xsl:apply-templates select="." mode="sw-d.systemoutput.id.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.systemoutput.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.select.atts.in">
      <xsl:apply-templates select="." mode="sw-d.systemoutput.platform.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.systemoutput.product.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.systemoutput.audience.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.systemoutput.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.systemoutput.rev.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.systemoutput.importance.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.systemoutput.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.systemoutput.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.atts.in">
      <xsl:apply-templates select="." mode="sw-d.userinput.univ.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.userinput.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.univ.atts.in">
      <xsl:apply-templates select="." mode="sw-d.userinput.id.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.userinput.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.id.atts.in">
      <xsl:apply-templates select="." mode="sw-d.userinput.id.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.userinput.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.select.atts.in">
      <xsl:apply-templates select="." mode="sw-d.userinput.platform.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.userinput.product.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.userinput.audience.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.userinput.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.userinput.rev.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.userinput.importance.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.userinput.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.userinput.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.atts.in">
      <xsl:apply-templates select="." mode="sw-d.varname.univ.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.keyref.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.univ.atts.in">
      <xsl:apply-templates select="." mode="sw-d.varname.id.atts.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.id.atts.in">
      <xsl:apply-templates select="." mode="sw-d.varname.id.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.select.atts.in">
      <xsl:apply-templates select="." mode="sw-d.varname.platform.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.product.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.audience.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.rev.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.importance.att.in"/>
      <xsl:apply-templates select="." mode="sw-d.varname.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="sw-d.varname.content.in">
      <xsl:apply-templates select="*|text()" mode="sw-d.varname.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="sw-d.varname.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' sw-d/varname '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>



</xsl:stylesheet>
