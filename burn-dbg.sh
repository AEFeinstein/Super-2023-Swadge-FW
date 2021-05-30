#!/bin/bash

# Try to kill putty
taskkill.exe /IM "putty.exe" /F

# exit when any command fails, then build
set -e
idf.py -p $ESP32_C3_PORT -b 2000000 flash

# Start putty
cmd.exe /C "START /B putty.exe -serial $ESP32_C3_PORT_WIN -sercfg 115200"