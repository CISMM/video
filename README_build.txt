The Video project uses CMake to construct its build files.  Version 3.0 or higher will need to
be installed along with a platform-specific compiler chain.

To get an application that is able to track beads from the command line, the GLUT package
must be installed:
  sudo apt install freeglut3-dev
	
If you want to be able to read standard image file formats, the ImageMagick package
should be installed before doing the minimal build:
  sudo apt install libmagickcore-6.q16-dev

For a minimal build of the spot-tracker library and some applications, use the minimal_build.sh
script.  It will install VRPN in /usr/local/ and when will build the video targets in
~/deleteme/build/video.  It has the side effect of re-checkout-out the video project in
~/deleteme, so it is not an optimal build script for production use.
	
For the stack collector and video spot tracker programs to have all expected CISMM cameras,
you must also install the NSRG Runtime Environment, which is a large download that includes
all of the DLLs and other files needed to run applications from the NSRG.
It installs itself in C:\NSRG\external\pc_win32.