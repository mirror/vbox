<!-- Replaces refsect2 elements with corresponding refsect1 but also replaces ndash etc entities. Thus, currently not used -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
    <xsl:output encoding="UTF-8" method="xml" omit-xml-declaration="no" indent="yes" />
    <xsl:output doctype-system="http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd"
         doctype-public="-//OASIS//DTD DocBook XML V4.5//EN"/>

    <xsl:template match="refsect2">
        <xsl:element name="refsect1">
            <xsl:for-each select="@*">
                <xsl:attribute name="{name()}"><xsl:value-of select="." /></xsl:attribute>
            </xsl:for-each>
            <xsl:copy-of select="node()"/>
        </xsl:element>
    </xsl:template>
    <xsl:template match="node()|@*">
        <xsl:copy>
            <xsl:apply-templates select="node()|@*"/>
        </xsl:copy>
    </xsl:template>
</xsl:stylesheet>