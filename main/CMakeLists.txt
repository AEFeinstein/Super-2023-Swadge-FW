idf_component_register(
    SRCS
        "advanced_usb_control.c"
        "colorchord/DFT32.c"
        "colorchord/embeddednf.c"
        "colorchord/embeddedout.c"
        "display/bresenham.c"
        "display/cndraw.c"
        "display/display.c"
        "display/palette.c"
        "meleeMenu.c"
        "modes/fighter/aabb_utils.c"
        "modes/fighter/fighter_hr_result.c"
        "modes/fighter/fighter_json.c"
        "modes/fighter/fighter_menu.c"
        "modes/fighter/fighter_mp_result.c"
        "modes/fighter/fighter_music.c"
        "modes/fighter/fighter_records.c"
        "modes/fighter/mode_fighter.c"
        "modes/jumper/jumper_menu.c"
        "modes/jumper/mode_jumper.c"
        "modes/mode_colorchord.c"
        "modes/mode_copypasta.c"
        "modes/mode_credits.c"
        "modes/mode_dance.c"
        "modes/mode_diceroller.c"
        "modes/mode_flight.c"
        "modes/mode_gamepad.c"
        "modes/mode_jukebox.c"
        "modes/mode_main_menu.c"
        "modes/mode_slide_whistle.c"
        "modes/mode_test.c"
        "modes/mode_tiltrads.c"
        "modes/mode_tunernome.c"
        "modes/paint/mode_paint.c"
        "modes/paint/paint_brush.c"
        "modes/paint/paint_common.c"
        "modes/paint/paint_draw.c"
        "modes/paint/paint_gallery.c"
        "modes/paint/paint_help.c"
        "modes/paint/paint_nvs.c"
        "modes/paint/paint_share.c"
        "modes/paint/paint_song.c"
        "modes/paint/paint_ui.c"
        "modes/paint/paint_util.c"
        "modes/paint/px_stack.c"
        "modes/picross/mode_picross.c"
        "modes/picross/picross_menu.c"
        "modes/picross/picross_select.c"
        "modes/picross/picross_tutorial.c"
        "modes/picross/picross_music.c"
        "modes/platformer/entity.c"
        "modes/platformer/entityManager.c"
        "modes/platformer/gameData.c"
        "modes/platformer/mode_platformer.c"
        "modes/platformer/platformer_sounds.c"
        "modes/platformer/tilemap.c"
        "p2pConnection.c"
        "settingsManager.c"
        "swadge_esp32.c"
        "swadge_util.c"
        "utils/linked_list.c"
        "utils/text_entry.c"
    INCLUDE_DIRS
        "."
        "../components/hdw-btn"
        "../components/hdw-buzzer"
        "../components/hdw-esp-now"
        "../components/hdw-i2c"
        "../components/hdw-led"
        "../components/hdw-mic"
        "../components/hdw-nvs"
        "../components/hdw-oled"
        "../components/hdw-qma6981"
        "../components/hdw-spiffs"
        "../components/hdw-temperature"
        "../components/hdw-tft"
        "../components/hdw-touch"
        "colorchord"
        "display"
        "modes"
        "modes/fighter"
        "modes/flight"
        "modes/jumper"
        "modes/paint"
        "modes/picross"
        "modes/platformer"
        "utils")

function(spiffs_file_preprocessor)
    add_custom_target(spiffs_preprocessor ALL
    COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs_file_preprocessor/
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs_file_preprocessor/spiffs_file_preprocessor -i ${CMAKE_CURRENT_SOURCE_DIR}/../assets/ -o ${CMAKE_CURRENT_SOURCE_DIR}/../spiffs_image/
    )
endfunction()

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
spiffs_file_preprocessor()
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)


execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE VERSION_SHA1 )

string(STRIP ${VERSION_SHA1} VERSION_SHA1)

add_definitions( -DGIT_SHA1="${VERSION_SHA1}" )
