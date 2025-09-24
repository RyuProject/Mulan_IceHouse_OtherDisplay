set(LV_DEMO_DIR ../managed_components/lvgl__lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

idf_component_register(
    SRCS main.c order_ui.c hex_utils.c utf8_validator.c ${LV_DEMOS_SOURCES}
    INCLUDE_DIRS . ${LV_DEMO_DIR}
    REQUIRES 
)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
)

# 添加字体分区支持
if(EXISTS ${CMAKE_SOURCE_DIR}/spiffs_image/fonts)
    spiffs_create_partition_assets(
        font
        ${CMAKE_SOURCE_DIR}/spiffs_image/fonts
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".bin"
        IMPORT_INC_PATH "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()
