#!/bin/bash
# Installation script for freenect
# The freenect packaged for Ubuntu is very old , so and installation from source is better
# This script pulls the "maintained" code from the github repo directly
#
# Freenect enables :
#   Acces to the video flux
#   Control of the LEDs and the tilt motor on the Xbox Knect
#
# Requirements
#   Needs internet access
#   Needs root privileges to properly execute
# -----

# installing required packages
echo -e "\nInstalling library dependencies ... \n"
apt-get -y install git cmake 
apt-get -y install build-essential libusb-1.0-0-dev
apt-get -y install freeglut3-dev libxmu-dev libxi-dev

# fetching the code repositories
echo -e "\nFetching the library repo ... \n"
git clone https://github.com/OpenKinect/libfreenect
cd libfreenect

# building and installing the library
echo -e "\Building and installing the library repo ... \n"
mkdir build && cd build
cmake ..
make install

# fixing the include problem in the libfreenect.hpp file
config_file="/usr/local/include/libfreenect/libfreenect.hpp"
sed -i 's/#include <libusb.h>/#include <libusb-1.0\/libusb.h>/g' $config_file

# cleaning up after the installation
echo -e "\Cleaning up ... \n"
cd ../..
rm -r libfreenect

echo -e "\Installation complete  ... \n"