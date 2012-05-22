set PATH=C:\NSRG\external\pc_win32\bin\ImageMagick-5.5.7-Q16;C:\NSRG\external\pc_win32\bin\;C:\Program Files (x86)\CISMM\external\bin
set TCL_LIBRARY=C:/tmp/vs2008/video-build/Release/tcl8.3/
set TK_LIBRARY=C:/tmp/vs2008/video-build/Release/tk8.3/

.\CUDA_video_filter.exe %1 %2 %3 %4 %5 %6 %7
if not errorlevel 0 pause
