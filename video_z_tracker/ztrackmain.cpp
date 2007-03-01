
#include <wx/wx.h>
#include <wx/image.h>
#include "ztracker.h"


class zTrackApp: public wxApp {
public:
    bool OnInit();
};

bool zTrackApp::OnInit()
{
	wxSize appSize(640, 480);
//    wxInitAllImageHandlers();
    zTracker* frmZTrack = new zTracker(0, -1, wxT("Video Z Tracker, prototype version."),wxDefaultPosition, appSize);
    SetTopWindow(frmZTrack);
    frmZTrack->Show();
    return true;
}

IMPLEMENT_APP_CONSOLE(zTrackApp)