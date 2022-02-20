# Table of Contents

* [Welcome](#welcome)
* [Continuous Integration](#continuous-integration)
* [Configuring Your Environment](#configuring-your-environment)
  * [Windows](#windows-powershell)
  * [Linux](#linux)
  * [macOS](#macos)
  * [Configuring VSCode](#configuring-vscode)
* [Building and Flashing Firmware](#building-and-flashing-firmware)
* [Building and Running the Emulator](#building-and-running-the-emulator)
* [Contribution Guide](#contribution-guide)
  * [How to Contribute a Feature](#how-to-contribute-a-feature)
  * [How to Report a Bug](#how-to-report-a-bug)
* [Adding a Swadge Mode](#adding-a-swadge-mode)
  * [Swadge Mode Struct](#swadge-mode-struct)
  * [Drawing to the Screen](#drawing-to-the-screen)
  * [Loading and Freeing Assets](#loading-and-freeing-assets)
  * [Saving Persistent Data](#saving-persistent-data)
  * [Lighting LEDs](#lighting-leds)
  * [Playing Sounds](#playing-sounds)
  * [ESP-NOW](#esp-now)
  * [Best Practices](#best-practices)
* [Troubleshooting](#troubleshooting)
* [Tips](#tips)

# Welcome

Magfest is cool. Welcome to this playground.

# Continuous Integration

This project uses CircleCI to automatically build the firmware any time a change is pushed to `main`. Eventually it will notify our Slack channel whenever there is a fresh executable. I hope the build is currently passing!

[![AEFeinstein](https://circleci.com/gh/AEFeinstein/esp32-c3-playground.svg?style=svg)](https://circleci.com/gh/AEFeinstein/esp32-c3-playground)

# Configuring Your Environment

No matter your environment, you'll need to first install `git`. Just google it.

You must follow the instructions on this page, but I recommend reading through [the official ESP32-S2 Get Started Guide for setting up a development environment](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html#setting-up-development-environment) for more context or if something written here doesn't work anymore. When given the option, use command line tools instead of installer programs.

From the official guide:
> Keep in mind that ESP-IDF does not support spaces in paths.

By default this guide sets up ESP-IDF in your home directory, so if your username has a space in it, please change all paths to something without a space, like `c:\esp\`. Also note that `ccache` uses a temporary directory in your home directory, and spaces in that path cause issues. `ccache` is enabled by default when running `export.ps1`, but it can be disabled by removing the following from `esp-idf/tools/tools.json`:
```
"export_vars": {
  "IDF_CCACHE_ENABLE": "1"
},
```
For Windows and Linux, I recommend setting up native tools. I don't recommend WSL in Windows. I haven't tried any setup on macOS yet.

As of writing, this project requires IDF v4.4 with a patch for USB HID support applied. The patch is [usb_hid.patch](/usb_hid.patch) and assuming you download it and move it to the `esp-idf` folder, it can be applied with this command:

```powershell
git apply usb_hid.patch
```

This project uses CircleCI to build the firmware each time code is committed to `main`. As a consequence, you can always read the [config.yml](/.circleci/config.yml) to see how the Windows build environment is set up from scratch for both the firmware and emulator.

## Windows (Powershell)

To set up the ESP32 build environment, you can run this powershell script. It will install the IDF in `~/esp/esp-idf` and the tools in `~/.espressif`. I don't recommend changing those paths.

```powershell
# Make an esp folder and move into it
mkdir ~/esp
cd ~/esp

# Clone the IDF and move into it
git clone -b v4.4 --recursive https://github.com/espressif/esp-idf.git esp-idf
cd ~/esp/esp-idf

# Download the USB HID patch and apply it
Invoke-WebRequest https://raw.githubusercontent.com/AEFeinstein/esp32-c3-playground/main/usb_hid.patch -OutFile usb_hid.patch
git apply usb_hid.patch

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
git clone -b v4.4 --recursive https://github.com/espressif/esp-idf.git esp-idf
cd ~/esp/esp-idf

# Download the USB HID patch and apply it
wget https://raw.githubusercontent.com/AEFeinstein/esp32-c3-playground/main/usb_hid.patch
git apply usb_hid.patch

# Initialize submodules
git submodule update --init --recursive

# Install tools
./install.sh

# Export paths and variables
./export.sh
```

To set up the emulator build environment, you'll need to install the following packages. The command is given for the `apt` package manager, but you should use whatever package manager your system uses

```bash
sudo apt install build-essential xorg-dev libx11-dev libxinerama-dev libxext-dev mesa-common-dev libglu1-mesa-dev libasound2-dev libpulse-dev
```

## macOS

```
TBD
```

## Configuring VSCode

For all OSes, I recommend using the [Visual Studio Code IDE](https://code.visualstudio.com/) and the [Espressif Extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).

If you've already configured your environment, which you should have, then when setting up the extension for the first time, point the extension at the existing IDF and tools rather than installing new ones. Remember that the IDF should exist in `~/esp/esp-idf` and the tools should exist in `~/.espressif/`.

The `.vscode` folder already has tasks for making and cleaning the emulator. It also has launch settings for launching the emulator with `gdb` attached. To build the firmware from VSCode, use the espressif extension buttons on the bottom toolbar. The build icon looks like a cylinder. Hover over the other icons to see what they do.

I also recommend installing the [Doxygen Documentation Generator
](https://marketplace.visualstudio.com/items?itemName=cschlosser.doxdocgen) plugin to generate [Doxygen comments](http://www.doxygen.nl/manual/docblocks.html) and the [Astyle](https://marketplace.visualstudio.com/items?itemName=chiehyu.vscode-astyle) plugin to automatically format code with with [this project's .astylerc file](/.astylerc).

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

A Swadge mode is kind of like an app. Only one runs at a time and it gets all the system resources.

All Swadge mode source code should be in the `/main/modes` folder. To build, the source file must be listed in [`/main/CMakeLists.txt`](/main/CMakeLists.txt).

## Swadge Mode Struct

A Swadge mode is struct of function pointers, which is the closest thing in `C` to an object. The struct is defined in `swadgeMode.h`. The ESP32's system firmware will call the Swadge mode's function pointers. If a mode does not need a particular function, say it doesn't do audio handling, it is safe to set the function pointer to NULL. It just won't be called. The details of the struct and all the function pointers it contains are below. You can also read this in source.

```C
typedef struct _swadgeMode
{
    /**
     * This swadge mode's name, mostly for debugging.
     * This is not a function pointer.
     */
    char* modeName;

    /**
     * This function is called when this mode is started. It should initialize
     * any necessary variables.
     * disp should be saved and used for later draw calls.
     *
     * @param disp The display to draw to
     */
    void (*fnEnterMode)(display_t * disp);

    /**
     * This function is called when the mode is exited. It should clean up
     * anything that shouldn't happen when the mode is not active
     */
    void (*fnExitMode)(void);

    /**
     * This function is called from the main loop. It's pretty quick, but the
     * timing may be inconsistent.
     *
     * @param elapsedUs The time elapsed since the last time this function was
     *                  called. Use this value to determine when it's time to do
     *                  things
     */
    void (*fnMainLoop)(int64_t elapsedUs);

    /**
     * This function is called when a button press is pressed. Buttons are
     * handled by interrupts and queued up for this callback, so there are no
     * strict timing restrictions for this function.
     *
     * @param evt The button event that occurred
     */
    void (*fnButtonCallback)(buttonEvt_t* evt);

    /**
     * This function is called when a touchpad event occurs.
     *
     * @param evt The touchpad event that occurred
     */
    void (*fnTouchCallback)(touch_event_t* evt);

    /**
     * This function is called periodically with the current acceleration
     * vector.
     *
     * @param accel A struct with 10 bit signed X, Y, and Z accel vectors
     */
    void (*fnAccelerometerCallback)(accel_t* accel);

    /**
     * This function is called whenever audio samples are read from the
     * microphone (ADC) and are ready for processing. Samples are read at 8KHz
     *
     * @param samples A pointer to 12 bit audio samples
     * @param sampleCnt The number of samples read
     */
    void (*fnAudioCallback)(int16_t * samples, uint32_t sampleCnt);

    /**
     * This function is called periodically with the current temperature
     *
     * @param temperature A floating point temperature in celcius
     */
    void (*fnTemperatureCallback)(float temperature);

    /**
     * This is a setting, not a function pointer. Set it to one of these
     * values to have the system configure the swadge's WiFi
     *
     * NO_WIFI - Don't use WiFi at all. This saves power.
     * ESP_NOW - Send and receive packets to and from all swadges in range
     */
    wifiMode_t wifiMode;

    /**
     * This function is called whenever an ESP-NOW packet is received.
     *
     * @param mac_addr The MAC address which sent this data
     * @param data     A pointer to the data received
     * @param len      The length of the data received
     * @param rssi     The RSSI for this packet, from 1 (weak) to ~90 (touching)
     */
    void (*fnEspNowRecvCb)(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi);

    /**
     * This function is called whenever an ESP-NOW packet is sent.
     * It is just a status callback whether or not the packet was actually sent.
     * This will be called after calling espNowSend()
     *
     * @param mac_addr The MAC address which the data was sent to
     * @param status   The status of the transmission
     */
    void (*fnEspNowSendCb)(const uint8_t* mac_addr, esp_now_send_status_t status);
} swadgeMode;
```

## Drawing to the Screen

Drawing to a display is done through a struct of function pointers, `display_t`. This struct has basic functions for setting and getting pixels, clearing the whole display, drawing the current framebuffer to the physical display (which you should not call directly). It also has the display's width and height, in pixels. This design decision was made to abstract any framebuffer specifics from functions that draw graphics. For instance, the TFT has 15 bit color, the OLED has 1 bit color, and the emulator has 24 bit color, but the same functions for drawing sprites can be used for all three. This design could even drive multiple displays simultaneously.

The TFT display uses 15 bit color and 1 bit alpha. That means that the red, green, and blue channels all range from 0 to 31 (0x1F), and alpha is either `PX_TRANSPARENT` or `PX_OPAQUE`. It packs neatly into a 16 bit variable, `rgba_pixel_t`, which is often used as a parameter when drawing.

You do not have to call any functions to draw the current framebuffer to the physical display. That is handled by the system firmware. Just draw your frame and it will get pushed out as fast as possible.

There are a few convenient ways to draw your frame. You can use the `display_t` struct's `clearPx()` function to clear the entire frame before drawing, unless you're only redrawing specific elements. If you really want to draw a single pixel at a time, you can call the `display_t` struct's `setPx()` function. Likewise, `getPx()` will return a pixel from the current frame. This may be useful for collision detection or something.

`bresenham.h` contains functions for drawing shapes like lines, rectangles, circles, or curves. Note that these shapes are not filled in. If you want filled shapes, or other shapes, we'll need to work on that. Remember that more complex polygons are just series of lines.

Drawing more complex graphics, like text or `png` images is explained in the next section, [Loading and Freeing Assets](#loading-and-freeing-assets).

As an example, this will clear the display, then draw a pixel, line, rectangle, and circle.

```C
#include "display.h"
#include "bresenham.h"

// Clear the display to black
demo->disp->clearPx();

// Draw a single white pixel in the middle of the display
rgba_pixel_t pxCol = { .a = PX_OPAQUE };
pxCol.r = 0x1F;
pxCol.g = 0x1F;
pxCol.b = 0x1F;
demo->disp->setPx(
    demo->disp->w / 2, // Middle of the screen width
    demo->disp->h / 2, // Middle of the screen height
    pxCol);

// Draw a yellow line
pxCol.r = 0x1F;
pxCol.g = 0x1F;
pxCol.b = 0x00;
plotLine(demo->disp, 10, 5, 50, 20, pxCol);

// Draw a magenta rectangle
pxCol.r = 0x1F;
pxCol.g = 0x00;
pxCol.b = 0x1F;
plotRect(demo->disp, 70, 5, 100, 20, pxCol);

// Draw a cyan circle
pxCol.r = 0x00;
pxCol.g = 0x1F;
pxCol.b = 0x1F;
plotCircle(demo->disp, 140, 30, 20, pxCol);
```

## Loading and Freeing Assets

The build system will automatically process, pack, and flash assets as a read-only part of the firmware. The the [`spiffs_file_preprocessor`](/spiffs_file_preprocessor/) is responsible for this. Assets are an easy way to include things like images, fonts, and eventually other file types. Any files in the [`/assets/`](/assets/) folder will be processed and the output will be written to [`/spiffs_image/`](/spiffs_image/).

`spiffs_file_preprocessor` currently only processes `.png` and `.font.png` files. `.png` are converted into `.qoi` files, which are reasonably compressed and have very little overhead to decode. You can read about that at [QOI Image Format](https://qoiformat.org/). `.font.png` files are a line of characters with underlines to denote char width (open one up to see what I'm talking about). They are converted into a one-bit bitmapped format.

Loading assets is a relatively slower operation, so often times it makes sense to load once when a mode starts and free when the mode finishes. On the other hand, loading assets eats up RAM, so it may be wise to only load assets when necessary. Engineering is a figuring out a series of trade-offs.

As an example, this will load, draw, and free both an image and some red text. Note that the TFT's screen uses 15 bit color, so the max for each r/g/b channel is `0x1F`.

```C
#include "display.h"

qoi_t megaman;
loadQoi("megaman.qoi", &megaman);
drawQoi(demo->disp, &megaman, 0, 0);
freeQoi(&megaman);

font_t ibm;
loadFont("ibm_vga8.font", &ibm);
rgba_pixel_t color = {.r = 0x1F, .g = 0x00, .b = 0x00, .a = PX_OPAQUE};
drawText(demo->disp, &ibm, color, "Demo Text", 0, 0);
freeFont(&ibm);
```

## Saving Persistent Data

Persistent data is saved in a key-value store. Currently the only value that can be stored is a 32 bit integer. This may be expanded in the future. The functions `readNvs32()` and `writeNvs32()` do what they say. As an example, this will write a value to NVS if it does not already exist.

```C
#include "nvs_manager.h"

#define MAGIC_VAL 0xAF

int32_t magicVal = 0;
if((false == readNvs32("magicVal", &magicVal)) || (MAGIC_VAL != magicVal))
{
    writeNvs32("magicVal", MAGIC_VAL);
}
```

## Lighting LEDs

Use an array of `led_t` structs and call `setLeds()` to light them the color of your choice. `NUM_LEDS` is defined as the number of LEDs on the hardware. Each color channel has eight bits of range (i.e. 0 to 255). For example, this will make six LEDs a rainbow.

```C
#include "led_util.h"

static const led_t leds[NUM_LEDS] =
{
    {.r = 0xFF, .g = 0x00, .b = 0x00},
    {.r = 0x80, .g = 0x80, .b = 0x00},
    {.r = 0x00, .g = 0xFF, .b = 0x00},
    {.r = 0x00, .g = 0x80, .b = 0x80},
    {.r = 0x00, .g = 0x00, .b = 0xFF},
    {.r = 0x80, .g = 0x00, .b = 0x80}
};

setLeds(leds, NUM_LEDS);
```

## Playing Sounds

The buzzer can play `song_t` structs. Each `song_t` is a collection of `musicalNote_t`, and each `musicalNote_t` has a `noteFrequency_t` and a duration. Eventually, songs will be loadable assets. For example, this will play three notes

```C
#include "musical_buzzer.h"

static const song_t do_re_mi =
{
    .notes =
    {
        {.note = A_4, .timeMs = 400},
        {.note = SILENCE, .timeMs = 100},
        {.note = B_4, .timeMs = 400},
        {.note = SILENCE, .timeMs = 100},
        {.note = C_4, .timeMs = 400},
    },
    .numNotes = 5,
    .shouldLoop = false
};

buzzer_play(&do_re_mi);
```

## ESP-NOW

ESP-NOW is a kind of connectionless Wi-Fi communication protocol that is defined by Espressif. You can read all about it [in the official documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/network/esp_now.html).

To enable ESP-NOW, set the Swadge mode struct's `wifiMode` to `ESP_NOW`.

The Swadge uses the simplest form of ESP-NOW, where every message sent to the broadcast address, and every Swadge receives every message in range. If all you want is simple UDP-like communication then simply add the two required function pointers to your Swadge mode struct. One will be called when receiving messages, and the other will be called after transmission with transmission statuses. Call `espNowSend()` to send packets, and receive other packets through the callback. For example:

```C
#include "espNowUtils.h"

// Broadcast some data
const uint8_t[] data = {1, 2, 3};
espNowSend(data, sizeof(data) / sizeof(data[0]));

// Callbacks for the Swadge mode struct
void demoEspNowRecvCb(const uint8_t* mac_addr, const char* data, uint8_t len, int8_t rssi)
{
    ; // Do something with received packet
}

void demoEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    ; // Confirmation that a packet was sent, or failed to send
}
```

If you want TCP-like communication between two Swadges, then you should use the `p2p` code. `p2p` builds on ESP-NOW by including a mode ID (so that a given Swadge mode doesn't try to connect to a different mode), destination MAC address, sequence number, and packet type in the payload. Packets are ack'ed and duplicates are ignored. Packets from other modes or Swadges are also ignored.

For `p2p` to work, it must be initialized and deinitialized, ideally when the Swadge mode starts and finishes. `p2pRecvCb()` and `p2pSendCb()` must be called from the respective functions registered for ESP-NOW in the Swadge struct.

A `p2p` connection is established by having both Swadges call `p2pStartConnection()` and being in close range of each other. The RSSI must be above a given threshold to establish a connection. Connection events are signaled through the `p2pConCbFn` callback function. Once established, one Swadge is designated `GOING_FIRST` and the other is designated `GOING_SECOND`. Calling `p2pGetPlayOrder()` will return the designation.

Once connected, messages can be sent by calling `p2pSendMsg()`. Messages are received through the `p2pMsgRxCbFn` callback function.

```C
#include "espNowUtils.h"
#include "p2pConnection.h"

// Initialization and deinitialization
void demoEnterMode(display_t * disp)
{
    p2pInitialize(&demo->p, "dmo", demoConCbFn, demoMsgRxCbFn, -10);
    p2pStartConnection(&demo->p);
}

void demoExitMode(void)
{
    p2pDeinit(&demo->p);
}

// Callbacks for the Swadge mode struct
void demoEspNowRecvCb(const uint8_t* mac_addr, const uint8_t* data, uint8_t len, int8_t rssi)
{
    p2pRecvCb(&demo->p, mac_addr, data, len, rssi);
}

void demoEspNowSendCb(const uint8_t* mac_addr, esp_now_send_status_t status)
{
    p2pSendCb(&demo->p, mac_addr, status);
}

// Callbacks for p2p
void demoConCbFn(p2pInfo* p2p, connectionEvt_t evt)
{
    ; // Do something when connection status changes
}

void demoMsgRxCbFn(p2pInfo* p2p, const char* msg, const char* payload, uint8_t len)
{
    ; // Do something when a message is received
}

void demoMsgTxCbFn(p2pInfo* p2p, messageStatus_t status)
{
    ; // Do something with the transmission status
}

// Send a message
const char tMsg[] = "Test Message";
p2pSendMsg(&(demo->p), "tst", tMsg, sizeof(tMsg), demoMsgTxCbFn);
```

## Best Practices

Only one Swadge mode runs at a time, and each mode wants as much RAM as possible. Therefore it's best practice to not statically allocate state variables, especially large ones. This includes `static` variables within functions. Don't use them. It's good practice to keep all your mode's state variables in a single struct which is allocated when the mode starts and freed when it ends. For example:

```C
// This is just a typedef, not a variable
typedef struct
{
    uint16_t stateVar1;
    uint8_t bunch_of_numbers[256];
} demo_t;

// This is just a single pointer, so it's small
demo_t * demo;

// When entering the mode, allocate memory for it
void demoEnterMode(display_t * disp)
{
    // Allocate memory for this mode
    demo = (demo_t *)malloc(sizeof(demo_t));
    memset(demo, 0, sizeof(demo_t));
}

// When exiting the mode, free the memory
void demoExitMode(void)
{
    free(demo);
}
```

# Troubleshooting

Reread the [Get Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html), then google your issue, then ask me about it either in a Github issue or the Slack channel. All troubleshooting issues should be written down here for posterity.

If VSCode isn't finding IDF symbols, try running the `export.ps1` script from a terminal, then launching `code` from that same session.

# Tips

To add more source files, they either need to be in the `main` folder, and added to the `CMakeLists.txt` file there, or in a subdirectory of the `components` folder with it's own `CMakeLists.txt`. The folder names are specific. You can read up on the [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) if you're curious.

There are a lot of example projects in the IDF that are worth looking at.
