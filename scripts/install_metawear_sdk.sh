#!/bin/bash
# Installation script for the MetaWearSDK (used by the accelerometer)
# Assumes that these requirements are respected : https://mbientlab.com/tutorials/Linux.html#
#
# Requirements
#   Needs internet access
#   Needs root privileges to properly execute
# -----

# installing required packages
echo -e "\nInstalling required packages ... \n"
apt-get -y install git

# fetching the SDK source code
echo -e "\nFetching the SDK source code ... \n"
git clone https://github.com/mbientlab/MetaWear-SDK-Cpp

# building the library from source and installing
echo -e "\nBuilding the source and installing ... \n"
cd MetaWear-SDK-Cpp
make install
ldconfig

# Moving the header files to "usr/local/include/metawear"
echo -e "\nMoving the header files ... \n"
mkdir metawear/
cp -a ./src/metawear/. ./metawear/ && cd metawear
find -type f -name "*.cpp" -exec rm {} \;
cd .. && mv metawear /usr/local/include/

# cleaning up
echo -e "\Cleaning up ... \n"
cd ..
rm -r MetaWear-SDK-Cpp/