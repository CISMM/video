@ECHO OFF
set EDTPATH=C:\EDT\PDV
%EDTPATH%\initcam -f %EDTPATH%\camera_config\ptm6710cl_3dfm.cfg
echo.
cd Release
.\GLUItake.exe
if not errorlevel 0 pause
