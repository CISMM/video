set PATH=C:\NSRG\external\pc_win32\bin\ImageMagick-5.5.7-Q16;C:\NSRG\external\pc_win32\bin\
set TCL_LIBRARY=C:/nsrg/external/pc_win32/lib/tcl8.3/
set TK_LIBRARY=C:/nsrg/external/pc_win32/lib/tk8.3/
.\average_videos.exe -max %1 %2 %3 %4 %5 %6 %7 %8 %9 %10 %11 %12 %13 %14 %15
if not errorlevel 0 pause
