set(srcs "btn.c" "findfile.c" "flash_args.c" "led.c" "main.c" "usb.c")
set(requires fatfs json)

if(CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH)
    list(APPEND requires wear_levelling)
endif()

idf_component_register(
    SRCS "flash.c" "${srcs}"
    INCLUDE_DIRS .
    REQUIRES "${requires}"
)
