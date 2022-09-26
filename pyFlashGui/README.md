# Compatibility

This Python script should be compatible with Python 2 & 3. I have tested it on Windows with Python 3.

# Summary

This Python script polls for USB serial ports which match the VID & PID of an ESP32-S2. When one is found, it will attempt to flash it using `esptool`.

The UI will be green when a Swadge is successfully flashed. It will be red if the flash failed.

# Dependencies

This script requires ``python3`` which can be installed from https://www.python.org/downloads/.

The following Python modules must be installed:
| Module | URL | PIP |
| ------------- | ------------- | ------------- |
| ``tkinter`` | https://tkdocs.com/tutorial/install.html | Tkinter (and, since Python 3.1, ttk, the interface to the newer themed widgets) is included in the Python standard library. | 
| ``pyserial`` | https://pyserial.readthedocs.io/en/latest/pyserial.html | ```pip install pyserial```| 
| ``esptool`` | https://docs.espressif.com/projects/esptool/en/latest/esp32s2/installation.html | ```pip install esptool``` |

For the script to actually flash firmware, the following files must be in the same directory as ``pyFlashGui.py``:

| File | Relative Path |
| ------------- | ------------- |
| ``bootloader.bin`` | ``../build/bootloader/bootloader.bin`` |
| ``swadge-esp32.bin`` | ``../build/swadge-esp32.bin`` |
| ``partition-table.bin`` | ``../build/partition_table/partition-table.bin`` |
| ``storage.bin`` | ``../build/storage.bin`` |

Assuming you've checked out the whole repository, set up the IDF, and are in the `Super-2023-Swadge-FW` folder, then this should build and place the files:

```bash
idf.py build
cp ./build/bootloader/bootloader.bin ./build/swadge-esp32.bin ./build/partition_table/partition-table.bin ./build/storage.bin ./pyFlashGui/
```

# Instructions

1. Download and install the dependencies
1. Download the final Swadge firmware from the Releases tab: https://github.com/AEFeinstein/Super-2023-Swadge-FW/releases/
1. Run the programmer script (``python3 pyFlashGui.py`` from a terminal)
1. Switch the Swadge's power to USB (right position)
1. Hold the PGM button down (Up on the directional pad)
1. Plug the Swadge into the computer with a USB-C cable
1. Wait for the GUI to flash green, then you're done.

TODO: Make instructional video (flash & test)
