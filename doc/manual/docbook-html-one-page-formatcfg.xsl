<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!-- Single html file template -->
<xsl:import href="html/docbook.xsl"/>

<xsl:import href="common-formatcfg.xsl"/>
<xsl:import href="common-html-formatcfg.xsl"/>

<!-- Adjust some params -->
<xsl:param name="draft.mode" select="'no'"/>
<xsl:param name="suppress.navigation" select="1"></xsl:param>
<xsl:param name="header.rule" select="0"></xsl:param>
<xsl:param name="abstract.notitle.enabled" select="0"></xsl:param>
<xsl:param name="footer.rule" select="0"></xsl:param>
<xsl:param name="css.decoration" select="1"></xsl:param>
<xsl:param name="html.cleanup" select="1"></xsl:param>
<xsl:param name="css.decoration" select="1"></xsl:param>

</xsl:stylesheet>
