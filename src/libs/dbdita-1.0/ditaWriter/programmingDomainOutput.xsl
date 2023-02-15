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

   <xsl:template match="*" mode="pr-d.apiname.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/apiname '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.out">
      <apiname>
         <xsl:apply-templates select="." mode="pr-d.apiname.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.apiname.content.in"/>
      </apiname>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/codeblock '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.out">
      <codeblock>
         <xsl:apply-templates select="." mode="pr-d.codeblock.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.codeblock.content.in"/>
      </codeblock>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/codeph '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.out">
      <codeph>
         <xsl:apply-templates select="." mode="pr-d.codeph.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.codeph.content.in"/>
      </codeph>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/delim '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.out">
      <delim>
         <xsl:apply-templates select="." mode="pr-d.delim.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.delim.content.in"/>
      </delim>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/fragment '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.out">
      <fragment>
         <xsl:apply-templates select="." mode="pr-d.fragment.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.fragment.content.in"/>
      </fragment>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/fragref '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.out">
      <fragref>
         <xsl:apply-templates select="." mode="pr-d.fragref.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.fragref.content.in"/>
      </fragref>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/groupchoice '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.out">
      <groupchoice>
         <xsl:apply-templates select="." mode="pr-d.groupchoice.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.groupchoice.content.in"/>
      </groupchoice>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/groupcomp '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.out">
      <groupcomp>
         <xsl:apply-templates select="." mode="pr-d.groupcomp.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.groupcomp.content.in"/>
      </groupcomp>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/groupseq '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.out">
      <groupseq>
         <xsl:apply-templates select="." mode="pr-d.groupseq.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.groupseq.content.in"/>
      </groupseq>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/kwd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.out">
      <kwd>
         <xsl:apply-templates select="." mode="pr-d.kwd.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.kwd.content.in"/>
      </kwd>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/oper '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.out">
      <oper>
         <xsl:apply-templates select="." mode="pr-d.oper.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.oper.content.in"/>
      </oper>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/option '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.out">
      <option>
         <xsl:apply-templates select="." mode="pr-d.option.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.option.content.in"/>
      </option>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/parml '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.out">
      <parml>
         <xsl:apply-templates select="." mode="pr-d.parml.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.parml.content.in"/>
      </parml>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/parmname '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.out">
      <parmname>
         <xsl:apply-templates select="." mode="pr-d.parmname.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.parmname.content.in"/>
      </parmname>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/pd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.out">
      <pd>
         <xsl:apply-templates select="." mode="pr-d.pd.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.pd.content.in"/>
      </pd>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/plentry '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.out">
      <plentry>
         <xsl:apply-templates select="." mode="pr-d.plentry.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.plentry.content.in"/>
      </plentry>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/pt '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.out">
      <pt>
         <xsl:apply-templates select="." mode="pr-d.pt.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.pt.content.in"/>
      </pt>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/repsep '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.out">
      <repsep>
         <xsl:apply-templates select="." mode="pr-d.repsep.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.repsep.content.in"/>
      </repsep>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/sep '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.out">
      <sep>
         <xsl:apply-templates select="." mode="pr-d.sep.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.sep.content.in"/>
      </sep>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/synblk '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.out">
      <synblk>
         <xsl:apply-templates select="." mode="pr-d.synblk.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.synblk.content.in"/>
      </synblk>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/synnote '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.out">
      <synnote>
         <xsl:apply-templates select="." mode="pr-d.synnote.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.synnote.content.in"/>
      </synnote>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/synnoteref '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.out">
      <synnoteref>
         <xsl:apply-templates select="." mode="pr-d.synnoteref.atts.in"/>
      </synnoteref>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/synph '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.out">
      <synph>
         <xsl:apply-templates select="." mode="pr-d.synph.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.synph.content.in"/>
      </synph>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/syntaxdiagram '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.out">
      <syntaxdiagram>
         <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.content.in"/>
      </syntaxdiagram>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' pr-d/var '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.out">
      <var>
         <xsl:apply-templates select="." mode="pr-d.var.atts.in"/>
         <xsl:apply-templates select="." mode="pr-d.var.content.in"/>
      </var>
   </xsl:template>


<!--= = = DEFAULT ELEMENT INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="pr-d.apiname.atts.in">
      <xsl:apply-templates select="." mode="pr-d.apiname.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.keyref.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.apiname.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.apiname.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.apiname.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.apiname.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.apiname.content.in">
      <xsl:apply-templates select="*|text()" mode="pr-d.apiname.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="pr-d.apiname.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/apiname '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.atts.in">
      <xsl:apply-templates select="." mode="pr-d.codeblock.display.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.spectitle.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.display.atts.in">
      <xsl:apply-templates select="." mode="pr-d.codeblock.scale.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.frame.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.codeblock.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.codeblock.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.codeblock.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeblock.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeblock.content.in">
      <xsl:apply-templates select="*|text()" mode="pr-d.codeblock.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="pr-d.codeblock.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/codeblock '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.atts.in">
      <xsl:apply-templates select="." mode="pr-d.codeph.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeph.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.codeph.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeph.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.codeph.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeph.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.codeph.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeph.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeph.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeph.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeph.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeph.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.codeph.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.codeph.content.in">
      <xsl:apply-templates select="*|text()" mode="pr-d.codeph.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="pr-d.codeph.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/codeph '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.atts.in">
      <xsl:apply-templates select="." mode="pr-d.delim.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.delim.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.delim.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.delim.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.delim.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.delim.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.delim.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.delim.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.delim.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.delim.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.delim.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.delim.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.delim.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.atts.in">
      <xsl:apply-templates select="." mode="pr-d.fragment.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragment.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.fragment.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragment.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.fragment.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragment.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.fragment.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragment.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragment.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragment.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragment.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragment.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragment.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.content.in">
      <xsl:apply-templates select="*" mode="pr-d.fragment.child"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragment.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/fragment '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.atts.in">
      <xsl:apply-templates select="." mode="pr-d.fragref.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.href.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.fragref.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.fragref.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.fragref.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.href.att.in">
      <xsl:apply-templates select="." mode="href.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.fragref.content.in">
      <xsl:apply-templates select="." mode="xrefph.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.atts.in">
      <xsl:apply-templates select="." mode="pr-d.groupchoice.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupchoice.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupchoice.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.groupchoice.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupchoice.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupchoice.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupchoice.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupchoice.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupchoice.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupchoice.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.groupchoice.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupchoice.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.content.in">
      <xsl:apply-templates select="*" mode="pr-d.groupchoice.child"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupchoice.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/groupchoice '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.atts.in">
      <xsl:apply-templates select="." mode="pr-d.groupcomp.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupcomp.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupcomp.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.groupcomp.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupcomp.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupcomp.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupcomp.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupcomp.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupcomp.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupcomp.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.groupcomp.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupcomp.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.content.in">
      <xsl:apply-templates select="*" mode="pr-d.groupcomp.child"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupcomp.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/groupcomp '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.atts.in">
      <xsl:apply-templates select="." mode="pr-d.groupseq.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupseq.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupseq.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.groupseq.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupseq.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupseq.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupseq.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupseq.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupseq.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupseq.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.groupseq.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.groupseq.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.content.in">
      <xsl:apply-templates select="*" mode="pr-d.groupseq.child"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.groupseq.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/groupseq '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.atts.in">
      <xsl:apply-templates select="." mode="pr-d.kwd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.keyref.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.kwd.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.kwd.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.kwd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.kwd.content.in">
      <xsl:apply-templates select="*|text()" mode="pr-d.kwd.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="pr-d.kwd.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/kwd '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.atts.in">
      <xsl:apply-templates select="." mode="pr-d.oper.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.oper.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.oper.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.oper.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.oper.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.oper.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.oper.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.oper.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.oper.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.oper.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.oper.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.oper.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.oper.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.atts.in">
      <xsl:apply-templates select="." mode="pr-d.option.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.keyref.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.option.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.option.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.option.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.option.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.option.content.in">
      <xsl:apply-templates select="*|text()" mode="pr-d.option.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="pr-d.option.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/option '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.atts.in">
      <xsl:apply-templates select="." mode="pr-d.parml.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.spectitle.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.compact.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.parml.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.parml.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.parml.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parml.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.compact.att.in">
      <xsl:apply-templates select="." mode="compact.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.content.in">
      <xsl:apply-templates select="." mode="pr-d.parml.pr-d.plentry.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parml.pr-d.plentry.in">
      <xsl:apply-templates select="." mode="pr-d.plentry.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' pr-d/parml '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.atts.in">
      <xsl:apply-templates select="." mode="pr-d.parmname.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.keyref.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.parmname.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.parmname.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.parmname.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.parmname.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.parmname.content.in">
      <xsl:apply-templates select="*|text()" mode="pr-d.parmname.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="pr-d.parmname.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/parmname '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.atts.in">
      <xsl:apply-templates select="." mode="pr-d.pd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.pd.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.pd.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.pd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.pd.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.pd.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pd.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pd.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pd.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pd.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pd.content.in">
      <xsl:apply-templates select="." mode="defn.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.atts.in">
      <xsl:apply-templates select="." mode="pr-d.plentry.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.plentry.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.plentry.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.plentry.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.content.in">
      <xsl:apply-templates select="." mode="pr-d.plentry.pr-d.pt.in"/>
      <xsl:apply-templates select="." mode="pr-d.plentry.pr-d.pd.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.pr-d.pt.in">
      <xsl:apply-templates select="." mode="pr-d.pt.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' pr-d/plentry '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.plentry.pr-d.pd.in">
      <xsl:apply-templates select="." mode="pr-d.pd.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' pr-d/plentry '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.atts.in">
      <xsl:apply-templates select="." mode="pr-d.pt.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.keyref.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.pt.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.pt.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.pt.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.pt.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.pt.content.in">
      <xsl:apply-templates select="." mode="term.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.atts.in">
      <xsl:apply-templates select="." mode="pr-d.repsep.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.repsep.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.repsep.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.repsep.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.repsep.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.repsep.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.repsep.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.repsep.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.repsep.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.repsep.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.repsep.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.repsep.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.repsep.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.atts.in">
      <xsl:apply-templates select="." mode="pr-d.sep.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.sep.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.sep.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.sep.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.sep.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.sep.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.sep.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.sep.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.sep.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.sep.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.sep.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.sep.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.sep.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synblk.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.synblk.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synblk.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.synblk.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synblk.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synblk.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synblk.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synblk.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synblk.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synblk.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synblk.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synblk.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synblk.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.content.in">
      <xsl:apply-templates select="*" mode="pr-d.synblk.child"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synblk.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/synblk '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synnote.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.callout.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synnote.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synnote.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synnote.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnote.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.callout.att.in">
      <xsl:apply-templates select="." mode="callout.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnote.content.in">
      <xsl:apply-templates select="*|text()" mode="pr-d.synnote.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="pr-d.synnote.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/synnote '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synnoteref.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.href.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synnoteref.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synnoteref.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synnoteref.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synnoteref.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.href.att.in">
      <xsl:apply-templates select="." mode="href.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synnoteref.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synph.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.synph.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synph.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.synph.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synph.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synph.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.synph.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synph.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synph.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synph.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synph.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synph.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.synph.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.synph.content.in">
      <xsl:apply-templates select="*|text()" mode="pr-d.synph.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="pr-d.synph.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/synph '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.atts.in">
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.display.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.select.atts.in">
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.importance.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.display.atts.in">
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.scale.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.frame.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.syntaxdiagram.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.content.in">
      <xsl:apply-templates select="*" mode="pr-d.syntaxdiagram.child"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.syntaxdiagram.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' pr-d/syntaxdiagram '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.atts.in">
      <xsl:apply-templates select="." mode="pr-d.var.univ.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.var.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.var.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.univ.atts.in">
      <xsl:apply-templates select="." mode="pr-d.var.id.atts.in"/>
      <xsl:apply-templates select="." mode="pr-d.var.platform.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.var.product.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.var.audience.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.var.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.var.rev.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.var.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.id.atts.in">
      <xsl:apply-templates select="." mode="pr-d.var.id.att.in"/>
      <xsl:apply-templates select="." mode="pr-d.var.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="pr-d.var.content.in">
      <xsl:apply-templates select="." mode="words.cnt.text.in"/>
   </xsl:template>



</xsl:stylesheet>
