
# raw_create_partition_image
# based on spiffs/project_include.cmake
#
function(raw_create_partition_image partition data_file)
    set(options FLASH_IN_PROJECT)
    set(multi DEPENDS)
    cmake_parse_arguments(arg "${options}" "" "${multi}" "${ARGN}")

    #idf_build_get_property(idf_path IDF_PATH)
    #set(spiffsgen_py ${PYTHON} ${idf_path}/components/spiffs/spiffsgen.py)

    get_filename_component(data_file_full_path ${data_file} ABSOLUTE)

    partition_table_get_partition_info(size "--partition-name ${partition}" "size")
    partition_table_get_partition_info(offset "--partition-name ${partition}" "offset")

    if("${size}" AND "${offset}")
		set(image_file ${CMAKE_BINARY_DIR}/${partition}.bin)
        set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY
            ADDITIONAL_MAKE_CLEAN_FILES
            ${image_file})

        idf_component_get_property(main_args esptool_py FLASH_ARGS)
        idf_component_get_property(sub_args esptool_py FLASH_SUB_ARGS)
        # Last (optional) parameter is the encryption for the target. In our
        # case, spiffs is not encrypt so pass FALSE to the function.
        esptool_py_flash_target(${partition}-flash "${main_args}" "${sub_args}" ALWAYS_PLAINTEXT)
        esptool_py_flash_to_partition(${partition}-flash "${partition}" "${image_file}0x00000.bin")

		add_custom_target( part_${partition}_gen ALL
			COMMAND 
			${ESPTOOLPY} make_image --segaddr ${offset} ${image_file} -f ${data_file_full_path}
			DEPENDS ${arg_DEPENDS}
		)

        add_dependencies(${partition}-flash part_${partition}_gen)

        if(arg_FLASH_IN_PROJECT)
            esptool_py_flash_to_partition(flash "${partition}" "${image_file}0x00000.bin")
            add_dependencies(flash part_${partition}_gen)
        endif()
    else()
        set(message "Failed to create SPIFFS image for partition '${partition}'. "
                    "Check project configuration if using the correct partition table file.")
        fail_at_build_time( ${partition}-flash "${message}")
    endif()
endfunction()