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

   <xsl:template match="*" mode="task.chdesc.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/chdesc '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.out">
      <chdesc>
         <xsl:apply-templates select="." mode="task.chdesc.atts.in"/>
         <xsl:apply-templates select="." mode="task.chdesc.content.in"/>
      </chdesc>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/chdeschd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.out">
      <chdeschd>
         <xsl:apply-templates select="." mode="task.chdeschd.atts.in"/>
         <xsl:apply-templates select="." mode="task.chdeschd.content.in"/>
      </chdeschd>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/chhead '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.out">
      <chhead>
         <xsl:apply-templates select="." mode="task.chhead.atts.in"/>
         <xsl:apply-templates select="." mode="task.chhead.content.in"/>
      </chhead>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/choice '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.out">
      <choice>
         <xsl:apply-templates select="." mode="task.choice.atts.in"/>
         <xsl:apply-templates select="." mode="task.choice.content.in"/>
      </choice>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/choices '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.out">
      <choices>
         <xsl:apply-templates select="." mode="task.choices.atts.in"/>
         <xsl:apply-templates select="." mode="task.choices.content.in"/>
      </choices>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/choicetable '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.out">
      <choicetable>
         <xsl:apply-templates select="." mode="task.choicetable.atts.in"/>
         <xsl:apply-templates select="." mode="task.choicetable.content.in"/>
      </choicetable>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/choption '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.out">
      <choption>
         <xsl:apply-templates select="." mode="task.choption.atts.in"/>
         <xsl:apply-templates select="." mode="task.choption.content.in"/>
      </choption>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/choptionhd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.out">
      <choptionhd>
         <xsl:apply-templates select="." mode="task.choptionhd.atts.in"/>
         <xsl:apply-templates select="." mode="task.choptionhd.content.in"/>
      </choptionhd>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/chrow '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.out">
      <chrow>
         <xsl:apply-templates select="." mode="task.chrow.atts.in"/>
         <xsl:apply-templates select="." mode="task.chrow.content.in"/>
      </chrow>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/cmd '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.out">
      <cmd>
         <xsl:apply-templates select="." mode="task.cmd.atts.in"/>
         <xsl:apply-templates select="." mode="task.cmd.content.in"/>
      </cmd>
   </xsl:template>

   <xsl:template match="*" mode="task.context.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/context '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.context.out">
      <context>
         <xsl:apply-templates select="." mode="task.context.atts.in"/>
         <xsl:apply-templates select="." mode="task.context.content.in"/>
      </context>
   </xsl:template>

   <xsl:template match="*" mode="task.info.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/info '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.info.out">
      <info>
         <xsl:apply-templates select="." mode="task.info.atts.in"/>
         <xsl:apply-templates select="." mode="task.info.content.in"/>
      </info>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/postreq '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.out">
      <postreq>
         <xsl:apply-templates select="." mode="task.postreq.atts.in"/>
         <xsl:apply-templates select="." mode="task.postreq.content.in"/>
      </postreq>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/prereq '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.out">
      <prereq>
         <xsl:apply-templates select="." mode="task.prereq.atts.in"/>
         <xsl:apply-templates select="." mode="task.prereq.content.in"/>
      </prereq>
   </xsl:template>

   <xsl:template match="*" mode="task.result.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/result '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.result.out">
      <result>
         <xsl:apply-templates select="." mode="task.result.atts.in"/>
         <xsl:apply-templates select="." mode="task.result.content.in"/>
      </result>
   </xsl:template>

   <xsl:template match="*" mode="task.step.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/step '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.step.out">
      <step>
         <xsl:apply-templates select="." mode="task.step.atts.in"/>
         <xsl:apply-templates select="." mode="task.step.content.in"/>
      </step>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/stepresult '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.out">
      <stepresult>
         <xsl:apply-templates select="." mode="task.stepresult.atts.in"/>
         <xsl:apply-templates select="." mode="task.stepresult.content.in"/>
      </stepresult>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/steps-unordered '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.out">
      <steps-unordered>
         <xsl:apply-templates select="." mode="task.steps-unordered.atts.in"/>
         <xsl:apply-templates select="." mode="task.steps-unordered.content.in"/>
      </steps-unordered>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/steps '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.out">
      <steps>
         <xsl:apply-templates select="." mode="task.steps.atts.in"/>
         <xsl:apply-templates select="." mode="task.steps.content.in"/>
      </steps>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/stepxmp '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.out">
      <stepxmp>
         <xsl:apply-templates select="." mode="task.stepxmp.atts.in"/>
         <xsl:apply-templates select="." mode="task.stepxmp.content.in"/>
      </stepxmp>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/substep '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.out">
      <substep>
         <xsl:apply-templates select="." mode="task.substep.atts.in"/>
         <xsl:apply-templates select="." mode="task.substep.content.in"/>
      </substep>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/substeps '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.out">
      <substeps>
         <xsl:apply-templates select="." mode="task.substeps.atts.in"/>
         <xsl:apply-templates select="." mode="task.substeps.content.in"/>
      </substeps>
   </xsl:template>

   <xsl:template match="*" mode="task.task.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/task '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.task.out">
      <task>
         <xsl:apply-templates select="." mode="task.task.atts.in"/>
         <xsl:apply-templates select="." mode="task.task.content.in"/>
      </task>
   </xsl:template>

   <xsl:template match="*" mode="task.taskbody.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/taskbody '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.taskbody.out">
      <taskbody>
         <xsl:apply-templates select="." mode="task.taskbody.atts.in"/>
         <xsl:apply-templates select="." mode="task.taskbody.content.in"/>
      </taskbody>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.in">
      <xsl:param name="container"/>
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:call-template name="check.unsupplied.input">
         <xsl:with-param name="container" select="$container"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
         <xsl:with-param name="input" select="' task/tutorialinfo '"/>
      </xsl:call-template>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.out">
      <tutorialinfo>
         <xsl:apply-templates select="." mode="task.tutorialinfo.atts.in"/>
         <xsl:apply-templates select="." mode="task.tutorialinfo.content.in"/>
      </tutorialinfo>
   </xsl:template>


<!--= = = DEFAULT ELEMENT INPUT RULES = = = = = = =-->

   <xsl:template match="*" mode="task.chdesc.atts.in">
      <xsl:apply-templates select="." mode="task.chdesc.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.specentry.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.univ.atts.in">
      <xsl:apply-templates select="." mode="task.chdesc.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.id.atts.in">
      <xsl:apply-templates select="." mode="task.chdesc.id.att.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.select.atts.in">
      <xsl:apply-templates select="." mode="task.chdesc.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.product.att.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.chdesc.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdesc.content.in">
      <xsl:apply-templates select="." mode="tblcell.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.atts.in">
      <xsl:apply-templates select="." mode="task.chdeschd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.specentry.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.univ.atts.in">
      <xsl:apply-templates select="." mode="task.chdeschd.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.id.atts.in">
      <xsl:apply-templates select="." mode="task.chdeschd.id.att.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.select.atts.in">
      <xsl:apply-templates select="." mode="task.chdeschd.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.product.att.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.chdeschd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chdeschd.content.in">
      <xsl:apply-templates select="." mode="tblcell.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.atts.in">
      <xsl:apply-templates select="." mode="task.chhead.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.chhead.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.univ.atts.in">
      <xsl:apply-templates select="." mode="task.chhead.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.chhead.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.id.atts.in">
      <xsl:apply-templates select="." mode="task.chhead.id.att.in"/>
      <xsl:apply-templates select="." mode="task.chhead.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.select.atts.in">
      <xsl:apply-templates select="." mode="task.chhead.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.chhead.product.att.in"/>
      <xsl:apply-templates select="." mode="task.chhead.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.chhead.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.chhead.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.chhead.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.chhead.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.content.in">
      <xsl:apply-templates select="." mode="task.chhead.task.choptionhd.in"/>
      <xsl:apply-templates select="." mode="task.chhead.task.chdeschd.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.task.choptionhd.in">
      <xsl:apply-templates select="." mode="task.choptionhd.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' task/chhead '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chhead.task.chdeschd.in">
      <xsl:apply-templates select="." mode="task.chdeschd.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' task/chhead '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.atts.in">
      <xsl:apply-templates select="." mode="task.choice.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.choice.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.univ.atts.in">
      <xsl:apply-templates select="." mode="task.choice.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.choice.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.id.atts.in">
      <xsl:apply-templates select="." mode="task.choice.id.att.in"/>
      <xsl:apply-templates select="." mode="task.choice.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.select.atts.in">
      <xsl:apply-templates select="." mode="task.choice.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.choice.product.att.in"/>
      <xsl:apply-templates select="." mode="task.choice.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.choice.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.choice.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.choice.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.choice.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choice.content.in">
      <xsl:apply-templates select="*|text()" mode="task.choice.child"/>
   </xsl:template>

   <xsl:template match="*|text()" mode="task.choice.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' task/choice '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.atts.in">
      <xsl:apply-templates select="." mode="task.choices.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.choices.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.univ.atts.in">
      <xsl:apply-templates select="." mode="task.choices.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.choices.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.id.atts.in">
      <xsl:apply-templates select="." mode="task.choices.id.att.in"/>
      <xsl:apply-templates select="." mode="task.choices.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.select.atts.in">
      <xsl:apply-templates select="." mode="task.choices.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.choices.product.att.in"/>
      <xsl:apply-templates select="." mode="task.choices.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.choices.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.choices.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.choices.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.choices.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.content.in">
      <xsl:apply-templates select="." mode="task.choices.task.choice.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choices.task.choice.in">
      <xsl:apply-templates select="." mode="task.choice.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' task/choices '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.atts.in">
      <xsl:apply-templates select="." mode="task.choicetable.display.atts.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.relcolwidth.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.keycol.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.refcols.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.spectitle.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.display.atts.in">
      <xsl:apply-templates select="." mode="task.choicetable.scale.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.frame.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.expanse.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.scale.att.in">
      <xsl:apply-templates select="." mode="scale.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.frame.att.in">
      <xsl:apply-templates select="." mode="frame.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.expanse.att.in">
      <xsl:apply-templates select="." mode="expanse.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.univ.atts.in">
      <xsl:apply-templates select="." mode="task.choicetable.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.id.atts.in">
      <xsl:apply-templates select="." mode="task.choicetable.id.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.select.atts.in">
      <xsl:apply-templates select="." mode="task.choicetable.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.product.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.relcolwidth.att.in">
      <xsl:apply-templates select="." mode="relcolwidth.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.keycol.att.in">
      <xsl:apply-templates select="." mode="keycol.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.refcols.att.in">
      <xsl:apply-templates select="." mode="refcols.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.spectitle.att.in">
      <xsl:apply-templates select="." mode="spectitle.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.content.in">
      <xsl:apply-templates select="." mode="task.choicetable.task.chhead.in"/>
      <xsl:apply-templates select="." mode="task.choicetable.task.chrow.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.task.chhead.in">
      <xsl:apply-templates select="." mode="task.chhead.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' task/choicetable '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choicetable.task.chrow.in">
      <xsl:apply-templates select="." mode="task.chrow.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' task/choicetable '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.atts.in">
      <xsl:apply-templates select="." mode="task.choption.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.choption.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="task.choption.specentry.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.univ.atts.in">
      <xsl:apply-templates select="." mode="task.choption.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.choption.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.id.atts.in">
      <xsl:apply-templates select="." mode="task.choption.id.att.in"/>
      <xsl:apply-templates select="." mode="task.choption.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.select.atts.in">
      <xsl:apply-templates select="." mode="task.choption.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.choption.product.att.in"/>
      <xsl:apply-templates select="." mode="task.choption.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.choption.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.choption.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.choption.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.choption.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choption.content.in">
      <xsl:apply-templates select="." mode="tblcell.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.atts.in">
      <xsl:apply-templates select="." mode="task.choptionhd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.outputclass.att.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.specentry.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.univ.atts.in">
      <xsl:apply-templates select="." mode="task.choptionhd.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.id.atts.in">
      <xsl:apply-templates select="." mode="task.choptionhd.id.att.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.select.atts.in">
      <xsl:apply-templates select="." mode="task.choptionhd.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.product.att.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.choptionhd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.specentry.att.in">
      <xsl:apply-templates select="." mode="specentry.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.choptionhd.content.in">
      <xsl:apply-templates select="." mode="tblcell.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.atts.in">
      <xsl:apply-templates select="." mode="task.chrow.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.chrow.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.univ.atts.in">
      <xsl:apply-templates select="." mode="task.chrow.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.chrow.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.id.atts.in">
      <xsl:apply-templates select="." mode="task.chrow.id.att.in"/>
      <xsl:apply-templates select="." mode="task.chrow.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.select.atts.in">
      <xsl:apply-templates select="." mode="task.chrow.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.chrow.product.att.in"/>
      <xsl:apply-templates select="." mode="task.chrow.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.chrow.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.chrow.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.chrow.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.chrow.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.content.in">
      <xsl:apply-templates select="." mode="task.chrow.task.choption.in"/>
      <xsl:apply-templates select="." mode="task.chrow.task.chdesc.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.task.choption.in">
      <xsl:apply-templates select="." mode="task.choption.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' task/chrow '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.chrow.task.chdesc.in">
      <xsl:apply-templates select="." mode="task.chdesc.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' task/chrow '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.atts.in">
      <xsl:apply-templates select="." mode="task.cmd.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.cmd.keyref.att.in"/>
      <xsl:apply-templates select="." mode="task.cmd.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.univ.atts.in">
      <xsl:apply-templates select="." mode="task.cmd.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.cmd.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.id.atts.in">
      <xsl:apply-templates select="." mode="task.cmd.id.att.in"/>
      <xsl:apply-templates select="." mode="task.cmd.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.select.atts.in">
      <xsl:apply-templates select="." mode="task.cmd.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.cmd.product.att.in"/>
      <xsl:apply-templates select="." mode="task.cmd.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.cmd.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.cmd.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.cmd.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.cmd.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.keyref.att.in">
      <xsl:apply-templates select="." mode="keyref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.cmd.content.in">
      <xsl:apply-templates select="." mode="ph.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.context.atts.in">
      <xsl:apply-templates select="." mode="task.context.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.context.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.context.univ.atts.in">
      <xsl:apply-templates select="." mode="task.context.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.context.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.context.id.atts.in">
      <xsl:apply-templates select="." mode="task.context.id.att.in"/>
      <xsl:apply-templates select="." mode="task.context.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.context.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.select.atts.in">
      <xsl:apply-templates select="." mode="task.context.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.context.product.att.in"/>
      <xsl:apply-templates select="." mode="task.context.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.context.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.context.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.context.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.context.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.context.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.context.content.in">
      <xsl:apply-templates select="." mode="section.notitle.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.info.atts.in">
      <xsl:apply-templates select="." mode="task.info.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.info.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.info.univ.atts.in">
      <xsl:apply-templates select="." mode="task.info.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.info.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.info.id.atts.in">
      <xsl:apply-templates select="." mode="task.info.id.att.in"/>
      <xsl:apply-templates select="." mode="task.info.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.info.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.select.atts.in">
      <xsl:apply-templates select="." mode="task.info.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.info.product.att.in"/>
      <xsl:apply-templates select="." mode="task.info.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.info.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.info.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.info.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.info.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.info.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.info.content.in">
      <xsl:apply-templates select="." mode="itemgroup.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.atts.in">
      <xsl:apply-templates select="." mode="task.postreq.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.postreq.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.univ.atts.in">
      <xsl:apply-templates select="." mode="task.postreq.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.postreq.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.id.atts.in">
      <xsl:apply-templates select="." mode="task.postreq.id.att.in"/>
      <xsl:apply-templates select="." mode="task.postreq.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.select.atts.in">
      <xsl:apply-templates select="." mode="task.postreq.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.postreq.product.att.in"/>
      <xsl:apply-templates select="." mode="task.postreq.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.postreq.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.postreq.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.postreq.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.postreq.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.postreq.content.in">
      <xsl:apply-templates select="." mode="section.notitle.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.atts.in">
      <xsl:apply-templates select="." mode="task.prereq.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.prereq.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.univ.atts.in">
      <xsl:apply-templates select="." mode="task.prereq.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.prereq.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.id.atts.in">
      <xsl:apply-templates select="." mode="task.prereq.id.att.in"/>
      <xsl:apply-templates select="." mode="task.prereq.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.select.atts.in">
      <xsl:apply-templates select="." mode="task.prereq.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.prereq.product.att.in"/>
      <xsl:apply-templates select="." mode="task.prereq.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.prereq.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.prereq.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.prereq.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.prereq.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.prereq.content.in">
      <xsl:apply-templates select="." mode="section.notitle.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.result.atts.in">
      <xsl:apply-templates select="." mode="task.result.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.result.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.result.univ.atts.in">
      <xsl:apply-templates select="." mode="task.result.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.result.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.result.id.atts.in">
      <xsl:apply-templates select="." mode="task.result.id.att.in"/>
      <xsl:apply-templates select="." mode="task.result.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.result.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.select.atts.in">
      <xsl:apply-templates select="." mode="task.result.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.result.product.att.in"/>
      <xsl:apply-templates select="." mode="task.result.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.result.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.result.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.result.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.result.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.result.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.result.content.in">
      <xsl:apply-templates select="." mode="section.notitle.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.step.atts.in">
      <xsl:apply-templates select="." mode="task.step.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.step.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.step.univ.atts.in">
      <xsl:apply-templates select="." mode="task.step.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.step.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.step.product.att.in"/>
      <xsl:apply-templates select="." mode="task.step.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.step.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.step.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.step.status.att.in"/>
      <xsl:apply-templates select="." mode="task.step.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.step.id.atts.in">
      <xsl:apply-templates select="." mode="task.step.id.att.in"/>
      <xsl:apply-templates select="." mode="task.step.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.step.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.step.content.in">
      <xsl:apply-templates select="*" mode="task.step.child"/>
   </xsl:template>

   <xsl:template match="*" mode="task.step.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' task/step '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.atts.in">
      <xsl:apply-templates select="." mode="task.stepresult.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.stepresult.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.univ.atts.in">
      <xsl:apply-templates select="." mode="task.stepresult.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.stepresult.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.id.atts.in">
      <xsl:apply-templates select="." mode="task.stepresult.id.att.in"/>
      <xsl:apply-templates select="." mode="task.stepresult.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.select.atts.in">
      <xsl:apply-templates select="." mode="task.stepresult.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.stepresult.product.att.in"/>
      <xsl:apply-templates select="." mode="task.stepresult.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.stepresult.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.stepresult.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.stepresult.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.stepresult.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepresult.content.in">
      <xsl:apply-templates select="." mode="itemgroup.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.atts.in">
      <xsl:apply-templates select="." mode="task.steps-unordered.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.steps-unordered.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.univ.atts.in">
      <xsl:apply-templates select="." mode="task.steps-unordered.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.steps-unordered.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.id.atts.in">
      <xsl:apply-templates select="." mode="task.steps-unordered.id.att.in"/>
      <xsl:apply-templates select="." mode="task.steps-unordered.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.select.atts.in">
      <xsl:apply-templates select="." mode="task.steps-unordered.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.steps-unordered.product.att.in"/>
      <xsl:apply-templates select="." mode="task.steps-unordered.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.steps-unordered.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.steps-unordered.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.steps-unordered.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.steps-unordered.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.content.in">
      <xsl:apply-templates select="." mode="task.steps-unordered.task.step.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps-unordered.task.step.in">
      <xsl:apply-templates select="." mode="task.step.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' task/steps-unordered '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.atts.in">
      <xsl:apply-templates select="." mode="task.steps.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.steps.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.univ.atts.in">
      <xsl:apply-templates select="." mode="task.steps.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.steps.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.id.atts.in">
      <xsl:apply-templates select="." mode="task.steps.id.att.in"/>
      <xsl:apply-templates select="." mode="task.steps.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.select.atts.in">
      <xsl:apply-templates select="." mode="task.steps.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.steps.product.att.in"/>
      <xsl:apply-templates select="." mode="task.steps.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.steps.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.steps.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.steps.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.steps.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.content.in">
      <xsl:apply-templates select="." mode="task.steps.task.step.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.steps.task.step.in">
      <xsl:apply-templates select="." mode="task.step.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' task/steps '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.atts.in">
      <xsl:apply-templates select="." mode="task.stepxmp.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.stepxmp.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.univ.atts.in">
      <xsl:apply-templates select="." mode="task.stepxmp.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.stepxmp.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.id.atts.in">
      <xsl:apply-templates select="." mode="task.stepxmp.id.att.in"/>
      <xsl:apply-templates select="." mode="task.stepxmp.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.select.atts.in">
      <xsl:apply-templates select="." mode="task.stepxmp.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.stepxmp.product.att.in"/>
      <xsl:apply-templates select="." mode="task.stepxmp.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.stepxmp.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.stepxmp.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.stepxmp.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.stepxmp.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.stepxmp.content.in">
      <xsl:apply-templates select="." mode="itemgroup.cnt.text.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.atts.in">
      <xsl:apply-templates select="." mode="task.substep.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.substep.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.univ.atts.in">
      <xsl:apply-templates select="." mode="task.substep.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.substep.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.substep.product.att.in"/>
      <xsl:apply-templates select="." mode="task.substep.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.substep.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.substep.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.substep.status.att.in"/>
      <xsl:apply-templates select="." mode="task.substep.importance.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.id.atts.in">
      <xsl:apply-templates select="." mode="task.substep.id.att.in"/>
      <xsl:apply-templates select="." mode="task.substep.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.content.in">
      <xsl:apply-templates select="*" mode="task.substep.child"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substep.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' task/substep '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.atts.in">
      <xsl:apply-templates select="." mode="task.substeps.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.substeps.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.univ.atts.in">
      <xsl:apply-templates select="." mode="task.substeps.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.substeps.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.id.atts.in">
      <xsl:apply-templates select="." mode="task.substeps.id.att.in"/>
      <xsl:apply-templates select="." mode="task.substeps.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.select.atts.in">
      <xsl:apply-templates select="." mode="task.substeps.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.substeps.product.att.in"/>
      <xsl:apply-templates select="." mode="task.substeps.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.substeps.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.substeps.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.substeps.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.substeps.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.content.in">
      <xsl:apply-templates select="." mode="task.substeps.task.substep.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.substeps.task.substep.in">
      <xsl:apply-templates select="." mode="task.substep.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' task/substeps '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.atts.in">
      <xsl:attribute name="xml:lang">
         <xsl:text>en-us</xsl:text>
      </xsl:attribute>
      <xsl:apply-templates select="." mode="task.task.select.atts.in"/>
      <xsl:apply-templates select="." mode="task.task.id.att.in"/>
      <xsl:apply-templates select="." mode="task.task.conref.att.in"/>
      <xsl:apply-templates select="." mode="task.task.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.task.select.atts.in">
      <xsl:apply-templates select="." mode="task.task.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.task.product.att.in"/>
      <xsl:apply-templates select="." mode="task.task.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.task.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.task.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.task.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.task.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.task.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.content.in">
      <xsl:apply-templates select="." mode="task.task.topic.title.in"/>
      <xsl:apply-templates select="." mode="task.task.topic.titlealts.in"/>
      <xsl:apply-templates select="." mode="task.task.topic.shortdesc.in"/>
      <xsl:apply-templates select="." mode="task.task.topic.prolog.in"/>
      <xsl:apply-templates select="." mode="task.task.task.taskbody.in"/>
      <xsl:apply-templates select="." mode="task.task.topic.related-links.in"/>
      <xsl:apply-templates select="." mode="task.task.topic.topic.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.task.topic.title.in">
      <xsl:apply-templates select="." mode="topic.title.in">
         <xsl:with-param name="isRequired" select="'yes'"/>
         <xsl:with-param name="container" select="' task/task '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.topic.titlealts.in">
      <xsl:apply-templates select="." mode="topic.titlealts.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' task/task '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.topic.shortdesc.in">
      <xsl:apply-templates select="." mode="topic.shortdesc.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' task/task '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.topic.prolog.in">
      <xsl:apply-templates select="." mode="topic.prolog.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' task/task '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.task.taskbody.in">
      <xsl:apply-templates select="." mode="task.taskbody.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' task/task '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.topic.related-links.in">
      <xsl:apply-templates select="." mode="topic.related-links.in">
         <xsl:with-param name="isRequired" select="'no'"/>
         <xsl:with-param name="container" select="' task/task '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.task.topic.topic.in">
      <xsl:apply-templates select="." mode="topic.topic.in">
         <xsl:with-param name="container" select="' task/task '"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.taskbody.atts.in">
      <xsl:apply-templates select="." mode="task.taskbody.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.taskbody.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.taskbody.id.atts.in">
      <xsl:apply-templates select="." mode="task.taskbody.id.att.in"/>
      <xsl:apply-templates select="." mode="task.taskbody.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.taskbody.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.taskbody.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.taskbody.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.taskbody.content.in">
      <xsl:apply-templates select="*" mode="task.taskbody.child"/>
   </xsl:template>

   <xsl:template match="*" mode="task.taskbody.child">
      <xsl:param name="isRequired" select="'no'"/>
      <xsl:apply-templates select="." mode="child">
         <xsl:with-param name="container" select="' task/taskbody '"/>
         <xsl:with-param name="isRequired" select="$isRequired"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.atts.in">
      <xsl:apply-templates select="." mode="task.tutorialinfo.univ.atts.in"/>
      <xsl:apply-templates select="." mode="task.tutorialinfo.outputclass.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.univ.atts.in">
      <xsl:apply-templates select="." mode="task.tutorialinfo.id.atts.in"/>
      <xsl:apply-templates select="." mode="task.tutorialinfo.select.atts.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.id.atts.in">
      <xsl:apply-templates select="." mode="task.tutorialinfo.id.att.in"/>
      <xsl:apply-templates select="." mode="task.tutorialinfo.conref.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.id.att.in">
      <xsl:apply-templates select="." mode="id.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.conref.att.in">
      <xsl:apply-templates select="." mode="conref.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.select.atts.in">
      <xsl:apply-templates select="." mode="task.tutorialinfo.platform.att.in"/>
      <xsl:apply-templates select="." mode="task.tutorialinfo.product.att.in"/>
      <xsl:apply-templates select="." mode="task.tutorialinfo.audience.att.in"/>
      <xsl:apply-templates select="." mode="task.tutorialinfo.otherprops.att.in"/>
      <xsl:apply-templates select="." mode="task.tutorialinfo.rev.att.in"/>
      <xsl:apply-templates select="." mode="task.tutorialinfo.importance.att.in"/>
      <xsl:apply-templates select="." mode="task.tutorialinfo.status.att.in"/>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.platform.att.in">
      <xsl:apply-templates select="." mode="platform.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.product.att.in">
      <xsl:apply-templates select="." mode="product.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.audience.att.in">
      <xsl:apply-templates select="." mode="audience.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.otherprops.att.in">
      <xsl:apply-templates select="." mode="otherprops.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.rev.att.in">
      <xsl:apply-templates select="." mode="rev.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.importance.att.in">
      <xsl:apply-templates select="." mode="importance.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.status.att.in">
      <xsl:apply-templates select="." mode="status.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.outputclass.att.in">
      <xsl:apply-templates select="." mode="outputclass.att.in">
         <xsl:with-param name="isRequired" select="'no'"/>
      </xsl:apply-templates>
   </xsl:template>

   <xsl:template match="*" mode="task.tutorialinfo.content.in">
      <xsl:apply-templates select="." mode="itemgroup.cnt.text.in"/>
   </xsl:template>



</xsl:stylesheet>
