$ErrorActionPreference = "Stop"

# Try to kill putty
taskkill.exe /IM "putty.exe" /F

# exit when any command fails, then build
idf.py -p COM8 -b 2000000 build flash
cmd.exe /C "START /B putty.exe -serial COM8 -sercfg 115200"

idf.py -p COM3 -b 2000000 flash
cmd.exe /C "START /B putty.exe -serial COM3 -sercfg 115200"
