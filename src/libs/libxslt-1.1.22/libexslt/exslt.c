#define IN_LIBEXSLT
#include "libexslt/libexslt.h"

#include <libxml/xmlversion.h>

#if defined(VBOX)
#include "vboxconfig.h"
#else
#include "config.h"
#endif

#include <libxslt/xsltconfig.h>
#include <libxslt/extensions.h>

#include "exsltconfig.h"
#include "exslt.h"

const char *exsltLibraryVersion = LIBEXSLT_VERSION_STRING 
				LIBEXSLT_VERSION_EXTRA;
const int exsltLibexsltVersion = LIBEXSLT_VERSION;
const int exsltLibxsltVersion = LIBXSLT_VERSION;
const int exsltLibxmlVersion = LIBXML_VERSION;

/**
 * exsltRegisterAll:
 *
 * Registers all available EXSLT extensions
 */
void
exsltRegisterAll (void) {
    exsltCommonRegister();
#ifdef EXSLT_CRYPTO_ENABLED
    exsltCryptoRegister();
#endif
    exsltMathRegister();
    exsltSetsRegister();
    exsltFuncRegister();
    exsltStrRegister();
    exsltDateRegister();
    exsltSaxonRegister();
    exsltDynRegister();
}

