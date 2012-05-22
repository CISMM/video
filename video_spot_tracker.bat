set PATH=C:\NSRG\external\pc_win32\bin\ImageMagick-5.5.7-Q16;C:\NSRG\external\pc_win32\bin\
set TCL_LIBRARY=C:/nsrg/external/pc_win32/lib/tcl8.3/
set TK_LIBRARY=C:/nsrg/external/pc_win32/lib/tk8.3/

REM By default, call the non-CUDA version of spot tracker.
set CMD=".\video_spot_tracker.exe"

REM If the first command-line argument is CUDA, then set the command name to run the
REM CUDA version and shift the command-line arguments to remove it from the ones sent
REM to the command we call.
IF NOT [%1]==[CUDA] GOTO NOCUDA
set CMD=".\video_spot_tracker_cuda.exe"
SHIFT
:NOCUDA

%CMD% %1 %2 %3 %4 %5 %6 %7
if not errorlevel 0 pause
