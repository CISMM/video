set TCL_LIBRARY=C:/nsrg/external/pc_win32/lib/tcl8.3/
set TK_LIBRARY=C:/nsrg/external/pc_win32/lib/tk8.3/
.\stereo_spin.exe %1 %2 %3 %4 %5
if not errorlevel 0 pause
