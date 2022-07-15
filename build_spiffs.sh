../mkspiffs/mkspiffs -c www -p 256 -b 4096 -s 131072 www.spiffs
python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp8266 --port /dev/ttyS6 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 2MB 0x1 10000 www.spiffs

