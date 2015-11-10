

#include "bs3kit.h"


BS3_DECL(void) BS3_CMN_NM(Bs3PrintStrSpacesColonSpace)(const char BS3_FAR *pszString, unsigned cch)
{
    static volatile char s_szStuff[] = "stuff";
    static volatile const char s_szStuff2[] = "stuff";
    //size_t cchString;

    //Bs3PrintStr(pszString);

    s_szStuff[0] = s_szStuff2[0];
}

