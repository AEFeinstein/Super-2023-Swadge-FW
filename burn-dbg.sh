#!/bin/bash

taskkill.exe /IM "putty.exe" /F
idf.py -p $ESP32_C3_PORT -b 2000000 flash
cmd.exe /C "START /B putty.exe -serial $ESP32_C3_PORT_WIN -sercfg 115200"