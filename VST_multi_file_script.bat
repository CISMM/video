@echo off

rem ==========================================================
rem Edit this to pick which version of Spot Tracker to run
cd "C:\NSRG\bin\video_spot_tracker_v05.09"

rem ==========================================================
rem Edit these locations to start elsewhere
set x=357
set y=43
set r=30

echo Starting a tracker at %x%, %y% with radius %r%
echo Edit the batch file to pick a different place

rem ==========================================================
rem Main tracking loop starts here, and goes through each
rem file passed in on the command line.
:AGAIN

rem The first thing we need to do is remove any quotes from the
rem long file name; the quotes are actually put into the name by
rem Windows, and Video Spot Tracker doesn't like them, of course.
rem There is a magic tilde to do this for us in Windows 2000/XP.
rem Note that we put the file name back in quotes below, but this
rem does not actually put the quotes into the name; it rather makes
rem sure that it is only one entry on the command line.
set var=%~1

echo Tracking %var%

set PATH=C:\NSRG\external\pc_win32\bin\ImageMagick-5.5.7-Q16;C:\NSRG\external\pc_win32\bin\
set TCL_LIBRARY=C:/nsrg/external/pc_win32/lib/tcl8.3/
set TK_LIBRARY=C:/nsrg/external/pc_win32/lib/tk8.3/
.\video_spot_tracker.exe -precision 0.01 -sample_spacing 0.1 -tracker %x% %y% %r% -outfile "%var%" "%var%"

rem ==========================================================
rem Now shift the command-line arguments left by one, so that if
rem we have another file name, we'll operate on it next.
shift

rem The reason for the "x" is not entirely clear to me in the example
rem I copied from.  I assume it is to avoid having an empty string on
rem each side of the ==.  It failed to use: if not "%1" == ""
if not %1x == x goto AGAIN

rem ==========================================================
echo Done
pause
