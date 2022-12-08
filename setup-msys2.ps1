# Download installer
Invoke-WebRequest -Uri https://github.com/msys2/msys2-installer/releases/download/2022-01-28/msys2-base-x86_64-20220128.sfx.exe -Outfile msys2.exe

# Extract to C:\msys64
.\msys2.exe -y -oC:\

# Delete the installer
Remove-Item .\msys2.exe

# Run for the first time
C:\msys64\usr\bin\bash -lc ' '

# Update MSYS2, first a core update then a normal update
C:\msys64\usr\bin\bash -lc 'pacman --noconfirm -Syuu'
C:\msys64\usr\bin\bash -lc 'pacman --noconfirm -Syuu'

# Install packages
C:\msys64\usr\bin\bash -lc 'pacman --noconfirm -S base-devel mingw-w64-x86_64-toolchain git make zip mingw-w64-x86_64-python-pip python-pip'
