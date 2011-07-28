@echo off
set EDTPATH=C:\EDT\PDV

%EDTPATH%\initcam -f %EDTPATH%\camera_config\ptm6710c1_3dfm.cfg

if not errorlevel 0 pause