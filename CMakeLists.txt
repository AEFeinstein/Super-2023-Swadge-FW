# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(swadge-esp32)

add_custom_target(before_flash
	# If the swadge is running reboot it into the bootloader.
	COMMAND make -C ../tools/reboot_into_bootloader reboot
)

add_dependencies(flash before_flash)

# NOTE: This doesn't work.  So, don't press-and-hold up. Just let the part naturally reboot.
#add_custom_command(TARGET flash POST_BUILD
#	Sometimes the swadge needs a kick to get rebooted
#	COMMAND ${ESPTOOLPY} --before no_reset --after no_reset load_ram ../tools/bootload_reboot_stub/bootload_reboot_stub.bin
#)
