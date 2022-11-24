#!/bin/sh
# Make an esp folder and move into it
mkdir ~/esp
cd ~/esp

# Clone the IDF and move into it
git clone -b v4.4.3 --recursive https://github.com/espressif/esp-idf.git esp-idf
cd ~/esp/esp-idf

# Initialize submodules
git submodule update --init --recursive

# Install tools
./install.sh

# Export paths and variables
./export.sh
