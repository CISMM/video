#pragma once

#include <string>

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

/**
 * Converts a wxString to a std::string.
 */
const std::string wx2std(const wxString& string);

/**
 * Converts a std::string to a wxString.
 */
const wxString std2wx(const std::string& string);
