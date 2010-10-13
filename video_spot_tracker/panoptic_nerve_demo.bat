REM This batch file takes three arguments:
REM   1: The name of the file in which to autolocate fluorescence beads
REM   2: The name of the file in which to track brightfield beads at those locations
REM   3: The output file name base for the resulting tracked CSV and VRPN files

set PATH=C:\NSRG\external\pc_win32\bin\ImageMagick-5.5.7-Q16;C:\NSRG\external\pc_win32\bin\
set TCL_LIBRARY=C:/nsrg/external/pc_win32/lib/tcl8.3/
set TK_LIBRARY=C:/nsrg/external/pc_win32/lib/tk8.3/

REM Delete any temporary output file from before
del C:\lottanerve_tmp.csv
del C:\lottanerve_tmp.vrpn

REM Autofind the spots in the first file and produce a temporary output file with the appropriate parameters.
REM Need to create an initial bogus tracker so it will optimize.  It will be near the border, so deleted.
C:
cd C:/nsrg/external/pc_win32/lib/
.\video_spot_tracker.exe -radius 10 -dead_zone_around_border 5 -dead_zone_around_trackers 3 -maintain_this_many_beads 10 -candidate_spot_threshold 0.8 -sliding_window_radius 10 -lost_behavior 1 -intensity_lost_sensitivity 1.5 -tracker 0 0 10 -outfile C:\lottanerve_tmp "%1"

if not errorlevel 0 pause

REM Continue tracking in the second file with the bead positions and radii from the end of the
REM first file.  Save the results to the output files specified in the third command-line
REM argument.

.\video_spot_tracker.exe -lost_behavior 1 -dead_zone_around_border 5 -dead_zone_around_trackers 3 -continue_from C:\lottanerve_tmp.csv -outfile "%3" "%2"

if not errorlevel 0 pause

