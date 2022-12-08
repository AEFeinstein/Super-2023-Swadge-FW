#!/bin/sh
# Only delete esp-idf/ in ~/esp/
rm -rf ~/esp/esp-idf/

# Delete ~/.espressif/ and everything in it
rm -rf ~/.espressif/

# Re-run the esp-idf setup
./setup-esp-idf.sh
