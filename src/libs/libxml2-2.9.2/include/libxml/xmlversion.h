/*
 * Include a proper pre-generated xmlversion file.
 * Note that configuration scripts will overwrite this file using a
 * xmlversion.h.in template.
 */

#ifdef RT_OS_WINDOWS
# include <libxml/xmlversion-win.h>
#else
# include <libxml/xmlversion-default.h>
#endif
