# Makefile by Adam, 2022

################################################################################
# What OS we're compiling on
################################################################################

ifeq ($(OS),Windows_NT)
    HOST_OS = Windows
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        HOST_OS = Linux
    endif
    ifeq ($(UNAME_S),Darwin)
        HOST_OS = Darwin
    endif
endif

FIND:=find
ifeq ($(HOST_OS),Windows)
	FIND:=$(shell cygpath `where find | grep bin | grep -v " "`)
endif

################################################################################
# Programs to use
################################################################################

ifeq ($(HOST_OS),Windows)
	CC = x86_64-w64-mingw32-gcc.exe
endif
ifeq ($(HOST_OS),Linux)
	CC = gcc
endif

################################################################################
# Source Files
################################################################################

# This is a list of directories to scan for c files recursively
SRC_DIRS_RECURSIVE = emu/src main/display main/modes main/colorchord main/utils
# This is a list of directories to scan for c files not recursively
SRC_DIRS_FLAT = main
# This is a list of files to compile directly. There's no scanning here
SRC_FILES = components/hdw-spiffs/heatshrink_decoder.c components/hdw-spiffs/spiffs_json.c components/hdw-spiffs/spiffs_txt.c
# This is all the source directories combined
SRC_DIRS = $(shell $(FIND) $(SRC_DIRS_RECURSIVE) -type d) $(SRC_DIRS_FLAT)
# This is all the source files combined
SOURCES   = $(shell $(FIND) $(SRC_DIRS) -maxdepth 1 -iname "*.[c]") $(SRC_FILES)

################################################################################
# Compiler Flags
################################################################################

# These are flags for the compiler, all files
CFLAGS = \
	-c \
	-std=gnu99 \
	-g \
	-static-libgcc \
	-static-libstdc++ \
	-ggdb \
	-Ofast

# ifeq ($(HOST_OS),Linux)
# CFLAGS += \
# 	-fsanitize=address \
# 	-fno-omit-frame-pointer
# endif

# These are warning flags that the IDF uses
CFLAGS_WARNINGS = \
	-Wall \
	-Werror=all \
	-Wno-error=unused-function \
	-Wno-error=unused-variable \
	-Wno-error=deprecated-declarations \
	-Wextra \
	-Wno-unused-parameter \
	-Wno-sign-compare \
	-Wno-error=unused-but-set-variable \
	-Wno-old-style-declaration
	
# These are warning flags that I like
CFLAGS_WARNINGS_EXTRA = \
	-Wundef \
	-Wformat=2 \
	-Winvalid-pch \
	-Wlogical-op \
	-Wmissing-format-attribute \
	-Wmissing-include-dirs \
	-Wpointer-arith \
	-Wunused-local-typedefs \
	-Wuninitialized \
	-Wshadow \
	-Wredundant-decls \
	-Wjump-misses-init \
	-Wswitch-enum \
	-Wcast-align \
	-Wformat-nonliteral \
	-Wno-switch-default \
	-Wunused \
	-Wunused-macros \
	-Wmissing-declarations \
	-Wmissing-prototypes \
	-Wcast-qual \
	-Wno-switch \
#	-Wstrict-prototypes \
#	-Wpedantic \
#	-Wconversion \
#	-Wsign-conversion \
#	-Wdouble-promotion

################################################################################
# Defines
################################################################################

# Create a variable with the git hash and branch name
GIT_HASH  = \"$(shell git rev-parse --short=7 HEAD)\"

# Used by the ESP SDK
DEFINES_LIST = \
	CONFIG_GC9307_240x280=y \
	CONFIG_IDF_TARGET_ESP32S2=y \
	SOC_RMT_CHANNELS_PER_GROUP=4 \
	SOC_TOUCH_SENSOR_NUM=14 \
	SOC_TOUCH_PROXIMITY_MEAS_DONE_SUPPORTED \
	WITH_PROFILING=0 \
	LOG_LOCAL_LEVEL=5 \
	EMU=1 \
	SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP=1 \
	SOC_PM_SUPPORT_CPU_PD=0 \
	SOC_ULP_SUPPORTED=0 \
	SOC_PM_SUPPORT_EXT_WAKEUP=0 \
	SOC_GPIO_SUPPORT_SLP_SWITCH=0 \
	CONFIG_MAC_BB_PD=0 \
	CONFIG_SWADGE_PROTOTYPE=1 \
	CONFIG_TFT_MAX_BRIGHTNESS=200 \
	CONFIG_TFT_MIN_BRIGHTNESS=10 \
	SOC_TIMER_GROUP_TIMERS_PER_GROUP=2 \
	SOC_TIMER_GROUPS=2 \
	GIT_SHA1=${GIT_HASH} \
	HAS_XINERAMA=1 \
	FULL_SCREEN_STEAL_FOCUS=1

DEFINES = $(patsubst %, -D%, $(DEFINES_LIST))

################################################################################
# Includes
################################################################################

# Look for folders with .h files in these directories, recursively
INC_DIRS_RECURSIVE = components
# Treat every source directory as one to search for headers in, also add a few more
INC_DIRS = $(SRC_DIRS) $(shell $(FIND) $(INC_DIRS_RECURSIVE) -type d)
# Prefix the directories for gcc
INC = $(patsubst %, -I%, $(INC_DIRS) )

################################################################################
# Output Objects
################################################################################

# This is the directory in which object files will be stored
OBJ_DIR = emu/obj

# This is a list of objects to build
OBJECTS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(SOURCES))

################################################################################
# Linker options
################################################################################

# This is a list of libraries to include. Order doesn't matter

ifeq ($(HOST_OS),Windows)
    LIBS = opengl32 gdi32 user32 winmm WSock32
endif
ifeq ($(HOST_OS),Linux)
    LIBS = m X11 asound pulse rt GL GLX pthread Xext Xinerama
endif

# These are directories to look for library files in
LIB_DIRS = 

# This combines the flags for the linker to find and use libraries
LIBRARY_FLAGS = $(patsubst %, -L%, $(LIB_DIRS)) $(patsubst %, -l%, $(LIBS)) \
	-static-libgcc \
	-static-libstdc++ \
	-ggdb \
	-Ofast

# ifeq ($(HOST_OS),Linux)
# LIBRARY_FLAGS += \
# 	-fsanitize=address \
# 	-fno-omit-frame-pointer \
# 	-static-libasan
# endif

################################################################################
# Build Filenames
################################################################################

# These are the files to build
EXECUTABLE = swadge_emulator

################################################################################
# Targets for Building
################################################################################

# This list of targets do not build files which match their name
.PHONY: all assets clean docs cppcheck print-%

# Build everything!
all: $(EXECUTABLE) assets

assets:
	$(MAKE) -C ./spiffs_file_preprocessor/
	./spiffs_file_preprocessor/spiffs_file_preprocessor -i ./assets/ -o ./spiffs_image/

# To build the main file, you have to compile the objects
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBRARY_FLAGS) -o $@

# This compiles each c file into an o file
./$(OBJ_DIR)/%.o: ./%.c
	@mkdir -p $(@D) # This creates a directory before building an object in it.
	$(CC) $(CFLAGS) $(CFLAGS_WARNINGS) $(CFLAGS_WARNINGS_EXTRA) $(DEFINES) $(INC) $< -o $@

# This clean everything
clean:
	$(MAKE) -C ./spiffs_file_preprocessor/ clean
	-@rm -f $(OBJECTS) $(EXECUTABLE)
	-@rm -rf docs

################################################################################
# Targets for Debugging
################################################################################

docs:
	doxygen swadge2019.doxyfile

cppcheck:
	cppcheck --std=c99 --platform=unix32 --suppress=missingIncludeSystem --enable=all $(DEFINES) $(INC) main > /dev/null

################################################################################
# Makefile Debugging
################################################################################

# Print any value from this makefile
print-%  : ; @echo $* = $($*)
