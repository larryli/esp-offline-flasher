file(GLOB srcs
    "*.c"
)
set(requires fatfs)

if(CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH)
    list(APPEND requires wear_levelling)
endif()

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS .
    REQUIRES "${requires}"
)
