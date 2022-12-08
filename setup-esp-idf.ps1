# Make an esp folder only if it does not yet exist, and move into it regardless
$Filename="~/esp"
if (-not(test-path $Filename)){mkdir $FileName}
cd ~/esp

# Clone the IDF and move into it
git clone -b v4.4.3 --recursive https://github.com/espressif/esp-idf.git esp-idf
cd ~/esp/esp-idf

# Initialize submodules
git submodule update --init --recursive

# Install tools
./install.ps1

# Export paths and variables
./export.ps1
