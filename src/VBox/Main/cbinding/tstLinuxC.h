#ifndef __tstLinuxC_h__
#define __tstLinuxC_h__

#ifdef __cplusplus
# include "VirtualBox_XPCOM.h"
#else
# include "VirtualBox_CXPCOM.h"
#endif

#ifdef IN_VBOXXPCOMC
# define VBOXXPCOMC_DECL(type)  PR_EXPORT(type)
#else
# define VBOXXPCOMC_DECL(type)  PR_IMPORT(type)
#endif

#ifdef __cplusplus
extern "C" {
#endif

VBOXXPCOMC_DECL(void) VBoxComInitialize(IVirtualBox **virtualBox, ISession **session);
VBOXXPCOMC_DECL(void) VBoxComUninitialize(void);
VBOXXPCOMC_DECL(void) VBoxComUnallocStr(PRUnichar *str_dealloc);
VBOXXPCOMC_DECL(void) VBoxComUnallocIID(nsIID *iid);
VBOXXPCOMC_DECL(const char *) VBoxConvertPRUnichartoAscii(PRUnichar *src);
VBOXXPCOMC_DECL(const PRUnichar *) VBoxConvertAsciitoPRUnichar(char *src);
VBOXXPCOMC_DECL(const PRUnichar *) VBoxConvertUTF8toPRUnichar(char *src);
VBOXXPCOMC_DECL(const char *) VBoxConvertPRUnichartoUTF8(PRUnichar *src);
VBOXXPCOMC_DECL(int)  VBoxStrToUtf16(const char *pszString, PRUnichar **ppwszString);
VBOXXPCOMC_DECL(int)  VBoxUtf16ToUtf8(const PRUnichar *pszString, char **ppwszString);
VBOXXPCOMC_DECL(void) VBoxUtf16Free(PRUnichar *pwszString);
VBOXXPCOMC_DECL(void) VBoxStrFree(char *pszString);

#ifdef __cplusplus
}
#endif

#endif /* __tstLinuxC_h__ */
