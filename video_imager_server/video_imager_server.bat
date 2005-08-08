set PATH=C:\NSRG\external\pc_win32\bin\ImageMagick-5.5.7-Q16;C:\NSRG\external\pc_win32\bin\
set TCL_LIBRARY=C:/nsrg/external/pc_win32/lib/tcl8.3/set TK_LIBRARY=C:/nsrg/external/pc_win32/lib/tk8.3/
.\video_imager_server.exe %1
if not errorlevel 0 pause