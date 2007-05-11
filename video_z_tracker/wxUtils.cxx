#include "wxUtils.h"

const std::string wx2std(const wxString& string)
{
    return std::string((const char*) string.mb_str(wxConvUTF8));
}

const wxString std2wx(const std::string& string)
{
    return wxString(string.c_str(), wxConvUTF8);
}
