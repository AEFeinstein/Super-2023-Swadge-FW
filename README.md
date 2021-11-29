# Welcome

Intro text to come later

## Configuring Your Environment

I recommend following [the official ESP32-C3 Get Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html).

For all OSes, I recommend setting up native tools.

As of writing, this project requires a modified IDF which can be found at https://github.com/AEFeinstein/esp-idf.git. Do not `git clone` Espressif's official IDF. The modifications add support for USB HID device mode and more TFT driver chips.

- Windows (Powershell)
    ```
    mkdir $HOME/esp
    cd $HOME/esp
    git clone --recursive https://github.com/AEFeinstein/esp-idf.git
    cd $HOME/esp/esp-idf
    ./install.ps1
    ./export.ps1
    ```
- Linux
    ```
    mkdir $HOME/esp
    cd $HOME/esp
    git clone --recursive https://github.com/AEFeinstein/esp-idf.git
    cd $HOME/esp/esp-idf
    git submodule update --init --recursive
    ./install.sh
    . ./export.sh
    ```
- Mac OS
    ```
    TBD
    ```

## Building and Flashing

1. Make sure the IDF symbols are exported. This example is for Windows, so the actual command may be different for your OS.
    ```
    cd $HOME/esp/esp-idf
    ./export.ps1
    ```
1. Clone this repository.
    ```
    cd $HOME
    git clone https://github.com/AEFeinstein/esp32-c3-playground.git
    cd esp32-c3-playground
    ```
1. Build, flash, and monitor with a single command. Note in this example the ESP is connected to `COM8`, and the serial port will likely be different on your system.
    ```
    idf.py -p COM8 -b 2000000 build flash monitor
    ```
1. Close the monitor with `ctrl` + `]`

## Configuring VSCode

For all OSes, I recommend using the Visual Studio Code IDE and the [Espressif Extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).

If you've already configured your environment, which you should have, then when setting up the extension for the first time, point the extension at the existing IDF and tools rather than installing new ones. Remember that the IDF should exist in `$HOME/esp/esp-idf` and the tools should exist in `$HOME/.espressif/`

## Troubleshooting

Reread the [Get Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html), then google your issue, then ask me about it. All troubleshooting issues should be written down here for posterity.

If VSCode isn't finding IDF symbols, try running the `export` script from a terminal, then launching `code` from that same session.

## Tips

To add more source files, they either need to be in the `main` folder, and added to the `CMakeLists.txt` file there, or in a subdirectory of the `components` folder with it's own `CMakeLists.txt`. The folder names are specific. You can read up on the [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) if you're curious.

There are a lot of example projects in the IDF that are worth looking at.
