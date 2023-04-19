set(requires fatfs json)

if(CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH)
    list(APPEND requires wear_levelling)
endif()

idf_component_register(
    SRCS "led.c" "main.c" "flash_args.c"
    INCLUDE_DIRS .
    REQUIRES "${requires}"
)
