# Table of Contents

* [Welcome](#welcome)
* [Continuous Integration](#continuous-integration)
* [Configuring Your Environment](#configuring-your-environment)
  * [Windows](#windows-powershell)
  * [Linux](#linux)
  * [macOS](#mac-os)
  * [Configuring VSCode](#configuring-vscode)
* [Building and Flashing Firmware](#building-and-flashing-firmware)
* [Building and Running the Emulator](#building-and-running-the-emulator)
* [Contribution Guide](#contribution-guide)
  * [How to Contribute a Feature](#how-to-contribute-a-feature)
  * [How to Report a Bug](#how-to-report-a-bug)
* [Adding a Swadge Mode](#adding-a-swadge-mode)
* [Troubleshooting](#troubleshooting)
* [Tips](#tips)

# Welcome

Magfest is cool. Welcome to this playground.

# Continuous Integration

This project uses CircleCI to automatically build the firmware any time a change is pushed to `main`. Eventually it will notify our Slack channel whenever there is a fresh executable. I hope the build is currently passing!

[![AEFeinstein](https://circleci.com/gh/AEFeinstein/esp32-c3-playground.svg?style=svg)](https://circleci.com/gh/AEFeinstein/esp32-c3-playground)

# Configuring Your Environment

No matter your environment, you'll need to first install `git`. Just google it.

You should follow the instructions on this page, but I recommend reading through [the official ESP32-S2 Get Started Guide for setting up a development environment](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html#setting-up-development-environment) for more context or if something written here doesn't work anymore. When given the option, use command line tools instead of installer programs.

For Windows and Linux, I recommend setting up native tools. I don't recommend WSL in Windows. I haven't tried any setup on macOS yet.

As of writing, this project requires a modified IDF which can be found at https://github.com/AEFeinstein/esp-idf.git. **DO NOT** `git clone` Espressif's official IDF. The modifications add support for USB HID device mode and more TFT driver chips.

This project uses CircleCI to build the firmware each time code is committed to `main`. As a consequence, you can always read the [config.yml](/.circleci/config.yml) to see how the Windows build environment is set up from scratch for both the firmware and emulator.

## Windows (Powershell)

To set up the ESP32 build environment, you can run this powershell script. It will install the IDF in `~/esp/esp-idf` and the tools in `~/.espressif`. I don't recommend changing those paths.

```powershell
# Make an esp folder and move into it
mkdir ~/esp
cd ~/esp

# Clone the IDF and move into it
git clone --recursive https://github.com/AEFeinstein/esp-idf.git
cd ~/esp/esp-idf

# Initialize submodules
git submodule update --init --recursive

# Install tools
./install.ps1

# Export paths and variables
./export.ps1
```

Note that `./export.ps1` does not make any permanent changes and it must be run each time you open a new terminal for a build.

To set up the emulator build environment, you'll need to install [msys2](https://www.msys2.org/). You can do that with their installer. The command to install required packages for building from an msys2 terinal after installing msys2 is:

```bash
pacman -S base-devel mingw-w64-x86_64-toolchain make zip mingw-w64-x86_64-python-pip
```

Alternatively, you can run this powershell script to download, install, and update msys2 in one shot. Feel free to replace `C:\` with a different path if you want. Note that there may be an updated installer than the one used in this script.

```powershell
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
```

After installing msys2, you'll need to add it to Windows's path variable. [Here are some instructions on how to do that](https://www.architectryan.com/2018/03/17/add-to-the-path-on-windows-10/). You must add `C:\msys64\mingw64\bin` and `C:\msys64\usr\bin` to the path, in that order, and **before** `C:\Windows\System32`. That's because two different `find.exe` programs exist, one in msys2 and one in System32, and the makefile expects the msys2 one. I also recommend adding the msys2 path variables **after** any Python installs you already have on your system. The new path will take effect for any new shells, so restart any shells you happen to have open.

## Linux

To set up the ESP32 build environment, you can run this bash script. It will install the IDF in `~/esp/esp-idf` and the tools in `~/.espressif`. I don't recommend changing those paths.

```bash
# Make an esp folder and move into it
mkdir ~/esp
cd ~/esp

# Clone the IDF and move into it
git clone --recursive https://github.com/AEFeinstein/esp-idf.git
cd ~/esp/esp-idf

# Initialize submodules
git submodule update --init --recursive

# Install tools
./install.sh

# Export paths and variables
./export.sh
```

To set up the emulator build environment, you'll need to install the following packages. The command is given for the `apt` package manager, but you should use whatever package manager your system uses

```bash
sudo apt install build-essential xorg-dev libx11-dev libxinerama-dev libxext-dev mesa-common-dev libglu1-mesa-dev
```

## Mac OS

```
TBD
```

## Configuring VSCode

For all OSes, I recommend using the Visual Studio Code IDE and the [Espressif Extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).

If you've already configured your environment, which you should have, then when setting up the extension for the first time, point the extension at the existing IDF and tools rather than installing new ones. Remember that the IDF should exist in `~/esp/esp-idf` and the tools should exist in `~/.espressif/`.

The `.vscode` folder already has tasks for making and cleaning the emulator. It also has launch settings for launching the emulator with `gdb` attached. To build the firmware from VSCode, use the espressif extension buttons on the bottom toolbar. The build icon looks like a cylinder. Hover over the other icons to see what they do.

# Building and Flashing Firmware

1. Clone this repository if you don't have it already.

    ```powershell
    cd ~/esp/
    git clone https://github.com/AEFeinstein/esp32-c3-playground.git
    cd esp32-c3-playground
    git submodule update --init --recursive
    ```

1. Make sure the IDF symbols are exported. This example is for Windows, so the actual command may be different for your OS.

    ```powershell
    ~/esp/esp-idf/export.ps1
    ```

1. Build, flash, and monitor with a single command. Note in this example the ESP is connected to `COM8`, and the serial port will likely be different on your system.

    ```powershell
    idf.py -p COM8 -b 2000000 build flash monitor
    ```

1. Close the monitor with `ctrl` + `]`

# Building and Running the Emulator

1. Clone this repository if you don't have it already. You probably already have it from the prior step but it never hurt to mention.

    ```powershell
    cd ~/esp/
    git clone https://github.com/AEFeinstein/esp32-c3-playground.git
    cd esp32-c3-playground
    git submodule update --init --recursive
    ```

1. Build the emulator. Simple

    ```powershell
    make -f emu.mk clean all
    ```

1. Run the emulator. Also simple.

   ```powershell
   swadge_emulator.exe
   ```

# Contribution Guide

## How to Contribute a Feature

1. Create an issue in this repository with a short summary of the feature and any specs you want to write. Summaries and specs are a great way to make sure everyone is on the same page before writing code. No one likes writing a feature only to have to rewrite or scrap it later due to a misunderstanding. Specs also make writing documentation like user guides easier.
1. [Fork this repository](https://help.github.com/en/articles/fork-a-repo) to create your own personal copy of the project. Working in your own forked repository guarantees no one else will mess with the project in unexpected ways during your development (and you won't mess with anyone else either!).
1. If you're going to develop multiple features in your fork, you should [create a branch](https://help.github.com/en/articles/creating-and-deleting-branches-within-your-repository) for each feature. Keeping a single feature per-branch leads to cleaner and easier to understand pull requests.
1. This is the fun part, write your feature!
    1. Please comment your code. This makes it easier for everyone to understand. 
    1. [Doxygen comments](http://www.doxygen.nl/manual/docblocks.html) will be especially appreciated. You can [set up VSCode](https://marketplace.visualstudio.com/items?itemName=cschlosser.doxdocgen) to automatically add empty Doxygen templates to functions
    1. You should [run astyle](http://astyle.sourceforge.net/) with [this projects .astylerc file](/.astylerc) to beautify the code. Everyone loves pretty code. There's a [VSCode Extension](https://marketplace.visualstudio.com/items?itemName=chiehyu.vscode-astyle) for this too.
    1. The code should compile without any warnings.
    1. Try to write small, useful messages in each commit.
1. Test your feature. Try everything, mash buttons, whatever. Get creative. Users certainly will. Write your test plan and steps in the ticket you opened.
1. Once your feature is written and tested, [create a pull request](https://help.github.com/en/articles/creating-a-pull-request) to merge the feature back to the master project. Please reference the ticket from step 1 in the pull request.
1. The new code will be reviewed and either merge it or have changes requested. The better the spec and conversation in step 1, the better the chances it gets merged quickly.

## How to Report a Bug

Create an issue in this project with the following information:

* What the expected behavior is.
* What the actual behavior is. Pictures or video could be useful here.
* If repeatable, steps to reproduce the buggy behavior.
* If not repeatable, what you were doing when the bug occurred.
* Serial logs, if you were logging.

# Adding a Swadge Mode

TODO

# Troubleshooting

Reread the [Get Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html), then google your issue, then ask me about it either in a Github issue or the Slack channel. All troubleshooting issues should be written down here for posterity.

If VSCode isn't finding IDF symbols, try running the `export.ps1` script from a terminal, then launching `code` from that same session.

# Tips

To add more source files, they either need to be in the `main` folder, and added to the `CMakeLists.txt` file there, or in a subdirectory of the `components` folder with it's own `CMakeLists.txt`. The folder names are specific. You can read up on the [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) if you're curious.

There are a lot of example projects in the IDF that are worth looking at.
