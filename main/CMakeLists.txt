set(COMPONENT_ADD_INCLUDEDIRS .)
set(COMPONENT_SRCS "main.c" "led.c" "battery.c" "servo.c")

spiffs_create_partition_image(www www FLASH_IN_PROJECT)

register_component()
