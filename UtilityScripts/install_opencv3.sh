#!/bin/bash
# Installation script for opencv (3.3.1) (build from source)
# This is a scripted version of the steps foud in the following tutorial : https://www.learnopencv.com/install-opencv3-on-ubuntu/
#
# Requirements
#   Needs internet access
#   Needs root privileges to properly execute
# -----

# updating packages
echo -e "\nRunning OS updates ... \n"
apt-get -y update
apt-get -y upgrade

# installing required libraries
echo -e "\nInstalling required packages ... \n"
apt-get -y remove x264 libx264-dev
apt-get -y install build-essential checkinstall cmake pkg-config yasm
apt-get -y install git gfortran
apt-get -y install libjpeg8-dev libjasper-dev libpng12-dev
apt-get -y install libtiff5-dev
apt-get -y install libavcodec-dev libavformat-dev libswscale-dev libdc1394-22-dev
apt-get -y install libxine2-dev libv4l-dev
apt-get -y install libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev
apt-get -y install qt5-default libgtk2.0-dev libtbb-dev
apt-get -y install libatlas-base-dev
apt-get -y install libfaac-dev libmp3lame-dev libtheora-dev
apt-get -y install libvorbis-dev libxvidcore-dev
apt-get -y install libopencore-amrnb-dev libopencore-amrwb-dev
apt-get -y install x264 v4l-utils

# installing ptional dependencies
echo -e "\nInstalling optional modules ... \n"
apt-get -y install libprotobuf-dev protobuf-compiler
apt-get -y install libgoogle-glog-dev libgflags-dev
apt-get -y install libgphoto2-dev libeigen3-dev libhdf5-dev doxygen

# fetching opencv from the official github
echo -e "\nFetching the official opencv sources ... \n"
git clone https://github.com/opencv/opencv.git
cd opencv
git checkout 3.4.10
cd ..

# fetching opencv contrib from the official github
echo -e "\nFetching the official opencv contrib sources ... \n"
git clone https://github.com/opencv/opencv_contrib.git
cd opencv_contrib
git checkout 3.4.10
cd ..

# creating a build directory in the opencv repo
cd opencv
mkdir build && cd build

# generating a make file
echo -e "\nGenerating the make file ... \n"
cmake -D CMAKE_BUILD_TYPE=RELEASE \
-D CMAKE_INSTALL_PREFIX=/usr/local \
-D INSTALL_C_EXAMPLES=ON \
-D INSTALL_PYTHON_EXAMPLES=ON \
-D WITH_TBB=ON \
-D WITH_V4L=ON \
-D WITH_QT=ON \
-D WITH_OPENGL=ON \
-D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules \
-D BUILD_EXAMPLES=ON ..

# compiling the source and installing
echo -e "\nCompiling the sources and installing ... \n"
make "-j$(nproc)"
make install
sh -c 'echo "/usr/local/lib" >> /etc/ld.so.conf.d/opencv.conf'
ldconfig

# cleaning up
echo -e "\Cleaning up ... \n"
cd ../..
rm -r opencv
rm -r opencv_contrib