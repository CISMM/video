# Microsoft Developer Studio Project File - Name="edt_library" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=edt_library - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "edt_library.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "edt_library.mak" CFG="edt_library - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "edt_library - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "edt_library - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "edt_library - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "edt_library___Win32_Release"
# PROP BASE Intermediate_Dir "edt_library___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "C:\EDT\pdv" /I "..\vrpn" /I "..\external\pc_win32\include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "edt_library - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "edt_library___Win32_Debug"
# PROP BASE Intermediate_Dir "edt_library___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "C:\EDT\pdv" /I "..\vrpn" /I "..\external\pc_win32\include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "edt_library - Win32 Release"
# Name "edt_library - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=C:\EDT\pdv\edt_bitload.c
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\edt_error.c
# End Source File
# Begin Source File

SOURCE=.\edt_server.cpp
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\edt_vco.c
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\edt_xilinx.c
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\libddcam.c
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\libdvu.c
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\libedt.c
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\libpdv.c
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\pdv_interlace.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\edt_server.h
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\libdvu.h
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\libedt.h
# End Source File
# Begin Source File

SOURCE=C:\EDT\pdv\libpdv.h
# End Source File
# End Group
# End Target
# End Project
