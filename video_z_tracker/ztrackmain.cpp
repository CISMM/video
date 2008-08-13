
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
	char* video_name = "";

	if (argc > 1) // get stage control specification
	{
	    stage_name = argv[1];
		size_t stage_name_len = strlen(stage_name);

		if (strchr(stage_name, '@') != NULL)
		{
			// for now we can just assume it's a valid server address
		}
		else if (strncmp(stage_name, "NULL", 4) == 0 || stage_name_len == 0)
		{
			stage_name = "";
		}
		else
		{
			printf("Local stage server not yet supported.\n");
			printf("Please specify the focus server, e.g. \"Focus@mercury-cs.cs.unc.edu\"\n");
			return false;
		}

		if (argc > 2) // get video server specification
		{
			video_name = argv[2];
			if (strchr(video_name, '@') != NULL)
			{
				// for now we can just assume it's a valid server address
			}
			else
			{
				printf("Likely a mal-formed video server specified: %s\n", video_name);
			}

		}
	}

    zTracker* frmZTrack = new zTracker(0, -1, wxT("Video Z Tracker, prototype version."),wxDefaultPosition, appSize,
		wxDEFAULT_FRAME_STYLE, stage_name, video_name);
    SetTopWindow(frmZTrack);
	frmZTrack->Show();

    return true;
}

IMPLEMENT_APP_CONSOLE(zTrackApp)