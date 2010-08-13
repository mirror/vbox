#include "VariantHelper.h"

std::string VariantTypeAsString(const VARIANT& v)
{
    switch (v.vt)
    {
    case    VT_UI1:
        return "VT_UI1";

    case    VT_I2:
        return "VT_I2";

    case    VT_I4:
        return "VT_I4";

    case    VT_R4:
        return "VT_R4";

    case    VT_R8:
        return "VT_R8";

    case    VT_BOOL:
        return "VT_BOOL";

    case    VT_ERROR:
        return "VT_ERROR";

    case    VT_CY:
        return "VT_CY";

    case    VT_DATE:
        return "VT_DATE";

    case    VT_BSTR:
        return "VT_BSTR";

    case    VT_BYREF|VT_DECIMAL:
        return "VT_BYREF|VT_DECIMAL";

    case    VT_UNKNOWN:
        return "VT_UNKNOWN";

    case    VT_DISPATCH:
        return "VT_DISPATCH";

        //case    VT_ARRAY|*:
        //return "VT_ARRAY|*";

    case    VT_BYREF|VT_UI1:
        return "VT_BYREF|VT_UI1";

    case    VT_BYREF|VT_I2:
        return "VT_BYREF|VT_I2";

    case    VT_BYREF|VT_I4:
        return "VT_BYREF|VT_I4";

    case    VT_BYREF|VT_R4:
        return "VT_BYREF|VT_R4";

    case    VT_BYREF|VT_R8:
        return "VT_BYREF|VT_R8";

    case    VT_BYREF|VT_BOOL:
        return "VT_BYREF|VT_BOOL";

    case    VT_BYREF|VT_ERROR:
        return "VT_BYREF|VT_ERROR";

    case    VT_BYREF|VT_CY:
        return "VT_BYREF|VT_CY";

    case    VT_BYREF|VT_DATE:
        return "VT_BYREF|VT_DATE";

    case    VT_BYREF|VT_BSTR:
        return "VT_BYREF|VT_BSTR";

    case    VT_BYREF|VT_UNKNOWN:
        return "VT_BYREF|VT_UNKNOWN";

    case    VT_BYREF|VT_DISPATCH:
        return "VT_BYREF|VT_DISPATCH";

        //case    VT_ARRAY|*:
        //return "VT_ARRAY|*";

    case    VT_BYREF|VT_VARIANT:
        return "VT_BYREF|VT_VARIANT";

        //Generic case ByRef:
        //return "ByRef";

    case    VT_I1:
        return "VT_I1";

    case    VT_UI2:
        return "VT_UI2";

    case    VT_UI4:
        return "VT_UI4";

    case    VT_INT:
        return "VT_INT";

    case    VT_UINT:
        return "VT_UINT";

    case    VT_BYREF|VT_I1:
        return "VT_BYREF|VT_I1";

    case    VT_BYREF|VT_UI2:
        return "VT_BYREF|VT_UI2";

    case    VT_BYREF|VT_UI4:
        return "VT_BYREF|VT_UI4";

    case    VT_BYREF|VT_INT:
        return "VT_BYREF|VT_INT";

    case    VT_BYREF|VT_UINT:
        return "VT_BYREF|VT_UINT";

    default:
        return "Unknown type";

    }
}
