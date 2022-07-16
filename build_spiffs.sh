./mkspiffs/mkspiffs -c www -p 256 -b 4096 -s 921600 www.spiffs
python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp8266 --port /dev/ttyS9 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 2MB 0x110000 www.spiffs
