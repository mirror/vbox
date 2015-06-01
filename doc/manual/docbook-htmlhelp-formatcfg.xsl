<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:import href="$DOCBOOKPATH/htmlhelp/htmlhelp.xsl"/>
<xsl:import href="$CFGPATH/common-formatcfg.xsl"/>

<xsl:include href="$TARGETPATH/titlepage-htmlhelp.xsl"/>

<!-- for some reason, the default docbook stuff doesn't wrap simple <arg> elements
     into HTML <code>, so with a default CSS a cmdsynopsis ends up with a mix of
     monospace and proportional fonts.  Elsewhere we hack that in the CSS, here
     that turned out to be harded, so we just wrap things in <code>, risking
     nested <code> elements, but who cares as long as it works... -->
<xsl:template match="group|arg">
    <xsl:choose>
        <xsl:when test="name(..) = 'arg' or name(..) = 'group'">
            <xsl:apply-imports/>
        </xsl:when>
        <xsl:otherwise>
            <code><xsl:apply-imports/></code>
        </xsl:otherwise>
    </xsl:choose>
</xsl:template>

</xsl:stylesheet>

