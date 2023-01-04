#!/bin/sh
# Only delete esp-idf/ in ~/esp/
rm -rf ~/esp/esp-idf/

# Delete ~/.espressif/ and everything in it
rm -rf ~/.espressif/

# Re-run the esp-idf setup
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd SCRIPT_DIR
./setup-esp-idf.sh
