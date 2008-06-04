
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

	//char* stage_name = "Focus@aurum-cs.cs.unc.edu";
	char* stage_name = "";

	if (argc > 1) // get stage control specification
	{
	    stage_name = argv[1];
		if (strchr(stage_name, '@') != NULL)
		{
			// for now we can just assume it's a valid server address
		}
		else
		{
			printf("Local stage server not yet supported.\n");
			printf("Please specify the focus server, e.g. \"Focus@mercury-cs.cs.unc.edu\"\n");
			return false;
		}
	}

    zTracker* frmZTrack = new zTracker(0, -1, wxT("Video Z Tracker, prototype version."),wxDefaultPosition, appSize,
		wxDEFAULT_FRAME_STYLE, stage_name);
    SetTopWindow(frmZTrack);
	frmZTrack->Show();

    return true;
}

IMPLEMENT_APP_CONSOLE(zTrackApp)