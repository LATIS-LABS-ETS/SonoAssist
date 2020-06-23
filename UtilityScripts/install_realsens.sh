#!/bin/bash
# Installation script for intel realsens sdk-2
# This is a scripted version of the steps foud in the following indications : https://www.intelrealsense.com/sdk-2/
#
# Requirements
#   Needs internet access
#   Needs root privileges to properly execute
# -----

# adding Intel server to the list of repositories :
echo -e "\nAddding Intel server to the list of repositories ... \n"
echo 'deb http://realsense-hw-public.s3.amazonaws.com/Debian/apt-repo xenial main' | sudo tee /etc/apt/sources.list.d/realsense-public.list
sudo apt-key adv --keyserver keys.gnupg.net --recv-key 6F3EFCDE
sudo apt-get update

# installing required packages
echo -e "\nInstalling required packages ... \n"
sudo apt-get -y install librealsense2-dkms
sudo apt-get -y install librealsense2-utils
sudo apt-get -y install librealsense2-dev
sudo apt-get -y install librealsense2-dbgclea