#!/bin/bash
#########################################
# Example script to build a minimal video_spot_tracker, including the
# libraries needed and some sample applications.  This pulls in and
# builds the required VRPN library, which is installed in /usr/local.
#  It then builds the video project using whatever cameras it can find,
# which may only include the vrpn_Imager.
#  It places the build outputs in ~/deleteme/build/video.

cd ~
mkdir -p deleteme
cd deleteme
git clone https://github.com/vrpn/vrpn.git
git clone https://github.com/CISMM/video.git
mkdir -p build
cd build
mkdir -p vrpn
cd vrpn
cmake ../../vrpn -DVRPN_USE_WIIUSE:BOOL=FALSE -DVRPN_BUILD_CLIENTS:BOOL=FALSE -DVRPN_BUILD_SERVERS:BOOL=FALSE -DBUILD_TESTING:BOOL=FALSE
make
sudo make install
cd ..
mkdir -p video
cd video
cmake ../../video
make

